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
#include <cups/ppd.h>

#include "pp-new-printer-dialog.h"
#include "pp-utils.h"

#include <libnotify/notify.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#define MECHANISM_BUS "org.opensuse.CupsPkHelper.Mechanism"

#define PACKAGE_KIT_BUS "org.freedesktop.PackageKit"
#define PACKAGE_KIT_PATH "/org/freedesktop/PackageKit"
#define PACKAGE_KIT_MODIFY_IFACE "org.freedesktop.PackageKit.Modify"
#define PACKAGE_KIT_QUERY_IFACE  "org.freedesktop.PackageKit.Query"

#define FIREWALLD_BUS "org.fedoraproject.FirewallD"
#define FIREWALLD_PATH "/org/fedoraproject/FirewallD"
#define FIREWALLD_IFACE "org.fedoraproject.FirewallD"

#define SCP_BUS   "org.fedoraproject.Config.Printing"
#define SCP_PATH  "/org/fedoraproject/Config/Printing"
#define SCP_IFACE "org.fedoraproject.Config.Printing"

#define ALLOWED_CHARACTERS "abcdefghijklmnopqrtsuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_"

static void pp_new_printer_dialog_hide (PpNewPrinterDialog *pp);
static void actualize_devices_list (PpNewPrinterDialog *pp);

enum
{
  NOTEBOOK_LOCAL_PAGE = 0,
  NOTEBOOK_NETWORK_PAGE,
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
  DEVICE_TYPE_NETWORK
};

enum
{
  STANDARD_TAB = 0,
  WARNING_TAB
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

  GCancellable *cancellable;

  gchar    *warning;
  gboolean  show_warning;
  gboolean  searching;
};

static void
show_notification (gchar *primary_text,
                   gchar *secondary_text,
                   gchar *icon_name)
{
  if (primary_text)
    {
      NotifyNotification *notification;
      notification = notify_notification_new (primary_text,
                                              secondary_text,
                                              icon_name);
      notify_notification_set_app_name (notification, _("Printers"));
      notify_notification_set_hint (notification, "transient", g_variant_new_boolean (TRUE));

      notify_notification_show (notification, NULL);
      g_object_unref (notification);
    }
}

static void
device_type_selection_changed_cb (GtkTreeSelection *selection,
                                  gpointer          user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog *) user_data;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  GtkWidget          *treeview = NULL;
  GtkWidget          *notebook = NULL;
  GtkWidget          *widget;
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
      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "device-type-notebook");

      gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), device_type);

      if (device_type == DEVICE_TYPE_LOCAL)
        {
          treeview = (GtkWidget*)
            gtk_builder_get_object (pp->builder, "local-devices-treeview");
          notebook = (GtkWidget*)
            gtk_builder_get_object (pp->builder, "local-devices-notebook");
        }
      else if (device_type == DEVICE_TYPE_NETWORK)
        {
          treeview = (GtkWidget*)
            gtk_builder_get_object (pp->builder, "network-devices-treeview");
          notebook = (GtkWidget*)
            gtk_builder_get_object (pp->builder, "network-devices-notebook");
        }

      if (notebook)
        {
          if (pp->show_warning && device_type == DEVICE_TYPE_NETWORK)
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), WARNING_TAB);
          else
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), STANDARD_TAB);
        }

      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "new-printer-add-button");

      if (treeview)
        gtk_widget_set_sensitive (widget,
          gtk_tree_selection_get_selected (
            gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
            &model,
            &iter));
    }
}

