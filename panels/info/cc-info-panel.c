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

#include "cc-info-panel.h"

#include <sys/vfs.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#define GNOME_SESSION_MANAGER_SCHEMA        "org.gnome.desktop.session"
#define KEY_SESSION_NAME          "session-name"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

G_DEFINE_DYNAMIC_TYPE (CcInfoPanel, cc_info_panel, CC_TYPE_PANEL)

#define INFO_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_INFO_PANEL, CcInfoPanelPrivate))

struct _CcInfoPanelPrivate
{
  GtkBuilder    *builder;
  char          *gnome_version;
  char          *gnome_distributor;
  char          *gnome_date;
  gboolean       updates_available;
  gboolean       is_fallback;

  GDBusConnection     *session_bus;
  GDBusProxy    *pk_proxy;
  GDBusProxy    *pk_transaction_proxy;
  GSettings     *session_settings;
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


static char *
get_graphics_info_lspci (void)
{
  GError     *error;
  GRegex     *re;
  GMatchInfo *match_info;
  char       *output;
  char       *result;
  GString    *info;

  info = g_string_new (NULL);

  error = NULL;
  g_spawn_command_line_sync ("lspci -nn", &output, NULL, NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to get graphics info: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  re = g_regex_new ("^[^ ]+ VGA compatible controller [^:]*: ([^([]+).*$", G_REGEX_MULTILINE, 0, &error);
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
get_graphics_info_glxinfo (void)
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
get_graphics_info (void)
{
  gchar *info;

  info = get_graphics_info_glxinfo ();
  if (info == NULL)
    info = get_graphics_info_lspci ();

  return info;
}

static gboolean
get_is_graphics_accelerated (void)
{
  GError *error = NULL;
  gchar *is_accelerated_binary;
  gchar *argv[2];
  gint estatus;

  is_accelerated_binary = g_build_filename (LIBEXECDIR, "gnome-session-is-accelerated", NULL);

  error = NULL;
  argv[0] = is_accelerated_binary;
  argv[G_N_ELEMENTS(argv)] = NULL;

  g_spawn_sync (NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                NULL, NULL, NULL, NULL, &estatus, &error);
  if (error != NULL || estatus != 0)
    return FALSE;
  else
    return TRUE;

}

static gboolean
get_current_is_fallback (CcInfoPanel  *self)
{
  GError   *error;
  GVariant *reply;
  GVariant *reply_bool;
  gboolean  is_fallback;

  error = NULL;
  if (!(reply = g_dbus_connection_call_sync (self->priv->session_bus,
                                             "org.gnome.SessionManager",
                                             "/org/gnome/SessionManager",
                                             "org.freedesktop.DBus.Properties",
                                             "Get",
                                             g_variant_new ("(ss)", "org.gnome.SessionManager", "fallback"),
                                             (GVariantType*)"(v)",
                                             0,
                                             -1,
                                             NULL, &error)))
    {
      g_warning ("Failed to get fallback mode: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }

  g_variant_get (reply, "(v)", &reply_bool);
  is_fallback = g_variant_get_boolean (reply_bool);
  g_variant_unref (reply_bool);
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

  G_OBJECT_CLASS (cc_info_panel_parent_class)->dispose (object);
}

static void
cc_info_panel_finalize (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  g_free (priv->gnome_version);
  g_free (priv->gnome_date);
  g_free (priv->gnome_distributor);

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

#define KILOBYTE_FACTOR (G_GOFFSET_CONSTANT (1000))
#define MEGABYTE_FACTOR (KILOBYTE_FACTOR * KILOBYTE_FACTOR)
#define GIGABYTE_FACTOR (MEGABYTE_FACTOR * KILOBYTE_FACTOR)
#define TERABYTE_FACTOR (GIGABYTE_FACTOR * KILOBYTE_FACTOR)
#define PETABYTE_FACTOR (TERABYTE_FACTOR * KILOBYTE_FACTOR)
#define EXABYTE_FACTOR  (PETABYTE_FACTOR * KILOBYTE_FACTOR)

static char *
format_size_for_display (goffset size)
{
  if (size < (goffset) KILOBYTE_FACTOR)
    return g_strdup_printf (g_dngettext(NULL, "%u byte", "%u bytes",(guint) size), (guint) size);
  else
    {
      gdouble displayed_size;

      if (size < (goffset) MEGABYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) KILOBYTE_FACTOR;
          return g_strdup_printf (_("%.1f KB"), displayed_size);
        }
      else if (size < (goffset) GIGABYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) MEGABYTE_FACTOR;
          return g_strdup_printf (_("%.1f MB"), displayed_size);
        }
      else if (size < (goffset) TERABYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) GIGABYTE_FACTOR;
          return g_strdup_printf (_("%.1f GB"), displayed_size);
        }
      else if (size < (goffset) PETABYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) TERABYTE_FACTOR;
          return g_strdup_printf (_("%.1f TB"), displayed_size);
        }
      else if (size < (goffset) EXABYTE_FACTOR)
        {
          displayed_size = (gdouble) size / (gdouble) PETABYTE_FACTOR;
          return g_strdup_printf (_("%.1f PB"), displayed_size);
        }
      else
        {
          displayed_size = (gdouble) size / (gdouble) EXABYTE_FACTOR;
          return g_strdup_printf (_("%.1f EB"), displayed_size);
        }
    }
}

static char *
get_primary_disc_info (void)
{
  guint64       total_bytes;
  GList        *points;
  GList        *p;

  total_bytes = 0;

  points = g_unix_mount_points_get (NULL);
  for (p = points; p != NULL; p = p->next)
    {
      GUnixMountEntry *mount = p->data;
      const char *mount_path;
      struct statfs buf;

      mount_path = g_unix_mount_get_mount_path (mount);

      if (g_str_has_prefix (mount_path, "/media/")
          || g_str_has_prefix (mount_path, g_get_home_dir ()))
        {
          g_free (mount);
          continue;
        }

      if (statfs (mount_path, &buf) < 0)
        {
          g_warning ("Unable to stat / filesystem: %s", g_strerror (errno));
          g_free (mount);
          continue;
        }
      else
        {
          total_bytes += (guint64) buf.f_blocks * buf.f_bsize;
        }

      g_free (mount);
    }
  g_list_free (points);

  return format_size_for_display (total_bytes);
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
      g_object_set (G_OBJECT (WID (self->priv->builder, "notebook")),
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
  GtkWidget *widget;

  widget = WID (self->priv->builder, "graphics_logout_warning_label");
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

  text = get_graphics_info ();
  widget = WID (self->priv->builder, "graphics_chipset_label");
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  text = NULL;
  widget = WID (self->priv->builder, "graphics_driver_label");
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  self->priv->is_fallback = get_current_is_fallback (self);
  if (self->priv->is_fallback)
    text = g_strdup (_("Fallback"));
  else
    text = g_strdup (_("Standard"));
  widget = WID (self->priv->builder, "graphics_experience_label");
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "graphics_fallback_switch_box");
  sw = GTK_SWITCH (gtk_switch_new ());
  g_settings_bind_with_mapping (self->priv->session_settings, KEY_SESSION_NAME,
                                sw, "active", 0,
                                switch_fallback_get_mapping,
                                switch_fallback_set_mapping, self, NULL);
  gtk_box_pack_start (GTK_BOX (widget), GTK_WIDGET (sw), FALSE, FALSE, 0);
  gtk_widget_show_all (GTK_WIDGET (sw));
}

static void
info_panel_setup_default_apps (CcInfoPanel  *self)
{
  GtkWidget *table;
  GtkWidget *button;

  table = WID (self->priv->builder, "default_apps_table");

  button = gtk_app_chooser_button_new ("x-scheme-handler/http");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 0, 1, GTK_FILL, 0, 0, 0);

  button = gtk_app_chooser_button_new ("x-scheme-handler/mailto");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);

