/* cc-applications-panel.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-applications-panel"

#include <config.h>
#include <glib/gi18n.h>
#ifdef HAVE_SNAP
#include <snapd-glib/snapd-glib.h>
#endif
#ifdef HAVE_MALCONTENT
#include <libmalcontent/malcontent.h>
#endif

#include <gio/gdesktopappinfo.h>

#include "cc-applications-panel.h"
#include "cc-applications-row.h"
#include "cc-toggle-row.h"
#include "cc-info-row.h"
#include "cc-action-row.h"
#include "cc-applications-resources.h"
#include "cc-util.h"
#ifdef HAVE_SNAP
#include "cc-snap-row.h"
#endif
#include "globs.h"
#include "list-box-helper.h"
#include "search.h"
#include "utils.h"

#define MASTER_SCHEMA "org.gnome.desktop.notifications"
#define APP_SCHEMA MASTER_SCHEMA ".application"
#define APP_PREFIX "/org/gnome/desktop/notifications/application/"

#define PORTAL_SNAP_PREFIX "snap."

struct _CcApplicationsPanel
{
  CcPanel          parent;

  GtkBox          *sidebar_box;
  GtkListBox      *sidebar_listbox;
  GtkEntry        *sidebar_search_entry;
  GtkButton       *header_button;
  GtkLabel        *title_label;
  GAppInfoMonitor *monitor;
  gulong           monitor_id;
#ifdef HAVE_MALCONTENT
  GCancellable    *cancellable;

  MctAppFilter    *app_filter;
  MctManager      *manager;
  guint            app_filter_id;
#endif

  gchar           *current_app_id;
  gchar           *current_portal_app_id;

  GHashTable      *globs;
  GHashTable      *search_providers;

  GDBusProxy      *perm_store;
  GSettings       *notification_settings;
  GSettings       *location_settings;
  GSettings       *privacy_settings;
  GSettings       *search_settings;

  GtkStack        *stack;
  GtkBox          *empty_box;
  GtkBox          *settings_box;

  GtkBox          *permission_section;
  GtkListBox      *permission_list;
  CcToggleRow     *camera;
  CcInfoRow       *no_camera;
  CcToggleRow     *location;
  CcInfoRow       *no_location;
  CcToggleRow     *shortcuts;
  CcToggleRow     *microphone;
  CcInfoRow       *no_microphone;
  CcInfoRow       *builtin;
  GtkDialog       *builtin_dialog;
  GtkLabel        *builtin_label;
  GtkListBox      *builtin_list;

  GtkBox          *integration_section;
  GtkListBox      *integration_list;
  CcToggleRow     *notification;
  CcToggleRow     *background;
  CcToggleRow     *wallpaper;
  CcToggleRow     *sound;
  CcInfoRow       *no_sound;
  CcToggleRow     *search;
  CcInfoRow       *no_search;

  GtkBox          *handler_section;
  GtkButton       *handler_reset;
  GtkListBox      *handler_list;
  CcInfoRow       *hypertext;
  CcInfoRow       *text;
  CcInfoRow       *images;
  CcInfoRow       *fonts;
  CcInfoRow       *archives;
  CcInfoRow       *packages;
  CcInfoRow       *audio;
  CcInfoRow       *video;
  CcInfoRow       *other;
  CcInfoRow       *link;

  GtkBox          *usage_section;
  GtkListBox      *usage_list;
  CcInfoRow       *storage;
  GtkDialog       *storage_dialog;
  GtkListBox      *storage_list;
  CcInfoRow       *app;
  CcInfoRow       *data;
  CcInfoRow       *cache;
  CcInfoRow       *total;
  GtkButton       *clear_cache_button;

  guint64          app_size;
  guint64          cache_size;
  guint64          data_size;
};

static void select_app (CcApplicationsPanel *self,
                        const gchar         *app_id);

G_DEFINE_TYPE (CcApplicationsPanel, cc_applications_panel, CC_TYPE_PANEL)

enum
{
  PROP_0,
  PROP_PARAMETERS
};

/* Callbacks */

static gboolean
privacy_link_cb (CcApplicationsPanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));
  g_autoptr(GError) error = NULL;

  if (!cc_shell_set_active_panel_from_id (shell, "location", NULL, &error))
    g_warning ("Failed to switch to privacy panel: %s", error->message);

  return TRUE;
}

static void
open_software_cb (CcApplicationsPanel *self)
{
  const gchar *argv[] = { "gnome-software", "--details", "appid", NULL };

  if (self->current_app_id == NULL)
    argv[1] = NULL;
  else
    argv[2] = self->current_app_id;

  g_spawn_async (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

/* --- portal permissions and utilities --- */

static gchar **
get_portal_permissions (CcApplicationsPanel *self,
                        const gchar         *table,
                        const gchar         *id,
                        const gchar         *app_id)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  const gchar *key = NULL;
  GStrv val;
  GStrv result = NULL;

  ret = g_dbus_proxy_call_sync (self->perm_store,
                                "Lookup",
                                g_variant_new ("(ss)", table, id),
                                0, G_MAXINT, NULL, NULL);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(a{sas}v)", &iter, NULL);

  while (g_variant_iter_loop (iter, "{&s^a&s}", &key, &val))
    {
      if (strcmp (key, app_id) == 0 && result == NULL)
        result = g_strdupv (val);
    }

  return result;
}

static void
set_portal_permissions (CcApplicationsPanel *self,
                        const gchar *table,
                        const gchar *id,
                        const gchar *app_id,
                        const gchar * const *permissions)
{
  g_autoptr(GError) error = NULL;

  g_dbus_proxy_call_sync (self->perm_store,
                          "SetPermission",
                          g_variant_new ("(sbss^as)", table, TRUE, id, app_id, permissions),
                          0,
                          G_MAXINT,
                          NULL,
                          &error);
  if (error)
    g_warning ("Error setting portal permissions: %s", error->message);
}

static gchar *
get_portal_app_id (GAppInfo *info)
{
  if (G_IS_DESKTOP_APP_INFO (info))
    {
      g_autofree gchar *snap_name = NULL;
      gchar *flatpak_id;

      flatpak_id = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (info), "X-Flatpak");
      if (flatpak_id != NULL)
        return flatpak_id;

      snap_name = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (info), "X-SnapInstanceName");
      if (snap_name != NULL)
        return g_strdup_printf ("%s%s", PORTAL_SNAP_PREFIX, snap_name);
    }

  return NULL;
}

static GFile *
get_flatpak_app_dir (const gchar *app_id,
                     const gchar *subdir)
{
  g_autofree gchar *path = NULL;
  g_autoptr(GFile) appdir = NULL;

  path = g_build_filename (g_get_home_dir (), ".var", "app", app_id, NULL);
  appdir = g_file_new_for_path (path);

  return g_file_get_child (appdir, subdir);
}

/* --- search settings --- */

