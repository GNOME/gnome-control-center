/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>

#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

#include "cc-background-panel.h"
#include "bg-wallpapers-source.h"
#include "bg-pictures-source.h"
#include "bg-colors-source.h"

#ifdef HAVE_LIBSOCIALWEB
#include "bg-flickr-source.h"
#endif

#include "cc-background-item.h"
#include "cc-background-xml.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_URI_KEY "picture-uri"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

enum {
  COL_SOURCE_NAME,
  COL_SOURCE_TYPE,
  COL_SOURCE,
  NUM_COLS
};

G_DEFINE_DYNAMIC_TYPE (CcBackgroundPanel, cc_background_panel, CC_TYPE_PANEL)

#define BACKGROUND_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BACKGROUND_PANEL, CcBackgroundPanelPrivate))

struct _CcBackgroundPanelPrivate
{
  GtkBuilder *builder;

  BgWallpapersSource *wallpapers_source;
  BgPicturesSource *pictures_source;
  BgColorsSource *colors_source;

#ifdef HAVE_LIBSOCIALWEB
  BgFlickrSource *flickr_source;
#endif

  GSettings *settings;

  GnomeDesktopThumbnailFactory *thumb_factory;

  CcBackgroundItem *current_background;
  gint current_source;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;

  GdkPixbuf *display_base;
  GdkPixbuf *display_overlay;
};

enum
{
  SOURCE_WALLPAPERS,
  SOURCE_PICTURES,
  SOURCE_COLORS,
#ifdef HAVE_LIBSOCIALWEB
  SOURCE_FLICKR
#endif
};


#define WID(y) (GtkWidget *) gtk_builder_get_object (priv->builder, y)

static void
cc_background_panel_get_property (GObject    *object,
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
cc_background_panel_set_property (GObject      *object,
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
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanelPrivate *priv = CC_BACKGROUND_PANEL (object)->priv;

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;

      /* destroying the builder object will also destroy the spinner */
      priv->spinner = NULL;
    }

  if (priv->wallpapers_source)
    {
      g_object_unref (priv->wallpapers_source);
      priv->wallpapers_source = NULL;
    }

  if (priv->pictures_source)
    {
      g_object_unref (priv->pictures_source);
      priv->pictures_source = NULL;
    }

  if (priv->colors_source)
    {
      g_object_unref (priv->colors_source);
      priv->colors_source = NULL;
    }
#ifdef HAVE_LIBSOCIALWEB
  if (priv->flickr_source)
    {
      g_object_unref (priv->flickr_source);
      priv->flickr_source = NULL;
    }
#endif

  if (priv->settings)
    {
      g_object_unref (priv->settings);
      priv->settings = NULL;
    }

  if (priv->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (priv->copy_cancellable);

      g_object_unref (priv->copy_cancellable);
      priv->copy_cancellable = NULL;
    }

  if (priv->thumb_factory)
    {
      g_object_unref (priv->thumb_factory);
      priv->thumb_factory = NULL;
    }

  if (priv->display_base)
    {
      g_object_unref (priv->display_base);
      priv->display_base = NULL;
    }

  if (priv->display_overlay)
    {
      g_object_unref (priv->display_overlay);
      priv->display_overlay = NULL;
    }

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanelPrivate *priv = CC_BACKGROUND_PANEL (object)->priv;

  if (priv->current_background)
    {
      g_object_unref (priv->current_background);
      priv->current_background = NULL;
    }

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcBackgroundPanelPrivate));

  object_class->get_property = cc_background_panel_get_property;
  object_class->set_property = cc_background_panel_set_property;
  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;
}

static void
cc_background_panel_class_finalize (CcBackgroundPanelClass *klass)
{
}

static void
source_update_edit_box (CcBackgroundPanelPrivate *priv,
			gboolean                  initial)
{
  CcBackgroundItemFlags flags;

  flags = cc_background_item_get_flags (priv->current_background);

  if ((flags & CC_BACKGROUND_ITEM_HAS_SCOLOR &&
       priv->current_source != SOURCE_COLORS) ||
      cc_background_item_get_shading (priv->current_background) == G_DESKTOP_BACKGROUND_SHADING_SOLID)
    gtk_widget_hide (WID ("style-scolor"));
  else
    gtk_widget_show (WID ("style-scolor"));

  if (flags & CC_BACKGROUND_ITEM_HAS_PCOLOR &&
      priv->current_source != SOURCE_COLORS)
    gtk_widget_hide (WID ("style-pcolor"));
  else
    gtk_widget_show (WID ("style-pcolor"));

  if (gtk_widget_get_visible (WID ("style-pcolor")) &&
      gtk_widget_get_visible (WID ("style-scolor")))
    gtk_widget_show (WID ("swap-color-button"));
  else
    gtk_widget_hide (WID ("swap-color-button"));

  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT ||
      cc_background_item_get_uri (priv->current_background) == NULL)
    gtk_widget_hide (WID ("style-combobox"));
  else
    gtk_widget_show (WID ("style-combobox"));

  /* FIXME What to do if the background has a gradient shading
   * and provides the colours? */
}

