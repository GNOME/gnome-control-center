/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2013 Kalev Lember <kalevlember@gmail.com>
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "config.h"
#include "cc-datetime-panel.h"
#include "cc-datetime-resources.h"

#include <langinfo.h>
#include <sys/time.h>
#include "cc-timezone-map.h"
#include "timedated.h"
#include "date-endian.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <gdesktop-enums.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#include <libgnome-desktop/gnome-wall-clock.h>
#include <polkit/polkit.h>

/* FIXME: This should be "Etc/GMT" instead */
#define DEFAULT_TZ "Europe/London"
#define GETTEXT_PACKAGE_TIMEZONES GETTEXT_PACKAGE "-timezones"

CC_PANEL_REGISTER (CcDateTimePanel, cc_date_time_panel)

#define DATE_TIME_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DATE_TIME_PANEL, CcDateTimePanelPrivate))

enum {
  CITY_COL_CITY,
  CITY_COL_REGION,
  CITY_COL_CITY_TRANSLATED,
  CITY_COL_REGION_TRANSLATED,
  CITY_COL_ZONE,
  CITY_NUM_COLS
};

enum {
  REGION_COL_REGION,
  REGION_COL_REGION_TRANSLATED,
  REGION_NUM_COLS
};

#define W(x) (GtkWidget*) gtk_builder_get_object (priv->builder, x)

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

struct _CcDateTimePanelPrivate
{
  GtkBuilder *builder;
  GtkWidget *map;

  TzLocation *current_location;

  GtkTreeModel *locations;
  GtkTreeModelFilter *city_filter;

  GDateTime *date;

  GSettings *settings;
  GDesktopClockFormat clock_format;
  gboolean ampm_available;
  GtkWidget *am_label;
  GtkWidget *pm_label;

  GnomeWallClock *clock_tracker;

  Timedate1 *dtm;
  GCancellable *cancellable;

  GPermission *permission;
};

static void update_time (CcDateTimePanel *self);
static void change_time (CcDateTimePanel *self);


static void
cc_date_time_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_date_time_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_date_time_panel_dispose (GObject *object)
{
  CcDateTimePanelPrivate *priv = CC_DATE_TIME_PANEL (object)->priv;

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }

  g_clear_object (&priv->builder);
  g_clear_object (&priv->clock_tracker);
  g_clear_object (&priv->dtm);
  g_clear_object (&priv->permission);
  g_clear_object (&priv->settings);

  g_clear_pointer (&priv->date, g_date_time_unref);

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->dispose (object);
}

static GPermission *
cc_date_time_panel_get_permission (CcPanel *panel)
{
  CcDateTimePanelPrivate *priv = CC_DATE_TIME_PANEL (panel)->priv;

  return priv->permission;
}

static const char *
cc_date_time_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/clock";
}

static void
cc_date_time_panel_class_init (CcDateTimePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDateTimePanelPrivate));

  object_class->get_property = cc_date_time_panel_get_property;
  object_class->set_property = cc_date_time_panel_set_property;
  object_class->dispose = cc_date_time_panel_dispose;

  panel_class->get_permission = cc_date_time_panel_get_permission;
  panel_class->get_help_uri   = cc_date_time_panel_get_help_uri;
}

static void clock_settings_changed_cb (GSettings       *settings,
                                       gchar           *key,
                                       CcDateTimePanel *panel);

static void
change_clock_settings (GObject         *gobject,
                       GParamSpec      *pspec,
                       CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  GDesktopClockFormat value;
  const char *active_id;

  g_signal_handlers_block_by_func (priv->settings, clock_settings_changed_cb,
                                   panel);

  active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (W ("format_combobox")));
  if (!g_strcmp0 (active_id, "24h"))
    value = G_DESKTOP_CLOCK_FORMAT_24H;
  else
    value = G_DESKTOP_CLOCK_FORMAT_12H;

  g_settings_set_enum (priv->settings, CLOCK_FORMAT_KEY, value);
  priv->clock_format = value;

  update_time (panel);

  g_signal_handlers_unblock_by_func (priv->settings, clock_settings_changed_cb,
                                     panel);
}

static void
clock_settings_changed_cb (GSettings       *settings,
                           gchar           *key,
                           CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  GtkWidget *format_combo;
  GDesktopClockFormat value;

  value = g_settings_get_enum (settings, CLOCK_FORMAT_KEY);
  priv->clock_format = value;

  format_combo = W ("format_combobox");

  g_signal_handlers_block_by_func (format_combo, change_clock_settings, panel);

  if (value == G_DESKTOP_CLOCK_FORMAT_24H)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (format_combo), "24h");
  else
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (format_combo), "12h");

  update_time (panel);

  g_signal_handlers_unblock_by_func (format_combo, change_clock_settings, panel);
}

