/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 GNOME Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <adwaita.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
#include <langinfo.h>
#include <locale.h>
#endif

#include "cc-bar-chart.h"
#include "cc-screen-time-statistics-row.h"
#include "cc-util.h"

/**
 * CcScreenTimeStatisticsRow:
 *
 * An #AdwPreferencesRow used to display the user’s screen time statistics.
 *
 * This presents some summary statistics of their screen time usage, plus an
 * interactive graph of their usage per day in the last few weeks.
 *
 * If no data is available, a placeholder will be displayed until some data is
 * available.
 *
 * Bars in the graph can be selected to show summary statistics relating to that
 * day. If data is available, a day must always be selected.
 *
 * The data is loaded from a file specified using
 * #CcScreenTimeStatisticsRow:history-file. The data is automatically reloaded
 * if the file changes, and as time passes.
 */
struct _CcScreenTimeStatisticsRow {
  AdwPreferencesRow parent_instance;

  /* Child widgets */
  CcBarChart *bar_chart;
  GtkLabel *selected_date_label;
  GtkLabel *selected_screen_time_label;
  GtkLabel *selected_average_label;
  GtkLabel *selected_average_value_label;
  GtkLabel *week_date_label;
  GtkLabel *week_screen_time_label;
  GtkLabel *week_average_value_label;

  GtkButton *previous_week_button;
  GtkButton *next_week_button;

  GtkStack *data_stack;

  /* Model data */
  struct
    {
      GDate start_date;  /* inclusive; invalid when unset */
      size_t n_days;
      double *screen_time_per_day;  /* minutes for each day; (nullable) (array length=n_days) (owned) */
    } model;

  /* UI state */
  GFile *history_file;  /* (nullable) (owned) */
  GFileMonitor *history_file_monitor;  /* (nullable) (owned) */
  gulong history_file_monitor_changed_id;
  GSource *update_timeout_source;  /* (nullable) (owned) */

  GDate selected_date;  /* invalid when unset */
  unsigned int daily_limit_minutes;
};

G_DEFINE_TYPE (CcScreenTimeStatisticsRow, cc_screen_time_statistics_row, ADW_TYPE_PREFERENCES_ROW)

typedef enum {
  PROP_HISTORY_FILE = 1,
  PROP_SELECTED_DATE,
  PROP_DAILY_LIMIT,
} CcScreenTimeStatisticsRowProperty;

static GParamSpec *props[PROP_DAILY_LIMIT + 1];

static void cc_screen_time_statistics_row_get_property (GObject    *object,
                                                        guint       property_id,
                                                        GValue     *value,
                                                        GParamSpec *pspec);
static void cc_screen_time_statistics_row_set_property (GObject      *object,
                                                        guint         property_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void cc_screen_time_statistics_row_dispose (GObject *object);
static void cc_screen_time_statistics_row_finalize (GObject *object);
static void cc_screen_time_statistics_row_map (GtkWidget *widget);
static void cc_screen_time_statistics_row_unmap (GtkWidget *widget);

static void get_today (GDate *today);
static unsigned int get_week_start (void);
static void update_model (CcScreenTimeStatisticsRow *self);
static char *bar_chart_continuous_axis_label_cb (CcBarChart *chart,
                                                 double      value,
                                                 void       *user_data);
static double bar_chart_continuous_axis_grid_line_cb (CcBarChart   *chart,
                                                      unsigned int  idx,
                                                      void         *user_data);
static void bar_chart_update_accessible_description (CcScreenTimeStatisticsRow *self);
static void bar_chart_notify_selected_index_cb (GObject    *object,
                                                GParamSpec *pspec,
                                                gpointer    user_data);
static void previous_week_button_clicked_cb (GtkButton *button,
                                             gpointer   user_data);
static void next_week_button_clicked_cb (GtkButton *button,
                                         gpointer   user_data);
static void maybe_enable_update_timeout (CcScreenTimeStatisticsRow *self);

static void
cc_screen_time_statistics_row_class_init (CcScreenTimeStatisticsRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_screen_time_statistics_row_get_property;
  object_class->set_property = cc_screen_time_statistics_row_set_property;
  object_class->dispose = cc_screen_time_statistics_row_dispose;
  object_class->finalize = cc_screen_time_statistics_row_finalize;

  widget_class->map = cc_screen_time_statistics_row_map;
  widget_class->unmap = cc_screen_time_statistics_row_unmap;

  /**
   * CcScreenTimeStatisticsRow:history-file: (nullable)
   *
   * File containing the screen time history to display.
   *
   * If %NULL, the widget will show a ‘no data available’ placeholder message.
   */
  props[PROP_HISTORY_FILE] =
    g_param_spec_object ("history-file",
                         NULL, NULL,
                         G_TYPE_FILE,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcScreenTimeStatisticsRow:selected-date: (nullable)
   *
   * Currently selected date.
   *
   * The data shown will be the week containing this date.
   *
   * This will be %NULL if no data is available. If any data is available, a
   * date will always be selected.
   */
  props[PROP_SELECTED_DATE] =
    g_param_spec_boxed ("selected-date",
                        NULL, NULL,
                        G_TYPE_DATE,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * CcScreenTimeStatisticsRow:daily-limit:
   *
   * Daily usage limit for the user, in minutes.
   *
   * If set, this results in a threshold line being drawn on the usage graph.
   * Zero if unset.
   */
  props[PROP_DAILY_LIMIT] =
    g_param_spec_uint ("daily-limit",
                       NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, G_N_ELEMENTS (props), props);

  g_type_ensure (CC_TYPE_BAR_CHART);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-screen-time-statistics-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, bar_chart);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, selected_date_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, selected_screen_time_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, selected_average_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, selected_average_value_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, week_date_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, week_screen_time_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, week_average_value_label);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, previous_week_button);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, next_week_button);
  gtk_widget_class_bind_template_child (widget_class, CcScreenTimeStatisticsRow, data_stack);

  gtk_widget_class_bind_template_callback (widget_class, bar_chart_notify_selected_index_cb);
  gtk_widget_class_bind_template_callback (widget_class, previous_week_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, next_week_button_clicked_cb);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_GROUP);
}

