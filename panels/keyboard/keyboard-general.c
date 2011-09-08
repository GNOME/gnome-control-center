/*
 * Copyright (C) 2010 Intel, Inc
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 */

#include "keyboard-general.h"

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

static GSettings *keyboard_settings = NULL;
static GSettings *interface_settings = NULL;

static gboolean
get_rate (GValue   *value,
          GVariant *variant,
          gpointer  user_data)
{
  int rate;
  gdouble fraction;

  rate = g_variant_get_uint32 (variant);
  fraction = 1.0 / ((gdouble) rate / 1000.0);
  g_value_set_double (value, fraction);
  g_debug ("Getting fraction %f for msecs %d", fraction, rate);
  return TRUE;
}

static GVariant *
set_rate (const GValue       *value,
          const GVariantType *expected_type,
          gpointer            user_data)
{
  gdouble rate;
  int msecs;

  rate = g_value_get_double (value);
  msecs = (1 / rate) * 1000;
  g_debug ("Setting repeat rate to %d", msecs);
  return g_variant_new_uint32 (msecs);
}

static gboolean
layout_link_clicked (GtkLinkButton *button,
                     CcPanel       *panel)
{
  CcShell *shell;
  GError *error = NULL;
  const char *argv[] = { "layouts", NULL };

  shell = cc_panel_get_shell (panel);
  if (cc_shell_set_active_panel_from_id (shell, "region", argv, &error) == FALSE)
    {
      g_warning ("Failed to activate Region panel: %s", error->message);
      g_error_free (error);
    }
  return TRUE;
}

void
keyboard_general_init (CcPanel *panel, GtkBuilder *builder)
{
  if (keyboard_settings == NULL)
    keyboard_settings = g_settings_new ("org.gnome.settings-daemon.peripherals.keyboard");

  if (interface_settings == NULL)
    interface_settings = g_settings_new ("org.gnome.desktop.interface");

  g_settings_bind (keyboard_settings, "repeat",
                   WID ("repeat_toggle"), "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (keyboard_settings, "repeat",
                   WID ("repeat_table"), "sensitive",
                   G_SETTINGS_BIND_GET);
  
  g_settings_bind (keyboard_settings, "delay",
                   gtk_range_get_adjustment (GTK_RANGE (WID ("repeat_delay_scale"))), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (keyboard_settings, "repeat-interval",
                                gtk_range_get_adjustment (GTK_RANGE (WID ("repeat_speed_scale"))), "value",
                                G_SETTINGS_BIND_DEFAULT,
                                get_rate, set_rate, NULL, NULL);

  g_settings_bind (interface_settings, "cursor-blink",
                   WID ("cursor_toggle"), "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (interface_settings, "cursor-blink",
                   WID ("cursor_table"), "sensitive",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (interface_settings, "cursor-blink-time",
                   gtk_range_get_adjustment (GTK_RANGE (WID ("cursor_blink_time_scale"))), "value",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (WID ("linkbutton"), "activate-link",
                    G_CALLBACK (layout_link_clicked), panel);
}

void
keyboard_general_dispose (CcPanel *panel)
{
  if (keyboard_settings != NULL)
    {
      g_object_unref (keyboard_settings);
      keyboard_settings = NULL;
    }

  if (interface_settings != NULL)
    {
      g_object_unref (interface_settings);
      interface_settings = NULL;
    }
}