static gboolean
am_pm_button_clicked (GtkWidget *button,
                      CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *stack;
  GtkWidget *visible_child;

  stack = W ("am_pm_stack");

  visible_child = gtk_stack_get_visible_child (GTK_STACK (stack));
  if (visible_child == priv->am_label)
    gtk_stack_set_visible_child (GTK_STACK (stack), priv->pm_label);
  else
    gtk_stack_set_visible_child (GTK_STACK (stack), priv->am_label);

  change_time (self);

  return TRUE;
}

/* Update the widgets based on the system time */
static void
update_time (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *h_spinbutton;
  GtkWidget *m_spinbutton;
  GtkWidget *am_pm_button;
  char *label;
  gint hour;
  gint minute;
  gboolean use_ampm;

  h_spinbutton = W("h_spinbutton");
  m_spinbutton = W("m_spinbutton");
  am_pm_button = W("am_pm_button");

  g_signal_handlers_block_by_func (h_spinbutton, change_time, self);
  g_signal_handlers_block_by_func (m_spinbutton, change_time, self);
  g_signal_handlers_block_by_func (am_pm_button, am_pm_button_clicked, self);

  if (priv->clock_format == G_DESKTOP_CLOCK_FORMAT_12H && priv->ampm_available)
    use_ampm = TRUE;
  else
    use_ampm = FALSE;

  hour = g_date_time_get_hour (priv->date);
  minute = g_date_time_get_minute (priv->date);

  if (!use_ampm)
    {
      /* Update the hours spinbutton */
      gtk_spin_button_set_range (GTK_SPIN_BUTTON (h_spinbutton), 0, 23);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (h_spinbutton), hour);
    }
  else
    {
      GtkWidget *am_pm_stack;
      gboolean is_pm_time;

      is_pm_time = (hour >= 12);

      /* Update the AM/PM button */
      am_pm_stack = W ("am_pm_stack");
      if (is_pm_time)
        gtk_stack_set_visible_child (GTK_STACK (am_pm_stack), priv->pm_label);
      else
        gtk_stack_set_visible_child (GTK_STACK (am_pm_stack), priv->am_label);

      /* Update the hours spinbutton */
      if (is_pm_time)
        hour -= 12;
      if (hour == 0)
        hour = 12;
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (h_spinbutton), hour);
      gtk_spin_button_set_range (GTK_SPIN_BUTTON (h_spinbutton), 1, 12);
    }

  gtk_widget_set_visible (am_pm_button, use_ampm);

  /* Update the minutes spinbutton */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (m_spinbutton), minute);

  g_signal_handlers_unblock_by_func (h_spinbutton, change_time, self);
  g_signal_handlers_unblock_by_func (m_spinbutton, change_time, self);
  g_signal_handlers_unblock_by_func (am_pm_button, am_pm_button_clicked, self);

  /* Update the time on the listbow row */
  if (use_ampm)
    label = g_date_time_format (priv->date, "%e %B %Y, %l:%M %p");
  else
    label = g_date_time_format (priv->date, "%e %B %Y, %R");

  gtk_label_set_text (GTK_LABEL (W ("datetime_label")), label);
  g_free (label);
}

static void
set_time_cb (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  CcDateTimePanel *self = user_data;
  GError *error;

  error = NULL;
  if (!timedate1_call_set_time_finish (self->priv->dtm,
                                       res,
                                       &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not set system time: %s", error->message);
      g_error_free (error);
    }
  else
    {
      update_time (self);
    }
}

static void
set_timezone_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  CcDateTimePanel *self = user_data;
  GError *error;

  error = NULL;
  if (!timedate1_call_set_timezone_finish (self->priv->dtm,
                                           res,
                                           &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not set system timezone: %s", error->message);
      g_error_free (error);
    }
}

static void
set_using_ntp_cb (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  CcDateTimePanel *self = user_data;
  GError *error;

  error = NULL;
  if (!timedate1_call_set_ntp_finish (self->priv->dtm,
                                      res,
                                      &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not set system to use NTP: %s", error->message);
      g_error_free (error);
    }
}

static void
queue_set_datetime (CcDateTimePanel *self)
{
  gint64 unixtime;

  /* timedated expects number of microseconds since 1 Jan 1970 UTC */
  unixtime = g_date_time_to_unix (self->priv->date);

  timedate1_call_set_time (self->priv->dtm,
                           unixtime * 1000000,
                           FALSE,
                           TRUE,
                           self->priv->cancellable,
                           set_time_cb,
                           self);
}

static void
queue_set_ntp (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  gboolean using_ntp;
  /* for now just do it */
  using_ntp = gtk_switch_get_active (GTK_SWITCH (W("network_time_switch")));

  timedate1_call_set_ntp (self->priv->dtm,
                          using_ntp,
                          TRUE,
                          self->priv->cancellable,
                          set_using_ntp_cb,
                          self);
}

static void
queue_set_timezone (CcDateTimePanel *self)
{
  /* for now just do it */
  if (self->priv->current_location)
    {
      timedate1_call_set_timezone (self->priv->dtm,
                                   self->priv->current_location->zone,
                                   TRUE,
                                   self->priv->cancellable,
                                   set_timezone_cb,
                                   self);
    }
}

static void
change_date (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  guint mon, y, d;
  GDateTime *old_date;

  old_date = priv->date;

  mon = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (W ("month-combobox")));
  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("year-spinbutton")));
  d = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("day-spinbutton")));

  priv->date = g_date_time_new_local (y, mon, d,
                                      g_date_time_get_hour (old_date),
                                      g_date_time_get_minute (old_date),
                                      g_date_time_get_second (old_date));
  g_date_time_unref (old_date);
  queue_set_datetime (self);
}