static void
cc_screen_time_statistics_row_init (CcScreenTimeStatisticsRow *self)
{
  GDate today_date;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Bar chart weekday labels. These need to take into account the user’s
   * preferred starting day of the week. */
  const char * const weekdays[] = {
    C_("abbreviated weekday name for Sunday", "S"),
    C_("abbreviated weekday name for Monday", "M"),
    C_("abbreviated weekday name for Tuesday", "T"),
    C_("abbreviated weekday name for Wednesday", "W"),
    C_("abbreviated weekday name for Thursday", "T"),
    C_("abbreviated weekday name for Friday", "F"),
    C_("abbreviated weekday name for Saturday", "S"),
  };
  const char *labels[G_N_ELEMENTS (weekdays) + 1 /* NULL terminator */];
  unsigned int week_start = get_week_start ();  /* 0 = Sunday, 1 = Monday, 2 = Tuesday, etc. */
  for (size_t i = 0; i < G_N_ELEMENTS (weekdays); i++)
    labels[i] = weekdays[(week_start + i) % G_N_ELEMENTS (weekdays)];
  labels[G_N_ELEMENTS (labels) - 1] = NULL;

  cc_bar_chart_set_discrete_axis_labels (self->bar_chart, labels);

  cc_bar_chart_set_continuous_axis_label_callback (self->bar_chart, bar_chart_continuous_axis_label_cb, NULL, NULL);
  cc_bar_chart_set_continuous_axis_grid_line_callback (self->bar_chart, bar_chart_continuous_axis_grid_line_cb, NULL, NULL);

  /* Load initial data and show it in the UI. */
  update_model (self);

  get_today (&today_date);
  cc_screen_time_statistics_row_set_selected_date (self, &today_date);
}

