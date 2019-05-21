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

struct _CcBackgroundChooserDialog
{
  GtkDialog parent_instance;

  GtkListStore *sources;
  GtkWidget *stack;
  GtkWidget *pictures_stack;

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

G_DEFINE_TYPE (CcBackgroundChooserDialog, cc_background_chooser_dialog, GTK_TYPE_DIALOG)

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

static void
cc_background_chooser_dialog_realize (GtkWidget *widget)
{
  CcBackgroundChooserDialog *chooser = CC_BACKGROUND_CHOOSER_DIALOG (widget);
  GtkWindow *parent;

  parent = gtk_window_get_transient_for (GTK_WINDOW (chooser));

  if (parent == NULL)
    {
      gtk_window_set_default_size (GTK_WINDOW (chooser), -1, 550);
    }
  else
    {
      gint width;
      gint height;

      gtk_window_get_size (parent, &width, &height);
      gtk_window_set_default_size (GTK_WINDOW (chooser), -1, (gint) (0.66 * height));
    }

  GTK_WIDGET_CLASS (cc_background_chooser_dialog_parent_class)->realize (widget);
}

static void
cc_background_chooser_dialog_dispose (GObject *object)
{
  CcBackgroundChooserDialog *chooser = CC_BACKGROUND_CHOOSER_DIALOG (object);

  if (chooser->copy_cancellable)
    {
      /* cancel any copy operation */
      g_cancellable_cancel (chooser->copy_cancellable);

      g_clear_object (&chooser->copy_cancellable);
    }

  g_clear_pointer (&chooser->item_to_focus, gtk_tree_row_reference_free);
  g_clear_object (&chooser->pictures_source);
  g_clear_object (&chooser->colors_source);
  g_clear_object (&chooser->wallpapers_source);
  g_clear_object (&chooser->thumb_factory);

  G_OBJECT_CLASS (cc_background_chooser_dialog_parent_class)->dispose (object);
}

static GtkWidget *
get_visible_view (CcBackgroundChooserDialog *chooser)
{
  GtkWidget *visible;
  GtkWidget *icon_view = NULL;

  visible = gtk_stack_get_visible_child (GTK_STACK (chooser->stack));
  if (GTK_IS_STACK (visible))
    {
      GtkWidget *sw;

      sw = gtk_stack_get_child_by_name (GTK_STACK (visible), "view");
      icon_view = gtk_bin_get_child (GTK_BIN (sw));
    }
  else if (GTK_IS_SCROLLED_WINDOW (visible))
    {
      icon_view = gtk_bin_get_child (GTK_BIN (visible));
    }
  else
    {
      g_assert_not_reached ();
    }

  return icon_view;
}

static void
possibly_show_empty_pictures_box (GtkTreeModel              *model,
                                  CcBackgroundChooserDialog *chooser)
{
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first (model, &iter))
    {
      gtk_stack_set_visible_child_name (GTK_STACK (chooser->pictures_stack), "view");
    }
  else
    {
      gtk_stack_set_visible_child_name (GTK_STACK (chooser->pictures_stack), "empty");
    }
}

static void
on_source_modified_cb (GtkTreeModel *tree_model,
                       GtkTreePath  *path,
                       GtkTreeIter  *iter,
                       gpointer      user_data)
{
  CcBackgroundChooserDialog *chooser = user_data;
  GtkTreePath *to_focus_path;
  GtkWidget *icon_view;

  if (chooser->item_to_focus == NULL)
    return;

  to_focus_path = gtk_tree_row_reference_get_path (chooser->item_to_focus);

  if (gtk_tree_path_compare (to_focus_path, path) != 0)
    goto out;

  /* Change source */
  gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "pictures");

  /* And select the newly added item */
  icon_view = get_visible_view (chooser);
  gtk_icon_view_select_path (GTK_ICON_VIEW (icon_view), to_focus_path);
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (icon_view),
                                to_focus_path, TRUE, 1.0, 1.0);
  g_clear_pointer (&chooser->item_to_focus, gtk_tree_row_reference_free);

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

  model = GTK_TREE_MODEL (bg_source_get_liststore (BG_SOURCE (chooser->pictures_source)));

  g_signal_connect (model, "row-inserted", G_CALLBACK (on_source_added_cb), chooser);
  g_signal_connect (model, "row-deleted", G_CALLBACK (on_source_removed_cb), chooser);
  g_signal_connect (model, "row-changed", G_CALLBACK (on_source_modified_cb), chooser);

  possibly_show_empty_pictures_box (model, chooser);
}