static void
region_changed_cb (GtkComboBox     *box,
                   CcDateTimePanel *self)
{
  GtkTreeModelFilter *modelfilter;

  modelfilter = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (self->priv->builder, "city-modelfilter"));

  gtk_tree_model_filter_refilter (modelfilter);
}

static void
city_changed_cb (GtkComboBox     *box,
                 CcDateTimePanel *self)
{
  static gboolean inside = FALSE;
  GtkTreeIter iter;
  gchar *zone;

  /* prevent re-entry from location changed callback */
  if (inside)
    return;

  inside = TRUE;

  if (gtk_combo_box_get_active_iter (box, &iter))
    {
      gtk_tree_model_get (gtk_combo_box_get_model (box), &iter,
                          CITY_COL_ZONE, &zone, -1);

      cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->priv->map), zone);

      g_free (zone);
    }

  inside = FALSE;
}

static void
update_timezone (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *widget;
  gchar **split;
  GtkTreeIter iter;
  GtkTreeModel *model;
  TzInfo *tz_info;
  char *label;

  /* tz.c updates the local timezone, which means the spin buttons can be
   * updated with the current time of the new location */

  split = g_strsplit (priv->current_location->zone, "/", 2);

  /* remove underscores */
  g_strdelimit (split[1], "_", ' ');

  /* update region combo */
  widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
                                                 "region_combobox");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  gtk_tree_model_get_iter_first (model, &iter);

  do
    {
      gchar *string;

      gtk_tree_model_get (model, &iter, CITY_COL_CITY, &string, -1);

      if (!g_strcmp0 (string, split[0]))
        {
          g_free (string);
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
          break;
        }
      g_free (string);
    }
  while (gtk_tree_model_iter_next (model, &iter));


  /* update city combo */
  widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
                                                 "city_combobox");
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  gtk_tree_model_filter_refilter ((GtkTreeModelFilter *) gtk_builder_get_object (priv->builder, "city-modelfilter"));
  gtk_tree_model_get_iter_first (model, &iter);

  do
    {
      gchar *string;

      gtk_tree_model_get (model, &iter, CITY_COL_CITY, &string, -1);

      if (!g_strcmp0 (string, split[1]))
        {
          g_free (string);
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
          break;
        }
      g_free (string);
    }
  while (gtk_tree_model_iter_next (model, &iter));

  g_strfreev (split);

  /* Update the timezone on the listbow row */
  tz_info = tz_info_from_location (self->priv->current_location);
  label = g_strdup_printf ("%s (%s)",
                           tz_info->tzname_normal,
                           self->priv->current_location->zone);
  tz_info_free (tz_info);

  gtk_label_set_text (GTK_LABEL (W ("timezone_label")), label);
  g_free (label);
}

static void
location_changed_cb (CcTimezoneMap   *map,
                     TzLocation      *location,
                     CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *region_combo, *city_combo;

  g_debug ("location changed to %s/%s", location->country, location->zone);

  self->priv->current_location = location;

  /* Update the combo boxes */
  region_combo = W("region_combobox");
  city_combo = W("city_combobox");

  g_signal_handlers_block_by_func (region_combo, region_changed_cb, self);
  g_signal_handlers_block_by_func (city_combo, city_changed_cb, self);

  update_timezone (self);

  g_signal_handlers_unblock_by_func (region_combo, region_changed_cb, self);
  g_signal_handlers_unblock_by_func (city_combo, city_changed_cb, self);

  queue_set_timezone (self);
}

