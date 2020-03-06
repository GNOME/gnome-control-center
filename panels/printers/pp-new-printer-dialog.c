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

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->builder, s))

#define AUTHENTICATION_PAGE "authentication-page"
#define ADDPRINTER_PAGE "addprinter-page"

static void     set_device (PpNewPrinterDialog *self,
                            PpPrintDevice      *device,
                            GtkTreeIter        *iter);
static void     replace_device (PpNewPrinterDialog *self,
                                PpPrintDevice      *old_device,
                                PpPrintDevice      *new_device);
static void     populate_devices_list (PpNewPrinterDialog *self);
static void     search_entry_activated_cb (GtkEntry *entry,
                                           gpointer  user_data);
static void     search_entry_changed_cb (GtkSearchEntry *entry,
                                         gpointer        user_data);
static void     new_printer_dialog_response_cb (GtkDialog *_dialog,
                                                gint       response_id,
                                                gpointer   user_data);
static void     update_dialog_state (PpNewPrinterDialog *self);
static void     add_devices_to_list (PpNewPrinterDialog  *self,
                                     GList               *devices);
static void     remove_device_from_list (PpNewPrinterDialog *self,
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

struct _PpNewPrinterDialog
{
  GObject parent_instance;

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

G_DEFINE_TYPE (PpNewPrinterDialog, pp_new_printer_dialog, G_TYPE_OBJECT)

static void pp_new_printer_dialog_finalize (GObject *object);

enum {
  PRE_RESPONSE,
  RESPONSE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
pp_new_printer_dialog_class_init (PpNewPrinterDialogClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = pp_new_printer_dialog_finalize;

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
                  0,
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
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_generic,
                  G_TYPE_NONE, 1, G_TYPE_INT);
}


PpNewPrinterDialog *
pp_new_printer_dialog_new (GtkWindow *parent,
                           PPDList   *ppd_list)
{
  PpNewPrinterDialog *self;

  self = g_object_new (PP_TYPE_NEW_PRINTER_DIALOG, NULL);

  self->list = ppd_list_copy (ppd_list);
  self->parent = parent;

  gtk_window_set_transient_for (GTK_WINDOW (self->dialog), GTK_WINDOW (parent));

  gtk_widget_show_all (self->dialog);

  return PP_NEW_PRINTER_DIALOG (self);
}

void
pp_new_printer_dialog_set_ppd_list (PpNewPrinterDialog *self,
                                    PPDList            *list)
{
  self->list = ppd_list_copy (list);

  if (self->ppd_selection_dialog)
    pp_ppd_selection_dialog_set_ppd_list (self->ppd_selection_dialog, self->list);
}

static void
emit_pre_response (PpNewPrinterDialog *self,
                   const gchar        *device_name,
                   const gchar        *device_location,
                   const gchar        *device_make_and_model,
                   gboolean            network_device)
{
  g_signal_emit (self,
                 signals[PRE_RESPONSE],
                 0,
                 device_name,
                 device_location,
                 device_make_and_model,
                 network_device);
}

static void
emit_response (PpNewPrinterDialog *self,
               gint                response_id)
{
  g_signal_emit (self, signals[RESPONSE], 0, response_id);
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
  AuthSMBData               *data = user_data;
  PpNewPrinterDialog        *self = PP_NEW_PRINTER_DIALOG (data->dialog);
  PpDevicesList             *result;
  PpPrintDevice             *device;
  GtkWidget                 *widget;
  gboolean                   cancelled = FALSE;
  PpSamba                   *samba = (PpSamba *) source_object;
  g_autoptr(GError)          error = NULL;
  GList                     *iter;

  g_object_ref (samba);

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  if (result != NULL)
    {
      self->samba_authenticated_searching = FALSE;

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
              add_devices_to_list (self, result->devices);

              device = (PpPrintDevice *) result->devices->data;
              if (device != NULL)
                {
                  widget = WID ("search-entry");
                  gtk_entry_set_text (GTK_ENTRY (widget), pp_print_device_get_device_location (device));
                  search_entry_activated_cb (GTK_ENTRY (widget), self);
                }
            }
        }

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          self->samba_authenticated_searching = FALSE;
          update_dialog_state (self);
        }
    }

  g_free (data->server_name);
  g_free (data);
}

static void
go_to_page (PpNewPrinterDialog *self,
            const gchar        *page)
{
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
                PpNewPrinterDialog *self)
{
  return g_strdup (gtk_entry_get_text (GTK_ENTRY (WID (object_name))));
}

