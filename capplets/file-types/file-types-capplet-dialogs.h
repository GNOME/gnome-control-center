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

#ifndef NAUTILUS_MIME_TYPE_CAPPLET_DIALOGS_H
#define NAUTILUS_MIME_TYPE_CAPPLET_DIALOGS_H

void 	show_edit_applications_dialog 				(const char 	*mime_type);
void	show_edit_components_dialog 				(const char 	*mime_type);
char   *name_from_oaf_server_info 				(OAF_ServerInfo *server);
char   *nautilus_mime_type_capplet_show_new_mime_window 	(void);
char   *nautilus_mime_type_capplet_show_new_extension_window 	(void);
char   *nautilus_mime_type_capplet_show_change_extension_window (const char 	*mime_type,
								 gboolean 	*new_list);

#endif /* NAUTILUS_MIME_TYPE_CAPPLET_DIALOGS_H */
