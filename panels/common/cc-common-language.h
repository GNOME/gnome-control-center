/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __CC_COMMON_LANGUAGE_H__
#define __CC_COMMON_LANGUAGE_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#if 0
void              um_add_user_languages       (GtkTreeModel     *model);
gchar            *um_get_current_language     (void);

GtkWidget        *um_language_chooser_new          (void);
gchar            *um_language_chooser_get_language (GtkWidget *chooser);
#endif

gint cc_common_language_sort_languages (GtkTreeModel *model,
					GtkTreeIter  *a,
					GtkTreeIter  *b,
					gpointer      data);
gboolean cc_common_language_get_iter_for_language (GtkTreeModel     *model,
						   const gchar      *lang,
						   GtkTreeIter      *iter);


G_END_DECLS

#endif
