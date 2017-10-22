/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "shell/list-box-helper.h"
#include "cc-notifications-panel.h"
#include "cc-notifications-resources.h"
#include "cc-edit-dialog.h"

#define MASTER_SCHEMA "org.gnome.desktop.notifications"
#define APP_SCHEMA MASTER_SCHEMA ".application"
#define APP_PREFIX "/org/gnome/desktop/notifications/application/"

struct _CcNotificationsPanel {
  CcPanel parent_instance;

  GSettings *master_settings;
  GtkBuilder *builder;

  GCancellable *apps_load_cancellable;

  GHashTable *known_applications;

  GtkAdjustment *focus_adjustment;

  GList *sections;
  GList *sections_reverse;

  GDBusProxy *perm_store;
};

struct _CcNotificationsPanelClass {
  CcPanelClass parent;
};

typedef struct {
  char *canonical_app_id;
  GAppInfo *app_info;
  GSettings *settings;

  /* Temporary pointer, to pass from the loading thread
     to the app */
  CcNotificationsPanel *panel;
} Application;

static void application_free (Application *app);
static void build_app_store (CcNotificationsPanel *panel);
static void select_app      (GtkListBox *box, GtkListBoxRow *row, CcNotificationsPanel *panel);
static int  sort_apps       (gconstpointer one, gconstpointer two, gpointer user_data);

CC_PANEL_REGISTER (CcNotificationsPanel, cc_notifications_panel);

static void
cc_notifications_panel_dispose (GObject *object)
{
  CcNotificationsPanel *panel = CC_NOTIFICATIONS_PANEL (object);

  g_clear_object (&panel->builder);
  g_clear_object (&panel->master_settings);
  g_clear_pointer (&panel->known_applications, g_hash_table_unref);
  g_clear_pointer (&panel->sections, g_list_free);
  g_clear_pointer (&panel->sections_reverse, g_list_free);

  g_cancellable_cancel (panel->apps_load_cancellable);

  G_OBJECT_CLASS (cc_notifications_panel_parent_class)->dispose (object);
}

static void
cc_notifications_panel_finalize (GObject *object)
{
  CcNotificationsPanel *panel = CC_NOTIFICATIONS_PANEL (object);

  g_clear_object (&panel->apps_load_cancellable);
  g_clear_object (&panel->perm_store);

  G_OBJECT_CLASS (cc_notifications_panel_parent_class)->finalize (object);
}

static gboolean
keynav_failed (GtkWidget            *widget,
               GtkDirectionType      direction,
               CcNotificationsPanel *panel)
{
  gdouble  value, lower, upper, page;
  GList   *item, *sections;

  /* Find the widget in the list of GtkWidgets */
  if (direction == GTK_DIR_DOWN)
    sections = panel->sections;
  else
    sections = panel->sections_reverse;

  item = g_list_find (sections, widget);
  g_assert (item);
  if (item->next)
    {
      gtk_widget_child_focus (GTK_WIDGET (item->next->data), direction);
      return TRUE;
    }

  value = gtk_adjustment_get_value (panel->focus_adjustment);
  lower = gtk_adjustment_get_lower (panel->focus_adjustment);
  upper = gtk_adjustment_get_upper (panel->focus_adjustment);
  page  = gtk_adjustment_get_page_size (panel->focus_adjustment);

  if (direction == GTK_DIR_UP && value > lower)
    {
      gtk_adjustment_set_value (panel->focus_adjustment, lower);
      return TRUE;
    }
  else if (direction == GTK_DIR_DOWN && value < upper - page)
    {
      gtk_adjustment_set_value (panel->focus_adjustment, upper - page);
      return TRUE;
    }

  return FALSE;
}

static void
on_perm_store_ready (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data)
{
  CcNotificationsPanel *self;
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to xdg-app permission store: %s",
                     error->message);
      g_error_free (error);

      return;
    }
  self = user_data;
  self->perm_store = proxy;
}

