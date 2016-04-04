/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2015  Red Hat, Inc,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <feborges@redhat.com>
 */

#ifndef _CC_MOUSE_CAPS_HELPER_H_
#define _CC_MOUSE_CAPS_HELPER_H_

#include <glib.h>

gboolean cc_touchpad_check_capabilities (gboolean *have_two_finger_scrolling,
                                         gboolean *have_edge_scrolling,
                                         gboolean *have_tap_to_click);

gboolean cc_synaptics_check (void);

#endif /* _CC_MOUSE_CAPS_HELPER_H_ */
