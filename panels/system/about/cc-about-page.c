/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Purism SPC
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

#include "cc-about-page.h"
#include "cc-hostname-entry.h"
#include "cc-hostname.h"
#include "info-cleanup.h"
#include "shell/cc-object-storage.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>
#include <udisks/udisks.h>
#include <gudev/gudev.h>
#ifdef HAVE_GMOBILE
#define GMOBILE_USE_UNSTABLE_API
#include <gmobile.h>
#endif

#include <gdk/gdk.h>

#include <locale.h>

struct _CcAboutPage
{
  AdwNavigationPage parent_instance;

  AdwToastOverlay   *toast_overlay;

  GtkPicture        *os_logo;

  /* Hardware Information */
  AdwActionRow      *hardware_model_row;
  AdwActionRow      *firmware_version_row;
  AdwActionRow      *memory_row;
  AdwActionRow      *processor_row;
  AdwPreferencesGroup *hardware_group;
  AdwActionRow      *disk_row;

  /* Software Information */
  AdwActionRow      *os_name_row;
  AdwActionRow      *os_build_row;
  AdwActionRow      *os_type_row;
  AdwActionRow      *gnome_version_row;
  AdwActionRow      *virtualization_row;
  AdwActionRow      *kernel_row;
};

G_DEFINE_FINAL_TYPE (CcAboutPage, cc_about_page, ADW_TYPE_NAVIGATION_PAGE)

#if !defined(DISTRIBUTOR_LOGO) || defined(DARK_MODE_DISTRIBUTOR_LOGO)
static gboolean
use_dark_theme (CcAboutPage *self)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();

  return adw_style_manager_get_dark (style_manager);
}
#endif

static void
setup_os_logo (CcAboutPage *self)
{
#ifdef DISTRIBUTOR_LOGO
#ifdef DARK_MODE_DISTRIBUTOR_LOGO
  if (use_dark_theme (self))
    {
      gtk_picture_set_filename (self->os_logo, DARK_MODE_DISTRIBUTOR_LOGO);
      return;
    }
#endif
  gtk_picture_set_filename (self->os_logo, DISTRIBUTOR_LOGO);
  return;
#else
  GtkIconTheme *icon_theme;
  g_autofree char *logo_name = g_get_os_info ("LOGO");
  g_autoptr(GtkIconPaintable) icon_paintable = NULL;
  g_autoptr(GPtrArray) array = NULL;
  g_autoptr(GIcon) icon = NULL;
  gboolean dark;

  dark = use_dark_theme (self);
  if (logo_name == NULL)
    logo_name = g_strdup ("gnome-logo");

  array = g_ptr_array_new_with_free_func (g_free);
  if (dark)
    g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-text-dark", logo_name));
  g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-text", logo_name));
  if (dark)
    g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-dark", logo_name));
  g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s", logo_name));

  icon = g_themed_icon_new_from_names ((char **) array->pdata, array->len);
  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  icon_paintable = gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                                   192,
                                                   gtk_widget_get_scale_factor (GTK_WIDGET (self)),
                                                   gtk_widget_get_direction (GTK_WIDGET (self)),
                                                   GTK_ICON_LOOKUP_NONE);
  gtk_picture_set_paintable (self->os_logo, GDK_PAINTABLE (icon_paintable));
#endif
}

static void
on_donate_button_clicked_cb (CcAboutPage *self)
{
  g_app_info_launch_default_for_uri ("https://donate.gnome.org/", NULL, NULL);
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

/* @env is an array of strings with each pair of strings being the
 * key followed by the value */
static char *
get_renderer_from_helper (const char **env)
{
  int status;
  char *argv[] = { LIBEXECDIR "/gnome-control-center-print-renderer", NULL };
  g_auto(GStrv) envp = NULL;
  g_autofree char *renderer = NULL;
  g_autoptr(GError) error = NULL;
  /* Environment variables that are needed to run the helper on X11 and Wayland */
  static const char *env_vars[] = { "DISPLAY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR" };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (env_vars); i++)
    {
      const char *value = g_getenv (env_vars[i]);
      if (value)
        envp = g_environ_setenv (envp, env_vars[i], value, TRUE);
    }

  g_debug ("About to launch '%s'", argv[0]);

  if (env != NULL)
    {
      g_debug ("With environment:");
      for (i = 0; env != NULL && env[i] != NULL; i = i + 2)
        {
          g_debug ("  %s = %s", env[i], env[i+1]);
          envp = g_environ_setenv (envp, env[i], env[i+1], TRUE);
        }
    }
  else
    {
      g_debug ("No additional environment variables");
    }

  if (!g_spawn_sync (NULL, (char **) argv, envp, 0, NULL, NULL, &renderer, NULL, &status, &error))
    {
      g_debug ("Failed to get GPU: %s", error->message);
      return NULL;
    }

  if (!g_spawn_check_wait_status (status, NULL))
    return NULL;

  if (renderer == NULL || *renderer == '\0')
    return NULL;

  return info_cleanup (renderer);
}

