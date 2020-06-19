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

#include <glib/gi18n.h>
#include <cairo/cairo.h>

#include "cc-fingerprint-dialog.h"

#include "cc-fingerprint-manager.h"
#include "cc-fprintd-generated.h"
#include "cc-list-row.h"

#include "config.h"

/* Translate fprintd strings */
#define TR(s) dgettext ("fprintd", s)
#include "fingerprint-strings.h"

struct _CcFingerprintDialog
{
  GtkWindow parent_instance;

  GtkButton      *back_button;
  GtkButton      *cancel_button;
  GtkButton      *delete_prints_button;
  GtkButton      *done_button;
  GtkContainer   *add_print_popover_box;
  GtkEntry       *enroll_print_entry;
  GtkFlowBox     *prints_gallery;
  GtkHeaderBar   *titlebar;
  GtkImage       *enroll_result_image;
  GtkLabel       *enroll_message;
  GtkLabel       *enroll_result_message;
  GtkLabel       *infobar_error;
  GtkLabel       *title;
  GtkListBox     *devices_list;
  GtkPopoverMenu *add_print_popover;
  GtkPopoverMenu *print_popover;
  GtkSpinner     *spinner;
  GtkStack       *stack;
  GtkWidget      *add_print_icon;
  GtkWidget      *delete_confirmation_infobar;
  GtkWidget      *device_selector;
  GtkWidget      *enroll_print_bin;
  GtkWidget      *enroll_result_icon;
  GtkWidget      *enrollment_view;
  GtkWidget      *error_infobar;
  GtkWidget      *no_devices_found;
  GtkWidget      *prints_manager;

  CcFingerprintManager *manager;
  CcFprintdDevice      *device;
  gboolean              device_claimed;
  gulong                device_signal_id;
  gulong                device_name_owner_id;
  GCancellable         *cancellable;
  GStrv                 enrolled_fingers;
  const char           *enrolling_finger;
  guint                 enroll_stages_passed;
  guint                 enroll_stage_passed_id;
  gdouble               enroll_progress;
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

G_DEFINE_TYPE (CcFingerprintDialog, cc_fingerprint_dialog, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_MANAGER,
  N_PROPS
};

#define N_VALID_FINGERS G_N_ELEMENTS (FINGER_IDS) - 1
/* The order of the fingers here will affect the UI order */
const char * FINGER_IDS[] = {
  "right-index-finger",
  "left-index-finger",
  "right-thumb",
  "right-middle-finger",
  "right-ring-finger",
  "right-little-finger",
  "left-thumb",
  "left-middle-finger",
  "left-ring-finger",
  "left-little-finger",
  "any",
};

typedef enum {
  ENROLL_STATE_NORMAL,
  ENROLL_STATE_SUCCESS,
  ENROLL_STATE_WARNING,
  ENROLL_STATE_ERROR,
  ENROLL_STATE_COMPLETED,
  N_ENROLL_STATES,
} EnrollState;

const char * ENROLL_STATE_CLASSES[N_ENROLL_STATES] = {
  "",
  "success",
  "warning",
  "error",
  "completed",
};

static GParamSpec *properties[N_PROPS];

CcFingerprintDialog *
cc_fingerprint_dialog_new (CcFingerprintManager *manager)
{
  return g_object_new (CC_TYPE_FINGERPRINT_DIALOG,
                       "fingerprint-manager", manager,
                       NULL);
}

static void
disconnect_device_signals (CcFingerprintDialog *self)
{
  if (!self->device)
    return;

  if (self->device_signal_id)
    {
      g_signal_handler_disconnect (self->device, self->device_signal_id);
      self->device_signal_id = 0;
    }

  if (self->device_name_owner_id)
    {
      g_signal_handler_disconnect (self->device, self->device_name_owner_id);
      self->device_name_owner_id = 0;
    }
}

static void
cc_fingerprint_dialog_dispose (GObject *object)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  g_clear_handle_id (&self->enroll_stage_passed_id, g_source_remove);

  if (self->device && self->device_claimed)
    {
      disconnect_device_signals (self);

      if (self->enrolling_finger)
        cc_fprintd_device_call_enroll_stop_sync (self->device, NULL, NULL);
      cc_fprintd_device_call_release (self->device, NULL, NULL, NULL);
    }

  g_clear_object (&self->manager);
  g_clear_object (&self->device);
  g_clear_pointer (&self->enrolled_fingers, g_strfreev);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (cc_fingerprint_dialog_parent_class)->dispose (object);
}

