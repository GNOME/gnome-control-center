/* -*- mode: c; style: linux -*- */

/* accessibility-keyboard.c
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by: Jody Goldberg <jody@gnome.org>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <math.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"

#define IDIR GNOMECC_DATA_DIR "/pixmaps/"
#define CONFIG_ROOT "/desktop/gnome/accessibility/keyboard"

static struct {
	char const * const checkbox;
	char const * const image;
	char const * const image_file;
	char const * const gconf_key;
	char const * const content [3];
} const features [] = {
	{ "bouncekeys_enable", "bouncekeys_image", IDIR "accessibility-keyboard-bouncekey.png",
		CONFIG_ROOT "/bouncekeys_enable",
		{ "bouncekey_table", NULL, NULL } },
	{ "slowkeys_enable", "slowkeys_image", IDIR "accessibility-keyboard-slowkey.png",
		CONFIG_ROOT "/slowkeys_enable",
		{ "slowkeys_table", NULL, NULL } },
	{ "mousekeys_enable", "mousekeys_image", IDIR "accessibility-keyboard-mousekey.png",
		CONFIG_ROOT "/mousekeys_enable",
		{ "mousekeys_table", NULL, NULL } },
	{ "stickykeys_enable", "stickykeys_image", IDIR "accessibility-keyboard-stickykey.png",
		CONFIG_ROOT "/stickykeys_enable",
		{ "stickeykeys_table", NULL, NULL } },
	{ "togglekeys_enable", "togglekeys_image", IDIR "accessibility-keyboard-togglekey.png",
		CONFIG_ROOT "/togglekeys_enable",
		{ NULL, NULL, NULL } },
	{ "timeout_enable", NULL, NULL,
		CONFIG_ROOT "/timeout_enable",
		{ "timeout_slide", "timeout_spin", "timeout_label" } },
	{ "feature_state_change_beep", NULL, NULL,
		CONFIG_ROOT "/feature_state_change_beep",
		{ NULL, NULL, NULL } }
};

static struct {
	char const * const slide;
	char const * const spin;
	int default_val;
	int min_val;
	int max_val;
	int step_size;
	char const * const gconf_key;
} const ranges [] = {
	{ "bouncekeys_delay_slide",	"bouncekeys_delay_spin",     300,  10,  900,   10,
	  CONFIG_ROOT "/bouncekeys_delay" },
	{ "slowkeys_delay_slide",	"slowkeys_delay_spin",	     300,  10,  500,   10,
	  CONFIG_ROOT "/slowkeys_delay" },
	  /* WARNING anything larger than approx 512 seems to loose all keyboard input */
	{ "mousekeys_max_speed_slide",	"mousekeys_max_speed_spin",  300,  10, 500,   20,
	  CONFIG_ROOT "/mousekeys_max_speed" },
	{ "mousekeys_accel_time_slide",	"mousekeys_accel_time_spin", 300,  10, 3000,  100,
	  CONFIG_ROOT "/mousekeys_accel_time" },
	{ "mousekeys_init_delay_slide",	"mousekeys_init_delay_spin", 300,  10, 5000,  100,
	  CONFIG_ROOT "/mousekeys_init_delay" },
	{ "timeout_slide",	"timeout_spin",			     200,  10,  500,   10,
	  CONFIG_ROOT "/timeout" },
};

static void
set_sensitive (GladeXML *dialog, char const *name, gboolean state)
{
	if (name != NULL)
		gtk_widget_set_sensitive (WID (name), state);
}

/**
 * cb_feature_toggled :
 *
 * NOTE : for this to work the toggle MUST be initialized to active in the
 * glade file.  That way if the gconf setting is FALSE the toggle will fire.
 */
static void
cb_feature_toggled (GtkToggleButton *btn, gpointer feature_index)
{
	gboolean const state =
		(GTK_WIDGET_STATE (btn) != GTK_STATE_INSENSITIVE) &&
		gtk_toggle_button_get_active (btn);
	GladeXML *dialog = g_object_get_data (G_OBJECT (btn), "dialog");
	int feature, i;

	g_return_if_fail (dialog != NULL);

	feature = GPOINTER_TO_INT (feature_index);

	if (features [feature].image != NULL)
		set_sensitive (dialog, features [feature].image, state);
	for (i = G_N_ELEMENTS (features [feature].content) ; i-- > 0 ; )
		set_sensitive (dialog, features [feature].content [i], state);
}

