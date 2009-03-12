/* gnome-about-me-fingerprint.h
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <dbus/dbus-glib-bindings.h>

#include "fingerprint-strings.h"
#include "capplet-util.h"

/* This must match the number of images on the 2nd page in the glade file */
#define MAX_ENROLL_STAGES 3

static DBusGProxy *manager = NULL;
static DBusGConnection *connection = NULL;
static gboolean is_disable = FALSE;

enum {
	STATE_NONE,
	STATE_CLAIMED,
	STATE_ENROLLING
};

typedef struct {
	GtkWidget *enable;
	GtkWidget *disable;

	GtkWidget *ass;
	GladeXML *dialog_page1;
	GladeXML *dialog_page2;

	DBusGProxy *device;
	gboolean is_swipe;
	int num_enroll_stages;
	int num_stages_done;
	char *name;
	const char *finger;
	gint state;
} EnrollData;

static void create_manager (void)
{
	GError *error = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (connection == NULL) {
		g_warning ("Failed to connect to session bus: %s", error->message);
		return;
	}

	manager = dbus_g_proxy_new_for_name (connection,
					     "net.reactivated.Fprint",
					     "/net/reactivated/Fprint/Manager",
					     "net.reactivated.Fprint.Manager");
}

static DBusGProxy *
get_first_device (void)
{
	DBusGProxy *device;
	char *device_str;

	if (!dbus_g_proxy_call (manager, "GetDefaultDevice", NULL, G_TYPE_INVALID,
				DBUS_TYPE_G_OBJECT_PATH, &device_str, G_TYPE_INVALID)) {
		return NULL;
	}

	device = dbus_g_proxy_new_for_name(connection,
					   "net.reactivated.Fprint",
					   device_str,
					   "net.reactivated.Fprint.Device");

	g_free (device_str);

	return device;
}

static const char *
get_reason_for_error (const char *dbus_error)
{
	if (g_str_equal (dbus_error, "net.reactivated.Fprint.Error.PermissionDenied"))
		return N_("You are not allowed to access the device. Contact your system administrator.");
	if (g_str_equal (dbus_error, "net.reactivated.Fprint.Error.AlreadyInUse"))
		return N_("The device is already in use.");
	if (g_str_equal (dbus_error, "net.reactivated.Fprint.Error.Internal"))
		return N_("An internal error occured");

	return NULL;
}

static GtkWidget *
get_error_dialog (const char *title,
		  const char *dbus_error,
		  GtkWindow *parent)
{
	GtkWidget *error_dialog;
	const char *reason;

	if (dbus_error == NULL)
		g_warning ("get_error_dialog called with reason == NULL");

	error_dialog =
		gtk_message_dialog_new (parent,
				GTK_DIALOG_MODAL,
				GTK_MESSAGE_ERROR,
				GTK_BUTTONS_OK,
				"%s", title);
	reason = get_reason_for_error (dbus_error);
	gtk_message_dialog_format_secondary_text
		(GTK_MESSAGE_DIALOG (error_dialog), "%s", reason ? _(reason) : _(dbus_error));

	gtk_window_set_title (GTK_WINDOW (error_dialog), ""); /* as per HIG */
	gtk_container_set_border_width (GTK_CONTAINER (error_dialog), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
					 GTK_RESPONSE_OK);
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (error_dialog), GTK_WIN_POS_CENTER_ON_PARENT);

	return error_dialog;
}

