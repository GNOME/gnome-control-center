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
 */

#pragma once

#include <gtk/gtk.h>

#include "cc-input-source.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

G_BEGIN_DECLS

GtkWidget     *cc_input_chooser_new              (GtkWindow    *parent,
                                                  gboolean      is_login,
                                                  GnomeXkbInfo *xkb_info,
                                                  GHashTable   *ibus_engines);

void           cc_input_chooser_set_ibus_engines (GtkWidget    *chooser,
                                                  GHashTable   *ibus_engines);

CcInputSource *cc_input_chooser_get_source       (GtkWidget    *chooser);

void           cc_input_chooser_reset            (GtkWidget    *chooser);

G_END_DECLS
