/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2016  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <feborges@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-details-dialog.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-printer.h"
#include "pp-utils.h"

struct _PpDetailsDialog {
  GtkDialog parent;

  GtkLabel *dialog_title;
  GtkEntry *printer_name_entry;
  GtkEntry *printer_location_entry;
  GtkLabel *printer_address_label;
  GtkLabel *printer_model_label;
  GtkStack *printer_model_stack;
  GtkWidget *search_for_drivers_button;
  GtkWidget *driver_buttons;

  gchar        *printer_name;
  gchar        *printer_location;
  gchar        *ppd_file_name;
  PPDList      *all_ppds_list;
  GCancellable *get_all_ppds_cancellable;
  GCancellable *get_ppd_names_cancellable;

  /* Dialogs */
  PpPPDSelectionDialog *pp_ppd_selection_dialog;
};

struct _PpDetailsDialogClass
{
  GtkDialogClass parent_class;

  void (*printer_renamed) (PpDetailsDialog *details_dialog, const gchar *new_name);
};

G_DEFINE_TYPE (PpDetailsDialog, pp_details_dialog, GTK_TYPE_DIALOG)

enum
{
  PRINTER_RENAMED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
on_printer_rename_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  pp_printer_rename_finish (PP_PRINTER (source_object), result, NULL);

  g_object_unref (source_object);
}

static void
pp_details_dialog_response_cb (GtkDialog *dialog,
                               gint       response_id,
                               gpointer   user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) dialog;
  const gchar *new_name;
  const gchar *new_location;

  new_location = gtk_entry_get_text (GTK_ENTRY (self->printer_location_entry));
  if (g_strcmp0 (self->printer_location, new_location) != 0)
    {
      printer_set_location (self->printer_name, new_location);

      self->printer_location = g_strdup (new_location);
    }

  new_name = gtk_entry_get_text (GTK_ENTRY (self->printer_name_entry));
  if (g_strcmp0 (self->printer_name, new_name) != 0)
    {
      PpPrinter *printer = pp_printer_new (self->printer_name);

      g_signal_emit_by_name (self, "printer-renamed", new_name);

      pp_printer_rename_async (printer,
                               new_name,
                               NULL,
                               on_printer_rename_cb,
                               NULL);
    }
}

static void
printer_name_changed (GtkEditable *editable,
                      gpointer     user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog *) user_data;
  const gchar *name;
  gchar *title;

  name = gtk_entry_get_text (GTK_ENTRY (self->printer_name_entry));

  /* Translators: This is the title of the dialog. %s is the printer name. */
  title = g_strdup_printf (_("%s Details"), name);
  gtk_label_set_label (self->dialog_title, title);

  g_free (title);
}

static void
ppd_names_free (gpointer user_data)
{
  PPDName **names = (PPDName **) user_data;
  gint      i;

  if (names)
    {
      for (i = 0; names[i]; i++)
        {
          g_free (names[i]->ppd_name);
          g_free (names[i]->ppd_display_name);
          g_free (names[i]);
        }

      g_free (names);
    }
}

static void set_ppd_cb (gchar *printer_name, gboolean success, gpointer user_data);

static void
get_ppd_names_cb (PPDName     **names,
                  const gchar  *printer_name,
                  gboolean      cancelled,
                  gpointer      user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) user_data;

  if (!cancelled)
    {
      if (names != NULL)
        {
          gtk_label_set_text (self->printer_model_label, names[0]->ppd_display_name);
          printer_set_ppd_async (printer_name,
                                 names[0]->ppd_name,
                                 self->get_ppd_names_cancellable,
                                 set_ppd_cb,
                                 self);
          ppd_names_free (names);
        }
      else
        {
          gtk_label_set_text (self->printer_model_label, _("No suitable driver found"));
        }

      gtk_stack_set_visible_child_name (self->printer_model_stack, "printer_model_label");
    }
}