static void
cc_fingerprint_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_value_set_object (value, self->manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_fingerprint_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_MANAGER:
      g_set_object (&self->manager, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
notify_error (CcFingerprintDialog *self,
              const char          *error_message)
{
  if (error_message)
    gtk_label_set_label (self->infobar_error, error_message);

  gtk_widget_set_visible (self->error_infobar, error_message != NULL);
}

static gboolean
fingerprint_icon_draw (GtkWidget *widget,
                       cairo_t   *cr,
                       gdouble   *progress_data)
{
  gdouble progress = 0.0f;

  if (progress_data)
    progress = *progress_data;

  if (G_APPROX_VALUE (progress, 0.f, FLT_EPSILON) || progress > 1)
    return FALSE;

  GTK_WIDGET_GET_CLASS (widget)->draw (widget, cr);

  if (progress > 0)
    {
      g_autoptr(GdkRGBA) outline_color = NULL;
      GtkStyleContext *context;
      GtkStateFlags state;
      int outline_width;
      int outline_offset;
      int width;
      int height;
      int radius;
      int delta;

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_save (context);

      state = gtk_style_context_get_state (context);

      gtk_style_context_add_class (context, "progress");
      gtk_style_context_get (context, state,
                             "outline-width", &outline_width,
                             "outline-offset", &outline_offset,
                             "outline-color", &outline_color,
                             NULL);

      width = gtk_widget_get_allocated_width (widget);
      height = gtk_widget_get_allocated_height (widget);
      radius = MIN (width / 2, height / 2) + outline_offset;
      delta = radius - outline_width / 2;

      cairo_arc (cr, width / 2., height / 2., delta,
                 1.5 * G_PI, (1.5 + progress * 2) * G_PI);
      gdk_cairo_set_source_rgba (cr, outline_color);

      cairo_set_line_width (cr, MIN (outline_width, radius));
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);
      cairo_stroke (cr);

      gtk_style_context_restore (context);
    }

  return TRUE;
}

static GtkWidget *
fingerprint_icon_new (const char *icon_name,
                      const char *label_text,
                      GType       icon_widget_type,
                      gpointer    progress_data,
                      GtkWidget **out_icon,
                      GtkWidget **out_label)
{
  GtkStyleContext *context;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *image;
  GtkWidget *icon_widget;

  g_return_val_if_fail (g_type_is_a (icon_widget_type, GTK_TYPE_WIDGET), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_name (box, "fingerprint-box");
  gtk_widget_set_halign (box, GTK_ALIGN_CENTER);

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_DND);

  if (icon_widget_type == GTK_TYPE_IMAGE)
    icon_widget = image;
  else
    icon_widget = g_object_new (icon_widget_type, NULL);

  if (progress_data)
    g_signal_connect (image, "draw", G_CALLBACK (fingerprint_icon_draw),
                      progress_data);

  if (g_type_is_a (icon_widget_type, GTK_TYPE_BUTTON))
    {
      gtk_button_set_image (GTK_BUTTON (icon_widget), image);
      gtk_button_set_relief (GTK_BUTTON (icon_widget), GTK_RELIEF_NONE);
      gtk_widget_set_can_focus (icon_widget, FALSE);
    }

  gtk_widget_set_halign (icon_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (icon_widget, GTK_ALIGN_CENTER);
  gtk_widget_set_name (icon_widget, "fingerprint-image");

  gtk_container_add (GTK_CONTAINER (box), icon_widget);

  context = gtk_widget_get_style_context (icon_widget);
  gtk_style_context_add_class (context, "fingerprint-image");

  label = gtk_label_new_with_mnemonic (label_text);
  gtk_container_add (GTK_CONTAINER (box), label);

  context = gtk_widget_get_style_context (box);
  gtk_style_context_add_class (context, "fingerprint-icon");

  if (out_icon)
    *out_icon = icon_widget;

  if (out_label)
    *out_label = label;

  return box;
}

static GtkWidget *
fingerprint_menu_button (const char *icon_name,
                         const char *label_text)
{
  GtkWidget *flowbox_child;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *box;

  box = fingerprint_icon_new (icon_name, label_text, GTK_TYPE_MENU_BUTTON, NULL,
                              &button, &label);

  flowbox_child = gtk_flow_box_child_new ();
  gtk_widget_set_focus_on_click (flowbox_child, FALSE);
  gtk_widget_set_name (flowbox_child, "fingerprint-flowbox");

  gtk_container_add (GTK_CONTAINER (flowbox_child), box);

  g_object_set_data (G_OBJECT (flowbox_child), "button", button);
  g_object_set_data (G_OBJECT (flowbox_child), "icon",
                     gtk_button_get_image (GTK_BUTTON (button)));
  g_object_set_data (G_OBJECT (flowbox_child), "label", label);
  g_object_set_data (G_OBJECT (button), "flowbox-child", flowbox_child);

  return flowbox_child;
}

static gboolean
prints_visibility_filter (GtkFlowBoxChild *child,
                          gpointer         user_data)
{
  CcFingerprintDialog *self = user_data;
  const char *finger_id;

  if (gtk_stack_get_visible_child (self->stack) != self->prints_manager)
    return FALSE;

  finger_id = g_object_get_data (G_OBJECT (child), "finger-id");

  if (!finger_id)
    return TRUE;

  if (!self->enrolled_fingers)
    return FALSE;

  return g_strv_contains ((const gchar **) self->enrolled_fingers, finger_id);
}

static void
update_prints_to_add_visibility (CcFingerprintDialog *self)
{
  g_autoptr(GList) print_buttons = NULL;
  GList *l;
  guint i;

  print_buttons = gtk_container_get_children (self->add_print_popover_box);

  for (i = 0, l = print_buttons; i < N_VALID_FINGERS && l; ++i, l = l->next)
    {
      GtkWidget *button = l->data;
      gboolean enrolled;

      enrolled = self->enrolled_fingers &&
                 g_strv_contains ((const gchar **) self->enrolled_fingers,
                                  FINGER_IDS[i]);

      gtk_widget_set_visible (button, !enrolled);
    }
}

static void
update_prints_visibility (CcFingerprintDialog *self)
{
  update_prints_to_add_visibility (self);

  gtk_flow_box_invalidate_filter (self->prints_gallery);
}

static void
list_enrolled_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_auto(GStrv) enrolled_fingers = NULL;
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;
  guint n_enrolled_fingers = 0;

  cc_fprintd_device_call_list_enrolled_fingers_finish (fprintd_device,
                                                       &enrolled_fingers,
                                                       res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  gtk_spinner_stop (self->spinner);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_print_icon), TRUE);

  if (self->device_claimed)
    gtk_widget_set_sensitive (GTK_WIDGET (self->prints_manager), TRUE);

  if (error)
    {
      g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);

      if (!dbus_error || !g_str_has_suffix (dbus_error, ".Error.NoEnrolledPrints"))
        {
          g_autofree char *error_message = NULL;

          g_dbus_error_strip_remote_error (error);
          error_message = g_strdup_printf (_("Failed to list fingerprints: %s"),
                                           error->message);
          g_warning ("Listing of fingerprints on device %s failed: %s",
                     cc_fprintd_device_get_name (self->device), error->message);
          notify_error (self, error_message);
          return;
        }
    }
  else
    {
      n_enrolled_fingers = g_strv_length (enrolled_fingers);
    }

  self->enrolled_fingers = g_steal_pointer (&enrolled_fingers);
  gtk_flow_box_set_max_children_per_line (self->prints_gallery,
                                          MIN (3, n_enrolled_fingers + 1));

  update_prints_visibility (self);

  if (n_enrolled_fingers == N_VALID_FINGERS)
    gtk_widget_set_sensitive (self->add_print_icon, FALSE);

  if (n_enrolled_fingers > 0)
    gtk_widget_show (GTK_WIDGET (self->delete_prints_button));
}

