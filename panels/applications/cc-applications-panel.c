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

#include <config.h>
#include <glib/gi18n.h>

#include <gio/gdesktopappinfo.h>

#include "cc-applications-panel.h"
#include "cc-applications-row.h"
#include "cc-toggle-row.h"
#include "cc-info-row.h"
#include "cc-action-row.h"
#include "cc-applications-resources.h"
#include "list-box-helper.h"
#include "utils.h"

#include <flatpak/flatpak.h>

/* Todo
 * - search
 * - undo for mime type removal
 * - reset per section ?
 * - handle cache async
 * - notification visibility
 * - empty state
 *
 * Missing in flatpak:
 * - make portal write notification permissions
 * - background
 * - usb devices
 *
 * Update privacy panel to match
 */

enum {
  PROP_0,
  PROP_PARAMETERS
};

struct _CcApplicationsPanel
{
  CcPanel     parent;

  GtkListBox *sidebar_listbox;
  GtkWidget *header_button;
  GtkWidget *title_label;
  GAppInfoMonitor *monitor;

  GCancellable *cancellable;

  char *current_app_id;

  GHashTable *globs;

  GDBusProxy *perm_store;
  GSettings *notification_settings;
  GSettings *location_settings;
  GSettings *privacy_settings;

  GtkListBox *stack;
  GtkWidget *permission_section;
  GtkWidget *permission_list;
  GtkWidget *builtin_dialog;
  GtkWidget *builtin_label;
  GtkWidget *builtin_list;

  GtkWidget *camera;
  GtkWidget *no_camera;
  GtkWidget *location;
  GtkWidget *no_location;
  GtkWidget *microphone;
  GtkWidget *no_microphone;

  GtkWidget *integration_section;
  GtkWidget *integration_list;
  GtkWidget *notification;
  GtkWidget *sound;
  GtkWidget *no_sound;
  GtkWidget *builtin;

  GtkWidget *device_section;
  GtkWidget *device_list;

  GtkWidget *handler_section;
  GtkWidget *handler_list;
  GtkWidget *hypertext;
  GtkWidget *text;
  GtkWidget *images;
  GtkWidget *fonts;
  GtkWidget *archives;
  GtkWidget *packages;
  GtkWidget *audio;
  GtkWidget *video;
  GtkWidget *other;
  GtkWidget *link;
  GtkWidget *usage_section;
  GtkWidget *usage_list;
  GtkWidget *storage;
  GtkWidget *storage_dialog;
  GtkWidget *storage_list;
  GtkWidget *app;
  GtkWidget *data;
  GtkWidget *cache;
  GtkWidget *total;
  GtkWidget *clear_cache_button;
};

G_DEFINE_TYPE (CcApplicationsPanel, cc_applications_panel, CC_TYPE_PANEL)

static void
cc_applications_panel_dispose (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);

  g_clear_object (&self->monitor);
  g_clear_object (&self->perm_store);

  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->dispose (object);
}

static void
cc_applications_panel_finalize (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);

  g_clear_object (&self->notification_settings);
  g_clear_object (&self->location_settings);
  g_clear_object (&self->privacy_settings);
  g_clear_object (&self->cancellable);

  g_free (self->current_app_id);
  g_hash_table_unref (self->globs);

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->finalize (object);
}

static GtkWidget*
cc_applications_panel_get_sidebar_widget (CcPanel *panel)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (panel);
  return GTK_WIDGET (self->sidebar_listbox);
}

static GtkWidget *
cc_applications_panel_get_title_widget (CcPanel *panel)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (panel);
  return self->title_label;
}

static void
cc_applications_panel_constructed (GObject *object)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (object);
  CcShell *shell;

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->constructed (object);

  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, self->header_button);
}

static char **
get_flatpak_permissions (CcApplicationsPanel *self,
                         const char *table,
                         const char *id,
                         const char *app_id)
{
  g_autoptr(GVariant) ret = NULL;
  g_autoptr(GVariantIter) iter = NULL;
  char *key;
  GVariant *val;
  char **permissions = NULL;

  ret = g_dbus_proxy_call_sync (self->perm_store,
                                "Lookup",
                                g_variant_new ("(ss)", table, id),
                                0, G_MAXINT, NULL, NULL);
  if (ret == NULL)
    return NULL;

  g_variant_get (ret, "(a{sas}v)", &iter, NULL);

  while (g_variant_iter_loop (iter, "{s@as}", &key, &val))
    {
      if (strcmp (key, app_id) == 0)
        {
          permissions = g_variant_dup_strv (val, NULL); 
          break;
        }
    }

  return permissions;
}