static void
search_for_drivers (GtkButton       *button,
                    PpDetailsDialog *self)
{
  gtk_stack_set_visible_child_name (self->printer_model_stack, "loading");
  gtk_widget_set_sensitive (self->search_for_drivers_button, FALSE);

  self->get_ppd_names_cancellable = g_cancellable_new ();
  get_ppd_names_async (self->printer_name,
                       1,
                       self->get_ppd_names_cancellable,
                       get_ppd_names_cb,
                       self);
}

static void
set_ppd_cb (gchar    *printer_name,
            gboolean  success,
            gpointer  user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) user_data;

  gtk_label_set_text (GTK_LABEL (self->printer_model_label), self->ppd_file_name);
}

static void
ppd_selection_dialog_response_cb (GtkDialog *dialog,
                                  gint       response_id,
                                  gpointer   user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog*) user_data;

  if (response_id == GTK_RESPONSE_OK)
    {
      gchar *ppd_name;

      ppd_name = pp_ppd_selection_dialog_get_ppd_name (self->pp_ppd_selection_dialog);

      if (self->printer_name && ppd_name)
        {
          GCancellable *cancellable;

          cancellable = g_cancellable_new ();

          printer_set_ppd_async (self->printer_name,
                                 ppd_name,
                                 cancellable,
                                 set_ppd_cb,
                                 self);

          g_clear_pointer (&self->ppd_file_name, g_free);
          self->ppd_file_name = g_strdup (ppd_name);
        }

      g_free (ppd_name);
    }

  pp_ppd_selection_dialog_free (self->pp_ppd_selection_dialog);
  self->pp_ppd_selection_dialog = NULL;
}

static void
get_all_ppds_async_cb (PPDList  *ppds,
                       gpointer  user_data)
{
  PpDetailsDialog *self = user_data;

  self->all_ppds_list = ppds;

  if (self->pp_ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (self->pp_ppd_selection_dialog,
                                          self->all_ppds_list);

  g_object_unref (self->get_all_ppds_cancellable);
  self->get_all_ppds_cancellable = NULL;
}

static void
select_ppd_in_dialog (GtkButton       *button,
                      PpDetailsDialog *self)
{
  gchar *device_id = NULL;
  gchar *manufacturer = NULL;

  g_clear_pointer (&self->ppd_file_name, g_free);
  self->ppd_file_name = g_strdup (cupsGetPPD (self->printer_name));

  if (!self->pp_ppd_selection_dialog)
    {
      device_id =
        get_ppd_attribute (self->ppd_file_name,
                           "1284DeviceID");

      if (device_id)
        {
          manufacturer = get_tag_value (device_id, "mfg");
          if (!manufacturer)
            manufacturer = get_tag_value (device_id, "manufacturer");
          }

      if (manufacturer == NULL)
        {
          manufacturer =
            get_ppd_attribute (self->ppd_file_name,
                               "Manufacturer");
          }

      if (manufacturer == NULL)
        {
          manufacturer = g_strdup ("Raw");
        }

       if (self->all_ppds_list == NULL)
         {
           self->get_all_ppds_cancellable = g_cancellable_new ();
           get_all_ppds_async (self->get_all_ppds_cancellable, get_all_ppds_async_cb, self);
         }

        self->pp_ppd_selection_dialog = pp_ppd_selection_dialog_new (
          GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
          self->all_ppds_list,
          manufacturer,
          ppd_selection_dialog_response_cb,
          self);

        g_free (manufacturer);
        g_free (device_id);
    }
}

static void
select_ppd_manually (GtkButton       *button,
                     PpDetailsDialog *self)
{
  GtkFileFilter *filter;
  GtkWidget     *dialog;

  dialog = gtk_file_chooser_dialog_new (_("Select PPD File"),
                                        GTK_WINDOW (self),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        _("_Open"), GTK_RESPONSE_ACCEPT,
                                        NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter,
    _("PostScript Printer Description files (*.ppd, *.PPD, *.ppd.gz, *.PPD.gz, *.PPD.GZ)"));
  gtk_file_filter_add_pattern (filter, "*.ppd");
  gtk_file_filter_add_pattern (filter, "*.PPD");
  gtk_file_filter_add_pattern (filter, "*.ppd.gz");
  gtk_file_filter_add_pattern (filter, "*.PPD.gz");
  gtk_file_filter_add_pattern (filter, "*.PPD.GZ");

  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
      gchar *ppd_filename;

      ppd_filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

      if (self->printer_name && ppd_filename)
        {
          printer_set_ppd_file_async (self->printer_name,
                                      ppd_filename,
                                      NULL,
                                      set_ppd_cb,
                                      self);
        }

      g_free (ppd_filename);
    }

  gtk_widget_destroy (dialog);
}

