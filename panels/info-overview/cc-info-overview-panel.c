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

#include "cc-info-overview-panel.h"


typedef struct
{
  GtkLabel  *disk_label;
  GtkLabel  *graphics_label;
  GtkLabel  *memory_label;
  GtkLabel  *name_entry;
  GtkLabel  *os_name_label;
  GtkLabel  *os_type_label;
  GtkLabel  *processor_label;
  GtkButton *updates_button;
  GtkLabel  *version_label;
  GtkLabel  *virt_type_label;
  GtkLabel  *virt_type_title_label;
} CcInfoOverviewPanelPrivate;

struct _CcInfoOverviewPanel
{
 CcPanel parent_instance;

  /*< private >*/
 CcInfoOverviewPanelPrivate *priv;
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
  char *argv[] = { GNOME_SESSION_DIR "/gnome-session-check-accelerated", NULL };
  g_auto(GStrv) envp = NULL;
  g_autofree char *renderer = NULL;
  g_autoptr(GError) error = NULL;

  if (env != NULL)
    {
      guint i;
      envp = g_get_environ ();
      for (i = 0; env[i] != NULL; i = i + 2)
        envp = g_environ_setenv (envp, env[i], env[i+1], TRUE);
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

static char *
get_renderer_from_switcheroo (void)
{
  g_autoptr(GDBusProxy) switcheroo_proxy = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  GString *renderers;
  guint i, num_children;

  renderers = g_string_new (NULL);
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
  for (i = 0; i < num_children; i++)
    {
      g_autoptr(GVariant) gpu;
      g_autoptr(GVariant) name = NULL;
      g_autoptr(GVariant) env = NULL;
      const char *name_s, **env_s;
      g_autofree char *renderer = NULL;

      gpu = g_variant_get_child_value (variant, i);
      if (!gpu)
        continue;

      name = g_variant_lookup_value (gpu, "Name", NULL);
      env = g_variant_lookup_value (gpu, "Environment", NULL);
      if (!name || !env)
        continue;
      name_s = g_variant_get_string (name, NULL);
      g_debug ("Getting renderer from helper for GPU '%s'", name_s);
      env_s = g_variant_get_strv (env, NULL);
      renderer = get_renderer_from_helper (env_s);
      g_free (env_s);

      /* We could give up if we don't have a renderer, but that
       * might just mean gnome-session isn't installed. We fall back
       * to the device name in udev instead, which is better than nothing */

      if (renderers->len > 0)
        g_string_append (renderers, " / ");
      g_string_append (renderers, renderer ? renderer : name_s);
    }

  if (renderers->len == 0)
    {
      g_string_free (renderers, TRUE);
      return NULL;
    }

  return g_string_free (renderers, FALSE);
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
  CcInfoOverviewPanelPrivate *priv;
  g_autoptr(UDisksClient) client = NULL;
  GDBusObjectManager *manager;
  g_autolist(GDBusObject) objects = NULL;
  GList *l;
  guint64 total_size;
  g_autoptr(GError) error = NULL;

  priv = cc_info_overview_panel_get_instance_private (self);
  total_size = 0;

  client = udisks_client_new_sync (NULL, &error);
  if (client == NULL)
    {
      g_warning ("Unable to get UDisks client: %s. Disk information will not be available.",
                 error->message);
      gtk_label_set_text (priv->disk_label, _("Unknown"));
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
      gtk_label_set_text (priv->disk_label, size);
    }
  else
    {
      gtk_label_set_text (priv->disk_label, _("Unknown"));
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
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);
  guint i;

  if (virt == NULL || *virt == '\0')
  {
    gtk_widget_hide (GTK_WIDGET (priv->virt_type_label));
    gtk_widget_hide (GTK_WIDGET (priv->virt_type_title_label));
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

  gtk_label_set_text (priv->virt_type_label, display_name ? display_name : virt);
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
  g_autofree gchar *gnome_version = NULL;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_type_text = NULL;
  g_autofree char *os_name_text = NULL;
  g_autofree gchar *graphics_hardware_string = NULL;
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);

  if (load_gnome_version (&gnome_version, NULL, NULL))
    {
      g_autofree gchar *text = NULL;
      text = g_strdup_printf (_("Version %s"), gnome_version);
      gtk_label_set_text (priv->version_label, text);
    }

  glibtop_get_mem (&mem);
  memory_text = g_format_size_full (mem.total, G_FORMAT_SIZE_IEC_UNITS);
  gtk_label_set_text (priv->memory_label, memory_text ? memory_text : "");

  info = glibtop_get_sysinfo ();

  cpu_text = get_cpu_info (info);
  gtk_label_set_markup (priv->processor_label, cpu_text ? cpu_text : "");

  os_type_text = get_os_type ();
  gtk_label_set_text (priv->os_type_label, os_type_text ? os_type_text : "");

  os_name_text = get_os_name ();
  gtk_label_set_text (priv->os_name_label, os_name_text ? os_name_text : "");

  get_primary_disc_info (self);

  graphics_hardware_string = get_graphics_hardware_string ();
  gtk_label_set_markup (priv->graphics_label, graphics_hardware_string);
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
on_updates_button_clicked (CcInfoOverviewPanel *self)
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
cc_info_overview_panel_class_init (CcInfoOverviewPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info-overview/cc-info-overview-panel.ui");

  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, disk_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, graphics_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, memory_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, name_entry);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, os_name_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, os_type_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, processor_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, updates_button);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, version_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, virt_type_label);
  gtk_widget_class_bind_template_child_private (widget_class, CcInfoOverviewPanel, virt_type_title_label);

  gtk_widget_class_bind_template_callback (widget_class, on_updates_button_clicked);

  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
}

static void
cc_info_overview_panel_init (CcInfoOverviewPanel *self)
{
  CcInfoOverviewPanelPrivate *priv = cc_info_overview_panel_get_instance_private (self);

  gtk_widget_init_template (GTK_WIDGET (self));

  g_resources_register (cc_info_overview_get_resource ());

  if (!does_gnome_software_exist () && !does_gpk_update_viewer_exist ())
    gtk_widget_destroy (GTK_WIDGET (priv->updates_button));

  info_overview_panel_setup_overview (self);
  info_overview_panel_setup_virt (self);
}

GtkWidget *
cc_info_overview_panel_new (void)
{
  return g_object_new (CC_TYPE_INFO_OVERVIEW_PANEL,
                       NULL);
}
