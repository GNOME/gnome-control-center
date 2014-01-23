/*
 * Copyright (C) 2013 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Matthias Clasen
 */

#ifndef __CC_FORMAT_CHOOSER_H__
#define __CC_FORMAT_CHOOSER_H__

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

GtkWidget   *cc_format_chooser_new          (GtkWidget   *parent);
void         cc_format_chooser_clear_filter (GtkWidget   *chooser);
const gchar *cc_format_chooser_get_region   (GtkWidget   *chooser);
void         cc_format_chooser_set_region   (GtkWidget   *chooser,
                                             const gchar *region);

G_END_DECLS

#endif /* __CC_FORMAT_CHOOSER_H__ */
