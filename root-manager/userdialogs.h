/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __USERDIALOGS_H__
#define __USERDIALOGS_H__

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <locale.h>
#include <libintl.h>
#define i18n(String) gettext(String)
#define N_(String) String

#define UD_OK_TEXT N_("OK")
#define UD_HELP_TEXT N_("Help")
#define UD_CANCEL_TEXT N_("Cancel")
#define UD_EXIT_TEXT N_("Exit")
#define UD_FALLBACK_TEXT N_("Run Unprivileged")

/* consider a "has args" arg, so I can use the arg argument or not at will */
GtkWidget* create_message_box(gchar* message, gchar* title);
GtkWidget* create_error_box(gchar* error, gchar* title);
GtkWidget* create_query_box(gchar* prompt, gchar* title, GtkSignalFunc func);
GtkWidget* create_invisible_query_box(gchar* prompt, gchar* title,
				      GtkSignalFunc func); 

#endif /* __USERDIALOGS_H__ */
