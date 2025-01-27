/* cc-format-preview.c
 *
 * Copyright (C) 2013 Red Hat, Inc.
 * Copyright (C) 2020 System76, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Matthias Clasen
 *     Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "cc-format-preview.h"

#include <errno.h>
#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include <glib/gi18n.h>

struct _CcFormatPreview {
  AdwPreferencesGroup     parent_instance;

  AdwActionRow *date_format_row;
  AdwActionRow *date_time_format_row;
  AdwActionRow *measurement_format_row;
  AdwActionRow *number_format_row;
  AdwActionRow *paper_format_row;
  AdwActionRow *time_format_row;

  gchar     *region;
};

enum
{
  PROP_0,
  PROP_REGION
};

G_DEFINE_TYPE (CcFormatPreview, cc_format_preview, ADW_TYPE_PREFERENCES_GROUP)

static void
display_date (AdwActionRow *row, GDateTime *dt, const gchar *format)
{
  g_autofree gchar *s = g_date_time_format (dt, format);
  adw_action_row_set_subtitle (row, g_strstrip (s));
}

static void
update_format_examples (CcFormatPreview *self)
{
  const gchar *region = self->region;
  locale_t locale;
  locale_t old_locale;
  g_autoptr(GDateTime) dt = NULL;
  g_autofree gchar *s = NULL;
#ifdef LC_MEASUREMENT
  const gchar *fmt;
  gboolean is_imperial = FALSE;
#endif
  g_autoptr(GtkPaperSize) paper = NULL;

  if (region == NULL || region[0] == '\0')
    return;

  locale = newlocale (LC_TIME_MASK, region, (locale_t) 0);
  if (locale == (locale_t) 0)
    g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
  else
    old_locale = uselocale (locale);

  dt = g_date_time_new_now_local ();
  display_date (self->date_format_row, dt, "%x");
  display_date (self->time_format_row, dt, "%X");
  display_date (self->date_time_format_row, dt, "%c");

  if (locale != (locale_t) 0)
    {
      uselocale (old_locale);
      freelocale (locale);
    }

  locale = newlocale (LC_NUMERIC_MASK, region, (locale_t) 0);
  if (locale == (locale_t) 0)
    g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
  else
    old_locale = uselocale (locale);

  s = g_strdup_printf ("%'.2f", 123456789.00);
  adw_action_row_set_subtitle (self->number_format_row, s);

  if (locale != (locale_t) 0)
    {
      uselocale (old_locale);
      freelocale (locale);
    }

#if 0
  locale = newlocale (LC_MONETARY_MASK, region, (locale_t) 0);
  if (locale == (locale_t) 0)
    g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
  else
    old_locale = uselocale (locale);

  num_info = localeconv ();
  if (num_info != NULL)
    adw_action_row_set_subtitle (self->currency_format_row, num_info->currency_symbol);

  if (locale != (locale_t) 0)
    {
      uselocale (old_locale);
      freelocale (locale);
    }
#endif

#ifdef LC_MEASUREMENT
  locale = newlocale (LC_MEASUREMENT_MASK, region, (locale_t) 0);
  if (locale == (locale_t) 0)
    g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
  else
    old_locale = uselocale (locale);

  fmt = nl_langinfo (_NL_MEASUREMENT_MEASUREMENT);
  /* The returned pointer of nl_langinfo could be invalid after switching
     locale, so we must use it here. */
  is_imperial = fmt && *fmt == 2;

  if (locale != (locale_t) 0)
    {
      uselocale (old_locale);
      freelocale (locale);
    }

  if (is_imperial)
    adw_action_row_set_subtitle (self->measurement_format_row, C_("measurement format", "Imperial"));
  else
    adw_action_row_set_subtitle (self->measurement_format_row, C_("measurement format", "Metric"));

#endif

#ifdef LC_PAPER
  locale = newlocale (LC_PAPER_MASK, region, (locale_t) 0);
  if (locale == (locale_t) 0)
    g_warning ("Failed to create locale %s: %s", region, g_strerror (errno));
  else
    old_locale = uselocale (locale);

  paper = gtk_paper_size_new (gtk_paper_size_get_default ());
  adw_action_row_set_subtitle (self->paper_format_row, gtk_paper_size_get_display_name (paper));

  if (locale != (locale_t) 0)
    {
      uselocale (old_locale);
      freelocale (locale);
    }
#endif
}

static void
cc_format_preview_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcFormatPreview *self;

  self = CC_FORMAT_PREVIEW (object);

  switch (prop_id) {
  case PROP_REGION:
    cc_format_preview_set_region (self, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_format_preview_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CcFormatPreview *self;

  self = CC_FORMAT_PREVIEW (object);

  switch (prop_id) {
  case PROP_REGION:
    g_value_set_string (value, self->region);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_format_preview_finalize (GObject *object)
{
  CcFormatPreview *self = CC_FORMAT_PREVIEW (object);

  g_clear_pointer (&self->region, g_free);

  G_OBJECT_CLASS (cc_format_preview_parent_class)->finalize (object);
}

void
cc_format_preview_class_init (CcFormatPreviewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_format_preview_get_property;
  object_class->set_property = cc_format_preview_set_property;
  object_class->finalize = cc_format_preview_finalize;

  g_object_class_install_property (object_class,
                                   PROP_REGION,
                                   g_param_spec_string ("region",
                                                        "region",
                                                        "region",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/region/cc-format-preview.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFormatPreview, date_format_row);
  gtk_widget_class_bind_template_child (widget_class, CcFormatPreview, date_time_format_row);
  gtk_widget_class_bind_template_child (widget_class, CcFormatPreview, measurement_format_row);
  gtk_widget_class_bind_template_child (widget_class, CcFormatPreview, number_format_row);
  gtk_widget_class_bind_template_child (widget_class, CcFormatPreview, paper_format_row);
  gtk_widget_class_bind_template_child (widget_class, CcFormatPreview, time_format_row);

  gtk_widget_class_set_css_name (widget_class, "formatpreview");
}

void
cc_format_preview_init (CcFormatPreview *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/org/gnome/control-center/system/region/region.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

void
cc_format_preview_set_region (CcFormatPreview *self,
                              const gchar     *region)
{
  g_free (self->region);
  self->region = g_strdup (region);
  update_format_examples (self);
}
