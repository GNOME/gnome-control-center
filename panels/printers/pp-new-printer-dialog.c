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

#define WID(s) GTK_WIDGET (gtk_builder_get_object (priv->builder, s))

#define AUTHENTICATION_PAGE "authentication-page"
#define ADDPRINTER_PAGE "addprinter-page"

static void     set_device (PpNewPrinterDialog *dialog,
                            PpPrintDevice      *device,
                            GtkTreeIter        *iter);
static void     replace_device (PpNewPrinterDialog *dialog,
                                PpPrintDevice      *old_device,
                                PpPrintDevice      *new_device);
static void     populate_devices_list (PpNewPrinterDialog *dialog);
static void     search_entry_activated_cb (GtkEntry *entry,
                                           gpointer  user_data);
static void     search_entry_changed_cb (GtkSearchEntry *entry,
                                         gpointer        user_data);
static void     new_printer_dialog_response_cb (GtkDialog *_dialog,
                                                gint       response_id,
                                                gpointer   user_data);
static void     update_dialog_state (PpNewPrinterDialog *dialog);
static void     add_devices_to_list (PpNewPrinterDialog  *dialog,
                                     GList               *devices);
static void     remove_device_from_list (PpNewPrinterDialog *dialog,
                                         const gchar        *device_name);

enum
{
  DEVICE_GICON_COLUMN = 0,
  DEVICE_NAME_COLUMN,
  DEVICE_DISPLAY_NAME_COLUMN,
  DEVICE_DESCRIPTION_COLUMN,
  SERVER_NEEDS_AUTHENTICATION_COLUMN,
  DEVICE_VISIBLE_COLUMN,
  DEVICE_COLUMN,
  DEVICE_N_COLUMNS
};

struct _PpNewPrinterDialogPrivate
{
  GtkBuilder *builder;

  GList *local_cups_devices;

  GtkListStore       *store;
  GtkTreeModelFilter *filter;
  GtkTreeView        *treeview;

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

  PpPrintDevice *new_device;

  PPDList *list;

  GtkWidget *dialog;
  GtkWindow *parent;

  GIcon *local_printer_icon;
  GIcon *remote_printer_icon;
  GIcon *authenticated_server_icon;

  PpHost  *snmp_host;
  PpHost  *socket_host;
  PpHost  *lpd_host;
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

  gtk_widget_show_all (priv->dialog);

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

  g_object_ref (samba);

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  data = (AuthSMBData *) user_data;

  if (result != NULL)
    {
      dialog = PP_NEW_PRINTER_DIALOG (data->dialog);
      priv = dialog->priv;

      priv->samba_authenticated_searching = FALSE;

      for (iter = result->devices; iter; iter = iter->next)
        {
          device = (PpPrintDevice *) iter->data;
          if (pp_print_device_is_authenticated_server (device))
            {
              cancelled = TRUE;
              break;
            }
        }

      if (!cancelled)
        {

          if (result->devices != NULL)
            {
              add_devices_to_list (dialog, result->devices);

              device = (PpPrintDevice *) result->devices->data;
              if (device != NULL)
                {
                  widget = WID ("search-entry");
                  gtk_entry_set_text (GTK_ENTRY (widget), pp_print_device_get_device_location (device));
                  search_entry_activated_cb (GTK_ENTRY (widget), dialog);
                }
            }
        }

      update_dialog_state (dialog);

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
          update_dialog_state (dialog);
        }

      g_error_free (error);
    }

  g_free (data->server_name);
  g_free (data);
}

static void
go_to_page (PpNewPrinterDialog *dialog,
            const gchar        *page)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkStack                  *stack;

  stack = GTK_STACK (WID ("dialog-stack"));
  gtk_stack_set_visible_child_name (stack, page);

  stack = GTK_STACK (WID ("headerbar-topright-buttons"));
  gtk_stack_set_visible_child_name (stack, page);

  stack = GTK_STACK (WID ("headerbar-topleft-buttons"));
  gtk_stack_set_visible_child_name (stack, page);
}

static gchar *
get_entry_text (const gchar        *object_name,
                PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;

  return g_strdup (gtk_entry_get_text (GTK_ENTRY (WID (object_name))));
}

static void
on_authenticate (GtkWidget *button,
                 gpointer   user_data)
{
  PpNewPrinterDialog        *dialog = PP_NEW_PRINTER_DIALOG (user_data);
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  gchar                     *hostname = NULL;
  gchar                     *username = NULL;
  gchar                     *password = NULL;

  username = get_entry_text ("username-entry", dialog);
  password = get_entry_text ("password-entry", dialog);

  if ((username == NULL) || (username[0] == '\0') ||
      (password == NULL) || (password[0] == '\0'))
    {
      g_clear_pointer (&username, g_free);
      g_clear_pointer (&password, g_free);
      return;
    }

  pp_samba_set_auth_info (PP_SAMBA (priv->samba_host), username, password);

  gtk_header_bar_set_title (GTK_HEADER_BAR (WID ("headerbar")), _("Add Printer"));
  go_to_page (dialog, ADDPRINTER_PAGE);

  g_object_get (PP_HOST (priv->samba_host), "hostname", &hostname, NULL);
  remove_device_from_list (dialog, hostname);
}

