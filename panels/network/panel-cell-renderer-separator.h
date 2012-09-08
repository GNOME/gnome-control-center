/* -*- Separator: C; tab-width: 8; indent-tabs-separator: nil; c-basic-offset: 8 -*-
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

#ifndef PANEL_CELL_RENDERER_SEPARATOR_H
#define PANEL_CELL_RENDERER_SEPARATOR_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "nm-device.h"
#include "nm-access-point.h"

#define PANEL_TYPE_CELL_RENDERER_SEPARATOR           (panel_cell_renderer_separator_get_type())
#define PANEL_CELL_RENDERER_SEPARATOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST((obj), PANEL_TYPE_CELL_RENDERER_SEPARATOR, PanelCellRendererSeparator))
#define PANEL_CELL_RENDERER_SEPARATOR_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST((cls), PANEL_TYPE_CELL_RENDERER_SEPARATOR, PanelCellRendererSeparatorClass))
#define PANEL_IS_CELL_RENDERER_SEPARATOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE((obj), PANEL_TYPE_CELL_RENDERER_SEPARATOR))
#define PANEL_IS_CELL_RENDERER_SEPARATOR_CLASS(cls)  (G_TYPE_CHECK_CLASS_TYPE((cls), PANEL_TYPE_CELL_RENDERER_SEPARATOR))
#define PANEL_CELL_RENDERER_SEPARATOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), PANEL_TYPE_CELL_RENDERER_SEPARATOR, PanelCellRendererSeparatorClass))

G_BEGIN_DECLS

typedef struct _PanelCellRendererSeparator           PanelCellRendererSeparator;
typedef struct _PanelCellRendererSeparatorClass      PanelCellRendererSeparatorClass;

struct _PanelCellRendererSeparator
{
        GtkCellRenderer parent;
        gboolean draw;
};

struct _PanelCellRendererSeparatorClass
{
        GtkCellRendererClass parent_class;
};

GType            panel_cell_renderer_separator_get_type (void);
GtkCellRenderer *panel_cell_renderer_separator_new      (void);

G_END_DECLS

#endif /* PANEL_CELL_RENDERER_SEPARATOR_H */

