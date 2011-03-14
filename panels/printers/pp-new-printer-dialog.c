/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
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

#include "pp-new-printer-dialog.h"
#include "pp-utils.h"

#include <dbus/dbus-glib.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#define MECHANISM_BUS "org.opensuse.CupsPkHelper.Mechanism"

#define PACKAGE_KIT_BUS "org.freedesktop.PackageKit"
#define PACKAGE_KIT_PATH "/org/freedesktop/PackageKit"
#define PACKAGE_KIT_IFACE "org.freedesktop.PackageKit.Modify"

#define ALLOWED_CHARACTERS "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_/"

static void pp_new_printer_dialog_hide (PpNewPrinterDialog *pp);
static void actualize_devices_list (PpNewPrinterDialog *pp);

enum
{
  NOTEBOOK_LOCAL_PAGE = 0,
  NOTEBOOK_NETWORK_PAGE,
  NOTEBOOK_HP_JETDIRECT_PAGE,
  NOTEBOOK_N_PAGES
};

enum
{
  DEVICE_TYPE_ID_COLUMN = 0,
  DEVICE_TYPE_NAME_COLUMN,
  DEVICE_TYPE_TYPE_COLUMN,
  DEVICE_TYPE_N_COLUMNS
};

enum
{
  DEVICE_ID_COLUMN = 0,
  DEVICE_NAME_COLUMN,
  DEVICE_N_COLUMNS
};

enum
{
  DEVICE_TYPE_LOCAL = 0,
  DEVICE_TYPE_NETWORK,
  DEVICE_TYPE_HP_JETDIRECT,
  DEVICE_TYPE_N
};

typedef struct{
  gchar *device_class;
  gchar *device_id;
  gchar *device_info;
  gchar *device_make_and_model;
  gchar *device_uri;
  gchar *device_location;
  gchar *device_ppd_uri;
  gchar *display_name;
  gchar *hostname;
  gint   host_port;
  gboolean show;
  gboolean found;
} CupsDevice;

struct _PpNewPrinterDialog {
  GtkBuilder *builder;
  GtkWidget  *parent;

  GtkWidget  *dialog;

  gchar **device_connection_types;
  gint    num_device_connection_types;

  CupsDevice *devices;
  gint        num_devices;

  UserResponseCallback user_callback;
  gpointer             user_data;
};

static void
device_type_selection_changed_cb (GtkTreeSelection *selection,
                                  gpointer          user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog *) user_data;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  gchar              *device_type_name = NULL;
  gint                device_type_id = -1;
  gint                device_type = -1;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  DEVICE_TYPE_ID_COLUMN, &device_type_id,
			  DEVICE_TYPE_NAME_COLUMN, &device_type_name,
			  DEVICE_TYPE_TYPE_COLUMN, &device_type,
			  -1);
    }

  if (device_type >= 0)
    {
      GtkWidget *widget;

      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "device-type-notebook");

      gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), device_type);
    }
}

static void
free_devices (PpNewPrinterDialog *pp)
{
  int i;

  for (i = 0; i < pp->num_devices; i++)
    {
      g_free (pp->devices[i].device_class);
      g_free (pp->devices[i].device_id);
      g_free (pp->devices[i].device_info);
      g_free (pp->devices[i].device_make_and_model);
      g_free (pp->devices[i].device_uri);
      g_free (pp->devices[i].device_location);
      g_free (pp->devices[i].device_ppd_uri);
      g_free (pp->devices[i].display_name);
      g_free (pp->devices[i].hostname);
    }

  pp->num_devices = 0;
  pp->devices = NULL;
}

