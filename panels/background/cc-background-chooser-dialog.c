/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "cc-background-chooser-dialog.h"
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

enum
{
  SOURCE_WALLPAPERS,
  SOURCE_PICTURES,
  SOURCE_COLORS,
#ifdef HAVE_LIBSOCIALWEB
  SOURCE_FLICKR
#endif
};

struct _CcBackgroundChooserDialogPrivate
{
  GtkListStore *sources;
  GtkWidget *icon_view;

  BgWallpapersSource *wallpapers_source;
  BgPicturesSource *pictures_source;
  BgColorsSource *colors_source;
#ifdef HAVE_LIBSOCIALWEB
  BgFlickrSource *flickr_source;
#endif

  GnomeDesktopThumbnailFactory *thumb_factory;
  gint current_source;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;
};

#define CC_CHOOSER_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_BACKGROUND_CHOOSER_DIALOG, CcBackgroundChooserDialogPrivate))

enum
{
  PROP_0,
};

G_DEFINE_TYPE (CcBackgroundChooserDialog, cc_background_chooser_dialog, GTK_TYPE_DIALOG)

static void
cc_background_chooser_dialog_realize (GtkWidget *widget)
{
  CcBackgroundChooserDialog *chooser = CC_BACKGROUND_CHOOSER_DIALOG (widget);
  GtkWindow *parent;

  parent = gtk_window_get_transient_for (GTK_WINDOW (chooser));
  if (parent != NULL)
    {
      gint width;
      gint height;

      gtk_window_get_size (parent, &width, &height);
      gtk_widget_set_size_request (GTK_WIDGET (chooser), (gint) (0.5 * width), (gint) (0.9 * height));
    }

  gtk_icon_view_set_model (GTK_ICON_VIEW (chooser->priv->icon_view),
                           GTK_TREE_MODEL (bg_source_get_liststore (BG_SOURCE (chooser->priv->wallpapers_source))));

  GTK_WIDGET_CLASS (cc_background_chooser_dialog_parent_class)->realize (widget);
}

static void
cc_background_chooser_dialog_dispose (GObject *object)
{
  CcBackgroundChooserDialog *chooser = CC_BACKGROUND_CHOOSER_DIALOG (object);
  CcBackgroundChooserDialogPrivate *priv = chooser->priv;

  if (priv->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (priv->copy_cancellable);

      g_clear_object (&priv->copy_cancellable);
    }

  g_clear_object (&priv->pictures_source);
  g_clear_object (&priv->colors_source);
  g_clear_object (&priv->wallpapers_source);
  g_clear_object (&priv->thumb_factory);

  G_OBJECT_CLASS (cc_background_chooser_dialog_parent_class)->dispose (object);
}

static void
cc_background_chooser_dialog_finalize (GObject *object)
{

  G_OBJECT_CLASS (cc_background_chooser_dialog_parent_class)->finalize (object);
}

static void
on_view_toggled (GtkToggleButton           *button,
                 CcBackgroundChooserDialog *chooser)
{
  BgSource *source;

  if (!gtk_toggle_button_get_active (button))
    return;

  source = g_object_get_data (G_OBJECT (button), "source");
  gtk_icon_view_set_model (GTK_ICON_VIEW (chooser->priv->icon_view),
                           GTK_TREE_MODEL (bg_source_get_liststore (source)));

}

static void
on_selection_changed (GtkIconView               *icon_view,
                      CcBackgroundChooserDialog *chooser)
{
  GList *list;

  list = gtk_icon_view_get_selected_items (icon_view);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
                                     GTK_RESPONSE_OK,
                                     (list != NULL));

  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);
}

static void
on_item_activated (GtkIconView               *icon_view,
                   GtkTreePath               *path,
                   CcBackgroundChooserDialog *chooser)
{
  gtk_dialog_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
}