static void
source_changed_cb (GtkComboBox              *combo,
                   CcBackgroundPanelPrivate *priv)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkIconView *view;
  guint type;
  BgSource *source;

  gtk_combo_box_get_active_iter (combo, &iter);
  model = gtk_combo_box_get_model (combo);
  gtk_tree_model_get (model, &iter,
                      COL_SOURCE_TYPE, &type,
                      COL_SOURCE, &source, -1);

  view = (GtkIconView *) gtk_builder_get_object (priv->builder,
                                                 "backgrounds-iconview");

  gtk_icon_view_set_model (view,
                           GTK_TREE_MODEL (bg_source_get_liststore (source)));
}

static void
select_style (GtkComboBox *box,
	      GDesktopBackgroundStyle new_style)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean cont;

  model = gtk_combo_box_get_model (box);
  cont = gtk_tree_model_get_iter_first (model, &iter);
  while (cont != FALSE)
    {
      GDesktopBackgroundStyle style;

      gtk_tree_model_get (model, &iter,
			  1, &style,
			  -1);

      if (style == new_style)
        {
          gtk_combo_box_set_active_iter (box, &iter);
          break;
	}
      cont = gtk_tree_model_iter_next (model, &iter);
    }

  if (cont == FALSE)
    gtk_combo_box_set_active (box, -1);
}

static void
update_preview (CcBackgroundPanelPrivate *priv,
                CcBackgroundItem         *item)
{
  gchar *markup;
  gboolean changes_with_time;

  if (item && priv->current_background)
    {
      g_object_unref (priv->current_background);
      priv->current_background = cc_background_item_copy (item);
      cc_background_item_load (priv->current_background, NULL);
    }

  source_update_edit_box (priv, FALSE);

  changes_with_time = FALSE;

  if (priv->current_background)
    {
      GdkColor pcolor, scolor;

      markup = g_strdup_printf ("<b>%s</b>", cc_background_item_get_name (priv->current_background));
      gtk_label_set_markup (GTK_LABEL (WID ("background-label")), markup);
      g_free (markup);

      gtk_label_set_text (GTK_LABEL (WID ("size_label")), cc_background_item_get_size (priv->current_background));

      gdk_color_parse (cc_background_item_get_pcolor (priv->current_background), &pcolor);
      gdk_color_parse (cc_background_item_get_scolor (priv->current_background), &scolor);

      gtk_color_button_set_color (GTK_COLOR_BUTTON (WID ("style-pcolor")), &pcolor);
      gtk_color_button_set_color (GTK_COLOR_BUTTON (WID ("style-scolor")), &scolor);

      select_style (GTK_COMBO_BOX (WID ("style-combobox")),
                    cc_background_item_get_placement (priv->current_background));

      changes_with_time = cc_background_item_changes_with_time (priv->current_background);
    }

  gtk_widget_set_visible (WID ("slide_image"), changes_with_time);
  gtk_widget_set_visible (WID ("slide-label"), changes_with_time);

  gtk_widget_queue_draw (WID ("preview-area"));
}

static char *
get_save_path (void)
{
  return g_build_filename (g_get_user_config_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   "last-edited.xml",
			   NULL);
}

static gboolean
create_save_dir (void)
{
  char *path;

  path = g_build_filename (g_get_user_config_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   NULL);
  if (g_mkdir_with_parents (path, 0755) < 0)
    {
      g_warning ("Failed to create directory '%s'", path);
      g_free (path);
      return FALSE;
    }
  g_free (path);
  return TRUE;
}

