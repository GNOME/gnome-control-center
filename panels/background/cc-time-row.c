/* cc-time-row.c
 *
 * Copyright 2025 Jamie Murphy <jmurphy@gnome.org>
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <config.h>

#include <gdesktop-enums.h>

#include "cc-time-editor.h"
#include "cc-time-row.h"

#define CLOCK_SCHEMA     "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

struct _CcTimeRow
{
  AdwActionRow parent_instance;

  GtkWidget *popover_box;
  GtkPopover *popover;
  GtkLabel *current;
  CcTimeEditor *time_editor;

  GSettings *clock_settings;
};

G_DEFINE_TYPE (CcTimeRow, cc_time_row, ADW_TYPE_ACTION_ROW);

enum
{
  PROP_0,
  PROP_TIME,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
set_fractional_time (CcTimeRow *self,
                     gdouble    frac_time)
{
  gdouble hours, mins = 0.f;

  mins = modf (frac_time, &hours) * 60.f;

  cc_time_editor_set_time (self->time_editor, (int) hours, (int) mins);
}

static gdouble
get_fractional_time (CcTimeRow *self)
{
  guint hours, mins;

  hours = cc_time_editor_get_hour (self->time_editor);
  mins = cc_time_editor_get_minute (self->time_editor);

  return hours + (mins / 60.0f);
}

static void
time_changed_cb (CcTimeRow    *self,
                 CcTimeEditor *time_editor)
{
  GDesktopClockFormat value;
  gboolean is_am_pm;
  GDateTime *datetime, *local;
  guint hour, minute;
  const char *format_str;

  value = g_settings_get_enum (self->clock_settings, CLOCK_FORMAT_KEY);
  is_am_pm = value == G_DESKTOP_CLOCK_FORMAT_12H;

  hour = cc_time_editor_get_hour (time_editor);
  minute = cc_time_editor_get_minute (time_editor);

  if (is_am_pm)
    format_str = "%I:%M %p";
  else
    format_str = "%H:%M";

  local = g_date_time_new_now_local ();
  datetime = g_date_time_new_local (g_date_time_get_year (local),
                                    g_date_time_get_month (local),
                                    g_date_time_get_day_of_month (local),
                                    hour, minute, 0);

  gtk_label_set_label (self->current, g_date_time_format (datetime, format_str));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TIME]);
}

static void
notify_popover_visible_cb (CcTimeRow *self)
{
  if (gtk_widget_get_visible (GTK_WIDGET (self->popover)))
    gtk_widget_add_css_class (GTK_WIDGET (self), "has-open-popup");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "has-open-popup");
}

static void
cc_time_row_activate (AdwActionRow *row)
{
  CcTimeRow *self = CC_TIME_ROW (row);

  if (gtk_widget_get_visible (self->popover_box))
    gtk_popover_popup (self->popover);
}

static void
cc_time_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CcTimeRow *self = CC_TIME_ROW (object);

  switch (prop_id)
    {
    case (PROP_TIME):
      g_value_set_double (value, cc_time_row_get_time (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_time_row_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  CcTimeRow *self = CC_TIME_ROW (object);

  switch (prop_id)
    {
    case (PROP_TIME):
      cc_time_row_set_time (self, g_value_get_double (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_time_row_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  CcTimeRow *self = CC_TIME_ROW (widget);

  GTK_WIDGET_CLASS (cc_time_row_parent_class)->size_allocate (widget, width, height, baseline);

  gtk_popover_present (self->popover);
}

static gboolean
cc_time_row_focus (GtkWidget        *widget,
                   GtkDirectionType  direction)
{
  CcTimeRow *self = CC_TIME_ROW (widget);

  if (self->popover && gtk_widget_get_visible (GTK_WIDGET (self->popover)))
    return gtk_widget_child_focus (GTK_WIDGET (self->popover), direction);
  else
    return GTK_WIDGET_CLASS (cc_time_row_parent_class)->focus (widget, direction);
}

static void
cc_time_row_class_init (CcTimeRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  AdwActionRowClass *row_class = ADW_ACTION_ROW_CLASS (klass);

  object_class->get_property = cc_time_row_get_property;
  object_class->set_property = cc_time_row_set_property;

  widget_class->size_allocate = cc_time_row_size_allocate;
  widget_class->focus = cc_time_row_focus;

  row_class->activate = cc_time_row_activate;

  properties[PROP_TIME] = g_param_spec_double ("time", NULL, NULL,
                                               0.0, 24.0, 0.0,
                                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-time-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcTimeRow, current);
  gtk_widget_class_bind_template_child (widget_class, CcTimeRow, popover_box);
  gtk_widget_class_bind_template_child (widget_class, CcTimeRow, popover);
  gtk_widget_class_bind_template_child (widget_class, CcTimeRow, time_editor);

  gtk_widget_class_bind_template_callback (widget_class, notify_popover_visible_cb);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_COMBO_BOX);
}

static void
cc_time_row_init (CcTimeRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->clock_settings = g_settings_new (CLOCK_SCHEMA);
  g_signal_connect_object (self->clock_settings, "changed::" CLOCK_FORMAT_KEY,
                           G_CALLBACK (time_changed_cb), self,
                           G_CONNECT_SWAPPED);

  g_signal_connect_swapped (self->time_editor, "time-changed",
                            G_CALLBACK (time_changed_cb), self);

  time_changed_cb (self, self->time_editor);
}

gdouble
cc_time_row_get_time (CcTimeRow *self)
{
  g_assert (CC_IS_TIME_ROW (self));

  return get_fractional_time (self);
}

void
cc_time_row_set_time (CcTimeRow *self,
                      gdouble    time)
{
  g_assert (CC_IS_TIME_ROW (self));
  g_assert (time >= 0.f);
  g_assert (time <= 24.0f);

  set_fractional_time (self, time);
}