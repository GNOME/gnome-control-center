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

  GtkFlowBox         *flowbox;
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
  g_autoptr(GList) list = NULL;
  CcBackgroundItem *item;

  list = gtk_flow_box_get_selected_children (self->flowbox);
  g_assert (g_list_length (list) == 1);

  item = g_object_get_data (list->data, "item");

  g_signal_emit (self, signals[BACKGROUND_CHOSEN], 0, item, flags);
}

static GtkWidget*
create_widget_func (gpointer model_item,
                    gpointer user_data)
{
  g_autoptr(GdkPixbuf) pixbuf = NULL;
  CcBackgroundChooser *self;
  CcBackgroundItem *item;
  GtkWidget *child;
  GtkWidget *image;

  self = CC_BACKGROUND_CHOOSER (user_data);
  item = CC_BACKGROUND_ITEM (model_item);
  pixbuf = cc_background_item_get_thumbnail (item,
                                             bg_source_get_thumbnail_factory (BG_SOURCE (self->wallpapers_source)),
                                             bg_source_get_thumbnail_width (BG_SOURCE (self->wallpapers_source)),
                                             bg_source_get_thumbnail_height (BG_SOURCE (self->wallpapers_source)),
                                             bg_source_get_scale_factor (BG_SOURCE (self->wallpapers_source)));
  image = gtk_image_new_from_pixbuf (pixbuf);
  gtk_widget_show (image);

  child = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                        "halign", GTK_ALIGN_CENTER,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  gtk_container_add (GTK_CONTAINER (child), image);
  gtk_widget_show (child);

  g_object_set_data_full (G_OBJECT (child), "item", g_object_ref (item), g_object_unref);

  return child;
}

static void
setup_flowbox (CcBackgroundChooser *self)
{
  GListStore *store;

  store = bg_source_get_liststore (BG_SOURCE (self->wallpapers_source));

  gtk_flow_box_bind_model (self->flowbox,
                           G_LIST_MODEL (store),
                           create_widget_func,
                           self,
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
on_item_activated_cb (GtkFlowBox          *flowbox,
                      GtkFlowBoxChild     *child,
                      CcBackgroundChooser *self)
{
  gtk_popover_set_relative_to (self->selection_popover, GTK_WIDGET (child));
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

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, flowbox);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundChooser, selection_popover);

  gtk_widget_class_bind_template_callback (widget_class, on_item_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_desktop_lock_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_desktop_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_lock_clicked_cb);
}

static void
cc_background_chooser_init (CcBackgroundChooser *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->wallpapers_source = bg_wallpapers_source_new (GTK_WIDGET (self));
  setup_flowbox (self);
}