static void
on_visible_child_notify (CcBackgroundChooserDialog *chooser)
{
  GtkWidget *icon_view;

  icon_view = get_visible_view (chooser);
  gtk_icon_view_unselect_all (GTK_ICON_VIEW (icon_view));
  gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_OK, FALSE);
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
  g_clear_pointer (&chooser->item_to_focus, gtk_tree_row_reference_free);

  bg_pictures_source_add (chooser->pictures_source, uri, &chooser->item_to_focus);
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
  GtkWidget *icon_view;

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

  if (bg_colors_source_add (chooser->colors_source, &rgba, &row_ref) == FALSE)
    return FALSE;

  /* Change source */
  gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "colors");

  /* And select the newly added item */
  to_focus_path = gtk_tree_row_reference_get_path (row_ref);
  icon_view = get_visible_view (chooser);
  gtk_icon_view_select_path (GTK_ICON_VIEW (icon_view), to_focus_path);
  gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (icon_view),
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
  g_auto(GStrv) uris = NULL;
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
      if (!bg_pictures_source_is_known (chooser->pictures_source, uri))
        {
          add_custom_wallpaper (chooser, uri);
          ret = TRUE;
        }
    }

out:
  gtk_drag_finish (context, ret, FALSE, time);
}

static GtkWidget *
create_view (CcBackgroundChooserDialog *chooser, GtkTreeModel *model)
{
  GtkCellRenderer *renderer;
  GtkWidget *icon_view;
  GtkWidget *sw;

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (sw);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);

  icon_view = gtk_icon_view_new ();
  gtk_widget_show (icon_view);
  gtk_icon_view_set_model (GTK_ICON_VIEW (icon_view), model);
  gtk_widget_set_hexpand (icon_view, TRUE);
  gtk_container_add (GTK_CONTAINER (sw), icon_view);
  g_signal_connect (icon_view, "selection-changed", G_CALLBACK (on_selection_changed), chooser);
  g_signal_connect (icon_view, "item-activated", G_CALLBACK (on_item_activated), chooser);

  gtk_icon_view_set_columns (GTK_ICON_VIEW (icon_view), 3);

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (icon_view),
                                  renderer,
                                  "surface", 0,
                                  NULL);

  return sw;
}

static void
cc_background_chooser_dialog_constructed (GObject *object)
{
  CcBackgroundChooserDialog *chooser = CC_BACKGROUND_CHOOSER_DIALOG (object);
  GtkListStore *model;
  GtkWidget *sw;

  G_OBJECT_CLASS (cc_background_chooser_dialog_parent_class)->constructed (object);

  model = bg_source_get_liststore (BG_SOURCE (chooser->wallpapers_source));
  sw = create_view (chooser, GTK_TREE_MODEL (model));
  gtk_widget_show (sw);
  gtk_stack_add_titled (GTK_STACK (chooser->stack), sw, "wallpapers", _("Wallpapers"));
  gtk_container_child_set (GTK_CONTAINER (chooser->stack), sw, "position", 0, NULL);

  model = bg_source_get_liststore (BG_SOURCE (chooser->pictures_source));
  sw = create_view (chooser, GTK_TREE_MODEL (model));
  gtk_widget_show (sw);
  gtk_stack_add_named (GTK_STACK (chooser->pictures_stack), sw, "view");

  model = bg_source_get_liststore (BG_SOURCE (chooser->colors_source));
  sw = create_view (chooser, GTK_TREE_MODEL (model));
  gtk_widget_show (sw);
  gtk_stack_add_titled (GTK_STACK (chooser->stack), sw, "colors", _("Colors"));

  gtk_stack_set_visible_child_name (GTK_STACK (chooser->stack), "wallpapers");
  monitor_pictures_model (chooser);
}

