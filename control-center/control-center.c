/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>

#include "control-center-categories.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <libgnome/libgnome.h>
#include <libgnomeui/libgnomeui.h>
#include "gnomecc-canvas.h"

#define SINGLE_CLICK_POLICY_KEY "/apps/nautilus/preferences/click_policy"

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

static void
on_click_policy_notified (GConfClient   *client,
			  guint          conn_id,
			  GConfEntry    *entry,
			  GnomeccCanvas *canvas)
{
	GConfValue *value;
	gboolean use_single_click = FALSE;
	gchar *policy;

	value = gconf_entry_get_value (entry);

	if (value->type == GCONF_VALUE_STRING) {
		policy = gconf_value_get_string (value);
		use_single_click = (0 == g_ascii_strcasecmp (policy, "single"));

		gnomecc_canvas_set_single_click_mode (canvas, use_single_click);
	}
}

static void
set_click_policy (GnomeccCanvas *canvas)
{
	GConfClient *client = gconf_client_get_default ();
	gboolean use_single_click = FALSE;
	gchar *policy;

	gconf_client_add_dir (client,
                              SINGLE_CLICK_POLICY_KEY,
                              GCONF_CLIENT_PRELOAD_NONE,
                              NULL);

	gconf_client_notify_add (client, 
				 SINGLE_CLICK_POLICY_KEY,
				 on_click_policy_notified,
				 canvas,
				 NULL, NULL);

	policy = gconf_client_get_string (client, SINGLE_CLICK_POLICY_KEY, NULL);

	if (policy) {
		use_single_click = (0 == g_ascii_strcasecmp (policy, "single"));
		g_free (policy);
	}

	gnomecc_canvas_set_single_click_mode (canvas, use_single_click);
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
	set_click_policy (GNOMECC_CANVAS (canvas));

	sw = gtk_scrolled_window_new (GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 10, 100, 100)),
				      GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 10, 100, 100)));

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), canvas);

	gnome_app_set_contents (GNOME_APP (window), sw);

	gtk_widget_show_all (window);

	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sw),
					     GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 10, 100, 100)));

	g_object_weak_ref (G_OBJECT (window), (GWeakNotify) gnome_cc_die, NULL);

	return GTK_WINDOW (window);
}

int
main (int argc, char *argv[])
{
        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-control-center",
			    VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);
	create_window ();

	gtk_main ();

	return 0;
}
