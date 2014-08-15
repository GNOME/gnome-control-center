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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "cc-background-chooser-dialog.h"
#include "bg-wallpapers-source.h"
#include "bg-pictures-source.h"
#include "bg-colors-source.h"

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
};

struct _CcBackgroundChooserDialogPrivate
{
  GtkListStore *sources;
  GtkWidget *icon_view;
  GtkWidget *empty_pictures_box;
  GtkWidget *sw_content;
  GtkWidget *pictures_button;
  GtkWidget *colors_button;

  BgWallpapersSource *wallpapers_source;
  BgPicturesSource *pictures_source;
  BgColorsSource *colors_source;

  GtkTreeRowReference *item_to_focus;

  GnomeDesktopThumbnailFactory *thumb_factory;

  GCancellable *copy_cancellable;

  GtkWidget *spinner;

  gulong row_inserted_id;
  gulong row_deleted_id;
  gulong row_modified_id;
};

#define CC_CHOOSER_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_BACKGROUND_CHOOSER_DIALOG, CcBackgroundChooserDialogPrivate))

enum
{
  PROP_0,
};

enum
{
  URI_LIST,
  COLOR
};

static const GtkTargetEntry color_targets[] =
{
  { "application/x-color", 0, COLOR }
};

G_DEFINE_TYPE (CcBackgroundChooserDialog, cc_background_chooser_dialog, GTK_TYPE_DIALOG)

static void
cc_background_chooser_dialog_realize (GtkWidget *widget)
{
  CcBackgroundChooserDialog *chooser = CC_BACKGROUND_CHOOSER_DIALOG (widget);
  GtkWindow *parent;

  parent = gtk_window_get_transient_for (GTK_WINDOW (chooser));

  if (parent == NULL)
    {
      gtk_widget_set_size_request (GTK_WIDGET (chooser), -1, 550);
      gtk_icon_view_set_columns (GTK_ICON_VIEW (chooser->priv->icon_view), 3);
    }
  else if (gtk_window_is_maximized (parent))
    {
      gtk_window_maximize (GTK_WINDOW (chooser));
    }
  else
    {
      gint width;
      gint height;

      gtk_window_get_size (parent, &width, &height);
      gtk_widget_set_size_request (GTK_WIDGET (chooser), (gint) (0.5 * width), (gint) (0.9 * height));
      gtk_icon_view_set_columns (GTK_ICON_VIEW (chooser->priv->icon_view), 3);
    }

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

  g_clear_pointer (&chooser->priv->item_to_focus, gtk_tree_row_reference_free);
  g_clear_object (&priv->pictures_source);
  g_clear_object (&priv->colors_source);
  g_clear_object (&priv->wallpapers_source);
  g_clear_object (&priv->thumb_factory);

  G_OBJECT_CLASS (cc_background_chooser_dialog_parent_class)->dispose (object);
}

static void
ensure_iconview_shown (CcBackgroundChooserDialog *chooser)
{
  gtk_widget_hide (chooser->priv->empty_pictures_box);
  gtk_widget_show (chooser->priv->sw_content);
}

static void
possibly_show_empty_pictures_box (GtkTreeModel              *model,
                                  CcBackgroundChooserDialog *chooser)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      ensure_iconview_shown (chooser);
    }
  else
    {
      gtk_widget_hide (chooser->priv->sw_content);
      gtk_widget_show (chooser->priv->empty_pictures_box);
    }
}

static void
on_source_modified_cb (GtkTreeModel *tree_model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      user_data)
{
  GtkTreePath *to_focus_path;
  CcBackgroundChooserDialog *chooser = user_data;
  CcBackgroundChooserDialogPrivate *priv = chooser->priv;

  if (chooser->priv->item_to_focus == NULL)
    return;

  to_focus_path = gtk_tree_row_reference_get_path (chooser->priv->item_to_focus);

  if (gtk_tree_path_compare (to_focus_path, path) != 0)
    goto out;

  /* Change source */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->pictures_button), TRUE);

  /* And select the newly added item */
  gtk_icon_view_select_path (GTK_ICON_VIEW (chooser->priv->icon_view), to_focus_path);
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (chooser->priv->icon_view),
                                to_focus_path, TRUE, 1.0, 1.0);
  g_clear_pointer (&chooser->priv->item_to_focus, gtk_tree_row_reference_free);

