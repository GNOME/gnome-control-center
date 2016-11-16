/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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

#include <config.h>

#include "cc-info-panel.h"
#include "cc-info-resources.h"
#include "info-cleanup.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gsd-disk-space-helper.h"

/* Autorun options */
#define PREF_MEDIA_AUTORUN_NEVER                "autorun-never"
#define PREF_MEDIA_AUTORUN_X_CONTENT_START_APP  "autorun-x-content-start-app"
#define PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE     "autorun-x-content-ignore"
#define PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "autorun-x-content-open-folder"

#define CUSTOM_ITEM_ASK "cc-item-ask"
#define CUSTOM_ITEM_DO_NOTHING "cc-item-do-nothing"
#define CUSTOM_ITEM_OPEN_FOLDER "cc-item-open-folder"

#define MEDIA_HANDLING_SCHEMA "org.gnome.desktop.media-handling"

#define WID(w) (GtkWidget *) gtk_builder_get_object (self->priv->builder, w)

CC_PANEL_REGISTER (CcInfoPanel, cc_info_panel)

#define INFO_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_INFO_PANEL, CcInfoPanelPrivate))

typedef struct {
  /* Will be one or 2 GPU name strings, or "Unknown" */
  char *hardware_string;
} GraphicsData;

typedef struct 
{
  const char *content_type;
  const char *label;
  /* A pattern used to filter supported mime types
     when changing preferred applications. NULL
     means no other types should be changed */
  const char *extra_type_filter;
} DefaultAppData;

struct _CcInfoPanelPrivate
{
  GtkBuilder    *builder;
  GtkWidget     *extra_options_dialog;
  char          *gnome_version;
  char          *gnome_distributor;
  char          *gnome_date;

  GCancellable  *cancellable;

  /* Free space */
  GList         *primary_mounts;
  guint64        total_bytes;

  /* Media */
  GSettings     *media_settings;
  GtkWidget     *other_application_combo;

  GraphicsData  *graphics_data;
};

static void get_primary_disc_info_start (CcInfoPanel *self);

typedef struct
{
  char *major;
  char *minor;
  char *micro;
  char *distributor;
  char *date;
  char **current;
} VersionData;

static void
version_start_element_handler (GMarkupParseContext      *ctx,
                               const char               *element_name,
                               const char              **attr_names,
                               const char              **attr_values,
                               gpointer                  user_data,
                               GError                  **error)
{
  VersionData *data = user_data;
  if (g_str_equal (element_name, "platform"))
    data->current = &data->major;
  else if (g_str_equal (element_name, "minor"))
    data->current = &data->minor;
  else if (g_str_equal (element_name, "micro"))
    data->current = &data->micro;
  else if (g_str_equal (element_name, "distributor"))
    data->current = &data->distributor;
  else if (g_str_equal (element_name, "date"))
    data->current = &data->date;
  else
    data->current = NULL;
}

static void
version_end_element_handler (GMarkupParseContext      *ctx,
                             const char               *element_name,
                             gpointer                  user_data,
                             GError                  **error)
{
  VersionData *data = user_data;
  data->current = NULL;
}

static void
version_text_handler (GMarkupParseContext *ctx,
                      const char          *text,
                      gsize                text_len,
                      gpointer             user_data,
                      GError             **error)
{
  VersionData *data = user_data;
  if (data->current != NULL)
    *data->current = g_strstrip (g_strdup (text));
}

static gboolean
load_gnome_version (char **version,
                    char **distributor,
                    char **date)
{
  GMarkupParser version_parser = {
    version_start_element_handler,
    version_end_element_handler,
    version_text_handler,
    NULL,
    NULL,
  };
  GError              *error;
  GMarkupParseContext *ctx;
  char                *contents;
  gsize                length;
  VersionData         *data;
  gboolean             ret;

  ret = FALSE;

  error = NULL;
  if (!g_file_get_contents (DATADIR "/gnome/gnome-version.xml",
                            &contents,
                            &length,
                            &error))
    return FALSE;

  data = g_new0 (VersionData, 1);
  ctx = g_markup_parse_context_new (&version_parser, 0, data, NULL);

  if (!g_markup_parse_context_parse (ctx, contents, length, &error))
    {
      g_warning ("Invalid version file: '%s'", error->message);
    }
  else
    {
      if (version != NULL)
        *version = g_strdup_printf ("%s.%s.%s", data->major, data->minor, data->micro);
      if (distributor != NULL)
        *distributor = g_strdup (data->distributor);
      if (date != NULL)
        *date = g_strdup (data->date);

      ret = TRUE;
    }

  g_markup_parse_context_free (ctx);
  g_free (data->major);
  g_free (data->minor);
  g_free (data->micro);
  g_free (data->distributor);
  g_free (data->date);
  g_free (data);
  g_free (contents);

  return ret;
};

static void
graphics_data_free (GraphicsData *gdata)
{
  g_free (gdata->hardware_string);
  g_slice_free (GraphicsData, gdata);
}