static void
set_flatpak_permissions (CcApplicationsPanel *self,
                         const char *table,
                         const char *id,
                         const char *app_id,
                         const char * const *permissions)
{
  g_dbus_proxy_call_sync (self->perm_store,
                          "SetPermission",
                          g_variant_new ("(sbss^as)", table, TRUE, id, app_id, permissions),
                          0, G_MAXINT, NULL, NULL);
}

static void
search_cb (CcApplicationsPanel *self)
{
  g_print ("search not implemented\n");
}

static gboolean
app_info_is_flatpak (GAppInfo *info)
{
  if (G_IS_DESKTOP_APP_INFO (info))
    {
      g_autofree char *marker = NULL;
      marker = g_desktop_app_info_get_string (G_DESKTOP_APP_INFO (info), "X-Flatpak");
      return marker != NULL;
    }

  return FALSE;
}

static void
get_notification_allowed (CcApplicationsPanel *self,
                          const char *app_id,
                          gboolean *set,
                          gboolean *allowed)
{
  if (self->notification_settings)
    {
      *set = TRUE; /* FIXME */
      *allowed = g_settings_get_boolean (self->notification_settings, "enable");
    }
  else
    {
      g_auto(GStrv) perms = get_flatpak_permissions (self, "notifications", "notification", app_id);
      *set = TRUE; /* FIXME */
      *allowed = perms == NULL || strcmp (perms[0], "no") != 0;
    }
}

static void
set_notification_allowed (CcApplicationsPanel *self,
                          gboolean allowed)
{
  if (self->notification_settings)
    {
      g_settings_set_boolean (self->notification_settings, "enable", allowed);
    }
  else
    {
      const char *perms[2] = { NULL, NULL };

      perms[0] = allowed ? "yes" : "no";
      set_flatpak_permissions (self, "notifications", "notification", self->current_app_id, perms);
    }
}

static void
notification_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_notification_allowed (self, cc_toggle_row_get_allowed (CC_TOGGLE_ROW (self->notification)));
}

static char *
munge_app_id (const char *app_id)
{
  int i;
  char *id = g_strdup (app_id);

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

#define MASTER_SCHEMA "org.gnome.desktop.notifications"
#define APP_SCHEMA MASTER_SCHEMA ".application"
#define APP_PREFIX "/org/gnome/desktop/notifications/application/"

static GSettings *
get_notification_settings (const char *app_id)
{
  g_autofree char *munged_app_id = munge_app_id (app_id);
  g_autofree char *path = g_strconcat (APP_PREFIX, munged_app_id, "/", NULL);
  return g_settings_new_with_path (APP_SCHEMA, path);
}

static void
get_device_allowed (CcApplicationsPanel *self,
                    const char *device,
                    const char *app_id,
                    gboolean *set,
                    gboolean *allowed)
{
  g_auto(GStrv) perms = NULL;

  perms = get_flatpak_permissions (self, "devices", device, app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || strcmp (perms[0], "no") != 0;
}

static void
set_device_allowed (CcApplicationsPanel *self,
                    const char *device,
                    gboolean allowed)
{
  const char *perms[2];

  perms[0] = allowed ? "yes" : "no";
  perms[1] = NULL;

  set_flatpak_permissions (self, "devices", device, self->current_app_id, perms);
}

static void
microphone_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_device_allowed (self, "microphone", cc_toggle_row_get_allowed (CC_TOGGLE_ROW (self->microphone)));
}

static void
sound_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
   set_device_allowed (self, "speakers", cc_toggle_row_get_allowed (CC_TOGGLE_ROW (self->sound)));
}

static void
camera_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_device_allowed (self, "camera", cc_toggle_row_get_allowed (CC_TOGGLE_ROW (self->camera)));
}

