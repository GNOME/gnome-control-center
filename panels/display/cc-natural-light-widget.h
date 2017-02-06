/*
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
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

#ifndef __CC_NATURAL_LIGHT_WIDGET_H__
#define __CC_NATURAL_LIGHT_WIDGET_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_NATURAL_LIGHT_WIDGET (cc_natural_light_widget_get_type ())
G_DECLARE_FINAL_TYPE (CcNaturalLightWidget, cc_natural_light_widget, CC, NATURAL_LIGHT_WIDGET, GtkDrawingArea)

GtkWidget   *cc_natural_light_widget_new      (void);
void         cc_natural_light_widget_set_to   (CcNaturalLightWidget *self,
                                               gdouble               to);
void         cc_natural_light_widget_set_from (CcNaturalLightWidget *self,
                                               gdouble               from);
void         cc_natural_light_widget_set_now  (CcNaturalLightWidget *self,
                                               gdouble               now);

G_END_DECLS

#endif
