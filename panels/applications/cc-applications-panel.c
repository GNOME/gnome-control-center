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
 * - handler section
 * - handle cache async
 *
 * Missing in flatpak:
 * - background
 * - usb devices
 */

struct _CcApplicationsPanel
{
  CcPanel     parent;

  GtkListBox *sidebar_listbox;
  GtkWidget *header_button;
  GtkWidget *title_label;
  GAppInfoMonitor *monitor;

  GCancellable *cancellable;

  char *current_app_id;

  GDBusProxy *perm_store;
  GSettings *notification_settings;

  GtkListBox *stack;
  GtkWidget *permission_section;
  GtkWidget *permission_list;

  GtkWidget *camera;
  GtkWidget *location;
  GtkWidget *microphone;

  GtkWidget *permission_footer;

  GtkWidget *information_section;
  GtkWidget *information_list;
  GtkWidget *notification;
  GtkWidget *sound;

  GtkWidget *device_section;
  GtkWidget *device_list;

  GtkWidget *handler_section;
  GtkWidget *handler_list;
  GtkWidget *storage_section;
  GtkWidget *storage_list;
  GtkWidget *app;
  GtkWidget *data;
  GtkWidget *cache;
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

  g_clear_object (&self->cancellable);

  g_free (self->current_app_id);

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

static gboolean
get_notification_allowed (CcApplicationsPanel *self,
                          const char *app_id)
{
  if (self->notification_settings)
    {
      return g_settings_get_boolean (self->notification_settings, "enable");
    }
  else
    {
      g_auto(GStrv) perms = get_flatpak_permissions (self,
                                                     "notifications",
                                                     "notification",
                                                     app_id);
      return perms == NULL || strcmp (perms[0], "no") != 0;
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

static gboolean
get_device_allowed (CcApplicationsPanel *self,
                    const char *device,
                    const char *app_id)
{
  g_auto(GStrv) perms = NULL;

  perms = get_flatpak_permissions (self, "devices", device, app_id);

  return perms == NULL || strcmp (perms[0], "no") != 0;
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

static gboolean
get_location_allowed (CcApplicationsPanel *self,
                      const char *app_id)
{
  g_auto(GStrv) perms = NULL;

  perms = get_flatpak_permissions (self, "location", "location", app_id);

  return perms == NULL || strcmp (perms[0], "NONE") != 0;
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
update_app_row (CcActionRow *row,
                const char *app_id)
{
  g_autofree char *formatted_size = NULL;

  formatted_size = get_flatpak_app_size (app_id);
  g_object_set (row, "subtitle", formatted_size, NULL);
}

static void
update_dir_row (CcActionRow *row,
                const char *app_id,
                const char *subdir)
{
  guint64 size;
  g_autofree char *formatted_size = NULL;

  subdir = g_strdup (subdir);
  g_object_set_data_full (G_OBJECT (row), "subdir", (gpointer)subdir, g_free);

  size = calculate_dir_size (app_id, subdir);
  formatted_size = g_format_size (size);
  g_object_set (row,
                "subtitle", formatted_size,
                "enabled", size > 0,
                NULL);
}

static void
clear_cb (CcActionRow *row, CcApplicationsPanel *self)
{
  g_autoptr(GFile) dir = NULL;
  const char *subdir;

  if (self->current_app_id == NULL)
    return;

  subdir = (const char *) g_object_get_data (G_OBJECT (row), "subdir");
  dir = get_app_dir (self->current_app_id, subdir);
  file_remove_recursively (dir);
  update_dir_row (CC_ACTION_ROW (row), self->current_app_id, subdir);
}

static void
uninstall_cb (CcActionRow *row, CcApplicationsPanel *self)
{
  // FIXME: confirmation dialog ? undo notification ?

  uninstall_flatpak_app (self->current_app_id);
}

static char *
get_app_id (GAppInfo *info)
{
  char *app_id = g_strdup (g_app_info_get_id (info));

  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  return app_id;
}

static FlatpakInstalledRef *
find_flatpak_ref (const char *app_id)
{
  g_autoptr(FlatpakInstallation) inst = NULL;
  g_autoptr(GPtrArray) array = NULL;
  FlatpakInstalledRef *ref;
  int i;

  inst = flatpak_installation_new_user (NULL, NULL);
  ref = flatpak_installation_get_current_installed_app (inst, app_id, NULL, NULL);
  if (ref)
    return ref;

  array = flatpak_get_system_installations (NULL, NULL);
  for (i = 0; i < array->len; i++)
    {
      FlatpakInstallation *si = g_ptr_array_index (array, i);
      ref = flatpak_installation_get_current_installed_app (si, app_id, NULL, NULL);
      if (ref)
        return ref;
    }

  return NULL;
}

static void
add_static_permission_row (CcApplicationsPanel *self,
                           const char *title,
                           const char *subtitle)
{
  GtkWidget *row;

  row = g_object_new (CC_TYPE_INFO_ROW,
                      "title", title,
                      "subtitle", subtitle,
                      "info", _("Built-in"),
                      "visible", TRUE,
                      NULL);
  gtk_container_add (GTK_CONTAINER (self->permission_list), row);
}

static void
add_static_permissions (CcApplicationsPanel *self,
                        const char *app_id)
{
  g_autoptr(FlatpakInstalledRef) ref = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GKeyFile) keyfile = NULL;
  char **strv;
  char *str;
  
  ref = find_flatpak_ref (app_id);
  bytes = flatpak_installed_ref_load_metadata (ref, NULL, NULL);
  keyfile = g_key_file_new ();
  if (!g_key_file_load_from_data (keyfile,
                                  g_bytes_get_data (bytes, NULL),
                                  g_bytes_get_size (bytes),
                                  0, &error))
    {
      g_warning ("%s", error->message);
      return;
    }

  strv = g_key_file_get_string_list (keyfile, "Context", "sockets", NULL, NULL);
  if (strv && g_strv_contains ((const char * const*)strv, "system-bus"))
    add_static_permission_row (self, _("System Bus"), _("Has access to the system bus"));
  if (strv && g_strv_contains ((const char * const*)strv, "x11"))
    add_static_permission_row (self, _("X11"), _("Has access to the display server"));
  g_strfreev (strv);

  strv = g_key_file_get_string_list (keyfile, "Context", "devices", NULL, NULL);
  if (strv && g_strv_contains ((const char * const*)strv, "all"))
    add_static_permission_row (self, _("Devices"), _("Has full access to /dev"));
  g_strfreev (strv);

  strv = g_key_file_get_string_list (keyfile, "Context", "shared", NULL, NULL);
  if (strv && g_strv_contains ((const char * const*)strv, "network"))
    add_static_permission_row (self, _("Network"), _("Has network access"));
  g_strfreev (strv);

  strv = g_key_file_get_string_list (keyfile, "Context", "filesystems", NULL, NULL);
  if (strv && (g_strv_contains ((const char * const *)strv, "home") ||
               g_strv_contains ((const char * const *)strv, "home:rw")))
    add_static_permission_row (self, _("Home"), _("Read-write access"));
  else if (strv && g_strv_contains ((const char * const *)strv, "home:ro"))
    add_static_permission_row (self, _("Home"), _("Readonly access"));
  if (strv && (g_strv_contains ((const char * const *)strv, "host") ||
               g_strv_contains ((const char * const *)strv, "host:rw")))
    add_static_permission_row (self, _("Filesystem"), _("Full filesystem access"));
  else if (strv && g_strv_contains ((const char * const *)strv, "host:ro"))
    add_static_permission_row (self, _("Filesystem"), _("Readonly access"));
  g_strfreev (strv);

  str = g_key_file_get_string (keyfile, "Session Bus Policy", "ca.desrt.dconf", NULL);
  if (str && g_str_equal (str, "talk"))
    add_static_permission_row (self, _("Settings"), _("Can change settings"));
  g_free (str);
}

static void
remove_static_permissions (CcApplicationsPanel *self)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->permission_list));
  for (l = children; l; l = l->next)
    {
      if (CC_IS_INFO_ROW (l->data))
        gtk_widget_destroy (GTK_WIDGET (l->data));
    }
  g_list_free (children);
}

static void
update_permission_section (CcApplicationsPanel *self,
                           GAppInfo *info)
{
  g_autofree char *app_id = get_app_id (info);
  g_autofree char *permissions = NULL;

  if (!app_info_is_flatpak (info))
    {
      gtk_widget_hide (self->permission_section);
      return;
    }

  gtk_widget_show (self->permission_section);

  remove_static_permissions (self);
  add_static_permissions (self, app_id);

  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->camera), get_device_allowed (self, "camera", app_id));
  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->location), get_location_allowed (self, app_id));
  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->microphone), get_device_allowed (self, "microphone", app_id));
}

