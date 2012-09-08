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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "panel-cell-renderer-separator.h"

enum {
        PROP_0,
        PROP_DRAW,
        PROP_LAST
};

G_DEFINE_TYPE (PanelCellRendererSeparator, panel_cell_renderer_separator, GTK_TYPE_CELL_RENDERER)

static void
panel_cell_renderer_separator_get_property (GObject    *object,
                                            guint       param_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
        PanelCellRendererSeparator *renderer = PANEL_CELL_RENDERER_SEPARATOR (object);

        switch (param_id) {
        case PROP_DRAW:
                g_value_set_boolean (value, renderer->draw);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
panel_cell_renderer_separator_set_property (GObject      *object,
                                            guint         param_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
        PanelCellRendererSeparator *renderer = PANEL_CELL_RENDERER_SEPARATOR (object);

        switch (param_id) {
        case PROP_DRAW:
                renderer->draw = g_value_get_boolean (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
                break;
        }
}

static void
render (GtkCellRenderer      *cell,
        cairo_t              *cr,
        GtkWidget            *widget,
        const GdkRectangle   *background_area,
        const GdkRectangle   *cell_area,
        GtkCellRendererState  flags)
{
        PanelCellRendererSeparator *renderer = PANEL_CELL_RENDERER_SEPARATOR (cell);
        GtkStyleContext *context;
        gint x, y, w, h, xpad, ypad;

        if (!renderer->draw)
                return;

        context = gtk_widget_get_style_context (widget);

        gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

        x = cell_area->x + xpad;
        y = cell_area->y + ypad;
        w = cell_area->width - xpad * 2;
        h = cell_area->height - ypad * 2;

        gtk_render_line (context, cr, x + w / 2, y, x + w / 2, y + h - 1);
}

static void
panel_cell_renderer_separator_class_init (PanelCellRendererSeparatorClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GtkCellRendererClass *cell_renderer_class = GTK_CELL_RENDERER_CLASS (class);

        object_class->get_property = panel_cell_renderer_separator_get_property;
        object_class->set_property = panel_cell_renderer_separator_set_property;

        cell_renderer_class->render = render;

        g_object_class_install_property (object_class, PROP_DRAW,
                                         g_param_spec_boolean ("draw", "draw", "draw",
                                                               TRUE,
                                                               G_PARAM_READWRITE));
}

static void
panel_cell_renderer_separator_init (PanelCellRendererSeparator *renderer)
{
        renderer->draw = TRUE;
}

GtkCellRenderer *
panel_cell_renderer_separator_new (void)
{
        return g_object_new (PANEL_TYPE_CELL_RENDERER_SEPARATOR, NULL);
}