static void
store_device_parameter (gpointer key,
                        gpointer value,
                        gpointer user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog *) user_data;
  gchar *cut;
  gint   index = -1;

  cut = g_strrstr ((gchar *)key, ":");
  if (cut)
    index = atoi ((gchar *)cut + 1);

  if (index >= 0)
    {
      if (g_str_has_prefix ((gchar *)key, "device-class"))
        pp->devices[index].device_class = g_strdup ((gchar *)value);
      else if (g_str_has_prefix ((gchar *)key, "device-id"))
        pp->devices[index].device_id = g_strdup ((gchar *)value);
      else if (g_str_has_prefix ((gchar *)key, "device-info"))
        pp->devices[index].device_info = g_strdup ((gchar *)value);
      else if (g_str_has_prefix ((gchar *)key, "device-make-and-model"))
        pp->devices[index].device_make_and_model = g_strdup ((gchar *)value);
      else if (g_str_has_prefix ((gchar *)key, "device-uri"))
        pp->devices[index].device_uri = g_strdup ((gchar *)value);
      else if (g_str_has_prefix ((gchar *)key, "device-location"))
        pp->devices[index].device_location = g_strdup ((gchar *)value);
    }
}

static void
devices_get_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog *) user_data;
  GHashTable         *devices = NULL;
  GtkWidget          *widget = NULL;
  GVariant           *dg_output = NULL;
  GError             *error = NULL;
  char               *ret_error = NULL;
  gint                i, j;

  dg_output = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                        res,
                                        &error);

  if (dg_output && g_variant_n_children (dg_output) == 2)
    {
      GVariant *devices_variant = NULL;

      g_variant_get (dg_output, "(&s@a{ss})",
                     &ret_error,
                     &devices_variant);

      if (devices_variant)
        {
          if (g_variant_is_of_type (devices_variant, G_VARIANT_TYPE ("a{ss}")))
            {
              GVariantIter *iter;
              GVariant *item;
              g_variant_get (devices_variant,
                             "a{ss}",
                             &iter);
              devices = g_hash_table_new (g_str_hash, g_str_equal);
              while ((item = g_variant_iter_next_value (iter)))
                {
                  gchar *key;
                  gchar *value;
                  g_variant_get (item,
                                 "{ss}",
                                 &key,
                                 &value);
  
                  g_hash_table_insert (devices, key, value);
                }
            }
          g_variant_unref (devices_variant);
        }
      g_variant_unref (dg_output);
    }
  g_object_unref (source_object);

  if (error || (ret_error && ret_error[0] != '\0'))
    {
      if (error)
        g_warning ("%s", error->message);

      if (ret_error && ret_error[0] != '\0')
        g_warning ("%s", ret_error);
    }

  free_devices (pp);
  if (devices)
    {
      GList *keys;
      GList *iter;
      gchar *cut;
      gint   max_index = -1;
      gint   index;

      keys = g_hash_table_get_keys (devices);
      for (iter = keys; iter; iter = iter->next)
        {
          index = -1;

          cut = g_strrstr ((gchar *)iter->data, ":");
          if (cut)
            index = atoi (cut + 1);

          if (index > max_index)
            max_index = index;
        }

      if (max_index >= 0)
        {
          pp->num_devices = max_index + 1;
          pp->devices = g_new0 (CupsDevice, pp->num_devices);

          g_hash_table_foreach (devices, store_device_parameter, pp);

          /* Assign names to devices */
          for (i = 0; i < pp->num_devices; i++)
            {
              gchar *name = NULL;

              if (pp->devices[i].device_id)
                {
                  name = get_tag_value (pp->devices[i].device_id, "mdl");
                  name = g_strcanon (name, ALLOWED_CHARACTERS, '-');
                }
              else if (pp->devices[i].device_info)
                {
                  name = g_strdup (pp->devices[i].device_info);
                  name = g_strcanon (name, ALLOWED_CHARACTERS, '-');
                }

              pp->devices[i].display_name = name;
            }

          /* Set show bool
           * Don't show duplicates.
           * Show devices with device-id.
           * Other preferences should apply here.
           */
          for (i = 0; i < pp->num_devices; i++)
            {
              for (j = 0; j < pp->num_devices; j++)
                {
                  if (i != j)
                    {
                      if (g_strcmp0 (pp->devices[i].display_name, pp->devices[j].display_name) == 0)
                        {
                          if (pp->devices[i].device_id && !pp->devices[j].show)
                            {
                              pp->devices[i].show = TRUE;
                            }
                        }
                    }
                }
            }
        }

      g_hash_table_destroy (devices);
      actualize_devices_list (pp);
    }

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "get-devices-status-label");
  gtk_label_set_text (GTK_LABEL (widget), " ");

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "spinner");
  gtk_spinner_stop (GTK_SPINNER (widget));
  gtk_widget_set_sensitive (widget, FALSE);

  g_clear_error (&error);
}