static char *
get_renderer_from_session (void)
{
  GDBusProxy *session_proxy;
  GVariant *renderer_variant;
  char *renderer;
  GError *error = NULL;

  session_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                 NULL,
                                                 "org.gnome.SessionManager",
                                                 "/org/gnome/SessionManager",
                                                 "org.gnome.SessionManager",
                                                 NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to connect to create a proxy for org.gnome.SessionManager: %s",
                 error->message);
      g_error_free (error);
      return NULL;
    }

  renderer_variant = g_dbus_proxy_get_cached_property (session_proxy, "Renderer");
  g_object_unref (session_proxy);

  if (!renderer_variant)
    {
      g_warning ("Unable to retrieve org.gnome.SessionManager.Renderer property");
      return NULL;
    }

  renderer = info_cleanup (g_variant_get_string (renderer_variant, NULL));
  g_variant_unref (renderer_variant);

  return renderer;
}

static char *
get_renderer_from_helper (gboolean discrete_gpu)
{
  int status;
  char *argv[] = { GNOME_SESSION_DIR "/gnome-session-check-accelerated", NULL };
  char **envp = NULL;
  char *renderer = NULL;
  char *ret = NULL;

  if (discrete_gpu)
    {
      envp = g_get_environ ();
      envp = g_environ_setenv (envp, "DRI_PRIME", "1", TRUE);
    }

  if (!g_spawn_sync (NULL, (char **) argv, envp, 0, NULL, NULL, &renderer, NULL, &status, NULL))
    goto out;

  if (!g_spawn_check_exit_status (status, NULL))
    goto out;

  if (renderer == NULL || *renderer == '\0')
    goto out;

  ret = info_cleanup (renderer);

out:
  g_free (renderer);
  g_strfreev (envp);
  return ret;
}

static gboolean
has_dual_gpu (void)
{
  GDBusProxy *switcheroo_proxy;
  GVariant *dualgpu_variant;
  gboolean ret;
  GError *error = NULL;

  switcheroo_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    "net.hadess.SwitcherooControl",
                                                    "/net/hadess/SwitcherooControl",
                                                    "net.hadess.SwitcherooControl",
                                                    NULL, &error);
  if (switcheroo_proxy == NULL)
    {
      g_debug ("Unable to connect to create a proxy for net.hadess.SwitcherooControl: %s",
               error->message);
      g_error_free (error);
      return FALSE;
    }

  dualgpu_variant = g_dbus_proxy_get_cached_property (switcheroo_proxy, "HasDualGpu");
  g_object_unref (switcheroo_proxy);

  if (!dualgpu_variant)
    {
      g_debug ("Unable to retrieve net.hadess.SwitcherooControl.HasDualGpu property, the daemon is likely not running");
      return FALSE;
    }

  ret = g_variant_get_boolean (dualgpu_variant);
  g_variant_unref (dualgpu_variant);

  if (ret)
    g_debug ("Dual-GPU machine detected");

  return ret;
}

static GraphicsData *
get_graphics_data (void)
{
  GraphicsData *result;
  GdkDisplay *display;

  result = g_slice_new0 (GraphicsData);

  display = gdk_display_get_default ();

#if defined(GDK_WINDOWING_X11) || defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_X11_DISPLAY (display) ||
      GDK_IS_WAYLAND_DISPLAY (display))
    {
      char *discrete_renderer = NULL;
      char *renderer;

      renderer = get_renderer_from_session ();
      if (!renderer)
        renderer = get_renderer_from_helper (FALSE);
      if (has_dual_gpu ())
        discrete_renderer = get_renderer_from_helper (TRUE);
      if (!discrete_renderer)
        result->hardware_string = g_strdup (renderer);
      else
        result->hardware_string = g_strdup_printf ("%s / %s",
                                                   renderer,
                                                   discrete_renderer);
      g_free (renderer);
      g_free (discrete_renderer);
    }
#endif

  if (!result->hardware_string)
    result->hardware_string = g_strdup (_("Unknown"));

  return result;
}

static void
cc_info_panel_dispose (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  g_clear_object (&priv->builder);
  g_clear_pointer (&priv->graphics_data, graphics_data_free);
  g_clear_pointer (&priv->extra_options_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (cc_info_panel_parent_class)->dispose (object);
}

static void
cc_info_panel_finalize (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }
  g_free (priv->gnome_version);
  g_free (priv->gnome_date);
  g_free (priv->gnome_distributor);

  g_clear_object (&priv->media_settings);

  G_OBJECT_CLASS (cc_info_panel_parent_class)->finalize (object);
}

static void
cc_info_panel_class_init (CcInfoPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcInfoPanelPrivate));

  object_class->dispose = cc_info_panel_dispose;
  object_class->finalize = cc_info_panel_finalize;
}

static GHashTable*
get_os_info (void)
{
  GHashTable *hashtable;
  gchar *buffer;

  hashtable = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
      gchar **lines;
      gint i;

      lines = g_strsplit (buffer, "\n", -1);

      for (i = 0; lines[i] != NULL; i++)
        {
          gchar *delimiter, *key, *value;

          /* Initialize the hash table if needed */
          if (!hashtable)
            hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

          delimiter = strstr (lines[i], "=");
          value = NULL;
          key = NULL;

          if (delimiter != NULL)
            {
              gint size;

              key = g_strndup (lines[i], delimiter - lines[i]);

              /* Jump the '=' */
              delimiter += strlen ("=");

              /* Eventually jump the ' " ' character */
              if (g_str_has_prefix (delimiter, "\""))
                delimiter += strlen ("\"");

              size = strlen (delimiter);

              /* Don't consider the last ' " ' too */
              if (g_str_has_suffix (delimiter, "\""))
                size -= strlen ("\"");

              value = g_strndup (delimiter, size);

              g_hash_table_insert (hashtable, key, value);
            }
        }

      g_strfreev (lines);
      g_free (buffer);
    }

  return hashtable;
}