static void
get_initial_timezone (CcDateTimePanel *self)
{
  const gchar *timezone;

  if (self->priv->dtm)
    timezone = timedate1_get_timezone (self->priv->dtm);
  else
    timezone = NULL;

  if (timezone == NULL ||
      !cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->priv->map), timezone))
    {
      g_warning ("Timezone '%s' is unhandled, setting %s as default", timezone ? timezone : "(null)", DEFAULT_TZ);
      cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->priv->map), DEFAULT_TZ);
    }
  self->priv->current_location = cc_timezone_map_get_location (CC_TIMEZONE_MAP (self->priv->map));
  update_timezone (self);
}

/* load region and city tree models */
struct get_region_data
{
  GtkListStore *region_store;
  GtkListStore *city_store;
  GHashTable *table;
};

/* Slash look-alikes that might be used in translations */
#define TRANSLATION_SPLIT                                                        \
        "\342\201\204"        /* FRACTION SLASH */                               \
        "\342\210\225"        /* DIVISION SLASH */                               \
        "\342\247\270"        /* BIG SOLIDUS */                                  \
        "\357\274\217"        /* FULLWIDTH SOLIDUS */                            \
        "/"

static void
get_regions (TzLocation             *loc,
             struct get_region_data *data)
{
  gchar *zone;
  gchar **split;
  gchar **split_translated;
  gchar *translated_city;

  zone = g_strdup (loc->zone);
  g_strdelimit (zone, "_", ' ');
  split = g_strsplit (zone, "/", 2);
  g_free (zone);

  /* Load the translation for it */
  zone = g_strdup (dgettext (GETTEXT_PACKAGE_TIMEZONES, loc->zone));
  g_strdelimit (zone, "_", ' ');
  split_translated = g_regex_split_simple ("[\\x{2044}\\x{2215}\\x{29f8}\\x{ff0f}/]", zone, 0, 0);
  g_free (zone);

  if (!g_hash_table_lookup_extended (data->table, split[0], NULL, NULL))
    {
      g_hash_table_insert (data->table, g_strdup (split[0]),
                           GINT_TO_POINTER (1));
      gtk_list_store_insert_with_values (data->region_store, NULL, 0,
                                         REGION_COL_REGION, split[0],
                                         REGION_COL_REGION_TRANSLATED, split_translated[0], -1);
    }

  /* g_regex_split_simple() splits too much for us, and would break
   * America/Argentina/Buenos_Aires into 3 strings, so rejoin the city part */
  translated_city = g_strjoinv ("/", split_translated + 1);

  gtk_list_store_insert_with_values (data->city_store, NULL, 0,
                                     CITY_COL_CITY, split[1],
                                     CITY_COL_CITY_TRANSLATED, translated_city,
                                     CITY_COL_REGION, split[0],
                                     CITY_COL_REGION_TRANSLATED, split_translated[0],
                                     CITY_COL_ZONE, loc->zone,
                                     -1);

  g_free (translated_city);
  g_strfreev (split);
  g_strfreev (split_translated);
}

static gboolean
city_model_filter_func (GtkTreeModel *model,
                        GtkTreeIter  *iter,
                        GtkComboBox  *combo)
{
  GtkTreeModel *combo_model;
  GtkTreeIter combo_iter;
  gchar *active_region = NULL;
  gchar *city_region = NULL;
  gboolean result;

  if (gtk_combo_box_get_active_iter (combo, &combo_iter) == FALSE)
    return FALSE;

  combo_model = gtk_combo_box_get_model (combo);
  gtk_tree_model_get (combo_model, &combo_iter,
                      CITY_COL_CITY, &active_region, -1);

  gtk_tree_model_get (model, iter,
                      CITY_COL_REGION, &city_region, -1);

  if (g_strcmp0 (active_region, city_region) == 0)
    result = TRUE;
  else
    result = FALSE;

  g_free (city_region);

  g_free (active_region);

  return result;
}


static void
load_regions_model (GtkListStore *regions, GtkListStore *cities)
{
  struct get_region_data data;
  TzDB *db;
  GHashTable *table;


  db = tz_load_db ();
  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  data.table = table;
  data.region_store = regions;
  data.city_store = cities;

  g_ptr_array_foreach (db->locations, (GFunc) get_regions, &data);

  g_hash_table_destroy (table);

  tz_db_free (db);

  /* sort the models */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (regions),
                                        REGION_COL_REGION_TRANSLATED,
                                        GTK_SORT_ASCENDING);
}

static void
update_widget_state_for_ntp (CcDateTimePanel *panel,
                             gboolean         using_ntp)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  gboolean allowed;

  /* need to check polkit before revealing to user */
  allowed = (! priv->permission || g_permission_get_allowed (priv->permission));

  gtk_widget_set_sensitive (W("datetime-button"), !using_ntp && allowed);
}