static void
copy_finished_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      pointer)
{
  GError *err = NULL;
  CcBackgroundPanel *panel = (CcBackgroundPanel *) pointer;
  CcBackgroundPanelPrivate *priv = panel->priv;
  CcBackgroundItem *item;

  if (!g_file_copy_finish (G_FILE (source_object), result, &err))
    {
      if (err->code != G_IO_ERROR_CANCELLED)
        g_warning ("Failed to copy image to cache location: %s", err->message);

      g_error_free (err);
    }
  item = g_object_get_data (source_object, "item");

  /* the panel may have been destroyed before the callback is run, so be sure
   * to check the widgets are not NULL */

  if (priv->spinner)
    {
      gtk_widget_destroy (GTK_WIDGET (priv->spinner));
      priv->spinner = NULL;
    }

  if (priv->current_background)
    cc_background_item_load (priv->current_background, NULL);

  if (priv->builder)
    {
      char *filename;

      update_preview (priv, item);

      /* Save the source XML if there is one */
      filename = get_save_path ();
      if (create_save_dir ())
        cc_background_xml_save (priv->current_background, filename);
    }

  /* remove the reference taken when the copy was set up */
  g_object_unref (panel);
}

static void
update_remove_button (CcBackgroundPanel *panel,
		      CcBackgroundItem  *item)
{
  CcBackgroundPanelPrivate *priv;
  const char *uri;
  char *cache_path;
  GFile *bg, *cache, *parent;
  gboolean sensitive = FALSE;

  priv = panel->priv;

  if (priv->current_source != SOURCE_PICTURES)
    goto bail;

  uri = cc_background_item_get_uri (item);
  if (uri == NULL)
    goto bail;

  bg = g_file_new_for_uri (uri);
  parent = g_file_get_parent (bg);
  if (parent == NULL)
    {
      g_object_unref (bg);
      goto bail;
    }
  cache_path = bg_pictures_source_get_cache_path ();
  cache = g_file_new_for_path (cache_path);
  g_free (cache_path);

  if (g_file_equal (parent, cache))
    sensitive = TRUE;

  g_object_unref (parent);
  g_object_unref (cache);

bail:
  gtk_widget_set_sensitive (WID ("remove_button"), sensitive);

}

static CcBackgroundItem *
get_selected_item (CcBackgroundPanel *panel)
{
  CcBackgroundPanelPrivate *priv = panel->priv;
  GtkIconView *icon_view;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GList *list;
  CcBackgroundItem *item;

  icon_view = GTK_ICON_VIEW (WID ("backgrounds-iconview"));
  item = NULL;
  list = gtk_icon_view_get_selected_items (icon_view);

  if (!list)
    return NULL;

  model = gtk_icon_view_get_model (icon_view);

  if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data) == FALSE)
    goto bail;

  gtk_tree_model_get (model, &iter, 1, &item, -1);

bail:
  g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (list);

  return item;
}

