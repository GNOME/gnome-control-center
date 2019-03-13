/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
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

#include "cc-hostname-entry.h"

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

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gsd-disk-space-helper.h"

#include "cc-info-overview-panel.h"


typedef struct {
  /* Will be one or 2 GPU name strings, or "Unknown" */
  char *hardware_string;
} GraphicsData;

typedef struct
{
  GtkWidget      *system_image;
  GtkWidget      *version_label;
  GtkWidget      *name_entry;
  GtkWidget      *memory_label;
  GtkWidget      *processor_label;
  GtkWidget      *os_name_label;
  GtkWidget      *os_type_label;
  GtkWidget      *disk_label;
  GtkWidget      *graphics_label;
  GtkWidget      *virt_type_label;
  GtkWidget      *updates_button;

  /* Virtualisation labels */
  GtkWidget      *label8;
  GtkWidget      *grid1;
  GtkWidget      *label18;

  char           *gnome_version;
  char           *gnome_distributor;
  char           *gnome_date;

  GCancellable   *cancellable;

  /* Free space */
  GList          *primary_mounts;
  guint64         total_bytes;

  GraphicsData   *graphics_data;
} CcInfoOverviewPanelPrivate;

struct _CcInfoOverviewPanel
{
 CcPanel parent_instance;

  /*< private >*/
 CcInfoOverviewPanelPrivate *priv;
};

static void get_primary_disc_info_start (CcInfoOverviewPanel *self);

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
version_data_free (VersionData *data)
{
  g_free (data->major);
  g_free (data->minor);
  g_free (data->micro);
  g_free (data->distributor);
  g_free (data->date);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (VersionData, version_data_free);

G_DEFINE_TYPE_WITH_PRIVATE (CcInfoOverviewPanel, cc_info_overview_panel, CC_TYPE_PANEL)

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
    {
      g_autofree char *stripped = NULL;

      stripped = g_strstrip (g_strdup (text));
      g_free (*data->current);
      *data->current = g_strdup (stripped);
    }
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
  g_autoptr(GError) error = NULL;
  g_autoptr(GMarkupParseContext) ctx = NULL;
  g_autofree char *contents = NULL;
  gsize length;
  g_autoptr(VersionData) data = NULL;

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

      return TRUE;
    }

  return FALSE;
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
  g_autoptr(GDBusProxy) session_proxy = NULL;
  g_autoptr(GVariant) renderer_variant = NULL;
  char *renderer;
  g_autoptr(GError) error = NULL;

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
      return NULL;
    }

  renderer_variant = g_dbus_proxy_get_cached_property (session_proxy, "Renderer");

  if (!renderer_variant)
    {
      g_warning ("Unable to retrieve org.gnome.SessionManager.Renderer property");
      return NULL;
    }

  renderer = info_cleanup (g_variant_get_string (renderer_variant, NULL));

  return renderer;
}

static char *
get_renderer_from_helper (gboolean discrete_gpu)
{
  int status;
  char *argv[] = { GNOME_SESSION_DIR "/gnome-session-check-accelerated", NULL };
  g_auto(GStrv) envp = NULL;
  g_autofree char *renderer = NULL;
  g_autoptr(GError) error = NULL;

  if (discrete_gpu)
    {
      envp = g_get_environ ();
      envp = g_environ_setenv (envp, "DRI_PRIME", "1", TRUE);
    }

  if (!g_spawn_sync (NULL, (char **) argv, envp, 0, NULL, NULL, &renderer, NULL, &status, &error))
    {
      g_debug ("Failed to get %s GPU: %s",
               discrete_gpu ? "discrete" : "integrated",
               error->message);
      return NULL;
    }

  if (!g_spawn_check_exit_status (status, NULL))
    return NULL;

  if (renderer == NULL || *renderer == '\0')
    return NULL;

  return info_cleanup (renderer);
}