static void
cc_background_chooser_dialog_init (CcBackgroundChooserDialog *chooser)
{
  GtkWidget *empty_pictures_box;
  GtkWidget *vbox;
  GtkWidget *headerbar;
  GtkWidget *img;
  GtkWidget *labels_grid;
  GtkWidget *label;
  GtkWidget *switcher;
  GtkStyleContext *context;
  g_autofree gchar *markup = NULL;
  g_autofree gchar *markup2 = NULL;
  g_autofree gchar *href = NULL;
  const gchar *pictures_dir;
  g_autofree gchar *pictures_dir_basename = NULL;
  g_autofree gchar *pictures_dir_uri = NULL;
  GtkTargetList *target_list;

  chooser->wallpapers_source = bg_wallpapers_source_new (GTK_WIDGET (chooser));
  chooser->pictures_source = bg_pictures_source_new (GTK_WIDGET (chooser));
  chooser->colors_source = bg_colors_source_new (GTK_WIDGET (chooser));

  gtk_window_set_modal (GTK_WINDOW (chooser), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (chooser), FALSE);
  /* translators: This is the title of the wallpaper chooser dialog. */
  gtk_window_set_title (GTK_WINDOW (chooser), _("Select Background"));

  vbox = gtk_dialog_get_content_area (GTK_DIALOG (chooser));
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

  chooser->stack = gtk_stack_new ();
  gtk_widget_show (chooser->stack);
  gtk_stack_set_homogeneous (GTK_STACK (chooser->stack), TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), chooser->stack);

  /* Add drag and drop support for bg images */
  gtk_drag_dest_set (chooser->stack, GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY);
  target_list = gtk_target_list_new (NULL, 0);
  gtk_target_list_add_uri_targets (target_list, URI_LIST);
  gtk_target_list_add_table (target_list, color_targets, 1);
  gtk_drag_dest_set_target_list (chooser->stack, target_list);
  gtk_target_list_unref (target_list);
  g_signal_connect (chooser->stack, "drag-data-received", G_CALLBACK (cc_background_panel_drag_items), chooser);

  headerbar = gtk_dialog_get_header_bar (GTK_DIALOG (chooser));

  switcher = gtk_stack_switcher_new ();
  gtk_widget_show (switcher);
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (chooser->stack));
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (headerbar), switcher);

  chooser->pictures_stack = gtk_stack_new ();
  gtk_widget_show (chooser->pictures_stack);
  gtk_stack_set_homogeneous (GTK_STACK (chooser->pictures_stack), TRUE);
  gtk_stack_add_titled (GTK_STACK (chooser->stack), chooser->pictures_stack, "pictures", _("Pictures"));

  empty_pictures_box = gtk_grid_new ();
  gtk_widget_show (empty_pictures_box);
  gtk_grid_set_column_spacing (GTK_GRID (empty_pictures_box), 12);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (empty_pictures_box),
                                  GTK_ORIENTATION_HORIZONTAL);
  context = gtk_widget_get_style_context (empty_pictures_box);
  gtk_style_context_add_class (context, "dim-label");
  gtk_stack_add_named (GTK_STACK (chooser->pictures_stack), empty_pictures_box, "empty");
  img = gtk_image_new_from_icon_name ("emblem-photos-symbolic", GTK_ICON_SIZE_DIALOG);
  gtk_widget_show (img);
  gtk_image_set_pixel_size (GTK_IMAGE (img), 64);
  gtk_widget_set_halign (img, GTK_ALIGN_END);
  gtk_widget_set_valign (img, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (img, TRUE);
  gtk_widget_set_vexpand (img, TRUE);
  gtk_container_add (GTK_CONTAINER (empty_pictures_box), img);
  labels_grid = gtk_grid_new ();
  gtk_widget_show (labels_grid);
  gtk_widget_set_halign (labels_grid, GTK_ALIGN_START);
  gtk_widget_set_valign (labels_grid, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (labels_grid, TRUE);
  gtk_widget_set_vexpand (labels_grid, TRUE);
  gtk_grid_set_row_spacing (GTK_GRID (labels_grid), 6);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (labels_grid),
                                  GTK_ORIENTATION_VERTICAL);
  gtk_widget_show (labels_grid);
  gtk_container_add (GTK_CONTAINER (empty_pictures_box), labels_grid);
  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  markup = g_markup_printf_escaped ("<b><span size='large'>%s</span></b>",
                                    /* translators: No pictures were found */
                                    _("No Pictures Found"));
  gtk_label_set_markup (GTK_LABEL (label), markup);
  gtk_container_add (GTK_CONTAINER (labels_grid), label);
  label = gtk_label_new ("");
  gtk_widget_show (label);
  gtk_label_set_max_width_chars (GTK_LABEL (label), 24);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);

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

  /* translators: %s here is the name of the Pictures directory, the string should be translated in
   * the context "You can add images to your Pictures folder and they will show up here" */
  markup2 = g_strdup_printf (_("You can add images to your %s folder and they will show up here"), href);
  gtk_label_set_markup (GTK_LABEL (label), markup2);
  gtk_container_add (GTK_CONTAINER (labels_grid), label);

  gtk_dialog_add_button (GTK_DIALOG (chooser), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (chooser), _("_Select"), GTK_RESPONSE_OK);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_OK);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser), GTK_RESPONSE_OK, FALSE);

  g_signal_connect_object (chooser->stack, "notify::visible-child", G_CALLBACK (on_visible_child_notify), chooser, G_CONNECT_SWAPPED);
}

static void
cc_background_chooser_dialog_class_init (CcBackgroundChooserDialogClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->constructed = cc_background_chooser_dialog_constructed;
  object_class->dispose = cc_background_chooser_dialog_dispose;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->realize = cc_background_chooser_dialog_realize;
}

GtkWidget *
cc_background_chooser_dialog_new (GtkWindow *transient_for)
{
  return g_object_new (CC_TYPE_BACKGROUND_CHOOSER_DIALOG,
                       "transient-for", transient_for,
                       "use-header-bar", TRUE,
                       NULL);
}

CcBackgroundItem *
cc_background_chooser_dialog_get_item (CcBackgroundChooserDialog *chooser)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *icon_view;
  GList *list;
  CcBackgroundItem *item;

  item = NULL;
  icon_view = get_visible_view (chooser);
  list = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (icon_view));

  if (!list)
    return NULL;

  model = gtk_icon_view_get_model (GTK_ICON_VIEW (icon_view));

  if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data) == FALSE)
    goto bail;

  gtk_tree_model_get (model, &iter, 1, &item, -1);

bail:
  g_list_free_full (list, (GDestroyNotify) gtk_tree_path_free);

  return item;
}