static char *
get_os_type (void)
{
  GHashTable *os_info;
  gchar *name, *result, *build_id;
  int bits;

  os_info = get_os_info ();

  if (!os_info)
    return NULL;

  name = g_hash_table_lookup (os_info, "PRETTY_NAME");
  build_id = g_hash_table_lookup (os_info, "BUILD_ID");

  if (GLIB_SIZEOF_VOID_P == 8)
    bits = 64;
  else
    bits = 32;

  if (build_id)
    {
      /* translators: This is the name of the OS, followed by the type
       * of architecture and the build id, for example:
       * "Fedora 18 (Spherical Cow) 64-bit (Build ID: xyz)" or
       * "Ubuntu (Oneric Ocelot) 32-bit (Build ID: jki)" */
      if (name)
        result = g_strdup_printf (_("%s %d-bit (Build ID: %s)"), name, bits, build_id);
      else
        result = g_strdup_printf (_("%d-bit (Build ID: %s)"), bits, build_id);
    }
  else
    {
      /* translators: This is the name of the OS, followed by the type
       * of architecture, for example:
       * "Fedora 18 (Spherical Cow) 64-bit" or "Ubuntu (Oneric Ocelot) 32-bit" */
      if (name)
        result = g_strdup_printf (_("%s %d-bit"), name, bits);
      else
        result = g_strdup_printf (_("%d-bit"), bits);
    }

  g_clear_pointer (&os_info, g_hash_table_destroy);

  return result;
}

static void
query_done (GFile        *file,
            GAsyncResult *res,
            CcInfoPanel  *self)
{
  GFileInfo *info;
  GError *error = NULL;

  self->priv->cancellable = NULL;
  info = g_file_query_filesystem_info_finish (file, res, &error);
  if (info != NULL)
    {
      self->priv->total_bytes += g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
      g_object_unref (info);
    }
  else
    {
      char *path;
      path = g_file_get_path (file);
      g_warning ("Failed to get filesystem free space for '%s': %s", path, error->message);
      g_free (path);
      g_error_free (error);
    }

  /* And onto the next element */
  get_primary_disc_info_start (self);
}

static void
get_primary_disc_info_start (CcInfoPanel *self)
{
  GUnixMountEntry *mount;
  GFile *file;

  if (self->priv->primary_mounts == NULL)
    {
      char *size;
      GtkWidget *widget;

      size = g_format_size (self->priv->total_bytes);
      widget = WID ("disk_label");
      gtk_label_set_text (GTK_LABEL (widget), size);
      g_free (size);

      return;
    }

  mount = self->priv->primary_mounts->data;
  self->priv->primary_mounts = g_list_remove (self->priv->primary_mounts, mount);
  file = g_file_new_for_path (g_unix_mount_get_mount_path (mount));
  g_unix_mount_free (mount);

  self->priv->cancellable = g_cancellable_new ();

  g_file_query_filesystem_info_async (file,
                                      G_FILE_ATTRIBUTE_FILESYSTEM_SIZE,
                                      0,
                                      self->priv->cancellable,
                                      (GAsyncReadyCallback) query_done,
                                      self);
  g_object_unref (file);
}

static void
get_primary_disc_info (CcInfoPanel *self)
{
  GList        *points;
  GList        *p;

  points = g_unix_mount_points_get (NULL);

  /* If we do not have /etc/fstab around, try /etc/mtab */
  if (points == NULL)
    points = g_unix_mounts_get (NULL);

  for (p = points; p != NULL; p = p->next)
    {
      GUnixMountEntry *mount = p->data;
      const char *mount_path;

      mount_path = g_unix_mount_get_mount_path (mount);

      if (gsd_should_ignore_unix_mount (mount) ||
          gsd_is_removable_mount (mount) ||
          g_str_has_prefix (mount_path, "/media/") ||
          g_str_has_prefix (mount_path, g_get_home_dir ()))
        {
          g_unix_mount_free (mount);
          continue;
        }

      self->priv->primary_mounts = g_list_prepend (self->priv->primary_mounts, mount);
    }
  g_list_free (points);

  get_primary_disc_info_start (self);
}