static void
devices_get (PpNewPrinterDialog *pp)
{
  GDBusProxy *proxy;
  GError     *error = NULL;
  GVariant        *dg_input = NULL;
  GVariantBuilder *in_include = NULL;
  GVariantBuilder *in_exclude = NULL;
  GtkWidget *widget = NULL;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         MECHANISM_BUS,
                                         "/",
                                         MECHANISM_BUS,
                                         NULL,
                                         &error);

  if (proxy)
    {
      in_include = g_variant_builder_new (G_VARIANT_TYPE ("as"));
      in_exclude = g_variant_builder_new (G_VARIANT_TYPE ("as"));

      dg_input = g_variant_new ("(iiasas)",
                                0,
                                60,
                                in_include,
                                in_exclude);

      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "get-devices-status-label");
      gtk_label_set_text (GTK_LABEL (widget), _("Getting devices..."));

      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "spinner");
      gtk_spinner_start (GTK_SPINNER (widget));
      gtk_widget_set_sensitive (widget, TRUE);

      g_dbus_proxy_call (proxy,
                         "DevicesGet",
                         dg_input,
                         G_DBUS_CALL_FLAGS_NONE,
                         60000,
                         NULL,
                         devices_get_cb,
                         pp);

      g_variant_builder_unref (in_exclude);
      g_variant_builder_unref (in_include);
      g_variant_unref (dg_input);
    }
}

static void
search_address_cb (GtkToggleButton *togglebutton,
                   gpointer         user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog*) user_data;
  GtkWidget *widget;
  gint i;

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "search-by-address-checkbutton");

  if (widget && gtk_toggle_button_get_active (togglebutton))
    {
      gchar *uri = NULL;

      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "address-entry");
      uri = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));

      if (uri && uri[0] != '\0')
        {
          http_t *http;
          cups_dest_t *dests = NULL;
          gint num_dests = 0;
          gchar *tmp = NULL;
          gchar *host = NULL;
          gchar *port_string = NULL;
          int  port = 631;
          gchar *position;

          if (g_strrstr (uri, "://"))
            tmp = g_strrstr (uri, "://") + 3;
          else
            tmp = uri;

          if (g_strrstr (tmp, "@"))
            tmp = g_strrstr (tmp, "@") + 1;

          if ((position = g_strrstr (tmp, "/")))
            {
              *position = '\0';
              host = g_strdup (tmp);
              *position = '/';
            }
          else
            host = g_strdup (tmp);

          if ((position = g_strrstr (host, ":")))
            {
              *position = '\0';
              port_string = position + 1;
            }

          if (port_string)
            port = atoi (port_string);

          /* Search for CUPS server */
          if (host)
            {
              http = httpConnectEncrypt (host, port, cupsEncryption ());
              if (http)
                {
                  gchar *device_uri = NULL;
                  gchar *device_ppd_uri = NULL;

                  num_dests = cupsGetDests2 (http, &dests);

                  if (num_dests > 0)
                    {
                      CupsDevice *devices = NULL;
                      devices = g_new0 (CupsDevice, pp->num_devices + num_dests);

                      for (i = 0; i < pp->num_devices; i++)
                        {
                          devices[i] = pp->devices[i];
                          pp->devices[i].device_class = NULL;
                          pp->devices[i].device_id = NULL;
                          pp->devices[i].device_info = NULL;
                          pp->devices[i].device_make_and_model = NULL;
                          pp->devices[i].device_uri = NULL;
                          pp->devices[i].device_location = NULL;
                          pp->devices[i].device_ppd_uri = NULL;
                          pp->devices[i].display_name = NULL;
                          pp->devices[i].hostname = NULL;
                        }

                      g_free (pp->devices);
                      pp->devices = devices;

                      for (i = 0; i < num_dests; i++)
                        {
                          device_uri = g_strdup_printf ("ipp://%s:%d/printers/%s", host, port, dests[i].name);
                          device_ppd_uri = g_strdup_printf ("%s.ppd", device_uri);

                          pp->devices[pp->num_devices + i].device_class = g_strdup ("network");
                          pp->devices[pp->num_devices + i].device_uri = device_uri;
                          pp->devices[pp->num_devices + i].display_name = g_strdup (dests[i].name);
                          pp->devices[pp->num_devices + i].device_ppd_uri = device_ppd_uri;
                          pp->devices[pp->num_devices + i].show = TRUE;
                          pp->devices[pp->num_devices + i].hostname = g_strdup (host);
                          pp->devices[pp->num_devices + i].host_port = port;
                          pp->devices[pp->num_devices + i].found = TRUE;
                        }

                      pp->num_devices += num_dests;
                    }

                  httpClose (http);
                }
              g_free (host);
            }
        }
      g_free (uri);
    }
  else
    {
      gint length = 0;
      gint j = 0;

      for (i = 0; i < pp->num_devices; i++)
        if (!pp->devices[i].found)
          length++;

      CupsDevice *devices = NULL;
      devices = g_new0 (CupsDevice, length);

      for (i = 0; i < pp->num_devices; i++)
        {
          if (!pp->devices[i].found)
            {
              devices[j] = pp->devices[i];
              pp->devices[i].device_class = NULL;
              pp->devices[i].device_id = NULL;
              pp->devices[i].device_info = NULL;
              pp->devices[i].device_make_and_model = NULL;
              pp->devices[i].device_uri = NULL;
              pp->devices[i].device_location = NULL;
              pp->devices[i].device_ppd_uri = NULL;
              pp->devices[i].display_name = NULL;
              pp->devices[i].hostname = NULL;
              j++;
            }
        }

      g_free (pp->devices);
      pp->devices = devices;
      pp->num_devices = length;
    }

  actualize_devices_list (pp);
}

