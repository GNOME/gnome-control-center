/*
 * Copyright (C) 2013, 2015 Red Hat, Inc
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

#ifndef __CC_INPUT_OPTIONS_H__
#define __CC_INPUT_OPTIONS_H__

#include <gtk/gtk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CC_TYPE_INPUT_OPTIONS (cc_input_options_get_type ())
G_DECLARE_FINAL_TYPE (CcInputOptions, cc_input_options, CC, INPUT_OPTIONS, GtkDialog)

GtkWidget *cc_input_options_new (GtkWidget *parent);

G_END_DECLS

#endif /* __CC_FORMAT_CHOOSER_H__ */
