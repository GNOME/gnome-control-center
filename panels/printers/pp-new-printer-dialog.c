/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <cups/cups.h>

#include "pp-new-printer-dialog.h"
#include "pp-ppd-selection-dialog.h"
#include "pp-utils.h"
#include "pp-host.h"
#include "pp-cups.h"
#include "pp-samba.h"
#include "pp-new-printer.h"

#include <gdk/gdkx.h>

#if (CUPS_VERSION_MAJOR > 1) || (CUPS_VERSION_MINOR > 5)
#define HAVE_CUPS_1_6 1
#endif

#ifndef HAVE_CUPS_1_6
#define ippGetState(ipp) ipp->state
#endif

/*
 * Additional delay to the default 150ms delay in GtkSearchEntry
 * resulting in total delay of 500ms.
 */
#define HOST_SEARCH_DELAY (500 - 150)

static void     actualize_devices_list (PpNewPrinterDialog *dialog);
static void     populate_devices_list (PpNewPrinterDialog *dialog);
static void     search_entry_activated_cb (GtkEntry *entry,
                                           gpointer  user_data);
static void     search_entry_changed_cb (GtkSearchEntry *entry,
                                         gpointer        user_data);
static void     new_printer_dialog_response_cb (GtkDialog *_dialog,
                                                gint       response_id,
                                                gpointer   user_data);
static void     update_spinner_state (PpNewPrinterDialog *dialog);
static void     add_devices_to_list (PpNewPrinterDialog  *dialog,
                                     GList               *devices,
                                     gboolean             new_device);
static void     remove_device_from_list (PpNewPrinterDialog *dialog,
                                         const gchar        *device_name);
static void     pp_devices_list_free (PpDevicesList *result);

enum
{
  DEVICE_GICON_COLUMN = 0,
  DEVICE_NAME_COLUMN,
  DEVICE_DISPLAY_NAME_COLUMN,
  DEVICE_DESCRIPTION_COLUMN,
  SERVER_NEEDS_AUTHENTICATION_COLUMN,
  DEVICE_N_COLUMNS
};

typedef struct
{
  gchar    *display_name;
  gchar    *device_name;
  gchar    *device_original_name;
  gchar    *device_info;
  gchar    *device_location;
  gchar    *device_make_and_model;
  gchar    *device_uri;
  gchar    *device_id;
  gchar    *device_ppd;
  gchar    *host_name;
  gint      host_port;
  gboolean  network_device;
  gint      acquisition_method;
  gboolean  is_authenticated_server;
  gboolean  show;
} TDevice;

static void     t_device_free (gpointer data);
static TDevice *t_device_copy (TDevice *device);

struct _PpNewPrinterDialogPrivate
{
  GtkBuilder *builder;

  GList *devices;
  GList *new_devices;

  cups_dest_t *dests;
  gint         num_of_dests;

  GCancellable *cancellable;
  GCancellable *remote_host_cancellable;

  gboolean  cups_searching;
  gboolean  samba_authenticated_searching;
  gboolean  samba_searching;

  GtkCellRenderer *text_renderer;
  GtkCellRenderer *icon_renderer;

  PpPPDSelectionDialog *ppd_selection_dialog;

  TDevice *new_device;

  PPDList *list;

  GtkWidget *dialog;
  GtkWindow *parent;

  GIcon *local_printer_icon;
  GIcon *remote_printer_icon;
  GIcon *authenticated_server_icon;

  PpHost  *snmp_host;
  PpHost  *remote_cups_host;
  PpSamba *samba_host;
  guint    host_search_timeout_id;
};

#define PP_NEW_PRINTER_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PP_TYPE_NEW_PRINTER_DIALOG, PpNewPrinterDialogPrivate))

static void pp_new_printer_dialog_finalize (GObject *object);

enum {
  PRE_RESPONSE,
  RESPONSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (PpNewPrinterDialog, pp_new_printer_dialog, G_TYPE_OBJECT)

static void
pp_new_printer_dialog_class_init (PpNewPrinterDialogClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = pp_new_printer_dialog_finalize;

  g_type_class_add_private (object_class, sizeof (PpNewPrinterDialogPrivate));

  /**
   * PpNewPrinterDialog::pre-response:
   * @device: the device that is being added
   *
   * The signal which gets emitted when the new printer dialog is closed.
   */
  signals[PRE_RESPONSE] =
    g_signal_new ("pre-response",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PpNewPrinterDialogClass, pre_response),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

  /**
   * PpNewPrinterDialog::response:
   * @response-id: response id of dialog
   *
   * The signal which gets emitted after the printer is added and configured.
   */
  signals[RESPONSE] =
    g_signal_new ("response",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PpNewPrinterDialogClass, response),
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 1, G_TYPE_INT);
}


PpNewPrinterDialog *
pp_new_printer_dialog_new (GtkWindow *parent,
                           PPDList   *ppd_list)
{
  PpNewPrinterDialogPrivate *priv;
  PpNewPrinterDialog        *dialog;

  dialog = g_object_new (PP_TYPE_NEW_PRINTER_DIALOG, NULL);
  priv = dialog->priv;

  priv->list = ppd_list_copy (ppd_list);
  priv->parent = parent;

  gtk_window_set_transient_for (GTK_WINDOW (priv->dialog), GTK_WINDOW (parent));

  return PP_NEW_PRINTER_DIALOG (dialog);
}

void
pp_new_printer_dialog_set_ppd_list (PpNewPrinterDialog *dialog,
                                    PPDList            *list)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;

  priv->list = ppd_list_copy (list);

  if (priv->ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (priv->ppd_selection_dialog, priv->list);
}

static void
emit_pre_response (PpNewPrinterDialog *dialog,
                   const gchar        *device_name,
                   const gchar        *device_location,
                   const gchar        *device_make_and_model,
                   gboolean            network_device)
{
  g_signal_emit (dialog,
                 signals[PRE_RESPONSE],
                 0,
                 device_name,
                 device_location,
                 device_make_and_model,
                 network_device);
}

static void
emit_response (PpNewPrinterDialog *dialog,
               gint                response_id)
{
  g_signal_emit (dialog, signals[RESPONSE], 0, response_id);
}

/*
 * Modify padding of the content area of the GtkDialog
 * so it is aligned with the action area.
 */