static void
actualize_devices_list (PpNewPrinterDialog *pp)
{
  GtkListStore *network_store;
  GtkListStore *local_store;
  GtkTreeView  *network_treeview;
  GtkTreeView  *local_treeview;
  GtkTreeIter   iter;
  gint          i;

  network_treeview = (GtkTreeView*)
    gtk_builder_get_object (pp->builder, "network-devices-treeview");

  local_treeview = (GtkTreeView*)
    gtk_builder_get_object (pp->builder, "local-devices-treeview");

  network_store = gtk_list_store_new (DEVICE_N_COLUMNS,
                                      G_TYPE_INT,
                                      G_TYPE_STRING);

  local_store = gtk_list_store_new (DEVICE_N_COLUMNS,
                                    G_TYPE_INT,
                                    G_TYPE_STRING);

  for (i = 0; i < pp->num_devices; i++)
    {
      if (pp->devices[i].device_id || pp->devices[i].device_ppd_uri)
        {
          if (g_strcmp0 (pp->devices[i].device_class, "network") == 0)
            {
              gtk_list_store_append (network_store, &iter);
              gtk_list_store_set (network_store, &iter,
                                  DEVICE_ID_COLUMN, i,
                                  DEVICE_NAME_COLUMN, pp->devices[i].display_name,
                                  -1);
            }
          else if (g_strcmp0 (pp->devices[i].device_class, "direct") == 0)
            {
              gtk_list_store_append (local_store, &iter);
              gtk_list_store_set (local_store, &iter,
                                  DEVICE_ID_COLUMN, i,
                                  DEVICE_NAME_COLUMN, pp->devices[i].display_name,
                                  -1);
            }
        }
    }

  gtk_tree_view_set_model (network_treeview, GTK_TREE_MODEL (network_store));
  gtk_tree_view_set_model (local_treeview, GTK_TREE_MODEL (local_store));

  if (gtk_tree_model_get_iter_first ((GtkTreeModel *) network_store, &iter))
    gtk_tree_selection_select_iter (
      gtk_tree_view_get_selection (GTK_TREE_VIEW (network_treeview)),
      &iter);

  if (gtk_tree_model_get_iter_first ((GtkTreeModel *) local_store, &iter))
    gtk_tree_selection_select_iter (
      gtk_tree_view_get_selection (GTK_TREE_VIEW (local_treeview)),
      &iter);

  g_object_unref (network_store);
  g_object_unref (local_store);
}