static void
on_authenticate (GtkWidget *button,
                 gpointer   user_data)
{
  PpNewPrinterDialog        *self = user_data;
  gchar                     *hostname = NULL;
  gchar                     *username = NULL;
  gchar                     *password = NULL;

  username = get_entry_text ("username-entry", self);
  password = get_entry_text ("password-entry", self);

  if ((username == NULL) || (username[0] == '\0') ||
      (password == NULL) || (password[0] == '\0'))
    {
      g_clear_pointer (&username, g_free);
      g_clear_pointer (&password, g_free);
      return;
    }

  pp_samba_set_auth_info (PP_SAMBA (self->samba_host), username, password);

  gtk_header_bar_set_title (GTK_HEADER_BAR (WID ("headerbar")), _("Add Printer"));
  go_to_page (self, ADDPRINTER_PAGE);

  g_object_get (PP_HOST (self->samba_host), "hostname", &hostname, NULL);
  remove_device_from_list (self, hostname);
}

static void
on_authentication_required (PpHost   *host,
                            gpointer  user_data)
{
  PpNewPrinterDialog        *self = user_data;
  g_autofree gchar          *hostname = NULL;
  g_autofree gchar          *title = NULL;
  g_autofree gchar          *text = NULL;

  gtk_header_bar_set_subtitle (GTK_HEADER_BAR (WID ("headerbar")), NULL);
  gtk_header_bar_set_title (GTK_HEADER_BAR (WID ("headerbar")), _("Unlock Print Server"));

  g_object_get (G_OBJECT (host), "hostname", &hostname, NULL);
  /* Translators: Samba server needs authentication of the user to show list of its printers. */
  title = g_strdup_printf (_("Unlock %s."), hostname);
  gtk_label_set_text (GTK_LABEL (WID ("authentication-title")), title);

  /* Translators: Samba server needs authentication of the user to show list of its printers. */
  text = g_strdup_printf (_("Enter username and password to view printers on %s."), hostname);
  gtk_label_set_text (GTK_LABEL (WID ("authentication-text")), text);

  go_to_page (self, AUTHENTICATION_PAGE);

  g_signal_connect (WID ("authenticate-button"), "clicked", G_CALLBACK (on_authenticate), self);
}

static void
auth_entries_changed (GtkEditable *editable,
                      gpointer     user_data)
{
  PpNewPrinterDialog        *self = user_data;
  gboolean                   can_authenticate = FALSE;
  gchar                     *username = NULL;
  gchar                     *password = NULL;

  username = get_entry_text ("username-entry", self);
  password = get_entry_text ("password-entry", self);

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
  PpNewPrinterDialog        *self = user_data;

  pp_samba_set_auth_info (self->samba_host, NULL, NULL);
  g_clear_object (&self->samba_host);

  go_to_page (self, ADDPRINTER_PAGE);
  gtk_header_bar_set_title (GTK_HEADER_BAR (WID ("headerbar")), _("Add Printer"));
  gtk_widget_set_sensitive (WID ("new-printer-add-button"), FALSE);

  gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (self->treeview));
}

static void
authenticate_samba_server (GtkButton *button,
                           gpointer   user_data)
{
  PpNewPrinterDialog        *self = user_data;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  AuthSMBData               *data;
  gchar                     *server_name = NULL;

  gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
  gtk_widget_set_sensitive (WID ("authenticate-button"), FALSE);
  gtk_widget_grab_focus (WID ("username-entry"));

  if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->treeview), &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          DEVICE_NAME_COLUMN, &server_name,
                          -1);

      if (server_name != NULL)
        {
          g_clear_object (&self->samba_host);

          self->samba_host = pp_samba_new (server_name);
          g_signal_connect_object (self->samba_host,
                                   "authentication-required",
                                   G_CALLBACK (on_authentication_required),
                                   self, 0);

          self->samba_authenticated_searching = TRUE;
          update_dialog_state (self);

          data = g_new (AuthSMBData, 1);
          data->server_name = server_name;
          data->dialog = self;

          pp_samba_get_devices_async (self->samba_host,
                                      TRUE,
                                      self->cancellable,
                                      get_authenticated_samba_devices_cb,
                                      data);
        }
    }
}

static gboolean
stack_key_press_cb (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  PpNewPrinterDialog        *self = user_data;

  gtk_widget_grab_focus (WID ("search-entry"));
  gtk_main_do_event (event);

  return TRUE;
}

