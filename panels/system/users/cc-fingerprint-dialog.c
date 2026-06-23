/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include <adwaita.h>
#include <cairo/cairo.h>
#include <glib/gi18n.h>

#include "cc-fingerprint-dialog.h"

#include "cc-fingerprint-manager.h"
#include "cc-fprintd-generated.h"
#include "cc-list-row.h"

#include "config.h"

#define CC_FPRINTD_NAME "net.reactivated.Fprint"

/* Translate fprintd strings */
#define TR(s) dgettext ("fprintd", s)
#include "fingerprint-strings.h"

typedef enum {
    DIALOG_STATE_NONE = 0,
    DIALOG_STATE_DEVICES_LISTING = (1 << 0),
    DIALOG_STATE_DEVICE_CLAIMING = (1 << 1),
    DIALOG_STATE_DEVICE_CLAIMED = (1 << 2),
    DIALOG_STATE_DEVICE_PRINTS_LISTING = (1 << 3),
    DIALOG_STATE_DEVICE_RELEASING = (1 << 4),
    DIALOG_STATE_DEVICE_ENROLL_STARTING = (1 << 5),
    DIALOG_STATE_DEVICE_ENROLLING = (1 << 6),
    DIALOG_STATE_DEVICE_ENROLL_STOPPING = (1 << 7),
    DIALOG_STATE_DEVICE_DELETING = (1 << 8),

    DIALOG_STATE_IDLE = DIALOG_STATE_DEVICE_CLAIMED | DIALOG_STATE_DEVICE_ENROLLING,
} DialogState;

struct _CcFingerprintDialog {
    AdwDialog parent_instance;

    GtkButton *back_button;
    GtkButton *done_button;
    AdwPreferencesGroup *prints_group;
    AdwHeaderBar *titlebar;
    GtkImage *enroll_result_image;
    GtkLabel *enroll_message;
    GtkLabel *enroll_result_message;
    AdwPreferencesGroup *left_hand_finger_group;
    AdwPreferencesGroup *right_hand_finger_group;
    GtkWidget *finger_selection_page;
    AdwPreferencesGroup *devices_list;
    AdwSpinner *spinner;
    GtkStack *stack;
    GtkWidget *add_finger_button;
    GtkWidget *device_selector;
    GtkWidget *enroll_result_icon;
    GtkWidget *enrollment_view;
    AdwStatusPage *error_page;
    GtkWidget *no_devices_found;
    GtkWidget *no_fingerprints_enrolled_page;
    GtkProgressBar *progress_bar;
    GtkWidget *prints_manager;

    CcFingerprintManager *manager;
    DialogState dialog_state;
    CcFprintdDevice *device;
    gulong device_signal_id;
    gulong device_name_owner_id;
    gulong device_finger_status_id;
    GCancellable *cancellable;
    GStrv enrolled_fingers;
    guint enroll_stages_passed;
    gdouble enroll_progress;

    GListStore *fingerprints_store;
    GListStore *left_hand_finger_options;
    GListStore *right_hand_finger_options;

    gboolean finger_on_reader;
};

/* TODO - fprintd and API changes required:
  - Identify the finger when the enroll dialog is visible
    + Only if device supports identification
      · And only in such case support enrolling more than one finger
  - Delete a single fingerprint | and remove the "Delete all" button
  - Highlight the finger when the sensor is touched during enrollment
  - Add customized labels to fingerprints
  - Devices hotplug (object manager)
 */

G_DEFINE_FINAL_TYPE (CcFingerprintDialog, cc_fingerprint_dialog, ADW_TYPE_DIALOG)

enum {
    PROP_0,
    PROP_MANAGER,
    N_PROPS
};

#define N_VALID_FINGERS G_N_ELEMENTS (RIGHT_HAND_FINGER_IDS) + G_N_ELEMENTS (LEFT_HAND_FINGER_IDS)
/* The order of the fingers here will affect the UI order */
const char *RIGHT_HAND_FINGER_IDS[] = {
    "right-thumb",
    "right-index-finger",
    "right-middle-finger",
};
const char *LEFT_HAND_FINGER_IDS[] = {
    "left-thumb",
    "left-index-finger",
    "left-middle-finger",
};

typedef enum {
    ENROLL_STATE_NORMAL,
    ENROLL_STATE_RETRY,
    ENROLL_STATE_SUCCESS,
    ENROLL_STATE_WARNING,
    ENROLL_STATE_ERROR,
    ENROLL_STATE_COMPLETED,
    N_ENROLL_STATES,
} EnrollState;

const char *ENROLL_STATE_CLASSES[N_ENROLL_STATES] = {
    "normal",               /* ENROLL_STATE_NORMAL (undefined) */
    "fingerprint-warning",  /* ENROLL_STATE RETRY */
    "fingerprint-touching", /* Used when finger is touching and when result is ENROLL_STATE_SUCCESS */
    "fingerprint-warning",  /* ENROLL_STATE_WARNING */
    "fingerprint-warning",  /* ENROLL_STATE_ERROR */
    "completed",            /* ENROLL_STATE_COMPLETED */
};