static void
cc_notifications_panel_init (CcNotificationsPanel *panel)
{
  GtkWidget *w;
  GtkWidget *label;
  GError *error = NULL;

  g_resources_register (cc_notifications_get_resource ());
  panel->known_applications = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     NULL, g_free);

  panel->builder = gtk_builder_new ();
  if (gtk_builder_add_from_resource (panel->builder,
                                     "/org/gnome/control-center/notifications/notifications.ui",
                                     &error) == 0)
    {
      g_error ("Error loading UI file: %s", error->message);
      g_error_free (error);
      return;
    }

  panel->master_settings = g_settings_new (MASTER_SCHEMA);

  g_settings_bind (panel->master_settings, "show-banners",
                   gtk_builder_get_object (panel->builder, "ccnotify-switch-banners"),
                   "active", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (panel->master_settings, "show-in-lock-screen",
                   gtk_builder_get_object (panel->builder, "ccnotify-switch-lock-screen"),
                   "active", G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                          "ccnotify-main-scrolled-window"));
  panel->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (w));

  w = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                          "ccnotify-main-box"));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (w), panel->focus_adjustment);

  w = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                          "ccnotify-switch-listbox"));
  panel->sections = g_list_append (panel->sections, w);
  panel->sections_reverse = g_list_prepend (panel->sections_reverse, w);
  g_signal_connect (w, "keynav-failed", G_CALLBACK (keynav_failed), panel);
  gtk_list_box_set_header_func (GTK_LIST_BOX (w),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  label = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                              "label1"));
  w = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                          "ccnotify-app-listbox"));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (label)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (w)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (w)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (label)));

  panel->sections = g_list_append (panel->sections, w);
  panel->sections_reverse = g_list_prepend (panel->sections_reverse, w);
  g_signal_connect (w, "keynav-failed", G_CALLBACK (keynav_failed), panel);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (w), (GtkListBoxSortFunc)sort_apps, NULL, NULL);
  gtk_list_box_set_header_func (GTK_LIST_BOX (w),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  g_signal_connect (GTK_LIST_BOX (w), "row-activated",
                    G_CALLBACK (select_app), panel);

  build_app_store (panel);

  w = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                          "ccnotify-main-scrolled-window"));
  gtk_container_add (GTK_CONTAINER (panel), w);

  gtk_widget_show (w);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            panel->apps_load_cancellable,
                            on_perm_store_ready,
                            panel);
}

static const char *
cc_notifications_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/shell-notifications";
}

static void
cc_notifications_panel_class_init (CcNotificationsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_notifications_panel_get_help_uri;

  /* Separate dispose() and finalize() functions are necessary
   * to make sure we cancel the running thread before the panel
   * gets finalized */
  object_class->dispose = cc_notifications_panel_dispose;
  object_class->finalize = cc_notifications_panel_finalize;
}

static inline GQuark
application_quark (void)
{
  static GQuark quark;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("cc-application");

  return quark;
}

static gboolean
on_off_label_mapping_get (GValue   *value,
                          GVariant *variant,
                          gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("On") : _("Off"));

  return TRUE;
}

static void
add_application (CcNotificationsPanel *panel,
                 Application          *app)
{
  GtkWidget *box, *w, *row, *list_box;
  GIcon *icon;
  const gchar *app_name;
  int size;

  app_name = g_app_info_get_name (app->app_info);
  if (app_name == NULL || *app_name == '\0')
    return;

  icon = g_app_info_get_icon (app->app_info);
  if (icon == NULL)
    icon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (icon);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (box), 10);

  row = gtk_list_box_row_new ();
  g_object_set_qdata_full (G_OBJECT (row), application_quark (),
                           app, (GDestroyNotify) application_free);

  list_box = GTK_WIDGET (gtk_builder_get_object (panel->builder,
                                                 "ccnotify-app-listbox"));

  gtk_container_add (GTK_CONTAINER (list_box), row);
  gtk_container_add (GTK_CONTAINER (row), box);

  w = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_DIALOG);
  gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &size, NULL);
  gtk_image_set_pixel_size (GTK_IMAGE (w), size);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (gtk_builder_get_object (panel->builder, "sizegroup1")), w);
  gtk_container_add (GTK_CONTAINER (box), w);
  g_object_unref (icon);

  w = gtk_label_new (app_name);
  gtk_container_add (GTK_CONTAINER (box), w);

  w = gtk_label_new ("");
  g_settings_bind_with_mapping (app->settings, "enable",
                                w, "label",
                                G_SETTINGS_BIND_GET |
                                G_SETTINGS_BIND_NO_SENSITIVITY,
                                on_off_label_mapping_get,
                                NULL,
                                NULL,
                                NULL);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), w, FALSE, FALSE, 0);

  gtk_widget_show_all (row);

  g_hash_table_add (panel->known_applications, g_strdup (app->canonical_app_id));
}

static void
maybe_add_app_id (CcNotificationsPanel *panel,
                  const char *canonical_app_id)
{
  Application *app;
  gchar *path;
  gchar *full_app_id;
  GSettings *settings;
  GAppInfo *app_info;

  if (*canonical_app_id == '\0')
    return;

  if (g_hash_table_contains (panel->known_applications,
                             canonical_app_id))
    return;

  path = g_strconcat (APP_PREFIX, canonical_app_id, "/", NULL);
  settings = g_settings_new_with_path (APP_SCHEMA, path);

  full_app_id = g_settings_get_string (settings, "application-id");
  app_info = G_APP_INFO (g_desktop_app_info_new (full_app_id));

  if (app_info == NULL) {
    g_debug ("Not adding application '%s' (canonical app ID: %s)",
             full_app_id, canonical_app_id);
    /* The application cannot be found, probably it was uninstalled */
    g_object_unref (settings);
  } else {
    app = g_slice_new (Application);
    app->canonical_app_id = g_strdup (canonical_app_id);
    app->settings = settings;
    app->app_info = app_info;

    g_debug ("Adding application '%s' (canonical app ID: %s)",
             full_app_id, canonical_app_id);

    add_application (panel, app);
  }

  g_free (path);
  g_free (full_app_id);
}

