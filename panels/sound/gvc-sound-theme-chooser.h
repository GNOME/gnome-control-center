/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
 */

#ifndef __GVC_SOUND_THEME_CHOOSER_H
#define __GVC_SOUND_THEME_CHOOSER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GVC_TYPE_SOUND_THEME_CHOOSER (gvc_sound_theme_chooser_get_type ())
G_DECLARE_FINAL_TYPE (GvcSoundThemeChooser, gvc_sound_theme_chooser, GVC, SOUND_THEME_CHOOSER, GtkBox)

GtkWidget *         gvc_sound_theme_chooser_new                 (void);

G_END_DECLS

#endif /* __GVC_SOUND_THEME_CHOOSER_H */