static GParamSpec *properties[N_PROPS];

static void enroll_finger (CcFingerprintDialog *self, const char *finger_id);
static void update_prints_store (CcFingerprintDialog *self);

CcFingerprintDialog *
cc_fingerprint_dialog_new (CcFingerprintManager *manager)
{
    return g_object_new (CC_TYPE_FINGERPRINT_DIALOG, "fingerprint-manager", manager, NULL);
}

static gboolean
update_dialog_state (CcFingerprintDialog *self, DialogState state)
{
    if (self->dialog_state == state)
        return FALSE;

    self->dialog_state = state;

    if (self->dialog_state == DIALOG_STATE_NONE || self->dialog_state == (self->dialog_state & DIALOG_STATE_IDLE)) {
        gtk_widget_set_visible (GTK_WIDGET (self->spinner), FALSE);
    } else {
        gtk_widget_set_visible (GTK_WIDGET (self->spinner), TRUE);
    }

    return TRUE;
}

static gboolean
add_dialog_state (CcFingerprintDialog *self, DialogState state)
{
    return update_dialog_state (self, (self->dialog_state | state));
}

static gboolean
remove_dialog_state (CcFingerprintDialog *self, DialogState state)
{
    return update_dialog_state (self, (self->dialog_state & ~state));
}

typedef struct {
    CcFingerprintDialog *dialog;
    DialogState state;
} DialogStateRemover;

static DialogStateRemover *
auto_state_remover (CcFingerprintDialog *self, DialogState state)
{
    DialogStateRemover *state_remover;

    state_remover = g_new0 (DialogStateRemover, 1);
    state_remover->dialog = g_object_ref (self);
    state_remover->state = state;

    return state_remover;
}

static void
auto_state_remover_cleanup (DialogStateRemover *state_remover)
{
    remove_dialog_state (state_remover->dialog, state_remover->state);
    g_clear_object (&state_remover->dialog);
    g_free (state_remover);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DialogStateRemover, auto_state_remover_cleanup);

static const char *
dbus_error_to_human (CcFingerprintDialog *self, GError *error)
{
    g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);

    if (dbus_error == NULL) { /* Fallback to generic */
    } else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.ClaimDevice"))
        return _("the device needs to be claimed to perform this action");
    else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.AlreadyInUse"))
        return _("the device is already claimed by another process");
    else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.PermissionDenied"))
        return _("you do not have permission to perform the action");
    else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoEnrolledPrints"))
        return _("no prints have been enrolled");
    else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoActionInProgress")) {  /* Fallback to generic */
    } else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.InvalidFingername")) { /* Fallback to generic */
    } else if (g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.Internal")) {          /* Fallback to generic */
    }

    if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING)
        return _("Failed to communicate with the device during enrollment");

    if (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED || self->dialog_state & DIALOG_STATE_DEVICE_CLAIMING)
        return _("Failed to communicate with the fingerprint reader");

    return _("Failed to communicate with the fingerprint daemon");
}

static void
disconnect_device_signals (CcFingerprintDialog *self)
{
    if (!self->device)
        return;

    g_clear_signal_handler (&self->device_signal_id, self->device);
    g_clear_signal_handler (&self->device_name_owner_id, self->device);
    g_clear_signal_handler (&self->device_finger_status_id, self->device);
}

static void
cc_fingerprint_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

    switch (prop_id) {
    case PROP_MANAGER:
        g_value_set_object (value, self->manager);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_fingerprint_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

    switch (prop_id) {
    case PROP_MANAGER:
        g_set_object (&self->manager, g_value_get_object (value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
notify_error (CcFingerprintDialog *self, const char *error_message)
{
    adw_status_page_set_description (self->error_page, error_message);
    gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->error_page));
}

static const char *
get_finger_name (const char *finger_id)
{
    if (g_str_equal (finger_id, "left-thumb"))
        return _("Left thumb");
    if (g_str_equal (finger_id, "left-middle-finger"))
        return _("Left middle finger");
    if (g_str_equal (finger_id, "left-index-finger"))
        return _("_Left index finger");
    if (g_str_equal (finger_id, "left-ring-finger"))
        return _("Left ring finger");
    if (g_str_equal (finger_id, "left-little-finger"))
        return _("Left little finger");
    if (g_str_equal (finger_id, "right-thumb"))
        return _("Right thumb");
    if (g_str_equal (finger_id, "right-middle-finger"))
        return _("Right middle finger");
    if (g_str_equal (finger_id, "right-index-finger"))
        return _("_Right index finger");
    if (g_str_equal (finger_id, "right-ring-finger"))
        return _("Right ring finger");
    if (g_str_equal (finger_id, "right-little-finger"))
        return _("Right little finger");

    g_return_val_if_reached (_("Unknown Finger"));
}

static void
on_fingerprint_deleted_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);

    cc_fprintd_device_call_delete_enrolled_finger_finish (fprintd_device, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    if (error) {
        g_autofree char *error_message = NULL;

        error_message = g_strdup_printf (_("Failed to delete fingerprint: %s"), dbus_error_to_human (self, error));
        g_warning ("Deletion of fingerprints on device %s failed: %s", cc_fprintd_device_get_name (self->device),
                   error->message);
        notify_error (self, error_message);
    }

    update_prints_store (self);
    cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
}

static void
delete_fingerprint (GtkButton *button, gpointer user_data)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);
    const gchar *finger_id = g_object_get_data (G_OBJECT (button), "finger-id");

    cc_fprintd_device_call_delete_enrolled_finger (self->device, finger_id, G_DBUS_CALL_FLAGS_NONE, -1,
                                                   self->cancellable, on_fingerprint_deleted_cb, self);
}