static void
update_prints_store (CcFingerprintDialog *self)
{
  ActUser *user;

  g_assert_true (CC_FPRINTD_IS_DEVICE (self->device));

  gtk_spinner_start (self->spinner);
  gtk_widget_set_sensitive (GTK_WIDGET (self->add_print_icon), FALSE);
  gtk_widget_hide (GTK_WIDGET (self->delete_prints_button));

  g_clear_pointer (&self->enrolled_fingers, g_strfreev);

  user = cc_fingerprint_manager_get_user (self->manager);
  cc_fprintd_device_call_list_enrolled_fingers (self->device,
                                                act_user_get_user_name (user),
                                                self->cancellable,
                                                list_enrolled_cb,
                                                self);
}

static void
delete_prints_cb (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_delete_enrolled_fingers2_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error)
    {
      g_autofree char *error_message = NULL;

      g_dbus_error_strip_remote_error (error);
      error_message = g_strdup_printf (_("Failed to delete saved fingerprints: %s"),
                                       error->message);
      g_warning ("Deletion of fingerprints on device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);
    }

  update_prints_store (self);
  cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
}

static void
delete_enrolled_prints (CcFingerprintDialog *self)
{
  g_return_if_fail (self->device_claimed);

  gtk_widget_set_sensitive (GTK_WIDGET (self->prints_manager), FALSE);
  gtk_spinner_start (self->spinner);

  cc_fprintd_device_call_delete_enrolled_fingers2 (self->device,
                                                   self->cancellable,
                                                   delete_prints_cb,
                                                   self);
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

static gboolean
have_multiple_devices (CcFingerprintDialog *self)
{
  g_autoptr(GList) devices_rows = NULL;

  devices_rows = gtk_container_get_children (GTK_CONTAINER (self->devices_list));

  return devices_rows && devices_rows->next;
}

static void
set_enroll_result_message (CcFingerprintDialog *self,
                           EnrollState          enroll_state,
                           const char          *message)
{
  GtkStyleContext *style_context;
  const char *icon_name;
  guint i;

  g_return_if_fail (enroll_state >= 0 && enroll_state < N_ENROLL_STATES);

  style_context = gtk_widget_get_style_context (self->enroll_result_icon);

  switch (enroll_state)
    {
      case ENROLL_STATE_WARNING:
      case ENROLL_STATE_ERROR:
        icon_name = "fingerprint-detection-warning-symbolic";
        break;
      case ENROLL_STATE_COMPLETED:
        icon_name = "fingerprint-detection-complete-symbolic";
        break;
      default:
        icon_name = "fingerprint-detection-symbolic";
    }

  for (i = 0; i < N_ENROLL_STATES; ++i)
    gtk_style_context_remove_class (style_context, ENROLL_STATE_CLASSES[i]);

  gtk_style_context_add_class (style_context, ENROLL_STATE_CLASSES[enroll_state]);

  gtk_image_set_from_icon_name (self->enroll_result_image, icon_name, GTK_ICON_SIZE_DND);
  gtk_label_set_label (self->enroll_result_message, message);
}

static gboolean
stage_passed_timeout_cb (gpointer user_data)
{
  CcFingerprintDialog *self = user_data;

  set_enroll_result_message (self, ENROLL_STATE_NORMAL, NULL);
  self->enroll_stage_passed_id = 0;

  return FALSE;
}

static void
handle_enroll_signal (CcFingerprintDialog *self,
                      const char          *result,
                      gboolean             done)
{
  gboolean completed;

  g_return_if_fail (self->enrolling_finger);

  g_debug ("Device enroll result message: %s, done: %d", result, done);

  completed = g_str_equal (result, "enroll-completed");
  g_clear_handle_id (&self->enroll_stage_passed_id, g_source_remove);

  if (g_str_equal (result, "enroll-stage-passed") || completed)
    {
      guint enroll_stages;

      enroll_stages = cc_fprintd_device_get_num_enroll_stages (self->device);

      self->enroll_stages_passed++;

      if (enroll_stages > 0)
        self->enroll_progress =
          MIN (1.0f, self->enroll_stages_passed / (double) enroll_stages);
      else
        g_warning ("The device %s requires an invalid number of enroll stages (%u)",
                   cc_fprintd_device_get_name (self->device), enroll_stages);

      g_debug ("Enroll state passed, %u/%u (%.2f%%)",
               self->enroll_stages_passed, (guint) enroll_stages,
               self->enroll_progress);

      if (!completed)
        {
          set_enroll_result_message (self, ENROLL_STATE_SUCCESS, NULL);

          self->enroll_stage_passed_id =
            g_timeout_add (750, stage_passed_timeout_cb, self);
        }
      else
        {
          if (!G_APPROX_VALUE (self->enroll_progress, 1.0f, FLT_EPSILON))
            {
              g_warning ("Device marked enroll as completed, but progress is at %.2f",
                         self->enroll_progress);
              self->enroll_progress = 1.0f;
            }
        }
    }
  else if (!done)
    {
      const char *scan_type;
      const char *message;
      gboolean is_swipe;

      scan_type = cc_fprintd_device_get_scan_type (self->device);
      is_swipe = g_str_equal (scan_type, "swipe");

      message = TR (enroll_result_str_to_msg (result, is_swipe));
      set_enroll_result_message (self, ENROLL_STATE_NORMAL, message);
    }

  if (done)
    {
      if (completed)
        {
          /* TRANSLATORS: This is the message shown when the fingerprint
           * enrollment has been completed successfully */
          set_enroll_result_message (self, ENROLL_STATE_COMPLETED,
                                     C_("Fingerprint enroll state", "Complete"));
          gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), FALSE);
          gtk_widget_set_sensitive (GTK_WIDGET (self->done_button), TRUE);
          gtk_widget_grab_focus (GTK_WIDGET (self->done_button));
        }
      else
        {
          const char *message;

          if (g_str_equal (result, "enroll-disconnected"))
            message = _("Fingerprint device disconnected");
          else if (g_str_equal (result, "enroll-data-full"))
            message = _("Fingerprint device storage is full");
          else
            message = _("Failed to enroll new fingerprint");

          self->enrolling_finger = NULL;
          set_enroll_result_message (self, ENROLL_STATE_WARNING, message);
        }
    }
}