static void
day_changed (GtkWidget       *widget,
             CcDateTimePanel *panel)
{
  change_date (panel);
}

static void
month_year_changed (GtkWidget       *widget,
                    CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  guint mon, y;
  guint num_days;
  GtkAdjustment *adj;
  GtkSpinButton *day_spin;

  mon = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (W ("month-combobox")));
  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("year-spinbutton")));

  /* Check the number of days in that month */
  num_days = g_date_get_days_in_month (mon, y);

  day_spin = GTK_SPIN_BUTTON (W("day-spinbutton"));
  adj = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (day_spin));
  gtk_adjustment_set_upper (adj, num_days + 1);

  if (gtk_spin_button_get_value_as_int (day_spin) > num_days)
    gtk_spin_button_set_value (day_spin, num_days);

  change_date (panel);
}

static void
on_clock_changed (GnomeWallClock  *clock,
		  GParamSpec      *pspec,
		  CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;

  g_date_time_unref (priv->date);
  priv->date = g_date_time_new_now_local ();
  update_time (panel);
}

static void
change_time (CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  guint h, m;
  GDateTime *old_date;

  old_date = priv->date;

  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("h_spinbutton")));
  m = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("m_spinbutton")));

  if (priv->clock_format == G_DESKTOP_CLOCK_FORMAT_12H && priv->ampm_available)
    {
      gboolean is_pm_time;
      GtkWidget *am_pm_stack;
      GtkWidget *visible_child;

      am_pm_stack = W ("am_pm_stack");
      visible_child = gtk_stack_get_visible_child (GTK_STACK (am_pm_stack));
      if (visible_child == priv->pm_label)
        is_pm_time = TRUE;
      else
        is_pm_time = FALSE;

      if (h == 12)
        h = 0;
      if (is_pm_time)
        h += 12;
    }

  priv->date = g_date_time_new_local (g_date_time_get_year (old_date),
                                      g_date_time_get_month (old_date),
                                      g_date_time_get_day_of_month (old_date),
                                      h, m,
                                      g_date_time_get_second (old_date));
  g_date_time_unref (old_date);

  update_time (panel);
  queue_set_datetime (panel);
}

static void
change_ntp (GObject         *gobject,
            GParamSpec      *pspec,
            CcDateTimePanel *self)
{
  update_widget_state_for_ntp (self, gtk_switch_get_active (GTK_SWITCH (gobject)));
  queue_set_ntp (self);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       gpointer     data)
{
  CcDateTimePanelPrivate *priv = CC_DATE_TIME_PANEL (data)->priv;
  gboolean allowed, using_ntp;

  allowed = g_permission_get_allowed (permission);
  using_ntp = gtk_switch_get_active (GTK_SWITCH (W("network_time_switch")));

  /* All the widgets but the lock button and the 24h setting */
  gtk_widget_set_sensitive (W("auto-datetime-row"), allowed);
  gtk_widget_set_sensitive (W("timezone-button"), allowed);
  update_widget_state_for_ntp (data, using_ntp);

  /* Hide the subdialogs if we no longer have permissions */
  if (!allowed)
    {
      gtk_widget_hide (GTK_WIDGET (W ("datetime-dialog")));
      gtk_widget_hide (GTK_WIDGET (W ("timezone-dialog")));
    }
}

static void
update_ntp_switch_from_system (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  gboolean using_ntp;
  GtkWidget *switch_widget;

  using_ntp = timedate1_get_ntp (self->priv->dtm);

  switch_widget = W("network_time_switch");
  g_signal_handlers_block_by_func (switch_widget, change_ntp, self);
  gtk_switch_set_active (GTK_SWITCH (switch_widget), using_ntp);
  update_widget_state_for_ntp (self, using_ntp);
  g_signal_handlers_unblock_by_func (switch_widget, change_ntp, self);
}

static void
on_can_ntp_changed (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *switch_widget;
  gboolean sensitive = TRUE;
  GVariant *value;

  switch_widget = W("network_time_switch");

  /* We need to access this directly so that we can default to TRUE if
   * it is not set.
   */
  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (self->priv->dtm), "CanNTP");
  if (value)
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        sensitive = g_variant_get_boolean (value);
      g_variant_unref (value);
    }

  gtk_widget_set_sensitive (switch_widget, sensitive);
}

static void
on_ntp_changed (CcDateTimePanel *self)
{
  update_ntp_switch_from_system (self);
}