static void
device_selection_changed_cb (GtkTreeSelection *selection,
                             gpointer          user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog *) user_data;
  GtkTreeModel       *model;
  GtkTreeIter         iter;
  GtkWidget          *treeview = NULL;
  GtkWidget          *widget;
  gchar              *device_type_name = NULL;
  gint                device_type_id = -1;
  gint                device_type = -1;

  treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "device-types-treeview");

  if (gtk_tree_selection_get_selected (
        gtk_tree_view_get_selection (
          GTK_TREE_VIEW (treeview)), &model, &iter))
    gtk_tree_model_get (model, &iter,
			DEVICE_TYPE_ID_COLUMN, &device_type_id,
			DEVICE_TYPE_NAME_COLUMN, &device_type_name,
			DEVICE_TYPE_TYPE_COLUMN, &device_type,
			-1);

  if (device_type == DEVICE_TYPE_LOCAL)
    treeview = (GtkWidget*)
      gtk_builder_get_object (pp->builder, "local-devices-treeview");
  else if (device_type == DEVICE_TYPE_NETWORK)
    treeview = (GtkWidget*)
      gtk_builder_get_object (pp->builder, "network-devices-treeview");

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "new-printer-add-button");

  if (treeview)
    gtk_widget_set_sensitive (widget,
      gtk_tree_selection_get_selected (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),
        &model,
        &iter));
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
devices_get_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  PpNewPrinterDialog *pp = user_data;
  cups_dest_t        *dests;
  GHashTable         *devices = NULL;
  GDBusConnection    *bus;
  GtkWidget          *widget = NULL;
  GVariant           *dg_output = NULL;
  gboolean            already_present;
  GError             *error = NULL;
  gchar              *new_name = NULL;
  gchar              *device_uri = NULL;
  char               *ret_error = NULL;
  gint                i, j, k;
  gint                name_index;
  gint                num_dests;


  dg_output = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                        res,
                                        &error);

  /* Do nothing if cancelled */
  if (!dg_output && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return;
    }

  if (dg_output)
    {
      if (g_variant_n_children (dg_output) == 2)
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

                      g_variant_unref (item);
                    }
                }
              g_variant_unref (devices_variant);
            }
        }
      g_variant_unref (dg_output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_object_unref (source_object);

  if (ret_error && ret_error[0] != '\0')
    g_warning ("%s", ret_error);

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
                  if (!name)
                    name = get_tag_value (pp->devices[i].device_id, "model");

                  if (name)
                    name = g_strcanon (name, ALLOWED_CHARACTERS, '-');
                }

              if (!name &&
                  pp->devices[i].device_info)
                {
                  name = g_strdup (pp->devices[i].device_info);
                  if (name)
                    name = g_strcanon (name, ALLOWED_CHARACTERS, '-');
                }

              name_index = 2;
              already_present = FALSE;
              num_dests = cupsGetDests (&dests);
              do
                {
                  if (already_present)
                    {
                      new_name = g_strdup_printf ("%s-%d", name, name_index);
                      name_index++;
                    }
                  else
                    new_name = g_strdup (name);

                  already_present = FALSE;
                  for (j = 0; j < num_dests; j++)
                    if (g_strcmp0 (dests[j].name, new_name) == 0)
                      already_present = TRUE;

                  if (already_present)
                    g_free (new_name);
                  else
                    {
                      g_free (name);
                      name = new_name;
                    }
                } while (already_present);
              cupsFreeDests (num_dests, dests);

              pp->devices[i].display_name = name;
            }

          /* Set show bool
           * Don't show duplicates.
           * Show devices with device-id.
           * Other preferences should apply here.
           */
          bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
          if (bus)
            {
              GVariantBuilder  device_list;
              GVariantBuilder  device_hash;
              GVariant        *output = NULL;
              GVariant        *array = NULL;
              GVariant        *subarray = NULL;

              g_variant_builder_init (&device_list, G_VARIANT_TYPE ("a{sv}"));

              for (i = 0; i < pp->num_devices; i++)
                {
                  if (pp->devices[i].device_uri)
                    {
                      g_variant_builder_init (&device_hash, G_VARIANT_TYPE ("a{ss}"));

                      if (pp->devices[i].device_id)
                        g_variant_builder_add (&device_hash,
                                               "{ss}",
                                               "device-id",
                                               pp->devices[i].device_id);

                      if (pp->devices[i].device_make_and_model)
                        g_variant_builder_add (&device_hash,
                                               "{ss}",
                                               "device-make-and-model",
                                               pp->devices[i].device_make_and_model);

                      if (pp->devices[i].device_class)
                        g_variant_builder_add (&device_hash,
                                               "{ss}",
                                               "device-class",
                                               pp->devices[i].device_class);

                      g_variant_builder_add (&device_list,
                                             "{sv}",
                                             pp->devices[i].device_uri,
                                             g_variant_builder_end (&device_hash));
                    }
                }

              output = g_dbus_connection_call_sync (bus,
                                                    SCP_BUS,
                                                    SCP_PATH,
                                                    SCP_IFACE,
                                                    "GroupPhysicalDevices",
                                                    g_variant_new ("(v)", g_variant_builder_end (&device_list)),
                                                    NULL,
                                                    G_DBUS_CALL_FLAGS_NONE,
                                                    60000,
                                                    NULL,
                                                    &error);

              if (output && g_variant_n_children (output) == 1)
                {
                  array = g_variant_get_child_value (output, 0);
                  if (array)
                    {
                      for (i = 0; i < g_variant_n_children (array); i++)
                        {
                          subarray = g_variant_get_child_value (array, i);
                          if (subarray)
                            {
                              device_uri = g_strdup (g_variant_get_string (
                                             g_variant_get_child_value (subarray, 0),
                                             NULL));

                              for (k = 0; k < pp->num_devices; k++)
                                if (g_str_has_prefix (pp->devices[k].device_uri, device_uri))
                                  pp->devices[k].show = TRUE;

                              g_free (device_uri);
                            }
                        }
                    }
                }

              if (output)
                g_variant_unref (output);
              g_object_unref (bus);
            }

          if (error)
            {
              if (bus == NULL ||
                  (error->domain == G_DBUS_ERROR &&
                   (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
                    error->code == G_DBUS_ERROR_UNKNOWN_METHOD)))
                g_warning ("Install system-config-printer which provides \
DBus method \"GroupPhysicalDevices\" to group duplicates in device list.");

              for (i = 0; i < pp->num_devices; i++)
                pp->devices[i].show = TRUE;
            }

          for (i = 0; i < pp->num_devices; i++)
            if (!pp->devices[i].device_id)
              pp->devices[i].show = FALSE;
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
  gtk_widget_hide (widget);

  if (pp->cancellable != NULL)
    {
      g_object_unref (pp->cancellable);
      pp->cancellable = NULL;
    }
}

