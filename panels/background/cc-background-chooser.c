/* cc-background-chooser.c
 *
 * Copyright 2019 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-background-chooser"

#include <glib/gi18n.h>

#include "bg-colors-source.h"
#include "bg-pictures-source.h"
#include "bg-recent-source.h"
#include "bg-wallpapers-source.h"
#include "cc-background-chooser.h"

struct _CcBackgroundChooser
{
  GtkBox              parent;

  GtkFlowBox         *flowbox;
  GtkWidget          *popover_recent_box;
  GtkWidget          *recent_box;
  GtkFlowBox         *recent_flowbox;
  GtkPopover         *selection_popover;

  gboolean            recent_selected;

  BgWallpapersSource *wallpapers_source;
  BgRecentSource     *recent_source;
};

G_DEFINE_TYPE (CcBackgroundChooser, cc_background_chooser, GTK_TYPE_BOX)

enum
{
  BACKGROUND_CHOSEN,
  N_SIGNALS,
};

static guint signals [N_SIGNALS];

static void
emit_background_chosen (CcBackgroundChooser        *self,
                        CcBackgroundSelectionFlags  flags)
{
  g_autoptr(GList) list = NULL;
  CcBackgroundItem *item;
  GtkFlowBox *flowbox;

  flowbox = self->recent_selected ? self->recent_flowbox : self->flowbox;
  list = gtk_flow_box_get_selected_children (flowbox);
  g_assert (g_list_length (list) == 1);

  item = g_object_get_data (list->data, "item");

  g_signal_emit (self, signals[BACKGROUND_CHOSEN], 0, item, flags);

  gtk_flow_box_unselect_all (flowbox);
}

static GtkWidget*
create_widget_func (gpointer model_item,
                    gpointer user_data)
{
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  CcBackgroundItem *item;
  GtkWidget *overlay;
  GtkWidget *child;
  GtkWidget *image;
  GtkWidget *icon;
  BgSource *source;

  source = BG_SOURCE (user_data);
  item = CC_BACKGROUND_ITEM (model_item);
  pixbuf = cc_background_item_get_thumbnail (item,
                                             bg_source_get_thumbnail_factory (source),
                                             bg_source_get_thumbnail_width (source),
                                             bg_source_get_thumbnail_height (source),
                                             bg_source_get_scale_factor (source));
  image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_widget_show (image);

  icon = g_object_new (GTK_TYPE_IMAGE,
                       "icon-name", "slideshow-emblem",
                       "pixel-size", 16,
                       "margin", 8,
                       "halign", GTK_ALIGN_END,
                       "valign", GTK_ALIGN_END,
                       "visible", cc_background_item_changes_with_time (item),
                       NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (icon), "slideshow-emblem");

  overlay = gtk_overlay_new ();
  gtk_container_add (GTK_CONTAINER (overlay), image);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), icon);
  gtk_widget_show (overlay);

  child = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                        "halign", GTK_ALIGN_CENTER,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  gtk_container_add (GTK_CONTAINER (child), overlay);
  gtk_widget_show (child);

  g_object_set_data_full (G_OBJECT (child), "item", g_object_ref (item), g_object_unref);

  return child;
}

static void
update_recent_visibility (CcBackgroundChooser *self)
{
  GListStore *store;
  gboolean has_items;

  store = bg_source_get_liststore (BG_SOURCE (self->recent_source));
  has_items = g_list_model_get_n_items (G_LIST_MODEL (store)) != 0;

  gtk_widget_set_visible (self->recent_box, has_items);
}

static void
setup_flowbox (CcBackgroundChooser *self)
{
  GListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (self->wallpapers_source));

  gtk_flow_box_bind_model (self->flowbox,
                           G_LIST_MODEL (store),
                           create_widget_func,
                           self->wallpapers_source,
                           NULL);

  store = bg_source_get_liststore (BG_SOURCE (self->recent_source));

  gtk_flow_box_bind_model (self->recent_flowbox,
                           G_LIST_MODEL (store),
                           create_widget_func,
                           self->recent_source,
                           NULL);

  update_recent_visibility (self);
  g_signal_connect_object (store,
                           "items-changed",
                           G_CALLBACK (update_recent_visibility),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
on_delete_background_clicked_cb (GtkButton           *button,
                                 CcBackgroundChooser *self)
{
  g_autoptr(GList) list = NULL;
  CcBackgroundItem *item;

  list = gtk_flow_box_get_selected_children (self->recent_flowbox);
  g_assert (g_list_length (list) == 1);

  item = g_object_get_data (list->data, "item");

  bg_recent_source_remove_item (self->recent_source, item);
}

static void
on_selection_desktop_lock_clicked_cb (GtkButton           *button,
                                      CcBackgroundChooser *self)
{
  emit_background_chosen (self, CC_BACKGROUND_SELECTION_DESKTOP | CC_BACKGROUND_SELECTION_LOCK_SCREEN);
  gtk_popover_popdown (self->selection_popover);
}

static void
on_selection_desktop_clicked_cb (GtkButton           *button,
                                 CcBackgroundChooser *self)
{
  emit_background_chosen (self, CC_BACKGROUND_SELECTION_DESKTOP);
  gtk_popover_popdown (self->selection_popover);
}

static void
on_selection_lock_clicked_cb (GtkButton           *button,
                              CcBackgroundChooser *self)
{
  emit_background_chosen (self, CC_BACKGROUND_SELECTION_LOCK_SCREEN);
  gtk_popover_popdown (self->selection_popover);
}

static void
on_item_activated_cb (GtkFlowBox          *flowbox,
                      GtkFlowBoxChild     *child,
                      CcBackgroundChooser *self)
{
  self->recent_selected = flowbox == self->recent_flowbox;
  gtk_widget_set_visible (self->popover_recent_box, self->recent_selected);

  gtk_popover_set_relative_to (self->selection_popover, GTK_WIDGET (child));
  gtk_popover_popup (self->selection_popover);
}

static void
on_file_chooser_response_cb (GtkDialog           *filechooser,
                             gint                 response,
                             CcBackgroundChooser *self)
{
  if (response == GTK_RESPONSE_ACCEPT)
    {
      g_autofree gchar *filename = NULL;

      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
      bg_recent_source_add_file (self->recent_source, filename);
    }

  gtk_widget_destroy (GTK_WIDGET (filechooser));
}

static void
on_file_chooser_selection_changed_cb (GtkFileChooser               *chooser,
                                      GnomeDesktopThumbnailFactory *thumbnail_factory)
{
  g_autofree gchar *uri = NULL;

  uri = gtk_file_chooser_get_uri (chooser);

  if (uri)
    {
      g_autoptr(GFileInfo) file_info = NULL;
      g_autoptr(GdkPixbuf) pixbuf = NULL;
      g_autofree gchar *mime_type = NULL;
      g_autoptr(GFile) file = NULL;
      GtkWidget *preview;

      preview = gtk_file_chooser_get_preview_widget (chooser);

      file = g_file_new_for_uri (uri);
      file_info = g_file_query_info (file,
                                     "standard::*",
                                     G_FILE_QUERY_INFO_NONE,
                                     NULL,
                                     NULL);

      if (file_info && g_file_info_get_file_type (file_info) != G_FILE_TYPE_DIRECTORY)
        mime_type = g_strdup (g_file_info_get_content_type (file_info));

      if (mime_type)
        {
          pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory,
                                                                       uri,
                                                                       mime_type);
        }

      gtk_dialog_set_response_sensitive (GTK_DIALOG (chooser),
                                         GTK_RESPONSE_ACCEPT,
                                         pixbuf != NULL);

      if (pixbuf)
        gtk_image_set_from_pixbuf (GTK_IMAGE (preview), pixbuf);
      else
        gtk_image_set_from_icon_name (GTK_IMAGE (preview), "dialog-question", GTK_ICON_SIZE_DIALOG);
    }

  gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

/* GObject overrides */

