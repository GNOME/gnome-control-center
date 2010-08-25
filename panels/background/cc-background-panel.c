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

#include "cc-background-panel.h"
#include "bg-wallpapers-source.h"
#include "bg-pictures-source.h"
#include "bg-colors-source.h"

#ifdef HAVE_LIBSOCIALWEB
#include "bg-flickr-source.h"
#endif

#include "gnome-wp-xml.h"
#include "gnome-wp-item.h"

#include <string.h>
#include <glib/gi18n-lib.h>

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

  GConfClient *client;

  GnomeDesktopThumbnailFactory *thumb_factory;

  GnomeWPItem *current_background;
  gboolean current_source_readonly;

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

  if (priv->client)
    {
      g_object_unref (priv->client);
      priv->client = NULL;
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
      gnome_wp_item_free (priv->current_background);
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
                      1, &type,
                      3, &source, -1);

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
    gtk_widget_show (WID ("preview-area"));

  /* remove the reference taken when the copy was set up */
  g_object_unref (panel);
}

static void
update_preview (CcBackgroundPanelPrivate *priv,
                GnomeWPItem              *item)
{
  gchar *markup;

  if (item && priv->current_background)
    {
      if (priv->current_background->pcolor)
        gdk_color_free (priv->current_background->pcolor);
      priv->current_background->pcolor = gdk_color_copy (item->pcolor);

      if (priv->current_background->scolor)
        gdk_color_free (priv->current_background->scolor);
      priv->current_background->scolor = gdk_color_copy (item->scolor);



      g_free (priv->current_background->filename);
      priv->current_background->filename = g_strdup (item->filename);

      g_free (priv->current_background->name);
      priv->current_background->name = g_strdup (item->name);

      priv->current_background->options = item->options;
    }


  if (!priv->current_source_readonly)
    gtk_widget_show (WID ("edit-hbox"));
  else
    gtk_widget_hide (WID ("edit-hbox"));

  if (priv->current_background)
    {
      markup = g_strdup_printf ("<b>%s</b>", priv->current_background->name);
      gtk_label_set_markup (GTK_LABEL (WID ("background-label")), markup);
      g_free (markup);

      gtk_color_button_set_color (GTK_COLOR_BUTTON (WID ("style-color")),
                                  priv->current_background->pcolor);

      gtk_combo_box_set_active (GTK_COMBO_BOX (WID ("style-combobox")),
                                priv->current_background->options);
    }

  gtk_widget_queue_draw (WID ("preview-area"));
}

static void
backgrounds_changed_cb (GtkIconView       *icon_view,
                        CcBackgroundPanel *panel)
{
  GtkTreeIter iter;
  GList *list;
  GtkTreeModel *model;
  GnomeWPItem *item;
  GConfChangeSet *cs;
  gchar *pcolor, *scolor;
  CcBackgroundPanelPrivate *priv = panel->priv;

  list = gtk_icon_view_get_selected_items (icon_view);

  if (!list)
    return;

  /* check if the current source is read only, i.e. the image placement and
   * color is predefined */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sources-combobox")));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("sources-combobox")),
                                 &iter);
  gtk_tree_model_get (model, &iter, 2, &priv->current_source_readonly, -1);


  model = gtk_icon_view_get_model (icon_view);

  gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data);

  g_list_foreach (list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free (list);

  gtk_tree_model_get (model, &iter, 1, &item, -1);

  cs = gconf_change_set_new ();

  if (!g_strcmp0 (item->filename, "(none)"))
    {
      gconf_change_set_set_string (cs, WP_OPTIONS_KEY, "none");
      gconf_change_set_set_string (cs, WP_FILE_KEY, "");
    }
  else if (item->source_url)
    {
      GFile *source, *dest;
      gchar *cache_path;
      GdkPixbuf *pixbuf;

      cache_path = g_build_filename (g_get_user_cache_dir (),
                                     "gnome-background",
                                     NULL);

      source = g_file_new_for_uri (item->source_url);
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
      gtk_box_pack_start (GTK_BOX (WID ("details-box")), priv->spinner, FALSE,
                          FALSE, 0);
      gtk_box_reorder_child (GTK_BOX (WID ("details-box")), priv->spinner, 0);
      gtk_widget_set_size_request (priv->spinner, 150, 75);
      gtk_widget_show (priv->spinner);
      gtk_widget_hide (WID ("preview-area"));


      /* reference the panel in case it is removed before the copy is
       * finished */
      g_object_ref (panel);
      g_file_copy_async (source, dest, G_FILE_COPY_OVERWRITE,
                         G_PRIORITY_DEFAULT, priv->copy_cancellable,
                         NULL, NULL,
                         copy_finished_cb, panel);

      gconf_change_set_set_string (cs, WP_FILE_KEY,
                                   cache_path);
      gconf_change_set_set_string (cs, WP_OPTIONS_KEY,
                                   wp_item_option_to_string (item->options));
      g_free (item->filename);
      item->filename = cache_path;
    }
  else
    {
       gchar *uri;

       if (g_utf8_validate (item->filename, -1, NULL))
         uri = g_strdup (item->filename);
       else
         uri = g_filename_to_utf8 (item->filename, -1, NULL, NULL, NULL);

       if (uri == NULL)
         {
           g_warning ("Failed to convert filename to UTF-8: %s",
                      item->filename);
         }
       else
         {
           gconf_change_set_set_string (cs, WP_FILE_KEY, uri);
           g_free (uri);
         }

       gconf_change_set_set_string (cs, WP_OPTIONS_KEY,
                                    wp_item_option_to_string (item->options));
    }

  gconf_change_set_set_string (cs, WP_SHADING_KEY,
                               wp_item_shading_to_string (item->shade_type));

  pcolor = gdk_color_to_string (item->pcolor);
  scolor = gdk_color_to_string (item->scolor);
  gconf_change_set_set_string (cs, WP_PCOLOR_KEY, pcolor);
  gconf_change_set_set_string (cs, WP_SCOLOR_KEY, scolor);
  g_free (pcolor);
  g_free (scolor);

  gconf_client_commit_change_set (priv->client, cs, TRUE, NULL);

  gconf_change_set_unref (cs);

  /* update the preview information */
  update_preview (priv, item);
}

