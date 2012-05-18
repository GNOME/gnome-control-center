/*
 * Copyright (c) 2010 Intel, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include <string.h>

#include "cc-shell-search-renderer.h"

G_DEFINE_TYPE (CcShellSearchRenderer, cc_shell_search_renderer, GTK_TYPE_CELL_RENDERER_TEXT)

#define SEARCH_RENDERER_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SHELL_TYPE_SEARCH_RENDERER, CcShellSearchRendererPrivate))

struct _CcShellSearchRendererPrivate
{
  gchar *title;
  gchar *search_target;
  gchar *search_string;

  PangoLayout *layout;
};

enum
{
  PROP_TITLE = 1,
  PROP_SEARCH_TARGET,
  PROP_SEARCH_STRING
};

#define FIXED_WIDTH 740
#define MARGIN 20
#define COLUMN_SPACING 20
#define ICON_SIZE 48
#define ICON_PADDING 10
#define FIXED_SEARCH_LABEL_WIDTH ((FIXED_WIDTH - 2*MARGIN - 3*ICON_SIZE - 6*ICON_PADDING - 2*COLUMN_SPACING)/3. - 2)

static void
cc_shell_search_renderer_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (property_id)
    {
  case PROP_TITLE:
  case PROP_SEARCH_TARGET:
  case PROP_SEARCH_STRING:
    break;


    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_shell_search_renderer_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  CcShellSearchRendererPrivate *priv = CC_SHELL_SEARCH_RENDERER (object)->priv;

  switch (property_id)
    {
  case PROP_TITLE:
    g_free (priv->title);
    priv->title = g_value_dup_string (value);
    /* set GtkCellRendererText::text for a11y */
    g_object_set (object, "text", priv->title, NULL);
    break;

  case PROP_SEARCH_TARGET:
    g_free (priv->search_target);
    priv->search_target = g_value_dup_string (value);
    break;

  case PROP_SEARCH_STRING:
    g_free (priv->search_string);
    priv->search_string = g_value_dup_string (value);
    break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_shell_search_renderer_dispose (GObject *object)
{
  CcShellSearchRendererPrivate *priv = CC_SHELL_SEARCH_RENDERER (object)->priv;

  if (priv->layout)
    {
      g_object_unref (priv->layout);
      priv->layout = NULL;
    }

  G_OBJECT_CLASS (cc_shell_search_renderer_parent_class)->dispose (object);
}

static void
cc_shell_search_renderer_finalize (GObject *object)
{
  CcShellSearchRendererPrivate *priv = CC_SHELL_SEARCH_RENDERER (object)->priv;

  if (priv->title)
    {
      g_free (priv->title);
      priv->title = NULL;
    }

  if (priv->search_target)
    {
      g_free (priv->search_target);
      priv->search_target = NULL;
    }

  if (priv->search_string)
    {
      g_free (priv->search_string);
      priv->search_string = NULL;
    }

  G_OBJECT_CLASS (cc_shell_search_renderer_parent_class)->finalize (object);
}

static void
cc_shell_search_renderer_set_layout (CcShellSearchRenderer *cell, GtkWidget *widget)
{
  gchar *display_string;
  CcShellSearchRendererPrivate *priv = cell->priv;
  gchar *needle, *haystack;
  gchar *full_string;

  if (!priv->layout)
    {
      priv->layout = pango_layout_new (gtk_widget_get_pango_context (widget));
      pango_layout_set_ellipsize (priv->layout, PANGO_ELLIPSIZE_END);
    }

  full_string = priv->search_target;

  if (priv->search_string != NULL)
    needle = g_utf8_casefold (priv->search_string, -1);
  else
    needle = NULL;
  if (full_string != NULL)
    haystack = g_utf8_casefold (full_string, -1);
  else
    haystack = NULL;

  /* clear any previous attributes */
  pango_layout_set_attributes (priv->layout, NULL);

  if (priv->search_string && priv->search_target && priv->title
      && (strstr (haystack, needle)))
    {
      gchar *start;
      gchar *lead, *trail, *leaddot;
      gchar *match;
      gint count;

#define CONTEXT 10

      count = strlen (needle);
      start = full_string + (strstr (haystack, needle) - haystack);

      lead = MAX (start - CONTEXT, full_string);
      trail = start + count;

      if (lead == full_string)
        leaddot = "";
      else
        leaddot = "…";

      match = g_strndup (start, count);
      lead = g_strndup (lead, start - lead);

      display_string = g_markup_printf_escaped ("%s\n"
                                                "<small>%s%s<b>%s</b>%s</small>",
                                                priv->title, leaddot, lead,
                                                match, trail);

      g_free (match);
      g_free (lead);
    }
  else
    display_string = g_markup_escape_text (priv->title, -1);


  pango_layout_set_markup (priv->layout, display_string, -1);
  g_free (display_string);
  g_free (needle);
  g_free (haystack);
}

