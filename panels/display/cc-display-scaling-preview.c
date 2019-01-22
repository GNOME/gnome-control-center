/* cc-display-scaling-preview.c
 *
 * Copyright (C) 2019  Red Hat, Inc.
 *
 * Written by: Benjamin Berg <bberg@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include "cc-display-scaling-preview.h"

struct _CcDisplayScalingPreview
{
  GtkDrawingArea parent_instance;

  gboolean selected;
  gdouble scaling;
  gdouble active_scaling;
};

G_DEFINE_TYPE (CcDisplayScalingPreview, cc_display_scaling_preview, GTK_TYPE_DRAWING_AREA)

CcDisplayScalingPreview *
cc_display_scaling_preview_new (void)
{
  return g_object_new (CC_TYPE_DISPLAY_SCALING_PREVIEW, NULL);
}

static gboolean
cc_display_scaling_preview_draw (GtkWidget *widget,
                                 cairo_t   *cr)
{
  CcDisplayScalingPreview *self = CC_DISPLAY_SCALING_PREVIEW (widget);
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GtkBorder margin, border, padding;
  GtkStateFlags state = GTK_STATE_FLAG_NORMAL;
  GtkAllocation alloc;
  PangoLayout *layout;
  PangoFontDescription *font = NULL;
  PangoAttrList *attrs;
  GdkRGBA color;
  gint w, h;

  cairo_save (cr);
  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  w = alloc.width;
  h = alloc.height;

  gtk_style_context_save (context);

  if (self->selected)
    state = GTK_STATE_FLAG_SELECTED;

  gtk_style_context_set_state (context, state);

  gtk_style_context_get_margin (context, state, &margin);

  w -= margin.left + margin.right;
  h -= margin.top + margin.bottom;
  cairo_translate (cr, margin.left, margin.top);

  gtk_render_background (context, cr, 0, 0, w, h);
  gtk_render_frame (context, cr, 0, 0, w, h);

  gtk_style_context_get_border (context, state, &border);
  gtk_style_context_get_padding (context, state, &padding);

  w -= border.left + border.right;
  h -= border.top + border.bottom;
  cairo_translate (cr, border.left, border.top);
  cairo_rectangle (cr, 0, 0, w, h);
  cairo_clip (cr);

  w -= padding.left + padding.right;
  h -= padding.top + padding.bottom;
  cairo_translate (cr, padding.left, padding.top);

  /* And draw a layout with a preview string */
  gtk_style_context_get (context, state, "font", &font, NULL);
  /* TRANSLATORS: This is used to preview display scaling. The only
   * purpose is to show how the text size changes with the scale. */
  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), _("This is a sample of what text would look like. This is a sample of what text would look like."));
  pango_layout_set_font_description (layout, font);
  pango_layout_set_width (layout, w * PANGO_SCALE);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD);
  attrs = pango_attr_list_new ();
  pango_attr_list_insert (attrs, pango_attr_scale_new (self->scaling / self->active_scaling));
  pango_layout_set_attributes (layout, attrs);
  g_clear_pointer (&attrs, pango_attr_list_unref);

  gtk_style_context_get_color (context, state, &color);
  gdk_cairo_set_source_rgba (cr, &color);

  gtk_render_layout (context, cr, 0, 0, layout);
  g_object_unref (layout);

  gtk_style_context_restore (context);
  cairo_restore (cr);

  return TRUE;
}

static void
cc_display_scaling_preview_class_init (CcDisplayScalingPreviewClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->draw = cc_display_scaling_preview_draw;
}

static void
cc_display_scaling_preview_init (CcDisplayScalingPreview *self)
{
  GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (self));

  self->scaling = 1.0;
  self->active_scaling = 1.0;

  gtk_style_context_add_class (context, "frame");
  gtk_style_context_add_class (context, "view");
  gtk_style_context_add_class (context, "display-scaling-preview");
}

void
cc_display_scaling_preview_set_scaling (CcDisplayScalingPreview *preview,
                                        gdouble                  scaling,
                                        gdouble                  active_scaling)
{
  g_return_if_fail (CC_IS_DISPLAY_SCALING_PREVIEW (preview));

  preview->scaling = scaling;
  preview->active_scaling = active_scaling;

  gtk_widget_queue_draw (GTK_WIDGET (preview));
}

void
cc_display_scaling_preview_get_scaling (CcDisplayScalingPreview *preview,
                                        gdouble                 *scaling,
                                        gdouble                 *active_scaling)
{
  g_return_if_fail (CC_IS_DISPLAY_SCALING_PREVIEW (preview));

  if (scaling)
    *scaling = preview->scaling;
  if (active_scaling)
    *active_scaling = preview->active_scaling;
}

void
cc_display_scaling_preview_set_selected (CcDisplayScalingPreview *preview,
                                         gboolean                 selected)
{
  g_return_if_fail (CC_IS_DISPLAY_SCALING_PREVIEW (preview));

  if (preview->selected == selected)
    return;

  preview->selected = selected;
  gtk_widget_queue_draw (GTK_WIDGET (preview));
}