static void
setup_toggles (GladeXML *dialog, GConfChangeSet *changeset)
{
	GObject *peditor;
	GtkWidget *checkbox;
	int i = G_N_ELEMENTS (features);

	while (i-- > 0) {
		checkbox = WID (features [i].checkbox);

		g_return_if_fail (checkbox != NULL);

		g_object_set_data (G_OBJECT (checkbox), "dialog", dialog);
		g_signal_connect (G_OBJECT (checkbox),
			"toggled",
			G_CALLBACK (cb_feature_toggled), GINT_TO_POINTER (i));
		peditor = gconf_peditor_new_boolean (changeset,
			(gchar *)features [i].gconf_key, checkbox, NULL);
	}
}

static void
setup_simple_toggles (GladeXML *dialog, GConfChangeSet *changeset)
{
	static struct {
		char const *gconf_key;
		char const *checkbox;
	} const simple_toggles [] = {
		{ CONFIG_ROOT "/bouncekeys_beep_reject",  "bouncekeys_beep_reject" },

		{ CONFIG_ROOT "/slowkeys_beep_press",      "slowkeys_beep_press" },
		{ CONFIG_ROOT "/slowkeys_beep_accept",     "slowkeys_beep_accept" },
		{ CONFIG_ROOT "/slowkeys_beep_reject",     "slowkeys_beep_reject" },

		{ CONFIG_ROOT "/stickykeys_two_key_off",   "stickykeys_two_key_off" },
		{ CONFIG_ROOT "/stickykeys_modifier_beep", "stickykeys_modifier_beep" },
	};
	int i = G_N_ELEMENTS (simple_toggles);
	while (i-- > 0) {
		GtkWidget *w = WID (simple_toggles [i].checkbox);

		g_return_if_fail (w != NULL);

		gconf_peditor_new_boolean (changeset,
			(gchar *) simple_toggles [i].gconf_key,
			w, NULL);
	}
}

static void
setup_ranges (GladeXML *dialog, GConfChangeSet *changeset)
{
	GObject *peditor;
	GtkWidget *slide, *spin;
	GtkAdjustment  *adjustment;
	int i = G_N_ELEMENTS (ranges);

	while (i-- > 0) {
		slide = WID (ranges [i].slide);
		spin  = WID (ranges [i].spin);
		g_return_if_fail (slide != NULL);
		g_return_if_fail (spin != NULL);

		adjustment = gtk_range_get_adjustment (GTK_RANGE (slide));

		g_return_if_fail (adjustment != NULL);

		adjustment->value = ranges [i].default_val;
		adjustment->lower = ranges [i].min_val;
		adjustment->upper = ranges [i].max_val + ranges [i].step_size;
		adjustment->step_increment = ranges [i].step_size;
		adjustment->page_increment = ranges [i].step_size;
		adjustment->page_size = ranges [i].step_size;

		gtk_adjustment_changed (adjustment);
		gtk_spin_button_configure (GTK_SPIN_BUTTON (spin), adjustment,
			ranges [i].step_size, 0);
		peditor = gconf_peditor_new_numeric_range (changeset,
			(gchar *)ranges [i].gconf_key, slide,
			 "conv-to-widget-cb",   gconf_value_int_to_float,
			 "conv-from-widget-cb", gconf_value_float_to_int,
			 NULL);
	}
}

static void
setup_images (GladeXML *dialog)
{
	int i = G_N_ELEMENTS (features);
	while (i-- > 0)
		if (features [i].image != NULL)
			gtk_image_set_from_file (GTK_IMAGE (WID (features [i].image)),
				features [i].image_file);
}

static void
cb_launch_keyboard_capplet (GtkButton *button, GtkWidget *dialog)
{
	GError *err = NULL;
	if (!g_spawn_command_line_async ("gnome-keyboard-properties", &err))
		capplet_error_dialog (GTK_WINDOW (gtk_widget_get_toplevel (dialog)),
			_("There was an error launching the keyboard capplet : %s"),
			err);
}

