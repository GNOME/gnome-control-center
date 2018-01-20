/* cc-tariff-editor.c
 *
 * Copyright © 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#include "cc-tariff-editor.h"

#include <gdesktop-enums.h>
#include <libmogwai-tariff/tariff-builder.h>

#define CLOCK_SCHEMA     "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"
#define SECONDS_PER_DAY  (24 * 60 * 60)

struct _CcTariffEditor
{
  GtkGrid             parent;

  GtkAdjustment      *adjustment_from_hours;
  GtkAdjustment      *adjustment_from_minutes;
  GtkAdjustment      *adjustment_to_hours;
  GtkAdjustment      *adjustment_to_minutes;

  GtkWidget          *stack_from;
  GtkWidget          *stack_to;

  GVariant           *tariff_as_variant;

  /* Clock format */
  GSettings          *settings_clock;
  GDesktopClockFormat clock_format;

  guint8              time_period_from;
  guint8              time_period_to;

  guint64             seconds_to_start;
  guint64             seconds_to_end;
};

G_DEFINE_TYPE (CcTariffEditor, cc_tariff_editor, GTK_TYPE_GRID)

G_DEFINE_QUARK (CcTariffEditorError, cc_tariff_editor_error)

enum
{
  TARIFF_CHANGED,
  LAST_SIGNAL
};

enum
{
  AM,
  PM,
};


static void on_time_changed_cb (CcTariffEditor *self);


static guint signals[LAST_SIGNAL] = { 0, };


/*
 * Auxiliary methods
 */

static gint
get_am_hour (GDateTime *dt)
{
  if (g_date_time_get_hour (dt) % 12 == 0)
    return 12;

  return g_date_time_get_hour (dt) % 12;
}

static void
update_seconds_from_adjustments (CcTariffEditor *self)
{
  g_autoptr(GDateTime) local_start = NULL;
  g_autoptr(GDateTime) local_end = NULL;
  gint minute_start = 0;
  gint minute_end = 0;
  gint hour_start = 0;
  gint hour_end = 0;

  hour_start = (gint) gtk_adjustment_get_value (self->adjustment_from_hours);
  hour_end = (gint) gtk_adjustment_get_value (self->adjustment_to_hours);
  minute_start = (gint) gtk_adjustment_get_value (self->adjustment_from_minutes);
  minute_end = (gint) gtk_adjustment_get_value (self->adjustment_to_minutes);

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    {
      hour_start = hour_start % 12 + (self->time_period_from == AM ? 0 : 12);
      hour_end = hour_end % 12 + (self->time_period_to == AM ? 0 : 12);
    }

  local_start = g_date_time_new_local (1970, 1, 1, hour_start, minute_start, 0);
  local_end = g_date_time_new_local (1970, 1, 1, hour_end, minute_end, 0);

  /*
   * If 'from' > 'to', e.g. [22:15, 05:45), this period is crossing days and we
   * need to add +1 day to the end date. Otherwise, e.g, [13:10, 19:15), the period
   * is contained in a single day, and we don't need to do anything.
   */
  if (self->seconds_to_start >= self->seconds_to_end)
    self->seconds_to_end += SECONDS_PER_DAY;
}

