/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
  *
  * Copyright (C) 2012 Red Hat, Inc.
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
  * along with this program; if not, write to the Free Software
  * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  */

#include "cc-add-user-dialog.h"

#include <gtk/gtk.h>

static void
on_dialog_complete (GObject *object,
                    GAsyncResult *result,
                    gpointer user_data)
{
	GMainLoop *loop = user_data;
	ActUser *user;

	user = cc_add_user_dialog_finish (CC_ADD_USER_DIALOG (object), result);
	if (user == NULL) {
		g_printerr ("No user created\n");
	} else {
		g_printerr ("User created: %s\n", act_user_get_user_name (user));
		g_object_unref (user);
	}

	g_main_loop_quit (loop);
}

int
main (int argc,
      char *argv[])
{
	CcAddUserDialog *dialog;
	GMainLoop *loop;

	gtk_init (&argc, &argv);

	dialog = cc_add_user_dialog_new ();
	loop = g_main_loop_new (NULL, FALSE);

	cc_add_user_dialog_show (dialog, NULL, NULL, on_dialog_complete, loop);

	g_main_loop_run (loop);
	g_main_loop_unref (loop);

	return 0;
}