static void
backgrounds_changed_cb (GtkIconView       *icon_view,
                        CcBackgroundPanel *panel)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  CcBackgroundItem *item;
  CcBackgroundPanelPrivate *priv = panel->priv;
  char *pcolor, *scolor;
  gboolean draw_preview = TRUE;
  const char *uri;
  CcBackgroundItemFlags flags;
  char *filename;

  item = get_selected_item (panel);

  if (item == NULL)
    return;

  /* Update current source */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sources-combobox")));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("sources-combobox")),
                                 &iter);
  gtk_tree_model_get (model, &iter,
		      COL_SOURCE_TYPE, &priv->current_source, -1);

  uri = cc_background_item_get_uri (item);
  flags = cc_background_item_get_flags (item);

  if ((flags & CC_BACKGROUND_ITEM_HAS_URI) && uri == NULL)
    {
      g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, G_DESKTOP_BACKGROUND_STYLE_NONE);
      g_settings_set_string (priv->settings, WP_URI_KEY, "");
    }
  else if (cc_background_item_get_source_url (item) != NULL &&
	   cc_background_item_get_needs_download (item))
    {
      GFile *source, *dest;
      gchar *cache_path, *basename, *dest_path, *display_name, *dest_uri;
      GdkPixbuf *pixbuf;

      cache_path = bg_pictures_source_get_cache_path ();
      if (g_mkdir_with_parents (cache_path, 0755) < 0)
        {
          g_warning ("Failed to create directory '%s'", cache_path);
          g_free (cache_path);
          return;
	}
      g_free (cache_path);

      dest_path = bg_pictures_source_get_unique_path (cc_background_item_get_source_url (item));
      dest = g_file_new_for_path (dest_path);
      g_free (dest_path);
      source = g_file_new_for_uri (cc_background_item_get_source_url (item));
      basename = g_file_get_basename (source);
      display_name = g_filename_display_name (basename);
      dest_path = g_file_get_path (dest);
      g_free (basename);

      /* create a blank image to use until the source image is ready */
      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
      gdk_pixbuf_fill (pixbuf, 0x00000000);
      gdk_pixbuf_save (pixbuf, dest_path, "png", NULL, NULL);
      g_object_unref (pixbuf);
      g_free (dest_path);

      if (priv->copy_cancellable)
        {
          g_cancellable_cancel (priv->copy_cancellable);
          g_cancellable_reset (priv->copy_cancellable);
        }

      if (priv->spinner)
        {
          gtk_widget_destroy (GTK_WIDGET (priv->spinner));
          priv->spinner = NULL;
        }

      /* create a spinner while the file downloads */
      priv->spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (priv->spinner));
      gtk_box_pack_start (GTK_BOX (WID ("bottom-hbox")), priv->spinner, FALSE,
                          FALSE, 6);
      gtk_widget_show (priv->spinner);

      /* reference the panel in case it is removed before the copy is
       * finished */
      g_object_ref (panel);
      g_object_set_data_full (G_OBJECT (source), "item", g_object_ref (item), g_object_unref);
      g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, priv->copy_cancellable,
                         NULL, NULL,
                         copy_finished_cb, panel);
      g_object_unref (source);
      dest_uri = g_file_get_uri (dest);
      g_object_unref (dest);

      g_settings_set_string (priv->settings, WP_URI_KEY, dest_uri);
      g_object_set (G_OBJECT (item),
		    "uri", dest_uri,
		    "needs-download", FALSE,
		    "name", display_name,
		    NULL);
      g_free (display_name);
      g_free (dest_uri);

      /* delay the updated drawing of the preview until the copy finishes */
      draw_preview = FALSE;
    }
  else
    {
      g_settings_set_string (priv->settings, WP_URI_KEY, uri);
    }

  /* Also set the placement if we have a URI and the previous value was none */
  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    {
      g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }
  else if (uri != NULL)
    {
      GDesktopBackgroundStyle style;
      style = g_settings_get_enum (priv->settings, WP_OPTIONS_KEY);
      if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
        g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_SHADING)
    g_settings_set_enum (priv->settings, WP_SHADING_KEY, cc_background_item_get_shading (item));

  /* When changing to a background with colours set,
   * don't overwrite what's in GSettings, but read
   * from it instead.
   * We have a hack for the colors source though */
  if (flags & CC_BACKGROUND_ITEM_HAS_PCOLOR &&
      priv->current_source != SOURCE_COLORS)
    {
      g_settings_set_string (priv->settings, WP_PCOLOR_KEY, cc_background_item_get_pcolor (item));
    }
  else
    {
      pcolor = g_settings_get_string (priv->settings, WP_PCOLOR_KEY);
      g_object_set (G_OBJECT (item), "primary-color", pcolor, NULL);
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_SCOLOR &&
      priv->current_source != SOURCE_COLORS)
    {
      g_settings_set_string (priv->settings, WP_SCOLOR_KEY, cc_background_item_get_scolor (item));
    }
  else
    {
      scolor = g_settings_get_string (priv->settings, WP_SCOLOR_KEY);
      g_object_set (G_OBJECT (item), "secondary-color", scolor, NULL);
    }

  /* Apply all changes */
  g_settings_apply (priv->settings);

  update_remove_button (panel, item);

  /* update the preview information */
  if (draw_preview != FALSE)
    {
      update_preview (priv, item);

      /* Save the source XML if there is one */
      filename = get_save_path ();
      if (create_save_dir ())
        cc_background_xml_save (priv->current_background, filename);
    }
}

