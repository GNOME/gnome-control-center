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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "um-fingerprint-dialog.h"

/* Retrieve a widget from the UI object */
#define WID(s) GTK_WIDGET (gtk_builder_get_object (dialog, s))

/* Translate fprintd strings */
#define TR(s) dgettext("fprintd", s)
#include "fingerprint-strings.h"

/* This must match the number of images on the 2nd page in the UI file */
#define MAX_ENROLL_STAGES 5

static GDBusProxy *manager = NULL;
static GDBusConnection *connection = NULL;
static gboolean is_disable = FALSE;

enum {
        STATE_NONE,
        STATE_CLAIMED,
        STATE_ENROLLING
};

typedef struct {
        GtkWidget *editable_button;

        GtkWidget *ass;
        GtkBuilder *dialog;

        GDBusProxy *device;
        gboolean is_swipe;
        int num_enroll_stages;
        int num_stages_done;
        char *name;
        const char *finger;
        gint state;
} EnrollData;

static void
ensure_manager (void)
{
        GError *error = NULL;

        if (manager != NULL)
                return;

        connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (connection == NULL) {
                g_warning ("Failed to connect to session bus: %s", error->message);
                g_error_free (error);
                return;
        }

        manager = g_dbus_proxy_new_sync (connection,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL,
                                         "net.reactivated.Fprint",
                                         "/net/reactivated/Fprint/Manager",
                                         "net.reactivated.Fprint.Manager",
                                         NULL,
                                         &error);
        if (manager == NULL) {
                g_warning ("Failed to create fingerprint manager proxy: %s", error->message);
                g_error_free (error);
        }
}