static char *
get_cpu_info (const glibtop_sysinfo *info)
{
  GHashTable    *counts;
  GString       *cpu;
  char          *ret;
  GHashTableIter iter;
  gpointer       key, value;
  int            i;
  int            j;

  counts = g_hash_table_new (g_str_hash, g_str_equal);

  /* count duplicates */
  for (i = 0; i != info->ncpu; ++i)
    {
      const char * const keys[] = { "model name", "cpu", "Processor" };
      char *model;
      int  *count;

      model = NULL;

      for (j = 0; model == NULL && j != G_N_ELEMENTS (keys); ++j)
        {
          model = g_hash_table_lookup (info->cpuinfo[i].values,
                                       keys[j]);
        }

      if (model == NULL)
          continue;

      count = g_hash_table_lookup (counts, model);
      if (count == NULL)
        g_hash_table_insert (counts, model, GINT_TO_POINTER (1));
      else
        g_hash_table_replace (counts, model, GINT_TO_POINTER (GPOINTER_TO_INT (count) + 1));
    }

  cpu = g_string_new (NULL);
  g_hash_table_iter_init (&iter, counts);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      char *cleanedup;
      int   count;

      count = GPOINTER_TO_INT (value);
      cleanedup = info_cleanup ((const char *) key);
      if (count > 1)
        g_string_append_printf (cpu, "%s \303\227 %d ", cleanedup, count);
      else
        g_string_append_printf (cpu, "%s ", cleanedup);
      g_free (cleanedup);
    }

  g_hash_table_destroy (counts);

  ret = g_string_free (cpu, FALSE);

  return ret;
}

static void
on_section_changed (GtkTreeSelection  *selection,
                    gpointer           data)
{
  CcInfoPanel *self = CC_INFO_PANEL (data);
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *path;
  gint *indices;
  int index;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path = gtk_tree_model_get_path (model, &iter);

  indices = gtk_tree_path_get_indices (path);
  index = indices[0];

  if (index >= 0)
    {
      g_object_set (G_OBJECT (WID ("notebook")),
                    "page", index, NULL);
    }

  gtk_tree_path_free (path);
}

static void
move_one_up (GtkWidget *table,
	     GtkWidget *child)
{
  int top_attach, bottom_attach;

  gtk_container_child_get (GTK_CONTAINER (table),
                           child,
                           "top-attach", &top_attach,
                           "bottom-attach", &bottom_attach,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (table),
                           child,
                           "top-attach", top_attach - 1,
                           "bottom-attach", bottom_attach - 1,
                           NULL);
}

static struct {
  const char *id;
  const char *display;
} const virt_tech[] = {
  { "kvm", "KVM" },
  { "qemu", "QEmu" },
  { "vmware", "VMware" },
  { "microsoft", "Microsoft" },
  { "oracle", "Oracle" },
  { "xen", "Xen" },
  { "bochs", "Bochs" },
  { "chroot", "chroot" },
  { "openvz", "OpenVZ" },
  { "lxc", "LXC" },
  { "lxc-libvirt", "LXC (libvirt)" },
  { "systemd-nspawn", "systemd (nspawn)" }
};

static void
set_virtualization_label (CcInfoPanel  *self,
			  const char   *virt)
{
  const char *display_name;
  GtkWidget *widget;
  guint i;

  if (virt == NULL || *virt == '\0')
  {
    gtk_widget_hide (WID ("virt_type_label"));
    gtk_widget_hide (WID ("label18"));
    move_one_up (WID("table1"), WID("label8"));
    move_one_up (WID("table1"), WID("disk_label"));
    return;
  }

  display_name = NULL;
  for (i = 0; i < G_N_ELEMENTS (virt_tech); i++)
    {
      if (g_str_equal (virt_tech[i].id, virt))
        {
          display_name = _(virt_tech[i].display);
          break;
        }
    }

  widget = WID ("virt_type_label");
  gtk_label_set_text (GTK_LABEL (widget), display_name ? display_name : virt);
}

static void
info_panel_setup_virt (CcInfoPanel  *self)
{
  GError *error = NULL;
  GDBusProxy *systemd_proxy;
  GVariant *variant;
  GVariant *inner;
  char *str;

  str = NULL;

  systemd_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                 NULL,
                                                 "org.freedesktop.systemd1",
                                                 "/org/freedesktop/systemd1",
                                                 "org.freedesktop.systemd1",
                                                 NULL,
                                                 &error);

  if (systemd_proxy == NULL)
    {
      g_debug ("systemd not available, bailing: %s", error->message);
      g_error_free (error);
      goto bail;
    }

  variant = g_dbus_proxy_call_sync (systemd_proxy,
                                    "org.freedesktop.DBus.Properties.Get",
                                    g_variant_new ("(ss)", "org.freedesktop.systemd1.Manager", "Virtualization"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);
  if (variant == NULL)
    {
      g_debug ("Failed to get property '%s': %s", "Virtualization", error->message);
      g_error_free (error);
      g_object_unref (systemd_proxy);
      goto bail;
    }

  g_variant_get (variant, "(v)", &inner);
  str = g_variant_dup_string (inner, NULL);
  g_variant_unref (variant);

  g_object_unref (systemd_proxy);

bail:
  set_virtualization_label (self, str);
  g_free (str);
}