static void
pp_new_printer_dialog_init (PpNewPrinterDialog *self)
{
  GtkStyleContext           *context;
  GtkWidget                 *widget;
  g_autoptr(GError)          error = NULL;
  gchar                     *objects[] = { "dialog",
                                           "devices-liststore",
                                           "devices-model-filter",
                                           NULL };
  guint                      builder_result;

  self->builder = gtk_builder_new ();

  builder_result = gtk_builder_add_objects_from_resource (self->builder,
                                                          "/org/gnome/control-center/printers/new-printer-dialog.ui",
                                                          objects, &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
    }

  /* GCancellable for cancelling of async operations */
  self->cancellable = g_cancellable_new ();

  /* Construct dialog */
  self->dialog = WID ("dialog");

  self->treeview = GTK_TREE_VIEW (WID ("devices-treeview"));

  self->store = GTK_LIST_STORE (gtk_builder_get_object (self->builder, "devices-liststore"));

  self->filter = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (self->builder, "devices-model-filter"));

  /* Connect signals */
  g_signal_connect (self->dialog, "response", G_CALLBACK (new_printer_dialog_response_cb), self);

  widget = WID ("search-entry");
  g_signal_connect (widget, "activate", G_CALLBACK (search_entry_activated_cb), self);
  g_signal_connect (widget, "search-changed", G_CALLBACK (search_entry_changed_cb), self);

  widget = WID ("unlock-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (authenticate_samba_server), self);

  g_signal_connect (WID ("stack"), "key-press-event", G_CALLBACK (stack_key_press_cb), self);

  /* Authentication form widgets */
  g_signal_connect (WID ("username-entry"), "changed", G_CALLBACK (auth_entries_changed), self);
  g_signal_connect (WID ("password-entry"), "changed", G_CALLBACK (auth_entries_changed), self);
  g_signal_connect (WID ("go-back-button"), "clicked", G_CALLBACK (on_go_back_button_clicked), self);

  /* Set junctions */
  widget = WID ("scrolledwindow1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

  widget = WID ("toolbar1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  /* Fill with data */
  populate_devices_list (self);
}

static void
free_devices_list (GList *devices)
{
  g_list_free_full (devices, (GDestroyNotify) g_object_unref);
}

static void
pp_new_printer_dialog_finalize (GObject *object)
{
  PpNewPrinterDialog *self = PP_NEW_PRINTER_DIALOG (object);

  g_cancellable_cancel (self->remote_host_cancellable);
  g_cancellable_cancel (self->cancellable);

  self->text_renderer = NULL;
  self->icon_renderer = NULL;

  g_clear_handle_id (&self->host_search_timeout_id, g_source_remove);
  g_clear_object (&self->remote_host_cancellable);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->dialog, gtk_widget_destroy);
  g_clear_pointer (&self->list, ppd_list_free);
  g_clear_object (&self->builder);
  g_clear_pointer (&self->local_cups_devices, free_devices_list);
  g_clear_object (&self->local_printer_icon);
  g_clear_object (&self->remote_printer_icon);
  g_clear_object (&self->authenticated_server_icon);

  if (self->num_of_dests > 0)
    {
      cupsFreeDests (self->num_of_dests, self->dests);
      self->num_of_dests = 0;
      self->dests = NULL;
    }

  G_OBJECT_CLASS (pp_new_printer_dialog_parent_class)->finalize (object);
}

static void
device_selection_changed_cb (GtkTreeSelection *selection,
                             gpointer          user_data)
{
  PpNewPrinterDialog        *self = user_data;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  GtkWidget                 *widget;
  GtkWidget                 *stack;
  gboolean                   authentication_needed;
  gboolean                   selected;

  selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->treeview),
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
remove_device_from_list (PpNewPrinterDialog *self,
                         const gchar        *device_name)
{
  PpPrintDevice             *device;
  GtkTreeIter                iter;
  gboolean                   cont;

  cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);
  while (cont)
    {
      gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
                          DEVICE_COLUMN, &device,
                          -1);

      if (g_strcmp0 (pp_print_device_get_device_name (device), device_name) == 0)
        {
          gtk_list_store_remove (self->store, &iter);
          g_object_unref (device);
          break;
        }

      g_object_unref (device);

      cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
    }

  update_dialog_state (self);
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
add_device_to_list (PpNewPrinterDialog *self,
                    PpPrintDevice      *device)
{
  PpPrintDevice             *store_device;
  GList                     *original_names_list = NULL;
  gint                       acquisistion_method;

  if (device)
    {
      if (pp_print_device_get_host_name (device) == NULL)
        {
          g_autofree gchar *host_name = guess_device_hostname (device);
          g_object_set (device, "host-name", host_name, NULL);
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
          g_autofree gchar *canonicalized_name = NULL;

          g_object_set (device,
                        "device-original-name", pp_print_device_get_device_name (device),
                        NULL);

          gtk_tree_model_foreach (GTK_TREE_MODEL (self->store),
                                  prepend_original_name,
                                  &original_names_list);

          original_names_list = g_list_reverse (original_names_list);

          canonicalized_name = canonicalize_device_name (original_names_list,
                                                         self->local_cups_devices,
                                                         self->dests,
                                                         self->num_of_dests,
                                                         device);

          g_list_free_full (original_names_list, g_free);

          g_object_set (device,
                        "display-name", canonicalized_name,
                        "device-name", canonicalized_name,
                        NULL);

          if (pp_print_device_get_acquisition_method (device) == ACQUISITION_METHOD_DEFAULT_CUPS_SERVER)
            self->local_cups_devices = g_list_append (self->local_cups_devices, g_object_ref (device));
          else
            set_device (self, device, NULL);
        }
      else if (pp_print_device_is_authenticated_server (device) &&
               pp_print_device_get_host_name (device) != NULL)
        {
          store_device = g_object_new (PP_TYPE_PRINT_DEVICE,
                                       "device-name", pp_print_device_get_host_name (device),
                                       "host-name", pp_print_device_get_host_name (device),
                                       "is-authenticated-server", pp_print_device_is_authenticated_server (device),
                                       NULL);

          set_device (self, store_device, NULL);

          g_object_unref (store_device);
        }
    }
}