static void
update_sensitivity (PpDetailsDialog *self,
                    gboolean         sensitive)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->printer_name_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->printer_location_entry), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (self->driver_buttons), sensitive);
}

static void
pp_details_dialog_init (PpDetailsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
pp_details_dialog_class_init (PpDetailsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/details-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, dialog_title);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_name_entry);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_location_entry);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_address_label);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_model_label);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_model_stack);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, search_for_drivers_button);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, driver_buttons);

  gtk_widget_class_bind_template_callback (widget_class, printer_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_for_drivers);
  gtk_widget_class_bind_template_callback (widget_class, select_ppd_in_dialog);
  gtk_widget_class_bind_template_callback (widget_class, select_ppd_manually);
  gtk_widget_class_bind_template_callback (widget_class, pp_details_dialog_response_cb);

  signals[PRINTER_RENAMED] = g_signal_new ("printer-renamed",
                                           G_TYPE_FROM_CLASS (klass),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (PpDetailsDialogClass, printer_renamed),
                                           NULL, NULL, NULL,
                                           G_TYPE_NONE, 1,
                                           G_TYPE_STRING);
}

PpDetailsDialog *
pp_details_dialog_new (GtkWindow            *parent,
                       gchar                *printer_name,
                       gchar                *printer_location,
                       gchar                *printer_address,
                       gchar                *printer_make_and_model,
                       gboolean              sensitive)
{
  PpDetailsDialog *self;
  gchar           *title;
  gchar           *printer_url;

  self = g_object_new (PP_DETAILS_DIALOG_TYPE,
                       "transient-for", parent,
                       "use-header-bar", TRUE,
                       NULL);

  self->printer_name = g_strdup (printer_name);
  self->printer_location = g_strdup (printer_location);
  self->ppd_file_name = NULL;

  /* Translators: This is the title of the dialog. %s is the printer name. */
  title = g_strdup_printf (_("%s Details"), printer_name);
  gtk_label_set_label (self->dialog_title, title);

  printer_url = g_strdup_printf ("<a href=\"http://%s:%d\">%s</a>", printer_address, ippPort (), printer_address);
  gtk_label_set_markup (GTK_LABEL (self->printer_address_label), printer_url);
  g_free (printer_url);

  gtk_entry_set_text (GTK_ENTRY (self->printer_name_entry), printer_name);
  gtk_entry_set_text (GTK_ENTRY (self->printer_location_entry), printer_location);
  gtk_label_set_text (GTK_LABEL (self->printer_model_label), printer_make_and_model);

  update_sensitivity (self, sensitive);

  return self;
}

void
pp_details_dialog_free (PpDetailsDialog *self)
{
  if (self != NULL)
    {
      g_clear_pointer (&self->printer_name, g_free);
      g_clear_pointer (&self->printer_location, g_free);
      g_clear_pointer (&self->ppd_file_name, g_free);

      if (self->all_ppds_list != NULL)
        {
          ppd_list_free (self->all_ppds_list);
          self->all_ppds_list = NULL;
        }

      g_cancellable_cancel (self->get_all_ppds_cancellable);
      g_clear_object (&self->get_all_ppds_cancellable);

      g_cancellable_cancel (self->get_ppd_names_cancellable);
      g_clear_object (&self->get_ppd_names_cancellable);

      gtk_widget_destroy (GTK_WIDGET (self));
    }
}