static void
on_timezone_changed (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *region_combo, *city_combo;

  region_combo = W("region_combobox");
  city_combo = W("city_combobox");

  g_signal_handlers_block_by_func (region_combo, region_changed_cb, self);
  g_signal_handlers_block_by_func (city_combo, city_changed_cb, self);
  g_signal_handlers_block_by_func (self->priv->map, location_changed_cb, self);

  get_initial_timezone (self);

  g_signal_handlers_unblock_by_func (region_combo, region_changed_cb, self);
  g_signal_handlers_unblock_by_func (city_combo, city_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->priv->map, location_changed_cb, self);
}

static void
on_timedated_properties_changed (GDBusProxy       *proxy,
                                 GVariant         *changed_properties,
                                 const gchar     **invalidated_properties,
                                 CcDateTimePanel  *self)
{
  GError *error;
  GVariant *variant;
  GVariant *v;
  guint i;

  if (invalidated_properties != NULL)
    for (i = 0; invalidated_properties[i] != NULL; i++) {
        error = NULL;
        /* See https://bugs.freedesktop.org/show_bug.cgi?id=37632 for the reason why we're doing this */
        variant = g_dbus_proxy_call_sync (proxy,
                                          "org.freedesktop.DBus.Properties.Get",
                                          g_variant_new ("(ss)", "org.freedesktop.timedate1", invalidated_properties[i]),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
        if (variant == NULL) {
                g_warning ("Failed to get property '%s': %s", invalidated_properties[i], error->message);
                g_error_free (error);
        } else {
                g_variant_get (variant, "(v)", &v);
                g_dbus_proxy_set_cached_property (proxy, invalidated_properties[i], v);
                g_variant_unref (variant);
        }
    }
}

static void
update_header (GtkListBoxRow *row,
               GtkListBoxRow *before,
               gpointer       user_data)
{
  GtkWidget *current;

  if (before == NULL)
    return;

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}

static void
run_dialog (CcDateTimePanel *self,
            const gchar     *dialog_name)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *dialog, *parent;

  dialog = W (dialog_name);

  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

static void
toggle_switch (GtkWidget *sw)
{
  gboolean active;

  active = gtk_switch_get_active (GTK_SWITCH (sw));
  gtk_switch_set_active (GTK_SWITCH (sw), !active);
}

static void
list_box_row_activated (GtkListBox      *listbox,
                        GtkListBoxRow   *row,
                        CcDateTimePanel *self)

{
  CcDateTimePanelPrivate *priv = self->priv;
  gchar *widget_name, *found;

  widget_name = g_strdup (gtk_buildable_get_name (GTK_BUILDABLE (row)));

  if (!widget_name)
    return;

  gtk_list_box_select_row (listbox, NULL);

  if (!g_strcmp0 (widget_name, "auto-datetime-row"))
    {
      toggle_switch (W ("network_time_switch"));
    }
  else if ((found = g_strrstr (widget_name, "button")))
    {
      /* replace "button" with "dialog" */
      memcpy (found, "dialog", 6);

      run_dialog (self, widget_name);
    }

  g_free (widget_name);
}

static void
setup_main_listview (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *listbox;

  listbox = W ("listbox");
  gtk_list_box_set_header_func (GTK_LIST_BOX (listbox), update_header, NULL, NULL);
  g_signal_connect (listbox, "row-activated",
                    G_CALLBACK (list_box_row_activated), self);
}

static gboolean
format_minutes_combobox (GtkSpinButton *spin,
                         gpointer       data)
{
  GtkAdjustment *adjustment;
  char *text;
  int value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = (int)gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%02d", value);
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  g_free (text);

  return TRUE;
}

static gboolean
format_hours_combobox (GtkSpinButton   *spin,
                       CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  GtkAdjustment *adjustment;
  char *text;
  int hour;
  gboolean use_ampm;

  if (priv->clock_format == G_DESKTOP_CLOCK_FORMAT_12H && priv->ampm_available)
    use_ampm = TRUE;
  else
    use_ampm = FALSE;

  adjustment = gtk_spin_button_get_adjustment (spin);
  hour = (int)gtk_adjustment_get_value (adjustment);
  if (use_ampm)
    text = g_strdup_printf ("%d", hour);
  else
    text = g_strdup_printf ("%02d", hour);
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  g_free (text);

  return TRUE;
}

static void
setup_timezone_dialog (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *button;
  GtkWidget *dialog;

  /* set up timezone map */
  priv->map = (GtkWidget *) cc_timezone_map_new ();
  gtk_widget_show (priv->map);
  gtk_container_add (GTK_CONTAINER (gtk_builder_get_object (priv->builder, "aspectmap")),
                     priv->map);

  button = W ("timezone-close-button");
  dialog = W ("timezone-dialog");
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);
}

static char *
format_am_label ()
{
  GDateTime *date;
  char *text;

  /* Construct a time at midnight, and use it to get localized AM identifier */
  date = g_date_time_new_utc (1, 1, 1, 0, 0, 0);
  text = g_date_time_format (date, "%p");
  g_date_time_unref (date);

  return text;
}