static void
on_authentication_required (PpHost   *host,
                            gpointer  user_data)
{
  PpNewPrinterDialogPrivate *priv;
  PpNewPrinterDialog        *dialog = PP_NEW_PRINTER_DIALOG (user_data);
  gchar                     *text, *hostname;

  priv = dialog->priv;

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (WID ("headerbar")), NULL);
  gtk_header_bar_set_title (GTK_HEADER_BAR (WID ("headerbar")), _("Unlock Print Server"));

  g_object_get (G_OBJECT (host), "hostname", &hostname, NULL);
  /* Translators: Samba server needs authentication of the user to show list of its printers. */
  text = g_strdup_printf (_("Unlock %s."), hostname);
  gtk_label_set_text (GTK_LABEL (WID ("authentication-title")), text);
  g_free (text);

  /* Translators: Samba server needs authentication of the user to show list of its printers. */
  text = g_strdup_printf (_("Enter username and password to view printers on %s."), hostname);
  gtk_label_set_text (GTK_LABEL (WID ("authentication-text")), text);
  g_free (hostname);
  g_free (text);

  go_to_page (dialog, AUTHENTICATION_PAGE);

  g_signal_connect (WID ("authenticate-button"), "clicked", G_CALLBACK (on_authenticate), dialog);
}

static void
auth_entries_changed (GtkEditable *editable,
                      gpointer     user_data)
{
  PpNewPrinterDialogPrivate *priv;
  PpNewPrinterDialog        *dialog = PP_NEW_PRINTER_DIALOG (user_data);
  gboolean                   can_authenticate = FALSE;
  gchar                     *username = NULL;
  gchar                     *password = NULL;

  priv = dialog->priv;

  username = get_entry_text ("username-entry", dialog);
  password = get_entry_text ("password-entry", dialog);

  can_authenticate = (username != NULL && username[0] != '\0' &&
                      password != NULL && password[0] != '\0');

  gtk_widget_set_sensitive (WID ("authenticate-button"), can_authenticate);

  g_clear_pointer (&username, g_free);
  g_clear_pointer (&password, g_free);
}

static void
on_go_back_button_clicked (GtkButton *button,
                           gpointer   user_data)
{
  PpNewPrinterDialog        *dialog = PP_NEW_PRINTER_DIALOG (user_data);
  PpNewPrinterDialogPrivate *priv = dialog->priv;

  pp_samba_set_auth_info (priv->samba_host, NULL, NULL);
  g_clear_object (&priv->samba_host);

  go_to_page (dialog, ADDPRINTER_PAGE);
  gtk_header_bar_set_title (GTK_HEADER_BAR (WID ("headerbar")), _("Add Printer"));
  gtk_widget_set_sensitive (WID ("new-printer-add-button"), FALSE);

  gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (priv->treeview));
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
  gchar                     *server_name = NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
  gtk_widget_set_sensitive (WID ("authenticate-button"), FALSE);
  gtk_widget_grab_focus (WID ("username-entry"));

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (priv->treeview), &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          DEVICE_NAME_COLUMN, &server_name,
                          -1);

      if (server_name != NULL)
        {
          g_clear_object (&priv->samba_host);

          priv->samba_host = pp_samba_new (server_name);
          g_signal_connect_object (priv->samba_host,
                                   "authentication-required",
                                   G_CALLBACK (on_authentication_required),
                                   dialog, 0);

          priv->samba_authenticated_searching = TRUE;
          update_dialog_state (dialog);

          data = g_new (AuthSMBData, 1);
          data->server_name = server_name;
          data->dialog = dialog;

          pp_samba_get_devices_async (priv->samba_host,
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
  gchar                     *objects[] = { "dialog",
                                           "devices-liststore",
                                           "devices-model-filter",
                                           NULL };
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
  priv->dialog = WID ("dialog");

  priv->treeview = GTK_TREE_VIEW (WID ("devices-treeview"));

  priv->store = GTK_LIST_STORE (gtk_builder_get_object (priv->builder, "devices-liststore"));

  priv->filter = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (priv->builder, "devices-model-filter"));

  /* Connect signals */
  g_signal_connect (priv->dialog, "response", G_CALLBACK (new_printer_dialog_response_cb), dialog);

  widget = WID ("search-entry");
  g_signal_connect (widget, "activate", G_CALLBACK (search_entry_activated_cb), dialog);
  g_signal_connect (widget, "search-changed", G_CALLBACK (search_entry_changed_cb), dialog);

  widget = WID ("unlock-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (authenticate_samba_server), dialog);

  /* Authentication form widgets */
  g_signal_connect (WID ("username-entry"), "changed", G_CALLBACK (auth_entries_changed), dialog);
  g_signal_connect (WID ("password-entry"), "changed", G_CALLBACK (auth_entries_changed), dialog);
  g_signal_connect (WID ("go-back-button"), "clicked", G_CALLBACK (on_go_back_button_clicked), dialog);

  /* Set junctions */
  widget = WID ("scrolledwindow1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

  widget = WID ("toolbar1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  /* Fill with data */
  populate_devices_list (dialog);
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

  g_list_free_full (priv->local_cups_devices, (GDestroyNotify) g_object_unref);
  priv->local_cups_devices = NULL;

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
  GtkWidget                 *widget;
  GtkWidget                 *stack;
  gboolean                   authentication_needed;
  gboolean                   selected;

  selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (priv->treeview),
                                              &model,
                                              &iter);

  if (selected)
    {
      gtk_tree_model_get (model, &iter,
                          SERVER_NEEDS_AUTHENTICATION_COLUMN, &authentication_needed,
                          -1);

      widget = WID ("new-printer-add-button");
      gtk_widget_set_sensitive (widget, selected);

      widget = WID ("unlock-button");
      gtk_widget_set_sensitive (widget, authentication_needed);

      stack = WID ("headerbar-topright-buttons");

      if (authentication_needed)
        gtk_stack_set_visible_child_name (GTK_STACK (stack), "unlock-button");
      else
        gtk_stack_set_visible_child_name (GTK_STACK (stack), ADDPRINTER_PAGE);
    }
}

static void
remove_device_from_list (PpNewPrinterDialog *dialog,
                         const gchar        *device_name)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  PpPrintDevice             *device;
  GtkTreeIter                iter;
  gboolean                   cont;

  cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter);
  while (cont)
    {
      gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                          DEVICE_COLUMN, &device,
                          -1);

      if (g_strcmp0 (pp_print_device_get_device_name (device), device_name) == 0)
        {
          gtk_list_store_remove (priv->store, &iter);
          g_object_unref (device);
          break;
        }

      g_object_unref (device);

      cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter);
    }

  update_dialog_state (dialog);
}