static void
devices_get (PpNewPrinterDialog *pp)
{
  GDBusProxy *proxy;
  GError     *error = NULL;
  GVariantBuilder *in_include = NULL;
  GVariantBuilder *in_exclude = NULL;
  GtkWidget *widget = NULL;

  pp->searching = TRUE;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         MECHANISM_BUS,
                                         "/",
                                         MECHANISM_BUS,
                                         NULL,
                                         &error);

  if (!proxy)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      pp->searching = FALSE;
      return;
    }

  if (pp->show_warning)
    {
      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "local-devices-notebook");
      gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), WARNING_TAB);

      widget = (GtkWidget*)
        gtk_builder_get_object (pp->builder, "network-devices-notebook");
      gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), WARNING_TAB);
    }

  in_include = g_variant_builder_new (G_VARIANT_TYPE ("as"));
  in_exclude = g_variant_builder_new (G_VARIANT_TYPE ("as"));

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "get-devices-status-label");
  gtk_label_set_text (GTK_LABEL (widget), _("Getting devices..."));

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "spinner");
  gtk_spinner_start (GTK_SPINNER (widget));
  gtk_widget_set_sensitive (widget, TRUE);
  gtk_widget_show (widget);

  pp->cancellable = g_cancellable_new ();

  g_dbus_proxy_call (proxy,
                     "DevicesGet",
                     g_variant_new ("(iiasas)",
                                    0,
                                    60,
                                    in_include,
                                    in_exclude),
                     G_DBUS_CALL_FLAGS_NONE,
                     60000,
                     pp->cancellable,
                     devices_get_cb,
                     pp);

  pp->searching = FALSE;
}

static gchar **
line_split (gchar *line)
{
  gboolean   escaped = FALSE;
  gboolean   quoted = FALSE;
  gboolean   in_word = FALSE;
  gchar    **words = NULL;
  gchar    **result = NULL;
  gchar     *buffer = NULL;
  gchar      ch;
  gint       n = 0;
  gint       i, j = 0, k = 0;

  if (line)
    {
      n = strlen (line);
      words = g_new0 (gchar *, n + 1);
      buffer = g_new0 (gchar, n + 1);

      for (i = 0; i < n; i++)
        {
          ch = line[i];

          if (escaped)
            {
              buffer[k++] = ch;
              escaped = FALSE;
              continue;
            }

          if (ch == '\\')
            {
              in_word = TRUE;
              escaped = TRUE;
              continue;
            }

          if (in_word)
            {
              if (quoted)
                {
                  if (ch == '"')
                    quoted = FALSE;
                  else
                    buffer[k++] = ch;
                }
              else if (g_ascii_isspace (ch))
                {
                  words[j++] = g_strdup (buffer);
                  memset (buffer, 0, n + 1);
                  k = 0;
                  in_word = FALSE;
                }
              else if (ch == '"')
                quoted = TRUE;
              else
                buffer[k++] = ch;
            }
          else
            {
              if (ch == '"')
                {
                  in_word = TRUE;
                  quoted = TRUE;
                }
              else if (!g_ascii_isspace (ch))
                {
                  in_word = TRUE;
                  buffer[k++] = ch;
                }
            }
        }
    }

  if (buffer && buffer[0] != '\0')
    words[j++] = g_strdup (buffer);

  result = g_strdupv (words);
  g_strfreev (words);
  g_free (buffer);

  return result;
}

