/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nautilus-mime-type-capplet.h
 *
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
 * Author: Gene Z. Ragan <gzr@eazel.com>
 */

#ifndef NAUTILUS_MIME_TYPE_CAPPLET_H
#define NAUTILUS_MIME_TYPE_CAPPLET_H

void 	    nautilus_mime_type_capplet_update_info 		             (const char *mime_type);
void 	    nautilus_mime_type_capplet_update_application_info 	             (const char *mime_type);
void 	    nautilus_mime_type_capplet_update_viewer_info   	             (const char *mime_type);
void 	    nautilus_mime_type_capplet_add_extension 		             (const char *extension);
const char *nautilus_mime_type_capplet_get_selected_item_mime_type 	     (void);
void	    nautilus_mime_type_capplet_update_mime_list_icon_and_description (const char *mime_string);

#endif /* NAUTILUS_MIME_TYPE_CAPPLET_H */