static void
cc_background_chooser_finalize (GObject *object)
{
  CcBackgroundChooser *self = (CcBackgroundChooser *)object;

  g_clear_object (&self->recent_source);
  g_clear_object (&self->wallpapers_source);

  G_OBJECT_CLASS (cc_background_chooser_parent_class)->finalize (object);
}

static void
cc_background_chooser_class_init (CcBackgroundChooserClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_background_chooser_finalize;

  signals[BACKGROUND_CHOSEN] = g_signal_new ("background-chosen",
                                             CC_TYPE_BACKGROUND_CHOOSER,
                                             G_SIGNAL_RUN_FIRST,
                                             0, NULL, NULL, NULL,
                                             G_TYPE_NONE,
                                             2,
                                             CC_TYPE_BACKGROUND_ITEM,
                                             G_TYPE_INT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-chooser.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, popover_recent_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, recent_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, recent_flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, selection_popover);

  gtk_widget_class_bind_template_callback (widget_class, on_delete_background_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_item_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_desktop_lock_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_desktop_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_lock_clicked_cb);
}

static void
cc_background_chooser_init (CcBackgroundChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->recent_source = bg_recent_source_new (GTK_WIDGET (self));
  self->wallpapers_source = bg_wallpapers_source_new (GTK_WIDGET (self));
  setup_flowbox (self);
}

void
cc_background_chooser_select_file (CcBackgroundChooser *self)
{
  g_autoptr(GnomeDesktopThumbnailFactory) factory = NULL;
  GtkFileFilter *filter;
  GtkWidget *filechooser;
  GtkWindow *toplevel;
  GtkWidget *preview;

  g_return_if_fail (CC_IS_BACKGROUND_CHOOSER (self));

  toplevel = (GtkWindow*) gtk_widget_get_toplevel (GTK_WIDGET (self));
  filechooser = gtk_file_chooser_dialog_new (_("Select a picture"),
                                             toplevel,
                                             GTK_FILE_CHOOSER_ACTION_OPEN,
                                             _("_Cancel"), GTK_RESPONSE_CANCEL,
                                             _("_Open"), GTK_RESPONSE_ACCEPT,
                                             NULL);
  gtk_window_set_modal (GTK_WINDOW (filechooser), TRUE);

  preview = gtk_image_new ();
  gtk_widget_set_size_request (preview, 256, -1);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (filechooser), preview);
  gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (filechooser), FALSE);
  gtk_widget_show (preview);

  factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);
  g_signal_connect_after (filechooser,
                          "selection-changed",
                          G_CALLBACK (on_file_chooser_selection_changed_cb),
                          factory);

  g_object_set_data_full (G_OBJECT (filechooser),
                          "factory",
                          g_object_ref (factory),
                          g_object_unref);

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (filechooser), filter);

  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (filechooser),
                                       g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));

  g_signal_connect_object (filechooser,
                           "response",
                           G_CALLBACK (on_file_chooser_response_cb),
                           self,
                           0);

  gtk_window_present (GTK_WINDOW (filechooser));
}
