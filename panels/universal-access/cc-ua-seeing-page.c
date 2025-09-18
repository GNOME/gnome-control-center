/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010 Intel, Inc
 * Copyright 2022 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright 2022 Purism SPC
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author(s):
 *   Thomas Wood <thomas.wood@intel.com>
 *   Rodrigo Moya <rodrigo@gnome.org>
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-ua-seeing-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <glib/gi18n-lib.h>

#include "cc-list-row.h"
#include "cc-cursor-size-page.h"
#include "cc-ua-macros.h"
#include "cc-ua-seeing-page.h"

struct _CcUaSeeingPage
{
  AdwNavigationPage   parent_instance;

  AdwSwitchRow       *high_contrast_row;
  AdwSwitchRow       *status_shapes_row;
  GtkSwitch          *animation_effects_switch;
  GtkScale           *text_size_scale;
  CcListRow          *cursor_size_row;
  AdwSwitchRow       *sound_keys_row;
  AdwSwitchRow       *show_scrollbars_row;

  AdwSwitchRow       *screen_reader_row;
  AdwButtonRow       *configure_screen_reader_row;

  GDBusProxy         *proxy;

  AdwDialog          *text_size_dialog;
  GtkWidget          *text_size_preview_label;

  GSettings          *kb_settings;
  GSettings          *interface_settings;
  GSettings          *application_settings;
  GSettings          *a11y_interface_settings;
};

G_DEFINE_TYPE (CcUaSeeingPage, cc_ua_seeing_page, ADW_TYPE_NAVIGATION_PAGE)

static void
apply_text_size_changes (CcUaSeeingPage *self)
{
  g_settings_set_double (self->interface_settings, KEY_TEXT_SCALING_FACTOR,
                         gtk_range_get_value (GTK_RANGE (self->text_size_scale)));
  adw_dialog_close (self->text_size_dialog);
}


static gboolean
ua_text_size_change_value (GtkRange      *text_size_range,
                           GtkScrollType *scroll,
                           gdouble        value,
                           gpointer       user_data)
{
  /* Always round to 0.05 multiples */
  gtk_range_set_value (text_size_range, round (value / 0.05) * 0.05);

  // TODO: resize self->text_size_preview_label

  return TRUE;
}

static void
orca_get_version_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      data)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) error = NULL;
  CcUaSeeingPage *self = data;

  g_assert (CC_IS_UA_SEEING_PAGE (self));

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res, &error);
  if (!val)
    {
      /* Orca implemented a DBus interface at the same time as ShowPreferences,
       * so apparently this Orca version is too old. So, hide the row.
       * No need to check the version otherwise for now.
       */
      g_debug ("Failed to get Orca version: %s", error->message);
      gtk_widget_set_visible (GTK_WIDGET (self->configure_screen_reader_row), FALSE);
      return;
    }
}

static void
check_orca_show_preferences_support (CcUaSeeingPage *self)
{
  g_assert (CC_IS_UA_SEEING_PAGE (self));

  g_dbus_proxy_call (self->proxy,
                     "GetVersion",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     orca_get_version_cb,
                     self);
}

static void
on_orca_proxy_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      data)
{
  g_autoptr(GError) error = NULL;
  CcUaSeeingPage *self = data;

  g_assert (CC_IS_UA_SEEING_PAGE (self));

  self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (self->proxy == NULL)
    {
      g_warning ("Error creating proxy: %s", error->message);
      gtk_widget_set_visible (GTK_WIDGET  (self->configure_screen_reader_row), FALSE);
      return;
    }

  check_orca_show_preferences_support (self);
}

static void
ua_seeing_interface_cursor_size_changed_cb (CcUaSeeingPage *self)
{
  g_autofree char *label = NULL;
  int cursor_size;

  g_assert (CC_IS_UA_SEEING_PAGE (self));

  cursor_size = g_settings_get_int (self->interface_settings, KEY_MOUSE_CURSOR_SIZE);

  switch (cursor_size)
    {
    case 24:
      /* translators: the labels will read:
       * Cursor Size: Default */
      label = g_strdup (C_("cursor size", "Default"));
      break;
    case 32:
      label = g_strdup (C_("cursor size", "Medium"));
      break;
    case 48:
      label = g_strdup (C_("cursor size", "Large"));
      break;
    case 64:
      label = g_strdup (C_("cursor size", "Larger"));
      break;
    case 96:
      label = g_strdup (C_("cursor size", "Largest"));
      break;
    default:
      label = g_strdup_printf (g_dngettext (GETTEXT_PACKAGE,
                                            "%d pixel",
                                            "%d pixels",
                                            cursor_size),
                               cursor_size);
      break;
    }

  cc_list_row_set_secondary_label (self->cursor_size_row, label);
}