static gboolean
queued_app_info (gpointer data)
{
  Application *app;
  CcNotificationsPanel *panel;

  app = data;
  panel = app->panel;
  app->panel = NULL;

  if (g_cancellable_is_cancelled (panel->apps_load_cancellable) ||
      g_hash_table_contains (panel->known_applications,
                             app->canonical_app_id))
    {
      application_free (app);
      g_object_unref (panel);
      return FALSE;
    }

  g_debug ("Processing queued application %s", app->canonical_app_id);

  add_application (panel, app);
  g_object_unref (panel);

  return FALSE;
}

static char *
app_info_get_id (GAppInfo *app_info)
{
  const char *desktop_id;
  char *ret;
  const char *filename;
  int l;

  desktop_id = g_app_info_get_id (app_info);
  if (desktop_id != NULL)
    {
      ret = g_strdup (desktop_id);
    }
  else
    {
      filename = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app_info));
      ret = g_path_get_basename (filename);
    }

  if (G_UNLIKELY (g_str_has_suffix (ret, ".desktop") == FALSE))
    {
      g_free (ret);
      return NULL;
    }

  l = strlen (desktop_id);
  *(ret + l - strlen(".desktop")) = '\0';
  return ret;
}

static void
process_app_info (CcNotificationsPanel *panel,
                  GTask                *task,
                  GAppInfo             *app_info)
{
  Application *app;
  char *app_id;
  char *canonical_app_id;
  char *path;
  GSettings *settings;
  GSource *source;
  guint i;

  app_id = app_info_get_id (app_info);
  canonical_app_id = g_strcanon (app_id,
                                 "0123456789"
                                 "abcdefghijklmnopqrstuvwxyz"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "-",
                                 '-');
  for (i = 0; canonical_app_id[i] != '\0'; i++)
    canonical_app_id[i] = g_ascii_tolower (canonical_app_id[i]);

  path = g_strconcat (APP_PREFIX, canonical_app_id, "/", NULL);
  settings = g_settings_new_with_path (APP_SCHEMA, path);

  app = g_slice_new (Application);
  app->canonical_app_id = canonical_app_id;
  app->settings = settings;
  app->app_info = g_object_ref (app_info);
  app->panel = g_object_ref (panel);

  source = g_idle_source_new ();
  g_source_set_callback (source, queued_app_info, app, NULL);
  g_source_attach (source, g_task_get_context (task));

  g_free (path);
}

static void
load_apps_thread (GTask        *task,
                  gpointer      panel,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  GList *iter, *apps;

  apps = g_app_info_get_all ();

  for (iter = apps; iter && !g_cancellable_is_cancelled (cancellable); iter = iter->next)
    {
      GDesktopAppInfo *app;

      app = iter->data;
      if (g_desktop_app_info_get_boolean (app, "X-GNOME-UsesNotifications")) {
        process_app_info (panel, task, G_APP_INFO (app));
        g_debug ("Processing app '%s'", g_app_info_get_id (G_APP_INFO (app)));
      } else {
        g_debug ("Skipped app '%s', doesn't use notifications", g_app_info_get_id (G_APP_INFO (app)));
      }
    }

  g_list_free_full (apps, g_object_unref);
}

static void
load_apps_async (CcNotificationsPanel *panel)
{
  GTask *task;

  panel->apps_load_cancellable = g_cancellable_new ();
  task = g_task_new (panel, panel->apps_load_cancellable, NULL, NULL);
  g_task_run_in_thread (task, load_apps_thread);

  g_object_unref (task);
}

static void
children_changed (GSettings            *settings,
                  const char           *key,
                  CcNotificationsPanel *panel)
{
  int i;
  gchar **new_app_ids;

  g_settings_get (panel->master_settings,
                  "application-children",
                  "^as", &new_app_ids);
  for (i = 0; new_app_ids[i]; i++)
    maybe_add_app_id (panel, new_app_ids[i]);
  g_strfreev (new_app_ids);
}

static void
build_app_store (CcNotificationsPanel *panel)
{
  /* Build application entries for known applications */
  children_changed (panel->master_settings, NULL, panel);
  g_signal_connect (panel->master_settings, "changed::application-children",
                    G_CALLBACK (children_changed), panel);

  /* Scan applications that statically declare to show notifications */
  load_apps_async (panel);
}

static void
select_app (GtkListBox           *list_box,
            GtkListBoxRow        *row,
            CcNotificationsPanel *panel)
{
  Application *app;

  app = g_object_get_qdata (G_OBJECT (row), application_quark ());
  cc_build_edit_dialog (panel, app->app_info, app->settings, panel->master_settings, panel->perm_store);
}

static void
application_free (Application *app)
{
  g_free (app->canonical_app_id);
  g_object_unref (app->app_info);
  g_object_unref (app->settings);

  g_slice_free (Application, app);
}

static int
sort_apps (gconstpointer one,
           gconstpointer two,
           gpointer      user_data)
{
  Application *a1, *a2;

  a1 = g_object_get_qdata (G_OBJECT (one), application_quark ());
  a2 = g_object_get_qdata (G_OBJECT (two), application_quark ());

  return g_utf8_collate (g_app_info_get_name (a1->app_info),
                         g_app_info_get_name (a2->app_info));
}