static GtkWidget *
create_fingerprint_row (GObject *item, gpointer user_data)
{
    GtkStringObject *fingerprint = GTK_STRING_OBJECT (item);
    const gchar *finger_id = gtk_string_object_get_string (fingerprint);
    GtkWidget *row = adw_action_row_new ();
    GtkWidget *delete_button = gtk_button_new ();

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), get_finger_name (finger_id));
    adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (row), TRUE);

    g_object_set_data (G_OBJECT (delete_button), "finger-id", g_strdup (finger_id));
    g_signal_connect (delete_button, "clicked", G_CALLBACK (delete_fingerprint), user_data);

    gtk_button_set_icon_name (GTK_BUTTON (delete_button), "edit-delete-symbolic");
    gtk_widget_add_css_class (delete_button, "flat");
    gtk_widget_set_valign (delete_button, GTK_ALIGN_CENTER);
    adw_action_row_add_suffix (ADW_ACTION_ROW (row), delete_button);

    return row;
}

static void
on_finger_selected_cb (AdwActionRow *row, gpointer user_data)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);
    const gchar *finger_id = g_object_get_data (G_OBJECT (row), "finger-id");

    enroll_finger (self, finger_id);
}

static GtkWidget *
create_finger_option_row (gpointer *item, gpointer *user_data)
{
    GtkStringObject *finger = GTK_STRING_OBJECT (item);
    const gchar *finger_id = gtk_string_object_get_string (finger);
    GtkWidget *row;

    row = g_object_new (CC_TYPE_LIST_ROW, "show-arrow", TRUE, NULL);
    gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
    adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (row), TRUE);

    adw_action_row_set_icon_name (ADW_ACTION_ROW (row), "fingerprint-detection-symbolic");

    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), get_finger_name (finger_id));
    g_object_set_data_full (G_OBJECT (row), "finger-id", g_strdup (finger_id), g_free);

    g_signal_connect (row, "activated", G_CALLBACK (on_finger_selected_cb), user_data);

    return row;
}

static void
populate_finger_groups (CcFingerprintDialog *self)
{
    int i;

    g_list_store_remove_all (self->right_hand_finger_options);
    g_list_store_remove_all (self->left_hand_finger_options);

    for (i = 0; i < G_N_ELEMENTS (RIGHT_HAND_FINGER_IDS); i++) {
        GtkStringObject *finger;
        const gchar *finger_id = RIGHT_HAND_FINGER_IDS[i];

        if (self->enrolled_fingers != NULL)
            if (g_strv_contains ((const gchar **) self->enrolled_fingers, finger_id))
                continue;

        finger = gtk_string_object_new (finger_id);
        g_list_store_append (self->right_hand_finger_options, finger);
    }

    for (i = 0; i < G_N_ELEMENTS (LEFT_HAND_FINGER_IDS); i++) {
        GtkStringObject *finger;
        const gchar *finger_id = LEFT_HAND_FINGER_IDS[i];

        if (self->enrolled_fingers != NULL)
            if (g_strv_contains ((const gchar **) self->enrolled_fingers, finger_id))
                continue;

        finger = gtk_string_object_new (finger_id);
        g_list_store_append (self->left_hand_finger_options, finger);
    }
}

static GList *
get_container_children (GtkWidget *container)
{
    GtkWidget *child;
    GList *list = NULL;

    child = gtk_widget_get_first_child (container);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling (child);

        list = g_list_append (list, child);

        child = next;
    }

    return list;
}