static void
get_location_allowed (CcApplicationsPanel *self,
                      const char *app_id,
                      gboolean *set,
                      gboolean *allowed)
{
  g_auto(GStrv) perms = NULL;

  perms = get_flatpak_permissions (self, "location", "location", app_id);

  *set = perms != NULL;
  *allowed = perms == NULL || strcmp (perms[0], "NONE") != 0;
}

static void
set_location_allowed (CcApplicationsPanel *self,
                      gboolean allowed)
{
  const char *perms[3];
  g_autofree char *time = NULL;

  // FIXME allow setting accuracy
  perms[0] = allowed ? "EXACT" : "NONE";
  perms[1] = "0";
  perms[2] = NULL;

  set_flatpak_permissions (self, "location", "location", self->current_app_id, perms);
}

static void
location_cb (CcApplicationsPanel *self)
{
  if (self->current_app_id)
    set_location_allowed (self, cc_toggle_row_get_allowed (CC_TOGGLE_ROW (self->location)));
}

static GFile *
get_app_dir (const char *app_id,
             const char *subdir)
{
  g_autofree char *path = g_build_filename (g_get_home_dir (), ".var", "app", app_id, NULL);
  g_autoptr(GFile) appdir = g_file_new_for_path (path);
  return g_file_get_child (appdir, subdir);
}

static guint64
calculate_dir_size (const char *app_id,
                    const char *subdir)
{
  g_autoptr(GFile) cachedir = get_app_dir (app_id, subdir);
  return file_size_recursively (cachedir);
}

static void
privacy_link_cb (CcApplicationsPanel *self)
{
  CcShell *shell = cc_panel_get_shell (CC_PANEL (self));
  g_autoptr(GError) error = NULL;

  if (!cc_shell_set_active_panel_from_id (shell, "privacy", NULL, &error))
    g_warning ("Failed to switch to privacy panel: %s", error->message);
}

static guint64
update_app_row (CcInfoRow *row,
                const char *app_id)
{
  guint64 size;
  g_autofree char *formatted_size = NULL;

  size = get_flatpak_app_size (app_id);
  formatted_size = g_format_size (size);
  g_object_set (row, "info", formatted_size, NULL);

  return size;
}

static guint64
update_dir_row (CcInfoRow *row,
                const char *app_id,
                const char *subdir)
{
  guint64 size;
  g_autofree char *formatted_size = NULL;

  size = calculate_dir_size (app_id, subdir);
  formatted_size = g_format_size (size);
  g_object_set (row, "info", formatted_size, NULL);

  return size;
}

static void update_flatpak_sizes (CcApplicationsPanel *self, const char *app_id);

static void
clear_cache_cb (CcApplicationsPanel *self)
{
  g_autoptr(GFile) dir = NULL;

  if (self->current_app_id == NULL)
    return;

  dir = get_app_dir (self->current_app_id, "cache");
  file_remove_recursively (dir);
  update_flatpak_sizes (self, self->current_app_id);
}

static char *
get_app_id (GAppInfo *info)
{
  char *app_id = g_strdup (g_app_info_get_id (info));

  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  return app_id;
}