static gboolean
prepend_original_name (GtkTreeModel *model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      data)
{
  PpPrintDevice  *device;
  GList         **list = data;

  gtk_tree_model_get (model, iter,
                      DEVICE_COLUMN, &device,
                      -1);

  *list = g_list_prepend (*list, g_strdup (pp_print_device_get_device_original_name (device)));

  g_object_unref (device);

  return FALSE;
}

static void
add_device_to_list (PpNewPrinterDialog *dialog,
                    PpPrintDevice      *device)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  PpPrintDevice             *store_device;
  GList                     *original_names_list = NULL;
  gchar                     *canonicalized_name = NULL;
  gchar                     *host_name;
  gint                       acquisistion_method;

  if (device)
    {
      if (pp_print_device_get_host_name (device) == NULL)
        {
          host_name = guess_device_hostname (device);
          g_object_set (device, "host-name", host_name, NULL);
          g_free (host_name);
        }

      acquisistion_method = pp_print_device_get_acquisition_method (device);
      if (pp_print_device_get_device_id (device) ||
          pp_print_device_get_device_ppd (device) ||
          (pp_print_device_get_host_name (device) &&
           acquisistion_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER) ||
           acquisistion_method == ACQUISITION_METHOD_SAMBA_HOST ||
           acquisistion_method == ACQUISITION_METHOD_SAMBA ||
          (pp_print_device_get_device_uri (device) &&
           (acquisistion_method == ACQUISITION_METHOD_JETDIRECT ||
            acquisistion_method == ACQUISITION_METHOD_LPD)))
        {
          g_object_set (device,
                        "device-original-name", pp_print_device_get_device_name (device),
                        NULL);

          gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                                  prepend_original_name,
                                  &original_names_list);

          original_names_list = g_list_reverse (original_names_list);

          canonicalized_name = canonicalize_device_name (original_names_list,
                                                         priv->local_cups_devices,
                                                         priv->dests,
                                                         priv->num_of_dests,
                                                         device);

          g_list_free_full (original_names_list, g_free);

          g_object_set (device,
                        "display-name", canonicalized_name,
                        "device-name", canonicalized_name,
                        NULL);

          g_free (canonicalized_name);

          if (pp_print_device_get_acquisition_method (device) == ACQUISITION_METHOD_DEFAULT_CUPS_SERVER)
            priv->local_cups_devices = g_list_append (priv->local_cups_devices, g_object_ref (device));
          else
            set_device (dialog, device, NULL);
        }
      else if (pp_print_device_is_authenticated_server (device) &&
               pp_print_device_get_host_name (device) != NULL)
        {
          store_device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                       "device-name", pp_print_device_get_host_name (device),
                                       "host-name", pp_print_device_get_host_name (device),
                                       "is-authenticated-server", pp_print_device_is_authenticated_server (device),
                                       NULL);

          set_device (dialog, store_device, NULL);

          g_object_unref (store_device);
        }
    }
}

static void
add_devices_to_list (PpNewPrinterDialog  *dialog,
                     GList               *devices)
{
  GList *iter;

  for (iter = devices; iter; iter = iter->next)
    {
      add_device_to_list (dialog, (PpPrintDevice *) iter->data);
    }
}