static void
add_devices_to_list (PpNewPrinterDialog  *self,
                     GList               *devices)
{
  GList *iter;

  for (iter = devices; iter; iter = iter->next)
    {
      add_device_to_list (self, (PpPrintDevice *) iter->data);
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
update_dialog_state (PpNewPrinterDialog *self)
{
  GtkTreeIter                iter;
  GtkWidget                 *header;
  GtkWidget                 *stack;
  gboolean                   searching;

  searching = self->cups_searching ||
              self->remote_cups_host != NULL ||
              self->snmp_host != NULL ||
              self->socket_host != NULL ||
              self->lpd_host != NULL ||
              self->samba_host != NULL ||
              self->samba_authenticated_searching ||
              self->samba_searching;

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

  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter))
      gtk_stack_set_visible_child_name (GTK_STACK (stack), "standard-page");
  else
      gtk_stack_set_visible_child_name (GTK_STACK (stack), searching ? "loading-page" : "no-printers-page");
}

static void
group_physical_devices_cb (gchar    ***device_uris,
                           gpointer    user_data)
{
  PpNewPrinterDialog        *self = user_data;
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
                  device = device_in_liststore (device_uris[i][j], self->store);
                  if (device != NULL)
                    break;
                }

              /* Is this sublist represented in the current list of devices? */
              if (device != NULL)
                {
                  /* Is there better device in the sublist? */
                  if (j != 0)
                    {
                      better_device = device_in_list (device_uris[i][0], self->local_cups_devices);
                      replace_device (self, device, better_device);
                      g_object_unref (better_device);
                    }

                  g_object_unref (device);
                }
              else
                {
                  device = device_in_list (device_uris[i][0], self->local_cups_devices);
                  if (device != NULL)
                    {
                      set_device (self, device, NULL);
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
      for (iter = self->local_cups_devices; iter != NULL; iter = iter->next)
        set_device (self, (PpPrintDevice *) iter->data, NULL);
      g_clear_pointer (&self->local_cups_devices, free_devices_list);
    }

  update_dialog_state (self);
}

static void
group_physical_devices_dbus_cb (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
  GVariant         *output;
  g_autoptr(GError) error = NULL;
  gchar          ***result = NULL;
  gint              i, j;

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
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s", error->message);
    }

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    group_physical_devices_cb (result, user_data);
}

static void
get_cups_devices_cb (GList    *devices,
                     gboolean  finished,
                     gboolean  cancelled,
                     gpointer  user_data)
{
  PpNewPrinterDialog         *self = user_data;
  GDBusConnection            *bus;
  GVariantBuilder             device_list;
  GVariantBuilder             device_hash;
  PpPrintDevice             **all_devices;
  PpPrintDevice              *pp_device;
  PpPrintDevice              *device;
  const gchar                *device_class;
  GtkTreeIter                 iter;
  gboolean                    cont;
  g_autoptr(GError)           error = NULL;
  GList                      *liter;
  gint                        length, i;


  if (!cancelled)
    {
      if (finished)
        {
          self->cups_searching = FALSE;
        }

      if (devices)
        {
          add_devices_to_list (self, devices);

          length = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (self->store), NULL) + g_list_length (self->local_cups_devices);
          if (length > 0)
            {
              all_devices = g_new0 (PpPrintDevice *, length);

              i = 0;
              cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);
              while (cont)
                {
                  gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
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

                  cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
                }

              for (liter = self->local_cups_devices; liter != NULL; liter = liter->next)
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
                                          self->cancellable,
                                          group_physical_devices_dbus_cb,
                                          self);
                }
              else
                {
                  g_warning ("Failed to get system bus: %s", error->message);
                  group_physical_devices_cb (NULL, user_data);
                }

              for (i = 0; i < length; i++)
                g_object_unref (all_devices[i]);
              g_free (all_devices);
            }
          else
            {
              update_dialog_state (self);
            }
        }
      else
        {
          update_dialog_state (self);
        }
    }

  free_devices_list (devices);
}

static void
get_snmp_devices_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpHost                    *host = (PpHost *) source_object;
  g_autoptr(GError)          error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_snmp_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      if ((gpointer) source_object == (gpointer) self->snmp_host)
        self->snmp_host = NULL;

      add_devices_to_list (self, result->devices);

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) self->snmp_host)
            self->snmp_host = NULL;

          update_dialog_state (self);
        }
    }
}