static gboolean
preview_expose_cb (GtkWidget         *widget,
                   GdkEventExpose    *expose,
                   CcBackgroundPanel *panel)
{
  cairo_t *cr;
  GtkAllocation allocation;
  CcBackgroundPanelPrivate *priv = panel->priv;
  GdkPixbuf *pixbuf = NULL;
  const gint preview_width = 416;
  const gint preview_height = 248;
  const gint preview_x = 45;
  const gint preview_y = 84;
  GdkPixbuf *preview, *temp;
  gfloat scale;
  gint size;

  cr = gdk_cairo_create (gtk_widget_get_window (widget));

  gtk_widget_get_allocation (widget, &allocation);

  if (priv->current_background)
    {
      pixbuf = gnome_wp_item_get_thumbnail (priv->current_background,
                                            priv->thumb_factory,
                                            preview_width,
                                            preview_height);
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
  cairo_destroy (cr);

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
  gchar *value;

  gtk_combo_box_get_active_iter (box, &iter);

  model = gtk_combo_box_get_model (box);

  gtk_tree_model_get (model, &iter, 1, &value, -1);

  gconf_client_set_string (priv->client,
                           "/desktop/gnome/background/picture_options",
                           value, NULL);

  g_free (value);

  if (priv->current_background)
    priv->current_background->options = gtk_combo_box_get_active (box);

  update_preview (priv, NULL);
}

static void
color_changed_cb (GtkColorButton    *button,
                  CcBackgroundPanel *panel)
{
  CcBackgroundPanelPrivate *priv = panel->priv;
  GdkColor color;
  gchar *value;

  gtk_color_button_get_color (button, &color);

  if (priv->current_background)
    *priv->current_background->pcolor = color;

  value = gdk_color_to_string (&color);

  gconf_client_set_string (priv->client,
                           "/desktop/gnome/background/primary_color",
                           value, NULL);

  gconf_client_set_string (priv->client,
                           "/desktop/gnome/background/secondary_color",
                           value, NULL);

  g_free (value);

  update_preview (priv, NULL);
}

static void
cc_background_panel_init (CcBackgroundPanel *self)
{
  CcBackgroundPanelPrivate *priv;
  gchar *objects[] = { "backgrounds-liststore", "style-liststore",
      "sources-liststore", "background-panel", "sizegroup", NULL };
  GError *err = NULL;
  GtkWidget *widget;
  gint width, height;
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

  priv->client = gconf_client_get_default ();

  store = (GtkListStore*) gtk_builder_get_object (priv->builder,
                                                  "sources-liststore");

  priv->wallpapers_source = bg_wallpapers_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     0, _("Wallpapers"),
                                     1, SOURCE_WALLPAPERS,
                                     2, TRUE,
                                     3, priv->wallpapers_source,
                                     -1);

  priv->pictures_source = bg_pictures_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     0, _("Pictures Folder"),
                                     1, SOURCE_PICTURES,
                                     2, FALSE,
                                     3, priv->pictures_source,
                                     -1);

  priv->colors_source = bg_colors_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     0, _("Colors"),
                                     1, SOURCE_COLORS,
                                     2, TRUE,
                                     3, priv->colors_source,
                                     -1);

#ifdef HAVE_LIBSOCIALWEB
  priv->flickr_source = bg_flickr_source_new ();
  gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
                                     0, _("Flickr"),
                                     1, SOURCE_FLICKR,
                                     2, FALSE,
                                     3, priv->flickr_source,
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

  /* connect to the background iconview change signal */
  widget = WID ("backgrounds-iconview");
  g_signal_connect (widget, "selection-changed",
                    G_CALLBACK (backgrounds_changed_cb),
                    self);

  /* setup preview area */
  widget = WID ("preview-area");
  g_signal_connect (widget, "expose-event", G_CALLBACK (preview_expose_cb),
                    self);

  priv->display_base = gdk_pixbuf_new_from_file (DATADIR "/display-base.png",
                                                 NULL);
  priv->display_overlay = gdk_pixbuf_new_from_file (DATADIR
                                                    "/display-overlay.png",
                                                    NULL);

  g_signal_connect (WID ("style-combobox"), "changed",
                    G_CALLBACK (style_changed_cb), self);

  g_signal_connect (WID ("style-color"), "color-set",
                    G_CALLBACK (color_changed_cb), self);

  priv->copy_cancellable = g_cancellable_new ();

  priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

  /* initalise the current background information from gconf */
  filename = gconf_client_get_string (priv->client, WP_FILE_KEY, NULL);
  if (!filename || !g_strcmp0 (filename, ""))
    {
      g_free (filename);
      filename = g_strdup ("(none)");
    }

  priv->current_background = g_new0 (GnomeWPItem, 1);
  priv->current_background->filename = filename;
  priv->current_background->name = g_strdup ("");

  gnome_wp_item_update (priv->current_background);
  gnome_wp_item_ensure_gnome_bg (priv->current_background);

  update_preview (priv, NULL);
}

void
cc_background_panel_register (GIOModule *module)
{
  cc_background_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_BACKGROUND_PANEL,
                                  "background", 0);
}

