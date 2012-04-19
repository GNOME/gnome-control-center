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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include <config.h>

#include "cc-info-panel.h"

#include <polkit/polkit.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#include "hostname-helper.h"
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

/* Session */
#define GNOME_SESSION_MANAGER_SCHEMA        "org.gnome.desktop.session"
#define KEY_SESSION_NAME          "session-name"

#define WID(w) (GtkWidget *) gtk_builder_get_object (self->priv->builder, w)

G_DEFINE_DYNAMIC_TYPE (CcInfoPanel, cc_info_panel, CC_TYPE_PANEL)

#define INFO_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_INFO_PANEL, CcInfoPanelPrivate))

typedef struct {
  /* Will be one of the other two below, or "Unknown" */ 
  const char *hardware_string;

  char *xorg_vesa_hardware;
  char *glx_renderer;
} GraphicsData;

typedef enum {
	PK_NOT_AVAILABLE,
	UPDATES_AVAILABLE,
	UPDATES_NOT_AVAILABLE,
	CHECKING_UPDATES
} UpdatesState;

struct _CcInfoPanelPrivate
{
  GtkBuilder    *builder;
  char          *gnome_version;
  char          *gnome_distributor;
  char          *gnome_date;
  UpdatesState   updates_state;
  gboolean       is_fallback;

  /* Free space */
  GList         *primary_mounts;
  guint64        total_bytes;
  GCancellable  *cancellable;

  /* Media */
  GSettings     *media_settings;
  GtkWidget     *other_application_combo;

  GDBusConnection     *session_bus;
  GDBusProxy          *pk_proxy;
  GDBusProxy          *pk_transaction_proxy;
  GDBusProxy          *hostnamed_proxy;
  GSettings           *session_settings;

  GraphicsData  *graphics_data;
};

static void get_primary_disc_info_start (CcInfoPanel *self);
static void refresh_update_button (CcInfoPanel *self);

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

typedef struct
{
  char *regex;
  char *replacement;
} ReplaceStrings;

static char *
prettify_info (const char *info)
{
  char *pretty;
  int   i;
  static const ReplaceStrings rs[] = {
    { "Mesa DRI ", ""},
    { "Intel[(]R[)]", "Intel<sup>\302\256</sup>"},
    { "Core[(]TM[)]", "Core<sup>\342\204\242</sup>"},
    { "Atom[(]TM[)]", "Atom<sup>\342\204\242</sup>"},
    { "Graphics Controller", "Graphics"},
  };

  pretty = g_markup_escape_text (info, -1);

  for (i = 0; i < G_N_ELEMENTS (rs); i++)
    {
      GError *error;
      GRegex *re;
      char   *new;

      error = NULL;

      re = g_regex_new (rs[i].regex, 0, 0, &error);
      if (re == NULL)
        {
          g_warning ("Error building regex: %s", error->message);
          g_error_free (error);
          continue;
        }

      new = g_regex_replace_literal (re,
                                     pretty,
                                     -1,
                                     0,
                                     rs[i].replacement,
                                     0,
                                     &error);

      g_regex_unref (re);

      if (error != NULL)
        {
          g_warning ("Error replacing %s: %s", rs[i].regex, error->message);
          g_error_free (error);
          continue;
        }

      g_free (pretty);
      pretty = new;
    }

  return pretty;
}

static void
graphics_data_free (GraphicsData *gdata)
{
  g_free (gdata->xorg_vesa_hardware);
  g_free (gdata->glx_renderer);
  g_slice_free (GraphicsData, gdata);
}