static void
cc_screen_time_statistics_row_get_property (GObject    *object,
                                            guint       property_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (object);

  switch ((CcScreenTimeStatisticsRowProperty) property_id)
    {
    case PROP_HISTORY_FILE:
      g_value_set_object (value, cc_screen_time_statistics_row_get_history_file (self));
      break;
    case PROP_SELECTED_DATE:
      g_value_set_boxed (value, cc_screen_time_statistics_row_get_selected_date (self));
      break;
    case PROP_DAILY_LIMIT:
      g_value_set_uint (value, cc_screen_time_statistics_row_get_daily_limit (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
cc_screen_time_statistics_row_set_property (GObject      *object,
                                            guint         property_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (object);

  switch ((CcScreenTimeStatisticsRowProperty) property_id)
    {
    case PROP_HISTORY_FILE:
      cc_screen_time_statistics_row_set_history_file (self, g_value_get_object (value));
      break;
    case PROP_SELECTED_DATE:
      cc_screen_time_statistics_row_set_selected_date (self, g_value_get_boxed (value));
      break;
    case PROP_DAILY_LIMIT:
      cc_screen_time_statistics_row_set_daily_limit (self, g_value_get_uint (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_screen_time_statistics_row_dispose (GObject *object)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (object);

  if (self->history_file_monitor != NULL)
    g_file_monitor_cancel (self->history_file_monitor);
  if (self->history_file_monitor_changed_id != 0)
    g_signal_handler_disconnect (self->history_file_monitor, self->history_file_monitor_changed_id);
  self->history_file_monitor_changed_id = 0;
  g_clear_object (&self->history_file_monitor);
  g_clear_object (&self->history_file);

  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_SCREEN_TIME_STATISTICS_ROW);

  G_OBJECT_CLASS (cc_screen_time_statistics_row_parent_class)->dispose (object);
}

static void
cc_screen_time_statistics_row_finalize (GObject *object)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (object);

  g_clear_pointer (&self->model.screen_time_per_day, g_free);

  /* Should have been freed on unmap */
  g_assert (self->update_timeout_source == NULL);

  G_OBJECT_CLASS (cc_screen_time_statistics_row_parent_class)->finalize (object);
}

static void
cc_screen_time_statistics_row_map (GtkWidget *widget)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (widget);

  GTK_WIDGET_CLASS (cc_screen_time_statistics_row_parent_class)->map (widget);

  maybe_enable_update_timeout (self);
}

static void
cc_screen_time_statistics_row_unmap (GtkWidget *widget)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (widget);

  GTK_WIDGET_CLASS (cc_screen_time_statistics_row_parent_class)->unmap (widget);

  maybe_enable_update_timeout (self);
}


static gboolean
is_day_in_model (CcScreenTimeStatisticsRow *self,
                 const GDate               *day)
{
  int days_diff = g_date_days_between (&self->model.start_date, day);
  return (days_diff >= 0 && days_diff < self->model.n_days);
}

static guint
get_screen_time_for_day (CcScreenTimeStatisticsRow *self,
                         const GDate               *day)
{
  int days_diff = g_date_days_between (&self->model.start_date, day);
  g_assert (is_day_in_model (self, day));
  return self->model.screen_time_per_day[(unsigned int) days_diff];
}

static char *
format_hours_and_minutes (unsigned int minutes,
                          gboolean omit_minutes_if_zero)
{
  unsigned int hours = minutes / 60;
  minutes %= 60;

  /* Technically we should be formatting these units as per the SI Brochure,
   * table 8 and §5.4.3: with a 0+00A0 (non-breaking space) between the value
   * and unit; and using ‘min’ as the unit for minutes, not ‘m’.
   *
   * However, space is very restricted here, so we’re optimising for that.
   * Given that the whole panel is about screen *time*, hopefully the meaning of
   * the numbers should be obvious. */

  if (hours == 0 && minutes > 0)
    {
      /* Translators: This is a duration in minutes, for example ‘15m’ for 15 minutes.
       * Use whatever shortest unit label is used for minutes in your locale. */
      return g_strdup_printf (_("%um"), minutes);
    }
  else if (minutes == 0)
    {
      /* Translators: This is a duration in hours, for example ‘2h’ for 2 hours.
       * Use whatever shortest unit label is used for hours in your locale. */
      return g_strdup_printf (_("%uh"), hours);
    }
  else
    {
      /* Translators: This is a duration in hours and minutes, for example
       * ‘3h 15m’ for 3 hours and 15 minutes. Use whatever shortest unit label
       * is used for hours and minutes in your locale. */
      return g_strdup_printf (_("%uh %um"), hours, minutes);
    }
}

static void
label_set_text_hours_and_minutes (GtkLabel     *label,
                                  unsigned int  minutes)
{
  g_autofree char *text = format_hours_and_minutes (minutes, FALSE);
  gtk_label_set_text (label, text);
}

/**
 * get_week_start:
 *
 * Gets the first week day for the current locale, expressed as a
 * number in the range 0..6, representing week days from Sunday to
 * Saturday.
 *
 * Returns: A number representing the first week day for the current
 *          locale
 */
/* Copied from gtkcalendar.c and shell-util.c */
static unsigned int
get_week_start (void)
{
  int week_start;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  union { unsigned int word; char *string; } langinfo;
  int week_1stday = 0;
  int first_weekday = 1;
  guint week_origin;
#else
  char *gtk_week_start;
#endif

#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
  langinfo.string = nl_langinfo (_NL_TIME_FIRST_WEEKDAY);
  first_weekday = langinfo.string[0];
  langinfo.string = nl_langinfo (_NL_TIME_WEEK_1STDAY);
  week_origin = langinfo.word;
  if (week_origin == 19971130) /* Sunday */
    week_1stday = 0;
  else if (week_origin == 19971201) /* Monday */
    week_1stday = 1;
  else
    g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.\n");

  week_start = (week_1stday + first_weekday - 1) % 7;
#else
  /* Use a define to hide the string from xgettext */
# define GTK_WEEK_START "calendar:week_start:0"
  gtk_week_start = dgettext ("gtk40", GTK_WEEK_START);

  if (strncmp (gtk_week_start, "calendar:week_start:", 20) == 0)
    week_start = *(gtk_week_start + 20) - '0';
  else
    week_start = -1;

  if (week_start < 0 || week_start > 6)
    {
      g_warning ("Whoever translated calendar:week_start:0 for GTK+ "
                 "did so wrongly.\n");
      return 0;
    }
#endif

  return week_start;
}

static void
get_first_day_of_week (const GDate *date,
                       GDate       *out_new_date)
{
  unsigned int week_start = get_week_start (); /* 0 = Sunday, 1 = Monday, 2 = Tuesday etc. */
  GDateWeekday week_start_as_weekday = (week_start == 0) ? G_DATE_SUNDAY : (GDateWeekday) week_start;
  GDateWeekday date_weekday = g_date_get_weekday (date);
  int weekday_diff;

  *out_new_date = *date;
  weekday_diff = date_weekday - week_start_as_weekday;
  if (weekday_diff >= 0)
    g_date_subtract_days (out_new_date, weekday_diff);
  else
    g_date_subtract_days (out_new_date, 7 + weekday_diff);

  /* The first day of the week must be no later than @date */
  g_assert (g_date_days_between (out_new_date, date) >= 0);
}

static void
get_last_day_of_week (const GDate *date,
                      GDate       *out_new_date)
{
  get_first_day_of_week (date, out_new_date);

  g_date_add_days (out_new_date, 6);

  /* The last day of the week must be no earlier than @date */
  g_assert (g_date_days_between (date, out_new_date) >= 0);
}

static void
get_today (GDate *today)
{
  time_t now = time (NULL);
  g_assert (now != (time_t) -1);  /* can only happen if the argument is non-NULL */
  g_date_set_time_t (today, now);
}

static gboolean
is_today (const GDate *date)
{
  GDate today;
  get_today (&today);
  return (g_date_compare (&today, date) == 0);
}

/* We can’t just use g_date_get_{monday,sunday}_week_of_year() because there
 * are some countries (such as Egypt) where the week starts on a Saturday.
 *
 * FIXME: date_get_week_of_year() can be replaced with new API from GLib once
 * that’s implemented; see https://gitlab.gnome.org/GNOME/glib/-/issues/3617 */
static unsigned int
date_get_week_of_year (const GDate  *date,
                       GDateWeekday  first_day_of_week)
{
  GDate first_day_of_year;
  unsigned int n_days_before_first_week;

  g_return_val_if_fail (g_date_valid (date), 0);

  g_date_clear (&first_day_of_year, 1);
  g_date_set_dmy (&first_day_of_year, 1, 1, g_date_get_year (date));

  n_days_before_first_week = (first_day_of_week - g_date_get_weekday (&first_day_of_year) + 7) % 7;
  return (g_date_get_day_of_year (date) + 6 - n_days_before_first_week) / 7;
}

static unsigned int
get_week_of_year (const GDate *date)
{
  unsigned int week_start = get_week_start (); /* 0 = Sunday, 1 = Monday, 2 = Tuesday etc. */
  GDateWeekday week_start_as_weekday = (week_start == 0) ? G_DATE_SUNDAY : (GDateWeekday) week_start;
  unsigned int week_of_year = date_get_week_of_year (date, week_start_as_weekday);

  /* Safety checks */
  if (week_start == 0)
    g_assert (week_of_year == g_date_get_sunday_week_of_year (date));
  else if (week_start == 1)
    g_assert (week_of_year == g_date_get_monday_week_of_year (date));

  return week_of_year;
}

static gboolean
is_this_week (const GDate *date)
{
  GDate today;
  unsigned int todays_week, dates_week;

  get_today (&today);
  todays_week = get_week_of_year (&today);
  dates_week = get_week_of_year (date);

  return (todays_week == dates_week);
}

/* Behaviour is undefined if model is unset. If there is no data for the given
 * @day_of_week (which can happen if the model is set but relatively
 * unpopulated), the result (@out_average) is undefined and FALSE is returned. */
static gboolean
calculate_average_screen_time_for_day_of_week (CcScreenTimeStatisticsRow *self,
                                               GDateWeekday               day_of_week,
                                               unsigned int              *out_average)
{
  GDateWeekday start_day_of_week = g_date_get_weekday (&self->model.start_date);
  size_t offset;
  unsigned int n;
  double sum;

  g_assert (start_day_of_week != G_DATE_BAD_WEEKDAY);
  g_assert (day_of_week != G_DATE_BAD_WEEKDAY);

  /* add 7 to the difference to ensure it’s positive */
  offset = (7 + (day_of_week - start_day_of_week)) % 7;
  sum = 0.0;
  n = 0;

  for (size_t i = offset; i < self->model.n_days; i += 7)
    {
      sum += self->model.screen_time_per_day[i];
      n++;
    }

  if (out_average != NULL)
    *out_average = (n != 0) ? sum / n : 0;

  return (n != 0);
}

/* Behaviour is undefined if model is unset. If the model is set, but the
 * chosen week lies partially outside the model, then zero will be assumed as
 * the screen time for the days outside the model. */
static unsigned int
calculate_total_screen_time_for_week (CcScreenTimeStatisticsRow *self,
                                      const GDate               *first_day_of_week)
{
  double sum;
  const int offset = g_date_days_between (&self->model.start_date, first_day_of_week);

  sum = 0.0;
  for (int i = 0; i < 7; i++)
    sum += (offset + i >= 0 && offset + i < self->model.n_days) ? self->model.screen_time_per_day[offset + i] : 0;

  return sum;
}

/* Behaviour is undefined if model is empty. */
static unsigned int
calculate_average_screen_time_per_week (CcScreenTimeStatisticsRow *self)
{
  /* Theoretically we want to group the screen_time_per_day values into complete
   * weeks, and then average that set of weeks. In practice, this equates to
   * summing all the screen_time_per_day values except any from the final
   * incomplete week, and then dividing by the number of whole weeks.
   *
   * If there’s less than one week of data, just use the sum of all the data. */
  const unsigned int n_days_rounded = self->model.n_days - (self->model.n_days % 7);
  const unsigned int n_complete_weeks = n_days_rounded / 7;
  double sum;

  sum = 0.0;
  for (size_t i = 0; i < n_days_rounded; i++)
    sum += self->model.screen_time_per_day[i];

  return (n_complete_weeks != 0) ? sum / n_complete_weeks : sum;
}

typedef enum
{
  USER_STATE_INACTIVE = 0,
  USER_STATE_ACTIVE = 1,
} UserState;

static void allocate_duration_to_days (const GDate *model_start_date,
                                       GArray      *model_screen_time_per_day,
                                       uint64_t     start_wall_time_secs,
                                       uint64_t     duration_secs);
static void allocate_duration_to_day (const GDate *model_start_date,
                                      GArray      *model_screen_time_per_day,
                                      GDateTime   *start_date_time,
                                      uint64_t     duration_secs);

static gboolean
set_json_error (const char  *history_file_path,
                GError     **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
               _("Failed to load session history file ‘%s’: %s"),
               history_file_path, _("Invalid file structure"));
  return FALSE;
}

static gboolean
load_session_active_history_data (CcScreenTimeStatisticsRow  *self,
                                  GDate                      *out_new_model_start_date,
                                  size_t                     *out_new_model_n_days,
                                  double                    **out_new_model_screen_time_per_day,
                                  GError                    **error)
{
  g_autofree char *history_file_path = NULL;
  g_autoptr(JsonParser) parser = NULL;
  JsonNode *root;
  JsonArray *root_array;
  uint64_t now_secs = g_get_real_time () / G_USEC_PER_SEC;
  uint64_t prev_wall_time_secs = 0;
  UserState prev_new_state = USER_STATE_INACTIVE;
  GDate new_model_start_date;
  g_autoptr(GArray) new_model_screen_time_per_day = NULL;  /* (element-type double) */

  g_date_clear (&new_model_start_date, 1);

  /* Set up in case of error. */
  if (out_new_model_start_date != NULL)
    g_date_clear (out_new_model_start_date, 1);
  if (out_new_model_n_days != NULL)
    *out_new_model_n_days = 0;
  if (out_new_model_screen_time_per_day != NULL)
    *out_new_model_screen_time_per_day = NULL;

  /* Load and parse the session active history file, written by gnome-shell.
   * See `timeLimitsManager.js` in gnome-shell for the code which writes this
   * file, and a description of the format. */
  if (self->history_file == NULL)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   _("Failed to load session history file: %s"),
                   _("File is empty"));
      return FALSE;
    }

  history_file_path = g_file_get_path (self->history_file);
  parser = json_parser_new_immutable ();
  if (!json_parser_load_from_mapped_file (parser, history_file_path, error))
    return FALSE;

  root = json_parser_get_root (parser);
  if (!JSON_NODE_HOLDS_ARRAY (root))
    return set_json_error (history_file_path, error);

  root_array = json_node_get_array (root);
  g_assert (root_array != NULL);

  for (unsigned int i = 0; i < json_array_get_length (root_array); i++)
    {
      JsonNode *element = json_array_get_element (root_array, i);
      JsonObject *element_object;
      JsonNode *old_state_member, *new_state_member, *wall_time_secs_member;
      int64_t old_state, new_state, wall_time_secs;

      if (!JSON_NODE_HOLDS_OBJECT (element))
        return set_json_error (history_file_path, error);

      element_object = json_node_get_object (element);
      g_assert (element_object != NULL);

      old_state_member = json_object_get_member (element_object, "oldState");
      new_state_member = json_object_get_member (element_object, "newState");
      wall_time_secs_member = json_object_get_member (element_object, "wallTimeSecs");

      if (old_state_member == NULL || !JSON_NODE_HOLDS_VALUE (old_state_member) ||
          new_state_member == NULL || !JSON_NODE_HOLDS_VALUE (new_state_member) ||
          wall_time_secs_member == NULL || !JSON_NODE_HOLDS_VALUE (wall_time_secs_member))
        return set_json_error (history_file_path, error);

      old_state = json_node_get_int (old_state_member);
      new_state = json_node_get_int (new_state_member);
      wall_time_secs = json_node_get_int (wall_time_secs_member);

      if (old_state == new_state ||
          wall_time_secs <= prev_wall_time_secs ||
          wall_time_secs < 0 ||
          wall_time_secs > now_secs ||
          (old_state != USER_STATE_INACTIVE && old_state != USER_STATE_ACTIVE) ||
          (new_state != USER_STATE_INACTIVE && new_state != USER_STATE_ACTIVE))
        return set_json_error (history_file_path, error);

      /* Set up the model if this is the first iteration */
      if (!g_date_valid (&new_model_start_date))
        {
          g_date_set_time_t (&new_model_start_date, wall_time_secs);
          new_model_screen_time_per_day = g_array_new (FALSE, TRUE, sizeof (double));
        }

      /* Interpret the data */
      if (new_state == USER_STATE_INACTIVE && prev_wall_time_secs > 0)
        {
          uint64_t duration_secs = wall_time_secs - prev_wall_time_secs;
          allocate_duration_to_days (&new_model_start_date, new_model_screen_time_per_day,
                                     prev_wall_time_secs, duration_secs);
        }

      prev_wall_time_secs = wall_time_secs;
      prev_new_state = new_state;
    }

  /* Was the final transition open-ended? */
  if (prev_wall_time_secs > 0 && prev_new_state == USER_STATE_ACTIVE)
    {
      uint64_t duration_secs = now_secs - prev_wall_time_secs;
      allocate_duration_to_days (&new_model_start_date, new_model_screen_time_per_day,
                                 prev_wall_time_secs, duration_secs);
    }

  /* Was the file empty? */
  if (new_model_screen_time_per_day == NULL || new_model_screen_time_per_day->len == 0)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                   _("Failed to load session history file ‘%s’: %s"),
                   history_file_path, _("File is empty"));
      return FALSE;
    }

  /* Success! */
  if (out_new_model_start_date != NULL)
    *out_new_model_start_date = new_model_start_date;
  if (out_new_model_n_days != NULL)
    *out_new_model_n_days = new_model_screen_time_per_day->len;
  if (out_new_model_screen_time_per_day != NULL)
    *out_new_model_screen_time_per_day = (double *) g_array_free (g_steal_pointer (&new_model_screen_time_per_day), FALSE);

  return TRUE;
}

