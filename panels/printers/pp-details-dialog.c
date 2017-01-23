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

#include "cc-editable-entry.h"
#include "pp-details-dialog.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-utils.h"

struct _PpDetailsDialog {
  GtkDialog parent;

  GtkEntry *printer_name_entry;
  GtkEntry *printer_location_entry;
  GtkLabel *printer_address_label;
  GtkLabel *printer_model_label;
  GtkStack *printer_model_stack;
  GtkWidget *search_for_drivers_button;

  gchar        *printer_name;
  gchar        *printer_location;
  gchar        *ppd_file_name;
  PPDList      *all_ppds_list;
  GHashTable   *preferred_drivers;
  GCancellable *get_all_ppds_cancellable;
  GCancellable *get_ppd_names_cancellable;

  /* Dialogs */
  PpPPDSelectionDialog *pp_ppd_selection_dialog;
};

struct _PpDetailsDialogClass
{
  GtkDialogClass parent_class;
};

G_DEFINE_TYPE (PpDetailsDialog, pp_details_dialog, GTK_TYPE_DIALOG);

static gboolean
printer_name_edit_cb (GtkWidget       *entry,
                      GdkEventFocus   *event,
                      PpDetailsDialog *self)
{
  const gchar *new_name;

  new_name = gtk_entry_get_text (GTK_ENTRY (self->printer_name_entry));
  if (g_strcmp0 (self->printer_name, new_name) != 0)
    {
      printer_rename (self->printer_name, new_name);

      self->printer_name = g_strdup (new_name);
    }

  return FALSE;
}

static void
printer_name_changed (GtkEditable *editable,
                      gpointer     user_data)
{
  PpDetailsDialog *self = (PpDetailsDialog *) user_data;
  GtkWidget *widget;
  const gchar *name;
  gchar *title;

  name = gtk_entry_get_text (GTK_ENTRY (self->printer_name_entry));

  title = g_strdup_printf ("%s Details", name);

  widget = gtk_dialog_get_header_bar (GTK_DIALOG (self));
  gtk_header_bar_set_title (GTK_HEADER_BAR (widget), title);

  g_free (title);
}

static gboolean
printer_location_edit_cb (GtkWidget       *entry,
                          GdkEventFocus   *event,
                          PpDetailsDialog *self)
{
  const gchar *location;

  location = gtk_entry_get_text (GTK_ENTRY (self->printer_location_entry));
  if (g_strcmp0 (self->printer_location, location) != 0)
    {
      printer_set_location (self->printer_name, location);

      self->printer_location = g_strdup (location);
    }

  return FALSE;
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
  gpointer  value = NULL;
  PPDName **hash_names = NULL;

  if (self->preferred_drivers == NULL)
    {
      self->preferred_drivers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       g_free, ppd_names_free);
    }

  if (!cancelled &&
      !g_hash_table_lookup_extended (self->preferred_drivers, printer_name, NULL, NULL))
    {
      g_hash_table_insert (self->preferred_drivers, g_strdup (printer_name), names);
    }

  if (self->preferred_drivers != NULL &&
      g_hash_table_lookup_extended (self->preferred_drivers, printer_name, NULL, &value))
    {
      hash_names = (PPDName **) value;
      if (hash_names != NULL)
        {
          gtk_label_set_text (self->printer_model_label, hash_names[0]->ppd_display_name);
          printer_set_ppd_async (printer_name,
                                 hash_names[0]->ppd_name,
                                 self->get_ppd_names_cancellable,
                                 set_ppd_cb,
                                 self);
        }
      else
        {
          gtk_label_set_text (self->printer_model_label, _("No suitable driver found"));
        }
    }

  gtk_stack_set_visible_child_name (self->printer_model_stack, "printer_model_label");
}

static void
search_for_drivers (GtkButton       *button,
                    PpDetailsDialog *self)
{
  gtk_stack_set_visible_child_name (self->printer_model_stack, "loading");
  gtk_widget_set_sensitive (self->search_for_drivers_button, FALSE);

  if (self->preferred_drivers != NULL &&
      g_hash_table_lookup_extended (self->preferred_drivers,
                                    self->printer_name,
                                    NULL, NULL))
    {
      get_ppd_names_cb (NULL, self->printer_name, FALSE, self);
    }
  else
    {
      self->get_ppd_names_cancellable = g_cancellable_new ();
      get_ppd_names_async (self->printer_name,
                           1,
                           self->get_ppd_names_cancellable,
                           get_ppd_names_cb,
                           self);
    }
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
          GCancellable *cancellable;

          cancellable = g_cancellable_new ();

          printer_set_ppd_file_async (self->printer_name,
                                      ppd_filename,
                                      cancellable,
                                      set_ppd_cb,
                                      self);
        }

      g_free (ppd_filename);
    }

  gtk_widget_destroy (dialog);
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

  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_name_entry);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_location_entry);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_address_label);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_model_label);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, printer_model_stack);
  gtk_widget_class_bind_template_child (widget_class, PpDetailsDialog, search_for_drivers_button);

  gtk_widget_class_bind_template_callback (widget_class, printer_name_edit_cb);
  gtk_widget_class_bind_template_callback (widget_class, printer_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, printer_location_edit_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_for_drivers);
  gtk_widget_class_bind_template_callback (widget_class, select_ppd_in_dialog);
  gtk_widget_class_bind_template_callback (widget_class, select_ppd_manually);
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

  title = g_strdup_printf (C_("Printer Details dialog title", "%s Details"), printer_name);
  gtk_window_set_title (GTK_WINDOW (self), title);

  printer_url = g_strdup_printf ("<a href=\"http://%s:%d\">%s</a>", printer_address, ippPort (), printer_address);
  gtk_label_set_markup (GTK_LABEL (self->printer_address_label), printer_url);
  g_free (printer_url);

  gtk_entry_set_text (GTK_ENTRY (self->printer_name_entry), printer_name);
  gtk_entry_set_text (GTK_ENTRY (self->printer_location_entry), printer_location);
  gtk_label_set_text (GTK_LABEL (self->printer_model_label), printer_make_and_model);

  gtk_widget_set_sensitive (gtk_dialog_get_content_area (GTK_DIALOG (self)), sensitive);

  self->preferred_drivers = NULL;

  return self;
}

void
pp_details_dialog_free (PpDetailsDialog *self)
{
  g_free (self->printer_name);
  self->printer_name = NULL;

  g_free (self->printer_location);
  self->printer_location = NULL;

  if (self->all_ppds_list != NULL)
    {
      ppd_list_free (self->all_ppds_list);
      self->all_ppds_list = NULL;
    }

  if (self->get_all_ppds_cancellable != NULL)
    {
      g_cancellable_cancel (self->get_all_ppds_cancellable);
      g_object_unref (self->get_all_ppds_cancellable);
      self->get_all_ppds_cancellable = NULL;
    }

  if (self->preferred_drivers != NULL)
    {
      g_hash_table_unref (self->preferred_drivers);
      self->preferred_drivers = NULL;
    }

  if (self->get_ppd_names_cancellable != NULL)
    {
      g_cancellable_cancel (self->get_ppd_names_cancellable);
      g_object_unref (self->get_ppd_names_cancellable);
      self->get_ppd_names_cancellable = NULL;
    }

  gtk_widget_destroy (GTK_WIDGET (self));
}