static void
get_remote_cups_devices_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpHost                    *host = (PpHost *) source_object;
  g_autoptr(GError)          error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_remote_cups_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      if ((gpointer) source_object == (gpointer) self->remote_cups_host)
        self->remote_cups_host = NULL;

      add_devices_to_list (self, result->devices);

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) self->remote_cups_host)
            self->remote_cups_host = NULL;

          update_dialog_state (self);
        }
    }
}

static void
get_samba_host_devices_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpDevicesList             *result;
  PpSamba                   *samba = (PpSamba *) source_object;
  g_autoptr(GError)          error = NULL;

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      if ((gpointer) source_object == (gpointer) self->samba_host)
        self->samba_host = NULL;

      add_devices_to_list (self, result->devices);

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) self->samba_host)
            self->samba_host = NULL;

          update_dialog_state (self);
        }
    }
}

static void
get_samba_devices_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpDevicesList             *result;
  PpSamba                   *samba = (PpSamba *) source_object;
  g_autoptr(GError)          error = NULL;

  result = pp_samba_get_devices_finish (samba, res, &error);
  g_object_unref (source_object);

  if (result)
    {
      self->samba_searching = FALSE;

      add_devices_to_list (self, result->devices);

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          self->samba_searching = FALSE;

          update_dialog_state (self);
        }
    }
}

static void
get_jetdirect_devices_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpHost                    *host = (PpHost *) source_object;
  g_autoptr(GError)          error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_jetdirect_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result != NULL)
    {
      if ((gpointer) source_object == (gpointer) self->socket_host)
        self->socket_host = NULL;

      add_devices_to_list (self, result->devices);

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) self->socket_host)
            self->socket_host = NULL;

          update_dialog_state (self);
        }
    }
}

static void
get_lpd_devices_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpHost                    *host = (PpHost *) source_object;
  g_autoptr(GError)          error = NULL;
  PpDevicesList             *result;

  result = pp_host_get_lpd_devices_finish (host, res, &error);
  g_object_unref (source_object);

  if (result != NULL)
    {
      if ((gpointer) source_object == (gpointer) self->lpd_host)
        self->lpd_host = NULL;

      add_devices_to_list (self, result->devices);

      update_dialog_state (self);

      pp_devices_list_free (result);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          if ((gpointer) source_object == (gpointer) self->lpd_host)
            self->lpd_host = NULL;

          update_dialog_state (self);
        }
    }
}

static void
get_cups_devices (PpNewPrinterDialog *self)
{
  self->cups_searching = TRUE;
  update_dialog_state (self);

  get_cups_devices_async (self->cancellable,
                          get_cups_devices_cb,
                          self);
}

