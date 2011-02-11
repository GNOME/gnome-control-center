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
#include <gsettings-desktop-schemas/gdesktop-enums.h>

#include "cc-background-panel.h"
#include "bg-wallpapers-source.h"
#include "bg-pictures-source.h"
#include "bg-colors-source.h"

#ifdef HAVE_LIBSOCIALWEB
#include "bg-flickr-source.h"
#endif

#include "gnome-wp-xml.h"
#include "cc-background-item.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_FILE_KEY "picture-filename"
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

  cc_background_item_dump (priv->current_background);

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

  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT ||
      cc_background_item_get_filename (priv->current_background) == NULL)
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
copy_finished_cb (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      pointer)
{
  GError *err = NULL;
  CcBackgroundPanel *panel = (CcBackgroundPanel *) pointer;
  CcBackgroundPanelPrivate *priv = panel->priv;

  if (!g_file_copy_finish (G_FILE (source_object), result, &err))
    {
      if (err->code != G_IO_ERROR_CANCELLED)
        g_warning ("Failed to copy image to cache location: %s", err->message);

      g_error_free (err);
    }

  /* the panel may have been destroyed before the callback is run, so be sure
   * to check the widgets are not NULL */

  if (priv->spinner)
    {
      gtk_widget_destroy (GTK_WIDGET (priv->spinner));
      priv->spinner = NULL;
    }

  if (priv->builder)
    gtk_widget_queue_draw (WID ("preview-area"));

  /* remove the reference taken when the copy was set up */
  g_object_unref (panel);
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
                CcBackgroundItem         *item,
                gboolean                  redraw_preview)
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

  if (redraw_preview)
    gtk_widget_queue_draw (WID ("preview-area"));
}

static void
backgrounds_changed_cb (GtkIconView       *icon_view,
                        CcBackgroundPanel *panel)
{
  GtkTreeIter iter;
  GList *list;
  GtkTreeModel *model;
  CcBackgroundItem *item;
  CcBackgroundPanelPrivate *priv = panel->priv;
  char *pcolor, *scolor;
  gboolean draw_preview = TRUE;
  const char *filename;
  CcBackgroundItemFlags flags;

  list = gtk_icon_view_get_selected_items (icon_view);

  if (!list)
    return;

  /* check if the current source is read only, i.e. the image placement and
   * color is predefined */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sources-combobox")));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("sources-combobox")),
                                 &iter);
  gtk_tree_model_get (model, &iter,
		      COL_SOURCE_TYPE, &priv->current_source, -1);

  model = gtk_icon_view_get_model (icon_view);

  gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data);

  g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (list);

  gtk_tree_model_get (model, &iter, 1, &item, -1);

  filename = cc_background_item_get_filename (item);
  flags = cc_background_item_get_flags (item);

  if ((flags & CC_BACKGROUND_ITEM_HAS_FNAME) && filename == NULL)
    {
      g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, G_DESKTOP_BACKGROUND_STYLE_NONE);
      g_settings_set_string (priv->settings, WP_FILE_KEY, "");
    }
  else if (cc_background_item_get_source_url (item) != NULL)
    {
      GFile *source, *dest;
      gchar *cache_path;
      GdkPixbuf *pixbuf;

      cache_path = g_build_filename (g_get_user_cache_dir (),
                                     "gnome-background",
                                     NULL);

      source = g_file_new_for_uri (cc_background_item_get_source_url (item));
      dest = g_file_new_for_path (cache_path);

      /* create a blank image to use until the source image is ready */
      pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
      gdk_pixbuf_fill (pixbuf, 0x00000000);
      gdk_pixbuf_save (pixbuf, cache_path, "png", NULL, NULL);
      g_object_unref (pixbuf);

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
      g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, priv->copy_cancellable,
                         NULL, NULL,
                         copy_finished_cb, panel);

      g_settings_set_string (priv->settings, WP_FILE_KEY, cache_path);
      g_object_set (G_OBJECT (item), "filename", cache_path, NULL);

      /* delay the updated drawing of the preview until the copy finishes */
      draw_preview = FALSE;
    }
  else
    {
       gchar *uri;

       //FIXME this is garbage, either use uri, or not
       if (g_utf8_validate (filename, -1, NULL))
         uri = g_strdup (filename);
       else
         uri = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

       if (uri == NULL)
         {
           g_warning ("Failed to convert filename to UTF-8: %s", filename);
         }
       else
         {
	   g_settings_set_string (priv->settings, WP_FILE_KEY, uri);
           g_free (uri);
         }
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    g_settings_set_enum (priv->settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));

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

  /* update the preview information */
  update_preview (priv, item, draw_preview);
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
                                                -2);
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

  update_preview (priv, NULL, TRUE);
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

  update_preview (priv, NULL, TRUE);
}

static void
cc_background_panel_init (CcBackgroundPanel *self)
{
  CcBackgroundPanelPrivate *priv;
  gchar *objects[] = { "backgrounds-liststore", "style-liststore",
      "sources-liststore", "background-panel", "sizegroup", NULL };
  GError *err = NULL;
  GtkWidget *widget;
  GtkListStore *store;
  gchar *filename;

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
  widget = (GtkWidget*)
    gtk_builder_get_object (priv->builder, "background-panel");

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

  /* setup preview area */
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

  priv->copy_cancellable = g_cancellable_new ();

  priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  /* initalise the current background information from settings */
  filename = g_settings_get_string (priv->settings, WP_FILE_KEY);
  priv->current_background = cc_background_item_new (filename);

  /* FIXME Set flags too:
   * - if we have a gradient and no filename, set PCOLOR, etc.
   *
   * Move into cc-background-item.c like the old cc_background_item_update()?
   */
  g_object_set (G_OBJECT (priv->current_background),
		"name", _("Current background"),
		"placement", g_settings_get_enum (priv->settings, WP_OPTIONS_KEY),
		"shading", g_settings_get_enum (priv->settings, WP_SHADING_KEY),
		"primary-color", g_settings_get_string (priv->settings, WP_PCOLOR_KEY),
		"secondary-color", g_settings_get_string (priv->settings, WP_SCOLOR_KEY),
		NULL);

  cc_background_item_load (priv->current_background, NULL);

  update_preview (priv, NULL, TRUE);

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

