/* mouse-properties-capplet.h
 *
 * Copyright (C) 2010 Intel, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Thomas Wood <thos@gnome.org>
 */


#ifndef __GNOME_MOUSE_PROPETIES_H__
#define __GNOME_MOUSE_PROPETIES_H__

#include <gconf/gconf-client.h>
#include <gtk/gtk.h>

GtkBuilder* create_dialog (void);
GConfClient* mouse_properties_conf_init (void);

#endif /* __GNOME_MOUSE_PROPETIES_H__ */
