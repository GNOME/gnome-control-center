/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2013 Red Hat, Inc,
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by:
 *   Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "um-avatar-picker.h"

#ifdef HAVE_CHEESE
#include <cheese-gtk.h>
#include <clutter-gtk/clutter-gtk.h>
#endif /* HAVE_CHEESE */

#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
    GtkWidget *picker;
    GdkPixbuf *pixbuf = NULL;
    int response;

#ifdef HAVE_CHEESE
    cheese_gtk_init (&argc, &argv);
    if (gtk_clutter_init (NULL, NULL) != CLUTTER_INIT_SUCCESS)
        g_error ("could not init clutter-gtk");
#endif /* HAVE_CHEESE */

    gtk_init (&argc, &argv);

    picker = um_avatar_picker_new ();
    response = gtk_dialog_run (GTK_DIALOG (picker));

    if (response == GTK_RESPONSE_ACCEPT) {
        g_print ("Selected an avatar\n");
        pixbuf = um_avatar_picker_get_avatar (UM_AVATAR_PICKER (picker));
        gdk_pixbuf_save (pixbuf, "picked-avatar.png", "png", NULL, NULL);
    } else if (response == GTK_RESPONSE_CANCEL) {
        g_print ("Cancelled\n");
    } else {
        g_warning ("Unexpected response %d\n", response);
        g_assert_not_reached ();
    }

    gtk_widget_destroy (GTK_WIDGET (picker));

    return 0;
}