static char *
get_graphics_data_glx_renderer (void)
{
  GError     *error;
  GRegex     *re;
  GMatchInfo *match_info;
  char       *output;
  char       *result;
  GString    *info;

  info = g_string_new (NULL);

  error = NULL;
  g_spawn_command_line_sync ("glxinfo -l", &output, NULL, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to get graphics info: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  re = g_regex_new ("^OpenGL renderer string: (.+)$", G_REGEX_MULTILINE, 0, &error);
  if (re == NULL)
    {
      g_warning ("Error building regex: %s", error->message);
      g_error_free (error);
      goto out;
    }

  g_regex_match (re, output, 0, &match_info);
  while (g_match_info_matches (match_info))
    {
      char *device;

      device = g_match_info_fetch (match_info, 1);
      g_string_append_printf (info, "%s ", device);
      g_free (device);

      g_match_info_next (match_info, NULL);
    }
  g_match_info_free (match_info);
  g_regex_unref (re);

 out:
  g_free (output);
  result = prettify_info (info->str);
  g_string_free (info, TRUE);

  return result;
}

static char *
get_graphics_data_xorg_vesa_hardware (void)
{
  char *display_num;
  char *log_path;
  char *log_contents;
  gsize log_len;
  GError *error = NULL;
  GRegex *re;
  GMatchInfo *match;
  char *result = NULL;

  {
    const char *display;

    display = g_getenv ("DISPLAY");
    if (!display)
      return NULL;

    re = g_regex_new ("^:([0-9]+)", 0, 0, NULL);
    g_assert (re != NULL);

    g_regex_match (re, display, 0, &match);

    if (!g_match_info_matches (match))
      {
        g_regex_unref (re);
        g_match_info_free (match);
        return NULL;
      }

    display_num = g_match_info_fetch (match, 1);

    g_regex_unref (re);
    re = NULL;
    g_match_info_free (match);
    match = NULL;
  }

  log_path = g_strdup_printf ("/var/log/Xorg.%s.log", display_num);
  g_free (display_num);
  log_contents = NULL;
  g_file_get_contents (log_path, &log_contents, &log_len, &error);
  g_free (log_path);
  if (!log_contents)
    return NULL;

  re = g_regex_new ("VESA VBE OEM Product: (.*)$", G_REGEX_MULTILINE, 0, NULL);
  g_assert (re != NULL);

  g_regex_match (re, log_contents, 0, &match);
  if (g_match_info_matches (match))
    {
      char *tmp;
      char *pretty_tmp;
      tmp = g_match_info_fetch (match, 1);
      pretty_tmp = prettify_info (tmp);
      g_free (tmp);
      /* Translators: VESA is an techncial acronym, don't translate it. */
      result = g_strdup_printf (_("VESA: %s"), pretty_tmp); 
      g_free (pretty_tmp);
    }
  g_match_info_free (match);
  g_regex_unref (re);

  return result;
}

static GraphicsData *
get_graphics_data (void)
{
  GraphicsData *result;

  result = g_slice_new0 (GraphicsData);

  result->glx_renderer = get_graphics_data_glx_renderer ();
  result->xorg_vesa_hardware = get_graphics_data_xorg_vesa_hardware ();

  if (result->xorg_vesa_hardware != NULL)
    result->hardware_string = result->xorg_vesa_hardware;
  else if (result->glx_renderer != NULL)
    result->hardware_string = result->glx_renderer;
  else
    result->hardware_string = _("Unknown");

  return result;
}

static gboolean
get_current_is_fallback (CcInfoPanel  *self)
{
  GError   *error;
  GVariant *reply;
  GVariant *reply_str;
  gboolean  is_fallback;

  error = NULL;
  if (!(reply = g_dbus_connection_call_sync (self->priv->session_bus,
                                             "org.gnome.SessionManager",
                                             "/org/gnome/SessionManager",
                                             "org.freedesktop.DBus.Properties",
                                             "Get",
                                             g_variant_new ("(ss)", "org.gnome.SessionManager", "session-name"),
                                             (GVariantType*)"(v)",
                                             0,
                                             -1,
                                             NULL, &error)))
    {
      g_warning ("Failed to get fallback mode: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }

  g_variant_get (reply, "(v)", &reply_str);
  is_fallback = g_strcmp0 ("gnome-fallback", g_variant_get_string (reply_str, NULL)) == 0;
  g_variant_unref (reply_str);
  g_variant_unref (reply);

  return is_fallback;
}

static void
cc_info_panel_get_property (GObject    *object,
                            guint       property_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_info_panel_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_info_panel_dispose (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  if (priv->builder != NULL)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  if (priv->pk_proxy != NULL)
    {
      g_object_unref (priv->pk_proxy);
      priv->pk_proxy = NULL;
    }

  if (priv->pk_transaction_proxy != NULL)
    {
      g_object_unref (priv->pk_transaction_proxy);
      priv->pk_transaction_proxy = NULL;
    }

  if (priv->graphics_data != NULL)
    {
      graphics_data_free (priv->graphics_data);
      priv->graphics_data = NULL;
    }

  G_OBJECT_CLASS (cc_info_panel_parent_class)->dispose (object);
}

static void
cc_info_panel_finalize (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      priv->cancellable = NULL;
    }
  g_free (priv->gnome_version);
  g_free (priv->gnome_date);
  g_free (priv->gnome_distributor);

  if (priv->hostnamed_proxy != NULL)
    {
      g_object_unref (priv->hostnamed_proxy);
      priv->hostnamed_proxy = NULL;
    }

  if (priv->media_settings != NULL)
    {
      g_object_unref (priv->media_settings);
      priv->media_settings = NULL;
    }

  if (priv->session_settings != NULL)
    {
      g_object_unref (priv->session_settings);
      priv->session_settings = NULL;
    }

  G_OBJECT_CLASS (cc_info_panel_parent_class)->finalize (object);
}

static void
cc_info_panel_class_init (CcInfoPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcInfoPanelPrivate));

  object_class->get_property = cc_info_panel_get_property;
  object_class->set_property = cc_info_panel_set_property;
  object_class->dispose = cc_info_panel_dispose;
  object_class->finalize = cc_info_panel_finalize;
}

static void
cc_info_panel_class_finalize (CcInfoPanelClass *klass)
{
}

static char *
get_os_type (void)
{
  int bits;

  if (GLIB_SIZEOF_VOID_P == 8)
    bits = 64;
  else
    bits = 32;

  /* translators: This is the type of architecture, for example:
   * "64-bit" or "32-bit" */
  return g_strdup_printf (_("%d-bit"), bits);
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
remove_duplicate_whitespace (const char *old)
{
  char   *new;
  GRegex *re;
  GError *error;

  error = NULL;
  re = g_regex_new ("[ \t\n\r]+", G_REGEX_MULTILINE, 0, &error);
  if (re == NULL)
    {
      g_warning ("Error building regex: %s", error->message);
      g_error_free (error);
      return g_strdup (old);
    }
  new = g_regex_replace (re,
                         old,
                         -1,
                         0,
                         " ",
                         0,
                         &error);
  g_regex_unref (re);
  if (new == NULL)
    {
      g_warning ("Error replacing string: %s", error->message);
      g_error_free (error);
      return g_strdup (old);
    }

  return new;
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
      const char * const keys[] = { "model name", "cpu" };
      char *model;
      int  *count;

      model = NULL;

      for (j = 0; model == NULL && j != G_N_ELEMENTS (keys); ++j)
        {
          model = g_hash_table_lookup (info->cpuinfo[i].values,
                                       keys[j]);
        }

      if (model == NULL)
          model = _("Unknown model");

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
      char *stripped;
      int   count;

      count = GPOINTER_TO_INT (value);
      stripped = remove_duplicate_whitespace ((const char *)key);
      if (count > 1)
        g_string_append_printf (cpu, "%s \303\227 %d ", stripped, count);
      else
        g_string_append_printf (cpu, "%s ", stripped);
      g_free (stripped);
    }

  g_hash_table_destroy (counts);

  ret = prettify_info (cpu->str);
  g_string_free (cpu, TRUE);

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

static gboolean
switch_fallback_get_mapping (GValue    *value,
                             GVariant  *variant,
                             gpointer   data)
{
  const char *setting;

  setting = g_variant_get_string (variant, NULL);
  g_value_set_boolean (value, strcmp (setting, "gnome") != 0);
  return TRUE;
}

static void
toggle_fallback_warning_label (CcInfoPanel *self,
                               gboolean     visible)
{
  GtkWidget  *widget;
  const char *text;

  widget = WID ("graphics_logout_warning_label");

  if (self->priv->is_fallback)
    text = _("The next login will attempt to use the standard experience.");
  else
    text = _("The next login will use the fallback mode intended for unsupported graphics hardware.");

  gtk_label_set_text (GTK_LABEL (widget), text);

  if (visible)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
}

static GVariant *
switch_fallback_set_mapping (const GValue        *value,
                             const GVariantType  *expected_type,
                             gpointer             data)
{
  CcInfoPanel *self = data;
  gboolean     is_set;

  is_set = g_value_get_boolean (value);
  if (is_set != self->priv->is_fallback)
    toggle_fallback_warning_label (self, TRUE);
  else
    toggle_fallback_warning_label (self, FALSE);

  return g_variant_new_string (is_set ? "gnome-fallback" : "gnome");
}

static void
info_panel_setup_graphics (CcInfoPanel  *self)
{
  GtkWidget *widget;
  GtkSwitch *sw;
  char *text;

  widget = WID ("graphics_driver_label");
  gtk_label_set_markup (GTK_LABEL (widget), self->priv->graphics_data->hardware_string);

  self->priv->is_fallback = get_current_is_fallback (self);
  if (self->priv->is_fallback)
    {
      /* translators: The hardware is not able to run GNOME 3's
       * shell, so we use the GNOME "Fallback" session */
      text = g_strdup (C_("Experience", "Fallback"));
    }
  else
    {
      /* translators: The hardware is able to run GNOME 3's
       * shell, also called "Standard" experience */
      text = g_strdup (C_("Experience", "Standard"));
    }
  widget = WID ("graphics_experience_label");
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID ("graphics_fallback_switch_box");
  sw = GTK_SWITCH (gtk_switch_new ());
  g_settings_bind_with_mapping (self->priv->session_settings, KEY_SESSION_NAME,
                                sw, "active", 0,
                                switch_fallback_get_mapping,
                                switch_fallback_set_mapping, self, NULL);
  gtk_box_pack_start (GTK_BOX (widget), GTK_WIDGET (sw), FALSE, FALSE, 0);
  gtk_widget_show_all (GTK_WIDGET (sw));
  widget = WID ("fallback-label");
  gtk_label_set_mnemonic_widget (GTK_LABEL (widget), GTK_WIDGET (sw));
}

static void
default_app_changed (GtkAppChooserButton *button,
                     CcInfoPanel         *self)
{
  GAppInfo *info;
  char *content_type;
  GError *error = NULL;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (button));
  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (button));
  if (g_app_info_set_as_default_for_type (info, content_type, &error) == FALSE)
    {
      g_warning ("Failed to set '%s' as the default application for '%s': %s",
                 g_app_info_get_name (info), content_type, error->message);
      g_error_free (error);
      error = NULL;
    }

  /* Set https support for the browser as well */
  if (g_str_equal (content_type, "x-scheme-handler/http"))
    {
      if (g_app_info_set_as_default_for_type (info, "x-scheme-handler/https", &error) == FALSE)
        {
          g_warning ("Failed to set '%s' as the default application for '%s': %s",
                     g_app_info_get_name (info), "x-scheme-handler/https", error->message);
          g_error_free (error);
        }
    }

  g_free (content_type);
  g_object_unref (info);
}