typedef struct {
  char *name;
  gboolean is_default;
} GpuData;

static void
gpu_data_free (GpuData *data)
{
  g_clear_pointer (&data->name, g_free);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GpuData, gpu_data_free)

static int
gpu_data_sort (gconstpointer a, gconstpointer b)
{
  GpuData *gpu_a = (GpuData *) a;
  GpuData *gpu_b = (GpuData *) b;

  if (gpu_a->is_default)
    return -1;
  if (gpu_b->is_default)
    return 1;
  return 0;
}

static GSList *
get_renderer_from_switcheroo (void)
{
  g_autoptr(GDBusProxy) switcheroo_proxy = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  guint i, num_children;
  GSList *renderers;

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
      return NULL;
    }

  variant = g_dbus_proxy_get_cached_property (switcheroo_proxy, "GPUs");

  if (!variant)
    {
      g_debug ("Unable to retrieve net.hadess.SwitcherooControl.GPUs property, the daemon is likely not running");
      return NULL;
    }

  num_children = g_variant_n_children (variant);
  renderers = NULL;
  for (i = 0; i < num_children; i++)
    {
      g_autoptr(GVariant) gpu;
      g_autoptr(GVariant) name = NULL;
      g_autoptr(GVariant) env = NULL;
      g_autoptr(GVariant) default_variant = NULL;
      const char *name_s;
      g_autofree const char **env_s = NULL;
      gsize env_len;
      g_autofree char *renderer = NULL;
      GpuData *gpu_data;

      gpu = g_variant_get_child_value (variant, i);
      if (!gpu ||
          !g_variant_is_of_type (gpu, G_VARIANT_TYPE ("a{s*}")))
        continue;

      name = g_variant_lookup_value (gpu, "Name", NULL);
      env = g_variant_lookup_value (gpu, "Environment", NULL);
      if (!name || !env)
        continue;
      name_s = g_variant_get_string (name, NULL);
      g_debug ("Getting renderer from helper for GPU '%s'", name_s);
      env_s = g_variant_get_strv (env, &env_len);
      if (env_s != NULL && env_len % 2 != 0)
        {
          g_autofree char *debug = NULL;
          debug = g_strjoinv ("\n", (char **) env_s);
          g_warning ("Invalid environment returned from switcheroo:\n%s", debug);
          g_clear_pointer (&env_s, g_free);
        }

      renderer = get_renderer_from_helper (env_s);
      default_variant = g_variant_lookup_value (gpu, "Default", NULL);

      /* We could give up if we don't have a renderer, but that
       * might just mean gnome-session isn't installed. We fall back
       * to the device name in udev instead, which is better than nothing */

      gpu_data = g_new0 (GpuData, 1);
      gpu_data->name = g_strdup (renderer ? renderer : name_s);
      gpu_data->is_default = default_variant ? g_variant_get_boolean (default_variant) : FALSE;
      renderers = g_slist_prepend (renderers, gpu_data);
    }

  renderers = g_slist_sort (renderers, gpu_data_sort);

  return renderers;
}