static int
add_static_permission_row (CcApplicationsPanel *self,
                           const char *title,
                           const char *subtitle)
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
permission_row_activated_cb (GtkListBox    *list,
                             GtkListBoxRow *list_row,
                             CcApplicationsPanel *self)
{
  GtkWidget *row = GTK_WIDGET (list_row);
  g_autoptr(GError) error = NULL;

  if (row == self->builtin)
    {
      gtk_window_set_transient_for (GTK_WINDOW (self->builtin_dialog),
                                    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
      gtk_window_present (GTK_WINDOW (self->builtin_dialog));
    }
}

static gboolean
add_static_permissions (CcApplicationsPanel *self,
                        GAppInfo *info,
                        const char *app_id)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  char **strv;
  char *str;
  int added = 0;
  g_autofree char *text = NULL;
  
  ref = find_flatpak_ref (app_id);
  bytes = flatpak_installed_ref_load_metadata (ref, NULL, NULL);
  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_data (keyfile,
                                  g_bytes_get_data (bytes, NULL),
                                  g_bytes_get_size (bytes),
                                  0, &error))
    {
      g_warning ("%s", error->message);
      return FALSE;
    }

  strv = g_key_file_get_string_list (keyfile, "Context", "sockets", NULL, NULL);
  if (strv && g_strv_contains ((const char * const*)strv, "system-bus"))
    added += add_static_permission_row (self, _("System Bus"), _("Full access"));
  if (strv && g_strv_contains ((const char * const*)strv, "session-bus"))
    added += add_static_permission_row (self, _("Session Bus"), _("Full access"));
  g_strfreev (strv);

  strv = g_key_file_get_string_list (keyfile, "Context", "devices", NULL, NULL);
  if (strv && g_strv_contains ((const char * const*)strv, "all"))
    added += add_static_permission_row (self, _("Devices"), _("Full access to /dev"));
  g_strfreev (strv);

  strv = g_key_file_get_string_list (keyfile, "Context", "shared", NULL, NULL);
  if (strv && g_strv_contains ((const char * const*)strv, "network"))
    added += add_static_permission_row (self, _("Network"), _("Has network access"));
  g_strfreev (strv);

  strv = g_key_file_get_string_list (keyfile, "Context", "filesystems", NULL, NULL);
  if (strv && (g_strv_contains ((const char * const *)strv, "home") ||
               g_strv_contains ((const char * const *)strv, "home:rw")))
    added += add_static_permission_row (self, _("Home"), _("Full access"));
  else if (strv && g_strv_contains ((const char * const *)strv, "home:ro"))
    added += add_static_permission_row (self, _("Home"), _("Read-only"));
  if (strv && (g_strv_contains ((const char * const *)strv, "host") ||
               g_strv_contains ((const char * const *)strv, "host:rw")))
    added += add_static_permission_row (self, _("File System"), _("Full access"));
  else if (strv && g_strv_contains ((const char * const *)strv, "host:ro"))
    added += add_static_permission_row (self, _("File System"), _("Read-only"));
  g_strfreev (strv);

  str = g_key_file_get_string (keyfile, "Session Bus Policy", "ca.desrt.dconf", NULL);
  if (str && g_str_equal (str, "talk"))
    added += add_static_permission_row (self, _("Settings"), _("Can change settings"));
  g_free (str);

  gtk_widget_set_visible (self->builtin, added > 0);

  text = g_strdup_printf (_("%s has the following permissions built-in. These cannot be altered. If you are concerned about these permissions, consider removing this application."), g_app_info_get_display_name (info));
  gtk_label_set_label (GTK_LABEL (self->builtin_label), text);

  return added > 0;
}

static void
remove_static_permissions (CcApplicationsPanel *self)
{
  container_remove_all (GTK_CONTAINER (self->builtin_list));
}

static void
update_permission_section (CcApplicationsPanel *self,
                           GAppInfo *info)
{
  g_autofree char *app_id = get_app_id (info);
  gboolean disabled, allowed, set;
  gboolean has_any = FALSE;

  if (!app_info_is_flatpak (info))
    {
      gtk_widget_hide (self->permission_section);
      return;
    }

  disabled = g_settings_get_boolean (self->privacy_settings, "disable-camera");
  get_device_allowed (self, "camera", app_id, &set, &allowed);
  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->camera), allowed);
  gtk_widget_set_visible (self->camera, set && !disabled);
  gtk_widget_set_visible (self->no_camera, set && disabled);
  has_any |= set;

  disabled = g_settings_get_boolean (self->privacy_settings, "disable-microphone");
  get_device_allowed (self, "microphone", app_id, &set, &allowed);
  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->microphone), allowed);
  gtk_widget_set_visible (self->microphone, set && !disabled);
  gtk_widget_set_visible (self->no_microphone, set && disabled);
  has_any |= set;

  disabled = !g_settings_get_boolean (self->location_settings, "enabled");
  get_location_allowed (self, app_id, &set, &allowed);
  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->location), allowed);
  gtk_widget_set_visible (self->location, set && !disabled);
  gtk_widget_set_visible (self->no_location, set && disabled);
  has_any |= set;

  remove_static_permissions (self);
  has_any |= add_static_permissions (self, info, app_id);

  gtk_widget_set_visible (self->permission_section, has_any);
}

