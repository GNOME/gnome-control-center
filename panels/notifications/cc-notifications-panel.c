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

#include <adwaita.h>
#include <gio/gdesktopappinfo.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "cc-app-notifications-page.h"
#include "cc-list-row.h"
#include "cc-notifications-panel.h"
#include "cc-notifications-resources.h"
#include "cc-ui-util.h"

#define MASTER_SCHEMA "org.gnome.desktop.notifications"
#define APP_SCHEMA MASTER_SCHEMA ".application"
#define APP_PREFIX "/org/gnome/desktop/notifications/application/"

struct _CcNotificationsPanel {
    CcPanel parent_instance;

    GtkListBox *app_listbox;
    AdwSwitchRow *lock_screen_row;
    AdwSwitchRow *dnd_row;

    GListStore *app_store;

    GSettings *master_settings;

    GCancellable *cancellable;

    GHashTable *known_applications;

    GDBusProxy *perm_store;
};

struct _CcNotificationsPanelClass {
    CcPanelClass parent;
};

static void build_app_store (CcNotificationsPanel *self);
static void select_app (CcNotificationsPanel *self, GtkListBoxRow *row);
static int sort_apps (gconstpointer one, gconstpointer two, gpointer user_data);
static GtkWidget *create_app_row (gpointer item, gpointer user_data);

CC_PANEL_REGISTER (CcNotificationsPanel, cc_notifications_panel);

static void
cc_notifications_panel_dispose (GObject *object)
{
    CcNotificationsPanel *self = CC_NOTIFICATIONS_PANEL (object);

    g_clear_object (&self->master_settings);
    g_clear_object (&self->app_store);
    g_clear_pointer (&self->known_applications, g_hash_table_unref);

    G_OBJECT_CLASS (cc_notifications_panel_parent_class)->dispose (object);
}

static void
cc_notifications_panel_finalize (GObject *object)
{
    CcNotificationsPanel *self = CC_NOTIFICATIONS_PANEL (object);

    g_clear_object (&self->perm_store);

    G_OBJECT_CLASS (cc_notifications_panel_parent_class)->finalize (object);
}

static void
on_perm_store_ready (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    CcNotificationsPanel *self;
    GDBusProxy *proxy;
    g_autoptr(GError) error = NULL;

    proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
    if (proxy == NULL) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("Failed to connect to xdg-app permission store: %s", error->message);
        return;
    }
    self = user_data;
    self->perm_store = proxy;
}

static void
cc_notifications_panel_init (CcNotificationsPanel *self)
{
    g_resources_register (cc_notifications_get_resource ());

    gtk_widget_init_template (GTK_WIDGET (self));

    self->known_applications = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

    self->master_settings = g_settings_new (MASTER_SCHEMA);

    g_settings_bind (self->master_settings, "show-banners", self->dnd_row, "active", G_SETTINGS_BIND_INVERT_BOOLEAN);
    g_settings_bind (self->master_settings, "show-in-lock-screen", self->lock_screen_row, "active",
                     G_SETTINGS_BIND_DEFAULT);

    self->app_store = g_list_store_new (G_TYPE_APP_INFO);
    gtk_list_box_bind_model (self->app_listbox, G_LIST_MODEL (self->app_store), create_app_row, self, NULL);

    build_app_store (self);

    g_dbus_proxy_new_for_bus (
        G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.freedesktop.impl.portal.PermissionStore",
        "/org/freedesktop/impl/portal/PermissionStore", "org.freedesktop.impl.portal.PermissionStore",
        cc_panel_get_cancellable (CC_PANEL (self)), on_perm_store_ready, self);
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
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
    CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

    panel_class->get_help_uri = cc_notifications_panel_get_help_uri;

    /* Separate dispose() and finalize() functions are necessary
     * to make sure we cancel the running thread before the panel
     * gets finalized */
    object_class->dispose = cc_notifications_panel_dispose;
    object_class->finalize = cc_notifications_panel_finalize;

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/control-center/notifications/cc-notifications-panel.ui");

    gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, app_listbox);
    gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, lock_screen_row);
    gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, dnd_row);

    gtk_widget_class_bind_template_callback (widget_class, cc_util_keynav_propagate_vertical);
    gtk_widget_class_bind_template_callback (widget_class, select_app);
}

static inline GQuark
application_quark (void)
{
    static GQuark quark;

    if (G_UNLIKELY (quark == 0))
        quark = g_quark_from_static_string ("cc-application");

    return quark;
}