static void
update_alignment_padding (GtkWidget     *widget,
                          GtkAllocation *allocation,
                          gpointer       user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkAllocation              allocation1, allocation2;
  GtkWidget                 *action_area;
  GtkWidget                 *content_area;
  gint                       offset_left, offset_right;
  guint                      padding_left, padding_right,
                             padding_top, padding_bottom;

  action_area = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "dialog-action-area1");
  gtk_widget_get_allocation (action_area, &allocation2);

  content_area = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "content-alignment");
  gtk_widget_get_allocation (content_area, &allocation1);

  offset_left = allocation2.x - allocation1.x;
  offset_right = (allocation1.x + allocation1.width) -
                 (allocation2.x + allocation2.width);

  gtk_alignment_get_padding  (GTK_ALIGNMENT (content_area),
                              &padding_top, &padding_bottom,
                              &padding_left, &padding_right);
  if (allocation1.x >= 0 && allocation2.x >= 0)
    {
      if (offset_left > 0 && offset_left != padding_left)
        gtk_alignment_set_padding (GTK_ALIGNMENT (content_area),
                                   padding_top, padding_bottom,
                                   offset_left, padding_right);

      gtk_alignment_get_padding  (GTK_ALIGNMENT (content_area),
                                  &padding_top, &padding_bottom,
                                  &padding_left, &padding_right);
      if (offset_right > 0 && offset_right != padding_right)
        gtk_alignment_set_padding (GTK_ALIGNMENT (content_area),
                                   padding_top, padding_bottom,
                                   padding_left, offset_right);
    }
}

typedef struct
{
  gchar    *server_name;
  gpointer  dialog;
} AuthSMBData;

static void
get_authenticated_samba_devices_cb (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  PpNewPrinterDialogPrivate *priv;
  PpNewPrinterDialog        *dialog;
  PpDevicesList             *result;
  PpPrintDevice             *device;
  AuthSMBData               *data;
  GtkWidget                 *widget;
  gboolean                   cancelled = FALSE;
  PpSamba                   *samba = (PpSamba *) source_object;
  GError                    *error = NULL;
  GList                     *iter;

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  data = (AuthSMBData *) user_data;

  if (result != NULL)
    {
      dialog = PP_NEW_PRINTER_DIALOG (data->dialog);
      priv = dialog->priv;

      priv->samba_authenticated_searching = FALSE;
      update_spinner_state (dialog);

      for (iter = result->devices; iter; iter = iter->next)
        {
          device = (PpPrintDevice *) iter->data;
          if (device->is_authenticated_server)
            {
              cancelled = TRUE;
              break;
            }
        }

      if (!cancelled)
        {
          remove_device_from_list (dialog,
                                   data->server_name);

          if (result->devices != NULL)
            {
              add_devices_to_list (dialog,
                                   result->devices,
                                   FALSE);

              device = (PpPrintDevice *) result->devices->data;
              if (device != NULL)
                {
                  widget = (GtkWidget*)
                    gtk_builder_get_object (priv->builder, "search-entry");
                  gtk_entry_set_text (GTK_ENTRY (widget), device->device_location);
                  search_entry_activated_cb (GTK_ENTRY (widget), dialog);
                }
            }

          actualize_devices_list (dialog);
        }

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          dialog = PP_NEW_PRINTER_DIALOG (data->dialog);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          priv->samba_authenticated_searching = FALSE;
          update_spinner_state (dialog);
        }

      g_error_free (error);
    }

  g_free (data->server_name);
  g_free (data);
}

static void
authenticate_samba_server (GtkButton *button,
                           gpointer   user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  AuthSMBData               *data;
  GtkWidget                 *treeview;
  PpSamba                   *samba_host;
  gchar                     *server_name = NULL;

  treeview = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "devices-treeview");

  if (treeview &&
      gtk_tree_selection_get_selected (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          DEVICE_NAME_COLUMN, &server_name,
                          -1);

      if (server_name != NULL)
        {
          samba_host = pp_samba_new (GTK_WINDOW (priv->dialog),
                                     server_name);

          priv->samba_authenticated_searching = TRUE;
          update_spinner_state (dialog);

          data = g_new (AuthSMBData, 1);
          data->server_name = server_name;
          data->dialog = dialog;

          pp_samba_get_devices_async (samba_host,
                                      TRUE,
                                      priv->cancellable,
                                      get_authenticated_samba_devices_cb,
                                      data);
        }
    }
}

static void
pp_new_printer_dialog_init (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv;
  GtkStyleContext           *context;
  GtkWidget                 *widget;
  GError                    *error = NULL;
  gchar                     *objects[] = { "dialog", "devices-liststore", NULL };
  guint                      builder_result;

  priv = PP_NEW_PRINTER_DIALOG_GET_PRIVATE (dialog);
  dialog->priv = priv;

  priv->builder = gtk_builder_new ();

  builder_result = gtk_builder_add_objects_from_resource (priv->builder,
                                                          "/org/gnome/control-center/printers/new-printer-dialog.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_error_free (error);
    }

  /* GCancellable for cancelling of async operations */
  priv->cancellable = g_cancellable_new ();

  /* Construct dialog */
  priv->dialog = (GtkWidget*) gtk_builder_get_object (priv->builder, "dialog");

  /* Connect signals */
  g_signal_connect (priv->dialog, "response", G_CALLBACK (new_printer_dialog_response_cb), dialog);
  g_signal_connect (priv->dialog, "size-allocate", G_CALLBACK (update_alignment_padding), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "search-entry");
  g_signal_connect (widget, "activate", G_CALLBACK (search_entry_activated_cb), dialog);
  g_signal_connect (widget, "search-changed", G_CALLBACK (search_entry_changed_cb), dialog);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "authenticate-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (authenticate_samba_server), dialog);

  /* Set junctions */
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "scrolledwindow1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "toolbar1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  /* Fill with data */
  populate_devices_list (dialog);

  gtk_widget_show (priv->dialog);
}

static void
pp_new_printer_dialog_finalize (GObject *object)
{
  PpNewPrinterDialog *dialog = PP_NEW_PRINTER_DIALOG (object);
  PpNewPrinterDialogPrivate *priv = dialog->priv;

  priv->text_renderer = NULL;
  priv->icon_renderer = NULL;

  if (priv->host_search_timeout_id != 0)
    {
      g_source_remove (priv->host_search_timeout_id);
      priv->host_search_timeout_id = 0;
    }

  if (priv->remote_host_cancellable)
    {
      g_cancellable_cancel (priv->remote_host_cancellable);
      g_clear_object (&priv->remote_host_cancellable);
    }

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_clear_pointer (&priv->dialog, gtk_widget_destroy);

  if (priv->builder)
    g_clear_object (&priv->builder);

  g_list_free_full (priv->devices, t_device_free);
  priv->devices = NULL;

  g_list_free_full (priv->new_devices, t_device_free);
  priv->new_devices = NULL;

  if (priv->num_of_dests > 0)
    {
      cupsFreeDests (priv->num_of_dests, priv->dests);
      priv->num_of_dests = 0;
      priv->dests = NULL;
    }

  g_clear_object (&priv->local_printer_icon);
  g_clear_object (&priv->remote_printer_icon);
  g_clear_object (&priv->authenticated_server_icon);

  G_OBJECT_CLASS (pp_new_printer_dialog_parent_class)->finalize (object);
}