static char *
format_pm_label ()
{
  GDateTime *date;
  char *text;

  /* Construct a time at noon, and use it to get localized PM identifier */
  date = g_date_time_new_utc (1, 1, 1, 12, 0, 0);
  text = g_date_time_format (date, "%p");
  g_date_time_unref (date);

  return text;
}

static void
setup_am_pm_button (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  GtkWidget *am_pm_button;
  GtkWidget *stack;
  char *text;

  text = format_am_label ();
  priv->am_label = gtk_label_new (text);
  g_free (text);

  text = format_pm_label ();
  priv->pm_label = gtk_label_new (text);
  g_free (text);

  stack = W ("am_pm_stack");
  gtk_container_add (GTK_CONTAINER (stack), priv->am_label);
  gtk_container_add (GTK_CONTAINER (stack), priv->pm_label);
  gtk_widget_show_all (stack);

  am_pm_button = W ("am_pm_button");
  g_signal_connect (am_pm_button, "clicked",
                    G_CALLBACK (am_pm_button_clicked), self);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   ".gnome-control-center-ampm-toggle-button {\n"
                                   "    font-size: 18px;\n"
                                   "}", -1, NULL);
  context = gtk_widget_get_style_context (am_pm_button);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

static void
setup_datetime_dialog (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkAdjustment *adjustment;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  GtkWidget *button;
  GtkWidget *dialog;
  guint num_days;

  setup_am_pm_button (self);

  /* Big time buttons */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   ".gnome-control-center-datetime-setup-time {\n"
                                   "    font-size: 32px;\n"
                                   "}", -1, NULL);
  context = gtk_widget_get_style_context (GTK_WIDGET (W ("time_grid")));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

  button = W ("datetime-close-button");
  dialog = W ("datetime-dialog");
  g_signal_connect_swapped (button, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  /* Force the direction for the time, so that the time
   * is presented correctly for RTL languages */
  gtk_widget_set_direction (W ("time_grid"), GTK_TEXT_DIR_LTR);

  /* Month */
  gtk_combo_box_set_active (GTK_COMBO_BOX (W ("month-combobox")),
                            g_date_time_get_month (priv->date) - 1);
  g_signal_connect (G_OBJECT (W("month-combobox")), "changed",
                    G_CALLBACK (month_year_changed), self);

  /* Day */
  num_days = g_date_get_days_in_month (g_date_time_get_month (priv->date),
                                       g_date_time_get_year (priv->date));
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_day_of_month (priv->date), 1,
                                                    num_days + 1, 1, 10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (W ("day-spinbutton")),
                                  adjustment);
  g_signal_connect (G_OBJECT (W ("day-spinbutton")), "value-changed",
                    G_CALLBACK (day_changed), self);

  /* Year */
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_year (priv->date),
                                                    G_MINDOUBLE, G_MAXDOUBLE, 1,
                                                    10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (W ("year-spinbutton")),
                                  adjustment);
  g_signal_connect (G_OBJECT (W ("year-spinbutton")), "value-changed",
                    G_CALLBACK (month_year_changed), self);

  /* Hours and minutes */
  g_signal_connect (W ("h_spinbutton"), "output",
                    G_CALLBACK (format_hours_combobox), self);
  g_signal_connect (W ("m_spinbutton"), "output",
                    G_CALLBACK (format_minutes_combobox), self);

  gtk_spin_button_set_increments (GTK_SPIN_BUTTON (W ("h_spinbutton")), 1, 0);
  gtk_spin_button_set_increments (GTK_SPIN_BUTTON (W ("m_spinbutton")), 1, 0);

  gtk_spin_button_set_range (GTK_SPIN_BUTTON (W ("h_spinbutton")), 0, 23);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (W ("m_spinbutton")), 0, 59);

  g_signal_connect_swapped (W ("h_spinbutton"), "value-changed",
                            G_CALLBACK (change_time), self);
  g_signal_connect_swapped (W ("m_spinbutton"), "value-changed",
                            G_CALLBACK (change_time), self);
}