static void
cb_master_enable_toggle (GtkToggleButton *btn, GladeXML *dialog)
{
	int i = G_N_ELEMENTS (features);
	gboolean flag = gtk_toggle_button_get_active (btn);
	GtkWidget *w;

	while (i-- > 0) {
		w = WID (features [i].checkbox);
		gtk_widget_set_sensitive (w, flag);
		cb_feature_toggled (GTK_TOGGLE_BUTTON (w), GINT_TO_POINTER (i));
	}
}

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset)
{
	GtkWidget *master_enable = WID ("master_enable");
	setup_images (dialog);
	setup_ranges (dialog, changeset);
	setup_toggles (dialog, changeset);
	setup_simple_toggles (dialog, changeset);

	g_signal_connect (master_enable,
		"toggled",
		G_CALLBACK (cb_master_enable_toggle), dialog);
	gconf_peditor_new_boolean (changeset,
		CONFIG_ROOT "/enable",
		GTK_WIDGET (master_enable), NULL);
}

/*******************************************************************************/

static void
xrm_get_bool (GConfClient *client, XrmDatabase *db, char const *gconf_key,
	      char const *res_str, char const *class_str)
{
	XrmValue  resourceValue;
	char	 *res;

	if (XrmGetResource (*db, res_str, class_str, &res, &resourceValue))
		gconf_client_set_bool (client, gconf_key,
			!g_ascii_strcasecmp (resourceValue.addr, "True"), NULL);
}

static void
xrm_get_int (GConfClient *client, XrmDatabase *db, char const *gconf_key,
	     char const *res_str, char const *class_str, float scale)
{
	XrmValue	resourceValue;
	char		*res;
	int		value, log_scale;
	char		resource [256];

	snprintf (resource, sizeof (resource), "%s.value", res_str);
	if (!XrmGetResource (*db, resource, class_str, &res, &resourceValue))
		return;
	value = atoi (resourceValue.addr);

	snprintf (resource, sizeof (resource), "%s.decimalPoints", res_str);
	if (!XrmGetResource (*db, resource, class_str, &res, &resourceValue))
		return;
	log_scale = atoi (resourceValue.addr);

	while (log_scale-- > 0)
		scale /= 10.;

	gconf_client_set_int (client, gconf_key, value, NULL);

	printf ("%f * %d\n", scale, value);
}