void
set_fingerprint_label (GtkWidget *enable, GtkWidget *disable)
{
	char **fingers;
	DBusGProxy *device;
	GError *error = NULL;

	gtk_widget_set_no_show_all (enable, TRUE);
	gtk_widget_set_no_show_all (disable, TRUE);

	if (manager == NULL) {
		create_manager ();
		if (manager == NULL) {
			gtk_widget_hide (enable);
			gtk_widget_hide (disable);
			return;
		}
	}

	device = get_first_device ();
	if (device == NULL) {
		gtk_widget_hide (enable);
		gtk_widget_hide (disable);
		return;
	}

	if (!dbus_g_proxy_call (device, "ListEnrolledFingers", &error, G_TYPE_STRING, "", G_TYPE_INVALID,
				G_TYPE_STRV, &fingers, G_TYPE_INVALID)) {
		if (dbus_g_error_has_name (error, "net.reactivated.Fprint.Error.NoEnrolledPrints") == FALSE) {
			gtk_widget_hide (enable);
			gtk_widget_hide (disable);
			g_object_unref (device);
			return;
		}
		fingers = NULL;
	}

	if (fingers == NULL || g_strv_length (fingers) == 0) {
		gtk_widget_hide (disable);
		gtk_widget_show (enable);
		is_disable = FALSE;
	} else {
		gtk_widget_hide (enable);
		gtk_widget_show (disable);
		is_disable = TRUE;
	}

	g_strfreev (fingers);
	g_object_unref (device);
}

static void
delete_fingerprints (void)
{
	DBusGProxy *device;

	if (manager == NULL) {
		create_manager ();
		if (manager == NULL)
			return;
	}

	device = get_first_device ();
	if (device == NULL)
		return;

	dbus_g_proxy_call (device, "DeleteEnrolledFingers", NULL, G_TYPE_STRING, "", G_TYPE_INVALID, G_TYPE_INVALID);

	g_object_unref (device);
}

static void
delete_fingerprints_question (GladeXML *dialog, GtkWidget *enable, GtkWidget *disable)
{
	GtkWidget *question;
	GtkWidget *button;

	question = gtk_message_dialog_new (GTK_WINDOW (WID ("about-me-dialog")),
					   GTK_DIALOG_MODAL,
					   GTK_MESSAGE_QUESTION,
					   GTK_BUTTONS_NONE,
					   _("Delete registered fingerprints?"));
	gtk_dialog_add_button (GTK_DIALOG (question), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	button = gtk_button_new_with_mnemonic (_("_Delete Fingerprints"));
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_DELETE, GTK_ICON_SIZE_BUTTON));
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (question), button, GTK_RESPONSE_OK);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (question),
						  _("Do you want to delete your registered fingerprints so fingerprint login is disabled?"));
	gtk_container_set_border_width (GTK_CONTAINER (question), 5);
	gtk_dialog_set_default_response (GTK_DIALOG (question), GTK_RESPONSE_OK);
	gtk_window_set_position (GTK_WINDOW (question), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_modal (GTK_WINDOW (question), TRUE);

	if (gtk_dialog_run (GTK_DIALOG (question)) == GTK_RESPONSE_OK) {
		delete_fingerprints ();
		set_fingerprint_label (enable, disable);
	}

	gtk_widget_destroy (question);
}

