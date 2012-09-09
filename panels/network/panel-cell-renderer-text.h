/* -*- Text: C; tab-width: 8; indent-tabs-text: nil; c-basic-offset: 8 -*-
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
 *
 * Written by Matthias Clasen
 */

#ifndef PANEL_CELL_RENDERER_TEXT_H
#define PANEL_CELL_RENDERER_TEXT_H

#include <glib-object.h>
#include <gtk/gtk.h>

#define PANEL_TYPE_CELL_RENDERER_TEXT           (panel_cell_renderer_text_get_type())
#define PANEL_CELL_RENDERER_TEXT(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), PANEL_TYPE_CELL_RENDERER_TEXT, PanelCellRendererText))
#define PANEL_CELL_RENDERER_TEXT_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST((cls), PANEL_TYPE_CELL_RENDERER_TEXT, PanelCellRendererTextClass))
#define PANEL_IS_CELL_RENDERER_TEXT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), PANEL_TYPE_CELL_RENDERER_TEXT))
#define PANEL_IS_CELL_RENDERER_TEXT_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE((cls), PANEL_TYPE_CELL_RENDERER_TEXT))
#define PANEL_CELL_RENDERER_TEXT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_CELL_RENDERER_TEXT, PanelCellRendererTextClass))

G_BEGIN_DECLS

typedef struct _PanelCellRendererText           PanelCellRendererText;
typedef struct _PanelCellRendererTextClass      PanelCellRendererTextClass;

struct _PanelCellRendererText
{
        GtkCellRendererText parent;
};

struct _PanelCellRendererTextClass
{
        GtkCellRendererTextClass parent_class;

        void (*activate) (PanelCellRendererText *text,
                          const gchar           *path);
};

GType            panel_cell_renderer_text_get_type (void);
GtkCellRenderer *panel_cell_renderer_text_new      (void);

G_END_DECLS

#endif /* PANEL_CELL_RENDERER_TEXT_H */