static void
device_selection_changed_cb (GtkTreeSelection *selection,
                             gpointer          user_data)
{
  PpNewPrinterDialog        *dialog = PP_NEW_PRINTER_DIALOG (user_data);
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  GtkWidget                 *treeview = NULL;
  GtkWidget                 *widget;
  GtkWidget                 *notebook;
  gboolean                   authentication_needed;
  gboolean                   selected;

  treeview = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "devices-treeview");

  if (treeview)
    {
      selected = gtk_tree_selection_get_selected (
                   gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                                                &model,
                                                &iter);

      if (selected)
        {
          gtk_tree_model_get (model, &iter,
                              SERVER_NEEDS_AUTHENTICATION_COLUMN, &authentication_needed,
                              -1);

          widget = (GtkWidget*)
            gtk_builder_get_object (priv->builder, "new-printer-add-button");
          gtk_widget_set_sensitive (widget, selected);

          widget = (GtkWidget*)
            gtk_builder_get_object (priv->builder, "authenticate-button");
          gtk_widget_set_sensitive (widget, authentication_needed);

          notebook = (GtkWidget*)
            gtk_builder_get_object (priv->builder, "notebook");

          if (authentication_needed)
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
          else
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
        }
    }
}

static void
remove_device_from_list (PpNewPrinterDialog *dialog,
                         const gchar        *device_name)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  TDevice                   *device;
  GList                     *iter;

  for (iter = priv->devices; iter; iter = iter->next)
    {
      device = (TDevice *) iter->data;
      if (g_strcmp0 (device->device_name, device_name) == 0)
        {
          priv->devices = g_list_remove_link (priv->devices, iter);
          t_device_free (iter->data);
          g_list_free (iter);
          break;
        }
    }
}

static void
add_device_to_list (PpNewPrinterDialog *dialog,
                    PpPrintDevice      *device,
                    gboolean            new_device)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  gboolean  network_device;
  gboolean  already_present;
  TDevice  *store_device;
  TDevice  *item;
  GList    *iter;
  gchar    *name = NULL;
  gchar    *canonized_name = NULL;
  gchar    *new_name;
  gchar    *new_canonized_name = NULL;
  gint      name_index, j;

  if (device)
    {
      if (device->host_name == NULL)
        device->host_name = guess_device_hostname (device);

      if (device->device_id ||
          device->device_ppd ||
          (device->host_name &&
           device->acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER) ||
           device->acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
           device->acquisition_method == ACQUISITION_METHOD_SAMBA)
        {
          network_device = FALSE;

          if (device->device_class &&
              g_strcmp0 (device->device_class, "network") == 0)
            network_device = TRUE;

          store_device = g_new0 (TDevice, 1);
          store_device->device_original_name = g_strdup (device->device_name);
          store_device->device_info = g_strdup (device->device_info);
          store_device->device_location = g_strdup (device->device_location);
          store_device->device_make_and_model = g_strdup (device->device_make_and_model);
          store_device->device_uri = g_strdup (device->device_uri);
          store_device->device_id = g_strdup (device->device_id);
          store_device->device_ppd = g_strdup (device->device_ppd);
          store_device->host_name = g_strdup (device->host_name);
          store_device->host_port = device->host_port;
          store_device->network_device = network_device;
          store_device->acquisition_method = device->acquisition_method;
          store_device->show = TRUE;

          if (device->device_id)
            {
              name = get_tag_value (device->device_id, "mdl");
              if (!name)
                name = get_tag_value (device->device_id, "model");
            }

          if (!name &&
              device->device_make_and_model &&
              device->device_make_and_model[0] != '\0')
            {
              name = g_strdup (device->device_make_and_model);
            }

          if (!name &&
              device->device_name &&
              device->device_name[0] != '\0')
            {
              name = g_strdup (device->device_name);
            }

          if (!name &&
              device->device_info &&
              device->device_info[0] != '\0')
            {
              name = g_strdup (device->device_info);
            }

          g_strstrip (name);

          name_index = 2;
          already_present = FALSE;
          do
            {
              if (already_present)
                {
                  new_name = g_strdup_printf ("%s %d", name, name_index);
                  name_index++;
                }
              else
                {
                  new_name = g_strdup (name);
                }

              if (new_name)
                {
                  new_canonized_name = g_strcanon (g_strdup (new_name), ALLOWED_CHARACTERS, '-');
                }

              already_present = FALSE;
              for (j = 0; j < priv->num_of_dests; j++)
                if (g_strcmp0 (priv->dests[j].name, new_canonized_name) == 0)
                  already_present = TRUE;

              for (iter = priv->devices; iter; iter = iter->next)
                {
                  item = (TDevice *) iter->data;
                  if (g_strcmp0 (item->device_name, new_canonized_name) == 0)
                    already_present = TRUE;
                }

              for (iter = priv->new_devices; iter; iter = iter->next)
                {
                  item = (TDevice *) iter->data;
                  if (g_strcmp0 (item->device_name, new_canonized_name) == 0)
                    already_present = TRUE;
                }

              if (already_present)
                {
                  g_free (new_name);
                  g_free (new_canonized_name);
                }
              else
                {
                  g_free (name);
                  g_free (canonized_name);
                  name = new_name;
                  canonized_name = new_canonized_name;
                }
            } while (already_present);

          store_device->display_name = g_strdup (canonized_name);
          store_device->device_name = canonized_name;
          g_free (name);

          if (new_device)
            priv->new_devices = g_list_append (priv->new_devices, store_device);
          else
            priv->devices = g_list_append (priv->devices, store_device);
        }
      else if (device->is_authenticated_server &&
              device->host_name != NULL)
        {
          store_device = g_new0 (TDevice, 1);
          store_device->device_name = g_strdup (device->host_name);
          store_device->host_name = g_strdup (device->host_name);
          store_device->is_authenticated_server = device->is_authenticated_server;
          store_device->show = TRUE;

          priv->devices = g_list_append (priv->devices, store_device);
        }
    }
}

static void
add_devices_to_list (PpNewPrinterDialog  *dialog,
                     GList               *devices,
                     gboolean             new_device)
{
  GList *iter;

  for (iter = devices; iter; iter = iter->next)
    {
      add_device_to_list (dialog, (PpPrintDevice *) iter->data, new_device);
    }
}