static void
set_search_enabled (CcApplicationsPanel *self,
                    const gchar         *app_id,
                    gboolean             enabled)
{
  g_autoptr(GPtrArray) new_apps = NULL;
  g_autofree gchar *desktop_id = NULL;
  g_auto(GStrv) apps = NULL;
  gpointer key, value;
  gboolean default_disabled;
  gint i;

  desktop_id = g_strconcat (app_id, ".desktop", NULL);

  if (!g_hash_table_lookup_extended (self->search_providers, app_id, &key, &value))
    {
      g_warning ("Trying to configure search for a provider-less app - this shouldn't happen");
      return;
    }

  default_disabled = GPOINTER_TO_INT (value);

  new_apps = g_ptr_array_new_with_free_func (g_free);
  if (default_disabled)
    {
      apps = g_settings_get_strv (self->search_settings, "enabled");
      for (i = 0; apps[i]; i++)
        {
          if (strcmp (apps[i], desktop_id) != 0)
            g_ptr_array_add (new_apps, g_strdup (apps[i]));
        }
      if (enabled)
        g_ptr_array_add (new_apps, g_strdup (desktop_id));
      g_ptr_array_add (new_apps, NULL);
      g_settings_set_strv (self->search_settings, "enabled",  (const gchar * const *)new_apps->pdata);
    }
  else
    {
      apps = g_settings_get_strv (self->search_settings, "disabled");
      for (i = 0; apps[i]; i++)
        {
          if (strcmp (apps[i], desktop_id) != 0)
            g_ptr_array_add (new_apps, g_strdup (apps[i]));
        }
      if (!enabled)
        g_ptr_array_add (new_apps, g_strdup (desktop_id));
      g_ptr_array_add (new_apps, NULL);
      g_settings_set_strv (self->search_settings, "disabled", (const gchar * const *)new_apps->pdata);
    }
}

static gboolean
search_contains_string_for_app (CcApplicationsPanel *self,
                                const gchar         *app_id,
                                const gchar         *setting)
{
  g_autofree gchar *desktop_id = NULL;
  g_auto(GStrv) apps = NULL;

  desktop_id = g_strconcat (app_id, ".desktop", NULL);
  apps = g_settings_get_strv (self->search_settings, setting);

  return g_strv_contains ((const gchar * const *)apps, desktop_id);
}

static gboolean
search_enabled_for_app (CcApplicationsPanel *self,
                        const gchar         *app_id)
{
  return search_contains_string_for_app (self, app_id, "enabled");
}

static gboolean
search_disabled_for_app (CcApplicationsPanel *self,
                         const gchar         *app_id)
{
  return search_contains_string_for_app (self, app_id, "disabled");
}

static void
get_search_enabled (CcApplicationsPanel *self,
                    const gchar         *app_id,
                    gboolean            *set,
                    gboolean            *enabled)
{
  gpointer key, value;

  *enabled = FALSE;
  *set = g_hash_table_lookup_extended (self->search_providers, app_id, &key, &value);
  if (!*set)
    return;

  if (search_enabled_for_app (self, app_id))
    *enabled = TRUE;
  else if (search_disabled_for_app (self, app_id))
    *enabled = FALSE;
  else
    *enabled = !GPOINTER_TO_INT (value);
}

static void
search_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_search_enabled (self,
                        self->current_app_id,
                        cc_toggle_row_get_allowed (self->search));
}

/* --- notification permissions (flatpaks and non-flatpak) --- */

static void
get_notification_allowed (CcApplicationsPanel *self,
                          const gchar         *app_id,
                          gboolean            *set,
                          gboolean            *allowed)
{
  if (self->notification_settings)
    {
      /* FIXME */
      *set = TRUE;
      *allowed = g_settings_get_boolean (self->notification_settings, "enable");
    }
  else
    {
      g_auto(GStrv) perms = get_portal_permissions (self, "notifications", "notification", app_id);
      *set = perms != NULL;
      /* FIXME: needs unreleased xdg-desktop-portals to write permissions on use */
      *set = TRUE;
      *allowed = perms == NULL || strcmp (perms[0], "no") != 0;
    }
}

static void
set_notification_allowed (CcApplicationsPanel *self,
                          gboolean             allowed)
{
  if (self->notification_settings)
    {
      g_settings_set_boolean (self->notification_settings, "enable", allowed);
    }
  else
    {
      const gchar *perms[2] = { NULL, NULL };

      perms[0] = allowed ? "yes" : "no";
      set_portal_permissions (self, "notifications", "notification", self->current_portal_app_id, perms);
    }
}

static void
notification_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_notification_allowed (self, cc_toggle_row_get_allowed (self->notification));
}

static gchar *
munge_app_id (const gchar *app_id)
{
  gchar *id = g_strdup (app_id);
  gint i;

  g_strcanon (id,
              "0123456789"
              "abcdefghijklmnopqrstuvwxyz"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "-",
              '-');
  for (i = 0; id[i] != '\0'; i++)
    id[i] = g_ascii_tolower (id[i]);

  return id;
}

static GSettings *
get_notification_settings (const gchar *app_id)
{
  g_autofree gchar *munged_app_id = munge_app_id (app_id);
  g_autofree gchar *path = g_strconcat (APP_PREFIX, munged_app_id, "/", NULL);
  return g_settings_new_with_path (APP_SCHEMA, path);
}


/* --- background --- */

static void
get_background_allowed (CcApplicationsPanel *self,
                        const gchar         *app_id,
                        gboolean            *set,
                        gboolean            *allowed)
{
  g_auto(GStrv) perms = get_portal_permissions (self, "background", "background", app_id);
  *set = TRUE;
  *allowed = perms == NULL || strcmp (perms[0], "no") != 0;
}

static void
set_background_allowed (CcApplicationsPanel *self,
                        gboolean             allowed)
{
  const gchar *perms[2] = { NULL, NULL };

  perms[0] = allowed ? "yes" : "no";
  set_portal_permissions (self, "background", "background", self->current_portal_app_id, perms);
}

static void
background_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_background_allowed (self, cc_toggle_row_get_allowed (self->background));
}

/* --- wallpaper --- */

static void
get_wallpaper_allowed (CcApplicationsPanel *self,
                       const gchar         *app_id,
                       gboolean            *set,
                       gboolean            *allowed)
{
  g_auto(GStrv) perms = get_portal_permissions (self, "wallpaper", "wallpaper", app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || strcmp (perms[0], "no") != 0;
}

static void
set_wallpaper_allowed (CcApplicationsPanel *self,
                       gboolean             allowed)
{
  const gchar *perms[2] = { NULL, NULL };

  perms[0] = allowed ? "yes" : "no";
  set_portal_permissions (self, "wallpaper", "wallpaper", self->current_app_id, perms);
}

static void
wallpaper_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_wallpaper_allowed (self, cc_toggle_row_get_allowed (self->wallpaper));
}

/* --- shortcuts permissions (flatpak) --- */

static void
get_shortcuts_allowed (CcApplicationsPanel *self,
                       const gchar         *app_id,
                       gboolean            *set,
                       gboolean            *granted)
{
  g_auto(GStrv) perms = NULL;

  perms = get_portal_permissions (self, "gnome", "shortcuts-inhibitor", app_id);

  /* GNOME Shell's "inhibit shortcut dialog" sets the permission to "GRANTED" if
   * the user allowed for the keyboard shortcuts to be inhibited, check for that
   * string value here.
   */
  *set = perms != NULL;
  *granted = (perms != NULL) && g_ascii_strcasecmp (perms[0], "GRANTED") == 0;
}

static void
set_shortcuts_allowed (CcApplicationsPanel *self,
                       gboolean             granted)
{
  const gchar *perms[2];
  g_autofree gchar *desktop_id = g_strconcat (self->current_app_id, ".desktop", NULL);

  /* "GRANTED" and "DENIED" here match the values set by the "inhibit shortcut
   * dialog" is GNOME Shell:
   * https://gitlab.gnome.org/GNOME/gnome-shell/-/blob/master/js/ui/inhibitShortcutsDialog.js
   */
  perms[0] = granted ? "GRANTED" : "DENIED";
  perms[1] = NULL;

  set_portal_permissions (self, "gnome", "shortcuts-inhibitor", desktop_id, perms);
}

static void
shortcuts_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_shortcuts_allowed (self, cc_toggle_row_get_allowed (self->shortcuts));
}