static void
list_enrolled_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_auto(GStrv) enrolled_fingers = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(DialogStateRemover) state_remover = NULL;
    CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
    CcFingerprintDialog *self = user_data;
    guint n_enrolled_fingers = 0;

    cc_fprintd_device_call_list_enrolled_fingers_finish (fprintd_device, &enrolled_fingers, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_PRINTS_LISTING);

    gtk_widget_set_sensitive (GTK_WIDGET (self->add_finger_button), TRUE);

    if (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED)
        gtk_widget_set_sensitive (GTK_WIDGET (self->prints_manager), TRUE);

    if (error) {
        g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);

        if (!dbus_error || !g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.NoEnrolledPrints")) {
            g_autofree char *error_message = NULL;

            error_message = g_strdup_printf (_("Failed to list fingerprints: %s"), dbus_error_to_human (self, error));
            g_warning ("Listing of fingerprints on device %s failed: %s", cc_fprintd_device_get_name (self->device),
                       error->message);
            notify_error (self, error_message);
            return;
        }
    } else {
        n_enrolled_fingers = g_strv_length (enrolled_fingers);
    }

    g_list_store_remove_all (self->fingerprints_store);

    self->enrolled_fingers = g_steal_pointer (&enrolled_fingers);
    if (self->enrolled_fingers != NULL) {
        for (gchar **ptr = self->enrolled_fingers; *ptr != NULL; ptr++) {
            const gchar *finger_id = *ptr;

            g_list_store_append (self->fingerprints_store, gtk_string_object_new (finger_id));
        }
    }

    populate_finger_groups (self);

    if (n_enrolled_fingers == N_VALID_FINGERS)
        gtk_widget_set_sensitive (self->add_finger_button, FALSE);

    if (n_enrolled_fingers == 0)
        gtk_stack_set_visible_child (self->stack, self->no_fingerprints_enrolled_page);
}

static void
update_prints_store (CcFingerprintDialog *self)
{
    ActUser *user;

    g_assert_true (CC_FPRINTD_IS_DEVICE (self->device));

    if (!add_dialog_state (self, DIALOG_STATE_DEVICE_PRINTS_LISTING))
        return;

    gtk_widget_set_sensitive (GTK_WIDGET (self->add_finger_button), FALSE);

    g_clear_pointer (&self->enrolled_fingers, g_strfreev);

    user = cc_fingerprint_manager_get_user (self->manager);
    cc_fprintd_device_call_list_enrolled_fingers (self->device, act_user_get_user_name (user),
                                                  G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION, -1,
                                                  self->cancellable, list_enrolled_cb, self);
}

static gboolean
have_multiple_devices (CcFingerprintDialog *self)
{
    g_autoptr(GList) devices_rows = NULL;

    devices_rows = get_container_children (GTK_WIDGET (self->devices_list));

    return devices_rows && devices_rows->next;
}

static void
remove_all_css_classes_from_enrollment_view (gpointer user_data)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);

    for (int i = 0; i < N_ENROLL_STATES; ++i)
        gtk_widget_remove_css_class (self->enrollment_view, ENROLL_STATE_CLASSES[i]);
}

static void
set_enroll_result_message (CcFingerprintDialog *self, EnrollState enroll_state, const char *message)
{
    const char *icon_name;

    g_return_if_fail (enroll_state >= 0 && enroll_state < N_ENROLL_STATES);

    switch (enroll_state) {
    case ENROLL_STATE_WARNING:
    case ENROLL_STATE_ERROR:
        icon_name = "fingerprint-detection-warning-symbolic";
        break;
    case ENROLL_STATE_COMPLETED:
        icon_name = "object-select-symbolic";
        adw_status_page_set_title (ADW_STATUS_PAGE (self->enrollment_view), _("Scan complete!"));
        break;
    default:
        icon_name = "fingerprint-detection-symbolic";
    }

    remove_all_css_classes_from_enrollment_view (self);
    if (self->finger_on_reader || enroll_state == ENROLL_STATE_COMPLETED) {
        gtk_widget_add_css_class (self->enrollment_view, ENROLL_STATE_CLASSES[enroll_state]);
    }

    adw_status_page_set_icon_name (ADW_STATUS_PAGE (self->enrollment_view), icon_name);
    adw_status_page_set_title (ADW_STATUS_PAGE (self->enrollment_view),
                               message ? message : _("Touch finger on reader"));
}

static void
on_finger_present_cb (CcFingerprintDialog *self)
{
    self->finger_on_reader = cc_fprintd_device_get_finger_present (self->device);
    g_debug ("Finger is touching the fingerprint reader: %s", self->finger_on_reader ? "yes" : "no");

    if (self->finger_on_reader) {
        gtk_widget_add_css_class (self->enrollment_view, "fingerprint-touching");
    } else {
        set_enroll_result_message (self, ENROLL_STATE_NORMAL, NULL);
    }
}

static void
update_enroll_progress (CcFingerprintDialog *self)
{
    guint enroll_stages = cc_fprintd_device_get_num_enroll_stages (self->device);

    self->enroll_stages_passed++;

    if (enroll_stages > 0) {
        self->enroll_progress = MIN (1.0f, self->enroll_stages_passed / (double) enroll_stages);
        gtk_progress_bar_set_fraction (self->progress_bar, self->enroll_progress);
    } else {
        g_warning ("The device %s requires an invalid number of enroll stages (%u)",
                   cc_fprintd_device_get_name (self->device), enroll_stages);
    }

    g_debug ("Enroll state passed, %u/%u (%.2f%%)", self->enroll_stages_passed, enroll_stages, self->enroll_progress);
}