static void
enroll_start_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_enroll_start_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  gtk_spinner_stop (self->spinner);

  if (error)
    {
      g_autofree char *error_message = NULL;

      self->enrolling_finger = NULL;

      g_dbus_error_strip_remote_error (error);
      error_message = g_strdup_printf (_("Failed to start enrollment: %s"),
                                       error->message);
      g_warning ("Enrollment on device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);

      set_enroll_result_message (self, ENROLL_STATE_ERROR,
                                 C_("Fingerprint enroll state",
                                    "Failed to enroll new fingerprint"));
      gtk_widget_set_sensitive (self->enrollment_view, FALSE);

      return;
    }
}

static void
enroll_stop_cb (GObject      *object,
                GAsyncResult *res,
                gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_enroll_stop_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  self->enrolling_finger = NULL;
  gtk_spinner_stop (self->spinner);
  gtk_widget_set_sensitive (self->enrollment_view, TRUE);
  gtk_stack_set_visible_child (self->stack, self->prints_manager);

  if (error)
    {
      g_autofree char *error_message = NULL;

      g_dbus_error_strip_remote_error (error);
      error_message = g_strdup_printf (_("Failed to stop enrollment: %s"),
                                       error->message);
      g_warning ("Stopping enrollment on device %s failed: %s",
                 cc_fprintd_device_get_name (self->device), error->message);
      notify_error (self, error_message);

      return;
    }

  cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
}