static void
update_integration_section (CcApplicationsPanel *self,
                            GAppInfo *info)
{
  g_autofree char *app_id = get_app_id (info);
  gboolean set, allowed, disabled;
  gboolean has_any = FALSE;

  if (app_info_is_flatpak (info))
    {
      g_clear_object (&self->notification_settings);
      get_notification_allowed (self, app_id, &set, &allowed);
      cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->notification), allowed);
      gtk_widget_set_visible (self->notification, set);
      has_any |= set;

      disabled = g_settings_get_boolean (self->privacy_settings, "disable-sound-output");
      get_device_allowed (self, "speakers", app_id, &set, &allowed);
      cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->sound), allowed);
      gtk_widget_set_visible (self->sound, set && !disabled);
      gtk_widget_set_visible (self->no_sound, set && disabled);
    }
  else
    {
      g_set_object (&self->notification_settings, get_notification_settings (app_id));
      get_notification_allowed (self, app_id, &set, &allowed);
      cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->notification), allowed);
      gtk_widget_set_visible (self->notification, set);
      has_any |= set;

      gtk_widget_hide (self->sound);
      gtk_widget_hide (self->no_sound);
    }

  gtk_widget_set_visible (self->integration_section, has_any);
}

static void
update_device_section (CcApplicationsPanel *self,
                       GAppInfo *info)
{
  // No usb portal yet
  gtk_widget_hide (self->device_section);
}

static void
unset_cb (CcActionRow *row,
          CcApplicationsPanel *self)
{
  const char *type;
  GtkListBoxRow *selected;
  GAppInfo *info;

  selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->sidebar_listbox));
  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (selected));  

  type = (const char *)g_object_get_data (G_OBJECT (row), "type");

  g_app_info_remove_supports_type (info, type, NULL);
}

static void
update_group_row_count (GtkWidget *row,
                        int delta)
{
  int count;
  g_autofree char *text = NULL;

  count = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (row), "count"));
  count += delta;
  g_object_set_data (G_OBJECT (row), "count", GINT_TO_POINTER (count));
  text = g_strdup_printf ("%d", count);
  g_object_set (row, "info", text, NULL);
}

static void
add_scheme (CcApplicationsPanel *self,
            GtkWidget *after,
            const char *type)
{
  CcActionRow *row = NULL;
  int pos;

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
      char *scheme = strrchr (type, '/') + 1;
      g_autofree char *title = g_strdup_printf (_("%s Links"), scheme);
      g_autofree char *subtitle = g_strdup_printf ("%s://", scheme);  

      row = cc_action_row_new ();
      cc_action_row_set_title (row, title);
      cc_action_row_set_subtitle (row, subtitle);
    }

  cc_action_row_set_action (row, _("Unset"), TRUE);
  g_object_set_data_full (G_OBJECT (row), "type", g_strdup (type), g_free);
  g_signal_connect (row, "activated", G_CALLBACK (unset_cb), self);

  if (after)
    {
      pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (after)) + 1;
      g_object_bind_property (after, "expanded",
                              row, "visible",
                              G_BINDING_SYNC_CREATE);
    }
  else
    pos = -1;
  gtk_list_box_insert (GTK_LIST_BOX (self->handler_list), GTK_WIDGET (row), pos);
  update_group_row_count (after, 1);
}

static void
add_file_type (CcApplicationsPanel *self,
               GtkWidget *after,
               const char *type)
{
  CcActionRow *row;
  const char *desc;
  int pos;
  const char *glob;

  glob = g_hash_table_lookup (self->globs, type);

  desc = g_content_type_get_description (type);
  row = cc_action_row_new ();
  cc_action_row_set_title (row, desc);
  cc_action_row_set_subtitle (row, glob ? glob : "");
  cc_action_row_set_action (row, _("Unset"), TRUE);
  g_object_set_data_full (G_OBJECT (row), "type", g_strdup (type), g_free);
  g_signal_connect (row, "activated", G_CALLBACK (unset_cb), self);

  if (after)
    {
      pos = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (after)) + 1;
      g_object_bind_property (after, "expanded",
                              row, "visible",
                              G_BINDING_SYNC_CREATE);
    }
  else
    pos = -1;
  gtk_list_box_insert (GTK_LIST_BOX (self->handler_list), GTK_WIDGET (row), pos);
  update_group_row_count (after, 1);
}