/* Take the time period [start_wall_time_secs, start_wall_time_secs + duration_secs]
 * and add it to the model, splitting it between day boundaries if needed, and
 * extending the `GArray` if needed. */
static void
allocate_duration_to_days (const GDate *model_start_date,
                           GArray      *model_screen_time_per_day,
                           uint64_t     start_wall_time_secs,
                           uint64_t     duration_secs)
{
  g_autoptr(GDateTime) start_date_time = NULL;

  start_date_time = g_date_time_new_from_unix_local (start_wall_time_secs);

  while (duration_secs > 0)
    {
      g_autoptr(GDateTime) start_of_day = NULL, start_of_next_day = NULL;
      g_autoptr(GDateTime) new_start_date_time = NULL;
      GTimeSpan span_usecs;
      uint64_t span_secs;

      start_of_day = g_date_time_new_local (g_date_time_get_year (start_date_time),
                                            g_date_time_get_month (start_date_time),
                                            g_date_time_get_day_of_month (start_date_time),
                                            0, 0, 0);
      g_assert (start_of_day != NULL);
      start_of_next_day = g_date_time_add_days (start_of_day, 1);
      g_assert (start_of_next_day != NULL);

      span_usecs = g_date_time_difference (start_of_next_day, start_date_time);
      span_secs = span_usecs / G_USEC_PER_SEC;
      if (span_secs > duration_secs)
        span_secs = duration_secs;

      allocate_duration_to_day (model_start_date, model_screen_time_per_day,
                                start_date_time, span_secs);

      duration_secs -= span_secs;
      new_start_date_time = g_date_time_add_seconds (start_date_time, span_secs);
      g_date_time_unref (start_date_time);
      start_date_time = g_steal_pointer (&new_start_date_time);
    }
}