/* --- device (microphone, camera, speaker) permissions (flatpak) --- */

static void
get_device_allowed (CcApplicationsPanel *self,
                    const gchar         *device,
                    const gchar         *app_id,
                    gboolean            *set,
                    gboolean            *allowed)
{
  g_auto(GStrv) perms = NULL;

  perms = get_portal_permissions (self, "devices", device, app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || strcmp (perms[0], "no") != 0;
}

static void
set_device_allowed (CcApplicationsPanel *self,
                    const gchar         *device,
                    gboolean             allowed)
{
  const gchar *perms[2];

  perms[0] = allowed ? "yes" : "no";
  perms[1] = NULL;

  set_portal_permissions (self, "devices", device, self->current_portal_app_id, perms);
}

static void
microphone_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
    set_device_allowed (self, "microphone", cc_toggle_row_get_allowed (self->microphone));
}

static void
sound_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
   set_device_allowed (self, "speakers", cc_toggle_row_get_allowed (self->sound));
}

static void
camera_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
    set_device_allowed (self, "camera", cc_toggle_row_get_allowed (self->camera));
}

/* --- location permissions (flatpak) --- */

static void
get_location_allowed (CcApplicationsPanel *self,
                      const gchar         *app_id,
                      gboolean            *set,
                      gboolean            *allowed)
{
  g_auto(GStrv) perms = NULL;

  perms = get_portal_permissions (self, "location", "location", app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || strcmp (perms[0], "NONE") != 0;
}

static void
set_location_allowed (CcApplicationsPanel *self,
                      gboolean             allowed)
{
  const gchar *perms[3];

  /* FIXME allow setting accuracy */
  perms[0] = allowed ? "EXACT" : "NONE";
  perms[1] = "0";
  perms[2] = NULL;

  set_portal_permissions (self, "location", "location", self->current_portal_app_id, perms);
}

static void
location_cb (CcApplicationsPanel *self)
{
  if (self->current_portal_app_id)
    set_location_allowed (self, cc_toggle_row_get_allowed (self->location));
}

/* --- permissions section --- */

#ifdef HAVE_SNAP
static void
remove_snap_permissions (CcApplicationsPanel *self)
{
  g_autoptr(GList) rows = NULL;
  GList *link;

  rows = gtk_container_get_children (GTK_CONTAINER (self->permission_list));
  for (link = rows; link; link = link->next)
    {
      GtkWidget *row = link->data;

      if (row == GTK_WIDGET (self->builtin))
        break;

      if (CC_IS_SNAP_ROW (row))
        gtk_container_remove (GTK_CONTAINER (self->permission_list), GTK_WIDGET (row));
    }
}

static gboolean
add_snap_permissions (CcApplicationsPanel *self,
                      GAppInfo            *info,
                      const gchar         *app_id)
{
  const gchar *snap_name;
  g_autoptr(GList) rows = NULL;
  gint index;
  g_autoptr(SnapdClient) client = NULL;
  g_autoptr(GPtrArray) interfaces = NULL;
  g_autoptr(GPtrArray) plugs = NULL;
  g_autoptr(GPtrArray) slots = NULL;
  SnapdInterface *interface = NULL;
  gint added = 0;
  g_autoptr(GError) error = NULL;
  g_autoptr(GError) interfaces_error = NULL;

  if (!g_str_has_prefix (app_id, PORTAL_SNAP_PREFIX))
    return FALSE;
  snap_name = app_id + strlen (PORTAL_SNAP_PREFIX);

  rows = gtk_container_get_children (GTK_CONTAINER (self->permission_list));
  index = g_list_index (rows, self->builtin);
  g_assert (index >= 0);

  client = snapd_client_new ();

  interfaces = snapd_client_get_interfaces2_sync (client,
                                                  SNAPD_GET_INTERFACES_FLAGS_NONE,
                                                  NULL,
                                                  NULL, &interfaces_error);
  if (interfaces == NULL)
    g_warning ("Failed to get snap interfaces: %s", interfaces_error->message);

  if (!snapd_client_get_connections2_sync (client,
                                           SNAPD_GET_CONNECTIONS_FLAGS_SELECT_ALL,
                                           NULL, NULL,
                                           NULL, NULL,
                                           &plugs, &slots,
                                           NULL, &error))
    {
      g_warning ("Failed to get snap connections: %s", error->message);
      return FALSE;
    }

  for (int i = 0; i < plugs->len; i++)
    {
      SnapdPlug *plug = g_ptr_array_index (plugs, i);
      CcSnapRow *row;
      g_autoptr(GPtrArray) available_slots = NULL;
      const gchar * const hidden_interfaces[] = { "content",
                                                  "desktop", "desktop-legacy",
                                                  "mir",
                                                  "unity7", "unity8",
                                                  "wayland",
                                                  "x11",
                                                  NULL };

      /* Skip if not relating to this snap */
      if (g_strcmp0 (snapd_plug_get_snap (plug), snap_name) != 0)
        continue;

      /* Ignore interfaces that are too low level to make sense to show or disable */
      if (g_strv_contains (hidden_interfaces, snapd_plug_get_interface (plug)))
        continue;

      available_slots = g_ptr_array_new_with_free_func (g_object_unref);
      for (int j = 0; j < slots->len; j++)
        {
          SnapdSlot *slot = g_ptr_array_index (slots, j);
          if (g_strcmp0 (snapd_plug_get_interface (plug), snapd_slot_get_interface (slot)) != 0)
            continue;

          g_ptr_array_add (available_slots, g_object_ref (slot));
        }

      if (interfaces != NULL)
        {
          for (int j = 0; j < interfaces->len; j++)
            {
              SnapdInterface *i = g_ptr_array_index (interfaces, j);
              if (g_strcmp0 (snapd_interface_get_name (i), snapd_plug_get_interface (plug)) == 0)
                interface = i;
            }
        }

      row = cc_snap_row_new (cc_panel_get_cancellable (CC_PANEL (self)), interface, plug, available_slots);
      gtk_widget_show (GTK_WIDGET (row));
      gtk_list_box_insert (GTK_LIST_BOX (self->permission_list), GTK_WIDGET (row), index);
      index++;
      added++;
    }

    return added > 0;
}
#endif

static gint
add_static_permission_row (CcApplicationsPanel *self,
                           const gchar         *title,
                           const gchar         *subtitle)
{
  GtkWidget *row;

  row = g_object_new (CC_TYPE_INFO_ROW,
                      "title", title,
                      "info", subtitle,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self->builtin_list), row);

  return 1;
}