static gboolean
is_hypertext_type (const char *type)
{
  const char *types[] = {
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
                  GtkWidget **row,
                  const char *title)
{
  if (*row == NULL)
    {
      CcInfoRow *r = CC_INFO_ROW (g_object_new (CC_TYPE_INFO_ROW,
                                                "title", title,
                                                "has-expander", TRUE,
                                                NULL));
      gtk_list_box_insert (GTK_LIST_BOX (self->handler_list), GTK_WIDGET (r), -1);
      *row = GTK_WIDGET (r);
    }
}

static void
add_link_type (CcApplicationsPanel *self,
               const char *type)
{
  ensure_group_row (self, &self->link, _("Links"));
  add_scheme (self, self->link, type);
}

static void
add_hypertext_type (CcApplicationsPanel *self,
                    const char *type)
{
  ensure_group_row (self, &self->hypertext, _("Hypertext Files"));
  add_file_type (self, self->hypertext, type);
}

static gboolean
is_text_type (const char *type)
{
  return g_content_type_is_a (type, "text/*");
}

static void
add_text_type (CcApplicationsPanel *self,
               const char *type)
{
  ensure_group_row (self, &self->text, _("Text Files"));
  add_file_type (self, self->text, type);
}

static gboolean
is_image_type (const char *type)
{
  return g_content_type_is_a (type, "image/*");
}

static void
add_image_type (CcApplicationsPanel *self,
                const char *type)
{
  ensure_group_row (self, &self->images, _("Image Files"));
  add_file_type (self, self->images, type);
}

static gboolean
is_font_type (const char *type)
{
  return g_content_type_is_a (type, "font/*") ||
         g_str_equal (type, "application/x-font-pcf") ||
         g_str_equal (type, "application/x-font-type1");
}

static void
add_font_type (CcApplicationsPanel *self,
               const char *type)
{
  ensure_group_row (self, &self->fonts, _("Font Files"));
  add_file_type (self, self->fonts, type);
}

static gboolean
is_archive_type (const char *type)
{
  const char *types[] = {
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
                  const char *type)
{
  ensure_group_row (self, &self->archives, _("Archive Files"));
  add_file_type (self, self->archives, type);
}

static gboolean
is_package_type (const char *type)
{
  const char *types[] = {
    "application/x-source-rpm",
    "application/x-rpm",
    "application/vnd.debian.binary-package",
    NULL
  };
  return g_strv_contains (types, type);
}

static void
add_package_type (CcApplicationsPanel *self,
                  const char *type)
{
  ensure_group_row (self, &self->packages, _("Package Files"));
  add_file_type (self, self->packages, type);
}

static gboolean
is_audio_type (const char *type)
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
                const char *type)
{
  ensure_group_row (self, &self->audio, _("Audio Files"));
  add_file_type (self, self->audio, type);
}

static gboolean
is_video_type (const char *type)
{
  return g_content_type_is_a (type, "video/*") ||
         g_str_equal (type, "application/x-smil") ||
         g_str_equal (type, "application/vnd.ms-asf") ||
         g_str_equal (type, "application/mxf");
}

static void
add_video_type (CcApplicationsPanel *self,
                const char *type)
{
  ensure_group_row (self, &self->video, _("Video Files"));
  add_file_type (self, self->video, type);
}

static void
add_other_type (CcApplicationsPanel *self,
                const char *type)
{
  ensure_group_row (self, &self->other, _("Other Files"));
  add_file_type (self, self->other, type);
}

static void
add_handler_row (CcApplicationsPanel *self,
                 const char *type)
{
  gtk_widget_show (self->handler_section);

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
handler_row_activated_cb (GtkListBox    *list,
                          GtkListBoxRow *list_row,
                          CcApplicationsPanel *self)
{
  GtkWidget *row = GTK_WIDGET (list_row);

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
    cc_info_row_set_expanded (CC_INFO_ROW (row),
                              !cc_info_row_get_expanded (CC_INFO_ROW (row)));
}

static gboolean
app_info_recommended_for (GAppInfo *info,
                          const char *type)
{
  /* this is horribly inefficient. I blame the mime system */
  GList *list, *l;
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

  g_list_free_full (list, g_object_unref);

  return ret;
}

static void
update_handler_sections (CcApplicationsPanel *self,
                         GAppInfo *info)
{
  const char **types;
  int i;
  g_autoptr(GHashTable) hash = NULL;

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

  gtk_widget_hide (self->handler_section);

  types = g_app_info_get_supported_types (info);
  if (types == NULL || types[0] == NULL)
    return;

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  for (i = 0; types[i]; i++)
    {
      char *ctype = g_content_type_from_mime_type (types[i]);
      if (g_hash_table_contains (hash, ctype))
        {
          g_free (ctype);
          continue;
        }
      if (!app_info_recommended_for (info, ctype))
        {
          g_free (ctype);
          continue;
        }
      g_hash_table_add (hash, ctype);
      add_handler_row (self, ctype);
    }
}

static void
storage_row_activated_cb (GtkListBox    *list,
                          GtkListBoxRow *list_row,
                          CcApplicationsPanel *self)
{
  GtkWidget *row = GTK_WIDGET (list_row);
  g_autoptr(GError) error = NULL;

  if (row == self->storage)
    {
      gtk_window_set_transient_for (GTK_WINDOW (self->storage_dialog),
                                    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
      gtk_window_present (GTK_WINDOW (self->storage_dialog));
    }
}

static void
update_flatpak_sizes (CcApplicationsPanel *self,
                      const char *app_id)
{
  g_autofree char *formatted_size = NULL;
  guint64 total = 0;
  guint64 cache_size;

  gtk_widget_show (self->usage_section);
  total += update_app_row (CC_INFO_ROW (self->app), app_id);
  cache_size = update_dir_row (CC_INFO_ROW (self->data), app_id, "data");
  total += cache_size;
  total += update_dir_row (CC_INFO_ROW (self->cache), app_id, "cache");

  formatted_size = g_format_size (total);
  g_object_set (self->total, "info", formatted_size, NULL);
  g_object_set (self->storage, "info", formatted_size, NULL);

  gtk_widget_set_sensitive (self->clear_cache_button, cache_size > 0);
}

static void
update_usage_section (CcApplicationsPanel *self,
                      GAppInfo *info)
{
  if (app_info_is_flatpak (info))
    {
      g_autofree char *app_id = get_app_id (info);
      update_flatpak_sizes (self, app_id);
    }
  else
    {
      gtk_widget_hide (self->usage_section);
    }
}

static void
update_panel (CcApplicationsPanel *self,
              GtkListBoxRow *row)
{
  GAppInfo *info;

  if (self->perm_store == NULL)
    {
      g_print ("no perm store proxy yet, come back later\n");
      return;
    }

  if (row == NULL)
    {
      gtk_label_set_label (GTK_LABEL (self->title_label), _("Applications"));
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "empty");
      return;
    }

  gtk_stack_set_visible_child_name (GTK_STACK (self->stack), "settings");

  g_clear_pointer (&self->current_app_id, g_free);

  info = cc_applications_row_get_info (CC_APPLICATIONS_ROW (row));

  gtk_label_set_label (GTK_LABEL (self->title_label), g_app_info_get_display_name (info));

  update_permission_section (self, info);
  update_integration_section (self, info);
  update_device_section (self, info);
  update_handler_sections (self, info);
  update_usage_section (self, info);

  self->current_app_id = get_app_id (info);
}

static void
populate_applications (CcApplicationsPanel *self)
{
  GList *infos, *l;

  container_remove_all (GTK_CONTAINER (self->sidebar_listbox));

  infos = g_app_info_get_all ();

  for (l = infos; l; l = l->next)
    {
      GAppInfo *info = l->data;
      GtkWidget *row;
      g_autofree char *id = NULL;

      if (!g_app_info_should_show (info))
        continue;

      row = GTK_WIDGET (cc_applications_row_new (info));
      gtk_list_box_insert (GTK_LIST_BOX (self->sidebar_listbox), row, -1);

      id = get_app_id (info);
      if (g_strcmp0 (id, self->current_app_id) == 0)
        gtk_list_box_select_row (GTK_LIST_BOX (self->sidebar_listbox), GTK_LIST_BOX_ROW (row));
    }

  g_list_free_full (infos, g_object_unref);
}

static void
prepare_content (CcApplicationsPanel *self)
{
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->permission_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->integration_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->handler_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->usage_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->builtin_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->storage_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

}

static int
compare_rows (GtkListBoxRow *row1,
              GtkListBoxRow *row2,
              gpointer       data)
{
  const char *key1 = cc_applications_row_get_sort_key (CC_APPLICATIONS_ROW (row1));
  const char *key2 = cc_applications_row_get_sort_key (CC_APPLICATIONS_ROW (row2));

  return strcmp (key1, key2);
}

static void
apps_changed (GAppInfoMonitor *monitor,
              CcApplicationsPanel *self)
{
  populate_applications (self);
}

static void
row_selected_cb (GtkListBox *list,
                 GtkListBoxRow *row,
                 CcApplicationsPanel *self)
{
  update_panel (self, row);
}

static void
open_software_cb (GtkButton *button,
                  CcApplicationsPanel *self)
{
  const char *argv[] = { "gnome-software", "--details", "appid", NULL };

  if (self->current_app_id == NULL)
    return;

  argv[2] = self->current_app_id;

  g_spawn_async (NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

static void
on_perm_store_ready (GObject *source_object,
                     GAsyncResult *res,
                     gpointer data)
{
  CcApplicationsPanel *self = data;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to flatpak permission store: %s",
                     error->message);
      return;
    }

  self->perm_store = proxy;

  update_panel (self, gtk_list_box_get_selected_row (self->sidebar_listbox));
}

static void
select_app (CcApplicationsPanel *self,
            const char *app_id)
{
  GList *children, *l;

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
  g_list_free (children);
}

static void
cc_applications_panel_set_property (GObject *object,
                                    guint property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
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
parse_globs (CcApplicationsPanel *self)
{
  g_autofree char *contents = NULL;

  self->globs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (g_file_get_contents ("/usr/share/mime/globs", &contents, NULL, NULL))
    {
      g_auto(GStrv) strv = NULL;
      int i;

      strv = g_strsplit (contents, "\n", 0);
      for (i = 0; strv[i]; i++)
        {
          g_auto(GStrv) parts = NULL;

          if (strv[i][0] == '#' || strv[i][0] == '\0')
            continue;

          parts = g_strsplit (strv[i], ":", 2);
          g_hash_table_insert (self->globs, g_strdup (parts[0]), g_strdup (parts[1]));
        }
    }
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

  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sidebar_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, title_label);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, header_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_label);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, builtin_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, integration_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, integration_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, notification);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, no_sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, device_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, device_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, usage_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, usage_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, data);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, cache);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, total);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, clear_cache_button);

  gtk_widget_class_bind_template_callback (widget_class, camera_cb);
  gtk_widget_class_bind_template_callback (widget_class, location_cb);
  gtk_widget_class_bind_template_callback (widget_class, microphone_cb);
  gtk_widget_class_bind_template_callback (widget_class, search_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_cb);
  gtk_widget_class_bind_template_callback (widget_class, privacy_link_cb);
  gtk_widget_class_bind_template_callback (widget_class, sound_cb);
  gtk_widget_class_bind_template_callback (widget_class, permission_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, handler_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, clear_cache_cb);
  gtk_widget_class_bind_template_callback (widget_class, storage_row_activated_cb);
}

static void
cc_applications_panel_init (CcApplicationsPanel *self)
{
  g_autoptr(GtkStyleProvider) provider = NULL;

  g_resources_register (cc_applications_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
  gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (provider),
                                       "/org/gnome/control-center/applications/cc-applications-panel.css");

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             provider,
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->sidebar_listbox), compare_rows, NULL, NULL);

  g_signal_connect (self->sidebar_listbox, "row-selected", G_CALLBACK (row_selected_cb), self);

  g_signal_connect (self->header_button, "clicked", G_CALLBACK (open_software_cb), self);

  self->location_settings = g_settings_new ("org.gnome.system.location");
  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  populate_applications (self);

  self->monitor = g_app_info_monitor_get ();
  g_signal_connect (self->monitor, "changed", G_CALLBACK (apps_changed), self);

  prepare_content (self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            self->cancellable,
                            on_perm_store_ready,
                            self);

  parse_globs (self);
}