static gboolean
preview_draw_cb (GtkWidget         *widget,
                 cairo_t           *cr,
                 CcBackgroundPanel *panel)
{
  GtkAllocation allocation;
  CcBackgroundPanelPrivate *priv = panel->priv;
  GdkPixbuf *pixbuf = NULL;
  const gint preview_width = 416;
  const gint preview_height = 248;
  const gint preview_x = 45;
  const gint preview_y = 84;
  GdkPixbuf *preview, *temp;
  gint size;

  gtk_widget_get_allocation (widget, &allocation);

  if (priv->current_background)
    {
      GIcon *icon;
      icon = cc_background_item_get_frame_thumbnail (priv->current_background,
                                                     priv->thumb_factory,
                                                     preview_width,
                                                     preview_height,
                                                     -2, TRUE);
      pixbuf = GDK_PIXBUF (icon);
    }

  if (!priv->display_base)
    return FALSE;


  preview = gdk_pixbuf_copy (priv->display_base);

  if (pixbuf)
    {
      gdk_pixbuf_composite (pixbuf, preview,
                            preview_x, preview_y,
                            preview_width, preview_height,
                            preview_x, preview_y, 1, 1,
                            GDK_INTERP_BILINEAR, 255);

      g_object_unref (pixbuf);
    }


  if (priv->display_overlay)
    {
      gdk_pixbuf_composite (priv->display_overlay, preview,
                            0, 0, 512, 512,
                            0, 0, 1, 1,
                            GDK_INTERP_BILINEAR, 255);
    }


  if (allocation.width < allocation.height)
    size = allocation.width;
  else
    size = allocation.height;

  temp = gdk_pixbuf_scale_simple (preview, size, size, GDK_INTERP_BILINEAR);

  gdk_cairo_set_source_pixbuf (cr,
                               temp,
                               allocation.width / 2 - (size / 2),
                               allocation.height / 2 - (size / 2));
  cairo_paint (cr);

  g_object_unref (temp);
  g_object_unref (preview);

  return TRUE;
}

static void
style_changed_cb (GtkComboBox       *box,
                  CcBackgroundPanel *panel)
{
  CcBackgroundPanelPrivate *priv = panel->priv;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GDesktopBackgroundStyle value;

  if (!gtk_combo_box_get_active_iter (box, &iter))
    {
      return;
    }

  model = gtk_combo_box_get_model (box);

  gtk_tree_model_get (model, &iter, 1, &value, -1);

  g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, value);

  if (priv->current_background)
    g_object_set (G_OBJECT (priv->current_background), "placement", value, NULL);

  g_settings_apply (priv->settings);

  update_preview (priv, NULL);
}

static void
color_changed_cb (GtkColorButton    *button,
                  CcBackgroundPanel *panel)
{
  CcBackgroundPanelPrivate *priv = panel->priv;
  GdkColor color;
  gchar *value;
  gboolean is_pcolor = FALSE;

  gtk_color_button_get_color (button, &color);
  if (WID ("style-pcolor") == GTK_WIDGET (button))
    is_pcolor = TRUE;

  value = gdk_color_to_string (&color);

  if (priv->current_background)
    {
      g_object_set (G_OBJECT (priv->current_background),
		    is_pcolor ? "primary-color" : "secondary-color", value, NULL);
    }

  g_settings_set_string (priv->settings,
			 is_pcolor ? WP_PCOLOR_KEY : WP_SCOLOR_KEY, value);

  g_settings_apply (priv->settings);

  g_free (value);

  update_preview (priv, NULL);
}

static void
swap_colors_clicked (GtkButton         *button,
                     CcBackgroundPanel *panel)
{
  CcBackgroundPanelPrivate *priv = panel->priv;
  GdkColor pcolor, scolor;
  char *new_pcolor, *new_scolor;

  gtk_color_button_get_color (GTK_COLOR_BUTTON (WID ("style-pcolor")), &pcolor);
  gtk_color_button_get_color (GTK_COLOR_BUTTON (WID ("style-scolor")), &scolor);

  gtk_color_button_set_color (GTK_COLOR_BUTTON (WID ("style-scolor")), &pcolor);
  gtk_color_button_set_color (GTK_COLOR_BUTTON (WID ("style-pcolor")), &scolor);

  new_pcolor = gdk_color_to_string (&scolor);
  new_scolor = gdk_color_to_string (&pcolor);

  g_object_set (priv->current_background,
                "primary-color", new_pcolor,
                "secondary-color", new_scolor,
                NULL);

  g_settings_set_string (priv->settings, WP_PCOLOR_KEY, new_pcolor);
  g_settings_set_string (priv->settings, WP_SCOLOR_KEY, new_scolor);

  g_free (new_pcolor);
  g_free (new_scolor);

  g_settings_apply (priv->settings);

  update_preview (priv, NULL);
}