static void
get_size (GtkCellRenderer *cell,
          GtkWidget       *widget,
          gint            *width,
          gint            *height)
{
  CcShellSearchRendererPrivate *priv = CC_SHELL_SEARCH_RENDERER (cell)->priv;
  PangoRectangle rect;

  cc_shell_search_renderer_set_layout (CC_SHELL_SEARCH_RENDERER (cell), widget);

  pango_layout_set_width (priv->layout, PANGO_SCALE * FIXED_SEARCH_LABEL_WIDTH);
  pango_layout_get_pixel_extents (priv->layout, NULL, &rect);

  if (width) *width = rect.width;
  if (height) *height = rect.height;
}

static void
cc_shell_search_renderer_get_preferred_width (GtkCellRenderer *cell,
                                           GtkWidget       *widget,
                                           gint            *minimum_size,
                                           gint            *natural_size)
{
  gint width;

  get_size (cell, widget, &width, NULL);
  if (minimum_size) *minimum_size = width;
  if (natural_size) *natural_size = width;
}

static void
cc_shell_search_renderer_get_preferred_height (GtkCellRenderer *cell,
                                            GtkWidget       *widget,
                                            gint            *minimum_size,
                                            gint            *natural_size)
{
  gint height;

  get_size (cell, widget, NULL, &height);
  if (minimum_size) *minimum_size = height;
  if (natural_size) *natural_size = height;
}

static void
cc_shell_search_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                      GtkWidget       *widget,
                                                      gint             width,
                                                      gint            *minimum_height,
                                                      gint            *natural_height)
{
  cc_shell_search_renderer_get_preferred_height (cell, widget, minimum_height, natural_height);
}

static void
cc_shell_search_renderer_get_aligned_area (GtkCellRenderer      *cell,
                                        GtkWidget            *widget,
                                        GtkCellRendererState  flags,
                                        const GdkRectangle   *cell_area,
                                        GdkRectangle         *aligned_area)
{
  get_size (cell, widget, &aligned_area->width, &aligned_area->height);
  aligned_area->x = cell_area->x;
  aligned_area->y = cell_area->y;
}

static void
cc_shell_search_renderer_render (GtkCellRenderer      *cell,
                              cairo_t              *cr,
                              GtkWidget            *widget,
                              const GdkRectangle   *background_area,
                              const GdkRectangle   *cell_area,
                              GtkCellRendererState  flags)
{
  CcShellSearchRendererPrivate *priv = CC_SHELL_SEARCH_RENDERER (cell)->priv;
  PangoRectangle rect;
  GtkStyleContext *context;
  gint layout_height;
  gint vcenter_offset;

  context = gtk_widget_get_style_context (widget);

  cc_shell_search_renderer_set_layout (CC_SHELL_SEARCH_RENDERER (cell), widget);

  pango_layout_get_pixel_extents (priv->layout, NULL, &rect);

  pango_layout_get_pixel_size (priv->layout, NULL, &layout_height);
  vcenter_offset = (cell_area->height - layout_height) / 2;

  cairo_save (cr);

  gtk_render_layout (context, cr,
                     cell_area->x,
                     cell_area->y + vcenter_offset,
                     priv->layout);

  cairo_restore (cr);
}

static void
cc_shell_search_renderer_class_init (CcShellSearchRendererClass *klass)
{
  GParamSpec *pspec;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_renderer = GTK_CELL_RENDERER_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcShellSearchRendererPrivate));

  object_class->get_property = cc_shell_search_renderer_get_property;
  object_class->set_property = cc_shell_search_renderer_set_property;
  object_class->dispose = cc_shell_search_renderer_dispose;
  object_class->finalize = cc_shell_search_renderer_finalize;

  cell_renderer->get_preferred_width = cc_shell_search_renderer_get_preferred_width;
  cell_renderer->get_preferred_height = cc_shell_search_renderer_get_preferred_height;
  cell_renderer->get_preferred_height_for_width = cc_shell_search_renderer_get_preferred_height_for_width;
  cell_renderer->get_aligned_area = cc_shell_search_renderer_get_aligned_area;

  cell_renderer->render = cc_shell_search_renderer_render;

  pspec = g_param_spec_string ("title",
                               "Title",
                               "Item title",
                               "",
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TITLE, pspec);

  pspec = g_param_spec_string ("search-target",
                               "Search Target",
                               "The string that will be searched",
                               "",
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SEARCH_TARGET, pspec);

  pspec = g_param_spec_string ("search-string",
                               "Search String",
                               "Current search string",
                               "",
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SEARCH_STRING, pspec);
}

static void
cc_shell_search_renderer_init (CcShellSearchRenderer *self)
{
  self->priv = SEARCH_RENDERER_PRIVATE (self);
}

CcShellSearchRenderer *
cc_shell_search_renderer_new (void)
{
  return g_object_new (SHELL_TYPE_SEARCH_RENDERER, NULL);
}