out:
  gtk_tree_path_free (to_focus_path);

}

static void
on_source_added_cb (GtkTreeModel *model,
                    GtkTreePath  *path,
                    GtkTreeIter  *iter,
                    gpointer     user_data)
{
  possibly_show_empty_pictures_box (model, CC_BACKGROUND_CHOOSER_DIALOG (user_data));
}

static void
on_source_removed_cb (GtkTreeModel *model,
                      GtkTreePath  *path,
                      gpointer     user_data)
{
  possibly_show_empty_pictures_box (model, CC_BACKGROUND_CHOOSER_DIALOG (user_data));
}

static void
monitor_pictures_model (CcBackgroundChooserDialog *chooser)
{
  GtkTreeModel *model;

  if (chooser->priv->row_inserted_id != 0)
    return;

  model = GTK_TREE_MODEL (bg_source_get_liststore (BG_SOURCE (chooser->priv->pictures_source)));

  chooser->priv->row_inserted_id = g_signal_connect (model, "row-inserted",
                                                     G_CALLBACK (on_source_added_cb),
                                                     chooser);

  chooser->priv->row_deleted_id = g_signal_connect (model, "row-deleted",
                                                    G_CALLBACK (on_source_removed_cb),
                                                    chooser);

  chooser->priv->row_modified_id = g_signal_connect (model, "row-changed",
                                                     G_CALLBACK (on_source_modified_cb),
                                                     chooser);

  possibly_show_empty_pictures_box (model, chooser);
}

static void
cancel_monitor_pictures_model (CcBackgroundChooserDialog *chooser)
{
  GtkTreeModel *model;

  model = GTK_TREE_MODEL (bg_source_get_liststore (BG_SOURCE (chooser->priv->pictures_source)));

  if (chooser->priv->row_inserted_id > 0)
    {
      g_signal_handler_disconnect (model, chooser->priv->row_inserted_id);
      chooser->priv->row_inserted_id = 0;
    }

  if (chooser->priv->row_deleted_id > 0)
    {
      g_signal_handler_disconnect (model, chooser->priv->row_deleted_id);
      chooser->priv->row_deleted_id = 0;
    }

  if (chooser->priv->row_modified_id > 0)
    {
      g_signal_handler_disconnect (model, chooser->priv->row_modified_id);
      chooser->priv->row_modified_id = 0;
    }

  ensure_iconview_shown (chooser);
}

static void
on_view_toggled (GtkToggleButton           *button,
                 CcBackgroundChooserDialog *chooser)
{
  BgSource *source;
  GtkTreeModel *model;

  if (!gtk_toggle_button_get_active (button))
    return;

  source = g_object_get_data (G_OBJECT (button), "source");
  model = GTK_TREE_MODEL (bg_source_get_liststore (source));
  gtk_icon_view_set_model (GTK_ICON_VIEW (chooser->priv->icon_view), model);
  /* When there are not any appropriate image files as direct children of
   * ~/Pictures show the empty_pictures_box to inform the user what's wrong
   * and how to add images to show here.
   */
  if (source == BG_SOURCE (chooser->priv->pictures_source))
    monitor_pictures_model (chooser);
  else
    cancel_monitor_pictures_model (chooser);
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
add_custom_wallpaper (CcBackgroundChooserDialog *chooser,
                      const char                *uri)
{
  g_clear_pointer (&chooser->priv->item_to_focus, gtk_tree_row_reference_free);

  monitor_pictures_model (chooser);
  bg_pictures_source_add (chooser->priv->pictures_source, uri, &chooser->priv->item_to_focus);
  /* and wait for the item to get added */
}

static gboolean
cc_background_panel_drag_color (CcBackgroundChooserDialog *chooser,
                                GtkSelectionData          *data)
{
  gint length;
  guint16 *dropped;
  GdkRGBA rgba;
  GtkTreeRowReference *row_ref;
  GtkTreePath *to_focus_path;

  length = gtk_selection_data_get_length (data);

  if (length < 0)
    return FALSE;

  if (length != 8)
    {
      g_warning ("%s: Received invalid color data", G_STRFUNC);
      return FALSE;
    }

  dropped = (guint16 *) gtk_selection_data_get_data (data);
  rgba.red   = dropped[0] / 65535.;
  rgba.green = dropped[1] / 65535.;
  rgba.blue  = dropped[2] / 65535.;
  rgba.alpha = dropped[3] / 65535.;

  if (bg_colors_source_add (chooser->priv->colors_source, &rgba, &row_ref) == FALSE)
    return FALSE;

  /* Change source */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (chooser->priv->colors_button), TRUE);

  /* And select the newly added item */
  to_focus_path = gtk_tree_row_reference_get_path (row_ref);
  gtk_icon_view_select_path (GTK_ICON_VIEW (chooser->priv->icon_view), to_focus_path);
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (chooser->priv->icon_view),
                                to_focus_path, TRUE, 1.0, 1.0);
  gtk_tree_row_reference_free (row_ref);
  gtk_tree_path_free (to_focus_path);

  return TRUE;
}