static void
enroll_stop (CcFingerprintDialog *self)
{
  g_return_if_fail (self->enrolling_finger);

  gtk_spinner_start (self->spinner);
  gtk_widget_set_sensitive (self->enrollment_view, FALSE);
  cc_fprintd_device_call_enroll_stop (self->device, self->cancellable,
                                      enroll_stop_cb, self);
}

static char *
get_enrollment_string (CcFingerprintDialog *self,
                       const char          *finger_id)
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

  ret = TR (finger_str_to_msg (finger_id, device_name, is_swipe));

  if (ret)
    return ret;

  return g_strdup (_("Repeatedly lift and place your finger on the reader to enroll your fingerprint"));
}

static void
enroll_finger (CcFingerprintDialog *self,
               const char          *finger_id)
{
  g_auto(GStrv) tmp_finger_name = NULL;
  g_autofree char *enroll_message = NULL;
  g_autofree char *finger_name = NULL;

  g_return_if_fail (finger_id);

  self->enrolling_finger = finger_id;
  self->enroll_progress = 0;
  self->enroll_stages_passed = 0;

  g_debug ("Enrolling finger %s", finger_id);

  enroll_message = TR (get_enrollment_string (self, finger_id));
  tmp_finger_name = g_strsplit (get_finger_name (finger_id), "_", -1);
  finger_name = g_strjoinv ("", tmp_finger_name);

  set_enroll_result_message (self, ENROLL_STATE_NORMAL, NULL);
  gtk_stack_set_visible_child (self->stack, self->enrollment_view);
  gtk_label_set_label (self->enroll_message, enroll_message);
  gtk_entry_set_text (self->enroll_print_entry, finger_name);
  gtk_spinner_start (self->spinner);

  cc_fprintd_device_call_enroll_start (self->device, finger_id, self->cancellable,
                                       enroll_start_cb, self);
}

static void
populate_enrollment_view (CcFingerprintDialog *self)
{
  GtkStyleContext *style_context;

  self->enroll_result_icon =
    fingerprint_icon_new ("fingerprint-detection-symbolic",
                          NULL,
                          GTK_TYPE_IMAGE,
                          &self->enroll_progress,
                          (GtkWidget **) &self->enroll_result_image,
                          (GtkWidget **) &self->enroll_result_message);

  gtk_container_add (GTK_CONTAINER (self->enroll_print_bin), self->enroll_result_icon);

  style_context = gtk_widget_get_style_context (self->enroll_result_icon);
  gtk_style_context_add_class (style_context,  "enroll-status");

  gtk_widget_show_all (self->enroll_print_bin);
}

static void
reenroll_finger_cb (CcFingerprintDialog *self)
{
  GtkWidget *button;
  GtkWidget *flowbox_child;
  const char *finger_id;

  button = gtk_popover_get_relative_to (GTK_POPOVER (self->print_popover));
  flowbox_child = g_object_get_data (G_OBJECT (button), "flowbox-child");
  finger_id = g_object_get_data (G_OBJECT (flowbox_child), "finger-id");

  enroll_finger (self, finger_id);
}

static void
on_print_activated_cb (GtkFlowBox          *flowbox,
                       GtkFlowBoxChild     *child,
                       CcFingerprintDialog *self)
{
  GtkWidget *selected_button;

  selected_button = g_object_get_data (G_OBJECT (child), "button");
  gtk_button_clicked (GTK_BUTTON (selected_button));
}

static void
on_enroll_cb (CcFingerprintDialog *self,
              GtkModelButton      *button)
{
  const char *finger_id;

  finger_id = g_object_get_data (G_OBJECT (button), "finger-id");
  enroll_finger (self, finger_id);
}

