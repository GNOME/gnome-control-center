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
#include <X11/Xresource.h>
#include <math.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"

#define IDIR GNOMECC_DATA_DIR "/pixmaps/"
#define CONFIG_ROOT "/desktop/gnome/accesibility/keyboard"

static struct {
	char const * const checkbox;
	char const * const image;
	char const * const image_file;
	char const * const gconf_key;
	gboolean only_for_dialog;
	char const * const content [5];
} const features [] = {
	{ "bouncekeys_enable", "bouncekeys_image", IDIR "accessibility-keyboard-bouncekey.png",
		CONFIG_ROOT "/bouncekeys_enable", FALSE,
		{ "bouncekeys_delay_slide", "bouncekeys_delay_spin", "bouncekeys_label1", "bouncekeys_label2", "bouncekeys_box" } },
	{ "slowkeys_enable", "slowkeys_image", IDIR "accessibility-keyboard-slowkey.png",
		CONFIG_ROOT "/slowkeys_enable", FALSE,
		{ "slowkeys_delay_slide", "slowkeys_delay_spin", "slowkeys_table", "slowkeys_label", NULL } },
	{ "mousekeys_enable", "mousekeys_image", IDIR "accessibility-keyboard-mousekey.png",
		CONFIG_ROOT "/mousekeys_enable", FALSE,
		{ "mousekeys_table", NULL, NULL, NULL, NULL } },
	{ "stickykeys_enable", "stickykeys_image", IDIR "accessibility-keyboard-stickykey.png",
		CONFIG_ROOT "/stickykeys_enable", FALSE,
		{ "stickykeys_two_key_off", "stickykeys_modifier_beep", NULL, NULL, NULL } },
	{ "togglekeys_enable", "togglekeys_image", IDIR "accessibility-keyboard-togglekey.png",
		CONFIG_ROOT "/togglekeys_enable", FALSE,
		{ NULL, NULL, NULL, NULL, NULL } },
	{ "timeout_enable", NULL, NULL,
		CONFIG_ROOT "/timeout_enable", TRUE,
		{ "timeout_slide", "timeout_spin", NULL, NULL, NULL } }
};

static struct {
	char const * const slide;
	char const * const spin;
	int default_val;
	int min_val;
	int max_val;
	int step_size;
	char const * const gconf_key;
	gboolean only_for_dialog;
} const ranges [] = {
	{ "bouncekeys_delay_slide",	"bouncekeys_delay_spin",	    300,  10, 900,  10,
	  CONFIG_ROOT "/bouncekeys_delay", FALSE },
	{ "slowkeys_delay_slide",	"slowkeys_delay_spin",	    300,  10, 900,  10,
	  CONFIG_ROOT "/slowkeys_delay", FALSE },
	{ "mousekeys_max_speed_slide",	"mousekeys_max_speed_spin",   70,  10, 500,  10,
	  CONFIG_ROOT "/mousekeys_max_speed", FALSE },
	{ "mousekeys_accel_time_slide",	"mousekeys_accel_time_spin", 300,  10, 900,  10,
	  CONFIG_ROOT "/mousekeys_accel_time", FALSE },
	{ "mousekeys_init_delay_slide",	"mousekeys_init_delay_spin", 200,  10, 500,  10,
	  CONFIG_ROOT "/mousekeys_init_delay", FALSE },
	{ "timeout_slide",	"timeout_spin", 200,  10, 500,  10,
	  CONFIG_ROOT "/timeout", TRUE },
};

static void
cb_accessibility_toggled (GtkToggleButton *btn, GtkWidget *table)
{
	gtk_widget_set_sensitive (table, gtk_toggle_button_get_active (btn));
}

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
	gboolean const state = gtk_toggle_button_get_active (btn);
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
setup_toggles (GladeXML *dialog, GConfChangeSet *changeset, gboolean as_dialog)
{
	GObject *peditor;
	GtkWidget *checkbox;
	int i = G_N_ELEMENTS (features);

	while (i-- > 0)
		if (as_dialog || !features [i].only_for_dialog) {
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
setup_simple_toggles (GladeXML *dialog, GConfChangeSet *changeset, gboolean as_dialog)
{
	static struct {
		char const *gconf_key;
		char const *checkbox;
		gboolean only_for_dialog;
	} const simple_toggles [] = {
		{ CONFIG_ROOT "/feature_state_change_beep","feature_state_change_beep", TRUE },
		{ CONFIG_ROOT "/bouncekeys_beep_reject",  "bouncekeys_beep_reject",	FALSE },

		{ CONFIG_ROOT "/slowkeys_beep_press",      "slowkeys_beep_press",	FALSE },
		{ CONFIG_ROOT "/slowkeys_beep_accept",     "slowkeys_beep_accept",	FALSE },
		{ CONFIG_ROOT "/slowkeys_beep_reject",     "slowkeys_beep_reject",	FALSE},

		{ CONFIG_ROOT "/stickykeys_two_key_off",   "stickykeys_two_key_off",	FALSE },
		{ CONFIG_ROOT "/stickykeys_modifier_beep", "stickykeys_modifier_beep",	FALSE},
	};
	int i = G_N_ELEMENTS (simple_toggles);
	while (i-- > 0)
		if (as_dialog || !simple_toggles [i].only_for_dialog) {
			GtkWidget *w = WID (simple_toggles [i].checkbox);

			g_return_if_fail (w != NULL);

			gconf_peditor_new_boolean (changeset,
				(gchar *) simple_toggles [i].gconf_key,
				w, NULL);
		}
}

static GConfValue*
cb_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	return gconf_value_int_to_float (value);
}
static GConfValue*
cb_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	return gconf_value_float_to_int (value);
}

