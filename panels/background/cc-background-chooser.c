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

#define G_LOG_DOMAIN "cc-background-chooser"

#include "bg-colors-source.h"
#include "bg-pictures-source.h"
#include "bg-wallpapers-source.h"
#include "cc-background-chooser.h"

struct _CcBackgroundChooser
{
  GtkBox              parent;

  GtkIconView        *icon_view;
  GtkPopover         *selection_popover;

  BgWallpapersSource *wallpapers_source;
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
  g_autolist (GtkTreePath) list = NULL;
  CcBackgroundItem *item;
  GtkTreeModel *model;
  GtkTreeIter iter;

  model = gtk_icon_view_get_model (self->icon_view);
  list = gtk_icon_view_get_selected_items (self->icon_view);
  g_assert (g_list_length (list) == 1);

  if (gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) list->data) == FALSE)
    return;

  gtk_tree_model_get (model, &iter, 1, &item, -1);

  g_signal_emit (self, signals[BACKGROUND_CHOSEN], 0, item, flags);
}

static void
setup_icon_view (CcBackgroundChooser *self)
{
  GtkCellRenderer *renderer;
  GtkListStore *model;

  model = bg_source_get_liststore (BG_SOURCE (self->wallpapers_source));

  gtk_icon_view_set_model (self->icon_view, GTK_TREE_MODEL (model));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->icon_view),
                              renderer,
                              FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self->icon_view),
                                  renderer,
                                  "surface", 0,
                                  NULL);

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
on_selection_changed_cb (GtkIconView         *icon_view,
                         CcBackgroundChooser *self)
{
}

static void
on_item_activated_cb (GtkIconView         *icon_view,
                      GtkTreePath         *path,
                      CcBackgroundChooser *self)
{
  GdkRectangle rect;

  g_message ("Item activated");

  gtk_icon_view_get_cell_rect (icon_view, path, NULL, &rect);
  gtk_popover_set_pointing_to (self->selection_popover, &rect);
  gtk_popover_popup (self->selection_popover);
}

/* GObject overrides */

static void
cc_background_chooser_finalize (GObject *object)
{
  CcBackgroundChooser *self = (CcBackgroundChooser *)object;

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

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, icon_view);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, selection_popover);

  gtk_widget_class_bind_template_callback (widget_class, on_item_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_desktop_lock_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_desktop_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_lock_clicked_cb);
}

static void
cc_background_chooser_init (CcBackgroundChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->wallpapers_source = bg_wallpapers_source_new (GTK_WIDGET (self));
  setup_icon_view (self);
}