static void
row_inserted (GtkTreeModel      *tree_model,
	      GtkTreePath       *path,
	      GtkTreeIter       *iter,
	      CcBackgroundPanel *panel)
{
  GtkListStore *store;
  CcBackgroundPanelPrivate *priv;

  priv = panel->priv;

  store = bg_source_get_liststore (BG_SOURCE (panel->priv->pictures_source));
  g_signal_handlers_disconnect_by_func (G_OBJECT (store), G_CALLBACK (row_inserted), panel);

  /* Change source */
  gtk_combo_box_set_active (GTK_COMBO_BOX (WID ("sources-combobox")), SOURCE_PICTURES);

  /* And select the newly added item */
  gtk_icon_view_select_path (GTK_ICON_VIEW (WID ("backgrounds-iconview")), path);
}

static void
add_custom_wallpaper (CcBackgroundPanel *panel,
		      const char        *uri)
{
  GtkListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (panel->priv->pictures_source));
  g_signal_connect (G_OBJECT (store), "row-inserted",
		    G_CALLBACK (row_inserted), panel);

  if (bg_pictures_source_add (panel->priv->pictures_source, uri) == FALSE) {
    g_signal_handlers_disconnect_by_func (G_OBJECT (store), G_CALLBACK (row_inserted), panel);
    return;
  }

  /* Wait for the item to get added */
}

static void
file_chooser_response (GtkDialog         *chooser,
                       gint               response,
                       CcBackgroundPanel *panel)
{
  GSList *selected, *l;

  if (response != GTK_RESPONSE_ACCEPT)
    {
      gtk_widget_destroy (GTK_WIDGET (chooser));
      return;
    }

  selected = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
  gtk_widget_destroy (GTK_WIDGET (chooser));

  for (l = selected; l != NULL; l = l->next)
    {
      char *uri = l->data;
      add_custom_wallpaper (panel, uri);
      g_free (uri);
    }
  g_slist_free (selected);
}

static void
update_chooser_preview (GtkFileChooser    *chooser,
			CcBackgroundPanel *panel)
{
  GnomeDesktopThumbnailFactory *thumb_factory;
  char *uri;

  thumb_factory = panel->priv->thumb_factory;

  uri = gtk_file_chooser_get_preview_uri (chooser);

  if (uri)
    {
      GdkPixbuf *pixbuf = NULL;
      const gchar *mime_type = NULL;
      GFile *file;
      GFileInfo *file_info;
      GtkWidget *preview;

      preview = gtk_file_chooser_get_preview_widget (chooser);

      file = g_file_new_for_uri (uri);
      file_info = g_file_query_info (file,
				     G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				     G_FILE_QUERY_INFO_NONE,
				     NULL, NULL);
      g_object_unref (file);

      if (file_info != NULL) {
	      mime_type = g_file_info_get_content_type (file_info);
	      g_object_unref (file_info);
      }

      if (mime_type)
        {
        pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumb_factory,
								     uri,
								     mime_type);
	}

      gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
					 GTK_RESPONSE_ACCEPT,
					 (pixbuf != NULL));

      if (pixbuf != NULL)
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
	  g_object_unref (pixbuf);
	}
      else
        {
          gtk_image_set_from_stock (GTK_IMAGE (preview),
				    GTK_STOCK_DIALOG_QUESTION,
				    GTK_ICON_SIZE_DIALOG);
	}

      if (bg_pictures_source_is_known (panel->priv->pictures_source, uri))
        gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT, FALSE);
      else
        gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT, TRUE);

      g_free (uri);
    }

  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static void
add_button_clicked (GtkButton         *button,
		    CcBackgroundPanel *panel)
{
  GtkWidget *chooser;
  const gchar *folder;
  GtkWidget *preview;
  GtkFileFilter *filter;
  CcBackgroundPanelPrivate *priv;

  priv = panel->priv;

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/*");

  chooser = gtk_file_chooser_dialog_new (_("Browse for more pictures"),
					 GTK_WINDOW (gtk_widget_get_toplevel (WID ("background-panel"))),
					 GTK_FILE_CHOOSER_ACTION_OPEN,
					 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					 NULL);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), TRUE);

  gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);

  preview = gtk_image_new ();
  gtk_widget_set_size_request (preview, 128, -1);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview);
  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (chooser), FALSE);
  gtk_widget_show (preview);
  g_signal_connect (chooser, "update-preview",
		    G_CALLBACK (update_chooser_preview), panel);

  folder = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (folder)
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					 folder);

  g_signal_connect (chooser, "response",
		    G_CALLBACK (file_chooser_response), panel);

  gtk_window_present (GTK_WINDOW (chooser));
}

static void
remove_button_clicked (GtkButton         *button,
		       CcBackgroundPanel *panel)
{
  CcBackgroundItem *item;
  GtkListStore *store;
  GtkTreePath *path;
  CcBackgroundPanelPrivate *priv;

  priv = panel->priv;

  item = get_selected_item (panel);
  if (item == NULL)
    g_assert_not_reached ();

  bg_pictures_source_remove (panel->priv->pictures_source, item);
  g_object_unref (item);

  /* Are there any items left in the pictures tree store? */
  store = bg_source_get_liststore (BG_SOURCE (panel->priv->pictures_source));
  if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 0)
    gtk_combo_box_set_active (GTK_COMBO_BOX (WID ("sources-combobox")), SOURCE_WALLPAPERS);

  path = gtk_tree_path_new_from_string ("0");
  gtk_icon_view_select_path (GTK_ICON_VIEW (WID ("backgrounds-iconview")), path);
  gtk_tree_path_free (path);
}