static void
default_app_changed (GtkAppChooserButton *button,
                     CcInfoPanel         *self)
{
  GAppInfo *info;
  GError *error = NULL;
  DefaultAppData *app_data;
  int i;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (button));
  app_data = g_object_get_data (G_OBJECT (button), "cc-default-app-data");

  if (g_app_info_set_as_default_for_type (info, app_data->content_type, &error) == FALSE)
    {
      g_warning ("Failed to set '%s' as the default application for '%s': %s",
                 g_app_info_get_name (info), app_data->content_type, error->message);
      g_error_free (error);
      error = NULL;
    }
  else
    {
      g_debug ("Set '%s' as the default handler for '%s'",
               g_app_info_get_name (info), app_data->content_type);
    }

  if (app_data->extra_type_filter)
    {
      const char *const *mime_types;
      GPatternSpec *pattern;

      pattern = g_pattern_spec_new (app_data->extra_type_filter);
      mime_types = g_app_info_get_supported_types (info);

      for (i = 0; mime_types && mime_types[i]; i++)
        {
          if (!g_pattern_match_string (pattern, mime_types[i]))
            continue;

          if (g_app_info_set_as_default_for_type (info, mime_types[i], &error) == FALSE)
            {
              g_warning ("Failed to set '%s' as the default application for secondary "
                         "content type '%s': %s",
                         g_app_info_get_name (info), mime_types[i], error->message);
              g_error_free (error);
            }
          else
            {
              g_debug ("Set '%s' as the default handler for '%s'",
              g_app_info_get_name (info), mime_types[i]);
            }
        }

      g_pattern_spec_free (pattern);
    }

  g_object_unref (info);
}

static void
info_panel_setup_default_app (CcInfoPanel    *self,
                              DefaultAppData *data,
                              guint           left_attach,
                              guint           top_attach)
{
  GtkWidget *button;
  GtkWidget *grid;
  GtkWidget *label;

  grid = WID ("default_apps_grid");

  button = gtk_app_chooser_button_new (data->content_type);
  g_object_set_data (G_OBJECT (button), "cc-default-app-data", data);

  gtk_app_chooser_button_set_show_default_item (GTK_APP_CHOOSER_BUTTON (button), TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, left_attach, top_attach,
                   1, 1);
  g_signal_connect (G_OBJECT (button), "changed",
                    G_CALLBACK (default_app_changed), self);
  gtk_widget_show (button);

  label = WID(data->label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
}

static DefaultAppData preferred_app_infos[] = {
  /* for web, we need to support text/html,
     application/xhtml+xml and x-scheme-handler/https,
     hence the "*" pattern
  */
  { "x-scheme-handler/http", "web-label", "*" },
  { "x-scheme-handler/mailto", "mail-label", NULL },
  { "text/calendar", "calendar-label", NULL },
  { "audio/x-vorbis+ogg", "music-label", "audio/*" },
  { "video/x-ogm+ogg", "video-label", "video/*" },
  { "image/jpeg", "photos-label", "image/*" }
};

static void
info_panel_setup_default_apps (CcInfoPanel  *self)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS(preferred_app_infos); i++)
    {
      info_panel_setup_default_app (self, &preferred_app_infos[i],
                                    1, i);
    }
}

static char **
remove_elem_from_str_array (char **v,
                            const char *s)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  for (idx = 0; v[idx] != NULL; idx++) {
    if (g_strcmp0 (v[idx], s) == 0) {
      continue;
    }

    g_ptr_array_add (array, v[idx]);
  }

  g_ptr_array_add (array, NULL);

  g_free (v);

  return (char **) g_ptr_array_free (array, FALSE);
}

static char **
add_elem_to_str_array (char **v,
                       const char *s)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  for (idx = 0; v[idx] != NULL; idx++) {
    g_ptr_array_add (array, v[idx]);
  }

  g_ptr_array_add (array, g_strdup (s));
  g_ptr_array_add (array, NULL);

  g_free (v);

  return (char **) g_ptr_array_free (array, FALSE);
}

static int
media_panel_g_strv_find (char **strv,
                         const char *find_me)
{
  guint index;

  g_return_val_if_fail (find_me != NULL, -1);

  for (index = 0; strv[index] != NULL; ++index) {
    if (g_strcmp0 (strv[index], find_me) == 0) {
      return index;
    }
  }

  return -1;
}

static void
autorun_get_preferences (CcInfoPanel *self,
                         const char *x_content_type,
                         gboolean *pref_start_app,
                         gboolean *pref_ignore,
                         gboolean *pref_open_folder)
{
  char **x_content_start_app;
  char **x_content_ignore;
  char **x_content_open_folder;

  g_return_if_fail (pref_start_app != NULL);
  g_return_if_fail (pref_ignore != NULL);
  g_return_if_fail (pref_open_folder != NULL);

  *pref_start_app = FALSE;
  *pref_ignore = FALSE;
  *pref_open_folder = FALSE;
  x_content_start_app = g_settings_get_strv (self->priv->media_settings,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->priv->media_settings,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->priv->media_settings,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);
  if (x_content_start_app != NULL) {
    *pref_start_app = media_panel_g_strv_find (x_content_start_app, x_content_type) != -1;
  }
  if (x_content_ignore != NULL) {
    *pref_ignore = media_panel_g_strv_find (x_content_ignore, x_content_type) != -1;
  }
  if (x_content_open_folder != NULL) {
    *pref_open_folder = media_panel_g_strv_find (x_content_open_folder, x_content_type) != -1;
  }
  g_strfreev (x_content_ignore);
  g_strfreev (x_content_start_app);
  g_strfreev (x_content_open_folder);
}