static void
cc_background_panel_drag_items (GtkWidget *widget,
                                GdkDragContext *context, gint x, gint y,
                                GtkSelectionData *data, guint info, guint time,
                                CcBackgroundChooserDialog *chooser)
{
  gint i;
  char *uri;
  gchar **uris;
  gboolean ret = FALSE;

  if (info == COLOR)
    {
      ret = cc_background_panel_drag_color (chooser, data);
      goto out;
    }

  uris = gtk_selection_data_get_uris (data);
  if (!uris)
    goto out;

  for (i = 0; uris[i] != NULL; i++)
    {
      uri = uris[i];
      if (!bg_pictures_source_is_known (chooser->priv->pictures_source, uri))
        {
          add_custom_wallpaper (chooser, uri);
          ret = TRUE;
        }
    }

  g_strfreev (uris);

out:
  gtk_drag_finish (context, ret, FALSE, time);
}

static void
cc_background_chooser_dialog_init (CcBackgroundChooserDialog *chooser)
{
  CcBackgroundChooserDialogPrivate *priv;
  GtkCellRenderer *renderer;
  GtkWidget *vbox;
  GtkWidget *button1;
  GtkWidget *button;
  GtkWidget *headerbar;
  GtkWidget *hbox;
  GtkWidget *grid;
  GtkWidget *img;
  GtkWidget *labels_grid;
  GtkWidget *label;
  GtkStyleContext *context;
  gchar *markup, *href;
  const gchar *pictures_dir;
  gchar *pictures_dir_basename;
  gchar *pictures_dir_uri;
  GtkTargetList *target_list;
  GtkSizeGroup *size_group;

  chooser->priv = CC_CHOOSER_DIALOG_GET_PRIVATE (chooser);
  priv = chooser->priv;

  priv->wallpapers_source = bg_wallpapers_source_new (GTK_WINDOW (chooser));
  priv->pictures_source = bg_pictures_source_new (GTK_WINDOW (chooser));
  priv->colors_source = bg_colors_source_new (GTK_WINDOW (chooser));

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

  headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (chooser));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_set_homogeneous (GTK_BOX (hbox), TRUE);
  gtk_widget_set_halign (hbox, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (hbox, TRUE);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (headerbar), hbox);
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
  priv->pictures_button = button;

  button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (button1), _("Colors"));
  context = gtk_widget_get_style_context (button);
  gtk_style_context_add_class (context, "raised");
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
  gtk_container_add (GTK_CONTAINER (hbox), button);
  g_signal_connect (button, "toggled", G_CALLBACK (on_view_toggled), chooser);
  g_object_set_data (G_OBJECT (button), "source", priv->colors_source);
  priv->colors_button = button;

  gtk_widget_show_all (hbox);

  priv->sw_content = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->sw_content), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->sw_content), GTK_SHADOW_IN);
  gtk_widget_set_hexpand (priv->sw_content, TRUE);
  gtk_widget_set_vexpand (priv->sw_content, TRUE);
  gtk_container_add (GTK_CONTAINER (grid), priv->sw_content);

  /* Add drag and drop support for bg images */
  gtk_drag_dest_set (priv->sw_content, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, URI_LIST);
  gtk_target_list_add_table (target_list, color_targets, 1);
  gtk_drag_dest_set_target_list (priv->sw_content, target_list);
  gtk_target_list_unref (target_list);
  g_signal_connect (priv->sw_content, "drag-data-received",
                    G_CALLBACK (cc_background_panel_drag_items), chooser);

  priv->empty_pictures_box = gtk_grid_new ();
  gtk_widget_set_no_show_all (priv->empty_pictures_box, TRUE);
  gtk_grid_set_column_spacing (GTK_GRID (priv->empty_pictures_box), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (priv->empty_pictures_box),
                                  GTK_ORIENTATION_HORIZONTAL);
  context = gtk_widget_get_style_context (priv->empty_pictures_box);
  gtk_style_context_add_class (context, "dim-label");
  gtk_container_add (GTK_CONTAINER (grid), priv->empty_pictures_box);
  img = gtk_image_new_from_icon_name ("emblem-photos-symbolic", GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (img), 64);
  gtk_widget_set_halign (img, GTK_ALIGN_END);
  gtk_widget_set_valign (img, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (img, TRUE);
  gtk_widget_set_vexpand (img, TRUE);
  gtk_widget_show (img);
  gtk_container_add (GTK_CONTAINER (priv->empty_pictures_box), img);
  labels_grid = gtk_grid_new ();
  gtk_widget_set_halign (labels_grid, GTK_ALIGN_START);
  gtk_widget_set_valign (labels_grid, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (labels_grid, TRUE);
  gtk_widget_set_vexpand (labels_grid, TRUE);
  gtk_grid_set_row_spacing (GTK_GRID (labels_grid), 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (labels_grid),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (labels_grid);
  gtk_container_add (GTK_CONTAINER (priv->empty_pictures_box), labels_grid);
  label = gtk_label_new ("");
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  markup = g_markup_printf_escaped ("<b><span size='large'>%s</span></b>",
                                    /* translators: No pictures were found */
                                    _("No Pictures Found"));
  gtk_label_set_markup (GTK_LABEL (label), (const gchar *) markup);
  g_free (markup);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (labels_grid), label);
  label = gtk_label_new ("");
  gtk_label_set_max_width_chars (GTK_LABEL (label), 24);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  pictures_dir = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
  if (pictures_dir == NULL)
    {
      pictures_dir = g_get_home_dir ();
      /* translators: "Home" is used in place of the Pictures
       * directory in the string below when XDG_PICTURES_DIR is
       * undefined */
      pictures_dir_basename = g_strdup (_("Home"));
    }
  else
    pictures_dir_basename = g_path_get_basename (pictures_dir);

  pictures_dir_uri = g_filename_to_uri (pictures_dir, NULL, NULL);
  href = g_markup_printf_escaped ("<a href=\"%s\">%s</a>", pictures_dir_uri, pictures_dir_basename);
  g_free (pictures_dir_uri);
  g_free (pictures_dir_basename);

  /* translators: %s here is the name of the Pictures directory, the string should be translated in
   * the context "You can add images to your Pictures folder and they will show up here" */
  markup = g_strdup_printf (_("You can add images to your %s folder and they will show up here"), href);
  g_free (href);
  gtk_label_set_markup (GTK_LABEL (label), (const gchar *) markup);
  g_free (markup);
  gtk_widget_show (label);
  gtk_container_add (GTK_CONTAINER (labels_grid), label);

  priv->icon_view = gtk_icon_view_new ();
  gtk_widget_set_hexpand (priv->icon_view, TRUE);
  gtk_container_add (GTK_CONTAINER (priv->sw_content), priv->icon_view);
  g_signal_connect (priv->icon_view, "selection-changed", G_CALLBACK (on_selection_changed), chooser);
  g_signal_connect (priv->icon_view, "item-activated", G_CALLBACK (on_item_activated), chooser);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->icon_view),
                                  renderer,
                                  "surface", 0,
                                  NULL);

  gtk_dialog_add_button (GTK_DIALOG (chooser), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (chooser), _("Select"), GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_OK, FALSE);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (chooser), GTK_RESPONSE_CANCEL);
  size_group = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  gtk_size_group_add_widget (size_group, button);
  gtk_size_group_add_widget (size_group, hbox);

  on_view_toggled (GTK_TOGGLE_BUTTON (button1), chooser);
  gtk_widget_show_all (vbox);
}

static void
cc_background_chooser_dialog_class_init (CcBackgroundChooserDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = cc_background_chooser_dialog_dispose;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = cc_background_chooser_dialog_realize;

  g_type_class_add_private (object_class, sizeof (CcBackgroundChooserDialogPrivate));
}

GtkWidget *
cc_background_chooser_dialog_new (void)
{
  return g_object_new (CC_TYPE_BACKGROUND_CHOOSER_DIALOG, "use-header-bar", TRUE, NULL);
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
