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
  AdwSwitchRow       *reduced_motion_row;
  CcListRow          *text_size_row;
  GtkScale           *text_size_scale;
  CcListRow          *cursor_size_row;
  AdwSwitchRow       *sound_keys_row;
  AdwSwitchRow       *show_scrollbars_row;

  AdwSwitchRow       *screen_reader_row;
  AdwButtonRow       *configure_screen_reader_row;

  GDBusProxy         *proxy;

  AdwDialog          *text_size_dialog;
  GtkLabel           *text_size_preview_label;
  GtkLabel           *text_size_label_small;
  GtkLabel           *text_size_label_large;

  GSettings          *kb_settings;
  GSettings          *interface_settings;
  GSettings          *application_settings;
  GSettings          *a11y_interface_settings;
};

G_DEFINE_TYPE (CcUaSeeingPage, cc_ua_seeing_page, ADW_TYPE_NAVIGATION_PAGE)

static void
set_label_scale (CcUaSeeingPage *self,
                 GtkLabel       *label,
                 double          scale)
{
  PangoContext *pango_ctx;
  PangoFontDescription *font_desc;
  double default_font_size;
  g_autoptr(PangoAttribute) attr = NULL;
  g_autoptr(PangoAttrList) new_attrs = NULL;

  pango_ctx = gtk_widget_get_pango_context (GTK_WIDGET (label));
  font_desc = pango_context_get_font_description (pango_ctx);

  if (font_desc)
    {
      default_font_size = pango_font_description_get_size (font_desc);

      /* We need absolute size without text scaling applied */
      if (pango_font_description_get_size_is_absolute (font_desc))
        default_font_size /= g_settings_get_double (self->interface_settings,
                                                    KEY_TEXT_SCALING_FACTOR);
      else
        default_font_size *= 96.0 / 72; /* 96 dpi */
    }
  else
    {
      default_font_size = 11 * PANGO_SCALE * 96.0 / 72; /* Assuming 11 pt, 96 dpi */
    }

  attr = pango_attr_size_new_absolute (round (scale * default_font_size));
  new_attrs = pango_attr_list_new ();
  pango_attr_list_insert (new_attrs, g_steal_pointer (&attr));

  gtk_label_set_attributes (label, new_attrs);
}

static void
update_text_size_row_label (CcUaSeeingPage *self)
{
  const gchar *label = NULL;
  double text_scaling_factor;

  text_scaling_factor = g_settings_get_double (self->interface_settings,
                                               KEY_TEXT_SCALING_FACTOR);
  label = text_scaling_factor > DPI_FACTOR_NORMAL ? _("Large") : _("Default");
  cc_list_row_set_secondary_label (self->text_size_row, label);
}

static void
apply_text_size_changes (CcUaSeeingPage *self)
{
  g_settings_set_double (self->interface_settings, KEY_TEXT_SCALING_FACTOR,
                         gtk_range_get_value (GTK_RANGE (self->text_size_scale)));
  adw_dialog_close (self->text_size_dialog);

  update_text_size_row_label (self);
}

static void
ua_text_size_value_changed (GtkRange      *text_size_range,
                            gpointer       user_data)
{
  CcUaSeeingPage *self = CC_UA_SEEING_PAGE (user_data);
  double value = gtk_range_get_value (text_size_range);

  gtk_range_set_value (text_size_range, value);

  set_label_scale (self, self->text_size_preview_label, value);
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
    }
}

static gboolean
get_reduced_motion_mapping (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  const char *val = g_variant_get_string (variant, NULL);

  if (!g_strcmp0 (val, "no-preference"))
    g_value_set_boolean (value, FALSE);
  else
    g_value_set_boolean (value, TRUE);

  return TRUE;
}

static GVariant *
set_reduced_motion_mapping (const GValue       *value,
                            const GVariantType *expected_type,
                            gpointer            user_data)
{
  GSettings *settings = user_data;

  if (g_value_get_boolean (value))
    return g_variant_new_string ("reduce");

  g_settings_reset (settings, KEY_REDUCED_MOTION);

  return NULL;
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
ua_text_size_row_activated_cb (CcUaSeeingPage *self)
{
  /* Intiialize scale with the current value. */
  gtk_range_set_value (GTK_RANGE (self->text_size_scale),
                       g_settings_get_double (self->interface_settings,
                                              KEY_TEXT_SCALING_FACTOR));
  adw_dialog_present (self->text_size_dialog, GTK_WIDGET (self));
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
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, reduced_motion_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_scale);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, cursor_size_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, sound_keys_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, show_scrollbars_row);

  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, screen_reader_row);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, configure_screen_reader_row);

  gtk_widget_class_bind_template_callback (widget_class, ua_cursor_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, ua_text_size_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, configure_screen_reader_activated_cb);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_preview_label);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_label_small);
  gtk_widget_class_bind_template_child (widget_class, CcUaSeeingPage, text_size_label_large);

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

  /* Reduced motion */
  g_settings_bind_with_mapping (self->a11y_interface_settings, KEY_REDUCED_MOTION,
                                self->reduced_motion_row, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                get_reduced_motion_mapping,
                                set_reduced_motion_mapping,
                                self->a11y_interface_settings,
                                NULL);

  /* Text Size */
  gtk_range_set_value (GTK_RANGE (self->text_size_scale),
                       g_settings_get_double (self->interface_settings,
                                              KEY_TEXT_SCALING_FACTOR));
  g_signal_connect (GTK_RANGE (self->text_size_scale), "value-changed",
                    G_CALLBACK (ua_text_size_value_changed), self);
  update_text_size_row_label (self);
  set_label_scale (self, self->text_size_label_small, 1.0);
  set_label_scale (self, self->text_size_label_large, 2.0);

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
