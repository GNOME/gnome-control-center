/* -*- mode: c; style: linux -*- */
/* -*- c-basic-offset: 2 -*- */

/* sound-theme-file-utils.h
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
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
 */
#ifndef __SOUND_THEME_FILE_UTILSHH__
#define __SOUND_THEME_HH__

#include <gio/gio.h>

char *custom_theme_dir_path (const char *child);

void delete_custom_theme_dir (void);
void delete_old_files (char **sounds);
void delete_disabled_files (char **sounds);

void add_disabled_file (char **sounds);
void add_custom_file (char **sounds, const char *filename);

#endif /* __SOUND_THEME_FILE_UTILS_HH__ */
