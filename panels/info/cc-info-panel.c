/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include "cc-info-panel.h"
#include "cc-info-overview-panel.h"
#include "cc-info-default-apps-panel.h"
#include "cc-info-removable-media-panel.h"
#include "cc-info-resources.h"
#include "info-cleanup.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gsd-disk-space-helper.h"

#define WID(w) (GtkWidget *) gtk_builder_get_object (self->builder, w)

struct _CcInfoPanel
{
  CcPanel     parent_instance;

  GtkBuilder *builder;
};

CC_PANEL_REGISTER (CcInfoPanel, cc_info_panel)

static void
cc_info_panel_dispose (GObject *object)
{
  CcInfoPanel *self = CC_INFO_PANEL (object);

  g_clear_object (&self->builder);

  G_OBJECT_CLASS (cc_info_panel_parent_class)->dispose (object);
}

static void
cc_info_panel_class_init (CcInfoPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_info_panel_dispose;

  g_type_ensure (CC_TYPE_INFO_OVERVIEW_PANEL);
  g_type_ensure (CC_TYPE_INFO_DEFAULT_APPS_PANEL);
  g_type_ensure (CC_TYPE_INFO_REMOVABLE_MEDIA_PANEL);
}

static void
on_section_changed (GtkTreeSelection  *selection,
                    gpointer           data)
{
  CcInfoPanel *self = CC_INFO_PANEL (data);
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *path;
  gint *indices;
  int index;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path = gtk_tree_model_get_path (model, &iter);

  indices = gtk_tree_path_get_indices (path);
  index = indices[0];

  if (index >= 0)
    {
      g_object_set (G_OBJECT (WID ("notebook")),
                    "page", index, NULL);
    }

  gtk_tree_path_free (path);
}

static void
info_panel_setup_selector (CcInfoPanel  *self)
{
  GtkTreeView *view;
  GtkListStore *model;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  int section_name_column = 0;

  view = GTK_TREE_VIEW (WID ("overview_treeview"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  model = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (view, GTK_TREE_MODEL (model));
  g_object_unref (model);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_padding (renderer, 4, 4);
  g_object_set (renderer,
                "width-chars", 20,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  column = gtk_tree_view_column_new_with_attributes (_("Section"),
                                                     renderer,
                                                     "text", section_name_column,
                                                     NULL);
  gtk_tree_view_append_column (view, column);


  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Overview"),
                      -1);
  gtk_tree_selection_select_iter (selection, &iter);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Default Applications"),
                      -1);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Removable Media"),
                      -1);

  g_signal_connect (selection, "changed",
                    G_CALLBACK (on_section_changed), self);
  on_section_changed (selection, self);

  gtk_widget_show_all (GTK_WIDGET (view));
}

static void
info_panel_setup_overview (CcInfoPanel  *self)
{
  GtkWidget  *widget;

  widget = WID ("info_vbox");
  gtk_container_add (GTK_CONTAINER (self), widget);
}

static void
cc_info_panel_init (CcInfoPanel *self)
{
  GError *error = NULL;

  g_resources_register (cc_info_get_resource ());

  self->builder = gtk_builder_new ();

  if (gtk_builder_add_from_resource (self->builder,
                                     "/org/gnome/control-center/info/info.ui",
                                     &error) == 0)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  info_panel_setup_selector (self);
  info_panel_setup_overview (self);
}
