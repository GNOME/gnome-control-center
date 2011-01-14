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

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

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

static char *
get_primary_disc_info (void)
{
  guint64       total_bytes;
  struct statfs buf;

  if (statfs ("/", &buf) < 0)
    {
      g_warning ("Unable to stat / filesystem: %s", g_strerror (errno));
      return NULL;
    }
  else
    total_bytes = (guint64) buf.f_blocks * buf.f_bsize;

  return g_format_size_for_display (total_bytes);
}

static char *
get_cpu_info (const glibtop_sysinfo *info)
{
  GHashTable    *counts;
  GString       *cpu;
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
      int count = GPOINTER_TO_INT (value);
      if (count > 1)
        g_string_append_printf (cpu, "%s \303\227 %d ", (char *)key, count);
      else
        g_string_append_printf (cpu, "%s ", (char *)key);
    }

  g_hash_table_destroy (counts);

  return g_string_free (cpu, FALSE);
}

static void
cc_info_panel_init (CcInfoPanel *self)
{
  GError     *error;
  GtkWidget  *widget;
  gboolean    res;
  glibtop_mem mem;
  const glibtop_sysinfo *info;
  char       *text;

  self->priv = INFO_PANEL_PRIVATE (self);

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_file (self->priv->builder,
                             GNOMECC_UI_DIR "/info.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  res = load_gnome_version (&self->priv->gnome_version,
                            &self->priv->gnome_distributor,
                            &self->priv->gnome_date);
  if (res)
    {
      widget = WID (self->priv->builder, "version_label");
      text = g_strdup_printf ("Version %s", self->priv->gnome_version);
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
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "os_type_label");
  text = get_os_type ();
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "disk_label");
  text = get_primary_disc_info ();
  gtk_label_set_text (GTK_LABEL (widget), text ? text : "");
  g_free (text);

  widget = WID (self->priv->builder, "info_vbox");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

void
cc_info_panel_register (GIOModule *module)
{
  cc_info_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_INFO_PANEL,
                                  "info", 0);
}