static void
info_panel_setup_default_app (CcInfoPanel *self,
                              const char  *content_type,
                              const char  *label_id,
                              guint        left_attach,
                              guint        right_attach,
                              guint        top_attach,
                              guint        bottom_attach)
{
  GtkWidget *button;
  GtkWidget *table;
  GtkWidget *label;

  table = WID ("default_apps_table");

  button = gtk_app_chooser_button_new (content_type);
  gtk_app_chooser_button_set_show_default_item (GTK_APP_CHOOSER_BUTTON (button), TRUE);
  gtk_table_attach (GTK_TABLE (table), button,
                    left_attach, right_attach,
                    top_attach, bottom_attach, GTK_FILL, 0, 0, 0);
  g_signal_connect (G_OBJECT (button), "changed",
                    G_CALLBACK (default_app_changed), self);
  gtk_widget_show (button);

  label = WID(label_id);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
}

static void
info_panel_setup_default_apps (CcInfoPanel  *self)
{
  info_panel_setup_default_app (self, "x-scheme-handler/http", "web-label",
                                1, 2, 0, 1);

  info_panel_setup_default_app (self, "x-scheme-handler/mailto", "mail-label",
                                1, 2, 1, 2);

  info_panel_setup_default_app (self, "text/calendar", "calendar-label",
                                1, 2, 2, 3);

  info_panel_setup_default_app (self, "audio/x-vorbis+ogg", "music-label",
                                1, 2, 3, 4);

  info_panel_setup_default_app (self, "video/x-ogm+ogg", "video-label",
                                1, 2, 4, 5);

  info_panel_setup_default_app (self, "image/jpeg", "photos-label",
                                1, 2, 5, 6);
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

  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "extra_options_dialog"));
  combo_box = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_other_type_combobox"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
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
    { "x-content/video-vcd", N_("Video CD") }
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

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Graphics"),
                      -1);

  g_signal_connect (selection, "changed",
                    G_CALLBACK (on_section_changed), self);
  on_section_changed (selection, self);

  gtk_widget_show_all (GTK_WIDGET (view));
}