/* Take the time period [start_date_time, start_date_time + duration_secs]
 * and add it to the model, extending the `GArray` if needed. The time period
 * *must not* cross a day boundary, i.e. it’s invalid to call this function
 * with `start_date_time` as 23:00 on a day, and `duration_secs` as 2h.
 *
 * Note that @model_screen_time_per_day is in minutes, whereas @duration_secs
 * is in seconds. */
static void
allocate_duration_to_day (const GDate *model_start_date,
                          GArray      *model_screen_time_per_day,
                          GDateTime   *start_date_time,
                          uint64_t     duration_secs)
{
  GDate start_date;
  int diff_days;
  double *element;

  g_date_clear (&start_date, 1);
  g_date_set_dmy (&start_date,
                  g_date_time_get_day_of_month (start_date_time),
                  g_date_time_get_month (start_date_time),
                  g_date_time_get_year (start_date_time));

  diff_days = g_date_days_between (model_start_date, &start_date);
  g_assert (diff_days >= 0);

  /* If the new day is outside the range of the model, insert it at the right
   * index. This will automatically create the indices between, and initialise
   * them to zero, which is what we want. */
  if (diff_days >= model_screen_time_per_day->len)
    {
      const double new_val = 0.0;
      g_array_insert_val (model_screen_time_per_day, diff_days, new_val);
    }

  element = &g_array_index (model_screen_time_per_day, double, diff_days);
  *element += duration_secs / 60.0;
}

/* Need to call update_ui_for_model_or_selected_date() after this to reflect
 * the new model in the UI. */