static void
permission_row_activated_cb (CcApplicationsPanel *self,
                             GtkListBoxRow       *list_row)
{
  if (list_row == GTK_LIST_BOX_ROW (self->builtin))
    {
      gtk_window_set_transient_for (GTK_WINDOW (self->builtin_dialog),
                                    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
      gtk_window_present (GTK_WINDOW (self->builtin_dialog));
    }
}

static gboolean
add_static_permissions (CcApplicationsPanel *self,
                        GAppInfo            *info,
                        const gchar         *app_id)
{
  g_autoptr(GKeyFile) keyfile = NULL;
  g_auto(GStrv) sockets = NULL;
  g_auto(GStrv) devices = NULL;
  g_auto(GStrv) shared = NULL;
  g_auto(GStrv) filesystems = NULL;
  g_autofree gchar *str = NULL;
  gint added = 0;
  g_autofree gchar *text = NULL;

  if (!g_str_has_prefix (app_id, PORTAL_SNAP_PREFIX))
    keyfile = get_flatpak_metadata (app_id);
  if (keyfile == NULL)
    return FALSE;

  sockets = g_key_file_get_string_list (keyfile, "Context", "sockets", NULL, NULL);
  if (sockets && g_strv_contains ((const gchar * const*)sockets, "system-bus"))
    added += add_static_permission_row (self, _("System Bus"), _("Full access"));
  if (sockets && g_strv_contains ((const gchar * const*)sockets, "session-bus"))
    added += add_static_permission_row (self, _("Session Bus"), _("Full access"));

  devices = g_key_file_get_string_list (keyfile, "Context", "devices", NULL, NULL);
  if (devices && g_strv_contains ((const gchar * const*)devices, "all"))
    added += add_static_permission_row (self, _("Devices"), _("Full access to /dev"));

  shared = g_key_file_get_string_list (keyfile, "Context", "shared", NULL, NULL);
  if (shared && g_strv_contains ((const gchar * const*)shared, "network"))
    added += add_static_permission_row (self, _("Network"), _("Has network access"));

  filesystems = g_key_file_get_string_list (keyfile, "Context", "filesystems", NULL, NULL);
  if (filesystems && (g_strv_contains ((const gchar * const *)filesystems, "home") ||
               g_strv_contains ((const gchar * const *)filesystems, "home:rw")))
    added += add_static_permission_row (self, _("Home"), _("Full access"));
  else if (filesystems && g_strv_contains ((const gchar * const *)filesystems, "home:ro"))
    added += add_static_permission_row (self, _("Home"), _("Read-only"));
  if (filesystems && (g_strv_contains ((const gchar * const *)filesystems, "host") ||
               g_strv_contains ((const gchar * const *)filesystems, "host:rw")))
    added += add_static_permission_row (self, _("File System"), _("Full access"));
  else if (filesystems && g_strv_contains ((const gchar * const *)filesystems, "host:ro"))
    added += add_static_permission_row (self, _("File System"), _("Read-only"));

  str = g_key_file_get_string (keyfile, "Session Bus Policy", "ca.desrt.dconf", NULL);
  if (str && g_str_equal (str, "talk"))
    added += add_static_permission_row (self, _("Settings"), _("Can change settings"));

  text = g_strdup_printf (_("%s has the following permissions built-in. These cannot be altered. If you are concerned about these permissions, consider removing this application."), g_app_info_get_display_name (info));
  gtk_label_set_label (self->builtin_label, text);

  return added > 0;
}

static void
remove_static_permissions (CcApplicationsPanel *self)
{
  container_remove_all (GTK_CONTAINER (self->builtin_list));
}

static void
update_permission_section (CcApplicationsPanel *self,
                           GAppInfo            *info)
{
  g_autofree gchar *portal_app_id = get_portal_app_id (info);
  gboolean disabled, allowed, set;
  gboolean has_any = FALSE, has_builtin = FALSE;

  if (portal_app_id == NULL)
    {
      gtk_widget_hide (GTK_WIDGET (self->permission_section));
      return;
    }

  disabled = g_settings_get_boolean (self->privacy_settings, "disable-camera");
  get_device_allowed (self, "camera", portal_app_id, &set, &allowed);
  cc_toggle_row_set_allowed (self->camera, allowed);
  gtk_widget_set_visible (GTK_WIDGET (self->camera), set && !disabled);
  gtk_widget_set_visible (GTK_WIDGET (self->no_camera), set && disabled);
  has_any |= set;

  disabled = g_settings_get_boolean (self->privacy_settings, "disable-microphone");
  get_device_allowed (self, "microphone", portal_app_id, &set, &allowed);
  cc_toggle_row_set_allowed (self->microphone, allowed);
  gtk_widget_set_visible (GTK_WIDGET (self->microphone), set && !disabled);
  gtk_widget_set_visible (GTK_WIDGET (self->no_microphone), set && disabled);
  has_any |= set;

  disabled = !g_settings_get_boolean (self->location_settings, "enabled");
  get_location_allowed (self, portal_app_id, &set, &allowed);
  cc_toggle_row_set_allowed (self->location, allowed);
  gtk_widget_set_visible (GTK_WIDGET (self->location), set && !disabled);
  gtk_widget_set_visible (GTK_WIDGET (self->no_location), set && disabled);
  has_any |= set;

#ifdef HAVE_SNAP
  remove_snap_permissions (self);
  has_any |= add_snap_permissions (self, info, portal_app_id);
#endif

  remove_static_permissions (self);
  has_builtin = add_static_permissions (self, info, portal_app_id);
  gtk_widget_set_visible (GTK_WIDGET (self->builtin), has_builtin);
  has_any |= has_builtin;

  gtk_widget_set_visible (GTK_WIDGET (self->permission_section), has_any);
}

/* --- gintegration section --- */

static void
update_integration_section (CcApplicationsPanel *self,
                            GAppInfo            *info)
{
  g_autofree gchar *app_id = get_app_id (info);
  g_autofree gchar *portal_app_id = get_portal_app_id (info);
  gboolean set, allowed, disabled;
  gboolean has_any = FALSE;

  disabled = g_settings_get_boolean (self->search_settings, "disable-external");
  get_search_enabled (self, app_id, &set, &allowed);
  cc_toggle_row_set_allowed (self->search, allowed);
  gtk_widget_set_visible (GTK_WIDGET (self->search), set && !disabled);
  gtk_widget_set_visible (GTK_WIDGET (self->no_search), set && disabled);

  if (app_id != NULL)
    {
      g_autofree gchar *desktop_id = g_strconcat (app_id, ".desktop", NULL);
      get_shortcuts_allowed (self, desktop_id, &set, &allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->shortcuts), set);
      cc_toggle_row_set_allowed (self->shortcuts, allowed);
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->shortcuts));
    }

  if (portal_app_id != NULL)
    {
      g_clear_object (&self->notification_settings);
      get_notification_allowed (self, portal_app_id, &set, &allowed);
      cc_toggle_row_set_allowed (self->notification, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->notification), set);
      has_any |= set;

      get_background_allowed (self, portal_app_id, &set, &allowed);
      cc_toggle_row_set_allowed (self->background, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->background), set);
      has_any |= set;

      get_wallpaper_allowed (self, portal_app_id, &set, &allowed);
      cc_toggle_row_set_allowed (self->wallpaper, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->wallpaper), set);
      has_any |= set;

      disabled = g_settings_get_boolean (self->privacy_settings, "disable-sound-output");
      get_device_allowed (self, "speakers", portal_app_id, &set, &allowed);
      cc_toggle_row_set_allowed (self->sound, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->sound), set && !disabled);
      gtk_widget_set_visible (GTK_WIDGET (self->no_sound), set && disabled);
    }
  else
    {
      g_set_object (&self->notification_settings, get_notification_settings (app_id));
      get_notification_allowed (self, app_id, &set, &allowed);
      cc_toggle_row_set_allowed (self->notification, allowed);
      gtk_widget_set_visible (GTK_WIDGET (self->notification), set);
      has_any |= set;

      gtk_widget_hide (GTK_WIDGET (self->background));
      gtk_widget_hide (GTK_WIDGET (self->wallpaper));
      gtk_widget_hide (GTK_WIDGET (self->sound));
      gtk_widget_hide (GTK_WIDGET (self->no_sound));
    }

  gtk_widget_set_visible (GTK_WIDGET (self->integration_section), has_any);
}

/* --- handler section --- */

static void
unset_cb (CcApplicationsPanel *self,
          CcActionRow         *row)
{
  const gchar *type;
  GtkListBoxRow *selected;
  GAppInfo *info;

  selected = gtk_list_box_get_selected_row (self->sidebar_listbox);
  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (selected));

  type = (const gchar *)g_object_get_data (G_OBJECT (row), "type");

  g_app_info_remove_supports_type (info, type, NULL);
}