static void
handle_enroll_stage_passed (CcFingerprintDialog *self)
{
    update_enroll_progress (self);
    set_enroll_result_message (self, ENROLL_STATE_SUCCESS, NULL);
}

static void
handle_enroll_completed (CcFingerprintDialog *self)
{
    update_enroll_progress (self);

    if (!G_APPROX_VALUE (self->enroll_progress, 1.0f, FLT_EPSILON)) {
        g_warning ("Device marked enroll as completed, but progress is at %.2f", self->enroll_progress);
        self->enroll_progress = 1.0f;
        gtk_progress_bar_set_fraction (self->progress_bar, self->enroll_progress);
    }

    set_enroll_result_message (self, ENROLL_STATE_COMPLETED, _("Scan complete"));
    gtk_widget_set_visible (GTK_WIDGET (self->done_button), TRUE);
    gtk_widget_grab_focus (GTK_WIDGET (self->done_button));
}

static void
handle_enroll_retry (CcFingerprintDialog *self, const char *result)
{
    const char *scan_type = cc_fprintd_device_get_scan_type (self->device);
    gboolean is_swipe = g_str_equal (scan_type, "swipe");
    const char *message = enroll_result_str_to_msg (result, is_swipe);

    /* Only show retry message if finger is still on the reader to avoid stale messages */
    if (self->finger_on_reader)
        set_enroll_result_message (self, ENROLL_STATE_RETRY, message);
}

static void
handle_enroll_failed (CcFingerprintDialog *self, const char *result)
{
    const char *message;

    if (g_str_equal (result, "enroll-disconnected")) {
        message = _("Fingerprint device disconnected");
        remove_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED | DIALOG_STATE_DEVICE_ENROLLING);
    } else if (g_str_equal (result, "enroll-data-full")) {
        message = _("Fingerprint device storage is full");
    } else if (g_str_equal (result, "enroll-duplicate")) {
        message = _("Fingerprint is duplicate");
    } else {
        message = _("Failed to enroll new fingerprint");
    }

    set_enroll_result_message (self, ENROLL_STATE_WARNING, message);
}

static void
handle_enroll_signal (CcFingerprintDialog *self, const char *result, gboolean done)
{
    g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING);

    g_debug ("Device enroll result message: %s, done: %d", result, done);

    if (g_str_equal (result, "enroll-completed")) {
        handle_enroll_completed (self);
        return;
    }

    if (g_str_equal (result, "enroll-stage-passed")) {
        handle_enroll_stage_passed (self);
        return;
    }

    if (done) {
        handle_enroll_failed (self, result);
        return;
    }

    handle_enroll_retry (self, result);
}

static void
enroll_start_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(DialogStateRemover) state_remover = NULL;
    CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
    CcFingerprintDialog *self = user_data;

    cc_fprintd_device_call_enroll_start_finish (fprintd_device, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_ENROLL_STARTING);

    if (error) {
        g_autofree char *error_message = NULL;

        remove_dialog_state (self, DIALOG_STATE_DEVICE_ENROLLING);

        error_message = g_strdup_printf (_("Failed to start enrollment: %s"), dbus_error_to_human (self, error));
        g_warning ("Enrollment on device %s failed: %s", cc_fprintd_device_get_name (self->device), error->message);
        notify_error (self, error_message);

        set_enroll_result_message (self, ENROLL_STATE_ERROR,
                                   C_("Fingerprint enroll state",
                                    "Failed to enroll new fingerprint"));
        gtk_widget_set_sensitive (self->enrollment_view, FALSE);

        return;
    }
}

static void
enroll_stop_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(DialogStateRemover) state_remover = NULL;
    CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
    CcFingerprintDialog *self = user_data;

    cc_fprintd_device_call_enroll_stop_finish (fprintd_device, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_ENROLLING | DIALOG_STATE_DEVICE_ENROLL_STOPPING);
    gtk_widget_set_sensitive (self->enrollment_view, TRUE);
    gtk_stack_set_visible_child (self->stack, self->prints_manager);

    if (error) {
        g_autofree char *error_message = NULL;

        error_message = g_strdup_printf (_("Failed to stop enrollment: %s"), dbus_error_to_human (self, error));
        g_warning ("Stopping enrollment on device %s failed: %s", cc_fprintd_device_get_name (self->device),
                   error->message);
        notify_error (self, error_message);

        return;
    }

    cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
}