static TDevice *
device_in_list (gchar *device_uri,
                GList *device_list)
{
  GList   *iter;
  TDevice *device;

  for (iter = device_list; iter; iter = iter->next)
    {
      device = (TDevice *) iter->data;
      /* GroupPhysicalDevices returns uris without port numbers */
      if (device->device_uri != NULL &&
          g_str_has_prefix (device->device_uri, device_uri))
        return device;
    }

  return NULL;
}

static void
t_device_free (gpointer data)
{
  if (data)
    {
      TDevice *device = (TDevice *) data;

      g_free (device->display_name);
      g_free (device->device_name);
      g_free (device->device_original_name);
      g_free (device->device_info);
      g_free (device->device_location);
      g_free (device->device_make_and_model);
      g_free (device->device_uri);
      g_free (device->device_id);
      g_free (device->device_ppd);
      g_free (device->host_name);
      g_free (device);
    }
}

static TDevice *
t_device_copy (TDevice *device)
{
  TDevice *result = NULL;

  if (device)
    {
      result = g_new0 (TDevice, 1);

      result->display_name = g_strdup (device->display_name);
      result->device_name = g_strdup (device->device_name);
      result->device_original_name = g_strdup (device->device_original_name);
      result->device_info = g_strdup (device->device_info);
      result->device_location = g_strdup (device->device_location);
      result->device_make_and_model = g_strdup (device->device_make_and_model);
      result->device_uri = g_strdup (device->device_uri);
      result->device_id = g_strdup (device->device_id);
      result->device_ppd = g_strdup (device->device_ppd);
      result->host_name = g_strdup (device->host_name);
      result->host_port = device->host_port;
      result->network_device = device->network_device;
      result->acquisition_method = device->acquisition_method;
      result->is_authenticated_server = device->is_authenticated_server;
      result->show = device->show;
    }

  return result;
}

static void
pp_devices_list_free (PpDevicesList *result)
{
  GList *iter;

  if (result)
    {
      for (iter = result->devices; iter; iter = iter->next)
        pp_print_device_free ((PpPrintDevice *) iter->data);
      g_list_free (result->devices);
      g_free (result);
    }
}

static void
update_spinner_state (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkWidget *spinner;

  if (priv->cups_searching ||
      priv->remote_cups_host != NULL ||
      priv->snmp_host != NULL ||
      priv->samba_host != NULL ||
      priv->samba_authenticated_searching ||
      priv->samba_searching)
    {
      spinner = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "spinner");
      gtk_spinner_start (GTK_SPINNER (spinner));
      gtk_widget_show (spinner);
    }
  else
    {
      spinner = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "spinner");
      gtk_spinner_stop (GTK_SPINNER (spinner));
      gtk_widget_hide (spinner);
    }
}

static void
group_physical_devices_cb (gchar    ***device_uris,
                           gpointer    user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  TDevice                   *device, *tmp;
  gint                       i, j;

  if (device_uris)
    {
      for (i = 0; device_uris[i]; i++)
        {
          if (device_uris[i])
            {
              for (j = 0; device_uris[i][j]; j++)
                {
                  device = device_in_list (device_uris[i][j], priv->devices);
                  if (device)
                    break;
                }

              if (device)
                {
                  for (j = 0; device_uris[i][j]; j++)
                    {
                      tmp = device_in_list (device_uris[i][j], priv->new_devices);
                      if (tmp)
                        {
                          priv->new_devices = g_list_remove (priv->new_devices, tmp);
                          t_device_free (tmp);
                        }
                    }
                }
              else
                {
                  for (j = 0; device_uris[i][j]; j++)
                    {
                      tmp = device_in_list (device_uris[i][j], priv->new_devices);
                      if (tmp)
                        {
                          priv->new_devices = g_list_remove (priv->new_devices, tmp);
                          if (j == 0)
                            {
                              priv->devices = g_list_append (priv->devices, tmp);
                            }
                          else
                            {
                              t_device_free (tmp);
                            }
                        }
                    }
                }
            }
        }

      for (i = 0; device_uris[i]; i++)
        {
          for (j = 0; device_uris[i][j]; j++)
            {
              g_free (device_uris[i][j]);
            }

          g_free (device_uris[i]);
        }

      g_free (device_uris);
    }
  else
    {
      priv->devices = g_list_concat (priv->devices, priv->new_devices);
      priv->new_devices = NULL;
    }

  actualize_devices_list (dialog);
}

static void
group_physical_devices_dbus_cb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  GVariant   *output;
  GError     *error = NULL;
  gchar    ***result = NULL;
  gint        i, j;

  output = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                          res,
                                          &error);
  g_object_unref (source_object);

  if (output)
    {
      GVariant *array;

      g_variant_get (output, "(@aas)", &array);

      if (array)
        {
          GVariantIter *iter;
          GVariantIter *subiter;
          GVariant     *item;
          GVariant     *subitem;
          gchar        *device_uri;

          result = g_new0 (gchar **, g_variant_n_children (array) + 1);
          g_variant_get (array, "aas", &iter);
          i = 0;
          while ((item = g_variant_iter_next_value (iter)))
            {
              result[i] = g_new0 (gchar *, g_variant_n_children (item) + 1);
              g_variant_get (item, "as", &subiter);
              j = 0;
              while ((subitem = g_variant_iter_next_value (subiter)))
                {
                  g_variant_get (subitem, "s", &device_uri);

                  result[i][j] = device_uri;

                  g_variant_unref (subitem);
                  j++;
                }

              g_variant_unref (item);
              i++;
            }

          g_variant_unref (array);
        }

      g_variant_unref (output);
    }
  else if (error &&
           error->domain == G_DBUS_ERROR &&
           (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
            error->code == G_DBUS_ERROR_UNKNOWN_METHOD))
    {
      g_warning ("Install system-config-printer which provides \
DBus method \"GroupPhysicalDevices\" to group duplicates in device list.");
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        g_warning ("%s", error->message);
    }

  if (!error ||
      error->domain != G_IO_ERROR ||
      error->code != G_IO_ERROR_CANCELLED)
    group_physical_devices_cb (result, user_data);

  if (error)
    g_error_free (error);
}