static PpPrintDevice *
device_in_list (gchar *device_uri,
                GList *device_list)
{
  PpPrintDevice *device;
  GList         *iter;

  for (iter = device_list; iter; iter = iter->next)
    {
      device = (PpPrintDevice *) iter->data;
      /* GroupPhysicalDevices returns uris without port numbers */
      if (pp_print_device_get_device_uri (device) != NULL &&
          g_str_has_prefix (pp_print_device_get_device_uri (device), device_uri))
        return g_object_ref (device);
    }

  return NULL;
}

static PpPrintDevice *
device_in_liststore (gchar        *device_uri,
                     GtkListStore *device_liststore)
{
  PpPrintDevice *device;
  GtkTreeIter    iter;
  gboolean       cont;

  cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (device_liststore), &iter);
  while (cont)
    {
      gtk_tree_model_get (GTK_TREE_MODEL (device_liststore), &iter,
                          DEVICE_COLUMN, &device,
                          -1);

      /* GroupPhysicalDevices returns uris without port numbers */
      if (pp_print_device_get_device_uri (device) != NULL &&
          g_str_has_prefix (pp_print_device_get_device_uri (device), device_uri))
        {
          return device;
        }

      g_object_unref (device);

      cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (device_liststore), &iter);
    }

  return NULL;
}

static void
update_dialog_state (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeIter                iter;
  GtkWidget                 *header;
  GtkWidget                 *stack;
  gboolean                   searching;

  searching = priv->cups_searching ||
              priv->remote_cups_host != NULL ||
              priv->snmp_host != NULL ||
              priv->socket_host != NULL ||
              priv->lpd_host != NULL ||
              priv->samba_host != NULL ||
              priv->samba_authenticated_searching ||
              priv->samba_searching;

  header = WID ("headerbar");
  stack = WID ("stack");

  if (searching)
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (header), _("Searching for Printers"));
    }
  else
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (header), NULL);
    }

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter))
      gtk_stack_set_visible_child_name (GTK_STACK (stack), "standard-page");
  else
      gtk_stack_set_visible_child_name (GTK_STACK (stack), searching ? "loading-page" : "no-printers-page");
}