static GDBusProxy *
get_first_device (void)
{
        GDBusProxy *device;
        GVariant *result;
        char *device_str = NULL;
        GError *error = NULL;

        result = g_dbus_proxy_call_sync (manager,
                                         "GetDefaultDevice",
                                         g_variant_new ("()"),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         NULL);
        if (result == NULL)
                return NULL;
        if (!g_variant_is_of_type (result, G_VARIANT_TYPE ("(o)")))
                g_warning ("net.reactivated.Fprint.Manager.GetDefaultDevice returns unknown result %s", g_variant_get_type_string (result));
        else
                g_variant_get (result, "(o)", &device_str);
        g_variant_unref (result);

        if (device_str == NULL)
                return NULL;

        device = g_dbus_proxy_new_sync (connection,
                                        G_DBUS_PROXY_FLAGS_NONE,
                                        NULL,
                                        "net.reactivated.Fprint",
                                        device_str,
                                        "net.reactivated.Fprint.Device",
                                        NULL,
                                        &error);
        if (device == NULL) {
                g_warning ("Failed to create fingerprint device proxy: %s", error->message);
                g_error_free (error);
        }

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
                return N_("An internal error occurred.");

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

gboolean
set_fingerprint_label (GtkWidget *button)
{
        GDBusProxy *device;
        GVariant *result;
        GVariantIter *fingers;
        GError *error = NULL;

        ensure_manager ();
        if (manager == NULL)
                return FALSE;

        device = get_first_device ();
        if (device == NULL)
                return FALSE;

        result = g_dbus_proxy_call_sync (device, "ListEnrolledFingers", g_variant_new ("(s)", ""), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
        if (!result) {
                if (!g_dbus_error_is_remote_error (error) ||
                    strcmp (g_dbus_error_get_remote_error(error), "net.reactivated.Fprint.Error.NoEnrolledPrints") != 0) {
                        g_object_unref (device);
                        return FALSE;
                }
        }

        if (result && g_variant_is_of_type (result, G_VARIANT_TYPE ("(as)")))
                g_variant_get (result, "(as)", &fingers);
        else
                fingers = NULL;

        if (fingers == NULL || g_variant_iter_n_children (fingers) == 0) {
                is_disable = FALSE;
                gtk_button_set_label (GTK_BUTTON (button), _("Disabled"));
        } else {
                is_disable = TRUE;
                gtk_button_set_label (GTK_BUTTON (button), _("Enabled"));
        }
        gtk_widget_set_halign (gtk_bin_get_child (GTK_BIN (button)), GTK_ALIGN_START);

        if (result != NULL)
                g_variant_unref (result);
        if (fingers != NULL)
                g_variant_iter_free (fingers);
        g_object_unref (device);

        return TRUE;
}

static void
delete_fingerprints (void)
{
        GDBusProxy *device;
        GVariant *result;

        ensure_manager ();
        if (manager == NULL)
                return;

        device = get_first_device ();
        if (device == NULL)
                return;

        result = g_dbus_proxy_call_sync (device, "DeleteEnrolledFingers", g_variant_new ("(s)", ""), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
        if (result)
                g_variant_unref (result);

        g_object_unref (device);
}

static void
delete_fingerprints_question (GtkWindow *parent,
                              GtkWidget *editable_button,
                              ActUser   *user)
{
        GtkWidget *question;
        GtkWidget *button;

        question = gtk_message_dialog_new (parent,
                                           GTK_DIALOG_MODAL,
                                           GTK_MESSAGE_QUESTION,
                                           GTK_BUTTONS_NONE,
                                           _("Delete registered fingerprints?"));
        gtk_dialog_add_button (GTK_DIALOG (question), _("_Cancel"), GTK_RESPONSE_CANCEL);
        gtk_window_set_modal (GTK_WINDOW (question), TRUE);

        button = gtk_button_new_with_mnemonic (_("_Delete Fingerprints"));
        gtk_widget_set_can_default (button, TRUE);
        gtk_widget_show (button);
        gtk_dialog_add_action_widget (GTK_DIALOG (question), button, GTK_RESPONSE_OK);

        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (question),
                                                  _("Do you want to delete your registered fingerprints so fingerprint login is disabled?"));
        gtk_dialog_set_default_response (GTK_DIALOG (question), GTK_RESPONSE_OK);

        if (gtk_dialog_run (GTK_DIALOG (question)) == GTK_RESPONSE_OK) {
                delete_fingerprints ();
                set_fingerprint_label (editable_button);
        }

        gtk_widget_destroy (question);
}

static gboolean
enroll_start (EnrollData *data, GError **error)
{
        GVariant *result;

        result = g_dbus_proxy_call_sync (data->device, "EnrollStart", g_variant_new ("(s)", data->finger), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
        if (result == NULL)
                return FALSE;
        g_variant_unref (result);
        return TRUE;
}

static gboolean
enroll_stop (EnrollData *data, GError **error)
{
        GVariant *result;

        result = g_dbus_proxy_call_sync (data->device, "EnrollStop", g_variant_new ("()"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
        if (result == NULL)
                return FALSE;
        g_variant_unref (result);
        return TRUE;
}

static gboolean
claim (EnrollData *data, GError **error)
{
        GVariant *result;

        result = g_dbus_proxy_call_sync (data->device, "Claim", g_variant_new ("(s)", ""), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
        if (result == NULL)
                return FALSE;
        g_variant_unref (result);
        return TRUE;
}

static gboolean
release (EnrollData *data, GError **error)
{
        GVariant *result;

        result = g_dbus_proxy_call_sync (data->device, "Release", g_variant_new ("()"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);
        if (result == NULL)
                return FALSE;
        g_variant_unref (result);
        return TRUE;
}

static void
enroll_data_destroy (EnrollData *data)
{
        switch (data->state) {
        case STATE_ENROLLING:
                enroll_stop (data, NULL);
                /* fall-through */
        case STATE_CLAIMED:
                release (data, NULL);
                /* fall-through */
        case STATE_NONE:
                g_free (data->name);
                g_object_unref (data->device);
                g_object_unref (data->dialog);
                gtk_widget_destroy (data->ass);

                g_free (data);
        }
}

static const char *
selected_finger (GtkBuilder *dialog)
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
        GtkBuilder *dialog = data->dialog;
        char *msg;

        data->finger = selected_finger (data->dialog);

        msg = finger_str_to_msg (data->finger, data->name, data->is_swipe);
        gtk_label_set_text (GTK_LABEL (WID("enroll-label")), msg);
        g_free (msg);
}

static void
finger_combobox_changed (GtkComboBox *combobox, EnrollData *data)
{
        GtkBuilder *dialog = data->dialog;
        char *msg;

        data->finger = selected_finger (data->dialog);

        msg = finger_str_to_msg (data->finger, data->name, data->is_swipe);
        gtk_label_set_text (GTK_LABEL (WID("enroll-label")), msg);
        g_free (msg);
}

static void
assistant_cancelled (GtkAssistant *ass, EnrollData *data)
{
        GtkWidget *editable_button = data->editable_button;

        enroll_data_destroy (data);
        set_fingerprint_label (editable_button);
}

static void
enroll_result (EnrollData *data, const char *result, gboolean done)
{
        GtkBuilder *dialog = data->dialog;
        char *msg;

        if (g_str_equal (result, "enroll-completed") || g_str_equal (result, "enroll-stage-passed")) {
                char *name, *path;

                data->num_stages_done++;
                name = g_strdup_printf ("image%d", data->num_stages_done);
                path = g_strdup_printf ("/org/gnome/control-center/user-accounts/print_ok.png");
                gtk_image_set_from_resource (GTK_IMAGE (WID (name)), path);
                g_free (name);
                g_free (path);
        }
        if (g_str_equal (result, "enroll-completed")) {
                gtk_label_set_text (GTK_LABEL (WID ("status-label")), _("Done!"));
                gtk_label_set_text (GTK_LABEL (WID("enroll-label")), "");
                gtk_assistant_set_page_complete (GTK_ASSISTANT (data->ass), WID ("page2"), TRUE);
        }

        if (done != FALSE) {
                enroll_stop (data, NULL);
                data->state = STATE_CLAIMED;
                if (g_str_equal (result, "enroll-completed") == FALSE) {
                        /* The enrollment failed, restart it */
                        enroll_start (data, NULL);
                        data->state = STATE_ENROLLING;
                        result = "enroll-retry-scan";
                } else {
                        return;
                }
        }

        msg = g_strdup_printf (TR(enroll_result_str_to_msg (result, data->is_swipe)), data->name);
        gtk_label_set_text (GTK_LABEL (WID ("status-label")), msg);
        g_free (msg);
}

static void
device_signal_cb (GDBusProxy *proxy, gchar *sender_name, gchar *signal_name, GVariant *parameters, EnrollData *data)
{
        if (strcmp (signal_name, "EnrollStatus") == 0) {
                if (g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sb)"))) {
                        gchar *result;
                        gboolean done;

                        g_variant_get (parameters, "(&sb)", &result, &done);
                        enroll_result (data, result, done);
                }
        }
}

static void
assistant_prepare (GtkAssistant *ass, GtkWidget *page, EnrollData *data)
{
        const char *name;

        name = g_object_get_data (G_OBJECT (page), "name");
        if (name == NULL)
                return;

        if (g_str_equal (name, "enroll")) {
                GError *error = NULL;
                GtkBuilder *dialog = data->dialog;
                char *path;
                guint i;
                GVariant *result;
                gint num_enroll_stages;

                if (!claim (data, &error)) {
                        GtkWidget *d;
                        char *msg;

                        /* translators:
                         * The variable is the name of the device, for example:
                         * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
                        msg = g_strdup_printf (_("Could not access “%s” device"), data->name);
                        d = get_error_dialog (msg, error->message, GTK_WINDOW (data->ass));
                        g_error_free (error);
                        gtk_dialog_run (GTK_DIALOG (d));
                        gtk_widget_destroy (d);
                        g_free (msg);

                        enroll_data_destroy (data);

                        return;
                }
                data->state = STATE_CLAIMED;

                result = g_dbus_connection_call_sync (connection,
                                                      "net.reactivated.Fprint",
                                                      g_dbus_proxy_get_object_path (data->device),
                                                      "org.freedesktop.DBus.Properties",
                                                      "Get",
                                                      g_variant_new ("(ss)", "net.reactivated.Fprint.Device", "num-enroll-stages"),
                                                      G_VARIANT_TYPE ("(v)"),
                                                      G_DBUS_CALL_FLAGS_NONE,
                                                      -1,
                                                      NULL,
                                                      &error);
                num_enroll_stages = 0;
                if (result) {
                        GVariant *v;

                        g_variant_get (result, "(v)", &v);
                        num_enroll_stages = g_variant_get_int32 (v);

                        g_variant_unref (result);
                        g_variant_unref (v);
                }

                if (num_enroll_stages < 1) {
                        GtkWidget *d;
                        char *msg;

                        /* translators:
                         * The variable is the name of the device, for example:
                         * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
                        msg = g_strdup_printf (_("Could not access “%s” device"), data->name);
                        d = get_error_dialog (msg, "net.reactivated.Fprint.Error.Internal", GTK_WINDOW (data->ass));
                        gtk_dialog_run (GTK_DIALOG (d));
                        gtk_widget_destroy (d);
                        g_free (msg);

                        enroll_data_destroy (data);
                        return;
                }

                data->num_enroll_stages = num_enroll_stages;

                /* Hide the extra "bulbs" if not needed */
                for (i = MAX_ENROLL_STAGES; i > data->num_enroll_stages; i--) {
                        char *name;

                        name = g_strdup_printf ("image%d", i);
                        gtk_widget_hide (WID (name));
                        g_free (name);
                }
                /* And set the right image */
                {
                        path = g_strdup_printf ("/org/gnome/control-center/user-accounts/%s.png", data->finger);
                }
                for (i = 1; i <= data->num_enroll_stages; i++) {
                        char *name;
                        name = g_strdup_printf ("image%d", i);
                        gtk_image_set_from_resource (GTK_IMAGE (WID (name)), path);
                        g_free (name);
                }
                g_free (path);

                g_signal_connect (data->device, "g-signal", G_CALLBACK (device_signal_cb), data);

                if (!enroll_start (data, &error)) {
                        GtkWidget *d;
                        char *msg;

                        /* translators:
                         * The variable is the name of the device, for example:
                         * "Could you not access "Digital Persona U.are.U 4000/4000B" device */
                        msg = g_strdup_printf (_("Could not start finger capture on “%s” device"), data->name);
                        d = get_error_dialog (msg, error->message, GTK_WINDOW (data->ass));
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
                        enroll_stop (data, NULL);
                        data->state = STATE_CLAIMED;
                }
                if (data->state == STATE_CLAIMED) {
                        release (data, NULL);
                        data->state = STATE_NONE;
                }
        }
}

static void
enroll_fingerprints (GtkWindow *parent,
                     GtkWidget *editable_button,
                     ActUser   *user)
{
        GDBusProxy *device = NULL;
        GtkBuilder *dialog;
        EnrollData *data;
        GtkWidget *ass;
        char *msg;
        GVariant *result;
        GError *error = NULL;

        ensure_manager ();
        if (manager != NULL)
                device = get_first_device ();

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
        data->editable_button = editable_button;

        /* Get some details about the device */
        result = g_dbus_connection_call_sync (connection,
                                              "net.reactivated.Fprint",
                                              g_dbus_proxy_get_object_path (data->device),
                                              "org.freedesktop.DBus.Properties",
                                              "GetAll",
                                              g_variant_new ("(s)", "net.reactivated.Fprint.Device"),
                                              G_VARIANT_TYPE ("(a{sv})"),
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              NULL);
        if (result) {
                GVariant *props;
                gchar *scan_type;

                g_variant_get (result, "(@a{sv})", &props);
                g_variant_lookup (props, "name", "s", &data->name);
                g_variant_lookup (props, "scan-type", "s", &scan_type);
                if (g_strcmp0 (scan_type, "swipe") == 0)
                        data->is_swipe = TRUE;
                g_free (scan_type);
                g_variant_unref (props);
                g_variant_unref (result);
        }

        dialog = gtk_builder_new ();
        if (!gtk_builder_add_from_resource (dialog,
                                            "/org/gnome/control-center/user-accounts/account-fingerprint.ui",
                                            &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                return;
        }
        data->dialog = dialog;

        ass = WID ("assistant");
        gtk_window_set_title (GTK_WINDOW (ass), _("Enable Fingerprint Login"));
        gtk_window_set_transient_for (GTK_WINDOW (ass), parent);
        gtk_window_set_modal (GTK_WINDOW (ass), TRUE);
        gtk_window_set_resizable (GTK_WINDOW (ass), FALSE);
        gtk_window_set_type_hint (GTK_WINDOW (ass), GDK_WINDOW_TYPE_HINT_DIALOG);

        g_signal_connect (G_OBJECT (ass), "cancel",
                          G_CALLBACK (assistant_cancelled), data);
        g_signal_connect (G_OBJECT (ass), "close",
                          G_CALLBACK (assistant_cancelled), data);
        g_signal_connect (G_OBJECT (ass), "prepare",
                          G_CALLBACK (assistant_prepare), data);

        /* Page 1 */
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
         * 'Digital Persona U.are.U 4000/4000B' device."
         */
        msg = g_strdup_printf (_("To enable fingerprint login, you need to save one of your fingerprints, using the “%s” device."),
                               data->name);
        gtk_label_set_text (GTK_LABEL (WID("intro-label")), msg);
        g_free (msg);

        gtk_assistant_set_page_complete (GTK_ASSISTANT (ass), WID("page1"), TRUE);

        gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page1"), _("Selecting finger"));
        gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page2"), _("Enrolling fingerprints"));
        gtk_assistant_set_page_title (GTK_ASSISTANT (ass), WID("page3"), _("Summary"));

        /* Page 2 */
        g_object_set_data (G_OBJECT (WID("page2")), "name", "enroll");

        msg = finger_str_to_msg (data->finger, data->name, data->is_swipe);
        gtk_label_set_text (GTK_LABEL (WID("enroll-label")), msg);
        g_free (msg);

        /* Page 3 */
        g_object_set_data (G_OBJECT (WID("page3")), "name", "summary");

        data->ass = ass;
        gtk_widget_show (ass);
}

void
fingerprint_button_clicked (GtkWindow *parent,
                            GtkWidget *editable_button,
                            ActUser   *user)
{
        bindtextdomain ("fprintd", GNOMELOCALEDIR);
        bind_textdomain_codeset ("fprintd", "UTF-8");

        if (is_disable != FALSE) {
                delete_fingerprints_question (parent, editable_button, user);
        } else {
                enroll_fingerprints (parent, editable_button, user);
        }
}

#pragma GCC diagnostic pop