static void
update_model (CcScreenTimeStatisticsRow *self)
{
  GDate new_model_start_date;
  size_t new_model_n_days = 0;
  g_autofree double *new_model_screen_time_per_day = NULL;
  g_autoptr(GError) local_error = NULL;

  if (!load_session_active_history_data (self, &new_model_start_date,
                                         &new_model_n_days, &new_model_screen_time_per_day,
                                         &local_error))
    {
      /* Not sure if it helps to display this error in the UI, so just log it
       * for now. `G_FILE_ERROR_NOENT` is used when the file doesn’t exist, or
       * exists but is empty, which could happen on new systems before the shell
       * logs anything. */
      if (!g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning ("Error loading session history JSON: %s", local_error->message);
      return;
    }

  /* Commit the new model. */
  g_free (self->model.screen_time_per_day);

  self->model.start_date = new_model_start_date;
  self->model.n_days = new_model_n_days;
  self->model.screen_time_per_day = g_steal_pointer (&new_model_screen_time_per_day);
}

static void
update_ui_for_model_or_selected_date (CcScreenTimeStatisticsRow *self)
{
  size_t retval;
  char selected_date_text[100] = { 0, };
  unsigned int screen_time_for_selected_date;
  const char * const average_weekday_labels[] = {
    NULL,  /* G_DATE_BAD_WEEKDAY */
    _("Average Monday"),
    _("Average Tuesday"),
    _("Average Wednesday"),
    _("Average Thursday"),
    _("Average Friday"),
    _("Average Saturday"),
    _("Average Sunday"),
  };
  unsigned int average_screen_time_for_selected_day_of_week;
  GDate today;
  GDate first_day_of_selected_week;
  GDate last_day_of_selected_week;
  g_autofree char *week_date_text = NULL;
  unsigned int screen_time_for_selected_week;
  unsigned int screen_time_for_average_week;
  gboolean data_available;
  g_autofree double *data_slice = NULL;
  int model_offset;

  /* The only way it’s possible to *not* have a date selected is if no data is
   * available. */
  data_available = (g_date_valid (&self->selected_date) && self->model.n_days > 0);
  gtk_stack_set_visible_child_name (self->data_stack, data_available ? "main" : "no-data");
  bar_chart_update_accessible_description (self);

  if (!data_available)
    return;

  get_today (&today);
  get_first_day_of_week (&self->selected_date, &first_day_of_selected_week);
  get_last_day_of_week (&self->selected_date, &last_day_of_selected_week);

  /* Do we need to change the data in the chart because the selected date is
   * outside the currently shown range? */
  model_offset = g_date_days_between (&self->model.start_date, &first_day_of_selected_week);

  /* If we naively took a slice of size 7 starting at
   * `self->model.screen_time_per_day + model_offset`, there would potentially
   * be out-of-bounds accesses at either end of the model. Allocate a temporary
   * buffer to avoid that, and initialise its values to NAN to indicate
   * unknown/unset data values. */
  data_slice = g_new (double, 7);
  for (int i = 0; i < 7; i++)
    data_slice[i] = (model_offset + i >= 0 && model_offset + i < self->model.n_days) ? self->model.screen_time_per_day[model_offset + i] : NAN;

  cc_bar_chart_set_data (self->bar_chart, data_slice, 7);

  /* Update UI */
  cc_bar_chart_set_selected_index (self->bar_chart, TRUE,
                                   g_date_days_between (&first_day_of_selected_week, &self->selected_date));

  if (is_today (&self->selected_date))
    {
      g_strlcpy (selected_date_text, _("Today"), sizeof (selected_date_text));
    }
  else
    {
      /* Translators: This a medium-length date, for example ‘15 April’ */
      retval = g_date_strftime (selected_date_text, sizeof (selected_date_text), _("%-d %B"), &self->selected_date);
      g_assert (retval != 0);
    }

  gtk_label_set_label (self->selected_date_label, selected_date_text);

  if (is_day_in_model (self, &self->selected_date))
    {
      screen_time_for_selected_date = get_screen_time_for_day (self, &self->selected_date);
      label_set_text_hours_and_minutes (self->selected_screen_time_label, screen_time_for_selected_date);
    }
  else
    {
      gtk_label_set_label (self->selected_screen_time_label, _("No Data"));
    }

  /* We can’t use g_date_strftime() for this, as in some locales weekdays have
   * different grammatical genders, and the ‘Average’ prefix needs to match that. */
  gtk_label_set_text (self->selected_average_label,
                      average_weekday_labels[g_date_get_weekday (&self->selected_date)]);

  if (calculate_average_screen_time_for_day_of_week (self, g_date_get_weekday (&self->selected_date), &average_screen_time_for_selected_day_of_week))
    label_set_text_hours_and_minutes (self->selected_average_value_label, average_screen_time_for_selected_day_of_week);
  else
    gtk_label_set_text (self->selected_average_value_label, _("No Data"));

  if (is_this_week (&self->selected_date))
    {
      week_date_text = g_strdup_printf (_("This Week"));
    }
  else if (g_date_get_month (&first_day_of_selected_week) == g_date_get_month (&last_day_of_selected_week))
    {
      char month_name[100] = { 0, };

      retval = g_date_strftime (month_name, sizeof (month_name), "%B", &first_day_of_selected_week);
      g_assert (retval != 0);

      /* Translators: This is a range of days within a given month.
       * For example ‘20–27 April’. The dash is an en-dash. */
      week_date_text = g_strdup_printf (_("%u–%u %s"),
                                        g_date_get_day (&first_day_of_selected_week),
                                        g_date_get_day (&last_day_of_selected_week),
                                        month_name);
    }
  else
    {
      char first_month_name[100] = { 0, };
      char last_month_name[100] = { 0, };

      retval = g_date_strftime (first_month_name, sizeof (first_month_name), "%B", &first_day_of_selected_week);
      g_assert (retval != 0);
      retval = g_date_strftime (last_month_name, sizeof (last_month_name), "%B", &last_day_of_selected_week);
      g_assert (retval != 0);

      /* Translators: This is a range of days spanning two months.
       * For example, ‘27 April–4 May’. The dash is an en-dash. */
      week_date_text = g_strdup_printf (_("%u %s–%u %s"),
                                        g_date_get_day (&first_day_of_selected_week),
                                        first_month_name,
                                        g_date_get_day (&last_day_of_selected_week),
                                        last_month_name);
    }

  gtk_label_set_label (self->week_date_label, week_date_text);

  screen_time_for_selected_week = calculate_total_screen_time_for_week (self, &first_day_of_selected_week);
  label_set_text_hours_and_minutes (self->week_screen_time_label, screen_time_for_selected_week);

  screen_time_for_average_week = calculate_average_screen_time_per_week (self);
  label_set_text_hours_and_minutes (self->week_average_value_label, screen_time_for_average_week);

  /* Update button sensitivity. */
  gtk_widget_set_sensitive (GTK_WIDGET (self->previous_week_button),
                            g_date_days_between (&self->model.start_date, &first_day_of_selected_week) > 0);
  gtk_widget_set_sensitive (GTK_WIDGET (self->next_week_button),
                            g_date_days_between (&last_day_of_selected_week, &today) > 0);
}

static char *
bar_chart_continuous_axis_label_cb (CcBarChart *chart,
                                    double      value,
                                    void       *user_data)
{
  if (isnan (value))
    return g_strdup ("");

  /* @value is in minutes already */
  return format_hours_and_minutes (value, TRUE);
}

static double
bar_chart_continuous_axis_grid_line_cb (CcBarChart   *chart,
                                        unsigned int  idx,
                                        void         *user_data)
{
  /* A grid line every 2h */
  return idx * 2 * 60;
}

static void
bar_chart_update_accessible_description (CcScreenTimeStatisticsRow *self)
{
  g_autofree char *description = NULL;

  if (g_date_valid (&self->selected_date) && self->daily_limit_minutes != 0)
    {
      char date_str[200];
      size_t retval;
      g_autofree char *daily_limit_str = NULL;
      GDate first_day_of_week;

      get_first_day_of_week (&self->selected_date, &first_day_of_week);
      retval = g_date_strftime (date_str, sizeof (date_str), "%x", &first_day_of_week);
      g_assert (retval != 0);

      daily_limit_str = cc_util_time_to_string_text (self->daily_limit_minutes * 60 * 1000);

      /* Translators: The first placeholder is a formatted date string
       * (formatted using the `%x` strftime placeholder, which gives the
       * preferred date representation for the current locale without the time).
       * The second placeholder is a formatted time duration (for example,
       * ‘3 hours’ or ‘25 minutes’). */
      description = g_strdup_printf (_("Bar chart of screen time usage over the "
                                       "week starting %s. A line is overlayed at "
                                       "the %s mark to indicate the "
                                       "configured screen time limit."),
                                     date_str, daily_limit_str);
    }
  else if (g_date_valid (&self->selected_date))
    {
      char date_str[200];
      size_t retval;
      GDate first_day_of_week;

      get_first_day_of_week (&self->selected_date, &first_day_of_week);
      retval = g_date_strftime (date_str, sizeof (date_str), "%x", &first_day_of_week);
      g_assert (retval != 0);

      /* Translators: The placeholder is a formatted date string (formatted
       * using the `%x` strftime placeholder, which gives the preferred date
       * representation for the current locale without the time). */
      description = g_strdup_printf (_("Bar chart of screen time usage over the "
                                       "week starting %s."),
                                     date_str);
    }
  else
    {
      description = g_strdup_printf (_("Placeholder for a bar chart of screen "
                                       "time usage. No data is currently "
                                       "available."));
    }

  gtk_accessible_update_property (GTK_ACCESSIBLE (self->bar_chart),
                                  GTK_ACCESSIBLE_PROPERTY_DESCRIPTION, description,
                                  -1);
}

static void
bar_chart_notify_selected_index_cb (GObject    *object,
                                    GParamSpec *pspec,
                                    gpointer    user_data)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (user_data);
  GDate new_selected_date;
  GDate *new_selected_date_ptr;
  size_t idx = 0;

  if (cc_bar_chart_get_selected_index (self->bar_chart, &idx))
    {
      get_first_day_of_week (&self->selected_date, &new_selected_date);
      g_date_add_days (&new_selected_date, idx);
      new_selected_date_ptr = &new_selected_date;
    }
  else
    {
      new_selected_date_ptr = NULL;
    }

  cc_screen_time_statistics_row_set_selected_date (self, new_selected_date_ptr);
}