static void
service_enable (gchar *service_name,
                gint   service_timeout)
{
  GDBusConnection *bus;
  GVariant   *output = NULL;
  GError     *error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  output = g_dbus_connection_call_sync (bus,
                                        FIREWALLD_BUS,
                                        FIREWALLD_PATH,
                                        FIREWALLD_IFACE,
                                        "enableService",
                                        g_variant_new ("(si)",
                                                       service_name,
                                                       service_timeout),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        60000,
                                        NULL,
                                        &error);

  g_object_unref (bus);

  if (output)
    {
      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static void
service_disable (gchar *service_name)
{
  GDBusConnection *bus;
  GVariant   *output = NULL;
  GError     *error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return;
    }

  output = g_dbus_connection_call_sync (bus,
                                        FIREWALLD_BUS,
                                        FIREWALLD_PATH,
                                        FIREWALLD_IFACE,
                                        "disableService",
                                        g_variant_new ("(s)", service_name),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        60000,
                                        NULL,
                                        &error);

  g_object_unref (bus);

  if (output)
    {
      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
}

static gboolean
service_enabled (gchar *service_name)
{
  GDBusConnection *bus;
  GVariant   *output = NULL;
  GError     *error = NULL;
  gint        query_result = 0;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return FALSE;
    }

  output = g_dbus_connection_call_sync (bus,
                                        FIREWALLD_BUS,
                                        FIREWALLD_PATH,
                                        FIREWALLD_IFACE,
                                        "queryService",
                                        g_variant_new ("(s)", service_name),
                                        G_VARIANT_TYPE ("(i)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        60000,
                                        NULL,
                                        &error);

  g_object_unref (bus);

  if (output)
    {
      if (g_variant_n_children (output) == 1)
        g_variant_get (output, "(i)", &query_result);
      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return FALSE;
    }

  if (query_result > 0)
    return TRUE;
  else
    return FALSE;
}

static gboolean
dbus_method_available (gchar *name,
                       gchar *path,
                       gchar *iface,
                       gchar *method)
{
  GDBusConnection *bus;
  GError     *error = NULL;
  GVariant   *output = NULL;
  gboolean    result = FALSE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return FALSE;
    }

  output = g_dbus_connection_call_sync (bus,
                                        name,
                                        path,
                                        iface,
                                        method,
                                        NULL,
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        60000,
                                        NULL,
                                        &error);

  g_object_unref (bus);

  if (output)
    {
      g_variant_unref (output);
      result = TRUE;
    }
  else
    {
      if (error->domain == G_DBUS_ERROR &&
          error->code == G_DBUS_ERROR_SERVICE_UNKNOWN)
        result = FALSE;
      else
        result = TRUE;
    }

  return result;
}

static void
search_address_cb (GtkToggleButton *togglebutton,
                   gpointer         user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog*) user_data;
  GtkWidget *widget;
  gint i;

  pp->searching = TRUE;

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
          cups_dest_t *dests = NULL;
          http_t      *http;
          GError      *error = NULL;
          gchar       *tmp = NULL;
          gchar       *host = NULL;
          gchar       *port_string = NULL;
          gchar       *position;
          gchar       *command;
          gchar       *standard_output = NULL;
          gint         exit_status = -1;
          gint         num_dests = 0;
          gint         length;
          int          port = 631;

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

          if (host)
            {
              /* Use CUPS to get printer's informations */
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

              /* Use SNMP to get printer's informations */
              command = g_strdup_printf ("/usr/lib/cups/backend/snmp %s", host);
              if (g_spawn_command_line_sync (command, &standard_output, NULL, &exit_status, &error))
                {
                  if (exit_status == 0 && standard_output)
                    {
                      gchar **printer_informations = NULL;

                      printer_informations = line_split (standard_output);
                      length = g_strv_length (printer_informations);

                      if (length >= 4)
                        {
                          CupsDevice *devices = NULL;
                          devices = g_new0 (CupsDevice, pp->num_devices + 1);

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

                          pp->devices[pp->num_devices].device_class = g_strdup (printer_informations[0]);
                          pp->devices[pp->num_devices].device_uri = g_strdup (printer_informations[1]);
                          pp->devices[pp->num_devices].device_make_and_model = g_strdup (printer_informations[2]);
                          pp->devices[pp->num_devices].device_info = g_strdup (printer_informations[3]);
                          pp->devices[pp->num_devices].display_name = g_strdup (printer_informations[3]);
                          pp->devices[pp->num_devices].display_name =
                            g_strcanon (pp->devices[pp->num_devices].display_name, ALLOWED_CHARACTERS, '-');
                          pp->devices[pp->num_devices].show = TRUE;
                          pp->devices[pp->num_devices].hostname = g_strdup (host);
                          pp->devices[pp->num_devices].host_port = port;
                          pp->devices[pp->num_devices].found = TRUE;

                          if (length >= 5 && printer_informations[4][0] != '\0')
                            pp->devices[pp->num_devices].device_id = g_strdup (printer_informations[4]);

                          if (length >= 6 && printer_informations[5][0] != '\0')
                            pp->devices[pp->num_devices].device_location = g_strdup (printer_informations[5]);

                          pp->num_devices++;
                        }
                      g_strfreev (printer_informations);
                      g_free (standard_output);
                    }
                }
              else
                {
                  g_warning ("%s", error->message);
                  g_error_free (error);
                }

              g_free (command);
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

  pp->searching = FALSE;

  actualize_devices_list (pp);
}

static void
actualize_devices_list (PpNewPrinterDialog *pp)
{
  GtkListStore *network_store;
  GtkListStore *local_store;
  GtkTreeModel *model;
  GtkTreeView  *network_treeview;
  GtkTreeView  *local_treeview;
  GtkTreeIter   iter;
  GtkWidget    *treeview;
  GtkWidget    *widget;
  GtkWidget    *local_notebook;
  GtkWidget    *network_notebook;
  gboolean      no_local_device = TRUE;
  gboolean      no_network_device = TRUE;
  gint          i;
  gint          device_type = -1;

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
      if ((pp->devices[i].device_id || pp->devices[i].device_ppd_uri) &&
          pp->devices[i].show)
        {
          if (g_strcmp0 (pp->devices[i].device_class, "network") == 0)
            {
              gtk_list_store_append (network_store, &iter);
              gtk_list_store_set (network_store, &iter,
                                  DEVICE_ID_COLUMN, i,
                                  DEVICE_NAME_COLUMN, pp->devices[i].display_name,
                                  -1);
              pp->show_warning = FALSE;
              no_network_device = FALSE;
            }
          else if (g_strcmp0 (pp->devices[i].device_class, "direct") == 0)
            {
              gtk_list_store_append (local_store, &iter);
              gtk_list_store_set (local_store, &iter,
                                  DEVICE_ID_COLUMN, i,
                                  DEVICE_NAME_COLUMN, pp->devices[i].display_name,
                                  -1);
              no_local_device = FALSE;
            }
        }
    }

  if (no_local_device && !pp->searching)
    {
      gtk_list_store_append (local_store, &iter);
      gtk_list_store_set (local_store, &iter,
                          DEVICE_ID_COLUMN, 0,
      /* Translators: No localy connected printers were found */
                          DEVICE_NAME_COLUMN, _("No local printers found"),
                          -1);
      gtk_widget_set_sensitive (GTK_WIDGET (local_treeview), FALSE);
    }
  else
    gtk_widget_set_sensitive (GTK_WIDGET (local_treeview), TRUE);

  if (no_network_device && !pp->show_warning && !pp->searching)
    {
      gtk_list_store_append (network_store, &iter);
      gtk_list_store_set (network_store, &iter,
                          DEVICE_ID_COLUMN, 0,
      /* Translators: No network printers were found */
                          DEVICE_NAME_COLUMN, _("No network printers found"),
                          -1);
      gtk_widget_set_sensitive (GTK_WIDGET (network_treeview), FALSE);
    }
  else
    gtk_widget_set_sensitive (GTK_WIDGET (network_treeview), TRUE);

  gtk_tree_view_set_model (network_treeview, GTK_TREE_MODEL (network_store));
  gtk_tree_view_set_model (local_treeview, GTK_TREE_MODEL (local_store));

  if (!no_network_device &&
      gtk_tree_model_get_iter_first ((GtkTreeModel *) network_store, &iter))
    gtk_tree_selection_select_iter (
      gtk_tree_view_get_selection (GTK_TREE_VIEW (network_treeview)),
      &iter);

  if (!no_local_device &&
      gtk_tree_model_get_iter_first ((GtkTreeModel *) local_store, &iter))
    gtk_tree_selection_select_iter (
      gtk_tree_view_get_selection (GTK_TREE_VIEW (local_treeview)),
      &iter);

  treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "device-types-treeview");

  if (gtk_tree_selection_get_selected (
        gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)), &model, &iter))
    gtk_tree_model_get (model, &iter,
                        DEVICE_TYPE_TYPE_COLUMN, &device_type,
                        -1);

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "device-type-notebook");

  local_notebook = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "local-devices-notebook");

  network_notebook = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "network-devices-notebook");

  gtk_notebook_set_current_page (GTK_NOTEBOOK (network_notebook), pp->show_warning ? WARNING_TAB : STANDARD_TAB);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (local_notebook), STANDARD_TAB);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), device_type);

  g_object_unref (network_store);
  g_object_unref (local_store);
}