  button = gtk_app_chooser_button_new ("text/calendar");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);

  button = gtk_app_chooser_button_new ("x-scheme-handler/xmpp");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 3, 4, GTK_FILL, 0, 0, 0);

  button = gtk_app_chooser_button_new ("audio/x-vorbis+ogg");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 4, 5, GTK_FILL, 0, 0, 0);

  button = gtk_app_chooser_button_new ("video/x-ogm+ogg");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 5, 6, GTK_FILL, 0, 0, 0);

  button = gtk_app_chooser_button_new ("image/jpeg");
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 6, 7, GTK_FILL, 0, 0, 0);

  gtk_widget_show_all (table);
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

  view = GTK_TREE_VIEW (WID (self->priv->builder, "overview_treeview"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  model = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (view, GTK_TREE_MODEL (model));
  g_object_unref (model);

  renderer = gtk_cell_renderer_text_new ();
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
                      _("Graphics"),
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
      widget = WID (self->priv->builder, "version_label");
      text = g_strdup_printf (_("Version %s"), self->priv->gnome_version);
      gtk_label_set_text (GTK_LABEL (widget), text);
      g_free (text);
    }

  glibtop_get_mem (&mem);
  text = g_format_size_for_display (mem.total);
  widget = WID (self->priv->builder, "memory_label");
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  info = glibtop_get_sysinfo ();

  widget = WID (self->priv->builder, "processor_label");
  text = get_cpu_info (info);
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "os_type_label");
  text = get_os_type ();
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "disk_label");
  text = get_primary_disc_info ();
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  text = get_graphics_info ();
  widget = WID (self->priv->builder, "graphics_label");
  gtk_label_set_markup (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "info_vbox");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

static void
refresh_update_button (CcInfoPanel  *self)
{
  GtkWidget *widget;

  widget = WID (self->priv->builder, "updates_button");
  if (self->priv->updates_available)
    gtk_widget_show (widget);
  else
    gtk_widget_hide (widget);
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
      self->priv->updates_available = TRUE;
    }
  else if (g_strcmp0 (signal_name, "Finished") == 0)
    {
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
  self->priv->updates_available = FALSE;

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
    g_warning ("Unable to get PackageKit proxy object");
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

  widget = WID (self->priv->builder, "updates_button");
  g_signal_connect (widget, "clicked", G_CALLBACK (on_updates_button_clicked), self);

  info_panel_setup_selector (self);
  info_panel_setup_overview (self);
  info_panel_setup_default_apps (self);
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