static void
update_group_row_count (CcInfoRow *row,
                        gint       delta)
{
  gint count;
  g_autofree gchar *text = NULL;

  count = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "count"));
  count += delta;
  g_object_set_data (G_OBJECT (row), "count", GINT_TO_POINTER (count));
  text = g_strdup_printf ("%d", count);
  g_object_set (row, "info", text, NULL);
}

static void
add_scheme (CcApplicationsPanel *self,
            CcInfoRow           *after,
            const gchar         *type)
{
  CcActionRow *row = NULL;
  gint pos;

  if (g_str_has_suffix (type, "http"))
    {
      row = cc_action_row_new ();
      cc_action_row_set_title (row, _("Web Links"));
      cc_action_row_set_subtitle (row, "http://, https://");
    }
  else if (g_str_has_suffix (type, "https"))
    {
      return; /* assume anything that handles https also handles http */
    }
  else if (g_str_has_suffix (type, "git"))
    {
      row = cc_action_row_new ();
      cc_action_row_set_title (row, _("Git Links"));
      cc_action_row_set_subtitle (row, "git://");
    }
  else
    {
      gchar *scheme = strrchr (type, '/') + 1;
      g_autofree gchar *title = g_strdup_printf (_("%s Links"), scheme);
      g_autofree gchar *subtitle = g_strdup_printf ("%s://", scheme);

      row = cc_action_row_new ();
      cc_action_row_set_title (row, title);
      cc_action_row_set_subtitle (row, subtitle);
    }

  cc_action_row_set_action (row, _("Unset"), TRUE);
  g_object_set_data_full (G_OBJECT (row), "type", g_strdup (type), g_free);
  g_signal_connect_object (row,
                           "activated",
                           G_CALLBACK (unset_cb),
                           self, G_CONNECT_SWAPPED);

  if (after)
    {
      pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (after)) + 1;
      g_object_bind_property (after, "expanded",
                              row, "visible",
                              G_BINDING_SYNC_CREATE);
    }
  else
    pos = -1;
  gtk_list_box_insert (self->handler_list, GTK_WIDGET (row), pos);
  update_group_row_count (after, 1);
}

static void
add_file_type (CcApplicationsPanel *self,
               CcInfoRow           *after,
               const gchar         *type)
{
  CcActionRow *row;
  g_autofree gchar *desc = NULL;
  gint pos;
  const gchar *glob;

  glob = g_hash_table_lookup (self->globs, type);

  desc = g_content_type_get_description (type);
  row = cc_action_row_new ();
  cc_action_row_set_title (row, desc);
  cc_action_row_set_subtitle (row, glob ? glob : "");
  cc_action_row_set_action (row, _("Unset"), TRUE);
  g_object_set_data_full (G_OBJECT (row), "type", g_strdup (type), g_free);
  g_signal_connect_object (row, "activated", G_CALLBACK (unset_cb), self, G_CONNECT_SWAPPED);

  if (after)
    {
      pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (after)) + 1;
      g_object_bind_property (after, "expanded",
                              row, "visible",
                              G_BINDING_SYNC_CREATE);
    }
  else
    {
      pos = -1;
    }

  gtk_list_box_insert (self->handler_list, GTK_WIDGET (row), pos);
  update_group_row_count (after, 1);
}

static gboolean
is_hypertext_type (const gchar *type)
{
  const gchar *types[] = {
    "text/html",
    "text/htmlh",
    "text/xml",
    "application/xhtml+xml",
    "application/vnd.mozilla.xul+xml",
    "text/mml",
    NULL
  };
  return g_strv_contains (types, type);
}

static void
ensure_group_row (CcApplicationsPanel *self,
                  CcInfoRow **row,
                  const gchar *title)
{
  if (*row == NULL)
    {
      CcInfoRow *r = CC_INFO_ROW (g_object_new (CC_TYPE_INFO_ROW,
                                                "title", title,
                                                "has-expander", TRUE,
                                                NULL));
      gtk_list_box_insert (self->handler_list, GTK_WIDGET (r), -1);
      *row = r;
    }
}

static void
add_link_type (CcApplicationsPanel *self,
               const gchar         *type)
{
  ensure_group_row (self, &self->link, _("Links"));
  add_scheme (self, self->link, type);
}

static void
add_hypertext_type (CcApplicationsPanel *self,
                    const gchar         *type)
{
  ensure_group_row (self, &self->hypertext, _("Hypertext Files"));
  add_file_type (self, self->hypertext, type);
}

static gboolean
is_text_type (const gchar *type)
{
  return g_content_type_is_a (type, "text/*");
}

static void
add_text_type (CcApplicationsPanel *self,
               const gchar         *type)
{
  ensure_group_row (self, &self->text, _("Text Files"));
  add_file_type (self, self->text, type);
}

static gboolean
is_image_type (const gchar *type)
{
  return g_content_type_is_a (type, "image/*");
}

static void
add_image_type (CcApplicationsPanel *self,
                const gchar         *type)
{
  ensure_group_row (self, &self->images, _("Image Files"));
  add_file_type (self, self->images, type);
}

static gboolean
is_font_type (const gchar *type)
{
  return g_content_type_is_a (type, "font/*") ||
         g_str_equal (type, "application/x-font-pcf") ||
         g_str_equal (type, "application/x-font-type1");
}

static void
add_font_type (CcApplicationsPanel *self,
               const gchar         *type)
{
  ensure_group_row (self, &self->fonts, _("Font Files"));
  add_file_type (self, self->fonts, type);
}

static gboolean
is_archive_type (const gchar *type)
{
  const gchar *types[] = {
    "application/bzip2",
    "application/zip",
    "application/x-xz-compressed-tar",
    "application/x-xz",
    "application/x-xar",
    "application/x-tarz",
    "application/x-tar",
    "application/x-lzma-compressed-tar",
    "application/x-lzma",
    "application/x-lzip-compressed-tar",
    "application/x-lzip",
    "application/x-lha",
    "application/gzip",
    "application/x-cpio",
    "application/x-compressed-tar",
    "application/x-compress",
    "application/x-bzip-compressed-tar",
    "application/x-bzip",
    "application/x-7z-compressed-tar",
    "application/x-7z-compressed",
    "application/x-zoo",
    "application/x-war",
    "application/x-stuffit",
    "application/x-rzip-compressed-tar",
    "application/x-rzip",
    "application/vnd.rar",
    "application/x-lzop-compressed-tar",
    "application/x-lzop",
    "application/x-lz4-compressed-tar",
    "application/x-lz4",
    "application/x-lrzip-compressed-tar",
    "application/x-lrzip",
    "application/x-lhz",
    "application/x-java-archive",
    "application/x-ear",
    "application/x-cabinet",
    "application/x-bzip1-compressed-tar",
    "application/x-bzip1",
    "application/x-arj",
    "application/x-archive",
    "application/x-ar",
    "application/x-alz",
    "application/x-ace",
    "application/vnd.ms-cab-compressed",
    NULL
  };
  return g_strv_contains (types, type);
}

static void
add_archive_type (CcApplicationsPanel *self,
                  const gchar         *type)
{
  ensure_group_row (self, &self->archives, _("Archive Files"));
  add_file_type (self, self->archives, type);
}

static gboolean
is_package_type (const gchar *type)
{
  const gchar *types[] = {
    "application/x-source-rpm",
    "application/x-rpm",
    "application/vnd.debian.binary-package",
    NULL
  };
  return g_strv_contains (types, type);
}