static void
populate_add_print_popover (CcFingerprintDialog *self)
{
  guint i;

  for (i = 0; i < N_VALID_FINGERS; ++i)
    {
      GtkWidget *finger_item;

      finger_item = gtk_model_button_new ();
      gtk_button_set_label (GTK_BUTTON (finger_item), get_finger_name (FINGER_IDS[i]));
      gtk_button_set_use_underline (GTK_BUTTON (finger_item), TRUE);
      g_object_set_data (G_OBJECT (finger_item), "finger-id", (gpointer) FINGER_IDS[i]);
      gtk_container_add (self->add_print_popover_box, finger_item);

      g_signal_connect_object (finger_item, "clicked", G_CALLBACK (on_enroll_cb),
                               self, G_CONNECT_SWAPPED);
    }
}

static void
populate_prints_gallery (CcFingerprintDialog *self)
{
  const char *add_print_label;
  GtkWidget *button;
  GtkStyleContext *style_context;
  guint i;

  g_return_if_fail (!GTK_IS_WIDGET (self->add_print_icon));

  for (i = 0; i < N_VALID_FINGERS; ++i)
    {
      GtkWidget *flowbox_child;

      flowbox_child = fingerprint_menu_button ("fingerprint-detection-symbolic",
                                               get_finger_name (FINGER_IDS[i]));

      button = g_object_get_data (G_OBJECT (flowbox_child), "button");

      gtk_menu_button_set_popover (GTK_MENU_BUTTON (button),
                                   GTK_WIDGET (self->print_popover));
      /* Move the popover on click, so we can just reuse the same instance */
      g_signal_connect_object (button, "clicked",
                               G_CALLBACK (gtk_popover_set_relative_to),
                               self->print_popover, G_CONNECT_SWAPPED);

      g_object_set_data (G_OBJECT (flowbox_child), "finger-id",
                         (gpointer) FINGER_IDS[i]);

      gtk_flow_box_insert (self->prints_gallery, flowbox_child, i);
    }

  /* TRANSLATORS: This is the label for the button to enroll a new finger */
  add_print_label = _("Scan new fingerprint");
  self->add_print_icon = fingerprint_menu_button ("list-add-symbolic",
                                                  add_print_label);
  style_context = gtk_widget_get_style_context (self->add_print_icon);
  gtk_style_context_add_class (style_context, "fingerprint-print-add");

  populate_add_print_popover (self);
  button = g_object_get_data (G_OBJECT (self->add_print_icon), "button");
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button),
                               GTK_WIDGET (self->add_print_popover));

  gtk_flow_box_insert (self->prints_gallery, self->add_print_icon, -1);
  gtk_flow_box_set_max_children_per_line (self->prints_gallery, 1);

  gtk_widget_show_all (GTK_WIDGET (self->prints_gallery));
  gtk_flow_box_set_filter_func (self->prints_gallery, prints_visibility_filter,
                                self, NULL);

  update_prints_visibility (self);
}

static void
release_device_cb (GObject      *object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_release_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error)
    {
      g_autofree char *error_message = NULL;

      g_dbus_error_strip_remote_error (error);
      error_message = g_strdup_printf (_("Failed to release fingerprint device %s: %s"),
                                       cc_fprintd_device_get_name (self->device),
                                       error->message);
      g_warning ("%s", error_message);

      notify_error (self, error_message);
      return;
    }

  self->device_claimed = FALSE;
}

static void
release_device (CcFingerprintDialog *self)
{
  if (!self->device || !self->device_claimed)
    return;

  disconnect_device_signals (self);

  cc_fprintd_device_call_release (self->device,
                                  self->cancellable,
                                  release_device_cb,
                                  self);
}

static void
on_device_signal (CcFingerprintDialog *self,
                  gchar               *sender_name,
                  gchar               *signal_name,
                  GVariant            *parameters,
                  gpointer             user_data)
{
  if (g_str_equal (signal_name, "EnrollStatus"))
    {
      const char *result;
      gboolean done;

      if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sb)")))
        {
          g_warning ("Unexpected enroll parameters type %s",
                     g_variant_get_type_string (parameters));
          return;
        }

      g_variant_get (parameters, "(&sb)", &result, &done);
      handle_enroll_signal (self, result, done);
    }
}

static void claim_device (CcFingerprintDialog *self);

static void
on_device_owner_changed (CcFprintdDevice     *device,
                         GParamSpec          *spec,
                         CcFingerprintDialog *self)
{
  g_autofree char *name_owner = NULL;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (device));

  if (!name_owner)
    {
      if (self->device_claimed)
        {
          disconnect_device_signals (self);

          if (self->enrolling_finger)
            {
              set_enroll_result_message (self, ENROLL_STATE_ERROR,
                                         C_("Fingerprint enroll state",
                                            "Problem Reading Device"));
              self->enrolling_finger = NULL;
            }

          self->device_claimed = FALSE;
          claim_device (self);
        }
    }
}