static void
populate_devices_list (PpNewPrinterDialog *pp)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkWidget         *network_treeview;
  GtkWidget         *local_treeview;

  network_treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "network-devices-treeview");

  local_treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "local-devices-treeview");

  actualize_devices_list (pp);
  devices_get (pp);

  renderer = gtk_cell_renderer_text_new ();

  /* Translators: Column of devices which can be installed */
  column = gtk_tree_view_column_new_with_attributes (_("Devices"), renderer,
                                                     "text", DEVICE_NAME_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (network_treeview), column);

  /* Translators: Column of devices which can be installed */
  column = gtk_tree_view_column_new_with_attributes (_("Devices"), renderer,
                                                     "text", DEVICE_NAME_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (local_treeview), column);
}

static void
actualize_device_types_list (PpNewPrinterDialog *pp)
{
  GtkListStore *store;
  GtkTreeView  *treeview;
  GtkTreeIter   iter;
  gint          i;

  treeview = (GtkTreeView*)
    gtk_builder_get_object (pp->builder, "device-types-treeview");

  store = gtk_list_store_new (DEVICE_TYPE_N_COLUMNS,
                              G_TYPE_INT,
                              G_TYPE_STRING,
                              G_TYPE_INT);

  pp->device_connection_types = g_new (gchar*, 2);
  pp->num_device_connection_types = 2;

  /* Translators: Local means local printers */
  pp->device_connection_types[0] = g_strdup (_("Local"));
  /* Translators: Network means network printers */
  pp->device_connection_types[1] = g_strdup (_("Network"));

  for (i = 0; i < pp->num_device_connection_types; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          DEVICE_TYPE_ID_COLUMN, i,
                          DEVICE_TYPE_NAME_COLUMN, pp->device_connection_types[i],
                          DEVICE_TYPE_TYPE_COLUMN, i,
                          -1);
    }

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));

  gtk_tree_model_get_iter_first ((GtkTreeModel *) store,
                                 &iter);

  gtk_tree_selection_select_iter (
    gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
    &iter);

  g_object_unref (store);
}

static void
populate_device_types_list (PpNewPrinterDialog *pp)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkWidget         *treeview;

  treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "device-types-treeview");

  actualize_device_types_list (pp);

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
                    "changed", G_CALLBACK (device_type_selection_changed_cb), pp);

  renderer = gtk_cell_renderer_text_new ();
  /* Translators: Device types column (network or local) */
  column = gtk_tree_view_column_new_with_attributes (_("Device types"), renderer,
                                                     "text", DEVICE_TYPE_NAME_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
}

static void
dialog_closed (GtkWidget          *dialog,
               gint                response_id,
               PpNewPrinterDialog *pp)
{
  gtk_widget_destroy (dialog);
}