static void
update_information_section (CcApplicationsPanel *self,
                            GAppInfo *info)
{
  g_autofree char *app_id = get_app_id (info);

  if (app_info_is_flatpak (info))
    {
      g_clear_object (&self->notification_settings);

      gtk_widget_show (self->sound);
    }
  else
    {
      g_set_object (&self->notification_settings, get_notification_settings (app_id));

      gtk_widget_hide (self->sound);
    }

  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->notification), get_notification_allowed (self, app_id));
  cc_toggle_row_set_allowed (CC_TOGGLE_ROW (self->sound), get_device_allowed (self, "speakers", app_id));
}

static void
update_device_section (CcApplicationsPanel *self,
                       GAppInfo *info)
{
  // No usb portal yet
  gtk_widget_hide (self->device_section);
}

static void
add_handler_row (CcApplicationsPanel *self,
                 GAppInfo *info,
                 const char *type)
{
  g_autofree char *desc = NULL;
  CcActionRow *row;

  desc = g_content_type_get_description (type);
  row = cc_action_row_new ();
  cc_action_row_set_title (row, desc);
  cc_action_row_set_action (row, _("Remove"), FALSE);
  gtk_list_box_insert (GTK_LIST_BOX (self->handler_list), GTK_WIDGET (row), -1);
}