static gboolean
setup_tariff (CcTariffEditor  *self,
              MwtTariff       *tariff,
              GError         **error)
{
  g_autoptr(GDateTime) maximum_date = NULL;
  GDateTime *period_start, *period_end;
  MwtPeriod *base_period, *period;
  GPtrArray *periods;

  if (!tariff)
    return FALSE;

  /*
   * Right now, it only parses the same kind of tariff it generates, a tariff
   * with these 2 periods:
   *
   *  1 → forbidden downloads period
   *  2 → allowed downloads period
   *
   * Anything that does not comform to that is ignored.
   */

  periods = mwt_tariff_get_periods (tariff);

  if (periods->len != 2)
    {
      g_set_error (error,
                   CC_TARIFF_EDITOR_ERROR,
                   CC_TARIFF_EDITOR_ERROR_NOT_SUPPORTED,
                   "Cannot parse complex tariffs yet");
      return FALSE;
    }

  /* Validate and setup the forbidden downloads period */
  maximum_date = g_date_time_new_utc (9999, 12, 31, 23 , 59, 59);
  base_period = g_ptr_array_index (periods, 0);
  period = g_ptr_array_index (periods, 1);

  if (mwt_period_get_repeat_type (base_period) != MWT_PERIOD_REPEAT_NONE ||
      g_date_time_to_unix (mwt_period_get_start (base_period)) != 0 ||
      g_date_time_to_unix (mwt_period_get_end (base_period)) != g_date_time_to_unix (maximum_date))
    {
      g_set_error (error,
                   CC_TARIFF_EDITOR_ERROR,
                   CC_TARIFF_EDITOR_ERROR_WRONG_FORMAT,
                   "Base tariff is wrong");
      return FALSE;
    }

  if (mwt_period_get_repeat_type (period) != MWT_PERIOD_REPEAT_DAY ||
      mwt_period_get_repeat_period (period) != 1)
    {
      g_set_error (error,
                   CC_TARIFF_EDITOR_ERROR,
                   CC_TARIFF_EDITOR_ERROR_WRONG_FORMAT,
                   "Repeating period is wrong");
      return FALSE;
    }

  g_signal_handlers_block_by_func (self->adjustment_from_hours, on_time_changed_cb, self);
  g_signal_handlers_block_by_func (self->adjustment_from_minutes, on_time_changed_cb, self);
  g_signal_handlers_block_by_func (self->adjustment_to_hours, on_time_changed_cb, self);
  g_signal_handlers_block_by_func (self->adjustment_to_minutes, on_time_changed_cb, self);

  period_start = mwt_period_get_start (period);
  period_end = mwt_period_get_end (period);

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_adjustment_set_value (self->adjustment_from_hours, g_date_time_get_hour (period_start));
      gtk_adjustment_set_value (self->adjustment_from_minutes, g_date_time_get_minute (period_start));
      gtk_adjustment_set_value (self->adjustment_to_hours, g_date_time_get_hour (period_end));
      gtk_adjustment_set_value (self->adjustment_to_minutes, g_date_time_get_minute (period_end));
    }
  else
    {
      self->time_period_from = g_date_time_get_hour (period_start) < 12 ? AM : PM;
      self->time_period_to = g_date_time_get_hour (period_end) < 12 ? AM : PM;

      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_from), self->time_period_from == AM ? "am" : "pm");
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_to), self->time_period_to == AM ? "am" : "pm");

      gtk_adjustment_set_value (self->adjustment_from_hours, get_am_hour (period_start));
      gtk_adjustment_set_value (self->adjustment_to_hours, get_am_hour (period_end));
      gtk_adjustment_set_value (self->adjustment_from_minutes, g_date_time_get_minute (period_start));
      gtk_adjustment_set_value (self->adjustment_to_minutes, g_date_time_get_minute (period_end));
    }

  g_signal_handlers_unblock_by_func (self->adjustment_from_hours, on_time_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->adjustment_from_minutes, on_time_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->adjustment_to_hours, on_time_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->adjustment_to_minutes, on_time_changed_cb, self);

  return TRUE;
}