static gboolean
parse_uri (const gchar  *uri,
           gchar       **scheme,
           gchar       **host,
           gint         *port)
{
  const gchar      *tmp = NULL;
  g_autofree gchar *resulting_host = NULL;
  gchar            *position;

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
  PpNewPrinterDialog *self = data->dialog;

  g_cancellable_cancel (self->remote_host_cancellable);
  g_clear_object (&self->remote_host_cancellable);

  self->remote_host_cancellable = g_cancellable_new ();

  self->remote_cups_host = pp_host_new (data->host_name);
  self->snmp_host = pp_host_new (data->host_name);
  self->socket_host = pp_host_new (data->host_name);
  self->lpd_host = pp_host_new (data->host_name);

  if (data->host_port != PP_HOST_UNSET_PORT)
    {
      g_object_set (self->remote_cups_host, "port", data->host_port, NULL);
      g_object_set (self->snmp_host, "port", data->host_port, NULL);

      /* Accept port different from the default one only if user specifies
       * scheme (for socket and lpd printers).
       */
      if (data->host_scheme != NULL &&
          g_ascii_strcasecmp (data->host_scheme, "socket") == 0)
        g_object_set (self->socket_host, "port", data->host_port, NULL);

      if (data->host_scheme != NULL &&
          g_ascii_strcasecmp (data->host_scheme, "lpd") == 0)
        g_object_set (self->lpd_host, "port", data->host_port, NULL);
    }

  self->samba_host = pp_samba_new (data->host_name);

  update_dialog_state (data->dialog);

  pp_host_get_remote_cups_devices_async (self->remote_cups_host,
                                         self->remote_host_cancellable,
                                         get_remote_cups_devices_cb,
                                         data->dialog);

  pp_host_get_snmp_devices_async (self->snmp_host,
                                  self->remote_host_cancellable,
                                  get_snmp_devices_cb,
                                  data->dialog);

  pp_host_get_jetdirect_devices_async (self->socket_host,
                                       self->remote_host_cancellable,
                                       get_jetdirect_devices_cb,
                                       data->dialog);

  pp_host_get_lpd_devices_async (self->lpd_host,
                                 self->remote_host_cancellable,
                                 get_lpd_devices_cb,
                                 data->dialog);

  pp_samba_get_devices_async (self->samba_host,
                              FALSE,
                              self->remote_host_cancellable,
                              get_samba_host_devices_cb,
                              data->dialog);

  self->host_search_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
search_address (const gchar        *text,
                PpNewPrinterDialog *self,
                gboolean            delay_search)
{
  PpPrintDevice              *device;
  GtkTreeIter                 iter;
  gboolean                    found = FALSE;
  gboolean                    subfound;
  gboolean                    next_set;
  gboolean                    cont;
  g_autofree gchar           *lowercase_text = NULL;
  gchar                     **words;
  gint                        words_length = 0;
  gint                        i;
  gint                        acquisition_method;

  lowercase_text = g_ascii_strdown (text, -1);
  words = g_strsplit_set (lowercase_text, " ", -1);

  if (words)
    {
      words_length = g_strv_length (words);

      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);
      while (cont)
        {
          g_autofree gchar *lowercase_name = NULL;
          g_autofree gchar *lowercase_location = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
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

          gtk_list_store_set (GTK_LIST_STORE (self->store), &iter,
                              DEVICE_VISIBLE_COLUMN, subfound,
                              -1);

          g_object_unref (device);

          cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
        }

      g_strfreev (words);
  }

  /*
   * The given word is probably an address since it was not found among
   * already present devices.
   */
  if (!found && words_length == 1)
    {
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);
      while (cont)
        {
          next_set = FALSE;
          gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          gtk_list_store_set (GTK_LIST_STORE (self->store), &iter,
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
              if (!gtk_list_store_remove (self->store, &iter))
                break;
              else
                next_set = TRUE;
            }

          if (!next_set)
            cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
        }

      if (text && text[0] != '\0')
        {
          g_autoptr(GSocketConnectable) conn = NULL;
          g_autofree gchar *test_uri = NULL;
          g_autofree gchar *test_port = NULL;
          gchar *scheme = NULL;
          gchar *host = NULL;
          gint   port;

          parse_uri (text, &scheme, &host, &port);

          if (host != NULL)
            {
              if (port >= 0)
                test_port = g_strdup_printf (":%d", port);
              else
                test_port = g_strdup ("");

              test_uri = g_strdup_printf ("%s://%s%s",
                                          scheme != NULL && scheme[0] != '\0' ? scheme : "none",
                                          host,
                                          test_port);

              conn = g_network_address_parse_uri (test_uri, 0, NULL);
              if (conn != NULL)
                {
                  THostSearchData *search_data;

                  search_data = g_new (THostSearchData, 1);
                  search_data->host_scheme = scheme;
                  search_data->host_name = host;
                  search_data->host_port = port;
                  search_data->dialog = self;

                  if (self->host_search_timeout_id != 0)
                    {
                      g_source_remove (self->host_search_timeout_id);
                      self->host_search_timeout_id = 0;
                    }

                  if (delay_search)
                    {
                      self->host_search_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
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
set_device (PpNewPrinterDialog *self,
            PpPrintDevice      *device,
            GtkTreeIter        *iter)
{
  GtkTreeIter                titer;
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
          g_autofree gchar *description = NULL;

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
            gtk_list_store_append (self->store, &titer);

          gtk_list_store_set (self->store, iter == NULL ? &titer : iter,
                              DEVICE_GICON_COLUMN, pp_print_device_is_network_device (device) ? self->remote_printer_icon : self->local_printer_icon,
                              DEVICE_NAME_COLUMN, pp_print_device_get_device_name (device),
                              DEVICE_DISPLAY_NAME_COLUMN, pp_print_device_get_display_name (device),
                              DEVICE_DESCRIPTION_COLUMN, description,
                              DEVICE_VISIBLE_COLUMN, TRUE,
                              DEVICE_COLUMN, device,
                              -1);
        }
      else if (pp_print_device_is_authenticated_server (device) &&
               pp_print_device_get_host_name (device) != NULL)
        {
          if (iter == NULL)
            gtk_list_store_append (self->store, &titer);

          gtk_list_store_set (self->store, iter == NULL ? &titer : iter,
                              DEVICE_GICON_COLUMN, self->authenticated_server_icon,
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
replace_device (PpNewPrinterDialog *self,
                PpPrintDevice      *old_device,
                PpPrintDevice      *new_device)
{
  PpPrintDevice             *device;
  GtkTreeIter                iter;
  gboolean                   cont;

  if (old_device != NULL && new_device != NULL)
    {
      cont = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (self->store), &iter);
      while (cont)
        {
          gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter,
                              DEVICE_COLUMN, &device,
                              -1);

          if (old_device == device)
            {
              set_device (self, new_device, &iter);
              g_object_unref (device);
              break;
            }

          g_object_unref (device);

          cont = gtk_tree_model_iter_next (GTK_TREE_MODEL (self->store), &iter);
        }
    }
}

static void
cups_get_dests_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpCupsDests               *dests;
  PpCups                    *cups = (PpCups *) source_object;
  g_autoptr(GError)          error = NULL;

  dests = pp_cups_get_dests_finish (cups, res, &error);
  g_object_unref (source_object);

  if (dests)
    {
      self->dests = dests->dests;
      self->num_of_dests = dests->num_of_dests;

      get_cups_devices (self);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          get_cups_devices (self);
        }
    }
}

static void
row_activated_cb (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
  PpNewPrinterDialog        *self = user_data;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  GtkWidget                 *widget;
  gboolean                   authentication_needed;
  gboolean                   selected;

  selected = gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->treeview),
                                              &model,
                                              &iter);

  if (selected)
    {
      gtk_tree_model_get (model, &iter, SERVER_NEEDS_AUTHENTICATION_COLUMN, &authentication_needed, -1);

      if (authentication_needed)
        {
          widget = WID ("unlock-button");
          authenticate_samba_server (GTK_BUTTON (widget), self);
        }
      else
        {
          gtk_dialog_response (GTK_DIALOG (self->dialog), GTK_RESPONSE_OK);
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
  PpNewPrinterDialog        *self = user_data;
  gboolean                   selected = FALSE;
  g_autofree gchar          *name = NULL;
  g_autofree gchar          *description = NULL;

  selected = gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (self->treeview), iter);

  gtk_tree_model_get (tree_model, iter,
                      DEVICE_DISPLAY_NAME_COLUMN, &name,
                      DEVICE_DESCRIPTION_COLUMN, &description,
                      -1);

  if (name != NULL)
    {
      g_autofree gchar *text = NULL;

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
    }
}

static void
populate_devices_list (PpNewPrinterDialog *self)
{
  GtkTreeViewColumn         *column;
  PpSamba                   *samba;
  GEmblem                   *emblem;
  PpCups                    *cups;
  GIcon                     *icon, *emblem_icon;

  g_signal_connect (gtk_tree_view_get_selection (self->treeview),
                    "changed", G_CALLBACK (device_selection_changed_cb), self);

  g_signal_connect (self->treeview,
                    "row-activated", G_CALLBACK (row_activated_cb), self);

  self->local_printer_icon = g_themed_icon_new ("printer");
  self->remote_printer_icon = g_themed_icon_new ("printer-network");

  icon = g_themed_icon_new ("network-server");
  emblem_icon = g_themed_icon_new ("changes-prevent");
  emblem = g_emblem_new (emblem_icon);

  self->authenticated_server_icon = g_emblemed_icon_new (icon, emblem);

  g_object_unref (icon);
  g_object_unref (emblem_icon);
  g_object_unref (emblem);

  self->icon_renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (self->icon_renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
  gtk_cell_renderer_set_alignment (self->icon_renderer, 1.0, 0.5);
  gtk_cell_renderer_set_padding (self->icon_renderer, 4, 4);
  column = gtk_tree_view_column_new_with_attributes ("Icon", self->icon_renderer,
                                                     "gicon", DEVICE_GICON_COLUMN, NULL);
  gtk_tree_view_column_set_max_width (column, -1);
  gtk_tree_view_column_set_min_width (column, 80);
  gtk_tree_view_append_column (self->treeview, column);


  self->text_renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Devices", self->text_renderer,
                                                     NULL);
  gtk_tree_view_column_set_cell_data_func (column, self->text_renderer, cell_data_func,
                                           self, NULL);
  gtk_tree_view_append_column (self->treeview, column);

  gtk_tree_model_filter_set_visible_column (self->filter, DEVICE_VISIBLE_COLUMN);

  cups = pp_cups_new ();
  pp_cups_get_dests_async (cups, self->cancellable, cups_get_dests_cb, self);

  self->samba_searching = TRUE;
  update_dialog_state (self);

  samba = pp_samba_new (NULL);
  pp_samba_get_devices_async (samba, FALSE, self->cancellable, get_samba_devices_cb, self);
}

static void
printer_add_async_cb (GObject      *source_object,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  PpNewPrinterDialog        *self = user_data;
  GtkResponseType            response_id = GTK_RESPONSE_OK;
  PpNewPrinter              *new_printer = (PpNewPrinter *) source_object;
  gboolean                   success;
  g_autoptr(GError)          error = NULL;

  success = pp_new_printer_add_finish (new_printer, res, &error);
  g_object_unref (source_object);

  if (success)
    {
      emit_response (self, response_id);
    }
  else
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("%s", error->message);

          response_id = GTK_RESPONSE_REJECT;

          emit_response (self, response_id);
        }
    }
}