static void
get_cups_devices_cb (GList    *devices,
                     gboolean  finished,
                     gboolean  cancelled,
                     gpointer  user_data)
{
  PpNewPrinterDialog         *dialog;
  PpNewPrinterDialogPrivate  *priv;
  GDBusConnection            *bus;
  GVariantBuilder             device_list;
  GVariantBuilder             device_hash;
  PpPrintDevice             **all_devices;
  PpPrintDevice              *pp_device;
  TDevice                    *device;
  GError                     *error = NULL;
  GList                      *iter;
  gint                        length, i;


  if (!cancelled)
    {
      dialog = (PpNewPrinterDialog *) user_data;
      priv = dialog->priv;

      if (finished)
        {
          priv->cups_searching = FALSE;
        }

      if (devices)
        {
          add_devices_to_list (dialog,
                               devices,
                               TRUE);

          length = g_list_length (priv->devices) + g_list_length (devices);
          if (length > 0)
            {
              all_devices = g_new0 (PpPrintDevice *, length);

              i = 0;
              for (iter = priv->devices; iter; iter = iter->next)
                {
                  device = (TDevice *) iter->data;
                  if (device)
                    {
                      all_devices[i] = g_new0 (PpPrintDevice, 1);
                      all_devices[i]->device_id = g_strdup (device->device_id);
                      all_devices[i]->device_make_and_model = g_strdup (device->device_make_and_model);
                      all_devices[i]->device_class = device->network_device ? g_strdup ("network") : strdup ("direct");
                      all_devices[i]->device_uri = g_strdup (device->device_uri);
                    }
                  i++;
                }

              for (iter = devices; iter; iter = iter->next)
                {
                  pp_device = (PpPrintDevice *) iter->data;
                  if (pp_device)
                    {
                      all_devices[i] = g_new0 (PpPrintDevice, 1);
                      all_devices[i]->device_id = g_strdup (pp_device->device_id);
                      all_devices[i]->device_make_and_model = g_strdup (pp_device->device_make_and_model);
                      all_devices[i]->device_class = g_strdup (pp_device->device_class);
                      all_devices[i]->device_uri = g_strdup (pp_device->device_uri);
                    }
                  i++;
                }

              bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
              if (bus)
                {
                  g_variant_builder_init (&device_list, G_VARIANT_TYPE ("a{sv}"));

                  for (i = 0; i < length; i++)
                    {
                      if (all_devices[i]->device_uri)
                        {
                          g_variant_builder_init (&device_hash, G_VARIANT_TYPE ("a{ss}"));

                          if (all_devices[i]->device_id)
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-id",
                                                   all_devices[i]->device_id);

                          if (all_devices[i]->device_make_and_model)
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-make-and-model",
                                                   all_devices[i]->device_make_and_model);

                          if (all_devices[i]->device_class)
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-class",
                                                   all_devices[i]->device_class);

                          g_variant_builder_add (&device_list,
                                                 "{sv}",
                                                 all_devices[i]->device_uri,
                                                 g_variant_builder_end (&device_hash));
                        }
                    }

                  g_dbus_connection_call (bus,
                                          SCP_BUS,
                                          SCP_PATH,
                                          SCP_IFACE,
                                          "GroupPhysicalDevices",
                                          g_variant_new ("(v)", g_variant_builder_end (&device_list)),
                                          G_VARIANT_TYPE ("(aas)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          priv->cancellable,
                                          group_physical_devices_dbus_cb,
                                          dialog);
                }
              else
                {
                  g_warning ("Failed to get system bus: %s", error->message);
                  g_error_free (error);
                  group_physical_devices_cb (NULL, user_data);
                }

              for (i = 0; i < length; i++)
                {
                  if (all_devices[i])
                    {
                      g_free (all_devices[i]->device_id);
                      g_free (all_devices[i]->device_make_and_model);
                      g_free (all_devices[i]->device_class);
                      g_free (all_devices[i]->device_uri);
                      g_free (all_devices[i]);
                    }
                }

              g_free (all_devices);
            }
          else
            {
              actualize_devices_list (dialog);
            }
        }
      else
        {
          actualize_devices_list (dialog);
        }
    }

  for (iter = devices; iter; iter = iter->next)
    pp_print_device_free ((PpPrintDevice *) iter->data);
  g_list_free (devices);
}

static void
get_snmp_devices_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  PpNewPrinterDialog        *dialog;
  PpNewPrinterDialogPrivate *priv;
  PpHost                    *host = (PpHost *) source_object;
  GError                    *error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_snmp_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      if ((gpointer) source_object == (gpointer) priv->snmp_host)
        priv->snmp_host = NULL;

      update_spinner_state (dialog);

      if (result->devices)
        {
          add_devices_to_list (dialog,
                               result->devices,
                               FALSE);
        }

      actualize_devices_list (dialog);

      pp_devices_list_free (result);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) priv->snmp_host)
            priv->snmp_host = NULL;

          update_spinner_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_remote_cups_devices_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  PpNewPrinterDialog        *dialog;
  PpNewPrinterDialogPrivate *priv;
  PpHost                    *host = (PpHost *) source_object;
  GError                    *error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_remote_cups_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      if ((gpointer) source_object == (gpointer) priv->remote_cups_host)
        priv->remote_cups_host = NULL;

      update_spinner_state (dialog);

      if (result->devices)
        {
          add_devices_to_list (dialog,
                               result->devices,
                               FALSE);
        }

      actualize_devices_list (dialog);

      pp_devices_list_free (result);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) priv->remote_cups_host)
            priv->remote_cups_host = NULL;

          update_spinner_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_samba_host_devices_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  PpNewPrinterDialogPrivate *priv;
  PpNewPrinterDialog        *dialog;
  PpDevicesList             *result;
  PpSamba                   *samba = (PpSamba *) source_object;
  GError                    *error = NULL;

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      if ((gpointer) source_object == (gpointer) priv->samba_host)
        priv->samba_host = NULL;

      update_spinner_state (dialog);

      if (result->devices)
        {
          add_devices_to_list (dialog,
                               result->devices,
                               FALSE);
        }

      actualize_devices_list (dialog);

      pp_devices_list_free (result);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) priv->samba_host)
            priv->samba_host = NULL;

          update_spinner_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_samba_devices_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PpNewPrinterDialogPrivate *priv;
  PpNewPrinterDialog        *dialog;
  PpDevicesList             *result;
  PpSamba                   *samba = (PpSamba *) source_object;
  GError                    *error = NULL;

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      priv->samba_searching = FALSE;
      update_spinner_state (dialog);

      if (result->devices)
        {
          add_devices_to_list (dialog,
                               result->devices,
                               FALSE);
        }

      actualize_devices_list (dialog);

      pp_devices_list_free (result);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          priv->samba_searching = FALSE;
          update_spinner_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_cups_devices (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;

  priv->cups_searching = TRUE;
  update_spinner_state (dialog);

  get_cups_devices_async (priv->cancellable,
                          get_cups_devices_cb,
                          dialog);
}

