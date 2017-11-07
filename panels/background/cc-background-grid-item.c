/* cc-background-grid-item.c
 *
 * Copyright (C) 2017 Julian Sparber <julian@sparber.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include "cc-background-grid-item.h"
#include "cc-background-item.h"

struct _CcBackgroundGridItem
{
  GtkFlowBoxChild            parent;

  /* data */
  CcBackgroundItem      *item;
  GdkPixbuf *cached_pixbuf;
};


G_DEFINE_TYPE (CcBackgroundGridItem, cc_background_grid_item, GTK_TYPE_FLOW_BOX_CHILD)

    enum {
      PROP_0,
      PROP_ITEM,
      PROP_PIXBUF_CACHE
    };

static void
add_slideshow_emblem (GdkPixbuf *pixbuf,
                      gint w,
                      gint h,
                      gint scale_factor)
{
  GdkPixbuf *slideshow_emblem;
  GIcon *icon = NULL;
  GtkIconInfo *icon_info = NULL;
  GError *error = NULL;
  GtkIconTheme *theme;

  int eh;
  int ew;
  //int h;
  //int w;
  int x;
  int y;

  icon = g_themed_icon_new ("slideshow-emblem");
  theme = gtk_icon_theme_get_default ();
  icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (theme,
                                                        icon,
                                                        16,
                                                        scale_factor,
                                                        GTK_ICON_LOOKUP_FORCE_SIZE |
                                                        GTK_ICON_LOOKUP_USE_BUILTIN);
  if (icon_info == NULL) {
    g_warning ("Your icon theme is missing the slideshow-emblem icon, "
               "please file a bug against it");
  }
  else {

    slideshow_emblem = gtk_icon_info_load_icon (icon_info, &error);
    if (slideshow_emblem == NULL) {
      g_warning ("Failed to load slideshow emblem: %s", error->message);
      g_error_free (error);
    }
    else {
      eh = gdk_pixbuf_get_height (slideshow_emblem);
      ew = gdk_pixbuf_get_width (slideshow_emblem);
      //h = gdk_pixbuf_get_height (pixbuf);
      //w = gdk_pixbuf_get_width (pixbuf);
      x = w - ew - 5;
      y = h - eh - 5;

      gdk_pixbuf_composite (slideshow_emblem, pixbuf, x, y, ew, eh, x, y, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
    }
  }

  g_clear_object (&icon_info);
  g_clear_object (&icon);
}

static gboolean
on_gallery_item_draw (GtkWidget         *widget,
                      cairo_t           *cr,
                      CcBackgroundGridItem *item)
{
  GdkPixbuf *pixbuf = item->cached_pixbuf;
  GdkPixbuf *new_pixbuf;
  const gint space_width = gtk_widget_get_allocated_width (widget);
  const gint space_height = gtk_widget_get_allocated_height ( (widget));
  //const gint pixbuf_width = gdk_pixbuf_get_width (pixbuf);
  //const gint pixbuf_height = gdk_pixbuf_get_height (pixbuf);
  const gint scale_factor = gtk_widget_get_scale_factor (widget);
  gint new_width;
  gint new_height;

  if (space_width * 9/16 > space_height) {
    new_width = space_width;
    new_height = space_width * 9/16;
  }
  else {
    new_width = space_height * 16/9;
    new_height = space_height;
  }

  new_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
                                        new_width,
                                        new_height,
                                        GDK_INTERP_BILINEAR);

  if (cc_background_item_changes_with_time (cc_background_grid_item_get_ref (gtk_widget_get_parent(widget)))) {
    add_slideshow_emblem (new_pixbuf, (space_width + new_width) / 2, (space_height + new_height)/2, scale_factor);
  }


  gdk_cairo_set_source_pixbuf (cr,
                               new_pixbuf,
                               (space_width - new_width) / 2,
                               (space_height - new_height) / 2);

  g_object_unref (new_pixbuf);
  cairo_paint (cr);

  return TRUE;
}

GtkWidget*
cc_background_grid_item_new (CcBackgroundItem *item, GdkPixbuf *pixbuf)
{

  return g_object_new (CC_TYPE_BACKGROUND_GRID_ITEM,
                       "item", item,
                       "cached_pixbuf", pixbuf,
                       NULL);
}

CcBackgroundItem * cc_background_grid_item_get_ref (GtkWidget *widget)
{
  CcBackgroundGridItem *self = (CcBackgroundGridItem *) widget;
  return self->item;
}
void
cc_background_grid_item_set_ref (GtkWidget *widget, CcBackgroundItem *item)
{
  CcBackgroundGridItem *self = (CcBackgroundGridItem *) widget;
  self->item = item;
}

static void
cc_background_grid_item_finalize (GObject *object)
{
  //CcBackgroundGridItem *self = CC_BACKGROUND_GRID_ITEM (object);

  G_OBJECT_CLASS (cc_background_grid_item_parent_class)->finalize (object);

}

static void
cc_background_grid_item_dispose (GObject *object)
{
  //CcBackgroundGridItem *self = CC_BACKGROUND_GRID_ITEM (object);

  G_OBJECT_CLASS (cc_background_grid_item_parent_class)->dispose (object);
}

static void
cc_background_grid_item_set_property (GObject *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  CcBackgroundGridItem *self = (CcBackgroundGridItem *) object;
  switch (prop_id)
    {

    case PROP_ITEM:
      g_set_object (&self->item, g_value_get_object (value));
      break;
    case PROP_PIXBUF_CACHE:
      g_set_object (&self->cached_pixbuf, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_grid_item_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  CcBackgroundGridItem *self = (CcBackgroundGridItem *) object;

  switch (prop_id)
    {
    case PROP_ITEM:
      g_value_set_object (value, self->item);
      break;

    case PROP_PIXBUF_CACHE:
      g_value_set_object (value, self->cached_pixbuf);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
cc_background_grid_item_class_init (CcBackgroundGridItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  //GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_background_grid_item_finalize;
  object_class->dispose = cc_background_grid_item_dispose;
  object_class->get_property = cc_background_grid_item_get_property;
  object_class->set_property = cc_background_grid_item_set_property;

  g_object_class_install_property (object_class,
                                   PROP_ITEM,
                                   g_param_spec_object ("item",
                                                        "Background item reference",
                                                        "The reference to this background item",
                                                        CC_TYPE_BACKGROUND_ITEM,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_PIXBUF_CACHE,
                                   g_param_spec_object ("cached_pixbuf",
                                                        "Cached Pixbuf for preview",
                                                        "The pixbuf for caching the preview in gallery",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE));

}

static void
cc_background_grid_item_init (CcBackgroundGridItem *self)
{
  GtkWidget *drawing;

  drawing = gtk_drawing_area_new ();
  gtk_widget_set_hexpand(drawing, TRUE);
  gtk_widget_set_vexpand(drawing, TRUE);
  g_signal_connect (G_OBJECT (drawing), "draw",
                    G_CALLBACK (on_gallery_item_draw), self);

  gtk_widget_set_size_request (self, 250, 200);
  gtk_widget_show (drawing);
  gtk_container_add (GTK_CONTAINER (self), drawing);
}