static void
group_physical_devices_cb (gchar    ***device_uris,
                           gpointer    user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  PpPrintDevice             *device, *better_device;
  GList                     *iter;
  gint                       i, j;

  if (device_uris != NULL)
    {
      for (i = 0; device_uris[i] != NULL; i++)
        {
          /* Is there any device in this sublist? */
          if (device_uris[i][0] != NULL)
            {
              device = NULL;
              for (j = 0; device_uris[i][j] != NULL; j++)
                {
                  device = device_in_liststore (device_uris[i][j], priv->store);
                  if (device != NULL)
                    break;
                }

              /* Is this sublist represented in the current list of devices? */
              if (device != NULL)
                {
                  /* Is there better device in the sublist? */
                  if (j != 0)
                    {
                      better_device = device_in_list (device_uris[i][0], priv->local_cups_devices);
                      replace_device (dialog, device, better_device);
                      g_object_unref (better_device);
                    }

                  g_object_unref (device);
                }
              else
                {
                  device = device_in_list (device_uris[i][0], priv->local_cups_devices);
                  if (device != NULL)
                    {
                      set_device (dialog, device, NULL);
                      g_object_unref (device);
                    }
                }
            }
        }

      for (i = 0; device_uris[i] != NULL; i++)
        g_strfreev (device_uris[i]);

      g_free (device_uris);
    }
  else
    {
      for (iter = priv->local_cups_devices; iter != NULL; iter = iter->next)
        set_device (dialog, (PpPrintDevice *) iter->data, NULL);
      g_list_free_full (priv->local_cups_devices, g_object_unref);
      priv->local_cups_devices = NULL;
    }

  update_dialog_state (dialog);
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
  PpPrintDevice              *device;
  const gchar                *device_class;
  GtkTreeIter                 iter;
  gboolean                    cont;
  GError                     *error = NULL;
  GList                      *liter;
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
          add_devices_to_list (dialog, devices);

          length = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (priv->store), NULL) + g_list_length (priv->local_cups_devices);
          if (length > 0)
            {
              all_devices = g_new0 (PpPrintDevice *, length);

              i = 0;
              cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter);
              while (cont)
                {
                  gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                                      DEVICE_COLUMN, &device,
                                      -1);

                  all_devices[i] = g_object_new (PP_TYPE_PRINT_DEVICE,
                                                 "device-id", pp_print_device_get_device_id (device),
                                                 "device-make-and-model", pp_print_device_get_device_make_and_model (device),
                                                 "is-network-device", pp_print_device_is_network_device (device),
                                                 "device-uri", pp_print_device_get_device_uri (device),
                                                 NULL);
                  i++;

                  g_object_unref (device);

                  cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter);
                }

              for (liter = priv->local_cups_devices; liter != NULL; liter = liter->next)
                {
                  pp_device = (PpPrintDevice *) liter->data;
                  if (pp_device != NULL)
                    {
                      all_devices[i] = g_object_new (PP_TYPE_PRINT_DEVICE,
                                                     "device-id", pp_print_device_get_device_id (pp_device),
                                                     "device-make-and-model", pp_print_device_get_device_make_and_model (pp_device),
                                                     "is-network-device", pp_print_device_is_network_device (pp_device),
                                                     "device-uri", pp_print_device_get_device_uri (pp_device),
                                                     NULL);
                      i++;
                    }
                }

              bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
              if (bus)
                {
                  g_variant_builder_init (&device_list, G_VARIANT_TYPE ("a{sv}"));

                  for (i = 0; i < length; i++)
                    {
                      if (pp_print_device_get_device_uri (all_devices[i]))
                        {
                          g_variant_builder_init (&device_hash, G_VARIANT_TYPE ("a{ss}"));

                          if (pp_print_device_get_device_id (all_devices[i]))
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-id",
                                                   pp_print_device_get_device_id (all_devices[i]));

                          if (pp_print_device_get_device_make_and_model (all_devices[i]))
                            g_variant_builder_add (&device_hash,
                                                   "{ss}",
                                                   "device-make-and-model",
                                                   pp_print_device_get_device_make_and_model (all_devices[i]));

                          if (pp_print_device_is_network_device (all_devices[i]))
                            device_class = "network";
                          else
                            device_class = "direct";

                          g_variant_builder_add (&device_hash,
                                                 "{ss}",
                                                 "device-class",
                                                 device_class);

                          g_variant_builder_add (&device_list,
                                                 "{sv}",
                                                 pp_print_device_get_device_uri (all_devices[i]),
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
                g_object_unref (all_devices[i]);
              g_free (all_devices);
            }
          else
            {
              update_dialog_state (dialog);
            }
        }
      else
        {
          update_dialog_state (dialog);
        }
    }

  g_list_free_full (devices, (GDestroyNotify) g_object_unref);
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

      add_devices_to_list (dialog, result->devices);

      update_dialog_state (dialog);

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

          update_dialog_state (dialog);
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

      add_devices_to_list (dialog, result->devices);

      update_dialog_state (dialog);

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

          update_dialog_state (dialog);
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

      add_devices_to_list (dialog, result->devices);

      update_dialog_state (dialog);

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

          update_dialog_state (dialog);
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

      add_devices_to_list (dialog, result->devices);

      update_dialog_state (dialog);

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

          update_dialog_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_jetdirect_devices_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  PpNewPrinterDialog        *dialog;
  PpNewPrinterDialogPrivate *priv;
  PpHost                    *host = (PpHost *) source_object;
  GError                    *error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_jetdirect_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result != NULL)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      if ((gpointer) source_object == (gpointer) priv->socket_host)
        priv->socket_host = NULL;

      add_devices_to_list (dialog, result->devices);

      update_dialog_state (dialog);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) priv->socket_host)
            priv->socket_host = NULL;

          update_dialog_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_lpd_devices_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  PpNewPrinterDialog        *dialog;
  PpNewPrinterDialogPrivate *priv;
  PpHost                    *host = (PpHost *) source_object;
  GError                    *error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_lpd_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result != NULL)
    {
      dialog = PP_NEW_PRINTER_DIALOG (user_data);
      priv = dialog->priv;

      if ((gpointer) source_object == (gpointer) priv->lpd_host)
        priv->lpd_host = NULL;

      add_devices_to_list (dialog, result->devices);

      update_dialog_state (dialog);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          dialog = PP_NEW_PRINTER_DIALOG (user_data);
          priv = dialog->priv;

          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) priv->lpd_host)
            priv->lpd_host = NULL;

          update_dialog_state (dialog);
        }

      g_error_free (error);
    }
}

static void
get_cups_devices (PpNewPrinterDialog *dialog)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;

  priv->cups_searching = TRUE;
  update_dialog_state (dialog);

  get_cups_devices_async (priv->cancellable,
                          get_cups_devices_cb,
                          dialog);
}

static gboolean
parse_uri (const gchar  *uri,
           gchar       **scheme,
           gchar       **host,
           gint         *port)
{
  const gchar *tmp = NULL;
  gchar       *resulting_host = NULL;
  gchar       *position;

  *port = PP_HOST_UNSET_PORT;

  position = g_strrstr (uri, "://");
  if (position != NULL)
    {
      *scheme = g_strndup (uri, position - uri);
      tmp = position + 3;
    }
  else
    {
      tmp = uri;
    }

  if (g_strrstr (tmp, "@"))
    tmp = g_strrstr (tmp, "@") + 1;

  if ((position = g_strrstr (tmp, "/")))
    {
      *position = '\0';
      resulting_host = g_strdup (tmp);
      *position = '/';
    }
  else
    {
      resulting_host = g_strdup (tmp);
    }

  if ((position = g_strrstr (resulting_host, ":")))
    {
      *position = '\0';
      *port = atoi (position + 1);
    }

  *host = g_uri_unescape_string (resulting_host,
                                 G_URI_RESERVED_CHARS_GENERIC_DELIMITERS
                                 G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS);

  g_free (resulting_host);

  return TRUE;
}

typedef struct
{
  PpNewPrinterDialog *dialog;
  gchar              *host_scheme;
  gchar              *host_name;
  gint                host_port;
} THostSearchData;