static void
update_tariff (CcTariffEditor *self,
               gboolean        should_notify)
{
  g_autoptr(MwtTariffBuilder) tariff_builder = NULL;
  g_autoptr(MwtPeriod) forbidden_period = NULL;
  g_autoptr(GDateTime) forbidden_start = NULL;
  g_autoptr(GDateTime) forbidden_end = NULL;
  g_autoptr(MwtPeriod) allowed_period = NULL;
  g_autoptr(GDateTime) allowed_start_utc = NULL;
  g_autoptr(GDateTime) allowed_end_utc = NULL;
  g_autoptr(GDateTime) allowed_start = NULL;
  g_autoptr(GDateTime) allowed_end = NULL;
  g_autoptr(GTimeZone) local_tz = g_time_zone_new_local ();

  /*
   *  This is the a very simple implementation of a tariff, with 2 defined
   * periods:
   *
   *  1. Forbidden: [0, forever), downloads are forbidden.
   *  2. Allowed: [start hour, end hour) daily, downloads are allowed.
   *
   * Period 1 is in UTC because it covers all time, so we don’t need to worry
   * about the timezone of the start and end, or recurrences. Period 2 is in
   * the local timezone, so that the recurrences which fall within daylight
   * savings still happen at the same hour of the day for the user.
   */

  tariff_builder = mwt_tariff_builder_new ();
  mwt_tariff_builder_set_name (tariff_builder, "System Tariff");

  update_seconds_from_adjustments (self);

  /* 1. Forbidden downloads period */
  forbidden_start = g_date_time_new_from_unix_utc (0);
  forbidden_end = g_date_time_new_utc (9999, 12, 31, 23 , 59, 59);
  forbidden_period = mwt_period_new (forbidden_start,
                                     forbidden_end,
                                     MWT_PERIOD_REPEAT_NONE, 0,
                                     "capacity-limit", 0,
                                     NULL);

  mwt_tariff_builder_add_period (tariff_builder, forbidden_period);

  /* 2. Allowed downloads period */
  allowed_start_utc = g_date_time_new_from_unix_utc (self->seconds_to_start);
  allowed_end_utc = g_date_time_new_from_unix_utc (self->seconds_to_end);
  allowed_start = g_date_time_to_timezone (allowed_start_utc, local_tz);
  allowed_end = g_date_time_to_timezone (allowed_end_utc, local_tz);
  allowed_period = mwt_period_new (allowed_start,
                                   allowed_end,
                                   MWT_PERIOD_REPEAT_DAY, 1,
                                   "capacity-limit", G_MAXUINT64,
                                   NULL);

  mwt_tariff_builder_add_period (tariff_builder, allowed_period);

  /* Store the new tariff */
  g_clear_pointer (&self->tariff_as_variant, g_variant_unref);
  self->tariff_as_variant = mwt_tariff_builder_get_tariff_as_variant (tariff_builder);

  if (!self->tariff_as_variant)
    g_critical ("Error building tariff");
  else if (should_notify)
    g_signal_emit (self, signals[TARIFF_CHANGED], 0);
}

static void
update_adjustments (CcTariffEditor *self)
{
  g_autoptr(GDateTime) start = NULL;
  g_autoptr(GDateTime) end = NULL;

  g_debug ("Updating adjustments");

  start = g_date_time_new_from_unix_local (self->seconds_to_start);
  end = g_date_time_new_from_unix_local (self->seconds_to_end);

  g_signal_handlers_block_by_func (self->adjustment_from_hours, on_time_changed_cb, self);
  g_signal_handlers_block_by_func (self->adjustment_to_hours, on_time_changed_cb, self);

  /* From */
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_from), "blank");

      gtk_adjustment_set_lower (self->adjustment_from_hours, 0);
      gtk_adjustment_set_upper (self->adjustment_from_hours, 23);

      gtk_adjustment_set_value (self->adjustment_from_hours, g_date_time_get_hour (start));
    }
  else
    {
      self->time_period_from = gtk_adjustment_get_value (self->adjustment_from_hours) < 12 ? AM : PM;

      if (self->time_period_from == AM)
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack_from), "am");
      else
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack_from), "pm");

      gtk_adjustment_set_lower (self->adjustment_from_hours, 1);
      gtk_adjustment_set_upper (self->adjustment_from_hours, 12);

      gtk_adjustment_set_value (self->adjustment_from_hours, get_am_hour (start));
    }

  /* To */
  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_to), "blank");

      gtk_adjustment_set_lower (self->adjustment_to_hours, 0);
      gtk_adjustment_set_upper (self->adjustment_to_hours, 23);

      gtk_adjustment_set_value (self->adjustment_to_hours, g_date_time_get_hour (end));
    }
  else
    {
      self->time_period_to = gtk_adjustment_get_value (self->adjustment_to_hours) < 12 ? AM : PM;

      if (self->time_period_to == AM)
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack_to), "am");
      else
        gtk_stack_set_visible_child_name (GTK_STACK (self->stack_to), "pm");

      gtk_adjustment_set_lower (self->adjustment_to_hours, 1);
      gtk_adjustment_set_upper (self->adjustment_to_hours, 12);

      gtk_adjustment_set_value (self->adjustment_to_hours, get_am_hour (end));
    }

  g_signal_handlers_unblock_by_func (self->adjustment_from_hours, on_time_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->adjustment_to_hours, on_time_changed_cb, self);
}


/*
 * Callbacks
 */

static void
on_clock_settings_changed_cb (GSettings      *settings_display,
                              gchar          *key,
                              CcTariffEditor *self)
{
  self->clock_format = g_settings_get_enum (settings_display, CLOCK_FORMAT_KEY);

  update_adjustments (self);
}

