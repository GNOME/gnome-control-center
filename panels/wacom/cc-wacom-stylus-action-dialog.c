/* cc-wacom-stylus-action-dialog.c
 *
 * Copyright © 2024 Red Hat, Inc.
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include <glib/gi18n.h>
#include <adwaita.h>
#ifdef HAVE_X11
#include <gdk/x11/gdkx.h>
#endif

#include <gdesktop-enums.h>

#include "cc-wacom-stylus-action-dialog.h"
#include "cc-wacom-panel.h"
#include "keyboard-shortcuts.h"

struct _CcWacomStylusActionDialog
{
	AdwDialog             parent_instance;

	AdwPreferencesGroup  *preferences_group;

	AdwActionRow         *left_button_row;
	AdwActionRow         *right_button_row;
	AdwActionRow         *middle_button_row;
	AdwActionRow         *back_row;
	AdwActionRow         *forward_row;
	AdwActionRow         *keybinding_row;
	AdwActionRow         *switch_monitor_row;

	GtkLabel             *keybinding_text;
	AdwDialog            *shortcut_window;

	GSettings            *settings;
	char                 *key;
	char                 *keybinding_button;
};

G_DEFINE_TYPE (CcWacomStylusActionDialog, cc_wacom_stylus_action_dialog, ADW_TYPE_DIALOG)

static void
cc_wacom_stylus_action_dialog_finalize (GObject *object)
{
	CcWacomStylusActionDialog *self = CC_WACOM_STYLUS_ACTION_DIALOG (object);

	g_clear_pointer (&self->key, free);

	G_OBJECT_CLASS (cc_wacom_stylus_action_dialog_parent_class)->finalize (object);
}

static void
left_button_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT);
}

static void
right_button_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT);
}

static void
middle_button_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE);
}

static void
back_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_BACK);
}

static void
forward_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD);
}

static void
keybinding_activated (CcWacomStylusActionDialog *self)
{
	if (g_settings_get_user_value(self->settings, self->keybinding_button) == NULL) {
		adw_dialog_present (self->shortcut_window, GTK_WIDGET (self));
	} else {
		g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_KEYBINDING);
	}
}

static void
keybinding_edited (CcWacomStylusActionDialog *self,
		   guint                      keyval,
		   guint                      keycode,
		   GdkModifierType            state,
		   GtkEventControllerKey     *key_controller)
{
	g_autofree gchar *custom_key = NULL;
	GdkModifierType real_mask;
	guint keyval_lower;
	gboolean is_modifier;
	GdkEvent *event;
	g_autofree char *label_str = NULL;

	if (keyval == GDK_KEY_Escape) {
		adw_dialog_close (self->shortcut_window);
		return;
	}

	event = gtk_event_controller_get_current_event (GTK_EVENT_CONTROLLER (key_controller));
	is_modifier = gdk_key_event_is_modifier (event);

	if (is_modifier)
		return;

	normalize_keyval_and_mask (keycode, state,
				   gtk_event_controller_key_get_group (key_controller),
				   &keyval_lower, &real_mask);

	custom_key = gtk_accelerator_name (keyval_lower, real_mask);
	label_str = gtk_accelerator_get_label (keyval_lower, real_mask);

	g_settings_set_string (self->settings, self->keybinding_button, custom_key);
	gtk_label_set_text (self->keybinding_text, label_str);
	adw_dialog_close (self->shortcut_window);
	adw_action_row_activate (self->keybinding_row);
}

static void
switch_monitor_activated (CcWacomStylusActionDialog *self)
{
	g_settings_set_enum (self->settings, self->key, G_DESKTOP_STYLUS_BUTTON_ACTION_SWITCH_MONITOR);
}

static void
cc_wacom_stylus_action_dialog_closed (CcWacomStylusActionDialog *self)
{
	if (g_settings_get_enum (self->settings, self->key) != G_DESKTOP_STYLUS_BUTTON_ACTION_KEYBINDING)
		g_settings_reset (self->settings, self->keybinding_button);
}

static void
cc_wacom_stylus_action_dialog_class_init (CcWacomStylusActionDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = cc_wacom_stylus_action_dialog_finalize;

	gtk_widget_class_set_template_from_resource (widget_class,
						     "/org/gnome/control-center/"
						     "wacom/cc-wacom-stylus-action-dialog.ui");

	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, preferences_group);

	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, left_button_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, right_button_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, middle_button_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, back_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, forward_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, keybinding_row);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, keybinding_text);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, shortcut_window);
	gtk_widget_class_bind_template_child (widget_class, CcWacomStylusActionDialog, switch_monitor_row);

	gtk_widget_class_bind_template_callback (widget_class, left_button_activated);
	gtk_widget_class_bind_template_callback (widget_class, middle_button_activated);
	gtk_widget_class_bind_template_callback (widget_class, right_button_activated);
	gtk_widget_class_bind_template_callback (widget_class, forward_activated);
	gtk_widget_class_bind_template_callback (widget_class, back_activated);
	gtk_widget_class_bind_template_callback (widget_class, keybinding_activated);
	gtk_widget_class_bind_template_callback (widget_class, keybinding_edited);
	gtk_widget_class_bind_template_callback (widget_class, switch_monitor_activated);
}

static void
cc_wacom_stylus_action_dialog_init (CcWacomStylusActionDialog *self)
{
	const char *text;

	gtk_widget_init_template (GTK_WIDGET (self));

	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->left_button_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->middle_button_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->right_button_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_BACK);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->back_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->forward_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_KEYBINDING);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->keybinding_row), text);
	text = cc_wacom_panel_get_stylus_button_action_label (G_DESKTOP_STYLUS_BUTTON_ACTION_SWITCH_MONITOR);
	adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->switch_monitor_row), text);
#ifdef HAVE_X11
	if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) {
		gtk_widget_set_visible (GTK_WIDGET(self->keybinding_row), FALSE);
		gtk_widget_set_visible (GTK_WIDGET(self->switch_monitor_row), FALSE);
	}
#endif /* HAVE_X11 */
}