static void
ua_cursor_row_activated_cb (CcUaSeeingPage *self)
{
  AdwNavigationPage *page;
  GtkWidget *parent;

  g_assert (CC_IS_UA_SEEING_PAGE (self));

  page = ADW_NAVIGATION_PAGE (cc_cursor_size_page_new ());
  parent = gtk_widget_get_parent (GTK_WIDGET (self));

  adw_navigation_view_push (ADW_NAVIGATION_VIEW (parent), page);
}

static void
orca_show_preferences_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      data)
{
  g_autoptr(GVariant) val = NULL;
  g_autoptr(GError) error = NULL;

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res, &error);
  if (!val)
    {
      g_warning ("Failed to launch Orca preferences: %s", error->message);
      return;
    }
}

static void
configure_screen_reader_activated_cb (CcUaSeeingPage *self)
{
  g_assert (CC_IS_UA_SEEING_PAGE (self));

  g_dbus_proxy_call (self->proxy,
                     "ShowPreferences",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     orca_show_preferences_cb,
                     NULL);
}

static void
cc_ua_seeing_page_dispose (GObject *object)
{
  CcUaSeeingPage *self = (CcUaSeeingPage *)object;

  g_clear_object (&self->kb_settings);
  g_clear_object (&self->interface_settings);
  g_clear_object (&self->application_settings);
  g_clear_object (&self->a11y_interface_settings);

  G_OBJECT_CLASS (cc_ua_seeing_page_parent_class)->dispose (object);
}

static void
cc_ua_seeing_page_class_init (CcUaSeeingPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_ua_seeing_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "universal-access/cc-ua-seeing-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, high_contrast_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, status_shapes_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, animation_effects_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, cursor_size_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, sound_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, show_scrollbars_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, screen_reader_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, configure_screen_reader_row);

  gtk_widget_class_bind_template_callback (widget_class, ua_cursor_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, configure_screen_reader_activated_cb);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_preview_label);

  gtk_widget_class_bind_template_callback (widget_class, apply_text_size_changes);
}

static void
cc_ua_seeing_page_init (CcUaSeeingPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->kb_settings = g_settings_new (KEYBOARD_SETTINGS);
  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);
  self->application_settings = g_settings_new (APPLICATION_SETTINGS);
  self->a11y_interface_settings = g_settings_new (A11Y_INTERFACE_SETTINGS);

  /* High contrast */
  g_settings_bind (self->a11y_interface_settings, KEY_HIGH_CONTRAST,
                   self->high_contrast_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Switch shapes */
  g_settings_bind (self->a11y_interface_settings, KEY_STATUS_SHAPES,
                   self->status_shapes_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Enable Animations */
  g_settings_bind (self->interface_settings, KEY_ENABLE_ANIMATIONS,
                   self->animation_effects_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Text Size */
  g_signal_connect (GTK_RANGE (self->text_size_scale), "change-value",
                    G_CALLBACK (ua_text_size_change_value), self);

  /* Sound Keys */
  g_settings_bind (self->kb_settings, KEY_TOGGLEKEYS_ENABLED,
                   self->sound_keys_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Overlay Scrollbars */
  g_settings_bind (self->interface_settings, KEY_OVERLAY_SCROLLING,
                   self->show_scrollbars_row, "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

  /* Screen Reader */
  g_settings_bind (self->application_settings, KEY_SCREEN_READER_ENABLED,
                   self->screen_reader_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self->interface_settings, "changed::" KEY_MOUSE_CURSOR_SIZE,
                           G_CALLBACK (ua_seeing_interface_cursor_size_changed_cb),
                           self, G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.Orca.Service",
                            "/org/gnome/Orca/Service",
                            "org.gnome.Orca.Service",
                            NULL,
                            on_orca_proxy_ready,
                            self);

  ua_seeing_interface_cursor_size_changed_cb (self);
}

GtkWidget *
cc_ua_seeing_page_new (void)
{
  return g_object_new (CC_TYPE_UA_SEEING_PAGE, NULL);
}