static gboolean
parse_uri (const gchar  *uri,
           gchar       **host,
           gint         *port)
{
  const gchar *tmp = NULL;
  gchar       *resulting_host = NULL;
  gchar       *port_string = NULL;
  gchar       *position;
  int          resulting_port = 631;

  if (g_strrstr (uri, "://"))
    tmp = g_strrstr (uri, "://") + 3;
  else
    tmp = uri;

  if (g_strrstr (tmp, "@"))
    tmp = g_strrstr (tmp, "@") + 1;

  if ((position = g_strrstr (tmp, "/")))
    {
      *position = '\0';
      resulting_host = g_strdup (tmp);
      *position = '/';
    }
  else
    resulting_host = g_strdup (tmp);

  if ((position = g_strrstr (resulting_host, ":")))
    {
      *position = '\0';
      port_string = position + 1;
    }

  if (port_string)
    resulting_port = atoi (port_string);

  *host = resulting_host;
  *port = resulting_port;

  return TRUE;
}

typedef struct
{
  PpNewPrinterDialog *dialog;
  gchar              *host_name;
  gint                host_port;
} THostSearchData;

static void
search_for_remote_printers_free (THostSearchData *data)
{
  g_free (data->host_name);
  g_free (data);
}

static gboolean
search_for_remote_printers (THostSearchData *data)
{
  PpNewPrinterDialogPrivate *priv = data->dialog->priv;

  if (priv->remote_host_cancellable != NULL)
    {
      g_cancellable_cancel (priv->remote_host_cancellable);
      g_clear_object (&priv->remote_host_cancellable);
    }

  priv->remote_host_cancellable = g_cancellable_new ();

  priv->remote_cups_host = pp_host_new (data->host_name, data->host_port);
  priv->snmp_host = pp_host_new (data->host_name, data->host_port);
  priv->samba_host = pp_samba_new (GTK_WINDOW (priv->dialog),
                                   data->host_name);

  update_spinner_state (data->dialog);

  pp_host_get_remote_cups_devices_async (priv->remote_cups_host,
                                         priv->remote_host_cancellable,
                                         get_remote_cups_devices_cb,
                                         data->dialog);

  pp_host_get_snmp_devices_async (priv->snmp_host,
                                  priv->remote_host_cancellable,
                                  get_snmp_devices_cb,
                                  data->dialog);

  pp_samba_get_devices_async (priv->samba_host,
                              TRUE,
                              priv->remote_host_cancellable,
                              get_samba_host_devices_cb,
                              data->dialog);

  priv->host_search_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
search_address (const gchar        *text,
                PpNewPrinterDialog *dialog,
                gboolean            delay_search)
{
  PpNewPrinterDialogPrivate  *priv = dialog->priv;
  gboolean                    found = FALSE;
  gboolean                    subfound;
  TDevice                    *device;
  GList                      *iter, *tmp;
  gchar                      *lowercase_name;
  gchar                      *lowercase_location;
  gchar                      *lowercase_text;
  gchar                     **words;
  gint                        words_length = 0;
  gint                        i;

  lowercase_text = g_ascii_strdown (text, -1);
  words = g_strsplit_set (lowercase_text, " ", -1);
  g_free (lowercase_text);

  if (words)
    {
      words_length = g_strv_length (words);

      for (iter = priv->devices; iter; iter = iter->next)
        {
          device = iter->data;

          lowercase_name = g_ascii_strdown (device->device_name, -1);
          if (device->device_location)
            lowercase_location = g_ascii_strdown (device->device_location, -1);
          else
            lowercase_location = NULL;

          subfound = TRUE;
          for (i = 0; words[i]; i++)
            {
              if (!g_strrstr (lowercase_name, words[i]) &&
                  (!lowercase_location || !g_strrstr (lowercase_location, words[i])))
                subfound = FALSE;
            }

          if (subfound)
            {
              device->show = TRUE;
              found = TRUE;
            }
          else
            {
              device->show = FALSE;
            }

          g_free (lowercase_location);
          g_free (lowercase_name);
        }

      g_strfreev (words);
  }

  if (!found && words_length == 1)
    {
      iter = priv->devices;
      while (iter)
        {
          device = iter->data;
          device->show = TRUE;

          if (device->acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER ||
              device->acquisition_method == ACQUISITION_METHOD_SNMP ||
              device->acquisition_method == ACQUISITION_METHOD_SAMBA_HOST)
            {
              tmp = iter;
              iter = iter->next;
              priv->devices = g_list_remove_link (priv->devices, tmp);
              g_list_free_full (tmp, t_device_free);
            }
          else
            iter = iter->next;
        }

      iter = priv->new_devices;
      while (iter)
        {
          device = iter->data;

          if (device->acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER ||
              device->acquisition_method == ACQUISITION_METHOD_SNMP ||
              device->acquisition_method == ACQUISITION_METHOD_SAMBA_HOST)
            {
              tmp = iter;
              iter = iter->next;
              priv->new_devices = g_list_remove_link (priv->new_devices, tmp);
              g_list_free_full (tmp, t_device_free);
            }
          else
            iter = iter->next;
        }

      if (text && text[0] != '\0')
        {
          gchar *host = NULL;
          gint   port = 631;

          parse_uri (text, &host, &port);

          if (host)
            {
              THostSearchData *search_data;

              search_data = g_new (THostSearchData, 1);
              search_data->host_name = host;
              search_data->host_port = port;
              search_data->dialog = dialog;

              if (priv->host_search_timeout_id != 0)
                {
                  g_source_remove (priv->host_search_timeout_id);
                  priv->host_search_timeout_id = 0;
                }

              if (delay_search)
                {
                  priv->host_search_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                                     HOST_SEARCH_DELAY,
                                                                     (GSourceFunc) search_for_remote_printers,
                                                                     search_data,
                                                                     (GDestroyNotify) search_for_remote_printers_free);
                }
              else
                {
                  search_for_remote_printers (search_data);
                  search_for_remote_printers_free (search_data);
                }
            }
        }
    }

  actualize_devices_list (dialog);
}

static void
search_entry_activated_cb (GtkEntry *entry,
                           gpointer  user_data)
{
  search_address (gtk_entry_get_text (entry),
                  PP_NEW_PRINTER_DIALOG (user_data),
                  FALSE);
}

static void
search_entry_changed_cb (GtkSearchEntry *entry,
                         gpointer        user_data)
{
  search_address (gtk_entry_get_text (GTK_ENTRY (entry)),
                  PP_NEW_PRINTER_DIALOG (user_data),
                  TRUE);
}