static void
enroll_stop (CcFingerprintDialog *self)
{
    g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING);

    if (!add_dialog_state (self, DIALOG_STATE_DEVICE_ENROLL_STOPPING))
        return;

    gtk_widget_set_sensitive (self->enrollment_view, FALSE);
    cc_fprintd_device_call_enroll_stop (self->device, G_DBUS_CALL_FLAGS_NONE, -1, self->cancellable, enroll_stop_cb,
                                        self);
}

static char *
get_enrollment_string (CcFingerprintDialog *self, const char *finger_id)
{
    char *ret;
    const char *scan_type;
    const char *device_name;
    gboolean is_swipe;

    device_name = NULL;
    scan_type = cc_fprintd_device_get_scan_type (self->device);
    is_swipe = g_str_equal (scan_type, "swipe");

    if (have_multiple_devices (self))
        device_name = cc_fprintd_device_get_name (self->device);

    ret = finger_str_to_msg ("any", device_name, is_swipe);

    if (ret)
        return ret;

    return g_strdup (_("Repeatedly lift and place your finger on the reader to enroll your fingerprint"));
}

static void
enroll_finger (CcFingerprintDialog *self, const char *finger_id)
{
    g_autofree char *enroll_message = NULL;

    g_return_if_fail (finger_id);

    if (!add_dialog_state (self, DIALOG_STATE_DEVICE_ENROLLING | DIALOG_STATE_DEVICE_ENROLL_STARTING))
        return;

    self->enroll_progress = 0;
    self->enroll_stages_passed = 0;

    g_debug ("Enrolling finger %s", finger_id);

    enroll_message = get_enrollment_string (self, finger_id);

    set_enroll_result_message (self, ENROLL_STATE_NORMAL, NULL);
    gtk_stack_set_visible_child (self->stack, self->enrollment_view);
    gtk_progress_bar_set_fraction (self->progress_bar, 0);
    adw_status_page_set_title (ADW_STATUS_PAGE (self->enrollment_view), enroll_message);

    cc_fprintd_device_call_enroll_start (self->device, finger_id, G_DBUS_CALL_FLAGS_NONE, -1, self->cancellable,
                                         enroll_start_cb, self);
}

static void
on_add_fingerprint_button_activated_cb (CcFingerprintDialog *self)
{
    gtk_stack_set_visible_child (self->stack, self->finger_selection_page);
}

static void
release_device_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
    CcFingerprintDialog *self = user_data;

    cc_fprintd_device_call_release_finish (fprintd_device, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    if (error) {
        g_autofree char *error_message = NULL;

        error_message =
            g_strdup_printf (_("Failed to release fingerprint device %s: %s"),
                               cc_fprintd_device_get_name (fprintd_device), dbus_error_to_human (self, error));
        g_warning ("Releasing device %s failed: %s", cc_fprintd_device_get_name (self->device), error->message);

        notify_error (self, error_message);
        return;
    }

    remove_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED);
}

static void
release_device (CcFingerprintDialog *self)
{
    if (!self->device || !(self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED))
        return;

    disconnect_device_signals (self);

    cc_fprintd_device_call_release (self->device, G_DBUS_CALL_FLAGS_NONE, -1, self->cancellable, release_device_cb,
                                    self);
}

static void
on_device_signal (CcFingerprintDialog *self, gchar *sender_name, gchar *signal_name, GVariant *parameters,
                  gpointer user_data)
{
    if (g_str_equal (signal_name, "EnrollStatus")) {
        const char *result;
        gboolean done;

        if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sb)"))) {
            g_warning ("Unexpected enroll parameters type %s", g_variant_get_type_string (parameters));
            return;
        }

        g_variant_get (parameters, "(&sb)", &result, &done);
        handle_enroll_signal (self, result, done);
    }
}

static void claim_device (CcFingerprintDialog *self);

static void
on_device_owner_changed (CcFprintdDevice *device, GParamSpec *spec, CcFingerprintDialog *self)
{
    g_autofree char *name_owner = NULL;

    name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (device));

    if (!name_owner) {
        if (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED) {
            disconnect_device_signals (self);

            if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING) {
                set_enroll_result_message (self, ENROLL_STATE_ERROR,
                                           C_("Fingerprint enroll state",
                                            "Problem Reading Device"));
            }

            remove_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED);
            claim_device (self);
        }
    }
}

