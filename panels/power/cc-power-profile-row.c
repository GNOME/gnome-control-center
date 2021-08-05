/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.c
 *
 * Copyright 2020 Red Hat Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-power-profile-row"

#include <config.h>

#include <glib/gi18n.h>
#include "cc-power-profile-row.h"

struct _CcPowerProfileRow
{
  GtkListBoxRow parent_instance;

  GtkRadioButton *button;
  GtkLabel       *subtitle_label;
  GtkLabel       *title_label;

  CcPowerProfile power_profile;
  char *performance_inhibited;
};

G_DEFINE_TYPE (CcPowerProfileRow, cc_power_profile_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  BUTTON_TOGGLED,
  N_SIGNALS
};

static guint signals[N_SIGNALS];

static const char *
get_performance_inhibited_text (const char *inhibited)
{
  if (!inhibited || *inhibited == '\0')
    return NULL;

  if (g_str_equal (inhibited, "lap-detected"))
    return _("Lap detected: performance mode unavailable");
  if (g_str_equal (inhibited, "high-operating-temperature"))
    return _("High hardware temperature: performance mode unavailable");
  return _("Performance mode unavailable");
}

static void
performance_profile_set_inhibited (CcPowerProfileRow *self,
                                   const char        *performance_inhibited)
{
  const char *text;
  gboolean inhibited = FALSE;

  if (self->power_profile != CC_POWER_PROFILE_PERFORMANCE)
    return;

  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self->subtitle_label)),
                                  GTK_STYLE_CLASS_DIM_LABEL);
  gtk_style_context_remove_class (gtk_widget_get_style_context (GTK_WIDGET (self->subtitle_label)),
                                  GTK_STYLE_CLASS_ERROR);

  text = get_performance_inhibited_text (performance_inhibited);
  if (text)
    inhibited = TRUE;
  else
    text = _("High performance and power usage.");
  gtk_label_set_text (GTK_LABEL (self->subtitle_label), text);

  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self->subtitle_label)),
                               inhibited ? GTK_STYLE_CLASS_ERROR : GTK_STYLE_CLASS_DIM_LABEL);
  gtk_widget_set_sensitive (GTK_WIDGET (self), !inhibited);
}

static void
cc_power_profile_row_button_toggled_cb (CcPowerProfileRow *self)
{
  g_signal_emit (self, signals[BUTTON_TOGGLED], 0);
}

static void
cc_power_profile_row_class_init (CcPowerProfileRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/power/cc-power-profile-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPowerProfileRow, button);
  gtk_widget_class_bind_template_child (widget_class, CcPowerProfileRow, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerProfileRow, title_label);

  gtk_widget_class_bind_template_callback (widget_class, cc_power_profile_row_button_toggled_cb);

  signals[BUTTON_TOGGLED] =
    g_signal_new ("button-toggled",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void
cc_power_profile_row_init (CcPowerProfileRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcPowerProfile
cc_power_profile_row_get_profile (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), -1);

  return self->power_profile;
}

GtkRadioButton *
cc_power_profile_row_get_radio_button (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), NULL);

  return self->button;
}

void
cc_power_profile_row_set_active (CcPowerProfileRow *self,
                                 gboolean           active)
{
  g_return_if_fail (CC_IS_POWER_PROFILE_ROW (self));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->button), active);
}

void
cc_power_profile_row_set_performance_inhibited (CcPowerProfileRow *self,
                                                const char        *performance_inhibited)
{
  g_return_if_fail (CC_IS_POWER_PROFILE_ROW (self));

  g_clear_pointer (&self->performance_inhibited, g_free);
  self->performance_inhibited = g_strdup (performance_inhibited);
  performance_profile_set_inhibited (self, self->performance_inhibited);
}

gboolean
cc_power_profile_row_get_active (CcPowerProfileRow *self)
{
  g_return_val_if_fail (CC_IS_POWER_PROFILE_ROW (self), FALSE);

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->button));
}

CcPowerProfileRow *
cc_power_profile_row_new (CcPowerProfile power_profile)
{
  CcPowerProfileRow *self;
  const char *text, *subtext;

  self = g_object_new (CC_TYPE_POWER_PROFILE_ROW, NULL);

  self->power_profile = power_profile;
  switch (self->power_profile)
    {
      case CC_POWER_PROFILE_PERFORMANCE:
        text = C_("Power profile", "Performance");
        subtext = _("High performance and power usage.");
        break;
      case CC_POWER_PROFILE_BALANCED:
        text = C_("Power profile", "Balanced");
        subtext = _("Standard performance and power usage.");
        break;
      case CC_POWER_PROFILE_POWER_SAVER:
        text = C_("Power profile", "Power Saver");
        subtext = _("Reduced performance and power usage.");
        break;
      default:
        g_assert_not_reached ();
    }

  gtk_label_set_markup (self->title_label, text);
  gtk_label_set_markup (self->subtitle_label, subtext);

  return self;
}

CcPowerProfile
cc_power_profile_from_str (const char *profile)
{
  if (g_strcmp0 (profile, "power-saver") == 0)
    return CC_POWER_PROFILE_POWER_SAVER;
  if (g_strcmp0 (profile, "balanced") == 0)
    return CC_POWER_PROFILE_BALANCED;
  if (g_strcmp0 (profile, "performance") == 0)
    return CC_POWER_PROFILE_PERFORMANCE;

  g_assert_not_reached ();
}

const char *
cc_power_profile_to_str (CcPowerProfile profile)
{
  switch (profile)
  {
  case CC_POWER_PROFILE_POWER_SAVER:
    return "power-saver";
  case CC_POWER_PROFILE_BALANCED:
    return "balanced";
  case CC_POWER_PROFILE_PERFORMANCE:
    return "performance";
  default:
    g_assert_not_reached ();
  }
}