static void
update_handler_section (CcApplicationsPanel *self,
                        GAppInfo *info)
{
  const char **types;
  int i;
  g_autoptr(GHashTable) hash = NULL;

  container_remove_all (GTK_CONTAINER (self->handler_list));

  types = g_app_info_get_supported_types (info);
  if (types == NULL || types[0] == NULL)
    {
      gtk_widget_hide (self->handler_section);
      return;
    }

  gtk_widget_show (self->handler_section);

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

  for (i = 0; types[i]; i++)
    {
      char *ctype = g_content_type_from_mime_type (types[i]);
      if (g_hash_table_contains (hash, ctype))
        {
          g_free (ctype);
          continue;
        }
      g_hash_table_add (hash, ctype);
      add_handler_row (self, info, ctype);
    }
}

static void
update_storage_section (CcApplicationsPanel *self,
                        GAppInfo *info)
{
  if (app_info_is_flatpak (info))
    {
      g_autofree char *app_id = get_app_id (info);
      gtk_widget_show (self->storage_section);
      update_app_row (CC_ACTION_ROW (self->app), app_id);
      update_dir_row (CC_ACTION_ROW (self->data), app_id, "data");
      update_dir_row (CC_ACTION_ROW (self->cache), app_id, "cache");
    }
  else
    {
      gtk_widget_hide (self->storage_section);
    }
}

static void
update_panel (CcApplicationsPanel *self)
{
  GtkListBoxRow *row = gtk_list_box_get_selected_row (self->sidebar_listbox);
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
  update_information_section (self, info);
  update_device_section (self, info);
  update_handler_section (self, info);
  update_storage_section (self, info);

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

      if (!g_app_info_should_show (info))
        continue;

      row = GTK_WIDGET (cc_applications_row_new (info));
      gtk_list_box_insert (GTK_LIST_BOX (self->sidebar_listbox), row, -1);
    }

  g_list_free_full (infos, g_object_unref);
}

static void
prepare_content (CcApplicationsPanel *self)
{
  gtk_list_box_set_header_func (GTK_LIST_BOX (self->permission_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->information_list),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->handler_list),
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
  update_panel (self);
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

  update_panel (self);
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

  panel_class->get_sidebar_widget = cc_applications_panel_get_sidebar_widget;
  panel_class->get_title_widget = cc_applications_panel_get_title_widget;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-applications-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sidebar_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, title_label);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, header_button);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, camera);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, location);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, microphone);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, permission_footer);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, information_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, information_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, notification);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sound);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, device_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, device_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, handler_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_section);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, storage_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, app);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, data);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, cache);

  gtk_widget_class_bind_template_callback (widget_class, camera_cb);
  gtk_widget_class_bind_template_callback (widget_class, location_cb);
  gtk_widget_class_bind_template_callback (widget_class, microphone_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_cb);
  gtk_widget_class_bind_template_callback (widget_class, sound_cb);
  gtk_widget_class_bind_template_callback (widget_class, clear_cb);
  gtk_widget_class_bind_template_callback (widget_class, uninstall_cb);
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
}