static void
load_current_bg (CcBackgroundPanel *self)
{
  CcBackgroundPanelPrivate *priv;
  CcBackgroundItem *saved, *configured;
  gchar *uri, *pcolor, *scolor;

  priv = self->priv;

  /* Load the saved configuration */
  uri = get_save_path ();
  saved = cc_background_xml_get_item (uri);
  g_free (uri);

  /* initalise the current background information from settings */
  uri = g_settings_get_string (priv->settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    {
      g_free (uri);
      uri = NULL;
    }
  else
    {
      GFile *file;

      file = g_file_new_for_commandline_arg (uri);
      g_object_unref (file);
    }
  configured = cc_background_item_new (uri);
  g_free (uri);

  pcolor = g_settings_get_string (priv->settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (priv->settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
		"name", _("Current background"),
		"placement", g_settings_get_enum (priv->settings, WP_OPTIONS_KEY),
		"shading", g_settings_get_enum (priv->settings, WP_SHADING_KEY),
		"primary-color", pcolor,
		"secondary-color", scolor,
		NULL);
  g_free (pcolor);
  g_free (scolor);

  if (saved != NULL && cc_background_item_compare (saved, configured))
    {
      CcBackgroundItemFlags flags;
      flags = cc_background_item_get_flags (saved);
      /* Special case for colours */
      if (cc_background_item_get_placement (saved) == G_DESKTOP_BACKGROUND_STYLE_NONE)
        flags &=~ (CC_BACKGROUND_ITEM_HAS_PCOLOR | CC_BACKGROUND_ITEM_HAS_SCOLOR);
      g_object_set (G_OBJECT (configured),
		    "name", cc_background_item_get_name (saved),
		    "flags", flags,
		    "source-url", cc_background_item_get_source_url (saved),
		    "source-xml", cc_background_item_get_source_xml (saved),
		    NULL);
    }
  if (saved != NULL)
    g_object_unref (saved);

  priv->current_background = configured;
  cc_background_item_load (priv->current_background, NULL);
}

static void
scrolled_realize_cb (GtkWidget         *scrolled,
                     CcBackgroundPanel *self)
{
  /* FIXME, hack for https://bugzilla.gnome.org/show_bug.cgi?id=645649 */
  GdkScreen *screen;
  GdkRectangle rect;
  int monitor;

  screen = gtk_widget_get_screen (scrolled);
  monitor = gdk_screen_get_monitor_at_window (screen, gtk_widget_get_window (scrolled));
  gdk_screen_get_monitor_geometry (screen, monitor, &rect);
  if (rect.height <= 768)
    g_object_set (G_OBJECT (scrolled), "height-request", 280, NULL);
}

static void
cc_background_panel_drag_uris (GtkWidget *widget,
			       GdkDragContext *context, gint x, gint y,
			       GtkSelectionData *data, guint info, guint time,
			       CcBackgroundPanel *panel)
{
  gint i;
  char *uri;
  gchar **uris;

  uris = gtk_selection_data_get_uris (data);
  if (!uris)
    return;

  gtk_drag_finish (context, TRUE, FALSE, time);

  for (i = 0; uris[i] != NULL; i++) {
    uri = uris[i];
    if (!bg_pictures_source_is_known (panel->priv->pictures_source, uri)) {
      add_custom_wallpaper (panel, uri);
    }
  }

  g_strfreev(uris);
}

static void
cc_background_panel_init (CcBackgroundPanel *self)
{
  CcBackgroundPanelPrivate *priv;
  gchar *objects[] = { "style-liststore",
      "sources-liststore", "background-panel", "sizegroup", NULL };
  GError *err = NULL;
  GtkWidget *widget;
  GtkListStore *store;
  GtkStyleContext *context;

  priv = self->priv = BACKGROUND_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  gtk_builder_add_objects_from_file (priv->builder,
                                     DATADIR"/background.ui",
                                     objects, &err);

  if (err)
    {
      g_warning ("Could not load ui: %s", err->message);
      g_error_free (err);
      return;
    }

  /* See shell_notify_cb for details */
  g_signal_connect (WID ("scrolledwindow1"), "realize",
                    G_CALLBACK (scrolled_realize_cb), self);

  priv->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (priv->settings);

  store = (GtkListStore*) gtk_builder_get_object (priv->builder,
                                                  "sources-liststore");

  priv->wallpapers_source = bg_wallpapers_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Wallpapers"),
                                     COL_SOURCE_TYPE, SOURCE_WALLPAPERS,
                                     COL_SOURCE, priv->wallpapers_source,
                                     -1);

  priv->pictures_source = bg_pictures_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Pictures Folder"),
                                     COL_SOURCE_TYPE, SOURCE_PICTURES,
                                     COL_SOURCE, priv->pictures_source,
                                     -1);

  priv->colors_source = bg_colors_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Colors & Gradients"),
                                     COL_SOURCE_TYPE, SOURCE_COLORS,
                                     COL_SOURCE, priv->colors_source,
                                     -1);