static gboolean
has_dual_gpu (void)
{
  g_autoptr(GDBusProxy) switcheroo_proxy = NULL;
  g_autoptr(GVariant) dualgpu_variant = NULL;
  gboolean ret;
  g_autoptr(GError) error = NULL;

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
      return FALSE;
    }

  dualgpu_variant = g_dbus_proxy_get_cached_property (switcheroo_proxy, "HasDualGpu");

  if (!dualgpu_variant)
    {
      g_debug ("Unable to retrieve net.hadess.SwitcherooControl.HasDualGpu property, the daemon is likely not running");
      return FALSE;
    }

  ret = g_variant_get_boolean (dualgpu_variant);

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
  gboolean x11_or_wayland = FALSE;
#ifdef GDK_WINDOWING_X11
  x11_or_wayland = GDK_IS_X11_DISPLAY (display);
#endif
#ifdef GDK_WINDOWING_WAYLAND
  x11_or_wayland = x11_or_wayland || GDK_IS_WAYLAND_DISPLAY (display);
#endif

  if (x11_or_wayland)
    {
      g_autofree char *discrete_renderer = NULL;
      g_autofree char *renderer = NULL;

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
    }
#endif

  if (!result->hardware_string)
    result->hardware_string = g_strdup (_("Unknown"));

  return result;
}

static GHashTable*
get_os_info (void)
{
  GHashTable *hashtable;
  g_autofree gchar *buffer = NULL;

  hashtable = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
      g_auto(GStrv) lines = NULL;
      gint i;

      lines = g_strsplit (buffer, "\n", -1);

      for (i = 0; lines[i] != NULL; i++)
        {
          gchar *delimiter;

          /* Initialize the hash table if needed */
          if (!hashtable)
            hashtable = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

          delimiter = strstr (lines[i], "=");

          if (delimiter != NULL)
            {
              gint size;
              gchar *key, *value;

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
    }

  return hashtable;
}

static char *
get_os_name (void)
{
  GHashTable *os_info;
  gchar *name, *version_id, *pretty_name, *build_id;
  gchar *result = NULL;
  g_autofree gchar *name_version = NULL;

  os_info = get_os_info ();

  if (!os_info)
    return NULL;

  name = g_hash_table_lookup (os_info, "NAME");
  version_id = g_hash_table_lookup (os_info, "VERSION_ID");
  pretty_name = g_hash_table_lookup (os_info, "PRETTY_NAME");
  build_id = g_hash_table_lookup (os_info, "BUILD_ID");

  if (pretty_name)
    name_version = g_strdup (pretty_name);
  else if (name && version_id)
    name_version = g_strdup_printf ("%s %s", name, version_id);
  else
    name_version = g_strdup (_("Unknown"));

  if (build_id)
    {
      /* translators: This is the name of the OS, followed by the build ID, for
       * example:
       * "Fedora 25 (Workstation Edition); Build ID: xyz" or
       * "Ubuntu 16.04 LTS; Build ID: jki" */
      result = g_strdup_printf (_("%s; Build ID: %s"), name_version, build_id);
    }
  else
    {
      result = g_strdup (name_version);
    }

  g_clear_pointer (&os_info, g_hash_table_destroy);

  return result;
}

static char *
get_os_type (void)
{
  if (GLIB_SIZEOF_VOID_P == 8)
    /* translators: This is the type of architecture for the OS */
    return g_strdup_printf (_("64-bit"));
  else
    /* translators: This is the type of architecture for the OS */
    return g_strdup_printf (_("32-bit"));
}

static void
query_done (GFile               *file,
            GAsyncResult        *res,
            CcInfoOverviewPanel *self)
{
  CcInfoOverviewPanelPrivate *priv;
  g_autoptr(GFileInfo) info = NULL;
  g_autoptr(GError) error = NULL;

  info = g_file_query_filesystem_info_finish (file, res, &error);
  if (info != NULL)
    {
      priv = cc_info_overview_panel_get_instance_private (self);
      priv->total_bytes += g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
    }
  else
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          return;
      else
        {
          g_autofree char *path = NULL;
          path = g_file_get_path (file);
          g_warning ("Failed to get filesystem free space for '%s': %s", path, error->message);
        }
    }

  /* And onto the next element */
  get_primary_disc_info_start (self);
}