static void
claim_device_cb (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcFprintdDevice *fprintd_device = CC_FPRINTD_DEVICE (object);
  CcFingerprintDialog *self = user_data;

  cc_fprintd_device_call_claim_finish (fprintd_device, res, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error)
    {
      g_autofree char *dbus_error = g_dbus_error_get_remote_error (error);
      g_autofree char *error_message = NULL;

      if (dbus_error && g_str_has_suffix (dbus_error, ".Error.AlreadyInUse"))
        self->device_claimed = TRUE;

      g_dbus_error_strip_remote_error (error);
      error_message = g_strdup_printf (_("Failed to claim fingerprint device %s: %s"),
                                       cc_fprintd_device_get_name (self->device),
                                       error->message);
      g_warning ("%s", error_message);
      notify_error (self, error_message);
      return;
    }

  gtk_widget_set_sensitive (self->prints_manager, TRUE);
  self->device_claimed = TRUE;
  self->device_signal_id = g_signal_connect_object (self->device, "g-signal",
                                                    G_CALLBACK (on_device_signal),
                                                    self, G_CONNECT_SWAPPED);
  self->device_name_owner_id = g_signal_connect_object (self->device, "notify::g-name-owner",
                                                        G_CALLBACK (on_device_owner_changed),
                                                        self, 0);
}

static void
claim_device (CcFingerprintDialog *self)
{
  ActUser *user;

  user = cc_fingerprint_manager_get_user (self->manager);
  gtk_widget_set_sensitive (self->prints_manager, FALSE);

  cc_fprintd_device_call_claim (self->device,
                                act_user_get_user_name (user),
                                self->cancellable,
                                claim_device_cb,
                                self);
}

static void
on_stack_child_changed (CcFingerprintDialog *self)
{
  GtkWidget *visible_child = gtk_stack_get_visible_child (self->stack);

  g_debug ("Fingerprint dialog child changed: %s",
           gtk_stack_get_visible_child_name (self->stack));

  gtk_widget_hide (GTK_WIDGET (self->back_button));
  gtk_widget_hide (GTK_WIDGET (self->cancel_button));
  gtk_widget_hide (GTK_WIDGET (self->done_button));

  gtk_header_bar_set_show_close_button (self->titlebar, TRUE);
  gtk_flow_box_invalidate_filter (self->prints_gallery);

  if (visible_child == self->prints_manager)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->back_button),
                              have_multiple_devices (self));
      notify_error (self, NULL);
      update_prints_store (self);

      if (!self->device_claimed)
        claim_device (self);
    }
  else if (visible_child == self->enrollment_view)
    {
      gtk_header_bar_set_show_close_button (self->titlebar, FALSE);

      gtk_widget_show (GTK_WIDGET (self->cancel_button));
      gtk_widget_set_sensitive (GTK_WIDGET (self->cancel_button), TRUE);

      gtk_widget_show (GTK_WIDGET (self->done_button));
      gtk_widget_set_sensitive (GTK_WIDGET (self->done_button), FALSE);
    }
  else
    {
      release_device (self);
      g_clear_object (&self->device);
    }
}

static void
cc_fingerprint_dialog_init (CcFingerprintDialog *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  self->cancellable = g_cancellable_new ();

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/org/gnome/control-center/user-accounts/cc-fingerprint-dialog.css");
  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  on_stack_child_changed (self);
  g_signal_connect_object (self->stack, "notify::visible-child",
                           G_CALLBACK (on_stack_child_changed), self,
                           G_CONNECT_SWAPPED);

  g_object_bind_property (self->stack, "visible-child-name",
                          self->title, "label", G_BINDING_SYNC_CREATE);

  populate_prints_gallery (self);
  populate_enrollment_view (self);
}

static void
select_device_row (CcFingerprintDialog *self,
                   GtkListBoxRow       *row,
                   GtkListBox          *listbox)
{
  CcFprintdDevice *device = g_object_get_data (G_OBJECT (row), "device");

  g_return_if_fail (CC_FPRINTD_DEVICE (device));

  g_set_object (&self->device, device);
  gtk_stack_set_visible_child (self->stack, self->prints_manager);
}