static void
autorun_set_preferences (CcInfoPanel *self,
                         const char *x_content_type,
                         gboolean pref_start_app,
                         gboolean pref_ignore,
                         gboolean pref_open_folder)
{
  char **x_content_start_app;
  char **x_content_ignore;
  char **x_content_open_folder;

  g_assert (x_content_type != NULL);

  x_content_start_app = g_settings_get_strv (self->priv->media_settings,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->priv->media_settings,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->priv->media_settings,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);

  x_content_start_app = remove_elem_from_str_array (x_content_start_app, x_content_type);
  if (pref_start_app) {
    x_content_start_app = add_elem_to_str_array (x_content_start_app, x_content_type);
  }
  g_settings_set_strv (self->priv->media_settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_START_APP, (const gchar * const*) x_content_start_app);

  x_content_ignore = remove_elem_from_str_array (x_content_ignore, x_content_type);
  if (pref_ignore) {
    x_content_ignore = add_elem_to_str_array (x_content_ignore, x_content_type);
  }
  g_settings_set_strv (self->priv->media_settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE, (const gchar * const*) x_content_ignore);

  x_content_open_folder = remove_elem_from_str_array (x_content_open_folder, x_content_type);
  if (pref_open_folder) {
    x_content_open_folder = add_elem_to_str_array (x_content_open_folder, x_content_type);
  }
  g_settings_set_strv (self->priv->media_settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER, (const gchar * const*) x_content_open_folder);

  g_strfreev (x_content_open_folder);
  g_strfreev (x_content_ignore);
  g_strfreev (x_content_start_app);

}

static void
custom_item_activated_cb (GtkAppChooserButton *button,
                          const gchar *item,
                          gpointer user_data)
{
  CcInfoPanel *self = user_data;
  gchar *content_type;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (button));

  if (g_strcmp0 (item, CUSTOM_ITEM_ASK) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, FALSE, FALSE);
  } else if (g_strcmp0 (item, CUSTOM_ITEM_OPEN_FOLDER) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, FALSE, TRUE);
  } else if (g_strcmp0 (item, CUSTOM_ITEM_DO_NOTHING) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, TRUE, FALSE);
  }

  g_free (content_type);
}

static void
combo_box_changed_cb (GtkComboBox *combo_box,
                      gpointer user_data)
{
  CcInfoPanel *self = user_data;
  GAppInfo *info;
  gchar *content_type;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (combo_box));

  if (info == NULL)
    return;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (combo_box));
  autorun_set_preferences (self, content_type,
                           TRUE, FALSE, FALSE);
  g_app_info_set_as_default_for_type (info, content_type, NULL);

  g_object_unref (info);
  g_free (content_type);
}

static void
prepare_combo_box (CcInfoPanel *self,
                   GtkWidget *combo_box,
                   const gchar *heading)
{
  GtkAppChooserButton *app_chooser = GTK_APP_CHOOSER_BUTTON (combo_box);
  gboolean pref_ask;
  gboolean pref_start_app;
  gboolean pref_ignore;
  gboolean pref_open_folder;
  GAppInfo *info;
  gchar *content_type;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (app_chooser));

  /* fetch preferences for this content type */
  autorun_get_preferences (self, content_type,
                           &pref_start_app, &pref_ignore, &pref_open_folder);
  pref_ask = !pref_start_app && !pref_ignore && !pref_open_folder;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (combo_box));

  /* append the separator only if we have >= 1 apps in the chooser */
  if (info != NULL) {
    gtk_app_chooser_button_append_separator (app_chooser);
    g_object_unref (info);
  }

  gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_ASK,
                                             _("Ask what to do"),
                                             NULL);

  gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_DO_NOTHING,
                                             _("Do nothing"),
                                             NULL);

  gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_OPEN_FOLDER,
                                             _("Open folder"),
                                             NULL);

  gtk_app_chooser_button_set_show_dialog_item (app_chooser, TRUE);
  gtk_app_chooser_button_set_heading (app_chooser, _(heading));

  if (pref_ask) {
    gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_ASK);
  } else if (pref_ignore) {
    gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_DO_NOTHING);
  } else if (pref_open_folder) {
    gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_OPEN_FOLDER);
  }

  g_signal_connect (app_chooser, "changed",
                    G_CALLBACK (combo_box_changed_cb), self);
  g_signal_connect (app_chooser, "custom-item-activated",
                    G_CALLBACK (custom_item_activated_cb), self);

  g_free (content_type);
}

static void
other_type_combo_box_changed (GtkComboBox *combo_box,
                              CcInfoPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *x_content_type;
  GtkWidget *action_container;
  GtkWidget *action_label;

  x_content_type = NULL;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
    return;
  }

  model = gtk_combo_box_get_model (combo_box);
  if (model == NULL) {
    return;
  }

  gtk_tree_model_get (model, &iter,
                      1, &x_content_type,
                      -1);

  action_container = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                         "media_other_action_container"));
  if (self->priv->other_application_combo != NULL) {
    gtk_widget_destroy (self->priv->other_application_combo);
  }

  self->priv->other_application_combo = gtk_app_chooser_button_new (x_content_type);
  gtk_box_pack_start (GTK_BOX (action_container), self->priv->other_application_combo, TRUE, TRUE, 0);
  prepare_combo_box (self, self->priv->other_application_combo, NULL);
  gtk_widget_show (self->priv->other_application_combo);

  action_label = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                     "media_other_action_label"));

  gtk_label_set_mnemonic_widget (GTK_LABEL (action_label), self->priv->other_application_combo);

  g_free (x_content_type);
}