#ifdef HAVE_LIBSOCIALWEB
  priv->flickr_source = bg_flickr_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     COL_SOURCE_NAME, _("Flickr"),
                                     COL_SOURCE_TYPE, SOURCE_FLICKR,
                                     COL_SOURCE, priv->flickr_source,
                                     -1);
#endif


  /* add the top level widget */
  widget = WID ("background-panel");

  gtk_container_add (GTK_CONTAINER (self), widget);
  gtk_widget_show_all (GTK_WIDGET (self));

  /* connect to source change signal */
  widget = WID ("sources-combobox");
  g_signal_connect (widget, "changed", G_CALLBACK (source_changed_cb), priv);

  /* select first item */
  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

  /* connect to the background iconview change signal */
  widget = WID ("backgrounds-iconview");
  g_signal_connect (widget, "selection-changed",
                    G_CALLBACK (backgrounds_changed_cb),
                    self);

  /* Join treeview and buttons */
  widget = WID ("scrolledwindow1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
  widget = WID ("toolbar1");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  g_signal_connect (WID ("add_button"), "clicked",
		    G_CALLBACK (add_button_clicked), self);
  g_signal_connect (WID ("remove_button"), "clicked",
		    G_CALLBACK (remove_button_clicked), self);

  /* Add drag and drop support for bg images */
  widget = WID ("scrolledwindow1");
  gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_dest_add_uri_targets (widget);
  g_signal_connect (widget, "drag-data-received",
		    G_CALLBACK (cc_background_panel_drag_uris), self);


  /* setup preview area */
  gtk_label_set_ellipsize (GTK_LABEL (WID ("background-label")), PANGO_ELLIPSIZE_END);
  widget = WID ("preview-area");
  g_signal_connect (widget, "draw", G_CALLBACK (preview_draw_cb),
                    self);

  priv->display_base = gdk_pixbuf_new_from_file (DATADIR "/display-base.png",
                                                 NULL);
  priv->display_overlay = gdk_pixbuf_new_from_file (DATADIR
                                                    "/display-overlay.png",
                                                    NULL);

  g_signal_connect (WID ("style-combobox"), "changed",
                    G_CALLBACK (style_changed_cb), self);

  g_signal_connect (WID ("style-pcolor"), "color-set",
                    G_CALLBACK (color_changed_cb), self);
  g_signal_connect (WID ("style-scolor"), "color-set",
                    G_CALLBACK (color_changed_cb), self);
  g_signal_connect (WID ("swap-color-button"), "clicked",
                    G_CALLBACK (swap_colors_clicked), self);

  priv->copy_cancellable = g_cancellable_new ();

  priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  load_current_bg (self);

  update_preview (priv, NULL);

  /* Setup the edit box with our current settings */
  source_update_edit_box (priv, TRUE);
}

void
cc_background_panel_register (GIOModule *module)
{
  cc_background_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_BACKGROUND_PANEL,
                                  "background", 0);
}