GtkWidget*
cc_wacom_stylus_action_dialog_new (GSettings   *settings,
				   const char  *stylus_name,
				   guint        button,
				   const char  *key)
{
	CcWacomStylusActionDialog *dialog = g_object_new (CC_TYPE_WACOM_STYLUS_ACTION_DIALOG, NULL);
	GDesktopStylusButtonAction action;
	AdwActionRow *row = NULL;
	GdkModifierType mask;
	guint keyval;
	g_autofree char *text = NULL;
	g_autofree char *title = NULL;
	g_autofree char *keybinding_setting = NULL;
	g_autofree char *keybinding = NULL;

	g_return_val_if_fail(button > 0 && button <= 3, NULL);

	dialog->settings = settings;
	dialog->key = g_strdup (key);

	switch (button) {
		case 1:
			dialog->keybinding_button = "button-keybinding";
			break;
		case 2:
			dialog->keybinding_button = "secondary-button-keybinding";
			break;
		case 3:
			dialog->keybinding_button = "tertiary-button-keybinding";
			break;
	}

	adw_dialog_set_title (ADW_DIALOG (dialog), stylus_name);

	text = g_strdup_printf (_("Choose an action when button %d on the stylus is pressed"), button);
	adw_preferences_group_set_description (dialog->preferences_group, text);

	title = g_strdup_printf (_("Button %d Mapping"), button);
	adw_preferences_group_set_title (dialog->preferences_group, title);

	keybinding_setting = g_settings_get_string (dialog->settings, dialog->keybinding_button);
	gtk_accelerator_parse (keybinding_setting, &keyval, &mask);
	keybinding = gtk_accelerator_get_label (keyval, mask);
	gtk_label_set_text (dialog->keybinding_text, keybinding);

	action = g_settings_get_enum (settings, key);
	switch (action) {
		case G_DESKTOP_STYLUS_BUTTON_ACTION_DEFAULT:
			row = dialog->left_button_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_MIDDLE:
			row = dialog->middle_button_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_RIGHT:
			row = dialog->right_button_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_BACK:
			row = dialog->back_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_FORWARD:
			row = dialog->forward_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_KEYBINDING:
			row = dialog->keybinding_row;
			break;
		case G_DESKTOP_STYLUS_BUTTON_ACTION_SWITCH_MONITOR:
			row = dialog->switch_monitor_row;
			break;
	}

	if (row)
		adw_action_row_activate (row);

	g_signal_connect (dialog, "closed", G_CALLBACK (cc_wacom_stylus_action_dialog_closed),  NULL);

	return GTK_WIDGET (dialog);
}