static gboolean
on_hours_output_cb (GtkSpinButton  *spin,
                    CcTariffEditor *self)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;

  adjustment = gtk_spin_button_get_adjustment (spin);

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    text = g_strdup_printf ("%.0f", gtk_adjustment_get_value (adjustment));
  else
    text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));

  gtk_entry_set_text (GTK_ENTRY (spin), text);

  return TRUE;
}

static gboolean
on_minutes_output_cb (GtkSpinButton  *spin,
                      CcTariffEditor *self)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;

  adjustment = gtk_spin_button_get_adjustment (spin);

  text = g_strdup_printf ("%02.0f", gtk_adjustment_get_value (adjustment));
  gtk_entry_set_text (GTK_ENTRY (spin), text);

  return TRUE;
}

static void
on_time_changed_cb (CcTariffEditor *self)
{
  update_tariff (self, TRUE);
}

static void
on_time_period_from_clicked_cb (GtkButton      *button,
                                CcTariffEditor *self)
{
  if (self->time_period_from == AM)
    {
      self->time_period_from = PM;
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_from), "pm");
    }
  else
    {
      self->time_period_from = AM;
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_from), "am");
    }
}

static void
on_time_period_to_clicked_cb (GtkButton      *button,
                              CcTariffEditor *self)
{
  if (self->time_period_to == AM)
    {
      self->time_period_to = PM;
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_to), "pm");
    }
  else
    {
      self->time_period_to = AM;
      gtk_stack_set_visible_child_name (GTK_STACK (self->stack_to), "am");
    }
}


/*
 * GObject overrides
 */

static void
cc_tariff_editor_finalize (GObject *object)
{
  CcTariffEditor *self = (CcTariffEditor *)object;

  g_clear_object (&self->settings_clock);
  g_clear_pointer (&self->tariff_as_variant, g_variant_unref);

  G_OBJECT_CLASS (cc_tariff_editor_parent_class)->finalize (object);
}


static void
cc_tariff_editor_class_init (CcTariffEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_tariff_editor_finalize;

  signals[TARIFF_CHANGED] = g_signal_new ("tariff-changed",
                                          CC_TYPE_TARIFF_EDITOR,
                                          G_SIGNAL_RUN_FIRST,
                                          0, NULL, NULL, NULL,
                                          G_TYPE_NONE,
                                          0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/updates/cc-tariff-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcTariffEditor, adjustment_from_hours);
  gtk_widget_class_bind_template_child (widget_class, CcTariffEditor, adjustment_from_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcTariffEditor, adjustment_to_hours);
  gtk_widget_class_bind_template_child (widget_class, CcTariffEditor, adjustment_to_minutes);
  gtk_widget_class_bind_template_child (widget_class, CcTariffEditor, stack_from);
  gtk_widget_class_bind_template_child (widget_class, CcTariffEditor, stack_to);

  gtk_widget_class_bind_template_callback (widget_class, on_hours_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_minutes_output_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_period_from_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_time_period_to_clicked_cb);
}

static void
cc_tariff_editor_init (CcTariffEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Hardcode the default hours (22:00 and 6:00) */
  self->seconds_to_start = 22 * 60 * 60;
  self->seconds_to_end = (6 + 24) * 60 * 60;

  /* Clock settings */
  self->settings_clock = g_settings_new (CLOCK_SCHEMA);
  self->clock_format = g_settings_get_enum (self->settings_clock, CLOCK_FORMAT_KEY);

  update_adjustments (self);

  g_signal_connect (self->settings_clock,
                    "changed::"CLOCK_FORMAT_KEY,
                    G_CALLBACK (on_clock_settings_changed_cb),
                    self);

  update_tariff (self, FALSE);
}

GVariant*
cc_tariff_editor_get_tariff_as_variant (CcTariffEditor *self)
{
  g_return_val_if_fail (CC_IS_TARIFF_EDITOR (self), NULL);

  return self->tariff_as_variant;
}

void
cc_tariff_editor_load_tariff (CcTariffEditor  *self,
                              MwtTariff       *tariff,
                              GError         **error)
{
  g_return_if_fail (CC_IS_TARIFF_EDITOR (self));

  if (setup_tariff (self, tariff, error))
    update_tariff (self, FALSE);
}