static void
search_for_remote_printers_free (THostSearchData *data)
{
  g_free (data->host_scheme);
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

  priv->remote_cups_host = pp_host_new (data->host_name);
  priv->snmp_host = pp_host_new (data->host_name);
  priv->socket_host = pp_host_new (data->host_name);
  priv->lpd_host = pp_host_new (data->host_name);

  if (data->host_port != PP_HOST_UNSET_PORT)
    {
      g_object_set (priv->remote_cups_host, "port", data->host_port, NULL);
      g_object_set (priv->snmp_host, "port", data->host_port, NULL);

      /* Accept port different from the default one only if user specifies
       * scheme (for socket and lpd printers).
       */
      if (data->host_scheme != NULL &&
          g_ascii_strcasecmp (data->host_scheme, "socket") == 0)
        g_object_set (priv->socket_host, "port", data->host_port, NULL);

      if (data->host_scheme != NULL &&
          g_ascii_strcasecmp (data->host_scheme, "lpd") == 0)
        g_object_set (priv->lpd_host, "port", data->host_port, NULL);
    }

  priv->samba_host = pp_samba_new (data->host_name);

  update_dialog_state (data->dialog);

  pp_host_get_remote_cups_devices_async (priv->remote_cups_host,
                                         priv->remote_host_cancellable,
                                         get_remote_cups_devices_cb,
                                         data->dialog);

  pp_host_get_snmp_devices_async (priv->snmp_host,
                                  priv->remote_host_cancellable,
                                  get_snmp_devices_cb,
                                  data->dialog);

  pp_host_get_jetdirect_devices_async (priv->socket_host,
                                       priv->remote_host_cancellable,
                                       get_jetdirect_devices_cb,
                                       data->dialog);

  pp_host_get_lpd_devices_async (priv->lpd_host,
                                 priv->remote_host_cancellable,
                                 get_lpd_devices_cb,
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
  PpPrintDevice              *device;
  GtkTreeIter                 iter;
  gboolean                    found = FALSE;
  gboolean                    subfound;
  gboolean                    next_set;
  gboolean                    cont;
  gchar                      *lowercase_name;
  gchar                      *lowercase_location;
  gchar                      *lowercase_text;
  gchar                     **words;
  gint                        words_length = 0;
  gint                        i;
  gint                        acquisition_method;

  lowercase_text = g_ascii_strdown (text, -1);
  words = g_strsplit_set (lowercase_text, " ", -1);
  g_free (lowercase_text);

  if (words)
    {
      words_length = g_strv_length (words);

      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter);
      while (cont)
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          lowercase_name = g_ascii_strdown (pp_print_device_get_device_name (device), -1);
          if (pp_print_device_get_device_location (device))
            lowercase_location = g_ascii_strdown (pp_print_device_get_device_location (device), -1);
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
            found = TRUE;

          gtk_list_store_set (GTK_LIST_STORE (priv->store), &iter,
                              DEVICE_VISIBLE_COLUMN, subfound,
                              -1);

          g_free (lowercase_location);
          g_free (lowercase_name);
          g_object_unref (device);

          cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter);
        }

      g_strfreev (words);
  }

  /*
   * The given word is probably an address since it was not found among
   * already present devices.
   */
  if (!found && words_length == 1)
    {
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter);
      while (cont)
        {
          next_set = FALSE;
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          gtk_list_store_set (GTK_LIST_STORE (priv->store), &iter,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              -1);

          acquisition_method = pp_print_device_get_acquisition_method (device);
          g_object_unref (device);
          if (acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER ||
              acquisition_method == ACQUISITION_METHOD_SNMP ||
              acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
              acquisition_method == ACQUISITION_METHOD_LPD ||
              acquisition_method == ACQUISITION_METHOD_SAMBA_HOST)
            {
              if (!gtk_list_store_remove (priv->store, &iter))
                break;
              else
                next_set = TRUE;
            }

          if (!next_set)
            cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter);
        }

      if (text && text[0] != '\0')
        {
          gchar *scheme = NULL;
          gchar *host = NULL;
          gint   port;

          parse_uri (text, &scheme, &host, &port);

          if (host)
            {
              THostSearchData *search_data;

              search_data = g_new (THostSearchData, 1);
              search_data->host_scheme = scheme;
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
set_device (PpNewPrinterDialog *dialog,
            PpPrintDevice      *device,
            GtkTreeIter        *iter)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeIter                titer;
  gchar                     *description;
  gint                       acquisition_method;

  if (device != NULL)
    {
      acquisition_method = pp_print_device_get_acquisition_method (device);
      if (pp_print_device_get_display_name (device) &&
          (pp_print_device_get_device_id (device) ||
           pp_print_device_get_device_ppd (device) ||
           (pp_print_device_get_host_name (device) &&
            acquisition_method == ACQUISITION_METHOD_REMOTE_CUPS_SERVER) ||
           (pp_print_device_get_device_uri (device) &&
            (acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
             acquisition_method == ACQUISITION_METHOD_LPD)) ||
           acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
           acquisition_method == ACQUISITION_METHOD_SAMBA))
        {
          description = get_local_scheme_description_from_uri (pp_print_device_get_device_uri (device));
          if (description == NULL)
            {
              if (pp_print_device_get_device_location (device) != NULL && pp_print_device_get_device_location (device)[0] != '\0')
                {
                  /* Translators: Location of found network printer (e.g. Kitchen, Reception) */
                  description = g_strdup_printf (_("Location: %s"), pp_print_device_get_device_location (device));
                }
              else if (pp_print_device_get_host_name (device) != NULL && pp_print_device_get_host_name (device)[0] != '\0')
                {
                  /* Translators: Network address of found printer */
                  description = g_strdup_printf (_("Address: %s"), pp_print_device_get_host_name (device));
                }
            }

          if (iter == NULL)
            gtk_list_store_append (priv->store, &titer);

          gtk_list_store_set (priv->store, iter == NULL ? &titer : iter,
                              DEVICE_GICON_COLUMN, pp_print_device_is_network_device (device) ? priv->remote_printer_icon : priv->local_printer_icon,
                              DEVICE_NAME_COLUMN, pp_print_device_get_device_name (device),
                              DEVICE_DISPLAY_NAME_COLUMN, pp_print_device_get_display_name (device),
                              DEVICE_DESCRIPTION_COLUMN, description,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              DEVICE_COLUMN, device,
                              -1);

          g_free (description);
        }
      else if (pp_print_device_is_authenticated_server (device) &&
               pp_print_device_get_host_name (device) != NULL)
        {
          if (iter == NULL)
            gtk_list_store_append (priv->store, &titer);

          gtk_list_store_set (priv->store, iter == NULL ? &titer : iter,
                              DEVICE_GICON_COLUMN, priv->authenticated_server_icon,
                              DEVICE_NAME_COLUMN, pp_print_device_get_host_name (device),
                              DEVICE_DISPLAY_NAME_COLUMN, pp_print_device_get_host_name (device),
                              /* Translators: This item is a server which needs authentication to show its printers */
                              DEVICE_DESCRIPTION_COLUMN, _("Server requires authentication"),
                              SERVER_NEEDS_AUTHENTICATION_COLUMN, TRUE,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              DEVICE_COLUMN, device,
                              -1);
        }
    }
}

static void
replace_device (PpNewPrinterDialog *dialog,
                PpPrintDevice      *old_device,
                PpPrintDevice      *new_device)
{
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  PpPrintDevice             *device;
  GtkTreeIter                iter;
  gboolean                   cont;

  if (old_device != NULL && new_device != NULL)
    {
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->store), &iter);
      while (cont)
        {
          gtk_tree_model_get (GTK_TREE_MODEL (priv->store), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          if (old_device == device)
            {
              set_device (dialog, new_device, &iter);
              g_object_unref (device);
              break;
            }

          g_object_unref (device);

          cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->store), &iter);
        }
    }
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
row_activated_cb (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  GtkWidget                 *widget;
  gboolean                   authentication_needed;
  gboolean                   selected;

  selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (priv->treeview),
                                              &model,
                                              &iter);

  if (selected)
    {
      gtk_tree_model_get (model, &iter, SERVER_NEEDS_AUTHENTICATION_COLUMN, &authentication_needed, -1);

      if (authentication_needed)
        {
          widget = WID ("unlock-button");
          authenticate_samba_server (GTK_BUTTON (widget), dialog);
        }
      else
        {
          gtk_dialog_response (GTK_DIALOG (priv->dialog), GTK_RESPONSE_OK);
        }
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
  gboolean                   selected = FALSE;
  gchar                     *name = NULL;
  gchar                     *description = NULL;
  gchar                     *text;

  selected = gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (priv->treeview), iter);

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
  PpSamba                   *samba;
  GEmblem                   *emblem;
  PpCups                    *cups;
  GIcon                     *icon, *emblem_icon;

  g_signal_connect (gtk_tree_view_get_selection (priv->treeview),
                    "changed", G_CALLBACK (device_selection_changed_cb), dialog);

  g_signal_connect (priv->treeview,
                    "row-activated", G_CALLBACK (row_activated_cb), dialog);

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
  gtk_tree_view_append_column (priv->treeview, column);


  priv->text_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Devices", priv->text_renderer,
                                                     NULL);
  gtk_tree_view_column_set_cell_data_func (column, priv->text_renderer, cell_data_func,
                                           dialog, NULL);
  gtk_tree_view_append_column (priv->treeview, column);

  gtk_tree_model_filter_set_visible_column (priv->filter, DEVICE_VISIBLE_COLUMN);

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups, priv->cancellable, cups_get_dests_cb, dialog);

  priv->samba_searching = TRUE;
  update_dialog_state (dialog);

  samba = pp_samba_new (NULL);
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
  GList                     *original_names_list = NULL;
  gchar                     *ppd_name;
  gchar                     *ppd_display_name;
  gchar                     *printer_name;
  guint                      window_id = 0;
  gint                       acquisition_method;

  ppd_name = pp_ppd_selection_dialog_get_ppd_name (priv->ppd_selection_dialog);
  ppd_display_name = pp_ppd_selection_dialog_get_ppd_display_name (priv->ppd_selection_dialog);
  pp_ppd_selection_dialog_free (priv->ppd_selection_dialog);
  priv->ppd_selection_dialog = NULL;

  if (ppd_name)
    {
      g_object_set (priv->new_device, "device-ppd", ppd_name, NULL);

      acquisition_method = pp_print_device_get_acquisition_method (priv->new_device);
      if ((acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
           acquisition_method == ACQUISITION_METHOD_LPD) &&
          ppd_display_name != NULL)
        {
          g_object_set (priv->new_device,
                        "device-name", ppd_display_name,
                        "device-original-name", ppd_display_name,
                        NULL);

          gtk_tree_model_foreach (GTK_TREE_MODEL (priv->store),
                                  prepend_original_name,
                                  &original_names_list);

          original_names_list = g_list_reverse (original_names_list);

          printer_name = canonicalize_device_name (original_names_list,
                                                   priv->local_cups_devices,
                                                   priv->dests,
                                                   priv->num_of_dests,
                                                   priv->new_device);

          g_list_free_full (original_names_list, g_free);

          g_object_set (priv->new_device,
                        "device-name", printer_name,
                        "device-original-name", printer_name,
                        NULL);

          g_free (printer_name);
        }

      emit_pre_response (dialog,
                         pp_print_device_get_device_name (priv->new_device),
                         pp_print_device_get_device_location (priv->new_device),
                         pp_print_device_get_device_make_and_model (priv->new_device),
                         pp_print_device_is_network_device (priv->new_device));

      window_id = (guint) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (priv->dialog)))));

      new_printer = pp_new_printer_new ();
      g_object_set (new_printer,
                    "name", pp_print_device_get_device_name (priv->new_device),
                    "original-name", pp_print_device_get_device_original_name (priv->new_device),
                    "device-uri", pp_print_device_get_device_uri (priv->new_device),
                    "device-id", pp_print_device_get_device_id (priv->new_device),
                    "ppd-name", pp_print_device_get_device_ppd (priv->new_device),
                    "ppd-file-name", pp_print_device_get_device_ppd (priv->new_device),
                    "info", pp_print_device_get_device_info (priv->new_device),
                    "location", pp_print_device_get_device_location (priv->new_device),
                    "make-and-model", pp_print_device_get_device_make_and_model (priv->new_device),
                    "host-name", pp_print_device_get_host_name (priv->new_device),
                    "host-port", pp_print_device_get_host_port (priv->new_device),
                    "is-network-device", pp_print_device_is_network_device (priv->new_device),
                    "window-id", window_id,
                    NULL);
      priv->cancellable = g_cancellable_new ();

      pp_new_printer_add_async (new_printer,
                                priv->cancellable,
                                printer_add_async_cb,
                                dialog);

      g_clear_object (&priv->new_device);
    }
}

