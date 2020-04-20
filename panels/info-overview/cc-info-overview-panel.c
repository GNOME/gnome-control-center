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

#include "cc-hostname-entry.h"
#include "cc-os-release.h"

#include "cc-info-overview-resources.h"
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
#include <udisks/udisks.h>

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "cc-list-row.h"
#include "list-box-helper.h"
#include "cc-info-overview-panel.h"

struct _CcInfoOverviewPanel
{
  CcPanel          parent_instance;

  GtkEntry        *device_name_entry;
  GtkWidget       *rename_button;
  CcListRow       *disk_row;
  CcListRow       *gnome_version_row;
  CcListRow       *graphics_row;
  GtkListBox      *hardware_box;
  GtkDialog       *hostname_editor;
  CcHostnameEntry *hostname_entry;
  CcListRow       *hostname_row;
  CcListRow       *memory_row;
  GtkListBox      *os_box;
  CcListRow       *os_name_row;
  CcListRow       *os_type_row;
  CcListRow       *processor_row;
  CcListRow       *software_updates_row;
  CcListRow       *virtualization_row;
  CcListRow       *windowing_system_row;
};

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

G_DEFINE_TYPE (CcInfoOverviewPanel, cc_info_overview_panel, CC_TYPE_PANEL)

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

  g_debug ("About to launch '%s'", argv[0]);

  if (env != NULL)
    {
      guint i;
      g_debug ("With environment:");
      envp = g_get_environ ();
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

  if (!g_spawn_check_exit_status (status, NULL))
    return NULL;

  if (renderer == NULL || *renderer == '\0')
    return NULL;

  return info_cleanup (renderer);
}

typedef struct {
  char *name;
  gboolean is_default;
} GpuData;

static int
gpu_data_sort (gconstpointer a, gconstpointer b)
{
  GpuData *gpu_a = (GpuData *) a;
  GpuData *gpu_b = (GpuData *) b;

  if (gpu_a->is_default)
    return 1;
  if (gpu_b->is_default)
    return -1;
  return 0;
}

static void
gpu_data_free (GpuData *data)
{
  g_free (data->name);
  g_free (data);
}

static char *
get_renderer_from_switcheroo (void)
{
  g_autoptr(GDBusProxy) switcheroo_proxy = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  GString *renderers_string;
  guint i, num_children;
  GSList *renderers, *l;

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

  renderers_string = g_string_new (NULL);
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
  for (l = renderers; l != NULL; l = l->next)
    {
      GpuData *data = l->data;
      if (renderers_string->len > 0)
        g_string_append (renderers_string, " / ");
      g_string_append (renderers_string, data->name);
    }
  g_slist_free_full (renderers, (GDestroyNotify) gpu_data_free);

  if (renderers_string->len == 0)
    {
      g_string_free (renderers_string, TRUE);
      return NULL;
    }

  return g_string_free (renderers_string, FALSE);
}

static gchar *
get_graphics_hardware_string (void)
{
  g_autofree char *discrete_renderer = NULL;
  g_autofree char *renderer = NULL;

  renderer = get_renderer_from_switcheroo ();
  if (!renderer)
    renderer = get_renderer_from_session ();
  if (!renderer)
    renderer = get_renderer_from_helper (NULL);
  if (!renderer)
    return g_strdup (_("Unknown"));
  return g_strdup (renderer);
}

static char *
get_os_name (void)
{
  g_autoptr(GHashTable) os_info = NULL;
  const gchar *name, *version_id, *pretty_name, *build_id;
  gchar *result = NULL;
  g_autofree gchar *name_version = NULL;

  os_info = cc_os_release_get_values ();

  if (!os_info)
    return g_strdup (_("Unknown"));

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
get_primary_disc_info (CcInfoOverviewPanel *self)
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
      cc_list_row_set_secondary_label (self->disk_row,  _("Unknown"));
      return;
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
    {
      g_autofree gchar *size = g_format_size (total_size);
      cc_list_row_set_secondary_label (self->disk_row, size);
    }
  else
    {
      cc_list_row_set_secondary_label (self->disk_row,  _("Unknown"));
    }
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
  guint i;

  if (virt == NULL || *virt == '\0')
    {
      gtk_widget_hide (GTK_WIDGET (self->virtualization_row));
      return;
    }

  gtk_widget_show (GTK_WIDGET (self->virtualization_row));

  display_name = NULL;
  for (i = 0; i < G_N_ELEMENTS (virt_tech); i++)
    {
      if (g_str_equal (virt_tech[i].id, virt))
        {
          display_name = _(virt_tech[i].display);
          break;
        }
    }

  cc_list_row_set_secondary_label (self->virtualization_row, display_name ? display_name : virt);
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

static const char *
get_windowing_system (void)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

#if defined(GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display))
    return _("X11");
#endif /* GDK_WINDOWING_X11 */
#if defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return _("Wayland");
#endif /* GDK_WINDOWING_WAYLAND */
  return C_("Windowing system (Wayland, X11, or Unknown)", "Unknown");
}