static void
get_primary_disc_info_start (CcInfoOverviewPanel *self)
{
  GUnixMountEntry *mount;
  g_autoptr(GFile) file = NULL;
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);

  if (priv->primary_mounts == NULL)
    {
      g_autofree char *size = NULL;

      size = g_format_size (priv->total_bytes);
      gtk_label_set_text (GTK_LABEL (priv->disk_label), size);

      return;
    }

  mount = priv->primary_mounts->data;
  priv->primary_mounts = g_list_remove (priv->primary_mounts, mount);
  file = g_file_new_for_path (g_unix_mount_get_mount_path (mount));
  g_unix_mount_free (mount);

  g_file_query_filesystem_info_async (file,
                                      G_FILE_ATTRIBUTE_FILESYSTEM_SIZE,
                                      0,
                                      priv->cancellable,
                                      (GAsyncReadyCallback) query_done,
                                      self);
}

static GList *
convert_points_to_entries (GList *points)
{
  GList *entries = NULL;
  GList *p;

  for (p = points; p != NULL; p = p->next)
    {
      GUnixMountPoint *point = p->data;
      GUnixMountEntry *mount;
      const gchar *mount_path = g_unix_mount_point_get_mount_path (point);

      mount = g_unix_mount_at (mount_path, NULL);
      if (mount)
        entries = g_list_append (entries, mount);
    }

  return entries;
}

static void
get_primary_disc_info (CcInfoOverviewPanel *self)
{
  GList *points, *entries = NULL;
  GList *p;
  GHashTable *hash;
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);

  hash = g_hash_table_new (g_str_hash, g_str_equal);
  points = g_unix_mount_points_get (NULL);

  entries = convert_points_to_entries (points);
  g_list_free_full (points, (GDestroyNotify) g_unix_mount_point_free);

  /* If we do not have /etc/fstab around, try /etc/mtab */
  if (entries == NULL)
    entries = g_unix_mounts_get (NULL);

  for (p = entries; p != NULL; p = p->next)
    {
      GUnixMountEntry *mount = p->data;
      const char *mount_path;
      const char *device_path;

      mount_path = g_unix_mount_get_mount_path (mount);
      device_path = g_unix_mount_get_device_path (mount);

      /* Do not count multiple mounts with same device_path, because it is
       * probably something like btrfs subvolume. Use only the first one in
       * order to count the real size. */
      if (gsd_should_ignore_unix_mount (mount) ||
          gsd_is_removable_mount (mount) ||
          g_str_has_prefix (mount_path, "/media/") ||
          g_str_has_prefix (mount_path, g_get_home_dir ()) ||
          g_hash_table_lookup (hash, device_path) != NULL)
        {
          g_unix_mount_free (mount);
          continue;
        }

      priv->primary_mounts = g_list_prepend (priv->primary_mounts, mount);
      g_hash_table_insert (hash, (gpointer) device_path, (gpointer) device_path);
    }
  g_list_free (entries);
  g_hash_table_destroy (hash);

  priv->cancellable = g_cancellable_new ();
  get_primary_disc_info_start (self);
}

static char *
get_cpu_info (const glibtop_sysinfo *info)
{
  g_autoptr(GHashTable) counts = NULL;
  g_autoptr(GString) cpu = NULL;
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
      g_autofree char *cleanedup = NULL;
      int count;

      count = GPOINTER_TO_INT (value);
      cleanedup = info_cleanup ((const char *) key);
      if (count > 1)
        g_string_append_printf (cpu, "%s \303\227 %d ", cleanedup, count);
      else
        g_string_append_printf (cpu, "%s ", cleanedup);
    }

  return g_strdup (cpu->str);
}

static void
move_one_up (GtkWidget *grid,
             GtkWidget *child)
{
  int top_attach;

  gtk_container_child_get (GTK_CONTAINER (grid),
                           child,
                           "top-attach", &top_attach,
                           NULL);
  gtk_container_child_set (GTK_CONTAINER (grid),
                           child,
                           "top-attach", top_attach - 1,
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
set_virtualization_label (CcInfoOverviewPanel *self,
                          const char          *virt)
{
  const char *display_name;
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);
  guint i;

  if (virt == NULL || *virt == '\0')
  {
    gtk_widget_hide (priv->virt_type_label);
    gtk_widget_hide (priv->label18);
    move_one_up (priv->grid1, priv->label8);
    move_one_up (priv->grid1, priv->disk_label);
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

  gtk_label_set_text (GTK_LABEL (priv->virt_type_label), display_name ? display_name : virt);
}

static void
info_overview_panel_setup_virt (CcInfoOverviewPanel *self)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GDBusProxy) systemd_proxy = NULL;
  g_autoptr(GVariant) variant = NULL;
  GVariant *inner;

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
      set_virtualization_label (self, NULL);
      return;
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
      set_virtualization_label (self, NULL);
      return;
    }

  g_variant_get (variant, "(v)", &inner);
  set_virtualization_label (self, g_variant_get_string (inner, NULL));
}