static void
add_package_type (CcApplicationsPanel *self,
                  const gchar         *type)
{
  ensure_group_row (self, &self->packages, _("Package Files"));
  add_file_type (self, self->packages, type);
}

static gboolean
is_audio_type (const gchar *type)
{
  return g_content_type_is_a (type, "audio/*") ||
         g_str_equal (type, "application/ogg") ||
         g_str_equal (type, "application/x-shorten") ||
         g_str_equal (type, "application/x-matroska") ||
         g_str_equal (type, "application/x-flac") ||
         g_str_equal (type, "application/x-extension-mp4") ||
         g_str_equal (type, "application/x-extension-m4a") ||
         g_str_equal (type, "application/vnd.rn-realmedia") ||
         g_str_equal (type, "application/ram") ||
         g_str_equal (type, "application/vnd.ms-wpl");
}

static void
add_audio_type (CcApplicationsPanel *self,
                const gchar         *type)
{
  ensure_group_row (self, &self->audio, _("Audio Files"));
  add_file_type (self, self->audio, type);
}

static gboolean
is_video_type (const gchar *type)
{
  return g_content_type_is_a (type, "video/*") ||
         g_str_equal (type, "application/x-smil") ||
         g_str_equal (type, "application/vnd.ms-asf") ||
         g_str_equal (type, "application/mxf");
}

static void
add_video_type (CcApplicationsPanel *self,
                const gchar         *type)
{
  ensure_group_row (self, &self->video, _("Video Files"));
  add_file_type (self, self->video, type);
}

static void
add_other_type (CcApplicationsPanel *self,
                const gchar         *type)
{
  ensure_group_row (self, &self->other, _("Other Files"));
  add_file_type (self, self->other, type);
}

static void
add_handler_row (CcApplicationsPanel *self,
                 const gchar         *type)
{
  gtk_widget_show (GTK_WIDGET (self->handler_section));

  if (g_content_type_is_a (type, "x-scheme-handler/*"))
    add_link_type (self, type);
  else if (is_hypertext_type (type))
    add_hypertext_type (self, type);
  else if (is_font_type (type))
    add_font_type (self, type);
  else if (is_package_type (type))
    add_package_type (self, type);
  else if (is_audio_type (type))
    add_audio_type (self, type);
  else if (is_video_type (type))
    add_video_type (self, type);
  else if (is_archive_type (type))
    add_archive_type (self, type);
  else if (is_text_type (type))
    add_text_type (self, type);
  else if (is_image_type (type))
    add_image_type (self, type);
  else
    add_other_type (self, type);
}

static void
handler_row_activated_cb (CcApplicationsPanel *self,
                          GtkListBoxRow       *list_row)
{
  CcInfoRow *row;

  if (!CC_IS_INFO_ROW (list_row))
    return;

  row = CC_INFO_ROW (list_row);
  if (row == self->hypertext ||
      row == self->text ||
      row == self->images ||
      row == self->fonts ||
      row == self->archives ||
      row == self->packages ||
      row == self->audio ||
      row == self->video ||
      row == self->other ||
      row == self->link)
    {
      cc_info_row_set_expanded (row, !cc_info_row_get_expanded (row));
    }
}

static gboolean
app_info_recommended_for (GAppInfo    *info,
                          const gchar *type)
{
  /* this is horribly inefficient. I blame the mime system */
  g_autolist(GObject) list = NULL;
  GList *l;
  gboolean ret = FALSE;

  list = g_app_info_get_recommended_for_type (type);
  for (l = list; l; l = l->next)
    {
      GAppInfo *ri = l->data;

      if (g_app_info_equal (info, ri))
        {
          ret = TRUE;
          break;
        }
    }

  return ret;
}

static void
handler_reset_cb (CcApplicationsPanel *self)
{
  GtkListBoxRow *selected;
  GAppInfo *info;
  const gchar **types;
  gint i;

  selected = gtk_list_box_get_selected_row (self->sidebar_listbox);
  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (selected));

  types = g_app_info_get_supported_types (info);
  if (types == NULL || types[0] == NULL)
    return;

  g_signal_handler_block (self->monitor, self->monitor_id);
  for (i = 0; types[i]; i++)
    {
      gchar *ctype = g_content_type_from_mime_type (types[i]);
      g_app_info_add_supports_type (info, ctype, NULL);
    }
  g_signal_handler_unblock (self->monitor, self->monitor_id);
  g_signal_emit_by_name (self->monitor, "changed");
}

static void
update_handler_sections (CcApplicationsPanel *self,
                         GAppInfo            *info)
{
  g_autoptr(GHashTable) hash = NULL;
  const gchar **types;
  gint i;

  container_remove_all (GTK_CONTAINER (self->handler_list));

  self->hypertext = NULL;
  self->text = NULL;
  self->images = NULL;
  self->fonts = NULL;
  self->archives = NULL;
  self->packages = NULL;
  self->audio = NULL;
  self->video = NULL;
  self->other = NULL;
  self->link = NULL;

  gtk_widget_hide (GTK_WIDGET (self->handler_section));

  types = g_app_info_get_supported_types (info);
  if (types == NULL || types[0] == NULL)
    return;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  gtk_widget_set_sensitive (GTK_WIDGET (self->handler_reset), FALSE);
  for (i = 0; types[i]; i++)
    {
      g_autofree gchar *ctype = g_content_type_from_mime_type (types[i]);

      if (g_hash_table_contains (hash, ctype))
        continue;

      if (!app_info_recommended_for (info, ctype))
        {
          gtk_widget_set_sensitive (GTK_WIDGET (self->handler_reset), TRUE);
          continue;
        }

      add_handler_row (self, ctype);
      g_hash_table_add (hash, g_steal_pointer (&ctype));
    }
}

/* --- usage section --- */

static void
storage_row_activated_cb (CcApplicationsPanel *self,
                          GtkListBoxRow       *list_row)
{
  if (list_row == GTK_LIST_BOX_ROW (self->storage))
    {
      gtk_window_set_transient_for (GTK_WINDOW (self->storage_dialog),
                                    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
      gtk_window_present (GTK_WINDOW (self->storage_dialog));
    }
}

static void
update_total_size (CcApplicationsPanel *self)
{
  g_autofree gchar *formatted_size = NULL;
  guint64 total;

  total = self->app_size + self->data_size + self->cache_size;
  formatted_size = g_format_size (total);
  g_object_set (self->total, "info", formatted_size, NULL);
  g_object_set (self->storage, "info", formatted_size, NULL);
}

static void
set_cache_size (GObject      *source,
                GAsyncResult *res,
                gpointer      data)
{
  CcApplicationsPanel *self = data;
  g_autofree gchar *formatted_size = NULL;
  guint64 size;
  g_autoptr(GError) error = NULL;

  if (!file_size_finish (G_FILE (source), res, &size, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get flatpak cache size: %s", error->message);
      return;
    }
  self->cache_size = size;

  formatted_size = g_format_size (self->cache_size);
  g_object_set (self->cache, "info", formatted_size, NULL);

  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_cache_button), self->cache_size > 0);

  update_total_size (self);
}

static void
update_cache_row (CcApplicationsPanel *self,
                  const gchar         *app_id)
{
  g_autoptr(GFile) dir = get_flatpak_app_dir (app_id, "cache");
  g_object_set (self->cache, "info", "...", NULL);
  file_size_async (dir, cc_panel_get_cancellable (CC_PANEL (self)), set_cache_size, self);
}