static GSettings *
get_app_settings (GAppInfo *app_info)
{
    const char *app_id;
    g_autofree gchar *path = NULL;

    app_id = g_object_get_qdata (G_OBJECT (app_info), application_quark ());
    path = g_strconcat (APP_PREFIX, app_id, "/", NULL);

    return g_settings_new_with_path (APP_SCHEMA, path);
}

static gboolean
on_off_label_mapping_get (GValue *value, GVariant *variant, gpointer user_data)
{
    g_value_set_string (value, g_variant_get_boolean (variant) ? _("On") : _("Off"));

    return TRUE;
}

static GtkWidget *
create_app_row (gpointer item, gpointer user_data)
{
    GAppInfo *app_info = item;
    CcListRow *row;
    GtkWidget *w;
    g_autoptr(GIcon) icon = NULL;
    g_autoptr(GSettings) settings = NULL;
    g_autofree gchar *escaped_app_name = NULL;

    escaped_app_name = g_markup_escape_text (g_app_info_get_name (app_info), -1);

    icon = g_app_info_get_icon (app_info);
    if (icon == NULL)
        icon = g_themed_icon_new ("application-x-executable");
    else
        g_object_ref (icon);

    row = g_object_new (CC_TYPE_LIST_ROW, "show-arrow", TRUE, NULL);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), escaped_app_name);

    w = gtk_image_new_from_gicon (icon);
    gtk_widget_add_css_class (w, "lowres-icon");
    gtk_image_set_icon_size (GTK_IMAGE (w), GTK_ICON_SIZE_LARGE);
    adw_action_row_add_prefix (ADW_ACTION_ROW (row), w);

    settings = get_app_settings (app_info);
    g_settings_bind_with_mapping (settings, "enable", row, "secondary-label",
                                  G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY, on_off_label_mapping_get, NULL,
                                  NULL, NULL);

    return GTK_WIDGET (row);
}

static void
add_application (CcNotificationsPanel *self, GAppInfo *app_info, const char *app_id)
{
    const gchar *app_name;

    app_name = g_app_info_get_name (app_info);
    if (app_name == NULL || *app_name == '\0')
        return;

    g_object_set_qdata_full (G_OBJECT (app_info), application_quark (), g_strdup (app_id), g_free);

    g_list_store_insert_sorted (self->app_store, app_info, sort_apps, NULL);

    g_hash_table_add (self->known_applications, g_strdup (app_id));
}

static gboolean
app_is_system_service (GDesktopAppInfo *app)
{
    g_auto(GStrv) split = NULL;
    const gchar *categories;

    categories = g_desktop_app_info_get_categories (app);
    if (categories == NULL || categories[0] == '\0')
        return FALSE;

    split = g_strsplit (categories, ";", -1);
    if (g_strv_contains ((const gchar *const *) split, "X-GNOME-Settings-Panel")
        || g_strv_contains ((const gchar *const *) split, "Settings")
        || g_strv_contains ((const gchar *const *) split, "System")) {
        return TRUE;
    }

    return FALSE;
}

static void
maybe_add_app_id (CcNotificationsPanel *self, const char *canonical_app_id)
{
    g_autofree gchar *path = NULL;
    g_autofree gchar *full_app_id = NULL;
    g_autoptr(GSettings) settings = NULL;
    g_autoptr(GAppInfo) app_info = NULL;

    if (*canonical_app_id == '\0')
        return;

    if (g_hash_table_contains (self->known_applications, canonical_app_id))
        return;

    path = g_strconcat (APP_PREFIX, canonical_app_id, "/", NULL);
    settings = g_settings_new_with_path (APP_SCHEMA, path);

    full_app_id = g_settings_get_string (settings, "application-id");
    app_info = G_APP_INFO (g_desktop_app_info_new (full_app_id));

    if (app_info == NULL) {
        g_debug ("Not adding application '%s' (canonical app ID: %s)", full_app_id, canonical_app_id);
        /* The application cannot be found, probably it was uninstalled */
        return;
    }

    if (!g_desktop_app_info_get_show_in (G_DESKTOP_APP_INFO (app_info), NULL)) {
        g_debug ("Not adding application '%s' (canonical app ID: %s), not shown in current desktop", full_app_id,
                 canonical_app_id);
        return;
    }

    if (app_is_system_service (G_DESKTOP_APP_INFO (app_info))) {
        /* We don't want to show system services in the notification list */
        return;
    }

    g_debug ("Adding application '%s' (canonical app ID: %s)", full_app_id, canonical_app_id);

    add_application (self, app_info, canonical_app_id);
}