static void
info_overview_panel_setup_overview (CcInfoOverviewPanel *self)
{
  gboolean    res;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_type_text = NULL;
  g_autofree char *os_name_text = NULL;
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);

  res = load_gnome_version (&priv->gnome_version,
                            &priv->gnome_distributor,
                            &priv->gnome_date);
  if (res)
    {
      g_autofree gchar *text = NULL;
      text = g_strdup_printf (_("Version %s"), priv->gnome_version);
      gtk_label_set_text (GTK_LABEL (priv->version_label), text);
    }

  glibtop_get_mem (&mem);
  memory_text = g_format_size_full (mem.total, G_FORMAT_SIZE_IEC_UNITS);
  gtk_label_set_text (GTK_LABEL (priv->memory_label), memory_text ? memory_text : "");

  info = glibtop_get_sysinfo ();

  cpu_text = get_cpu_info (info);
  gtk_label_set_markup (GTK_LABEL (priv->processor_label), cpu_text ? cpu_text : "");

  os_type_text = get_os_type ();
  gtk_label_set_text (GTK_LABEL (priv->os_type_label), os_type_text ? os_type_text : "");

  os_name_text = get_os_name ();
  gtk_label_set_text (GTK_LABEL (priv->os_name_label), os_name_text ? os_name_text : "");

  get_primary_disc_info (self);

  gtk_label_set_markup (GTK_LABEL (priv->graphics_label), priv->graphics_data->hardware_string);
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
on_updates_button_clicked (GtkWidget           *widget,
                           CcInfoOverviewPanel *self)
{
  g_autoptr(GError) error = NULL;
  gboolean ret;
  g_auto(GStrv) argv = NULL;

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
      g_warning ("Failed to spawn %s: %s", argv[0], error->message);
}

static void
cc_info_overview_panel_dispose (GObject *object)
{
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (CC_INFO_OVERVIEW_PANEL (object));

  g_clear_pointer (&priv->graphics_data, graphics_data_free);

  G_OBJECT_CLASS (cc_info_overview_panel_parent_class)->dispose (object);
}

static void
cc_info_overview_panel_finalize (GObject *object)
{
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (CC_INFO_OVERVIEW_PANEL (object));

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  if (priv->primary_mounts)
    g_list_free_full (priv->primary_mounts, (GDestroyNotify) g_unix_mount_free);

  g_free (priv->gnome_version);
  g_free (priv->gnome_date);
  g_free (priv->gnome_distributor);

  G_OBJECT_CLASS (cc_info_overview_panel_parent_class)->finalize (object);
}

static void
cc_info_overview_panel_class_init (CcInfoOverviewPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_info_overview_panel_finalize;
  object_class->dispose = cc_info_overview_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info/info-overview.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, system_image);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, version_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, memory_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, processor_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, os_name_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, os_type_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, disk_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, graphics_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, virt_type_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, updates_button);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, label8);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, grid1);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, label18);

  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
}

static void
cc_info_overview_panel_init (CcInfoOverviewPanel *self)
{
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_resources_register (cc_info_get_resource ());

  priv->graphics_data = get_graphics_data ();

  if (does_gnome_software_exist () || does_gpk_update_viewer_exist ())
    g_signal_connect (priv->updates_button, "clicked", G_CALLBACK (on_updates_button_clicked), self);
  else
    gtk_widget_destroy (priv->updates_button);

  info_overview_panel_setup_overview (self);
  info_overview_panel_setup_virt (self);
}

GtkWidget *
cc_info_overview_panel_new (void)
{
  return g_object_new (CC_TYPE_INFO_OVERVIEW_PANEL,
                       NULL);
}