static void
set_data_size (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  CcApplicationsPanel *self = data;
  g_autofree gchar *formatted_size = NULL;
  guint64 size;
  g_autoptr(GError) error = NULL;

  if (!file_size_finish (G_FILE (source), res, &size, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to get flatpak data size: %s", error->message);
      return;
    }
  self->data_size = size;

  formatted_size = g_format_size (self->data_size);
  g_object_set (self->data, "info", formatted_size, NULL);

  update_total_size (self);
}

static void
update_data_row (CcApplicationsPanel *self,
                 const gchar          *app_id)
{
  g_autoptr(GFile) dir = get_flatpak_app_dir (app_id, "data");

  g_object_set (self->data, "info", "...", NULL);
  file_size_async (dir, cc_panel_get_cancellable (CC_PANEL (self)), set_data_size, self);
}

static void
cache_cleared (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  CcApplicationsPanel *self = data;
  g_autoptr(GError) error = NULL;

  if (!file_remove_finish (G_FILE (source), res, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to remove cache: %s", error->message);
      return;
    }

  update_cache_row (self, self->current_app_id);
}

static void
clear_cache_cb (CcApplicationsPanel *self)
{
  g_autoptr(GFile) dir = NULL;

  if (self->current_app_id == NULL)
    return;

  dir = get_flatpak_app_dir (self->current_app_id, "cache");
  file_remove_async (dir, cc_panel_get_cancellable (CC_PANEL (self)), cache_cleared, self);
}

static void
update_app_row (CcApplicationsPanel *self,
                const gchar         *app_id)
{
  g_autofree gchar *formatted_size = NULL;

  if (g_str_has_prefix (app_id, PORTAL_SNAP_PREFIX))
    self->app_size = get_snap_app_size (app_id + strlen (PORTAL_SNAP_PREFIX));
  else
    self->app_size = get_flatpak_app_size (app_id);
  formatted_size = g_format_size (self->app_size);
  g_object_set (self->app, "info", formatted_size, NULL);
  update_total_size (self);
}

static void
update_app_sizes (CcApplicationsPanel *self,
                  const gchar         *app_id)
{
  gtk_widget_set_sensitive (GTK_WIDGET (self->clear_cache_button), FALSE);

  self->app_size = self->data_size = self->cache_size = 0;

  update_app_row (self, app_id);
  update_cache_row (self, app_id);
  update_data_row (self, app_id);
}

static void
update_usage_section (CcApplicationsPanel *self,
                      GAppInfo            *info)
{
  g_autofree gchar *portal_app_id = get_portal_app_id (info);

  if (portal_app_id != NULL)
    {
      gtk_widget_show (GTK_WIDGET (self->usage_section));
      update_app_sizes (self, portal_app_id);
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self->usage_section));
    }
}

/* --- panel setup --- */

static void
update_panel (CcApplicationsPanel *self,
              GtkListBoxRow       *row)
{
  GAppInfo *info;

  if (self->perm_store == NULL)
    {
      g_message ("No permissions store proxy yet, come back later");
      return;
    }

  if (row == NULL)
    {
      gtk_label_set_label (self->title_label, _("Applications"));
      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->empty_box));
      gtk_widget_hide (GTK_WIDGET (GTK_WIDGET (self->header_button)));
      return;
    }

  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (row));

  gtk_label_set_label (self->title_label, g_app_info_get_display_name (info));
  gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->settings_box));
  gtk_widget_show (GTK_WIDGET (self->header_button));

  g_clear_pointer (&self->current_app_id, g_free);
  g_clear_pointer (&self->current_portal_app_id, g_free);

  update_permission_section (self, info);
  update_integration_section (self, info);
  update_handler_sections (self, info);
  update_usage_section (self, info);

  self->current_app_id = get_app_id (info);
  self->current_portal_app_id = get_portal_app_id (info);
}

static void
populate_applications (CcApplicationsPanel *self)
{
  g_autolist(GObject) infos = NULL;
  GList *l;

  container_remove_all (GTK_CONTAINER (self->sidebar_listbox));
#ifdef HAVE_MALCONTENT
  g_signal_handler_block (self->manager, self->app_filter_id);
#endif

  infos = g_app_info_get_all ();

  for (l = infos; l; l = l->next)
    {
      GAppInfo *info = l->data;
      GtkWidget *row;
      g_autofree gchar *id = NULL;

      if (!g_app_info_should_show (info))
        continue;

#ifdef HAVE_MALCONTENT
      if (!mct_app_filter_is_appinfo_allowed (self->app_filter, info))
        continue;
#endif

      row = GTK_WIDGET (cc_applications_row_new (info));
      gtk_list_box_insert (self->sidebar_listbox, row, -1);

      id = get_app_id (info);
      if (g_strcmp0 (id, self->current_app_id) == 0)
        gtk_list_box_select_row (self->sidebar_listbox, GTK_LIST_BOX_ROW (row));
    }
#ifdef HAVE_MALCONTENT
  g_signal_handler_unblock (self->manager, self->app_filter_id);
#endif
}

static gint
compare_rows (GtkListBoxRow *row1,
              GtkListBoxRow *row2,
              gpointer       data)
{
  const gchar *key1 = cc_applications_row_get_sort_key (CC_APPLICATIONS_ROW (row1));
  const gchar *key2 = cc_applications_row_get_sort_key (CC_APPLICATIONS_ROW (row2));

  return strcmp (key1, key2);
}

static gboolean
filter_sidebar_rows (GtkListBoxRow *row,
                     gpointer       data)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (data);
  g_autofree gchar *app_name = NULL;
  g_autofree gchar *search_text = NULL;
  GAppInfo *info;

  /* Only filter after the second character */
  if (gtk_entry_get_text_length (self->sidebar_search_entry) < 2)
    return TRUE;

  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (row));
  app_name = cc_util_normalize_casefold_and_unaccent (g_app_info_get_name (info));
  search_text = cc_util_normalize_casefold_and_unaccent (gtk_entry_get_text (self->sidebar_search_entry));

  return g_strstr_len (app_name, -1, search_text) != NULL;
}

#ifdef HAVE_MALCONTENT
static void
app_filter_changed_cb (MctAppFilter        *app_filter,
                       uid_t               uid,
                       CcApplicationsPanel *self)
{
  populate_applications (self);
}
#endif

static void
apps_changed (CcApplicationsPanel *self)
{
  populate_applications (self);
}

static void
row_activated_cb (CcApplicationsPanel *self,
                  GtkListBoxRow       *row)
{
  update_panel (self, row);
  g_signal_emit_by_name (self, "sidebar-activated");
}

static void
on_perm_store_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      data)
{
  CcApplicationsPanel *self = data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to portal permission store: %s",
                     error->message);
      return;
    }

  self->perm_store = proxy;

  update_panel (self, gtk_list_box_get_selected_row (self->sidebar_listbox));
}

static void
select_app (CcApplicationsPanel *self,
            const gchar         *app_id)
{
  g_autoptr(GList) children = NULL;
  GList *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->sidebar_listbox));
  for (l = children; l; l = l->next)
    {
      CcApplicationsRow *row = CC_APPLICATIONS_ROW (l->data);
      GAppInfo *info = cc_applications_row_get_info (row);
      if (g_str_has_prefix (g_app_info_get_id (info), app_id))
        {
          gtk_list_box_select_row (self->sidebar_listbox, GTK_LIST_BOX_ROW (row));
          break;
        }
    }
}