static char *
app_info_get_id (GAppInfo *app_info)
{
    const char *desktop_id;
    g_autofree gchar *ret = NULL;
    const char *filename;
    int l;

    desktop_id = g_app_info_get_id (app_info);
    if (desktop_id != NULL) {
        ret = g_strdup (desktop_id);
    } else {
        filename = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app_info));
        ret = g_path_get_basename (filename);
    }

    if (G_UNLIKELY (g_str_has_suffix (ret, ".desktop") == FALSE))
        return NULL;

    l = strlen (ret);
    *(ret + l - strlen (".desktop")) = '\0';
    return g_steal_pointer (&ret);
}

static void
process_app_info (CcNotificationsPanel *self, GAppInfo *app_info)
{
    g_autofree gchar *app_id = NULL;
    guint i;

    app_id = app_info_get_id (app_info);
    g_strcanon (app_id,
                "0123456789"
                "abcdefghijklmnopqrstuvwxyz"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "-",
                '-');
    for (i = 0; app_id[i] != '\0'; i++)
        app_id[i] = g_ascii_tolower (app_id[i]);

    if (g_hash_table_contains (self->known_applications, app_id))
        return;

    g_debug ("Processing queued application %s", app_id);

    add_application (self, app_info, app_id);
}

static void
load_apps (CcNotificationsPanel *self)
{
    GList *iter, *apps;

    apps = g_app_info_get_all ();

    for (iter = apps; iter; iter = iter->next) {
        GDesktopAppInfo *app;

        app = iter->data;
        if (g_desktop_app_info_get_boolean (app, "X-GNOME-UsesNotifications")) {
            if (!g_desktop_app_info_get_show_in (app, NULL)) {
                g_debug ("Skipped app '%s', not shown in current desktop (NotShowIn/OnlyShowIn)",
                         g_app_info_get_id (G_APP_INFO (app)));
                continue;
            }

            if (app_is_system_service (app)) {
                g_debug ("Skipped app '%s', as it is a system service", g_app_info_get_id (G_APP_INFO (app)));
                continue;
            }

            process_app_info (self, G_APP_INFO (app));
            g_debug ("Processing app '%s'", g_app_info_get_id (G_APP_INFO (app)));
        } else {
            g_debug ("Skipped app '%s', doesn't use notifications", g_app_info_get_id (G_APP_INFO (app)));
        }
    }

    g_list_free_full (apps, g_object_unref);
}

static void
children_changed (CcNotificationsPanel *self, const char *key)
{
    int i;
    g_auto(GStrv) new_app_ids = NULL;

    g_settings_get (self->master_settings, "application-children", "^as", &new_app_ids);
    for (i = 0; new_app_ids[i]; i++)
        maybe_add_app_id (self, new_app_ids[i]);
}

static void
build_app_store (CcNotificationsPanel *self)
{
    /* Build application entries for known applications */
    children_changed (self, NULL);
    g_signal_connect_object (self->master_settings, "changed::application-children", G_CALLBACK (children_changed),
                             self, G_CONNECT_SWAPPED);

    /* Scan applications that statically declare to show notifications */
    load_apps (self);
}

static void
select_app (CcNotificationsPanel *self, GtkListBoxRow *row)
{
    g_autoptr(GAppInfo) app_info = NULL;
    g_autoptr(GSettings) settings = NULL;
    g_autofree gchar *app_id = NULL;
    CcAppNotificationsPage *page;

    app_info = g_list_model_get_item (G_LIST_MODEL (self->app_store), gtk_list_box_row_get_index (row));
    if (app_info == NULL)
        return;

    app_id = g_strdup (g_app_info_get_id (app_info));
    if (g_str_has_suffix (app_id, ".desktop"))
        app_id[strlen (app_id) - strlen (".desktop")] = '\0';

    settings = get_app_settings (app_info);

    page = cc_app_notifications_page_new (app_id, g_app_info_get_name (app_info), settings, self->master_settings,
                                          self->perm_store);
    cc_panel_push_subpage (CC_PANEL (self), ADW_NAVIGATION_PAGE (page));
}

static int
sort_apps (gconstpointer one, gconstpointer two, gpointer user_data)
{
    GAppInfo *a1 = (GAppInfo *) one;
    GAppInfo *a2 = (GAppInfo *) two;

    return g_utf8_collate (g_app_info_get_name (a1), g_app_info_get_name (a2));
}