static void
new_printer_add_button_cb (GtkButton *button,
                           gpointer   user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog*) user_data;
  GtkResponseType     dialog_response = GTK_RESPONSE_OK;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  GtkWidget          *treeview;
  gchar              *device_name = NULL;
  gchar              *ppd_name = NULL;
  gint                device_id = -1;
  gint                device_type = -1;

  treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "device-types-treeview");

  if (gtk_tree_selection_get_selected (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), &model, &iter))
    gtk_tree_model_get (model, &iter,
                        DEVICE_TYPE_TYPE_COLUMN, &device_type,
                        -1);

  switch (device_type)
    {
      case DEVICE_TYPE_LOCAL:
        treeview = (GtkWidget*)
          gtk_builder_get_object (pp->builder, "local-devices-treeview");
        break;
      case DEVICE_TYPE_NETWORK:
        treeview = (GtkWidget*)
          gtk_builder_get_object (pp->builder, "network-devices-treeview");
        break;
      default:
        treeview = NULL;
        break;
    }

  if (treeview &&
      gtk_tree_selection_get_selected (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  DEVICE_ID_COLUMN, &device_id,
			  DEVICE_NAME_COLUMN, &device_name,
			  -1);
    }

  if (device_id >= 0)
    {
      if (pp->devices[device_id].device_ppd_uri)
        {
          http_t *http;

          http = httpConnectEncrypt (pp->devices[device_id].hostname,
                                     pp->devices[device_id].host_port,
                                     cupsEncryption ());

          if (http)
            {
              const char *ppd_file_name;

              ppd_file_name = cupsGetPPD2 (http, pp->devices[device_id].display_name);

              if (ppd_file_name)
                {
                  DBusGProxy *proxy;
                  GError     *error = NULL;
                  char       *ret_error = NULL;

                  proxy = get_dbus_proxy (MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          TRUE);
                  if (proxy)
                    {
                      dbus_g_proxy_call (proxy, "PrinterAddWithPpdFile", &error,
                                         G_TYPE_STRING, pp->devices[device_id].display_name,
                                         G_TYPE_STRING, pp->devices[device_id].device_uri,
                                         G_TYPE_STRING, ppd_file_name,
                                         G_TYPE_STRING, pp->devices[device_id].device_info,
                                         G_TYPE_STRING, pp->devices[device_id].device_location,
                                         G_TYPE_INVALID,
                                         G_TYPE_STRING, &ret_error,
                                         G_TYPE_INVALID);

                      if (error || (ret_error && ret_error[0] != '\0'))
                        {
                          dialog_response = GTK_RESPONSE_REJECT;

                          if (error)
                            g_warning ("%s", error->message);

                          if (ret_error && ret_error[0] != '\0')
                            g_warning ("%s", ret_error);

                          g_clear_error (&error);
                        }
                      else
                        {
                          ret_error = NULL;

                          dbus_g_proxy_call (proxy, "PrinterSetAcceptJobs", &error,
                                             G_TYPE_STRING, pp->devices[device_id].display_name,
                                             G_TYPE_BOOLEAN, TRUE,
                                             G_TYPE_STRING, "none",
                                             G_TYPE_INVALID,
                                             G_TYPE_STRING, &ret_error,
                                             G_TYPE_INVALID);

                          dbus_g_proxy_call (proxy, "PrinterSetEnabled", &error,
                                             G_TYPE_STRING, pp->devices[device_id].display_name,
                                             G_TYPE_BOOLEAN, TRUE,
                                             G_TYPE_INVALID,
                                             G_TYPE_STRING, &ret_error,
                                             G_TYPE_INVALID);

                          if (error || (ret_error && ret_error[0] != '\0'))
                            {
                              if (error)
                                g_warning ("%s", error->message);

                              if (ret_error && ret_error[0] != '\0')
                                g_warning ("%s", ret_error);

                              g_clear_error (&error);
                            }
                          else
                            {
                              if (g_strcmp0 (pp->devices[device_id].device_class, "direct") == 0)
                                {
                                  gchar *commands = get_dest_attr (pp->devices[device_id].display_name, "printer-commands");
                                  gchar *commands_lowercase = g_ascii_strdown (commands, -1);
                                  ipp_t *response = NULL;

                                  if (g_strrstr (commands_lowercase, "AutoConfigure"))
                                    {
                                      response = execute_maintenance_command (pp->devices[device_id].display_name,
                                                                              "AutoConfigure",
                                      /* Translators: Name of job which makes printer to autoconfigure itself */
                                                                              _("Automatic configuration"));
                                      if (response)
                                        {
                                          if (response->state == IPP_ERROR)
                                          /* Translators: An error has occured during execution of AutoConfigure CUPS maintenance command */
                                            g_warning ("An error has occured during automatic configuration of new printer.");
                                          ippDelete (response);
                                        }
                                    }
                                  g_free (commands);
                                  g_free (commands_lowercase);
                                }
                            }
                        }
                      g_object_unref (proxy);
                    }

                  g_unlink (ppd_file_name);
                }
              else
                {
                  dialog_response = GTK_RESPONSE_REJECT;
                  g_warning ("Getting of PPD for %s from %s:%d failed.",
                             pp->devices[device_id].display_name,
                             pp->devices[device_id].hostname,
                             pp->devices[device_id].host_port);
                }
            }
        }
      else if (pp->devices[device_id].device_id)
        {
          /* Try whether CUPS has a driver for the new printer */
          ppd_name = get_ppd_name (pp->devices[device_id].device_class,
                           pp->devices[device_id].device_id,
                           pp->devices[device_id].device_info,
                           pp->devices[device_id].device_make_and_model,
                           pp->devices[device_id].device_uri,
                           pp->devices[device_id].device_location);

          if (ppd_name == NULL)
            {
              /* Try PackageKit to install printer driver */
              DBusGProxy *proxy;
              GError     *error = NULL;

              proxy = get_dbus_proxy (PACKAGE_KIT_BUS,
                                      PACKAGE_KIT_PATH,
                                      PACKAGE_KIT_IFACE,
                                      FALSE);

              if (proxy)
                {
                  gchar **device_ids = NULL;

                  device_ids = g_new (gchar *, 2);
                  device_ids[0] = pp->devices[device_id].device_id;
                  device_ids[1] = NULL;

                  dbus_g_proxy_call (proxy, "InstallPrinterDrivers", &error,

#ifdef GDK_WINDOWING_X11
                                     G_TYPE_UINT, GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (pp->dialog))),
#else
                                     G_TYPE_UINT, 0,
#endif
                                     G_TYPE_STRV, device_ids,
                                     G_TYPE_STRING, "hide-finished",
                                     G_TYPE_INVALID,
                                     G_TYPE_INVALID);

                  g_object_unref (proxy);

                  if (error)
                    g_warning ("%s", error->message);

                  g_clear_error (&error);

                  /* Search CUPS for driver */
                  ppd_name = get_ppd_name (pp->devices[device_id].device_class,
                               pp->devices[device_id].device_id,
                               pp->devices[device_id].device_info,
                               pp->devices[device_id].device_make_and_model,
                               pp->devices[device_id].device_uri,
                               pp->devices[device_id].device_location);

                  g_free (device_ids);
                }
            }

          /* Add the new printer */
          if (ppd_name)
            {
              DBusGProxy *proxy;
              GError     *error = NULL;
              char       *ret_error = NULL;

              proxy = get_dbus_proxy (MECHANISM_BUS,
                                      "/",
                                      MECHANISM_BUS,
                                      TRUE);
              if (proxy)
                {
                  dbus_g_proxy_call (proxy, "PrinterAdd", &error,
                                     G_TYPE_STRING, pp->devices[device_id].display_name,
                                     G_TYPE_STRING, pp->devices[device_id].device_uri,
                                     G_TYPE_STRING, ppd_name,
                                     G_TYPE_STRING, pp->devices[device_id].device_info,
                                     G_TYPE_STRING, pp->devices[device_id].device_location,
                                     G_TYPE_INVALID,
                                     G_TYPE_STRING, &ret_error,
                                     G_TYPE_INVALID);

                  if (error || (ret_error && ret_error[0] != '\0'))
                    {
                      dialog_response = GTK_RESPONSE_REJECT;

                      if (error)
                        g_warning ("%s", error->message);

                      if (ret_error && ret_error[0] != '\0')
                        g_warning ("%s", ret_error);

                      g_clear_error (&error);
                    }
                  else
                    {
                      ret_error = NULL;

                      dbus_g_proxy_call (proxy, "PrinterSetAcceptJobs", &error,
                                         G_TYPE_STRING, pp->devices[device_id].display_name,
                                         G_TYPE_BOOLEAN, TRUE,
                                         G_TYPE_STRING, "none",
                                         G_TYPE_INVALID,
                                         G_TYPE_STRING, &ret_error,
                                         G_TYPE_INVALID);

                      dbus_g_proxy_call (proxy, "PrinterSetEnabled", &error,
                                         G_TYPE_STRING, pp->devices[device_id].display_name,
                                         G_TYPE_BOOLEAN, TRUE,
                                         G_TYPE_INVALID,
                                         G_TYPE_STRING, &ret_error,
                                         G_TYPE_INVALID);

                      if (error || (ret_error && ret_error[0] != '\0'))
                        {
                          if (error)
                            g_warning ("%s", error->message);

                          if (ret_error && ret_error[0] != '\0')
                            g_warning ("%s", ret_error);

                          g_clear_error (&error);
                        }
                      else
                        {
                          if (g_strcmp0 (pp->devices[device_id].device_class, "direct") == 0)
                            {
                              gchar *commands = get_dest_attr (pp->devices[device_id].display_name, "printer-commands");
                              gchar *commands_lowercase = g_ascii_strdown (commands, -1);
                              ipp_t *response = NULL;

                              if (g_strrstr (commands_lowercase, "AutoConfigure"))
                                {
                                  response = execute_maintenance_command (pp->devices[device_id].display_name,
                                                                          "AutoConfigure",
                                  /* Translators: Name of job which makes printer to autoconfigure itself */
                                                                          _("Automatic configuration"));
                                  if (response)
                                    {
                                      if (response->state == IPP_ERROR)
                                      /* Translators: An error has occured during execution of AutoConfigure CUPS maintenance command */
                                        g_warning ("An error has occured during automatic configuration of new printer.");
                                      ippDelete (response);
                                    }
                                }
                              g_free (commands);
                              g_free (commands_lowercase);
                            }
                        }
                    }
                  g_object_unref (proxy);
                }
            }
        }
    }

  pp_new_printer_dialog_hide (pp);
  pp->user_callback (GTK_DIALOG (pp->dialog), dialog_response, pp->user_data);
}