static gchar *
get_local_scheme_description_from_uri (gchar *device_uri)
{
  gchar *description = NULL;

  if (device_uri != NULL)
    {
      if (g_str_has_prefix (device_uri, "usb") ||
          g_str_has_prefix (device_uri, "hp:/usb/") ||
          g_str_has_prefix (device_uri, "hpfax:/usb/"))
        {
          /* Translators: The found device is a printer connected via USB */
          description = g_strdup (_("USB"));
        }
      else if (g_str_has_prefix (device_uri, "serial"))
        {
          /* Translators: The found device is a printer connected via serial port */
          description = g_strdup (_("Serial Port"));
        }
      else if (g_str_has_prefix (device_uri, "parallel") ||
               g_str_has_prefix (device_uri, "hp:/par/") ||
               g_str_has_prefix (device_uri, "hpfax:/par/"))
        {
          /* Translators: The found device is a printer connected via parallel port */
          description = g_strdup (_("Parallel Port"));
        }
      else if (g_str_has_prefix (device_uri, "bluetooth"))
        {
          /* Translators: The found device is a printer connected via Bluetooth */
          description = g_strdup (_("Bluetooth"));
        }
    }

  return description;
}

static void
actualize_devices_list (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeSelection  *selection;
  GtkListStore      *store;
  GtkTreeView       *treeview;
  GtkTreeIter        iter;
  GtkWidget         *widget;
  gboolean           no_device = TRUE;
  TDevice           *device;
  GList             *item;
  gchar             *description;

  treeview = (GtkTreeView *)
    gtk_builder_get_object (priv->builder, "devices-treeview");

  store = (GtkListStore *)
    gtk_builder_get_object (priv->builder, "devices-liststore");

  gtk_list_store_clear (store);

  for (item = priv->devices; item; item = item->next)
    {
      device = (TDevice *) item->data;

      if (device->display_name &&
          (device->device_id ||
           device->device_ppd ||
           (device->host_name &&
            device->acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER) ||
           device->acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
           device->acquisition_method == ACQUISITION_METHOD_SAMBA) &&
          device->show)
        {
          description = get_local_scheme_description_from_uri (device->device_uri);
          if (description == NULL)
            {
              if (device->device_location != NULL && device->device_location[0] != '\0')
                {
                  /* Translators: Location of found network printer (e.g. Kitchen, Reception) */
                  description = g_strdup_printf (_("Location: %s"), device->device_location);
                }
              else if (device->host_name != NULL && device->host_name[0] != '\0')
                {
                  /* Translators: Network address of found printer */
                  description = g_strdup_printf (_("Address: %s"), device->host_name);
                }
            }

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              DEVICE_GICON_COLUMN, device->network_device ? priv->remote_printer_icon : priv->local_printer_icon,
                              DEVICE_NAME_COLUMN, device->device_name,
                              DEVICE_DISPLAY_NAME_COLUMN, device->display_name,
                              DEVICE_DESCRIPTION_COLUMN, description,
                              -1);
          no_device = FALSE;

          g_free (description);
        }
      else if (device->is_authenticated_server &&
               device->host_name != NULL)
        {
          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              DEVICE_GICON_COLUMN, priv->authenticated_server_icon,
                              DEVICE_NAME_COLUMN, device->host_name,
                              DEVICE_DISPLAY_NAME_COLUMN, device->host_name,
                              /* Translators: This item is a server which needs authentication to show its printers */
                              DEVICE_DESCRIPTION_COLUMN, _("Server requires authentication"),
                              SERVER_NEEDS_AUTHENTICATION_COLUMN, TRUE,
                              -1);
          no_device = FALSE;
        }
    }

  widget = (GtkWidget *)
    gtk_builder_get_object (priv->builder, "stack");

  if (no_device &&
      !priv->cups_searching &&
      priv->remote_cups_host == NULL &&
      priv->snmp_host == NULL &&
      priv->samba_host == NULL &&
      !priv->samba_authenticated_searching &&
      !priv->samba_searching)
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "no-printers-page");
  else
    gtk_stack_set_visible_child_name (GTK_STACK (widget), "standard-page");

  if (!no_device &&
      gtk_tree_model_get_iter_first ((GtkTreeModel *) store, &iter) &&
      (selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))) != NULL)
    gtk_tree_selection_select_iter (selection, &iter);

  update_spinner_state (dialog);
}

static void
cups_get_dests_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  PpNewPrinterDialog        *dialog;
  PpNewPrinterDialogPrivate *priv;
  PpCupsDests               *dests;
  PpCups                    *cups = (PpCups *) source_object;
  GError                    *error = NULL;

  dests = pp_cups_get_dests_finish (cups, res, &error);
  g_object_unref (source_object);

  if (dests)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      priv->dests = dests->dests;
      priv->num_of_dests = dests->num_of_dests;

      get_cups_devices (dialog);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);

          g_warning ("%s", error->message);

          get_cups_devices (dialog);
        }

      g_error_free (error);
    }
}

static void
cell_data_func (GtkTreeViewColumn  *tree_column,
                GtkCellRenderer    *cell,
                GtkTreeModel       *tree_model,
                GtkTreeIter        *iter,
                gpointer            user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkWidget                 *treeview;
  gboolean                   selected = FALSE;
  gchar                     *name = NULL;
  gchar                     *description = NULL;
  gchar                     *text;

  treeview = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "devices-treeview");

  if (treeview != NULL)
    selected = gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                                                    iter);

  gtk_tree_model_get (tree_model, iter,
                      DEVICE_DISPLAY_NAME_COLUMN, &name,
                      DEVICE_DESCRIPTION_COLUMN, &description,
                      -1);

  if (name != NULL)
    {
      if (description != NULL)
        {
          if (selected)
            text = g_markup_printf_escaped ("<b>%s</b>\n<small>%s</small>",
                                            name,
                                            description);
          else
            text = g_markup_printf_escaped ("<b>%s</b>\n<small><span foreground=\"#555555\">%s</span></small>",
                                            name,
                                            description);
        }
      else
        {
          text = g_markup_printf_escaped ("<b>%s</b>\n ",
                                          name);
        }

      g_object_set (G_OBJECT (cell),
                    "markup", text,
                    NULL);

      g_free (text);
    }

  g_free (name);
  g_free (description);
}