static void
populate_devices_list (PpNewPrinterDialog *pp)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTextBuffer     *text_buffer;
  GtkTextView       *warning_textview;
  GtkTextIter        text_iter;
  GtkWidget         *network_treeview;
  GtkWidget         *local_treeview;

  network_treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "network-devices-treeview");

  local_treeview = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "local-devices-treeview");

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (network_treeview)),
                    "changed", G_CALLBACK (device_selection_changed_cb), pp);

  g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (local_treeview)),
                    "changed", G_CALLBACK (device_selection_changed_cb), pp);

  actualize_devices_list (pp);

  if (dbus_method_available (FIREWALLD_BUS,
                             FIREWALLD_PATH,
                             FIREWALLD_IFACE,
                             "getServices"))
    {
      if (!service_enabled ("mdns"))
        service_enable ("mdns", 300);

      if (!service_enabled ("ipp"))
        service_enable ("ipp", 300);

      if (!service_enabled ("ipp-client"))
        service_enable ("ipp-client", 300);

      if (!service_enabled ("samba-client"))
        service_enable ("samba-client", 300);
    }
  else
    {
      pp->warning = g_strdup (_("FirewallD is not running. \
Network printer detection needs services mdns, ipp, ipp-client \
and samba-client enabled on firewall."));

      warning_textview = (GtkTextView*)
        gtk_builder_get_object (pp->builder, "local-warning");
      text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (warning_textview));

      gtk_text_buffer_set_text (text_buffer, "", 0);
      gtk_text_buffer_get_iter_at_offset (text_buffer, &text_iter, 0);
      gtk_text_buffer_insert (text_buffer, &text_iter, pp->warning, -1);

      warning_textview = (GtkTextView*)
        gtk_builder_get_object (pp->builder, "network-warning");
      text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (warning_textview));

      gtk_text_buffer_set_text (text_buffer, "", 0);
      gtk_text_buffer_get_iter_at_offset (text_buffer, &text_iter, 0);
      gtk_text_buffer_insert (text_buffer, &text_iter, pp->warning, -1);

      pp->show_warning = TRUE;
    }

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
  pp->device_connection_types[0] = g_strdup (C_("printer type", "Local"));
  /* Translators: Network means network printers */
  pp->device_connection_types[1] = g_strdup (C_("printer type", "Network"));

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

