/*
 * Copyright (c) 2010 Intel, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#ifndef _CC_WINDOW_H
#define _CC_WINDOW_H

#include <glib-object.h>
#include "cc-shell.h"

G_BEGIN_DECLS

#define CC_TYPE_WINDOW (cc_window_get_type ())

G_DECLARE_FINAL_TYPE (CcWindow, cc_window, CC, WINDOW, GtkApplicationWindow)

CcWindow *cc_window_new (GtkApplication *application);

void cc_window_set_overview_page (CcWindow *center);

void cc_window_set_search_item (CcWindow *center,
                                const char *search);

G_END_DECLS

#endif /* _CC_WINDOW_H */