static void
new_printer_dialog_response_cb (GtkDialog *_dialog,
                                gint       response_id,
                                gpointer   user_data)
{
  PpNewPrinterDialog        *dialog = (PpNewPrinterDialog *) user_data;
  PpNewPrinterDialogPrivate *priv = dialog->priv;
  PpPrintDevice             *device = NULL;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  gint                       acquisition_method;

  gtk_widget_hide (GTK_WIDGET (_dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);

      if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (priv->treeview), &model, &iter))
        {
          gtk_tree_model_get (model, &iter,
                              DEVICE_COLUMN, &device,
                              -1);
        }

      if (device)
        {
          PpNewPrinter *new_printer;
          guint         window_id = 0;

          acquisition_method = pp_print_device_get_acquisition_method (device);
          if (acquisition_method == ACQUISITION_METHOD_SAMBA ||
              acquisition_method == ACQUISITION_METHOD_SAMBA_HOST ||
              acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
              acquisition_method == ACQUISITION_METHOD_LPD)
            {
              priv->new_device = pp_print_device_copy (device);
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
                                 pp_print_device_get_device_name (device),
                                 pp_print_device_get_device_location (device),
                                 pp_print_device_get_device_make_and_model (device),
                                 pp_print_device_is_network_device (device));

              window_id = (guint) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (_dialog)))));

              new_printer = pp_new_printer_new ();
              g_object_set (new_printer,
                            "name", pp_print_device_get_device_name (device),
                            "original-name", pp_print_device_get_device_original_name (device),
                            "device-uri", pp_print_device_get_device_uri (device),
                            "device-id", pp_print_device_get_device_id (device),
                            "ppd-name", pp_print_device_get_device_ppd (device),
                            "ppd-file-name", pp_print_device_get_device_ppd (device),
                            "info", pp_print_device_get_device_info (device),
                            "location", pp_print_device_get_device_location (device),
                            "make-and-model", pp_print_device_get_device_make_and_model (device),
                            "host-name", pp_print_device_get_host_name (device),
                            "host-port", pp_print_device_get_host_port (device),
                            "is-network-device", pp_print_device_is_network_device (device),
                            "window-id", window_id,
                            NULL);

              priv->cancellable = g_cancellable_new ();

              pp_new_printer_add_async (new_printer,
                                        priv->cancellable,
                                        printer_add_async_cb,
                                        dialog);
            }

          g_object_unref (device);
        }
    }
  else
    {
      emit_response (dialog, GTK_RESPONSE_CANCEL);
    }
}
