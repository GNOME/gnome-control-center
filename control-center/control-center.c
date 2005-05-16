/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>

#include "control-center-categories.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include "gnomecc-canvas.h"

static void
gnome_cc_die (void)
{
        gtk_main_quit ();
}

static void
change_status (GnomeccCanvas *canvas, const gchar *status, void *data)
{
	GnomeAppBar *bar = data;

	if (!status)
		status = "";

	gnome_appbar_set_status (bar, status);
}

static GtkWindow *
create_window (void)
{
	GtkWidget *window, *appbar, *sw, *canvas;
	ControlCenterInformation *info;
	GnomeClient *client;

	client = gnome_master_client ();
	g_signal_connect (G_OBJECT (client), "die",
			  G_CALLBACK (gnome_cc_die), NULL);

	info = control_center_get_information ();
	window = gnome_app_new ("gnomecc", _("Desktop Preferences"));
	gtk_window_set_icon_name (GTK_WINDOW (window), "gnome-control-center");
	gtk_window_set_default_size (GTK_WINDOW (window), 760, 530);

	appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar (GNOME_APP (window), appbar);

	canvas = gnomecc_canvas_new (info);
	g_signal_connect (G_OBJECT (canvas), "selection-changed",
			  G_CALLBACK (change_status), appbar);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), canvas);

	gnome_app_set_contents (GNOME_APP (window), sw);

	gtk_widget_show_all (window);

	g_object_weak_ref (G_OBJECT (window), (GWeakNotify) gnome_cc_die, NULL);

	return GTK_WINDOW (window);
}

int
main (int argc, char *argv[])
{
	GnomeProgram *ccprogram;

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	ccprogram = gnome_program_init ("gnome-control-center",
			    VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);
	create_window ();

	gtk_main ();

	return 0;
}