static char *
get_hostname_property (CcInfoPanel *self,
		       const char  *property)
{
  GVariant *variant;
  char *str;

  variant = g_dbus_proxy_get_cached_property (self->priv->hostnamed_proxy,
                                              property);
  if (!variant)
    {
      GError *error = NULL;
      GVariant *inner;

      /* Work around systemd-hostname not sending us back
       * the property value when changing values */
      variant = g_dbus_proxy_call_sync (self->priv->hostnamed_proxy,
                                        "org.freedesktop.DBus.Properties.Get",
                                        g_variant_new ("(ss)", "org.freedesktop.hostname1", property),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
      if (variant == NULL)
        {
          g_warning ("Failed to get property '%s': %s", property, error->message);
          g_error_free (error);
          return NULL;
        }

      g_variant_get (variant, "(v)", &inner);
      str = g_variant_dup_string (inner, NULL);
      g_variant_unref (variant);
    }
  else
    {
      str = g_variant_dup_string (variant, NULL);
      g_variant_unref (variant);
    }

  return str;
}

static char *
info_panel_get_hostname (CcInfoPanel  *self)
{
  char *str;

  str = get_hostname_property (self, "PrettyHostname");

  /* Empty strings means that we need to fallback */
  if (str != NULL &&
      *str == '\0')
    {
      g_free (str);
      str = get_hostname_property (self, "Hostname");
    }

  return str;
}

static void
info_panel_set_hostname (CcInfoPanel *self,
                         const char  *text)
{
  char *hostname;
  GVariant *variant;
  GError *error = NULL;

  g_debug ("Setting PrettyHostname to '%s'", text);
  variant = g_dbus_proxy_call_sync (self->priv->hostnamed_proxy,
                                    "SetPrettyHostname",
                                    g_variant_new ("(sb)", text, FALSE),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1, NULL, &error);
  if (variant == NULL)
    {
      g_warning ("Could not set PrettyHostname: %s", error->message);
      g_error_free (error);
      error = NULL;
    }
  else
    {
      g_variant_unref (variant);
    }

  /* Set the static hostname */
  hostname = pretty_hostname_to_static (text, FALSE);
  g_assert (hostname);

  g_debug ("Setting StaticHostname to '%s'", hostname);
  variant = g_dbus_proxy_call_sync (self->priv->hostnamed_proxy,
                                    "SetStaticHostname",
                                    g_variant_new ("(sb)", hostname, FALSE),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1, NULL, &error);
  if (variant == NULL)
    {
      g_warning ("Could not set StaticHostname: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_variant_unref (variant);
    }
  g_free (hostname);
}

static void
text_changed_cb (GtkEntry        *entry,
                 CcInfoPanel     *self)
{
  const char *text;

  text = gtk_entry_get_text (GTK_ENTRY (entry));
  info_panel_set_hostname (self, text);
}

static void
info_panel_setup_hostname (CcInfoPanel  *self,
                           GPermission  *permission)
{
  char *str;
  GtkWidget *entry;
  GError *error = NULL;

  if (permission == NULL)
    {
      g_debug ("Will not show hostname, hostnamed not installed");
      return;
    }

  entry = WID ("name_entry");

  if (g_permission_get_allowed (permission) != FALSE)
    {
      g_debug ("Not allowed to change the hostname");
      gtk_widget_set_sensitive (entry, TRUE);
    }

  self->priv->hostnamed_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                               G_DBUS_PROXY_FLAGS_NONE,
                                                               NULL,
                                                               "org.freedesktop.hostname1",
                                                               "/org/freedesktop/hostname1",
                                                               "org.freedesktop.hostname1",
                                                               NULL,
                                                               &error);

  /* This could only happen if the policy file was installed
   * but not hostnamed, which points to a system bug */
  if (self->priv->hostnamed_proxy == NULL)
    {
      g_debug ("Couldn't get hostnamed to start, bailing: %s", error->message);
      g_error_free (error);
      return;
    }

  gtk_widget_show (WID ("label4"));
  gtk_widget_show (entry);

  str = info_panel_get_hostname (self);
  if (str != NULL)
    gtk_entry_set_text (GTK_ENTRY (entry), str);
  else
    gtk_entry_set_text (GTK_ENTRY (entry), "");
  g_free (str);

  g_signal_connect (G_OBJECT (entry), "changed",
		    G_CALLBACK (text_changed_cb), self);
}

static void
info_panel_setup_overview (CcInfoPanel  *self)
{
  GtkWidget  *widget;
  gboolean    res;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  char       *text;
  GPermission *permission;

  permission = polkit_permission_new_sync ("org.freedesktop.hostname1.set-static-hostname", NULL, NULL, NULL);
  /* Is hostnamed installed? */
    info_panel_setup_hostname (self, permission);

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
  gtk_widget_reparent (widget, (GtkWidget *) self);

  refresh_update_button (self);
}

static void
refresh_update_button (CcInfoPanel  *self)
{
  GtkWidget *widget;

  widget = WID ("updates_button");
  if (widget == NULL)
    return;

  switch (self->priv->updates_state)
    {
      case PK_NOT_AVAILABLE:
        gtk_widget_set_visible (widget, FALSE);
        break;
      case UPDATES_AVAILABLE:
        gtk_widget_set_sensitive (widget, TRUE);
        gtk_button_set_label (GTK_BUTTON (widget), _("Install Updates"));
        break;
      case UPDATES_NOT_AVAILABLE:
        gtk_widget_set_sensitive (widget, FALSE);
        gtk_button_set_label (GTK_BUTTON (widget), _("System Up-To-Date"));
        break;
      case CHECKING_UPDATES:
        gtk_widget_set_sensitive (widget, FALSE);
        gtk_button_set_label (GTK_BUTTON (widget), _("Checking for Updates"));
        break;
    }
}

static void
on_pk_transaction_signal (GDBusProxy *proxy,
                          char *sender_name,
                          char *signal_name,
                          GVariant *parameters,
                          CcInfoPanel *self)
{
  if (g_strcmp0 (signal_name, "Package") == 0)
    {
      self->priv->updates_state = UPDATES_AVAILABLE;
    }
  else if (g_strcmp0 (signal_name, "Finished") == 0)
    {
      if (self->priv->updates_state == CHECKING_UPDATES)
        self->priv->updates_state = UPDATES_NOT_AVAILABLE;
      refresh_update_button (self);
    }
  else if (g_strcmp0 (signal_name, "ErrorCode") == 0)
    {
      self->priv->updates_state = PK_NOT_AVAILABLE;
      refresh_update_button (self);
    }
  else if (g_strcmp0 (signal_name, "Destroy") == 0)
    {
      g_object_unref (self->priv->pk_transaction_proxy);
      self->priv->pk_transaction_proxy = NULL;
    }
}

static void
on_pk_get_updates_ready (GObject      *source,
                         GAsyncResult *res,
                         CcInfoPanel  *self)
{
  GError     *error;
  GVariant   *result;

  error = NULL;
  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (result == NULL)
    {
      g_warning ("Error getting PackageKit updates list: %s", error->message);
      g_error_free (error);
      return;
    }
}

static void
on_pk_get_tid_ready (GObject      *source,
                     GAsyncResult *res,
                     CcInfoPanel  *self)
{
  GError     *error;
  GVariant   *result;
  char       *tid;

  error = NULL;
  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (result == NULL)
    {
      if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) == FALSE)
        g_warning ("Error getting PackageKit transaction ID: %s", error->message);
      g_error_free (error);
      return;
    }

  g_variant_get (result, "(s)", &tid);

  self->priv->pk_transaction_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                                    NULL,
                                                                    "org.freedesktop.PackageKit",
                                                                    tid,
                                                                    "org.freedesktop.PackageKit.Transaction",
                                                                    NULL,
                                                                    NULL);
  g_free (tid);
  g_variant_unref (result);

  if (self->priv->pk_transaction_proxy == NULL)
    {
      g_warning ("Unable to get PackageKit transaction proxy object");
      return;
    }

  g_signal_connect (self->priv->pk_transaction_proxy,
                    "g-signal",
                    G_CALLBACK (on_pk_transaction_signal),
                    self);

  g_dbus_proxy_call (self->priv->pk_transaction_proxy,
                     "GetUpdates",
                     g_variant_new ("(s)", "none"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) on_pk_get_updates_ready,
                     self);
}