static GSList *
get_graphics_hardware_list (void)
{
  GSList *renderers = NULL;

  renderers = get_renderer_from_switcheroo ();

  if (!renderers)
    {
      GpuData *gpu_data;
      g_autofree char *renderer;
      renderer = get_renderer_from_session ();
      if (!renderer)
        renderer = get_renderer_from_helper (NULL);
      if (!renderer)
        renderer = g_strdup (_("Unknown"));

      gpu_data = g_new0 (GpuData, 1);
      gpu_data->name = g_strdup (renderer);
      gpu_data->is_default = TRUE;
      renderers = g_slist_prepend (renderers, gpu_data);
    }

  return renderers;
}

static void
create_graphics_rows (CcAboutPage *self,
                      GSList      *devices)
{
  GSList *l;
  guint i = 0;

  for (l = devices; l != NULL; l = l->next)
    {
      GpuData *data = l->data;
      const char *name = data->name;
      g_autofree char *label = NULL;
      GtkWidget *gpu_entry;

      if (data->is_default)
        label = g_strdup (_("Graphics"));
      else
        label = g_strdup_printf (_("Graphics %d"), ++i);

      gpu_entry = adw_action_row_new ();
      adw_preferences_row_set_title (ADW_PREFERENCES_ROW (gpu_entry), label);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (gpu_entry), name);
      adw_action_row_set_subtitle_selectable (ADW_ACTION_ROW (gpu_entry), TRUE);
      gtk_widget_add_css_class (gpu_entry, "property");

      adw_preferences_group_add (self->hardware_group, gpu_entry);
    }
}

char *
get_os_name (void)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *version_id = NULL;
  g_autofree gchar *pretty_name = NULL;

  name = g_get_os_info (G_OS_INFO_KEY_NAME);
  version_id = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  pretty_name = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);

  if (pretty_name)
    return g_steal_pointer (&pretty_name);
  else if (name && version_id)
    return g_strdup_printf ("%s %s", name, version_id);
  else
    return g_strdup (_("Unknown"));
}