static void
on_sidebar_search_entry_activated_cb (CcApplicationsPanel *self)
{
  GtkListBoxRow *row;

  row = gtk_list_box_get_row_at_y (self->sidebar_listbox, 0);

  if (!row)
    return;

  /* Show the app */
  gtk_list_box_select_row (self->sidebar_listbox, row);
  g_signal_emit_by_name (row, "activate");

  /* Cleanup the entry */
  gtk_entry_set_text (self->sidebar_search_entry, "");
  gtk_widget_grab_focus (GTK_WIDGET (self->sidebar_search_entry));
}

static void
on_sidebar_search_entry_search_changed_cb (CcApplicationsPanel *self)
{
  gtk_list_box_invalidate_filter (self->sidebar_listbox);
}

static void
on_sidebar_search_entry_search_stopped_cb (CcApplicationsPanel *self)
{
  gtk_entry_set_text (self->sidebar_search_entry, "");
}

static void
cc_applications_panel_dispose (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);

  g_clear_object (&self->monitor);
  g_clear_object (&self->perm_store);

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->dispose (object);
}

static void
cc_applications_panel_finalize (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);
#ifdef HAVE_MALCONTENT
  if (self->app_filter != NULL && self->app_filter_id != 0)
    {
      g_signal_handler_disconnect (self->manager, self->app_filter_id);
      self->app_filter_id = 0;
    }
  g_clear_pointer (&self->app_filter, mct_app_filter_unref);

  g_clear_object (&self->manager);
#endif
  g_clear_object (&self->notification_settings);
  g_clear_object (&self->location_settings);
  g_clear_object (&self->privacy_settings);
  g_clear_object (&self->search_settings);

  g_clear_pointer (&self->current_app_id, g_free);
  g_clear_pointer (&self->current_portal_app_id, g_free);
  g_clear_pointer (&self->globs, g_hash_table_unref);
  g_clear_pointer (&self->search_providers, g_hash_table_unref);

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->finalize (object);
}

static void
cc_applications_panel_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (property_id)
    {
      case PROP_PARAMETERS:
        {
          GVariant *parameters, *v;
          const gchar *first_arg = NULL;

          parameters = g_value_get_variant (value);
          if (parameters == NULL)
            return;

          if (g_variant_n_children (parameters) > 0)
            {
              g_variant_get_child (parameters, 0, "v", &v);
              if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
                first_arg = g_variant_get_string (v, NULL);
              else
                g_warning ("Wrong type for the second argument GVariant, expected 's' but got '%s'",
                           (gchar *)g_variant_get_type (v));
              g_variant_unref (v);

              select_app (CC_APPLICATIONS_PANEL (object), first_arg);
            }

          return;
        }
    }

  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cc_applications_panel_constructed (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);
  CcShell *shell;

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->constructed (object);

  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, GTK_WIDGET (self->header_button), GTK_POS_RIGHT);
}

static GtkWidget*
cc_applications_panel_get_sidebar_widget (CcPanel *panel)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (panel);
  return GTK_WIDGET (self->sidebar_box);
}

static GtkWidget *
cc_applications_panel_get_title_widget (CcPanel *panel)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (panel);
  return GTK_WIDGET (self->title_label);
}

static void
cc_applications_panel_class_init (CcApplicationsPanelClass *klass)
{
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_applications_panel_dispose;
  object_class->finalize = cc_applications_panel_finalize;
  object_class->constructed = cc_applications_panel_constructed;
  object_class->set_property = cc_applications_panel_set_property;

  panel_class->get_sidebar_widget = cc_applications_panel_get_sidebar_widget;
  panel_class->get_title_widget = cc_applications_panel_get_title_widget;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-applications-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_label);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, cache);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, clear_cache_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, data);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, empty_box);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, header_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_reset);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, integration_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, integration_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_search);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, notification);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, background);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, wallpaper);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, shortcuts);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sidebar_box);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sidebar_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sidebar_search_entry);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, search);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, settings_box);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, title_label);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, total);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, usage_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, usage_section);

  gtk_widget_class_bind_template_callback (widget_class, camera_cb);
  gtk_widget_class_bind_template_callback (widget_class, location_cb);
  gtk_widget_class_bind_template_callback (widget_class, microphone_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_cb);
  gtk_widget_class_bind_template_callback (widget_class, background_cb);
  gtk_widget_class_bind_template_callback (widget_class, wallpaper_cb);
  gtk_widget_class_bind_template_callback (widget_class, shortcuts_cb);
  gtk_widget_class_bind_template_callback (widget_class, privacy_link_cb);
  gtk_widget_class_bind_template_callback (widget_class, sound_cb);
  gtk_widget_class_bind_template_callback (widget_class, permission_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, handler_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, clear_cache_cb);
  gtk_widget_class_bind_template_callback (widget_class, storage_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, open_software_cb);
  gtk_widget_class_bind_template_callback (widget_class, handler_reset_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_sidebar_search_entry_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_sidebar_search_entry_search_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_sidebar_search_entry_search_stopped_cb);
}

static void
cc_applications_panel_init (CcApplicationsPanel *self)
{
  g_autoptr(GtkStyleProvider) provider = NULL;
  GtkListBoxRow *row;
#ifdef HAVE_MALCONTENT
  g_autoptr(GDBusConnection) system_bus = NULL;
  g_autoptr(GError) error = NULL;
#endif

  g_resources_register (cc_applications_get_resource ());

  g_type_ensure(CC_TYPE_TOGGLE_ROW);
  g_type_ensure(CC_TYPE_INFO_ROW);

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (provider),
                                       "/org/gnome/control-center/applications/cc-applications-panel.css");

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_signal_connect_object (self->sidebar_listbox, "row-activated",
                           G_CALLBACK (row_activated_cb), self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->header_button, "clicked", G_CALLBACK (open_software_cb), self, G_CONNECT_SWAPPED);

  gtk_list_box_set_header_func (self->permission_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (self->integration_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (self->handler_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (self->usage_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (self->builtin_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (self->storage_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_sort_func (self->sidebar_listbox,
                              compare_rows,
                              NULL, NULL);

  gtk_list_box_set_filter_func (self->sidebar_listbox,
                                filter_sidebar_rows,
                                self, NULL);

  self->location_settings = g_settings_new ("org.gnome.system.location");
  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");
  self->search_settings = g_settings_new ("org.gnome.desktop.search-providers");
#ifdef HAVE_MALCONTENT
   /* FIXME: should become asynchronous */
  system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, self->cancellable, &error);
  if (system_bus == NULL)
    {
      g_warning ("Error getting system bus while setting up app permissions: %s", error->message);
      return;
    }

  /* Load the user’s parental controls settings too, so we can filter the list. */
  self->manager = mct_manager_new (system_bus);
  self->app_filter = mct_manager_get_app_filter (self->manager,
                                                 getuid (),
                                                 MCT_GET_APP_FILTER_FLAGS_NONE,
                                                 self->cancellable,
                                                 &error);
  if (error)
    {
      g_warning ("Error retrieving app filter: %s", error->message);
      return;
    }

  self->app_filter_id = g_signal_connect (self->manager, "app-filter-changed",
                                          G_CALLBACK (app_filter_changed_cb), self);
#endif
  populate_applications (self);

  self->monitor = g_app_info_monitor_get ();
  self->monitor_id = g_signal_connect_object (self->monitor, "changed", G_CALLBACK (apps_changed), self, G_CONNECT_SWAPPED);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            cc_panel_get_cancellable (CC_PANEL (self)),
                            on_perm_store_ready,
                            self);

  self->globs = parse_globs ();
  self->search_providers = parse_search_providers ();

  /* Select the first row */
  row = gtk_list_box_get_row_at_index (self->sidebar_listbox, 0);
  gtk_list_box_select_row (self->sidebar_listbox, row);
  g_signal_emit_by_name (row, "activate");
}
