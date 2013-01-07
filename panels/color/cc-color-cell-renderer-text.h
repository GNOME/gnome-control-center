/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Richard Hughes <richard@hughsie.com>
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

#ifndef CC_COLOR_CELL_RENDERER_TEXT_H
#define CC_COLOR_CELL_RENDERER_TEXT_H

#include <gtk/gtk.h>

#define CC_COLOR_TYPE_CELL_RENDERER_TEXT                (cc_color_cell_renderer_text_get_type())
#define CC_COLOR_CELL_RENDERER_TEXT(obj)                (G_TYPE_CHECK_INSTANCE_CAST((obj), CC_COLOR_TYPE_CELL_RENDERER_TEXT, CcColorCellRendererText))
#define CC_COLOR_CELL_RENDERER_TEXT_CLASS(cls)          (G_TYPE_CHECK_CLASS_CAST((cls), CC_COLOR_TYPE_CELL_RENDERER_TEXT, CcColorCellRendererTextClass))
#define CC_COLOR_IS_CELL_RENDERER_TEXT(obj)             (G_TYPE_CHECK_INSTANCE_TYPE((obj), CC_COLOR_TYPE_CELL_RENDERER_TEXT))
#define CC_COLOR_IS_CELL_RENDERER_TEXT_CLASS(cls)       (G_TYPE_CHECK_CLASS_TYPE((cls), CC_COLOR_TYPE_CELL_RENDERER_TEXT))
#define CC_COLOR_CELL_RENDERER_TEXT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS((obj), CC_COLOR_TYPE_CELL_RENDERER_TEXT, CcColorCellRendererTextClass))

G_BEGIN_DECLS

typedef struct _CcColorCellRendererText         CcColorCellRendererText;
typedef struct _CcColorCellRendererTextClass    CcColorCellRendererTextClass;

struct _CcColorCellRendererText
{
  GtkCellRendererText      parent;
  gboolean                 is_dim_label;
};

struct _CcColorCellRendererTextClass
{
  GtkCellRendererTextClass parent_class;
};

GType            cc_color_cell_renderer_text_get_type           (void);
GtkCellRenderer *cc_color_cell_renderer_text_new                (void);

G_END_DECLS

#endif /* CC_COLOR_CELL_RENDERER_TEXT_H */