static void
info_overview_panel_setup_overview (CcInfoOverviewPanel *self)
{
  g_autofree gchar *gnome_version = NULL;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_type_text = NULL;
  g_autofree char *os_name_text = NULL;
  g_autofree gchar *graphics_hardware_string = NULL;

  if (load_gnome_version (&gnome_version, NULL, NULL))
    cc_list_row_set_secondary_label (self->gnome_version_row, gnome_version);

  cc_list_row_set_secondary_label (self->windowing_system_row, get_windowing_system ());

  glibtop_get_mem (&mem);
  memory_text = g_format_size_full (mem.total, G_FORMAT_SIZE_IEC_UNITS);
  cc_list_row_set_secondary_label (self->memory_row, memory_text);

  info = glibtop_get_sysinfo ();

  cpu_text = get_cpu_info (info);
  cc_list_row_set_secondary_markup (self->processor_row, cpu_text);

  os_type_text = get_os_type ();
  cc_list_row_set_secondary_label (self->os_type_row, os_type_text);

  os_name_text = get_os_name ();
  cc_list_row_set_secondary_label (self->os_name_row, os_name_text);

  get_primary_disc_info (self);

  graphics_hardware_string = get_graphics_hardware_string ();
  cc_list_row_set_secondary_markup (self->graphics_row, graphics_hardware_string);
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
open_software_update (CcInfoOverviewPanel *self)
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
on_device_name_entry_changed (CcInfoOverviewPanel *self)
{
  const gchar *current_hostname, *new_hostname;

  current_hostname = gtk_entry_get_text (GTK_ENTRY (self->hostname_entry));
  new_hostname = gtk_entry_get_text (GTK_ENTRY (self->device_name_entry));
  gtk_widget_set_sensitive (self->rename_button,
                            g_strcmp0 (current_hostname, new_hostname) != 0);
}

static void
open_hostname_edit_dialog (CcInfoOverviewPanel *self)
{
  GtkWindow *toplevel;
  CcShell *shell;
  const gchar *hostname;
  gint response;

  g_assert (CC_IS_INFO_OVERVIEW_PANEL (self));

  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (shell));
  gtk_window_set_transient_for (GTK_WINDOW (self->hostname_editor), toplevel);

  hostname = gtk_entry_get_text (GTK_ENTRY (self->hostname_entry));
  gtk_entry_set_text (self->device_name_entry, hostname);
  gtk_widget_grab_focus (GTK_WIDGET (self->device_name_entry));

  response = gtk_dialog_run (self->hostname_editor);
  gtk_widget_hide (GTK_WIDGET (self->hostname_editor));

  if (response != GTK_RESPONSE_APPLY)
    return;

  /* We simply change the CcHostnameEntry text.  CcHostnameEntry
   * listens to changes and updates hostname on change.
   */
  hostname = gtk_entry_get_text (self->device_name_entry);
  gtk_entry_set_text (GTK_ENTRY (self->hostname_entry), hostname);
}

static void
cc_info_panel_row_activated_cb (CcInfoOverviewPanel *self,
                                CcListRow           *row)
{
  g_assert (CC_IS_INFO_OVERVIEW_PANEL (self));
  g_assert (CC_IS_LIST_ROW (row));

  if (row == self->hostname_row)
    open_hostname_edit_dialog (self);
  else if (row == self->software_updates_row)
    open_software_update (self);
}

static void
cc_info_overview_panel_class_init (CcInfoOverviewPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info-overview/cc-info-overview-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, device_name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, disk_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, gnome_version_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, graphics_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hardware_box);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hostname_editor);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hostname_entry);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hostname_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, memory_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, os_box);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, os_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, os_type_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, processor_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, rename_button);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, software_updates_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, virtualization_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, windowing_system_row);

  gtk_widget_class_bind_template_callback (widget_class, cc_info_panel_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_device_name_entry_changed);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
}

static void
cc_info_overview_panel_init (CcInfoOverviewPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_list_box_set_header_func (self->hardware_box, cc_list_box_update_header_func, NULL, NULL);
  gtk_list_box_set_header_func (self->os_box, cc_list_box_update_header_func, NULL, NULL);

  g_resources_register (cc_info_overview_get_resource ());

  if (!does_gnome_software_exist () && !does_gpk_update_viewer_exist ())
    gtk_widget_hide (GTK_WIDGET (self->software_updates_row));

  info_overview_panel_setup_overview (self);
  info_overview_panel_setup_virt (self);
}

GtkWidget *
cc_info_overview_panel_new (void)
{
  return g_object_new (CC_TYPE_INFO_OVERVIEW_PANEL,
                       NULL);
}