static GList *
glist_uniq (GList *list)
{
  GList *result = NULL;
  GList *iter = NULL;
  GList *tmp = NULL;

  for (iter = list; iter; iter = iter->next)
    {
      if (tmp == NULL ||
          g_strcmp0 ((gchar *) tmp->data, (gchar *) iter->data) != 0)
        {
          tmp = iter;
          result = g_list_append (result, g_strdup (iter->data));
        }
    }

  g_list_free_full (list, g_free);

  return result;
}

static void
new_printer_add_button_cb (GtkButton *button,
                           gpointer   user_data)
{
  PpNewPrinterDialog *pp = (PpNewPrinterDialog*) user_data;
  GtkResponseType     dialog_response = GTK_RESPONSE_OK;
  GtkTreeModel       *model;
  cups_dest_t        *dests;
  GtkTreeIter         iter;
  GtkWidget          *treeview;
  gboolean            success = FALSE;
  PPDName            *ppd_name = NULL;
  gchar              *device_name = NULL;
  gint                device_id = -1;
  gint                device_type = -1;
  gint                i;
  int                 num_dests;

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
                  GDBusConnection *bus;
                  GError     *error = NULL;

                  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
                  if (!bus)
                    {
                      g_warning ("Failed to get system bus: %s", error->message);
                      g_error_free (error);
                    }
                  else
                    {
                      GVariant *output;

                      output = g_dbus_connection_call_sync (bus,
                                                            MECHANISM_BUS,
                                                            "/",
                                                            MECHANISM_BUS,
                                                            "PrinterAddWithPpdFile",
                                                            g_variant_new ("(sssss)",
                                                                           pp->devices[device_id].display_name,
                                                                           pp->devices[device_id].device_uri,
                                                                           ppd_file_name,
                                                                           pp->devices[device_id].device_info ? pp->devices[device_id].device_info : "",
                                                                           pp->devices[device_id].device_location ? pp->devices[device_id].device_location : ""),
                                                            G_VARIANT_TYPE ("(s)"),
                                                            G_DBUS_CALL_FLAGS_NONE,
                                                            -1,
                                                            NULL,
                                                            &error);
                      g_object_unref (bus);

                      if (output)
                        {
                          const gchar *ret_error;

                          g_variant_get (output, "(&s)", &ret_error);
                          if (ret_error[0] != '\0')
                            {
                              g_warning ("%s", ret_error);
                              dialog_response = GTK_RESPONSE_REJECT;
                            }
                          else
                            success = TRUE;

                          g_variant_unref (output);
                        }
                      else
                        {
                          g_warning ("%s", error->message);
                          g_error_free (error);
                          dialog_response = GTK_RESPONSE_REJECT;
                        }
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
          ppd_name = get_ppd_name (pp->devices[device_id].device_id,
                       pp->devices[device_id].device_make_and_model,
                       pp->devices[device_id].device_uri);

          if (ppd_name == NULL || ppd_name->ppd_match_level < PPD_EXACT_MATCH)
            {
              /* Try PackageKit to install printer driver */
              GDBusConnection *bus;
              GError     *error = NULL;

              bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
              if (!bus)
                {
                  g_warning ("Failed to get session bus: %s", error->message);
                  g_error_free (error);
                }
              else
                {
                  GVariantBuilder array_builder;
                  GVariant *output;
                  guint window_id = 0;

                  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
                  g_variant_builder_add (&array_builder, "s", pp->devices[device_id].device_id);

#ifdef GDK_WINDOWING_X11
                  window_id = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (pp->dialog)));
#endif

                  output = g_dbus_connection_call_sync (bus,
                                                        PACKAGE_KIT_BUS,
                                                        PACKAGE_KIT_PATH,
                                                        PACKAGE_KIT_MODIFY_IFACE,
                                                        "InstallPrinterDrivers",
                                                        g_variant_new ("(uass)",
                                                                       window_id,
                                                                       &array_builder,
                                                                       "hide-finished"),
                                                        G_VARIANT_TYPE ("()"),
                                                        G_DBUS_CALL_FLAGS_NONE,
                                                        3600000,
                                                        NULL,
                                                        &error);
                  g_object_unref (bus);

                  if (output)
                    g_variant_unref (output);
                  else
                    {
                      g_warning ("%s", error->message);
                      g_error_free (error);
                    }

                  if (ppd_name)
                    {
                      g_free (ppd_name->ppd_name);
                      g_free (ppd_name);
                    }

                  /* Search CUPS for driver */
                  ppd_name = get_ppd_name (pp->devices[device_id].device_id,
                               pp->devices[device_id].device_make_and_model,
                               pp->devices[device_id].device_uri);
                }
            }

          /* Add the new printer */
          if (ppd_name && ppd_name->ppd_name)
            {
              GDBusConnection *bus;
              GError     *error = NULL;
              GVariant   *output;

              bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
              if (!bus)
                {
                  g_warning ("Failed to get system bus: %s", error->message);
                  g_error_free (error);
                }
              else
                {
                  output = g_dbus_connection_call_sync (bus,
                                                        MECHANISM_BUS,
                                                        "/",
                                                        MECHANISM_BUS,
                                                        "PrinterAdd",
                                                        g_variant_new ("(sssss)",
                                                                       pp->devices[device_id].display_name,
                                                                       pp->devices[device_id].device_uri,
                                                                       ppd_name->ppd_name,
                                                                       pp->devices[device_id].device_info ? pp->devices[device_id].device_info : "",
                                                                       pp->devices[device_id].device_location ? pp->devices[device_id].device_location : ""),
                                                        G_VARIANT_TYPE ("(s)"),
                                                        G_DBUS_CALL_FLAGS_NONE,
                                                        -1,
                                                        NULL,
                                                        &error);
                  g_object_unref (bus);

                  if (output)
                    {
                      const gchar *ret_error;

                      g_variant_get (output, "(&s)", &ret_error);
                      if (ret_error[0] != '\0')
                        {
                          g_warning ("%s", ret_error);
                          dialog_response = GTK_RESPONSE_REJECT;
                        }

                      g_variant_unref (output);
                    }
                  else
                    {
                      g_warning ("%s", error->message);
                      g_error_free (error);
                      dialog_response = GTK_RESPONSE_REJECT;
                    }
                }

              g_free (ppd_name->ppd_name);
              g_free (ppd_name);
            }

          num_dests = cupsGetDests (&dests);
          for (i = 0; i < num_dests; i++)
            if (g_strcmp0 (dests[i].name, pp->devices[device_id].display_name) == 0)
              success = TRUE;
          cupsFreeDests (num_dests, dests);
        }

      /* Set some options of the new printer */
      if (success)
        {
          const char *ppd_file_name = NULL;
          GDBusConnection *bus;
          GError     *error = NULL;

          ppd_file_name = cupsGetPPD (pp->devices[device_id].display_name);

          printer_set_accepting_jobs (pp->devices[device_id].display_name, TRUE, NULL);
          printer_set_enabled (pp->devices[device_id].display_name, TRUE);

          if (g_strcmp0 (pp->devices[device_id].device_class, "direct") == 0)
            {
              gchar *commands = get_dest_attr (pp->devices[device_id].display_name, "printer-commands");
              gchar *commands_lowercase = g_ascii_strdown (commands, -1);
              ipp_t *response = NULL;

              if (g_strrstr (commands_lowercase, "autoconfigure"))
                {
                  response = execute_maintenance_command (pp->devices[device_id].display_name,
                                                          "AutoConfigure",
                  /* Translators: Name of job which makes printer to autoconfigure itself */
                                                          _("Automatic configuration"));
                  if (response)
                    {
                      if (response->state == IPP_ERROR)
                        g_warning ("An error has occured during automatic configuration of new printer.");
                      ippDelete (response);
                    }
                }
              g_free (commands);
              g_free (commands_lowercase);
            }

          printer_set_default_media_size (pp->devices[device_id].display_name);

          if (pp->devices[device_id].device_uri &&
              dbus_method_available (FIREWALLD_BUS,
                                     FIREWALLD_PATH,
                                     FIREWALLD_IFACE,
                                     "getServices"))
            {
              if (g_str_has_prefix (pp->devices[device_id].device_uri, "dnssd:") ||
                  g_str_has_prefix (pp->devices[device_id].device_uri, "mdns:"))
                {
                  show_notification (_("Opening firewall for mDNS connections"),
                                     NULL,
                                     "dialog-information-symbolic");
                  service_disable ("mdns");
                  service_enable ("mdns", 0);
                }

              if (g_strrstr (pp->devices[device_id].device_uri, "smb:") != NULL)
                {
                  show_notification (_("Opening firewall for Samba connections"),
                                     NULL,
                                     "dialog-information-symbolic");
                  service_disable ("samba-client");
                  service_enable ("samba-client", 0);
                }

              if (g_strrstr (pp->devices[device_id].device_uri, "ipp:") != NULL)
                {
                  show_notification (_("Opening firewall for IPP connections"),
                                     NULL,
                                     "dialog-information-symbolic");
                  service_disable ("ipp");
                  service_enable ("ipp", 0);
                  service_disable ("ipp-client");
                  service_enable ("ipp-client", 0);
                }
            }

          if (ppd_file_name)
            {
              GVariant   *output;
              GVariant   *array;
              GList      *executables = NULL;
              GList      *packages = NULL;

              error = NULL;

              bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
              if (bus)
                {
                  output = g_dbus_connection_call_sync (bus,
                                                        SCP_BUS,
                                                        SCP_PATH,
                                                        SCP_IFACE,
                                                        "MissingExecutables",
                                                        g_variant_new ("(s)", ppd_file_name),
                                                        NULL,
                                                        G_DBUS_CALL_FLAGS_NONE,
                                                        60000,
                                                        NULL,
                                                        &error);
                  g_object_unref (bus);

                  if (output)
                    {
                      if (g_variant_n_children (output) == 1)
                        {
                          array = g_variant_get_child_value (output, 0);
                          if (array)
                            {
                              for (i = 0; i < g_variant_n_children (array); i++)
                                {
                                  executables = g_list_append (
                                                  executables,
                                                    g_strdup (g_variant_get_string (
                                                      g_variant_get_child_value (array, i),
                                                      NULL)));
                                }
                            }
                        }
                      g_variant_unref (output);
                    }
                }

              if (bus == NULL ||
                  (error &&
                   error->domain == G_DBUS_ERROR &&
                   (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
                    error->code == G_DBUS_ERROR_UNKNOWN_METHOD)))
                {
                  g_warning ("Install system-config-printer which provides \
DBus method \"MissingExecutables\" to find missing executables and filters.");
                  g_error_free (error);
                }

              executables = g_list_sort (executables, (GCompareFunc) g_strcmp0);
              executables = glist_uniq (executables);

              if (executables)
                {
                  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
                  if (bus)
                    {
                      GList *exec_iter;

                      for (exec_iter = executables; exec_iter; exec_iter = exec_iter->next)
                        {
                          output = g_dbus_connection_call_sync (bus,
                                                                PACKAGE_KIT_BUS,
                                                                PACKAGE_KIT_PATH,
                                                                PACKAGE_KIT_QUERY_IFACE,
                                                                "SearchFile",
                                                                g_variant_new ("(ss)",
                                                                               (gchar *) exec_iter->data,
                                                                               ""),
                                                                G_VARIANT_TYPE ("(bs)"),
                                                                G_DBUS_CALL_FLAGS_NONE,
                                                                60000,
                                                                NULL,
                                                                &error);

                          if (output)
                            {
                              gboolean  installed;
                              gchar    *package;

                              g_variant_get (output,
                                             "(bs)",
                                             &installed,
                                             &package);
                              if (!installed)
                                packages = g_list_append (packages, g_strdup (package));
                              g_variant_unref (output);
                            }
                          else
                            {
                              g_warning ("%s", error->message);
                              g_error_free (error);
                            }
                        }

                      g_object_unref (bus);
                    }
                  else
                    {
                      g_warning ("%s", error->message);
                      g_error_free (error);
                    }

                  g_list_free_full (executables, g_free);
                }

              packages = g_list_sort (packages, (GCompareFunc) g_strcmp0);
              packages = glist_uniq (packages);

              if (packages)
                {
                  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
                  if (bus)
                    {
                      GVariantBuilder  array_builder;
                      GList           *pkg_iter;
                      guint            window_id = 0;

                      g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));

                      for (pkg_iter = packages; pkg_iter; pkg_iter = pkg_iter->next)
                        g_variant_builder_add (&array_builder,
                                               "s",
                                               (gchar *) pkg_iter->data);

#ifdef GDK_WINDOWING_X11
                      window_id = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (pp->dialog))),
