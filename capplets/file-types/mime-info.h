/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* mime-info.h
 *
 * Copyright (C) 1998 Redhat Software Inc. 
 * Copyright (C) 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: 	Jonathan Blandford <jrb@redhat.com>
 * 		Gene Z. Ragan <gzr@eazel.com>
 */

#ifndef MIME_INFO_H
#define MIME_INFO_H

#include "gnome.h"
#include <regex.h>

void init_mime_info (void);
void discard_key_info (void);
void set_mime_key_value (gchar *mime_type, gchar *key, gchar *value);
const char * local_mime_get_value (const char *mime_type, char *key);
void write_user_keys (void);
void write_initial_keys (void);
void remove_mime_info (gchar *mime_type);

#endif