static void
cc_background_chooser_dialog_init (CcBackgroundChooserDialog *chooser)
{
  CcBackgroundChooserDialogPrivate *priv;
  GtkCellRenderer *renderer;
  GtkWidget *sw_content;
  GtkWidget *vbox;
  GtkWidget *button1;
  GtkWidget *button;
  GtkWidget *hbox;
  GtkWidget *grid;
  GtkStyleContext *context;

  chooser->priv = CC_CHOOSER_DIALOG_GET_PRIVATE (chooser);
  priv = chooser->priv;

  priv->wallpapers_source = bg_wallpapers_source_new ();
  priv->pictures_source = bg_pictures_source_new ();
  priv->colors_source = bg_colors_source_new ();
#ifdef HAVE_LIBSOCIALWEB
  priv->flickr_source = bg_flickr_source_new ();
#endif

  gtk_container_set_border_width (GTK_CONTAINER (chooser), 6);
  gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (chooser), FALSE);
  /* translators: This is the title of the wallpaper chooser dialog. */
  gtk_window_set_title (GTK_WINDOW (chooser), _("Select Background"));

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (chooser));
  grid = gtk_grid_new ();
  gtk_container_set_border_width (GTK_CONTAINER (grid), 5);
  gtk_widget_set_margin_bottom (grid, 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (grid), GTK_ORIENTATION_VERTICAL);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 0);
  gtk_container_add (GTK_CONTAINER (vbox), grid);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_container_add (GTK_CONTAINER (grid), hbox);
  context = gtk_widget_get_style_context (hbox);
  gtk_style_context_add_class (context, "linked");

  button1 = gtk_radio_button_new_with_label (NULL, _("Wallpapers"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button1), TRUE);
  context = gtk_widget_get_style_context (button1);
  gtk_style_context_add_class (context, "raised");
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button1), FALSE);
  gtk_container_add (GTK_CONTAINER (hbox), button1);
  g_signal_connect (button1, "toggled", G_CALLBACK (on_view_toggled), chooser);
  g_object_set_data (G_OBJECT (button1), "source", priv->wallpapers_source);

  button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button1), _("Pictures"));
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "toggled", G_CALLBACK (on_view_toggled), chooser);
  g_object_set_data (G_OBJECT (button), "source", priv->pictures_source);

  button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button1), _("Colors"));
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "toggled", G_CALLBACK (on_view_toggled), chooser);
  g_object_set_data (G_OBJECT (button), "source", priv->colors_source);

#ifdef HAVE_LIBSOCIALWEB
  button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button1), _("Flickr"));
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "toggled", G_CALLBACK (on_view_toggled), chooser);
  g_object_set_data (G_OBJECT (button), "source", priv->flickr_source);
#endif

  sw_content = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw_content), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw_content), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (sw_content, TRUE);
  gtk_widget_set_vexpand (sw_content, TRUE);
  gtk_container_add (GTK_CONTAINER (grid), sw_content);
  g_object_set (sw_content,
                "width-request", 850,
                "height-request", 550,
                NULL);

  priv->icon_view = gtk_icon_view_new ();
  gtk_widget_set_hexpand (priv->icon_view, TRUE);
  gtk_container_add (GTK_CONTAINER (sw_content), priv->icon_view);
  g_signal_connect (priv->icon_view, "selection-changed", G_CALLBACK (on_selection_changed), chooser);
  g_signal_connect (priv->icon_view, "item-activated", G_CALLBACK (on_item_activated), chooser);

  renderer = gtk_cell_renderer_pixbuf_new ();
  /* set stock size to 32px so that emblems render at 16px. see:
   * https://bugzilla.gnome.org/show_bug.cgi?id=682123#c4
   */
  g_object_set (renderer,
                "stock-size", GTK_ICON_SIZE_DND,
                NULL);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->icon_view),
                                  renderer,
                                  "gicon", 0,
                                  NULL);

  gtk_dialog_add_button (GTK_DIALOG (chooser), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (chooser), _("Select"), GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_OK, FALSE);

  gtk_widget_show_all (vbox);
}

static void
cc_background_chooser_dialog_class_init (CcBackgroundChooserDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = cc_background_chooser_dialog_dispose;
  object_class->finalize = cc_background_chooser_dialog_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = cc_background_chooser_dialog_realize;

  g_type_class_add_private (object_class, sizeof (CcBackgroundChooserDialogPrivate));
}

GtkWidget *
cc_background_chooser_dialog_new (void)
{
  return g_object_new (CC_TYPE_BACKGROUND_CHOOSER_DIALOG, NULL);
}

CcBackgroundItem *
cc_background_chooser_dialog_get_item (CcBackgroundChooserDialog *chooser)
{
  CcBackgroundChooserDialogPrivate *priv = chooser->priv;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GList *list;
  CcBackgroundItem *item;

  item = NULL;
  list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (priv->icon_view));

  if (!list)
    return NULL;

  model = gtk_icon_view_get_model (GTK_ICON_VIEW (priv->icon_view));

  if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data) == FALSE)
    goto bail;

  gtk_tree_model_get (model, &iter, 1, &item, -1);

bail:
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

  return item;
}