/* This loads the current users XKB settings from their file */
static gboolean
load_CDE_file (GtkFileSelection *fsel)
{
	char const *file = gtk_file_selection_get_filename (fsel);
	GConfClient *client;
	XrmDatabase  db;

	if (!(db = XrmGetFileDatabase (file))) {
		GtkWidget *warn = gtk_message_dialog_new (
			gtk_window_get_transient_for (GTK_WINDOW (fsel)),
			GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
			_("Unable to import AccessX settings from file '%s'"),
			file);
		g_signal_connect (warn,
			"response",
			G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (warn);
		return FALSE;
	}

	client = gconf_client_get_default ();
	gconf_client_set_bool (client, CONFIG_ROOT "/enable", TRUE, NULL);

	xrm_get_bool (client, &db, CONFIG_ROOT "/feature_state_change_beep",
		"*SoundOnOffToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/timeout_enable",
		"*TimeOutToggle.set",		"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/stickykeys_enable",
		"*StickyKeysToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/mousekeys_enable",
		"*MouseKeysToggle.set",		"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/togglekeys_enable",
		"*ToggleKeysToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/slowkeys_enable",
		"*SlowKeysToggle.set",		"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/bouncekeys_enable",
		"*BounceKeysToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/stickykeys_modifier_beep",
		"*StickyModSoundToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/stickykeys_two_key_off",
		"*StickyTwoKeysToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/slowkeys_beep_press",
		"*SlowKeysOnPressToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_bool (client, &db, CONFIG_ROOT "/slowkeys_beep_accept",
		"*SlowKeysOnAcceptToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
	xrm_get_int  (client, &db, CONFIG_ROOT "/timeout",
		"*TimeOutScale",		"AccessX*XmScale", 60);
	xrm_get_int  (client, &db, CONFIG_ROOT "/mousekeys_max_speed",
		"*MouseMaxSpeedScale",		"AccessX*XmScale", 1);
	xrm_get_int  (client, &db, CONFIG_ROOT "/mousekeys_accel_time",
		"*MouseAccelScale",		"AccessX*XmScale", 1);
	xrm_get_int  (client, &db, CONFIG_ROOT "/mousekeys_init_delay",
		"*MouseDelayScale",		"AccessX*XmScale", 1);
	xrm_get_int  (client, &db, CONFIG_ROOT "/slowkeys_delay",
		"*KRGSlowKeysDelayScale",	"AccessX*XmScale", 1000);
	xrm_get_int  (client, &db, CONFIG_ROOT "/bouncekeys_delay",
		"*KRGDebounceScale",		"AccessX*XmScale", 1000);

	/* Set the master enable flag last */
	xrm_get_bool (client, &db, CONFIG_ROOT "/enable",
		"*EnableAccessXToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");

	return TRUE;
}

static void
fsel_dialog_finish (GtkWidget *fsel)
{
	gtk_widget_hide_all (fsel);
}

static void
fsel_handle_ok (GtkWidget *widget, GtkFileSelection *fsel)
{
	gchar const *file_name;

	file_name = gtk_file_selection_get_filename (fsel);

	/* Change into directory if that's what user selected */
	if (g_file_test (file_name, G_FILE_TEST_IS_DIR)) {
		gint name_len;
		gchar *dir_name;

		name_len = strlen (file_name);
		if (name_len < 1 || file_name [name_len - 1] != '/') {
			/* The file selector needs a '/' at the end of a directory name */
			dir_name = g_strconcat (file_name, "/", NULL);
		} else {
			dir_name = g_strdup (file_name);
		}
		gtk_file_selection_set_filename (fsel, dir_name);
		g_free (dir_name);
	} else if (load_CDE_file (fsel))
		fsel_dialog_finish (GTK_WIDGET (fsel));
}

static void
fsel_handle_cancel (GtkWidget *widget, GtkFileSelection *fsel)
{
	fsel_dialog_finish (GTK_WIDGET (fsel));
}

static gint
fsel_delete_event (GtkWidget *fsel, GdkEventAny *event)
{
	fsel_dialog_finish (fsel);
	return TRUE;
}

static gint
fsel_key_event (GtkWidget *fsel, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_Escape) {
		gtk_signal_emit_stop_by_name (GTK_OBJECT (fsel), "key_press_event");
		fsel_dialog_finish (fsel);
		return TRUE;
	}

	return FALSE;
}

static GtkFileSelection *fsel = NULL;
static void
cb_load_CDE_file (GtkButton *button, GtkWidget *dialog)
{
	if (fsel == NULL) {
		fsel = GTK_FILE_SELECTION (
			gtk_file_selection_new (_("Select CDE AccessX file")));

		gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (fsel));
		gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (fsel), FALSE);

		gtk_window_set_modal (GTK_WINDOW (fsel), TRUE);
		gtk_window_set_transient_for (GTK_WINDOW (fsel),
			GTK_WINDOW (gtk_widget_get_toplevel (dialog)));
		g_signal_connect (G_OBJECT (fsel->ok_button),
			"clicked",
			G_CALLBACK (fsel_handle_ok), fsel);
		g_signal_connect (G_OBJECT (fsel->cancel_button),
			"clicked",
			G_CALLBACK (fsel_handle_cancel), fsel);
		g_signal_connect (G_OBJECT (fsel),
			"key_press_event",
			G_CALLBACK (fsel_key_event), NULL);
		g_signal_connect (G_OBJECT (fsel),
			"delete_event",
			G_CALLBACK (fsel_delete_event), NULL);
	}
	gtk_widget_show_all (GTK_WIDGET (fsel));
}

/*******************************************************************************/

GtkWidget *
setup_accessX_dialog (GConfChangeSet *changeset)
{
	GConfClient *client;
	char const  *toplevel_name = "accessX_dialog";
	GladeXML    *dialog = glade_xml_new (GNOMECC_DATA_DIR
		"/interfaces/gnome-accessibility-keyboard-properties.glade",
		toplevel_name, NULL);
	GtkWidget *toplevel = WID (toplevel_name);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, CONFIG_ROOT, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	setup_dialog (dialog, changeset);

	g_signal_connect (G_OBJECT (WID ("load_CDE_file")),
		"clicked",
		G_CALLBACK (cb_load_CDE_file), toplevel);
	g_signal_connect (G_OBJECT (WID ("launch_keyboard_capplet")),
		"clicked",
		G_CALLBACK (cb_launch_keyboard_capplet), toplevel);

	return toplevel;
}
