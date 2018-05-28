/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

#ifndef _CC_MOUSE_PROPERTIES_H
#define _CC_MOUSE_PROPERTIES_H

G_BEGIN_DECLS

#define CC_TYPE_MOUSE_PROPERTIES (cc_mouse_properties_get_type ())
G_DECLARE_FINAL_TYPE (CcMouseProperties, cc_mouse_properties, CC, MOUSE_PROPERTIES, GtkBin)

GtkWidget *cc_mouse_properties_new (void);

G_END_DECLS

#endif /* _CC_MOUSE_PROPERTIES_H */