static void
on_devices_list (GObject      *object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  g_autolist (CcFprintdDevice) fprintd_devices = NULL;
  g_autoptr(GError) error = NULL;
  CcFingerprintManager *fingerprint_manager = CC_FINGERPRINT_MANAGER (object);
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (user_data);

  fprintd_devices = cc_fingerprint_manager_get_devices_finish (fingerprint_manager,
                                                               res, &error);
  gtk_spinner_stop (self->spinner);

  if (fprintd_devices == NULL)
    {
      if (error)
        {
          g_autofree char *error_message = NULL;

          g_dbus_error_strip_remote_error (error);
          error_message = g_strdup_printf (_("Failed to get fingerprint devices: %s"),
                                           error->message);
          g_warning ("%s", error_message);
          notify_error (self, error_message);
        }

      gtk_stack_set_visible_child (self->stack, GTK_WIDGET (self->no_devices_found));
    }
  else if (fprintd_devices->next == NULL)
    {
      /* We have just one device... Skip devices selection */
      self->device = g_object_ref (fprintd_devices->data);
      gtk_stack_set_visible_child (self->stack, self->prints_manager);
    }
  else
    {
      GList *l;

      for (l = fprintd_devices; l; l = l->next)
        {
          CcFprintdDevice *device = l->data;
          CcListRow *device_row;

          device_row = g_object_new (CC_TYPE_LIST_ROW,
                                     "visible", TRUE,
                                     "icon-name", "go-next-symbolic",
                                     "title", cc_fprintd_device_get_name (device),
                                     NULL);

          gtk_list_box_insert (self->devices_list, GTK_WIDGET (device_row), -1);
          g_object_set_data_full (G_OBJECT (device_row), "device",
                                  g_object_ref (device), g_object_unref);
        }

      gtk_stack_set_visible_child (self->stack, self->device_selector);
    }
}

static void
cc_fingerprint_dialog_constructed (GObject *object)
{
  CcFingerprintDialog *self = CC_FINGERPRINT_DIALOG (object);

  bindtextdomain ("fprintd", GNOMELOCALEDIR);
  bind_textdomain_codeset ("fprintd", "UTF-8");

  gtk_spinner_start (self->spinner);
  cc_fingerprint_manager_get_devices (self->manager, self->cancellable,
                                      on_devices_list, self);
}

static void
back_button_clicked_cb (CcFingerprintDialog *self)
{
  if (gtk_stack_get_visible_child (self->stack) == self->prints_manager)
    {
      notify_error (self, NULL);
      gtk_stack_set_visible_child (self->stack, self->device_selector);
      return;
    }

  g_return_if_reached ();
}

static void
confirm_deletion_button_clicked_cb (CcFingerprintDialog *self)
{
  gtk_widget_hide (self->delete_confirmation_infobar);
  delete_enrolled_prints (self);
}

static void
cancel_deletion_button_clicked_cb (CcFingerprintDialog *self)
{
  gtk_widget_set_sensitive (self->prints_manager, TRUE);
  gtk_widget_hide (self->delete_confirmation_infobar);
}

static void
delete_prints_button_clicked_cb (CcFingerprintDialog *self)
{
  gtk_widget_set_sensitive (self->prints_manager, FALSE);
  gtk_widget_show (self->delete_confirmation_infobar);
}

static void
cancel_button_clicked_cb (CcFingerprintDialog *self)
{
  if (self->enrolling_finger)
    {
      g_cancellable_cancel (self->cancellable);
      g_set_object (&self->cancellable, g_cancellable_new ());

      g_debug ("Cancelling enroll operation");
      enroll_stop (self);
    }
  else
    {
      gtk_stack_set_visible_child (self->stack, self->prints_manager);
    }
}

static void
done_button_clicked_cb (CcFingerprintDialog *self)
{
  g_return_if_fail (self->enrolling_finger);

  g_debug ("Completeing enroll operation");
  enroll_stop (self);
}

static void
fingerprint_dialog_delete_cb (CcFingerprintDialog *self)
{
  cc_fingerprint_manager_update_state (self->manager, NULL, NULL);
  gtk_widget_destroy (GTK_WIDGET (self));
}

static void
cc_fingerprint_dialog_class_init (CcFingerprintDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class,
    "/org/gnome/control-center/user-accounts/cc-fingerprint-dialog.ui");

  object_class->constructed = cc_fingerprint_dialog_constructed;
  object_class->dispose = cc_fingerprint_dialog_dispose;
  object_class->get_property = cc_fingerprint_dialog_get_property;
  object_class->set_property = cc_fingerprint_dialog_set_property;

  properties[PROP_MANAGER] =
    g_param_spec_object ("fingerprint-manager",
                         "FingerprintManager",
                         "The CC fingerprint manager",
                         CC_TYPE_FINGERPRINT_MANAGER,
                         G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, add_print_popover);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, add_print_popover_box);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, back_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, delete_confirmation_infobar);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, delete_prints_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, device_selector);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, devices_list);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, done_button);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enroll_message);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enroll_print_bin);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enroll_print_entry);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, enrollment_view);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, error_infobar);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, infobar_error);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, no_devices_found);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, print_popover);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, prints_gallery);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, prints_manager);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, spinner);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, stack);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, title);
  gtk_widget_class_bind_template_child (widget_class, CcFingerprintDialog, titlebar);

  gtk_widget_class_bind_template_callback (widget_class, back_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, cancel_deletion_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, confirm_deletion_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, delete_prints_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, done_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, fingerprint_dialog_delete_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_print_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, reenroll_finger_cb);
  gtk_widget_class_bind_template_callback (widget_class, select_device_row);
}