static void
refresh_updates (CcInfoPanel *self)
{
  self->priv->updates_state = CHECKING_UPDATES;
  refresh_update_button (self);

  g_assert (self->priv->pk_proxy != NULL);
  g_dbus_proxy_call (self->priv->pk_proxy,
                     "GetTid",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     (GAsyncReadyCallback) on_pk_get_tid_ready,
                     self);
}

static void
on_pk_signal (GDBusProxy *proxy,
              char *sender_name,
              char *signal_name,
              GVariant *parameters,
              CcInfoPanel *self)
{
  if (g_strcmp0 (signal_name, "UpdatesChanged") == 0)
    {
      refresh_updates (self);
    }
}

static void
on_updates_button_clicked (GtkWidget   *widget,
                           CcInfoPanel *self)
{
  GError *error;
  error = NULL;
  g_spawn_command_line_async ("gpk-update-viewer", &error);
  if (error != NULL)
    {
      g_warning ("unable to launch Software Updates: %s", error->message);
      g_error_free (error);
    }
}

static void
cc_info_panel_init (CcInfoPanel *self)
{
  GError *error = NULL;
  GtkWidget *widget;

  self->priv = INFO_PANEL_PRIVATE (self);

  self->priv->builder = gtk_builder_new ();

  self->priv->session_settings = g_settings_new (GNOME_SESSION_MANAGER_SCHEMA);
  self->priv->media_settings = g_settings_new (MEDIA_HANDLING_SCHEMA);

  self->priv->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  g_assert (self->priv->session_bus);

  self->priv->pk_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        NULL,
                                                        "org.freedesktop.PackageKit",
                                                        "/org/freedesktop/PackageKit",
                                                        "org.freedesktop.PackageKit",
                                                        NULL,
                                                        NULL);
  if (self->priv->pk_proxy == NULL)
    {
      g_warning ("Unable to get PackageKit proxy object");
      self->priv->updates_state = PK_NOT_AVAILABLE;
    }
  else
    {
      g_signal_connect (self->priv->pk_proxy,
                        "g-signal",
                        G_CALLBACK (on_pk_signal),
                        self);
      refresh_updates (self);
    }

  gtk_builder_add_from_file (self->priv->builder,
                             GNOMECC_UI_DIR "/info.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  self->priv->graphics_data = get_graphics_data ();

  widget = WID ("updates_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (on_updates_button_clicked), self);

  info_panel_setup_selector (self);
  info_panel_setup_overview (self);
  info_panel_setup_default_apps (self);
  info_panel_setup_media (self);
  info_panel_setup_graphics (self);
}

void
cc_info_panel_register (GIOModule *module)
{
  cc_info_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_INFO_PANEL,
                                  "info", 0);
}

