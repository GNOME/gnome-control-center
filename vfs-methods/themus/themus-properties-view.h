/* -*- mode: C; c-basic-offset: 4 -*-
 * themus - utilities for GNOME themes
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef THEMUS_PROPERTIES_VIEW_H
#define THEMUS_PROPERTIES_VIEW_H

#include <gtk/gtk.h>

#define THEMUS_TYPE_PROPERTIES_VIEW	    (themus_properties_view_get_type ())
#define THEMUS_PROPERTIES_VIEW(obj)	    (G_TYPE_CHECK_INSTANCE_CAST ((obj), THEMUS_TYPE_PROPERTIES_VIEW, ThemusPropertiesView))
#define THEMUS_PROPERTIES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THEMUS_TYPE_PROPERTIES_VIEW, ThemusPropertiesViewClass))
#define THEMUS_IS_PROPERTIES_VIEW(obj)	    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THEMUS_TYPE_PROPERTIES_VIEW))
#define THEMUS_IS_PROPERTIES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THEMUS_TYPE_PROPERTIES_VIEW))

typedef struct ThemusPropertiesViewDetails ThemusPropertiesViewDetails;

typedef struct {
	GtkTable parent;
	ThemusPropertiesViewDetails *details;
} ThemusPropertiesView;

typedef struct {
	GtkTableClass parent;
} ThemusPropertiesViewClass;

GType      themus_properties_view_get_type      (void);
void       themus_properties_view_register_type (GTypeModule *module);

GtkWidget *themus_properties_view_new           (const char *location);
void       themus_properties_view_set_location  (ThemusPropertiesView *view,
						 const char        *location);

#endif /* THEMUS_PROPERTIES_VIEW_H */
