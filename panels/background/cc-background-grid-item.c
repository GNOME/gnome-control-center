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

#include "config.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "cc-background-grid-item.h"
#include "cc-background-item.h"

struct _CcBackgroundGridItem
{
  GtkFlowBoxChild       parent;

  /* data */
  CcBackgroundItem      *item;
  GdkPixbuf             *base_pixbuf;
  GdkPixbuf             *cached_pixbuf;
  gint                  pos_zero_x;
  gint                  pos_zero_y;
};

G_DEFINE_TYPE (CcBackgroundGridItem, cc_background_grid_item, GTK_TYPE_DRAWING_AREA)

    enum {
      PROP_0,
      PROP_ITEM,
      PROP_PIXBUF_CACHE
    };

static void
add_slideshow_emblem (GdkPixbuf *pixbuf,
                      gint      w,
                      gint      h,
                      gint      scale_factor)
{
  GdkPixbuf *slideshow_emblem;
  GIcon *icon = NULL;
  GtkIconInfo *icon_info = NULL;
  GError *error = NULL;
  GtkIconTheme *theme;

  int eh;
  int ew;
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
      x = w - ew - 5;
      y = h - eh - 5;

      gdk_pixbuf_composite (slideshow_emblem, pixbuf, x, y, ew, eh, x, y, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);
    }
  }

  g_clear_object (&icon_info);
  g_clear_object (&icon);
}


static void
on_gallery_item_size_allocate (GtkWidget *widget,
                               GdkRectangle *allocation,
                               gpointer  *pointer)
{
  CcBackgroundGridItem *item = (CcBackgroundGridItem *) widget;
  GdkPixbuf *pixbuf = item->base_pixbuf;
  GdkPixbuf *new_pixbuf;
  const gint space_width = gtk_widget_get_allocated_width (widget);
  const gint space_height = gtk_widget_get_allocated_height ( (widget));
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

  if (cc_background_item_changes_with_time (cc_background_grid_item_get_item (widget))) {
    add_slideshow_emblem (new_pixbuf, (space_width + new_width) / 2, (space_height + new_height)/2, scale_factor);
  }

  if (item->cached_pixbuf != NULL)
    g_object_unref (item->cached_pixbuf);

  item->cached_pixbuf = new_pixbuf;
  item->pos_zero_x = (space_width - new_width) / 2;
  item->pos_zero_y = (space_height - new_height) / 2;
}

static gboolean
on_gallery_item_draw (GtkWidget *widget,
                      cairo_t   *cr,
                      gpointer  *pointer)
{
  CcBackgroundGridItem *item = (CcBackgroundGridItem *) widget;
  GdkPixbuf *pixbuf = item->cached_pixbuf;

  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               item->pos_zero_x,
                               item->pos_zero_y);
  cairo_paint (cr);

  return TRUE;
}

CcBackgroundGridItem *
cc_background_grid_item_new (CcBackgroundItem *item,
                             GdkPixbuf        *pixbuf)
{
  return g_object_new (CC_TYPE_BACKGROUND_GRID_ITEM,
                       "item", item,
                       "base_pixbuf", pixbuf,
                       NULL);
}

CcBackgroundItem*
cc_background_grid_item_get_item (GtkWidget *widget)
{
  if (GTK_IS_DRAWING_AREA (widget)) {
    CcBackgroundGridItem *self = (CcBackgroundGridItem *) widget;
    return self->item;
  }
  else {
    return NULL;
  }
}

static void
cc_background_grid_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_background_grid_item_parent_class)->finalize (object);
}

static void
cc_background_grid_item_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_background_grid_item_parent_class)->dispose (object);
}

static void
cc_background_grid_item_set_property (GObject      *object,
                                      guint        prop_id,
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
      g_set_object (&self->base_pixbuf, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_background_grid_item_get_property (GObject    *object,
                                      guint      prop_id,
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
      g_value_set_object (value, self->base_pixbuf);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
cc_background_grid_item_class_init (CcBackgroundGridItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
                                   g_param_spec_object ("base_pixbuf",
                                                        "Base Pixbuf for preview",
                                                        "The pixbuf base size for the preview in gallery",
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE));

}

static void
cc_background_grid_item_init (CcBackgroundGridItem *self)
{
  gtk_widget_set_hexpand(GTK_WIDGET (self), TRUE);
  gtk_widget_set_vexpand(GTK_WIDGET (self), TRUE);
  g_signal_connect (G_OBJECT (self), "draw",
                    G_CALLBACK (on_gallery_item_draw), NULL);
  g_signal_connect (G_OBJECT (self), "size-allocate",
                    G_CALLBACK (on_gallery_item_size_allocate), NULL);

  gtk_widget_set_size_request (GTK_WIDGET(self), 250, 200);
}