static void
on_extra_options_dialog_response (GtkWidget    *dialog,
                                  int           response,
                                  CcInfoPanel *self)
{
  gtk_widget_hide (dialog);

  if (self->priv->other_application_combo != NULL) {
    gtk_widget_destroy (self->priv->other_application_combo);
    self->priv->other_application_combo = NULL;
  }
}

static void
on_extra_options_button_clicked (GtkWidget    *button,
                                 CcInfoPanel *self)
{
  GtkWidget *dialog;
  GtkWidget *combo_box;

  dialog = self->priv->extra_options_dialog;
  combo_box = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_other_type_combobox"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Other Media"));
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_extra_options_dialog_response),
                    self);
  g_signal_connect (dialog,
                    "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);
  /* update other_application_combo */
  other_type_combo_box_changed (GTK_COMBO_BOX (combo_box), self);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
info_panel_setup_media (CcInfoPanel *self)
{
  guint n;
  GList *l, *content_types;
  GtkWidget *other_type_combo_box;
  GtkWidget *extras_button;
  GtkListStore *other_type_list_store;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GtkBuilder *builder = self->priv->builder;

  struct {
    const gchar *widget_name;
    const gchar *content_type;
    const gchar *heading;
  } const defs[] = {
    { "media_audio_cdda_combobox", "x-content/audio-cdda", N_("Select an application for audio CDs") },
    { "media_video_dvd_combobox", "x-content/video-dvd", N_("Select an application for video DVDs") },
    { "media_music_player_combobox", "x-content/audio-player", N_("Select an application to run when a music player is connected") },
    { "media_dcf_combobox", "x-content/image-dcf", N_("Select an application to run when a camera is connected") },
    { "media_software_combobox", "x-content/unix-software", N_("Select an application for software CDs") },
  };

  struct {
    const gchar *content_type;
    const gchar *description;
  } const other_defs[] = {
    /* translators: these strings are duplicates of shared-mime-info
     * strings, just here to fix capitalization of the English originals.
     * If the shared-mime-info translation works for your language,
     * simply leave these untranslated.
     */
    { "x-content/audio-dvd", N_("audio DVD") },
    { "x-content/blank-bd", N_("blank Blu-ray disc") },
    { "x-content/blank-cd", N_("blank CD disc") },
    { "x-content/blank-dvd", N_("blank DVD disc") },
    { "x-content/blank-hddvd", N_("blank HD DVD disc") },
    { "x-content/video-bluray", N_("Blu-ray video disc") },
    { "x-content/ebook-reader", N_("e-book reader") },
    { "x-content/video-hddvd", N_("HD DVD video disc") },
    { "x-content/image-picturecd", N_("Picture CD") },
    { "x-content/video-svcd", N_("Super Video CD") },
    { "x-content/video-vcd", N_("Video CD") },
    { "x-content/win32-software", N_("Windows software") },
  };

  for (n = 0; n < G_N_ELEMENTS (defs); n++) {
    prepare_combo_box (self,
                       GTK_WIDGET (gtk_builder_get_object (builder, defs[n].widget_name)),
                       defs[n].heading);
  }

  other_type_combo_box = GTK_WIDGET (gtk_builder_get_object (builder, "media_other_type_combobox"));

  other_type_list_store = gtk_list_store_new (2,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (other_type_list_store),
                                        1, GTK_SORT_ASCENDING);


  content_types = g_content_types_get_registered ();

  for (l = content_types; l != NULL; l = l->next) {
    char *content_type = l->data;
    char *description = NULL;

    if (!g_str_has_prefix (content_type, "x-content/"))
      continue;

    for (n = 0; n < G_N_ELEMENTS (defs); n++) {
      if (g_content_type_is_a (content_type, defs[n].content_type)) {
        goto skip;
      }
    }

    for (n = 0; n < G_N_ELEMENTS (other_defs); n++) {
       if (strcmp (content_type, other_defs[n].content_type) == 0) {
         const gchar *s = other_defs[n].description;
         if (s == _(s))
           description = g_content_type_get_description (content_type);
         else
           description = g_strdup (_(s));

         break;
       }
    }

    if (description == NULL) {
      g_debug ("Content type '%s' is missing from the info panel", content_type);
      description = g_content_type_get_description (content_type);
    }

    gtk_list_store_append (other_type_list_store, &iter);

    gtk_list_store_set (other_type_list_store, &iter,
                        0, description,
                        1, content_type,
                        -1);
    g_free (description);
  skip:
    ;
  }

  g_list_free_full (content_types, g_free);

  gtk_combo_box_set_model (GTK_COMBO_BOX (other_type_combo_box),
                           GTK_TREE_MODEL (other_type_list_store));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
                                  "text", 0,
                                  NULL);

  g_signal_connect (other_type_combo_box,
                    "changed",
                    G_CALLBACK (other_type_combo_box_changed),
                    self);

  gtk_combo_box_set_active (GTK_COMBO_BOX (other_type_combo_box), 0);

  extras_button = GTK_WIDGET (gtk_builder_get_object (builder, "extra_options_button"));
  g_signal_connect (extras_button,
                    "clicked",
                    G_CALLBACK (on_extra_options_button_clicked),
                    self);

  g_settings_bind (self->priv->media_settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   gtk_builder_get_object (self->priv->builder, "media_autorun_never_checkbutton"),
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->priv->media_settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_handling_vbox")),
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static void
info_panel_setup_selector (CcInfoPanel  *self)
{
  GtkTreeView *view;
  GtkListStore *model;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  int section_name_column = 0;

  view = GTK_TREE_VIEW (WID ("overview_treeview"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  model = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (view, GTK_TREE_MODEL (model));
  g_object_unref (model);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_padding (renderer, 4, 4);
  g_object_set (renderer,
                "width-chars", 20,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  column = gtk_tree_view_column_new_with_attributes (_("Section"),
                                                     renderer,
                                                     "text", section_name_column,
                                                     NULL);
  gtk_tree_view_append_column (view, column);


  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Overview"),
                      -1);
  gtk_tree_selection_select_iter (selection, &iter);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Default Applications"),
                      -1);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Removable Media"),
                      -1);

  g_signal_connect (selection, "changed",
                    G_CALLBACK (on_section_changed), self);
  on_section_changed (selection, self);

  gtk_widget_show_all (GTK_WIDGET (view));
}

static void
info_panel_setup_overview (CcInfoPanel  *self)
{
  GtkWidget  *widget;
  gboolean    res;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  char       *text;

  res = load_gnome_version (&self->priv->gnome_version,
                            &self->priv->gnome_distributor,
                            &self->priv->gnome_date);
  if (res)
    {
      widget = WID ("version_label");
      text = g_strdup_printf (_("Version %s"), self->priv->gnome_version);
      gtk_label_set_text (GTK_LABEL (widget), text);
      g_free (text);
    }

  glibtop_get_mem (&mem);
  text = g_format_size_full (mem.total, G_FORMAT_SIZE_IEC_UNITS);
  widget = WID ("memory_label");
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  info = glibtop_get_sysinfo ();

  widget = WID ("processor_label");
  text = get_cpu_info (info);
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID ("os_type_label");
  text = get_os_type ();
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  get_primary_disc_info (self);

  widget = WID ("graphics_label");
  gtk_label_set_markup (GTK_LABEL (widget), self->priv->graphics_data->hardware_string);

  widget = WID ("info_vbox");
  gtk_container_add (GTK_CONTAINER (self), widget);
}

static gboolean
does_gnome_software_exist (void)
{
  return g_file_test (BINDIR "/gnome-software", G_FILE_TEST_EXISTS);
}

static gboolean
does_gpk_update_viewer_exist (void)
{
  return g_file_test (BINDIR "/gpk-update-viewer", G_FILE_TEST_EXISTS);
}

static void
on_updates_button_clicked (GtkWidget   *widget,
                           CcInfoPanel *self)
{
  GError *error = NULL;
  gboolean ret;
  gchar **argv;

  argv = g_new0 (gchar *, 3);
  if (does_gnome_software_exist ())
    {
      argv[0] = g_build_filename (BINDIR, "gnome-software", NULL);
      argv[1] = g_strdup_printf ("--mode=updates");
    }
  else
    {
      argv[0] = g_build_filename (BINDIR, "gpk-update-viewer", NULL);
    }
  ret = g_spawn_async (NULL, argv, NULL, 0, NULL, NULL, NULL, &error);
  if (!ret)
    {
      g_warning ("Failed to spawn %s: %s", argv[0], error->message);
      g_error_free (error);
    }
  g_strfreev (argv);
}

static void
cc_info_panel_init (CcInfoPanel *self)
{
  GError *error = NULL;
  GtkWidget *widget;

  self->priv = INFO_PANEL_PRIVATE (self);
  g_resources_register (cc_info_get_resource ());

  self->priv->builder = gtk_builder_new ();

  self->priv->media_settings = g_settings_new (MEDIA_HANDLING_SCHEMA);

  if (gtk_builder_add_from_resource (self->priv->builder,
                                     "/org/gnome/control-center/info/info.ui",
                                     &error) == 0)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  self->priv->extra_options_dialog = WID ("extra_options_dialog");

  self->priv->graphics_data = get_graphics_data ();

  widget = WID ("updates_button");
  if (does_gnome_software_exist () || does_gpk_update_viewer_exist ())
    {
      g_signal_connect (widget, "clicked", G_CALLBACK (on_updates_button_clicked), self);
    }
  else
    {
      gtk_widget_destroy (widget);
    }

  info_panel_setup_selector (self);
  info_panel_setup_overview (self);
  info_panel_setup_default_apps (self);
  info_panel_setup_media (self);
  info_panel_setup_virt (self);
}