static void
claim_device_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_autoptr(GError) error = NULL;
    g_autoptr(DialogStateRemover) state_remover = NULL;
    CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
    CcFingerprintDialog *self = user_data;

    cc_fprintd_device_call_claim_finish (fprintd_device, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    state_remover = auto_state_remover (self, DIALOG_STATE_DEVICE_CLAIMING);

    if (error) {
        g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);
        g_autofree char *error_message = NULL;

        if (dbus_error && g_str_equal (dbus_error, CC_FPRINTD_NAME ".Error.AlreadyInUse")
            && (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED))
            return;

        error_message =
            g_strdup_printf (_("Failed to claim fingerprint device %s: %s"), cc_fprintd_device_get_name (self->device),
                               dbus_error_to_human (self, error));
        g_warning ("Claiming device %s failed: %s", cc_fprintd_device_get_name (self->device), error->message);
        notify_error (self, error_message);
        return;
    }

    if (!add_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMED))
        return;

    gtk_widget_set_sensitive (self->prints_manager, TRUE);
    update_prints_store (self);
    self->device_signal_id =
        g_signal_connect_object (self->device, "g-signal", G_CALLBACK (on_device_signal), self, G_CONNECT_SWAPPED);
    self->device_name_owner_id =
        g_signal_connect_object (self->device, "notify::g-name-owner", G_CALLBACK (on_device_owner_changed), self, 0);

    self->device_finger_status_id = g_signal_connect_object (
        self->device, "notify::finger-present", G_CALLBACK (on_finger_present_cb), self, G_CONNECT_SWAPPED);
}

static void
claim_device (CcFingerprintDialog *self)
{
    ActUser *user;

    g_return_if_fail (!(self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED));

    if (!add_dialog_state (self, DIALOG_STATE_DEVICE_CLAIMING))
        return;

    user = cc_fingerprint_manager_get_user (self->manager);
    gtk_widget_set_sensitive (self->prints_manager, FALSE);

    cc_fprintd_device_call_claim (self->device, act_user_get_user_name (user),
                                  G_DBUS_CALL_FLAGS_ALLOW_INTERACTIVE_AUTHORIZATION, -1, self->cancellable,
                                  claim_device_cb, self);
}

static void
on_stack_child_changed (CcFingerprintDialog *self)
{
    GtkWidget *visible_child = gtk_stack_get_visible_child (self->stack);
    GtkStackPage *visible_page = gtk_stack_get_page (self->stack, visible_child);
    const gchar *title = gtk_stack_page_get_title (visible_page);

    g_debug ("Fingerprint dialog child changed: %s", gtk_stack_get_visible_child_name (self->stack));

    /* Set an empty string for status pages (stack pages with no title). */
    adw_dialog_set_title (ADW_DIALOG (self), title ? title : "");

    gtk_widget_set_visible (GTK_WIDGET (self->back_button), FALSE);
    gtk_widget_set_visible (GTK_WIDGET (self->done_button), FALSE);

    if (visible_child == self->prints_manager || visible_child == self->no_fingerprints_enrolled_page) {
        gtk_widget_set_visible (GTK_WIDGET (self->back_button), have_multiple_devices (self));

        if (!(self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED)) {
            claim_device (self);
        } else if (visible_child == self->prints_manager) {
            update_prints_store (self);
        }
    } else if (visible_child == self->enrollment_view) {
        gtk_widget_set_visible (GTK_WIDGET (self->done_button), FALSE);
    } else if (visible_child == self->finger_selection_page) { // Do nothing here.
    } else {
        release_device (self);
        g_clear_object (&self->device);
    }
}

static void
cc_fingerprint_dialog_init (CcFingerprintDialog *self)
{
    self->cancellable = g_cancellable_new ();

    gtk_widget_init_template (GTK_WIDGET (self));

    self->fingerprints_store = g_list_store_new (GTK_TYPE_STRING_OBJECT);
    adw_preferences_group_bind_model (self->prints_group, G_LIST_MODEL (self->fingerprints_store),
                                      (GtkListBoxCreateWidgetFunc) create_fingerprint_row, self, NULL);

    self->right_hand_finger_options = g_list_store_new (GTK_TYPE_STRING_OBJECT);
    adw_preferences_group_bind_model (self->right_hand_finger_group, G_LIST_MODEL (self->right_hand_finger_options),
                                      (GtkListBoxCreateWidgetFunc) create_finger_option_row, self, NULL);
    self->left_hand_finger_options = g_list_store_new (GTK_TYPE_STRING_OBJECT);
    adw_preferences_group_bind_model (self->left_hand_finger_group, G_LIST_MODEL (self->left_hand_finger_options),
                                      (GtkListBoxCreateWidgetFunc) create_finger_option_row, self, NULL);

    on_stack_child_changed (self);
    g_signal_connect_object (self->stack, "notify::visible-child", G_CALLBACK (on_stack_child_changed), self,
                             G_CONNECT_SWAPPED);
}

static void
select_device_row (AdwActionRow *row, gpointer user_data)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);
    CcFprintdDevice *device = g_object_get_data (G_OBJECT (row), "device");

    g_return_if_fail (CC_FPRINTD_DEVICE (device));

    g_set_object (&self->device, device);
    gtk_stack_set_visible_child (self->stack, self->prints_manager);
}