static void
new_printer_cancel_button_cb (GtkButton *button,
                              gpointer   user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog*) user_data;

  pp_new_printer_dialog_hide (pp);
  pp->user_callback (GTK_DIALOG (pp->dialog), GTK_RESPONSE_CANCEL, pp->user_data);
}

PpNewPrinterDialog *
pp_new_printer_dialog_new (GtkWindow            *parent,
                           UserResponseCallback  user_callback,
                           gpointer              user_data)
{
  PpNewPrinterDialog *pp;
  GtkWidget          *widget;
  GError             *error = NULL;
  gchar              *objects[] = { "dialog", "main-vbox", NULL };

  pp = g_new0 (PpNewPrinterDialog, 1);

  pp->builder = gtk_builder_new ();
  pp->parent = GTK_WIDGET (parent);

  gtk_builder_add_objects_from_file (pp->builder,
                                     DATADIR"/new-printer-dialog.ui",
                                     objects, &error);

  if (error)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  pp->device_connection_types = NULL;
  pp->num_device_connection_types = 0;

  pp->devices = NULL;
  pp->num_devices = 0;

  pp->dialog = (GtkWidget *) gtk_builder_get_object (pp->builder, "dialog");

  pp->user_callback = user_callback;
  pp->user_data = user_data;

  /* connect signals */
  g_signal_connect (pp->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "new-printer-add-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (new_printer_add_button_cb), pp);

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "new-printer-cancel-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (new_printer_cancel_button_cb), pp);

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "search-by-address-checkbutton");
  g_signal_connect (widget, "toggled", G_CALLBACK (search_address_cb), pp);

  gtk_window_set_transient_for (GTK_WINDOW (pp->dialog), GTK_WINDOW (parent));
  gtk_window_set_modal (GTK_WINDOW (pp->dialog), TRUE);
  gtk_window_present (GTK_WINDOW (pp->dialog));
  gtk_widget_show_all (GTK_WIDGET (pp->dialog));

  populate_device_types_list (pp);
  populate_devices_list (pp);

  return pp;
}

void
pp_new_printer_dialog_free (PpNewPrinterDialog *pp)
{
  gint i;

  for (i = 0; i < pp->num_device_connection_types; i++)
    g_free (pp->device_connection_types[i]);
  g_free (pp->device_connection_types);
  pp->device_connection_types = NULL;

  free_devices (pp);

  gtk_widget_destroy (GTK_WIDGET (pp->dialog));
  pp->dialog = NULL;

  g_object_unref (pp->builder);
  pp->builder = NULL;

  g_free (pp);
}

static void
pp_new_printer_dialog_hide (PpNewPrinterDialog *pp)
{
  gtk_widget_hide (GTK_WIDGET (pp->dialog));
}