static void
previous_week_button_clicked_cb (GtkButton *button,
                                 gpointer   user_data)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (user_data);
  GDate first_day_of_previous_week;

  get_first_day_of_week (&self->selected_date, &first_day_of_previous_week);
  g_date_subtract_days (&first_day_of_previous_week, 7);

  cc_screen_time_statistics_row_set_selected_date (self, &first_day_of_previous_week);
}

static void
next_week_button_clicked_cb (GtkButton *button,
                             gpointer   user_data)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (user_data);
  GDate first_day_of_next_week;

  get_first_day_of_week (&self->selected_date, &first_day_of_next_week);
  g_date_add_days (&first_day_of_next_week, 7);

  cc_screen_time_statistics_row_set_selected_date (self, &first_day_of_next_week);
}

static void
history_file_monitor_changed_cb (GFileMonitor      *monitor,
                                 GFile             *file,
                                 GFile             *other_file,
                                 GFileMonitorEvent  event_type,
                                 gpointer           user_data)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (user_data);

  g_assert (self->history_file != NULL);

  g_debug ("%s: Reloading history file ‘%s’ as it’s changed", G_STRFUNC, g_file_peek_path (self->history_file));

  update_model (self);
  update_ui_for_model_or_selected_date (self);
}

static gboolean
history_file_update_timeout_cb (gpointer user_data)
{
  CcScreenTimeStatisticsRow *self = CC_SCREEN_TIME_STATISTICS_ROW (user_data);

  g_debug ("%s: Reloading history data due to the passage of time", G_STRFUNC);

  update_model (self);
  update_ui_for_model_or_selected_date (self);

  return G_SOURCE_CONTINUE;
}

static void
maybe_enable_update_timeout (CcScreenTimeStatisticsRow *self)
{
  gboolean should_be_enabled = (self->history_file != NULL && gtk_widget_get_mapped (GTK_WIDGET (self)));
  gboolean is_enabled = (self->update_timeout_source != NULL);

  if (should_be_enabled && !is_enabled)
    {
      self->update_timeout_source = g_timeout_source_new_seconds (60 * 60);
      g_source_set_callback (self->update_timeout_source, G_SOURCE_FUNC (history_file_update_timeout_cb), self, NULL);
      g_source_attach (self->update_timeout_source, NULL);
    }
  else if (is_enabled && !should_be_enabled)
    {
      g_source_destroy (self->update_timeout_source);
      g_clear_pointer (&self->update_timeout_source, g_source_unref);
    }
}

/**
 * cc_screen_time_statistics_row_new:
 *
 * Create a new #CcScreenTimeStatisticsRow.
 *
 * Returns: (transfer full): the new #CcScreenTimeStatisticsRow
 */
CcScreenTimeStatisticsRow *
cc_screen_time_statistics_row_new (void)
{
  return g_object_new (CC_TYPE_SCREEN_TIME_STATISTICS_ROW, NULL);
}