static char *
get_os_image_version (void)
{
  char *image_version = NULL;

  image_version = g_get_os_info ("IMAGE_VERSION");

  return image_version;
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

char *
get_primary_disk_info (void)
{
  g_autoptr(UDisksClient) client = NULL;
  GDBusObjectManager *manager;
  g_autolist(GDBusObject) objects = NULL;
  GList *l;
  guint64 total_size;
  g_autoptr(GError) error = NULL;

  total_size = 0;

  client = udisks_client_new_sync (NULL, &error);
  if (client == NULL)
    {
      g_warning ("Unable to get UDisks client: %s. Disk information will not be available.",
                 error->message);
      return NULL;
    }

  manager = udisks_client_get_object_manager (client);
  objects = g_dbus_object_manager_get_objects (manager);

  for (l = objects; l != NULL; l = l->next)
    {
      UDisksDrive *drive;
      drive = udisks_object_peek_drive (UDISKS_OBJECT (l->data));

      /* Skip removable devices */
      if (drive == NULL ||
          udisks_drive_get_removable (drive) ||
          udisks_drive_get_ejectable (drive))
        {
          continue;
        }

      total_size += udisks_drive_get_size (drive);
    }

  if (total_size > 0)
      return g_format_size (total_size);

  return NULL;
}

#ifdef HAVE_GMOBILE
static char *
get_hardware_model_string_from_device_tree (void)
{
  g_auto(GStrv) compatibles = gm_device_tree_get_compatibles (NULL, NULL);
  g_autoptr(GmDeviceInfo) info = NULL;
  const char *name;
  GmDisplayPanel *panel;

  if (gm_strv_is_null_or_empty (compatibles))
    return NULL;

  info = gm_device_info_new ((const char * const *)compatibles);
  panel = gm_device_info_get_display_panel (info);
  if (!panel)
    return NULL;

  name = gm_display_panel_get_name (panel);
  if (name)
    return g_strdup (name);

  return g_strdup (compatibles[0]);
}
#endif

char *
get_hardware_model_string (void)
{
  g_autofree char *vendor_string = NULL;
  g_autofree char *model_string = NULL;

  vendor_string = cc_hostname_get_property (cc_hostname_get_default (), "HardwareVendor");
  model_string = cc_hostname_get_property (cc_hostname_get_default (), "HardwareModel");

  if (vendor_string && g_strcmp0 (vendor_string, "") != 0 &&
      model_string && g_strcmp0 (model_string, "") != 0)
    return g_strdup_printf ("%s %s", vendor_string, model_string);

#ifdef HAVE_GMOBILE
  {
    g_autofree char *device_tree_string = get_hardware_model_string_from_device_tree ();
    if (device_tree_string)
      return g_steal_pointer (&device_tree_string);
  }
#endif

  return NULL;
}

static char *
get_firmware_version_string ()
{
  g_autofree char *firmware_version_string = NULL;

  firmware_version_string = cc_hostname_get_property (cc_hostname_get_default (), "FirmwareVersion");
  if (!firmware_version_string || g_strcmp0 (firmware_version_string, "") == 0)
    return NULL;

  return g_steal_pointer (&firmware_version_string);
}

static char *
get_kernel_version_string ()
{
  g_autofree char *kernel_name = NULL;
  g_autofree char *kernel_release = NULL;

  kernel_name = cc_hostname_get_property (cc_hostname_get_default (), "KernelName");
  if (!kernel_name || g_strcmp0 (kernel_name, "") == 0)
    return NULL;

  kernel_release = cc_hostname_get_property (cc_hostname_get_default (), "KernelRelease");
  if (!kernel_release || g_strcmp0 (kernel_release, "") == 0)
    return NULL;

  return g_strdup_printf ("%s %s", kernel_name, kernel_release);
}

char *
get_cpu_info ()
{
  g_autoptr(GHashTable) counts = NULL;
  g_autoptr(GString) cpu = NULL;
  const glibtop_sysinfo *info;
  GHashTableIter iter;
  gpointer       key, value;
  int            i;
  int            j;

  counts = g_hash_table_new (g_str_hash, g_str_equal);
  info = glibtop_get_sysinfo ();

  /* count duplicates */
  for (i = 0; i != info->ncpu; ++i)
    {
      const char * const keys[] = { "model name", "cpu", "Processor", "Model Name" };
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
      if (cpu->len != 0)
        g_string_append_printf (cpu, " ");
      if (count > 1)
        g_string_append_printf (cpu, "%s \303\227 %d", cleanedup, count);
      else
        g_string_append_printf (cpu, "%s", cleanedup);
    }

  return g_strdup (cpu->str);
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
set_virtualization_label (CcAboutPage *self,
                          const char  *virt)
{
  const char *display_name;
  guint i;

  if (virt == NULL || *virt == '\0')
    {
      gtk_widget_set_visible (GTK_WIDGET (self->virtualization_row), FALSE);

      return;
    }

  gtk_widget_set_visible (GTK_WIDGET (self->firmware_version_row), FALSE);

  gtk_widget_set_visible (GTK_WIDGET (self->virtualization_row), TRUE);

  display_name = NULL;
  for (i = 0; i < G_N_ELEMENTS (virt_tech); i++)
    {
      if (g_str_equal (virt_tech[i].id, virt))
        {
          display_name = _(virt_tech[i].display);
          break;
        }
    }

  adw_action_row_set_subtitle (self->virtualization_row, display_name ? display_name : virt);
}

static void
cc_about_page_setup_virt (CcAboutPage *self)
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

guint64
get_ram_size_libgtop (void)
{
  glibtop_mem mem;

  glibtop_get_mem (&mem);
  return mem.total;
}

guint64
get_ram_size_dmi (void)
{
  g_autoptr(GUdevClient) client = NULL;
  g_autoptr(GUdevDevice) dmi = NULL;
  const gchar * const subsystems[] = {"dmi", NULL };
  guint64 ram_total = 0;
  guint64 num_ram;
  guint i;

  client = g_udev_client_new (subsystems);
  dmi = g_udev_client_query_by_sysfs_path (client, "/sys/devices/virtual/dmi/id");
  if (!dmi)
    return 0;
  num_ram = g_udev_device_get_property_as_uint64 (dmi, "MEMORY_ARRAY_NUM_DEVICES");
  for (i = 0; i < num_ram ; i++) {
    g_autofree char *prop = NULL;

    prop = g_strdup_printf ("MEMORY_DEVICE_%d_SIZE", i);
    ram_total += g_udev_device_get_property_as_uint64 (dmi, prop);
  }
  return ram_total;
}

static void
system_details_window_title_print_padding (const gchar *title, GString *dst_string, gsize maxlen)
{
  gsize title_len;
  gsize maxpad = maxlen;

  if (maxlen == 0)
    maxpad = 50;

  if (title == NULL || dst_string == NULL)
    return;
  g_string_append_printf (dst_string, "%s", title);

  title_len = g_utf8_strlen (title, -1) + 1;
  for (gsize i = title_len; i < maxpad; i++)
    g_string_append (dst_string, " ");
}

static void
on_copy_row_activated_cb (GtkWidget    *widget,
                           CcAboutPage  *self)
{
  GdkClipboard *clip_board;
  GdkDisplay *display;
  g_autofree gchar *date_string = NULL;
  g_autoptr(GDateTime) date = NULL;
  guint64 ram_size;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_type_text = NULL;
  g_autofree char *os_name_text = NULL;
  g_autofree char *os_build_text = NULL;
  g_autofree char *hardware_model_text = NULL;
  g_autofree char *firmware_version_text = NULL;
  g_autofree char *kernel_version_text = NULL;
  g_autoslist(GpuData) graphics_hardware_list = NULL;
  GSList *l;
  g_autofree gchar *disk_capacity_string = NULL;
  g_autoptr(GString) result_str;
  locale_t untranslated_locale;

  /* Don't use translations for the copied content */
  untranslated_locale = newlocale (LC_ALL_MASK, "C", (locale_t) 0);
  uselocale (untranslated_locale);

  result_str = g_string_new (NULL);

  g_string_append (result_str, "# System Details Report\n");
  g_string_append (result_str, "---\n\n");

  g_string_append (result_str, "## Report details\n");

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Date generated:**", result_str, 0);
  date = g_date_time_new_now_local ();
  date_string = g_date_time_format (date, "%Y-%m-%d %H:%M:%S");

  g_string_append_printf (result_str, "%s\n\n", date_string);

  g_string_append (result_str, "## Hardware Information:\n");

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Hardware Model:**", result_str, 0);
  hardware_model_text = get_hardware_model_string ();
  g_string_append_printf (result_str, "%s\n", hardware_model_text);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Memory:**", result_str, 0);
  ram_size = get_ram_size_dmi ();
  if (ram_size == 0)
    ram_size = get_ram_size_libgtop ();
  memory_text = g_format_size_full (ram_size, G_FORMAT_SIZE_IEC_UNITS);
  g_string_append_printf (result_str, "%s\n", memory_text);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Processor:**", result_str, 0);
  cpu_text = get_cpu_info ();
  g_string_append_printf (result_str, "%s\n", cpu_text);

  graphics_hardware_list = get_graphics_hardware_list ();
  guint i = 0;

  for (l = graphics_hardware_list; l != NULL; l = l->next)
    {
      GpuData *data = l->data;
      const char *name = data->name;
      g_autofree char *label = NULL;

      if (data->is_default)
        label = g_strdup ("**Graphics:**");
      else
        label = g_strdup_printf ("**Graphics %d:**", ++i);
      g_string_append (result_str, "- ");
      system_details_window_title_print_padding (label, result_str, 0);
      g_string_append_printf (result_str, "%s\n", name);
    }

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Disk Capacity:**", result_str, 0);
  disk_capacity_string = get_primary_disk_info ();
  g_string_append_printf (result_str, "%s\n", disk_capacity_string);

  g_string_append (result_str, "\n");

  g_string_append (result_str, "## Software Information:\n");

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Firmware Version:**", result_str, 0);
  firmware_version_text = get_firmware_version_string ();
  g_string_append_printf (result_str, "%s\n", firmware_version_text);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**OS Name:**", result_str, 0);
  os_name_text = get_os_name ();
  g_string_append_printf (result_str, "%s\n", os_name_text);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**OS Build:**", result_str, 0);
  os_build_text = get_os_image_version ();
  g_string_append_printf (result_str, "%s\n", os_build_text);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**OS Type:**", result_str, 0);
  os_type_text = get_os_type ();
  g_string_append_printf (result_str, "%s\n", os_type_text);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**GNOME Version:**", result_str, 0);
  g_string_append_printf (result_str, "%s\n", MAJOR_VERSION);

  g_string_append (result_str, "- ");
  system_details_window_title_print_padding ("**Kernel Version:**", result_str, 0);
  kernel_version_text = get_kernel_version_string ();
  g_string_append_printf (result_str, "%s\n", kernel_version_text);

  display = gdk_display_get_default ();
  clip_board = gdk_display_get_clipboard (display);
  gdk_clipboard_set_text (clip_board, result_str->str);

  /* Reset to the user's original locale. */
  uselocale (LC_GLOBAL_LOCALE);
  freelocale (untranslated_locale);

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Details copied to clipboard")));
}

static void
cc_about_page_setup_overview (CcAboutPage *self)
{
  guint64 ram_size;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_type_text = NULL;
  g_autofree char *os_name_text = NULL;
  g_autofree char *os_build_text = NULL;
  g_autofree char *hardware_model_text = NULL;
  g_autofree char *firmware_version_text = NULL;
  g_autofree char *kernel_version_text = NULL;
  g_autoslist(GpuData) graphics_hardware_list = NULL;
  g_autofree gchar *disk_capacity_string = NULL;

  hardware_model_text = get_hardware_model_string ();
  adw_action_row_set_subtitle (self->hardware_model_row, hardware_model_text);
  gtk_widget_set_visible (GTK_WIDGET (self->hardware_model_row), hardware_model_text != NULL);

  firmware_version_text = get_firmware_version_string ();
  adw_action_row_set_subtitle (self->firmware_version_row, firmware_version_text);
  gtk_widget_set_visible (GTK_WIDGET (self->firmware_version_row), firmware_version_text != NULL);

  ram_size = get_ram_size_dmi ();
  if (ram_size == 0)
    ram_size = get_ram_size_libgtop ();
  memory_text = g_format_size_full (ram_size, G_FORMAT_SIZE_IEC_UNITS);
  adw_action_row_set_subtitle (self->memory_row, memory_text);

  cpu_text = get_cpu_info ();
  adw_action_row_set_subtitle (self->processor_row, cpu_text);

  graphics_hardware_list = get_graphics_hardware_list ();
  create_graphics_rows (self, graphics_hardware_list);

  disk_capacity_string = get_primary_disk_info ();
  if (disk_capacity_string == NULL)
    disk_capacity_string = g_strdup (_("Unknown"));
  adw_action_row_set_subtitle (self->disk_row, disk_capacity_string);

  os_name_text = get_os_name ();
  adw_action_row_set_subtitle (self->os_name_row, os_name_text);

  os_build_text = get_os_image_version ();
  adw_action_row_set_subtitle (self->os_build_row, os_build_text);
  gtk_widget_set_visible (GTK_WIDGET (self->os_build_row), os_build_text != NULL);

  os_type_text = get_os_type ();
  adw_action_row_set_subtitle (self->os_type_row, os_type_text);

  adw_action_row_set_subtitle (self->gnome_version_row, MAJOR_VERSION);

  kernel_version_text = get_kernel_version_string ();
  adw_action_row_set_subtitle (self->kernel_row, kernel_version_text);
  gtk_widget_set_visible (GTK_WIDGET (self->kernel_row), kernel_version_text != NULL);
}

static void
cc_about_page_class_init (CcAboutPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/about/cc-about-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, toast_overlay);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, disk_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, gnome_version_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, hardware_group);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, hardware_model_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, firmware_version_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, kernel_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, memory_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, os_logo);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, os_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, os_build_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, os_type_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, processor_row);
  gtk_widget_class_bind_template_child (widget_class, CcAboutPage, virtualization_row);

  gtk_widget_class_bind_template_callback (widget_class, on_copy_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_donate_button_clicked_cb);
}

static void
cc_about_page_init (CcAboutPage *self)
{
  AdwStyleManager *style_manager;

  gtk_widget_init_template (GTK_WIDGET (self));

  cc_about_page_setup_overview (self);
  cc_about_page_setup_virt (self);

  style_manager = adw_style_manager_get_default ();
  g_signal_connect_object (style_manager, "notify::dark", G_CALLBACK (setup_os_logo), self, G_CONNECT_SWAPPED);
  setup_os_logo (self);
}