static void
enroll_data_destroy (EnrollData *data)
{
	switch (data->state) {
	case STATE_ENROLLING:
		dbus_g_proxy_call(data->device, "EnrollStop", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
		/* fall-through */
	case STATE_CLAIMED:
		dbus_g_proxy_call(data->device, "Release", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
		/* fall-through */
	case STATE_NONE:
		g_free (data->name);
		g_object_unref (data->device);
		g_object_unref (data->dialog_page1);
		g_object_unref (data->dialog_page2);
		gtk_widget_destroy (data->ass);

		g_free (data);
	}
}

static const char *
selected_finger (GladeXML *dialog)
{
	int index;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("radiobutton1")))) {
		gtk_widget_set_sensitive (WID ("finger_combobox"), FALSE);
		return "right-index-finger";
	}
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (WID ("radiobutton2")))) {
		gtk_widget_set_sensitive (WID ("finger_combobox"), FALSE);
		return "left-index-finger";
	}
	gtk_widget_set_sensitive (WID ("finger_combobox"), TRUE);
	index = gtk_combo_box_get_active (GTK_COMBO_BOX (WID ("finger_combobox")));
	switch (index) {
	case 0:
		return "left-thumb";
	case 1:
		return "left-middle-finger";
	case 2:
		return "left-ring-finger";
	case 3:
		return "left-little-finger";
	case 4:
		return "right-thumb";
	case 5:
		return "right-middle-finger";
	case 6:
		return "right-ring-finger";
	case 7:
		return "right-little-finger";
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

static void
finger_radio_button_toggled (GtkToggleButton *button, EnrollData *data)
{
	data->finger = selected_finger (data->dialog_page1);
}

static void
finger_combobox_changed (GtkComboBox *combobox, EnrollData *data)
{
	data->finger = selected_finger (data->dialog_page1);
}

static void
assistant_cancelled (GtkAssistant *ass, EnrollData *data)
{
	GtkWidget *enable, *disable;

	enable = data->enable;
	disable = data->disable;

	enroll_data_destroy (data);
	set_fingerprint_label (enable, disable);
}

static void
enroll_result (GObject *object, const char *result, gboolean done, EnrollData *data)
{
	GladeXML *dialog = data->dialog_page2;
	char *msg;

	if (g_str_equal (result, "enroll-completed") || g_str_equal (result, "enroll-stage-passed")) {
		char *name;

		data->num_stages_done++;
		name = g_strdup_printf ("image%d", data->num_stages_done);
		gtk_image_set_from_stock (GTK_IMAGE (WID (name)), GTK_STOCK_YES, GTK_ICON_SIZE_DIALOG);
		g_free (name);
	}
	if (g_str_equal (result, "enroll-completed")) {
		gtk_label_set_text (GTK_LABEL (WID ("status-label")), _("Done!"));
		gtk_assistant_set_page_complete (GTK_ASSISTANT (data->ass), WID ("page2"), TRUE);
	}

	if (done != FALSE) {
		dbus_g_proxy_call(data->device, "EnrollStop", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
		data->state = STATE_CLAIMED;
		if (g_str_equal (result, "enroll-completed") == FALSE) {
			/* The enrollment failed, restart it */
			dbus_g_proxy_call(data->device, "EnrollStart", NULL, G_TYPE_STRING, data->finger, G_TYPE_INVALID, G_TYPE_INVALID);
			data->state = STATE_ENROLLING;
			result = "enroll-retry-scan";
		} else {
			return;
		}
	}

	msg = g_strdup_printf (enroll_result_str_to_msg (result, data->is_swipe), data->name);
	gtk_label_set_text (GTK_LABEL (WID ("status-label")), msg);
	g_free (msg);
}

static void
assistant_prepare (GtkAssistant *ass, GtkWidget *page, EnrollData *data)
{
	const char *name;

	name = g_object_get_data (G_OBJECT (page), "name");
	if (name == NULL)
		return;

	if (g_str_equal (name, "enroll")) {
		DBusGProxy *p;
		GError *error = NULL;
		GladeXML *dialog = data->dialog_page2;
		guint i;
		GValue value = { 0, };

		if (!dbus_g_proxy_call (data->device, "Claim", &error, G_TYPE_STRING, "", G_TYPE_INVALID, G_TYPE_INVALID)) {
			GtkWidget *d;
			char *msg;

			/* translators:
			 * The variable is the name of the device, for example:
			 * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
			msg = g_strdup_printf (_("Could not access '%s' device"), data->name);
			d = get_error_dialog (msg, dbus_g_error_get_name (error), GTK_WINDOW (data->ass));
			g_error_free (error);
			gtk_dialog_run (GTK_DIALOG (d));
			gtk_widget_destroy (d);
			g_free (msg);

			enroll_data_destroy (data);

			return;
		}
		data->state = STATE_CLAIMED;

		p = dbus_g_proxy_new_from_proxy (data->device, "org.freedesktop.DBus.Properties", NULL);
		if (!dbus_g_proxy_call (p, "Get", NULL, G_TYPE_STRING, "net.reactivated.Fprint.Device", G_TYPE_STRING, "num-enroll-stages", G_TYPE_INVALID,
				       G_TYPE_VALUE, &value, G_TYPE_INVALID) || g_value_get_int (&value) < 1) {
			GtkWidget *d;
			char *msg;

			/* translators:
			 * The variable is the name of the device, for example:
			 * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
			msg = g_strdup_printf (_("Could not access '%s' device"), data->name);
			d = get_error_dialog (msg, "net.reactivated.Fprint.Error.Internal", GTK_WINDOW (data->ass));
			gtk_dialog_run (GTK_DIALOG (d));
			gtk_widget_destroy (d);
			g_free (msg);

			enroll_data_destroy (data);

			g_object_unref (p);
			return;
		}
		g_object_unref (p);

		data->num_enroll_stages = g_value_get_int (&value);

		/* Hide the extra "bulbs" if not needed */
		for (i = MAX_ENROLL_STAGES; i > data->num_enroll_stages; i--) {
			char *name;

			name = g_strdup_printf ("image%d", i);
			gtk_widget_hide (WID (name));
			g_free (name);
		}

		dbus_g_proxy_add_signal(data->device, "EnrollStatus", G_TYPE_STRING, G_TYPE_BOOLEAN, NULL);
		dbus_g_proxy_connect_signal(data->device, "EnrollStatus", G_CALLBACK(enroll_result), data, NULL);

		if (!dbus_g_proxy_call(data->device, "EnrollStart", &error, G_TYPE_STRING, data->finger, G_TYPE_INVALID, G_TYPE_INVALID)) {
			GtkWidget *d;
			char *msg;

			/* translators:
			 * The variable is the name of the device, for example:
			 * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
			msg = g_strdup_printf (_("Could not start finger capture on '%s' device"), data->name);
			d = get_error_dialog (msg, dbus_g_error_get_name (error), GTK_WINDOW (data->ass));
			g_error_free (error);
			gtk_dialog_run (GTK_DIALOG (d));
			gtk_widget_destroy (d);
			g_free (msg);

			enroll_data_destroy (data);

			return;
		}
		data->state = STATE_ENROLLING;;
	} else {
		if (data->state == STATE_ENROLLING) {
			dbus_g_proxy_call(data->device, "EnrollStop", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
			data->state = STATE_CLAIMED;
		}
		if (data->state == STATE_CLAIMED) {
			dbus_g_proxy_call(data->device, "Release", NULL, G_TYPE_INVALID, G_TYPE_INVALID);
			data->state = STATE_NONE;
		}
	}
}

static void
enroll_fingerprints (GtkWindow *parent, GtkWidget *enable, GtkWidget *disable)
{
	DBusGProxy *device, *p;
	GHashTable *props;
	GladeXML *dialog;
	EnrollData *data;
	GtkWidget *ass;
	char *msg;

	device = NULL;

	if (manager == NULL) {
		create_manager ();
		if (manager != NULL)
			device = get_first_device ();
	} else {
		device = get_first_device ();
	}

	if (manager == NULL || device == NULL) {
		GtkWidget *d;

		d = get_error_dialog (_("Could not access any fingerprint readers"),
				      _("Please contact your system administrator for help."),
				      parent);
		gtk_dialog_run (GTK_DIALOG (d));
		gtk_widget_destroy (d);
		return;
	}

	data = g_new0 (EnrollData, 1);
	data->device = device;
	data->enable = enable;
	data->disable = disable;

	/* Get some details about the device */
	p = dbus_g_proxy_new_from_proxy (device, "org.freedesktop.DBus.Properties", NULL);
	if (dbus_g_proxy_call (p, "GetAll", NULL, G_TYPE_STRING, "net.reactivated.Fprint.Device", G_TYPE_INVALID,
			       dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &props, G_TYPE_INVALID)) {
		const char *scan_type;
		data->name = g_value_dup_string (g_hash_table_lookup (props, "name"));
		scan_type = g_value_dup_string (g_hash_table_lookup (props, "scan-type"));
		if (g_str_equal (scan_type, "swipe"))
			data->is_swipe = TRUE;
		g_hash_table_destroy (props);
	}
	g_object_unref (p);

	ass = gtk_assistant_new ();
	gtk_window_set_title (GTK_WINDOW (ass), _("Enable Fingerprint Login"));
	gtk_window_set_transient_for (GTK_WINDOW (ass), parent);
	gtk_window_set_position (GTK_WINDOW (ass), GTK_WIN_POS_CENTER_ON_PARENT);
	g_signal_connect (G_OBJECT (ass), "cancel",
			  G_CALLBACK (assistant_cancelled), data);
	g_signal_connect (G_OBJECT (ass), "close",
			  G_CALLBACK (assistant_cancelled), data);
	g_signal_connect (G_OBJECT (ass), "prepare",
			  G_CALLBACK (assistant_prepare), data);

	/* Page 1 */
	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-about-me-fingerprint.glade",
				"page1", NULL);
	data->dialog_page1 = dialog;

	gtk_assistant_append_page (GTK_ASSISTANT (ass), WID("page1"));
	gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page1"), _("Select finger"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (ass), WID("page1"), GTK_ASSISTANT_PAGE_CONTENT);
	gtk_combo_box_set_active (GTK_COMBO_BOX (WID ("finger_combobox")), 0);

	g_signal_connect (G_OBJECT (WID ("radiobutton1")), "toggled",
			  G_CALLBACK (finger_radio_button_toggled), data);
	g_signal_connect (G_OBJECT (WID ("radiobutton2")), "toggled",
			  G_CALLBACK (finger_radio_button_toggled), data);
	g_signal_connect (G_OBJECT (WID ("radiobutton3")), "toggled",
			  G_CALLBACK (finger_radio_button_toggled), data);
	g_signal_connect (G_OBJECT (WID ("finger_combobox")), "changed",
			  G_CALLBACK (finger_combobox_changed), data);

	data->finger = selected_finger (dialog);

	g_object_set_data (G_OBJECT (WID("page1")), "name", "intro");

	/* translators:
	 * The variable is the name of the device, for example:
	 * "To enable fingerprint login, you need to save one of your fingerprints, using the
	 * 'Digital Persona U.are.U 4000/4000B' device." */
	msg = g_strdup_printf (_("To enable fingerprint login, you need to save one of your fingerprints, using the '%s' device."),
			       data->name);
	gtk_label_set_text (GTK_LABEL (WID("intro-label")), msg);
	g_free (msg);

	gtk_assistant_set_page_complete (GTK_ASSISTANT (ass), WID("page1"), TRUE);

	/* Page 2 */
	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-about-me-fingerprint.glade",
				"page2", NULL);
	data->dialog_page2 = dialog;
	gtk_assistant_append_page (GTK_ASSISTANT (ass), WID("page2"));
	if (data->is_swipe != FALSE)
		gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page2"), _("Swipe finger on reader"));
	else
		gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page2"), _("Place finger on reader"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (ass), WID("page2"), GTK_ASSISTANT_PAGE_CONTENT);

	g_object_set_data (G_OBJECT (WID("page2")), "name", "enroll");

	msg = g_strdup_printf (finger_str_to_msg (data->finger, data->is_swipe), data->name);
	gtk_label_set_text (GTK_LABEL (WID("enroll-label")), msg);
	g_free (msg);

	/* Page 3 */
	dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-about-me-fingerprint.glade",
				"page3", NULL);
	gtk_assistant_append_page (GTK_ASSISTANT (ass), WID("page3"));
	gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page3"), _("Done!"));
	gtk_assistant_set_page_type (GTK_ASSISTANT (ass), WID("page3"), GTK_ASSISTANT_PAGE_SUMMARY);

	g_object_set_data (G_OBJECT (WID("page3")), "name", "summary");

	data->ass = ass;
	gtk_widget_show_all (ass);
}

void
fingerprint_button_clicked (GladeXML *dialog,
			    GtkWidget *enable,
			    GtkWidget *disable)
{
	if (is_disable != FALSE) {
		delete_fingerprints_question (dialog, enable, disable);
	} else {
		enroll_fingerprints (GTK_WINDOW (WID ("about-me-dialog")), enable, disable);
	}
}