#endif

                      output = g_dbus_connection_call_sync (bus,
                                                            PACKAGE_KIT_BUS,
                                                            PACKAGE_KIT_PATH,
                                                            PACKAGE_KIT_MODIFY_IFACE,
                                                            "InstallPackageNames",
                                                            g_variant_new ("(uass)",
                                                                           window_id,
                                                                           &array_builder,
                                                                           "hide-finished"),
                                                            NULL,
                                                            G_DBUS_CALL_FLAGS_NONE,
                                                            60000,
                                                            NULL,
                                                            &error);
                      g_object_unref (bus);

                      if (output)
                        {
                          g_variant_unref (output);
                        }
                      else
                        {
                          g_warning ("%s", error->message);
                          g_error_free (error);
                        }
                    }
                  else
                    {
                      g_warning ("%s", error->message);
                      g_error_free (error);
                    }

                  g_list_free_full (packages, g_free);
                }
            }

          if (ppd_file_name)
            g_unlink (ppd_file_name);
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
  guint               builder_result;

  pp = g_new0 (PpNewPrinterDialog, 1);

  pp->builder = gtk_builder_new ();
  pp->parent = GTK_WIDGET (parent);

  builder_result = gtk_builder_add_objects_from_file (pp->builder,
                                                      DATADIR"/new-printer-dialog.ui",
                                                      objects, &error);

  if (builder_result == 0)
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

  pp->cancellable = NULL;
  pp->warning = NULL;
  pp->show_warning = FALSE;

  /* connect signals */
  g_signal_connect (pp->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  widget = (GtkWidget*)
    gtk_builder_get_object (pp->builder, "new-printer-add-button");
  g_signal_connect (widget, "clicked", G_CALLBACK (new_printer_add_button_cb), pp);
  gtk_widget_set_sensitive (widget, FALSE);

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

  pp->searching = TRUE;
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

  if (pp->cancellable)
    {
      g_cancellable_cancel (pp->cancellable);
      g_object_unref (pp->cancellable);
    }

  g_free (pp->warning);

  g_free (pp);
}

static void
pp_new_printer_dialog_hide (PpNewPrinterDialog *pp)
{
  gtk_widget_hide (GTK_WIDGET (pp->dialog));
}