static void
on_devices_list (GObject *object, GAsyncResult *res, gpointer user_data)
{
    g_autolist(CcFprintdDevice) fprintd_devices = NULL;
    g_autoptr(DialogStateRemover) state_remover = NULL;
    g_autoptr(GError) error = NULL;
    CcFingerprintManager *fingerprint_manager = CC_FINGERPRINT_MANAGER (object);
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);

    fprintd_devices = cc_fingerprint_manager_get_devices_finish (fingerprint_manager, res, &error);

    if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

    state_remover = auto_state_remover (self, DIALOG_STATE_DEVICES_LISTING);

    if (fprintd_devices == NULL) {
        if (error) {
            g_autofree char *error_message = NULL;

            error_message =
                g_strdup_printf (_("Failed to get fingerprint devices: %s"), dbus_error_to_human (self, error));
            g_warning ("Retrieving fingerprint devices failed: %s", error->message);
            notify_error (self, error_message);
        }

        gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->no_devices_found));
    } else if (fprintd_devices->next == NULL) {
        /* We have just one device... Skip devices selection */
        self->device = g_object_ref (fprintd_devices->data);
        gtk_stack_set_visible_child (self->stack, self->prints_manager);
    } else {
        GList *l;

        for (l = fprintd_devices; l; l = l->next) {
            CcFprintdDevice *device = l->data;
            CcListRow *device_row;

            device_row =
                g_object_new (CC_TYPE_LIST_ROW, "show-arrow", TRUE, "title", cc_fprintd_device_get_name (device), NULL);

            adw_preferences_group_add (self->devices_list, GTK_WIDGET (device_row));
            g_signal_connect (device_row, "activated", G_CALLBACK (select_device_row), self);
            g_object_set_data_full (G_OBJECT (device_row), "device", g_object_ref (device), g_object_unref);
        }

        gtk_stack_set_visible_child (self->stack, self->device_selector);
    }
}

static void
cc_fingerprint_dialog_constructed (GObject *object)
{
    CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

    G_OBJECT_CLASS (cc_fingerprint_dialog_parent_class)->constructed (object);

    bindtextdomain ("fprintd", GNOMELOCALEDIR);
    bind_textdomain_codeset ("fprintd", "UTF-8");

    add_dialog_state (self, DIALOG_STATE_DEVICES_LISTING);
    cc_fingerprint_manager_get_devices (self->manager, self->cancellable, on_devices_list, self);
}

static void
back_button_clicked_cb (CcFingerprintDialog *self)
{
    if (gtk_stack_get_visible_child (self->stack) == self->prints_manager) {
        gtk_stack_set_visible_child (self->stack, self->device_selector);
        return;
    }

    g_return_if_reached ();
}

static void
cancel_button_clicked_cb (CcFingerprintDialog *self)
{
    if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING) {
        g_cancellable_cancel (self->cancellable);
        g_set_object (&self->cancellable, g_cancellable_new ());

        g_debug ("Cancelling enroll operation");
        enroll_stop (self);
    } else {
        gtk_stack_set_visible_child (self->stack, self->prints_manager);
    }
}

static void
done_button_clicked_cb (CcFingerprintDialog *self)
{
    g_return_if_fail (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING);

    g_debug ("Completing enroll operation");
    enroll_stop (self);
}

static void
on_dialog_closed_cb (CcFingerprintDialog *self)
{
    cc_fingerprint_manager_update_state (self->manager, NULL, NULL);

    if (self->device && (self->dialog_state & DIALOG_STATE_DEVICE_CLAIMED)) {
        disconnect_device_signals (self);

        if (self->dialog_state & DIALOG_STATE_DEVICE_ENROLLING) {
            cc_fprintd_device_call_enroll_stop_sync (self->device, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
        }

        cc_fprintd_device_call_release (self->device, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    }

    g_clear_object (&self->manager);
    g_clear_object (&self->device);
    g_clear_pointer (&self->enrolled_fingers, g_strfreev);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
}

static void
cc_fingerprint_dialog_class_init (CcFingerprintDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/control-center/system/users/cc-fingerprint-dialog.ui");

    object_class->constructed = cc_fingerprint_dialog_constructed;
    object_class->get_property = cc_fingerprint_dialog_get_property;
    object_class->set_property = cc_fingerprint_dialog_set_property;

    properties[PROP_MANAGER] =
        g_param_spec_object ("fingerprint-manager", NULL, NULL, CC_TYPE_FINGERPRINT_MANAGER,
                             G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, add_finger_button);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, back_button);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, device_selector);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, devices_list);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, done_button);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enrollment_view);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, error_page);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, left_hand_finger_group);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, finger_selection_page);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, no_devices_found);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, no_fingerprints_enrolled_page);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, prints_group);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, prints_manager);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, progress_bar);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, right_hand_finger_group);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, spinner);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, stack);
    gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, titlebar);

    gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
    gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
    gtk_widget_class_bind_template_callback (widget_class, done_button_clicked_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_add_fingerprint_button_activated_cb);
    gtk_widget_class_bind_template_callback (widget_class, on_dialog_closed_cb);
}