/**
 * cc_screen_time_statistics_row_get_history_file:
 * @self: a #CcScreenTimeStatisticsRow
 *
 * Get the value of #CcScreenTimeStatisticsRow:history-file.
 *
 * Returns: (transfer none) (nullable): history file which has been loaded, or
 *   %NULL if not set
 */
GFile *
cc_screen_time_statistics_row_get_history_file (CcScreenTimeStatisticsRow *self)
{
  g_return_val_if_fail (CC_IS_SCREEN_TIME_STATISTICS_ROW (self), NULL);

  return self->history_file;
}

/**
 * cc_screen_time_statistics_row_set_history_file:
 * @self: a #CcScreenTimeStatisticsRow
 * @selected_date: (transfer none) (nullable): new history file to load, or
 *   %NULL to clear it
 *
 * Set the value of #CcScreenTimeStatisticsRow:history-file.
 */
void
cc_screen_time_statistics_row_set_history_file (CcScreenTimeStatisticsRow *self,
                                                GFile                     *history_file)
{
  g_autoptr(GError) local_error = NULL;

  g_return_if_fail (CC_IS_SCREEN_TIME_STATISTICS_ROW (self));
  g_return_if_fail (history_file == NULL || G_IS_FILE (history_file));

  if (g_set_object (&self->history_file, history_file))
    {
      g_debug ("%s: Loading history file ‘%s’", G_STRFUNC, (history_file != NULL) ? g_file_peek_path (history_file) : "(unset)");

      update_model (self);
      update_ui_for_model_or_selected_date (self);

      /* Monitor the file for changes. */
      if (self->history_file_monitor_changed_id != 0)
        g_signal_handler_disconnect (self->history_file_monitor, self->history_file_monitor_changed_id);
      self->history_file_monitor_changed_id = 0;
      if (self->history_file_monitor != NULL)
        g_file_monitor_cancel (self->history_file_monitor);
      g_clear_object (&self->history_file_monitor);

      if (self->history_file != NULL)
        {
          g_autoptr(GFileMonitor) monitor = NULL;

          monitor = g_file_monitor_file (self->history_file, G_FILE_MONITOR_NONE,
                                         NULL, &local_error);
          if (local_error != NULL)
            g_warning ("Error monitoring history file ‘%s’: %s",
                       g_file_peek_path (self->history_file), local_error->message);
          else
            self->history_file_monitor_changed_id = g_signal_connect (monitor, "changed", G_CALLBACK (history_file_monitor_changed_cb), self);

          g_set_object (&self->history_file_monitor, monitor);
        }

      /* Periodically reload the data so the graph is updated with the passage
       * of time. */
      maybe_enable_update_timeout (self);

      g_object_notify_by_pspec (G_OBJECT (self), props[PROP_HISTORY_FILE]);
    }
}

/**
 * cc_screen_time_statistics_row_get_selected_date:
 * @self: a #CcScreenTimeStatisticsRow
 *
 * Get the value of #CcScreenTimeStatisticsRow:selected-date.
 *
 * Returns: (nullable) (transfer none): currently selected date, or %NULL if no
 *   data is available
 */
const GDate *
cc_screen_time_statistics_row_get_selected_date (CcScreenTimeStatisticsRow *self)
{
  g_return_val_if_fail (CC_IS_SCREEN_TIME_STATISTICS_ROW (self), NULL);

  return g_date_valid (&self->selected_date) ? &self->selected_date : NULL;
}

/**
 * cc_screen_time_statistics_row_set_selected_date:
 * @self: a #CcScreenTimeStatisticsRow
 * @selected_date: (transfer none) (nullable): new selected date, or %NULL if no
 *   data is available
 *
 * Set the value of #CcScreenTimeStatisticsRow:selected-date.
 */
void
cc_screen_time_statistics_row_set_selected_date (CcScreenTimeStatisticsRow *self,
                                                 const GDate               *selected_date)
{
  g_return_if_fail (CC_IS_SCREEN_TIME_STATISTICS_ROW (self));

  if ((!g_date_valid (&self->selected_date) && selected_date == NULL) ||
      (g_date_valid (&self->selected_date) && selected_date != NULL &&
       g_date_compare (&self->selected_date, selected_date) == 0))
    return;

  /* Log the selected date */
    {
      char date_str[200];
      if (selected_date != NULL)
        g_date_strftime (date_str, sizeof (date_str), "%x", selected_date);
      g_debug ("%s: %s", G_STRFUNC, (selected_date != NULL) ? date_str : "(unset)");
    }

  self->selected_date = *selected_date;

  update_ui_for_model_or_selected_date (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SELECTED_DATE]);
}

/**
 * cc_screen_time_statistics_row_get_daily_limit:
 * @self: a #CcScreenTimeStatisticsRow
 *
 * Get the value of #CcScreenTimeStatisticsRow:daily-limit.
 *
 * Returns: the daily computer usage time limit, in minutes, or `0` if unset
 */
unsigned int
cc_screen_time_statistics_row_get_daily_limit (CcScreenTimeStatisticsRow *self)
{
  g_return_val_if_fail (CC_IS_SCREEN_TIME_STATISTICS_ROW (self), 0);

  return self->daily_limit_minutes;
}

/**
 * cc_screen_time_statistics_row_set_daily_limit:
 * @self: a #CcScreenTimeStatisticsRow
 * @daily_limit_minutes: the daily computer usage time limit, in minutes, or
 *   `0` to unset it
 *
 * Set #CcScreenTimeStatisticsRow:daily-limit.
 */
void
cc_screen_time_statistics_row_set_daily_limit (CcScreenTimeStatisticsRow *self,
                                               unsigned int               daily_limit_minutes)
{
  g_return_if_fail (CC_IS_SCREEN_TIME_STATISTICS_ROW (self));

  if (self->daily_limit_minutes == daily_limit_minutes)
    return;

  cc_bar_chart_set_overlay_line_value (self->bar_chart, (daily_limit_minutes > 0) ? daily_limit_minutes : NAN);
  bar_chart_update_accessible_description (self);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_DAILY_LIMIT]);
}