static void
cc_date_time_panel_init (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv;
  GtkWidget *widget;
  GError *error;
  GtkTreeModelFilter *city_modelfilter;
  GtkTreeModelSort *city_modelsort;
  const char *ampm;
  int ret;

  priv = self->priv = DATE_TIME_PANEL_PRIVATE (self);
  g_resources_register (cc_datetime_get_resource ());

  priv->cancellable = g_cancellable_new ();
  error = NULL;
  priv->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.freedesktop.timedate1",
                                                "/org/freedesktop/timedate1",
                                                priv->cancellable,
                                                &error);
  if (priv->dtm == NULL) {
        g_warning ("could not get proxy for DateTimeMechanism: %s", error->message);
        g_clear_error (&error);
  }

  priv->builder = gtk_builder_new ();
  ret = gtk_builder_add_from_resource (priv->builder,
                                       "/org/gnome/control-center/datetime/datetime.ui",
                                       &error);

  if (ret == 0)
    {
      g_warning ("Could not load ui: %s", error ? error->message : "No reason");
      if (error)
        g_error_free (error);
      return;
    }

  priv->date = g_date_time_new_now_local ();

  setup_timezone_dialog (self);
  setup_datetime_dialog (self);
  setup_main_listview (self);

  /* set up network time button */
  if (priv->dtm != NULL)
    {
      update_ntp_switch_from_system (self);
      on_can_ntp_changed (self);
    }
  g_signal_connect (W("network_time_switch"), "notify::active",
                    G_CALLBACK (change_ntp), self);

  /* Clock settings */
  priv->settings = g_settings_new (CLOCK_SCHEMA);

  ampm = nl_langinfo (AM_STR);
  /* There are no AM/PM indicators for this locale, so
   * offer the 24 hr clock as the only option */
  if (ampm == NULL || ampm[0] == '\0')
    {
      gtk_widget_set_visible (W("timeformat-frame"), FALSE);
      priv->ampm_available = FALSE;
    }
  else
    {
      priv->ampm_available = TRUE;
    }

  widget = W ("vbox_datetime");
  gtk_widget_reparent (widget, GTK_WIDGET (self));

  /* setup the time itself */
  priv->clock_tracker = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
  g_signal_connect (priv->clock_tracker, "notify::clock", G_CALLBACK (on_clock_changed), self);

  clock_settings_changed_cb (priv->settings, CLOCK_FORMAT_KEY, self);
  g_signal_connect (priv->settings, "changed::" CLOCK_FORMAT_KEY,
                    G_CALLBACK (clock_settings_changed_cb), self);

  g_signal_connect (W("format_combobox"), "notify::active-id",
                    G_CALLBACK (change_clock_settings), self);

  update_time (self);

  priv->locations = (GtkTreeModel*) gtk_builder_get_object (priv->builder,
                                                            "region-liststore");

  load_regions_model (GTK_LIST_STORE (priv->locations),
                      GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                              "city-liststore")));

  city_modelfilter = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (priv->builder, "city-modelfilter"));

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "region_combobox");
  city_modelsort = GTK_TREE_MODEL_SORT (gtk_builder_get_object (priv->builder, "city-modelsort"));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (city_modelsort), CITY_COL_CITY_TRANSLATED,
                                        GTK_SORT_ASCENDING);

  gtk_tree_model_filter_set_visible_func (city_modelfilter,
                                          (GtkTreeModelFilterVisibleFunc) city_model_filter_func,
                                          widget,
                                          NULL);

  /* After the initial setup, so we can be sure that
   * the model is filled up */
  get_initial_timezone (self);

  widget = (GtkWidget*) gtk_builder_get_object (self->priv->builder,
                                                "region_combobox");
  g_signal_connect (widget, "changed", G_CALLBACK (region_changed_cb), self);

  widget = (GtkWidget*) gtk_builder_get_object (self->priv->builder,
                                                "city_combobox");
  g_signal_connect (widget, "changed", G_CALLBACK (city_changed_cb), self);

  g_signal_connect (self->priv->map, "location-changed",
                    G_CALLBACK (location_changed_cb), self);

  /* Watch changes of timedated remote service properties */
  if (priv->dtm)
    {
      g_signal_connect (priv->dtm, "g-properties-changed",
                        G_CALLBACK (on_timedated_properties_changed), self);
      g_signal_connect_swapped (priv->dtm, "notify::ntp",
                                G_CALLBACK (on_ntp_changed), self);
      g_signal_connect_swapped (priv->dtm, "notify::can-ntp",
                                G_CALLBACK (on_can_ntp_changed), self);
      g_signal_connect_swapped (priv->dtm, "notify::timezone",
                                G_CALLBACK (on_timezone_changed), self);
    }
  /* We ignore UTC <--> LocalRTC changes at the moment */

  /* add the lock button */
  priv->permission = polkit_permission_new_sync ("org.gnome.controlcenter.datetime.configure", NULL, NULL, NULL);
  if (priv->permission == NULL)
    {
      g_warning ("Your system does not have the '%s' PolicyKit files installed. Please check your installation",
                 "org.gnome.controlcenter.datetime.configure");
      return;
    }

  g_signal_connect (priv->permission, "notify",
                    G_CALLBACK (on_permission_changed), self);
  on_permission_changed (priv->permission, NULL, self);
}
