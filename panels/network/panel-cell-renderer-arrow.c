/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "panel-cell-renderer-arrow.h"

enum {
  ACTIVATE,
  LAST_SIGNAL
};

static gint signals[LAST_SIGNAL];

G_DEFINE_TYPE (PanelCellRendererArrow, panel_cell_renderer_arrow, GTK_TYPE_CELL_RENDERER_PIXBUF)

static gint
panel_cell_renderer_arrow_activate (GtkCellRenderer      *cell,
                                    GdkEvent             *event,
                                    GtkWidget            *widget,
                                    const gchar          *path,
                                    const GdkRectangle   *background_area,
                                    const GdkRectangle   *cell_area,
                                    GtkCellRendererState  flags)
{
  g_signal_emit (cell, signals[ACTIVATE], 0, path);
  return TRUE;
}

static void
panel_cell_renderer_arrow_class_init (PanelCellRendererArrowClass *class)
{
        GtkCellRendererClass *r_class = GTK_CELL_RENDERER_CLASS (class);

        r_class->activate = panel_cell_renderer_arrow_activate;

  signals[ACTIVATE] =
    g_signal_new ("activate",
                  G_OBJECT_CLASS_TYPE (class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (PanelCellRendererArrowClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1, G_TYPE_STRING);
}


static void
panel_cell_renderer_arrow_init (PanelCellRendererArrow *renderer)
{
}

GtkCellRenderer *
panel_cell_renderer_arrow_new (void)
{
        return g_object_new (PANEL_TYPE_CELL_RENDERER_ARROW,
                             "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                             "icon-name", "go-next-symbolic",
                             NULL);
}