static void
populate_devices_list (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeViewColumn         *column;
  GtkWidget                 *treeview;
  PpSamba                   *samba;
  GEmblem                   *emblem;
  PpCups                    *cups;
  GIcon                     *icon, *emblem_icon;

  treeview = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "devices-treeview");

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                    "changed", G_CALLBACK (device_selection_changed_cb), dialog);

  priv->local_printer_icon = g_themed_icon_new ("printer");
  priv->remote_printer_icon = g_themed_icon_new ("printer-network");

  icon = g_themed_icon_new ("network-server");
  emblem_icon = g_themed_icon_new ("changes-prevent");
  emblem = g_emblem_new (emblem_icon);

  priv->authenticated_server_icon = g_emblemed_icon_new (icon, emblem);

  g_object_unref (icon);
  g_object_unref (emblem_icon);
  g_object_unref (emblem);

  priv->icon_renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (priv->icon_renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
  gtk_cell_renderer_set_alignment (priv->icon_renderer, 1.0, 0.5);
  gtk_cell_renderer_set_padding (priv->icon_renderer, 4, 4);
  column = gtk_tree_view_column_new_with_attributes ("Icon", priv->icon_renderer,
                                                     "gicon", DEVICE_GICON_COLUMN, NULL);
  gtk_tree_view_column_set_max_width (column, -1);
  gtk_tree_view_column_set_min_width (column, 80);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);


  priv->text_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Devices", priv->text_renderer,
                                                     NULL);
  gtk_tree_view_column_set_cell_data_func (column, priv->text_renderer, cell_data_func,
                                           dialog, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups, priv->cancellable, cups_get_dests_cb, dialog);

  priv->samba_searching = TRUE;
  update_spinner_state (dialog);

  samba = pp_samba_new (GTK_WINDOW (priv->dialog), NULL);
  pp_samba_get_devices_async (samba, FALSE, priv->cancellable, get_samba_devices_cb, dialog);
}

static void
printer_add_async_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PpNewPrinterDialog        *dialog;
  GtkResponseType            response_id = GTK_RESPONSE_OK;
  PpNewPrinter              *new_printer = (PpNewPrinter *) source_object;
  gboolean                   success;
  GError                    *error = NULL;

  success = pp_new_printer_add_finish (new_printer, res, &error);
  g_object_unref (source_object);

  if (success)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);

      emit_response (dialog, response_id);
    }
  else
    {
      if (error->domain != G_IO_ERROR ||
          error->code != G_IO_ERROR_CANCELLED)
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);

          g_warning ("%s", error->message);

          response_id = GTK_RESPONSE_REJECT;

          emit_response (dialog, response_id);
        }

      g_error_free (error);
    }
}

static void
ppd_selection_cb (GtkDialog *_dialog,
                  gint       response_id,
                  gpointer   user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  PpNewPrinter              *new_printer;
  gchar                     *ppd_name;
  guint                      window_id = 0;

  ppd_name = pp_ppd_selection_dialog_get_ppd_name (priv->ppd_selection_dialog);
  pp_ppd_selection_dialog_free (priv->ppd_selection_dialog);
  priv->ppd_selection_dialog = NULL;

  if (ppd_name)
    {
      priv->new_device->device_ppd = ppd_name;

      emit_pre_response (dialog,
                         priv->new_device->device_name,
                         priv->new_device->device_location,
                         priv->new_device->device_make_and_model,
                         priv->new_device->network_device);

      window_id = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (priv->dialog)));

      new_printer = pp_new_printer_new ();
      g_object_set (new_printer,
                    "name", priv->new_device->device_name,
                    "original-name""", priv->new_device->device_original_name,
                    "device-uri", priv->new_device->device_uri,
                    "device-id", priv->new_device->device_id,
                    "ppd-name", priv->new_device->device_ppd,
                    "ppd-file-name", priv->new_device->device_ppd,
                    "info", priv->new_device->device_info,
                    "location", priv->new_device->device_location,
                    "make-and-model", priv->new_device->device_make_and_model,
                    "host-name", priv->new_device->host_name,
                    "host-port", priv->new_device->host_port,
                    "is-network-device", priv->new_device->network_device,
                    "window-id", window_id,
                    NULL);
      priv->cancellable = g_cancellable_new ();

      pp_new_printer_add_async (new_printer,
                                priv->cancellable,
                                printer_add_async_cb,
                                dialog);

      t_device_free (priv->new_device);
      priv->new_device = NULL;
    }
}

static void
new_printer_dialog_response_cb (GtkDialog *_dialog,
                                gint       response_id,
                                gpointer   user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  GtkWidget                 *treeview;
  TDevice                   *device = NULL;
  TDevice                   *tmp;
  GList                     *list_iter;
  gchar                     *device_name = NULL;

  gtk_widget_hide (GTK_WIDGET (_dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);

      treeview = (GtkWidget*)
        gtk_builder_get_object (priv->builder, "devices-treeview");

      if (treeview &&
          gtk_tree_selection_get_selected (
            gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), &model, &iter))
        {
          gtk_tree_model_get (model, &iter,
                              DEVICE_NAME_COLUMN, &device_name,
                              -1);
        }

      for (list_iter = priv->devices; list_iter; list_iter = list_iter->next)
        {
          tmp = (TDevice *) list_iter->data;
          if (tmp && g_strcmp0 (tmp->device_name, device_name) == 0)
            {
              device = tmp;
              break;
            }
        }

      if (device)
        {
          PpNewPrinter *new_printer;
          guint         window_id = 0;

          if (device->acquisition_method == ACQUISITION_METHOD_SAMBA ||
              device->acquisition_method == ACQUISITION_METHOD_SAMBA_HOST)
            {
              priv->new_device = t_device_copy (device);
              priv->ppd_selection_dialog =
                pp_ppd_selection_dialog_new (priv->parent,
                                             priv->list,
                                             NULL,
                                             ppd_selection_cb,
                                             dialog);
            }
          else
            {
              emit_pre_response (dialog,
                                 device->device_name,
                                 device->device_location,
                                 device->device_make_and_model,
                                 device->network_device);

              window_id = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (_dialog)));

              new_printer = pp_new_printer_new ();
              g_object_set (new_printer,
                            "name", device->device_name,
                            "original-name""", device->device_original_name,
                            "device-uri", device->device_uri,
                            "device-id", device->device_id,
                            "ppd-name", device->device_ppd,
                            "ppd-file-name", device->device_ppd,
                            "info", device->device_info,
                            "location", device->device_location,
                            "make-and-model", device->device_make_and_model,
                            "host-name", device->host_name,
                            "host-port", device->host_port,
                            "is-network-device", device->network_device,
                            "window-id", window_id,
                            NULL);

              priv->cancellable = g_cancellable_new ();

              pp_new_printer_add_async (new_printer,
                                        priv->cancellable,
                                        printer_add_async_cb,
                                        dialog);
            }
        }
    }
  else
    {
      emit_response (dialog, GTK_RESPONSE_CANCEL);
    }
}