static void
ppd_selection_cb (GtkDialog *_dialog,
                  gint       response_id,
                  gpointer   user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpNewPrinter              *new_printer;
  GList                     *original_names_list = NULL;
  gchar                     *ppd_name;
  gchar                     *ppd_display_name;
  guint                      window_id = 0;
  gint                       acquisition_method;

  ppd_name = pp_ppd_selection_dialog_get_ppd_name (self->ppd_selection_dialog);
  ppd_display_name = pp_ppd_selection_dialog_get_ppd_display_name (self->ppd_selection_dialog);
  pp_ppd_selection_dialog_free (self->ppd_selection_dialog);
  self->ppd_selection_dialog = NULL;

  if (ppd_name)
    {
      g_object_set (self->new_device, "device-ppd", ppd_name, NULL);

      acquisition_method = pp_print_device_get_acquisition_method (self->new_device);
      if ((acquisition_method == ACQUISITION_METHOD_JETDIRECT ||
           acquisition_method == ACQUISITION_METHOD_LPD) &&
          ppd_display_name != NULL)
        {
          g_autofree gchar *printer_name = NULL;

          g_object_set (self->new_device,
                        "device-name", ppd_display_name,
                        "device-original-name", ppd_display_name,
                        NULL);

          gtk_tree_model_foreach (GTK_TREE_MODEL (self->store),
                                  prepend_original_name,
                                  &original_names_list);

          original_names_list = g_list_reverse (original_names_list);

          printer_name = canonicalize_device_name (original_names_list,
                                                   self->local_cups_devices,
                                                   self->dests,
                                                   self->num_of_dests,
                                                   self->new_device);

          g_list_free_full (original_names_list, g_free);

          g_object_set (self->new_device,
                        "device-name", printer_name,
                        "device-original-name", printer_name,
                        NULL);
        }

      emit_pre_response (self,
                         pp_print_device_get_device_name (self->new_device),
                         pp_print_device_get_device_location (self->new_device),
                         pp_print_device_get_device_make_and_model (self->new_device),
                         pp_print_device_is_network_device (self->new_device));

      window_id = (guint) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (self->dialog)))));

      new_printer = pp_new_printer_new ();
      g_object_set (new_printer,
                    "name", pp_print_device_get_device_name (self->new_device),
                    "original-name", pp_print_device_get_device_original_name (self->new_device),
                    "device-uri", pp_print_device_get_device_uri (self->new_device),
                    "device-id", pp_print_device_get_device_id (self->new_device),
                    "ppd-name", pp_print_device_get_device_ppd (self->new_device),
                    "ppd-file-name", pp_print_device_get_device_ppd (self->new_device),
                    "info", pp_print_device_get_device_info (self->new_device),
                    "location", pp_print_device_get_device_location (self->new_device),
                    "make-and-model", pp_print_device_get_device_make_and_model (self->new_device),
                    "host-name", pp_print_device_get_host_name (self->new_device),
                    "host-port", pp_print_device_get_host_port (self->new_device),
                    "is-network-device", pp_print_device_is_network_device (self->new_device),
                    "window-id", window_id,
                    NULL);
      self->cancellable = g_cancellable_new ();

      pp_new_printer_add_async (new_printer,
                                self->cancellable,
                                printer_add_async_cb,
                                self);

      g_clear_object (&self->new_device);
    }
}