static void
setup_ranges (GladeXML *dialog, GConfChangeSet *changeset, gboolean as_dialog)
{
	GObject *peditor;
	GtkWidget *slide, *spin;
	GtkAdjustment  *adjustment;
	int i = G_N_ELEMENTS (ranges);

	while (i-- > 0) {
		if (!as_dialog && ranges [i].only_for_dialog)
			continue;

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
			 "conv-to-widget-cb",	cb_to_widget,
			 "conv-from-widget-cb", cb_from_widget,
			 NULL);
	}
}

static void
setup_images (GladeXML *dialog, gboolean as_dialog)
{
	int i = G_N_ELEMENTS (features);
	while (i-- > 0)
		if (features [i].image != NULL &&
		    (as_dialog || !features [i].only_for_dialog))
			gtk_image_set_from_file (GTK_IMAGE (WID (features [i].image)),
				features [i].image_file);
}

static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset, gboolean as_dialog)
{
	GtkWidget *content = WID ("keyboard_table");
	GtkWidget *page = WID ("accessX_page");
	GObject *label;

	g_return_if_fail (content != NULL);
	g_return_if_fail (page != NULL);

	setup_images (dialog, as_dialog);
	setup_ranges (dialog, changeset, as_dialog);
	setup_toggles (dialog, changeset, as_dialog);
	setup_simple_toggles (dialog, changeset, as_dialog);

	label = g_object_new (GTK_TYPE_CHECK_BUTTON,
		"label",	 _("_Enable keyboard accesibility"),
		"use_underline", TRUE,
		/* init true so that if gconf is false toggle will fire */
		"active",	 TRUE,
		NULL);
	gtk_frame_set_label_widget (GTK_FRAME (page), GTK_WIDGET (label));
	g_signal_connect (label,
		"toggled",
		G_CALLBACK (cb_accessibility_toggled), content);
	gconf_peditor_new_boolean (changeset,
		CONFIG_ROOT "/enable",
		GTK_WIDGET (label), NULL);
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
	     char const *res_str, char const *class_str)
{
	XrmValue	resourceValue;
	char		*res;
	int		value;
	char		resource [256];

	snprintf (resource, sizeof (resource), "%s.value", res_str);
	if (!XrmGetResource (*db, resource, class_str, &res, &resourceValue))
		return;
	value = atoi (resourceValue.addr);

#if 0
	{
	int decimal;
	snprintf (resource, sizeof (resource), "%s.decimalPoints", res_str);
	if (!XrmGetResource (*db, resource, class_str, &res, &resourceValue))
		return;
	decimal = atoi (resourceValue.addr);
	}
#endif

	gconf_client_set_int (client, gconf_key, value, NULL);
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
	xrm_get_bool (client, &db, CONFIG_ROOT "/enable",
		"*EnableAccessXToggle.set",	"AccessX*ToggleButtonGadget.XmCSet");
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
		"*TimeOutScale",		"AccessX*XmScale");
	xrm_get_int  (client, &db, CONFIG_ROOT "/mousekeys_max_speed",
		"*MouseMaxSpeedScale",		"AccessX*XmScale");
	xrm_get_int  (client, &db, CONFIG_ROOT "/mousekeys_accel_time",
		"*MouseAccelScale",		"AccessX*XmScale");
	xrm_get_int  (client, &db, CONFIG_ROOT "/mousekeys_init_delay",
		"*MouseDelayScale",		"AccessX*XmScale");
	xrm_get_int  (client, &db, CONFIG_ROOT "/slowkeys_delay",
		"*KRGSlowKeysDelayScale",	"AccessX*XmScale");
	xrm_get_int  (client, &db, CONFIG_ROOT "/bouncekeys_delay",
		"*KRGDebounceScale",		"AccessX*XmScale");

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
setup_accessX_dialog (GConfChangeSet *changeset, gboolean as_dialog)
{
	GConfClient *client;
	char const  *toplevel_name = as_dialog ? "accessX_dialog" : "accessX_page";
	GladeXML    *dialog = glade_xml_new (GNOMECC_DATA_DIR
		"/interfaces/gnome-accessibility-keyboard-properties.glade2",
		toplevel_name, NULL);
	GtkWidget   *toplevel = WID (toplevel_name);

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, CONFIG_ROOT, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	setup_dialog (dialog, changeset, as_dialog);

	if (as_dialog) {
		GtkWidget *load_cde = WID ("load_CDE_file");
		g_signal_connect (G_OBJECT (load_cde),
			"activate",
			G_CALLBACK (cb_load_CDE_file), toplevel);
	}
	return toplevel;
}