static void
new_printer_dialog_response_cb (GtkDialog *dialog,
                                gint       response_id,
                                gpointer   user_data)
{
  PpNewPrinterDialog        *self = user_data;
  PpPrintDevice             *device = NULL;
  GtkTreeModel              *model;
  GtkTreeIter                iter;
  gint                       acquisition_method;

  gtk_widget_hide (GTK_WIDGET (dialog));

  if (response_id == GTK_RESPONSE_OK)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);

      if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (self->treeview), &model, &iter))
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
              self->new_device = pp_print_device_copy (device);
              self->ppd_selection_dialog =
                pp_ppd_selection_dialog_new (self->parent,
                                             self->list,
                                             NULL,
                                             ppd_selection_cb,
                                             self);
            }
          else
            {
              emit_pre_response (self,
                                 pp_print_device_get_device_name (device),
                                 pp_print_device_get_device_location (device),
                                 pp_print_device_get_device_make_and_model (device),
                                 pp_print_device_is_network_device (device));

              window_id = (guint) GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (gtk_window_get_transient_for (GTK_WINDOW (dialog)))));

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

              self->cancellable = g_cancellable_new ();

              pp_new_printer_add_async (new_printer,
                                        self->cancellable,
                                        printer_add_async_cb,
                                        self);
            }

          g_object_unref (device);
        }
    }
  else
    {
      emit_response (self, GTK_RESPONSE_CANCEL);
    }
}
