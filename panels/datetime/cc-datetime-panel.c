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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "config.h"
#include "cc-datetime-panel.h"
#include "cc-datetime-resources.h"

#include <langinfo.h>
#include <sys/time.h>
#include "list-box-helper.h"
#include "cc-timezone-map.h"
#include "timedated.h"
#include "date-endian.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <gdesktop-enums.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#include <glib/gi18n.h>
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-wall-clock.h>
#include <polkit/polkit.h>

/* FIXME: This should be "Etc/GMT" instead */
#define DEFAULT_TZ "Europe/London"
#define GETTEXT_PACKAGE_TIMEZONES GETTEXT_PACKAGE "-timezones"

enum {
  CITY_COL_CITY_HUMAN_READABLE,
  CITY_COL_ZONE,
  CITY_NUM_COLS
};

#define DATETIME_PERMISSION "org.gnome.controlcenter.datetime.configure"
#define DATETIME_TZ_PERMISSION "org.freedesktop.timedate1.set-timezone"
#define LOCATION_SETTINGS "org.gnome.system.location"
#define LOCATION_ENABLED "enabled"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

#define FILECHOOSER_SCHEMA "org.gtk.Settings.FileChooser"

#define DATETIME_SCHEMA "org.gnome.desktop.datetime"
#define AUTO_TIMEZONE_KEY "automatic-timezone"

struct _CcDateTimePanel
{
  CcPanel parent_instance;

  GtkBuilder *builder;
  GtkWidget *map;

  GList *listboxes;
  GList *listboxes_reverse;
  GList *toplevels;

  TzLocation *current_location;

  GtkTreeModelFilter *city_filter;

  GDateTime *date;

  GSettings *clock_settings;
  GSettings *datetime_settings;
  GSettings *filechooser_settings;
  GDesktopClockFormat clock_format;
  GtkWidget *am_label;
  GtkWidget *am_pm_button;
  GtkWidget *am_pm_stack;
  GtkWidget *aspectmap;
  GtkWidget *auto_datetime_row;
  GtkWidget *auto_timezone_row;
  GtkWidget *auto_timezone_switch;
  GtkListStore *city_liststore;
  GtkTreeModelSort *city_modelsort;
  GtkWidget *date_grid;
  GtkWidget *datetime_button;
  GtkWidget *datetime_dialog;
  GtkWidget *datetime_label;
  GtkWidget *day_spinbutton;
  GtkWidget *format_combobox;
  GtkWidget *h_spinbutton;
  GtkWidget *listbox1;
  GtkWidget *listbox2;
  GtkLockButton *lock_button;
  GtkWidget *m_spinbutton;
  GtkWidget *month_combobox;
  GtkListStore *month_liststore;
  GtkWidget *network_time_switch;
  GtkWidget *pm_label;
  GtkWidget *time_box;
  GtkWidget *time_grid;
  GtkWidget *timezone_button;
  GtkWidget *timezone_dialog;
  GtkWidget *timezone_label;
  GtkWidget *timezone_searchentry;
  GtkWidget *year_spinbutton;

  GnomeWallClock *clock_tracker;

  Timedate1 *dtm;
  GCancellable *cancellable;

  GPermission *permission;
  GPermission *tz_permission;
  GSettings *location_settings;
};

CC_PANEL_REGISTER (CcDateTimePanel, cc_date_time_panel)

static void update_time (CcDateTimePanel *self);
static void change_time (CcDateTimePanel *self);


static void
cc_date_time_panel_dispose (GObject *object)
{
  CcDateTimePanel *panel = CC_DATE_TIME_PANEL (object);

  if (panel->cancellable)
    {
      g_cancellable_cancel (panel->cancellable);
      g_clear_object (&panel->cancellable);
    }

  if (panel->toplevels)
    {
      g_list_free_full (panel->toplevels, (GDestroyNotify) gtk_widget_destroy);
      panel->toplevels = NULL;
    }

  g_clear_object (&panel->builder);
  g_clear_object (&panel->clock_tracker);
  g_clear_object (&panel->dtm);
  g_clear_object (&panel->permission);
  g_clear_object (&panel->tz_permission);
  g_clear_object (&panel->location_settings);
  g_clear_object (&panel->clock_settings);
  g_clear_object (&panel->datetime_settings);
  g_clear_object (&panel->filechooser_settings);

  g_clear_pointer (&panel->date, g_date_time_unref);

  g_clear_pointer (&panel->listboxes, g_list_free);
  g_clear_pointer (&panel->listboxes_reverse, g_list_free);

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->dispose (object);
}

static void
cc_date_time_panel_constructed (GObject *object)
{
  CcDateTimePanel *self = CC_DATE_TIME_PANEL (object);

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->constructed (object);

  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (self)),
                                   GTK_WIDGET (self->lock_button),
                                   GTK_POS_RIGHT);
}

static const char *
cc_date_time_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/shell-overview#time";
}

static void clock_settings_changed_cb (CcDateTimePanel *panel,
                                       gchar           *key);

static void
change_clock_settings (GObject         *gobject,
                       GParamSpec      *pspec,
                       CcDateTimePanel *self)
{
  GDesktopClockFormat value;
  const char *active_id;

  g_signal_handlers_block_by_func (self->clock_settings, clock_settings_changed_cb,
                                   self);

  active_id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (self->format_combobox));
  if (!g_strcmp0 (active_id, "24h"))
    value = G_DESKTOP_CLOCK_FORMAT_24H;
  else
    value = G_DESKTOP_CLOCK_FORMAT_12H;

  g_settings_set_enum (self->clock_settings, CLOCK_FORMAT_KEY, value);
  g_settings_set_enum (self->filechooser_settings, CLOCK_FORMAT_KEY, value);
  self->clock_format = value;

  update_time (self);

  g_signal_handlers_unblock_by_func (self->clock_settings, clock_settings_changed_cb,
                                     self);
}

static void
clock_settings_changed_cb (CcDateTimePanel *self,
                           gchar           *key)
{
  GDesktopClockFormat value;

  value = g_settings_get_enum (self->clock_settings, CLOCK_FORMAT_KEY);
  self->clock_format = value;

  g_signal_handlers_block_by_func (self->format_combobox, change_clock_settings, self);

  if (value == G_DESKTOP_CLOCK_FORMAT_24H)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (self->format_combobox), "24h");
  else
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (self->format_combobox), "12h");

  update_time (self);

  g_signal_handlers_unblock_by_func (self->format_combobox, change_clock_settings, self);
}

static void
am_pm_stack_visible_child_changed_cb (CcDateTimePanel *self)
{
  AtkObject *am_pm_button_accessible;
  GtkWidget *visible_label;
  const gchar *visible_text;

  am_pm_button_accessible = gtk_widget_get_accessible (self->am_pm_button);
  if (am_pm_button_accessible == NULL)
    return;

  visible_label = gtk_stack_get_visible_child (GTK_STACK (self->am_pm_stack));
  visible_text = gtk_label_get_text (GTK_LABEL (visible_label));
  atk_object_set_name (am_pm_button_accessible, visible_text);
}

static gboolean
am_pm_button_clicked (GtkWidget *button,
                      CcDateTimePanel *self)
{
  GtkWidget *visible_child;

  visible_child = gtk_stack_get_visible_child (GTK_STACK (self->am_pm_stack));
  if (visible_child == self->am_label)
    gtk_stack_set_visible_child (GTK_STACK (self->am_pm_stack), self->pm_label);
  else
    gtk_stack_set_visible_child (GTK_STACK (self->am_pm_stack), self->am_label);

  change_time (self);

  return TRUE;
}

/* Update the widgets based on the system time */
static void
update_time (CcDateTimePanel *self)
{
  g_autofree gchar *label = NULL;
  gint hour;
  gint minute;
  gboolean use_ampm;

  g_signal_handlers_block_by_func (self->h_spinbutton, change_time, self);
  g_signal_handlers_block_by_func (self->m_spinbutton, change_time, self);
  g_signal_handlers_block_by_func (self->am_pm_button, am_pm_button_clicked, self);

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    use_ampm = TRUE;
  else
    use_ampm = FALSE;

  hour = g_date_time_get_hour (self->date);
  minute = g_date_time_get_minute (self->date);

  if (!use_ampm)
    {
      /* Update the hours spinbutton */
      gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->h_spinbutton), 0, 23);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->h_spinbutton), hour);
    }
  else
    {
      gboolean is_pm_time;

      is_pm_time = (hour >= 12);

      /* Update the AM/PM button */
      if (is_pm_time)
        gtk_stack_set_visible_child (GTK_STACK (self->am_pm_stack), self->pm_label);
      else
        gtk_stack_set_visible_child (GTK_STACK (self->am_pm_stack), self->am_label);

      /* Update the hours spinbutton */
      if (is_pm_time)
        hour -= 12;
      if (hour == 0)
        hour = 12;
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->h_spinbutton), hour);
      gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->h_spinbutton), 1, 12);
    }

  gtk_widget_set_visible (self->am_pm_button, use_ampm);

  /* Update the minutes spinbutton */
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->m_spinbutton), minute);

  g_signal_handlers_unblock_by_func (self->h_spinbutton, change_time, self);
  g_signal_handlers_unblock_by_func (self->m_spinbutton, change_time, self);
  g_signal_handlers_unblock_by_func (self->am_pm_button, am_pm_button_clicked, self);

  /* Update the time on the listbow row */
  if (use_ampm)
    {
      /* Translators: This is the full date and time format used in 12-hour mode. */
      label = g_date_time_format (self->date, _("%e %B %Y, %l:%M %p"));
    }
  else
    {
      /* Translators: This is the full date and time format used in 24-hour mode. */
      label = g_date_time_format (self->date, _("%e %B %Y, %R"));
    }

  gtk_label_set_text (GTK_LABEL (self->datetime_label), label);
}

static void
set_time_cb (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  CcDateTimePanel *self = user_data;
  g_autoptr(GError) error = NULL;

  if (!timedate1_call_set_time_finish (self->dtm,
                                       res,
                                       &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not set system time: %s", error->message);
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
  g_autoptr(GError) error = NULL;

  if (!timedate1_call_set_timezone_finish (self->dtm,
                                           res,
                                           &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not set system timezone: %s", error->message);
    }
}

static void
set_using_ntp_cb (GObject      *source,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  CcDateTimePanel *self = user_data;
  g_autoptr(GError) error = NULL;

  if (!timedate1_call_set_ntp_finish (self->dtm,
                                      res,
                                      &error))
    {
      /* TODO: display any error in a user friendly way */
      g_warning ("Could not set system to use NTP: %s", error->message);
    }
}

static void
queue_set_datetime (CcDateTimePanel *self)
{
  gint64 unixtime;

  /* timedated expects number of microseconds since 1 Jan 1970 UTC */
  unixtime = g_date_time_to_unix (self->date);

  timedate1_call_set_time (self->dtm,
                           unixtime * 1000000,
                           FALSE,
                           TRUE,
                           self->cancellable,
                           set_time_cb,
                           self);
}

static void
queue_set_ntp (CcDateTimePanel *self)
{
  gboolean using_ntp;
  /* for now just do it */
  using_ntp = gtk_switch_get_active (GTK_SWITCH (self->network_time_switch));

  timedate1_call_set_ntp (self->dtm,
                          using_ntp,
                          TRUE,
                          self->cancellable,
                          set_using_ntp_cb,
                          self);
}

static void
queue_set_timezone (CcDateTimePanel *self)
{
  /* for now just do it */
  if (self->current_location)
    {
      timedate1_call_set_timezone (self->dtm,
                                   self->current_location->zone,
                                   TRUE,
                                   self->cancellable,
                                   set_timezone_cb,
                                   self);
    }
}

static void
change_date (CcDateTimePanel *self)
{
  guint mon, y, d;
  g_autoptr(GDateTime) old_date = NULL;

  mon = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (self->month_combobox));
  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->year_spinbutton));
  d = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->day_spinbutton));

  old_date = self->date;
  self->date = g_date_time_new_local (y, mon, d,
                                      g_date_time_get_hour (old_date),
                                      g_date_time_get_minute (old_date),
                                      g_date_time_get_second (old_date));
  queue_set_datetime (self);
}

static gboolean
city_changed_cb (CcDateTimePanel    *self,
                 GtkTreeModel       *model,
                 GtkTreeIter        *iter,
                 GtkEntryCompletion *completion)
{
  GtkWidget *entry;
  g_autofree gchar *zone = NULL;

  gtk_tree_model_get (model, iter,
                      CITY_COL_ZONE, &zone, -1);
  cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->map), zone);

  entry = gtk_entry_completion_get_entry (completion);
  gtk_entry_set_text (GTK_ENTRY (entry), "");

  return TRUE;
}

static char *
translated_city_name (TzLocation *loc)
{
  g_autofree gchar *zone_translated = NULL;
  g_auto(GStrv) split_translated = NULL;
  g_autofree gchar *country = NULL;
  gchar *name;
  gint length;

  /* Load the translation for it */
  zone_translated = g_strdup (dgettext (GETTEXT_PACKAGE_TIMEZONES, loc->zone));
  g_strdelimit (zone_translated, "_", ' ');
  split_translated = g_regex_split_simple ("[\\x{2044}\\x{2215}\\x{29f8}\\x{ff0f}/]",
                                           zone_translated,
                                           0, 0);

  length = g_strv_length (split_translated);

  country = gnome_get_country_from_code (loc->country, NULL);
  /* Translators: "city, country" */
  name = g_strdup_printf (C_("timezone loc", "%s, %s"),
                          split_translated[length-1],
                          country);

  return name;
}

static void
update_timezone (CcDateTimePanel *self)
{
  g_autofree gchar *bubble_text = NULL;
  g_autofree gchar *city_country = NULL;
  g_autofree gchar *label = NULL;
  g_autofree gchar *time_label = NULL;
  g_autofree gchar *utc_label = NULL;
  g_autofree gchar *tz_desc = NULL;
  gboolean use_ampm;

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    use_ampm = TRUE;
  else
    use_ampm = FALSE;

  city_country = translated_city_name (self->current_location);

  /* Update the timezone on the listbow row */
  /* Translators: "timezone (details)" */
  label = g_strdup_printf (C_("timezone desc", "%s (%s)"),
                           g_date_time_get_timezone_abbreviation (self->date),
                           city_country);
  gtk_label_set_text (GTK_LABEL (self->timezone_label), label);

  /* Translators: UTC here means the Coordinated Universal Time.
   * %:::z will be replaced by the offset from UTC e.g. UTC+02 */
  utc_label = g_date_time_format (self->date, _("UTC%:::z"));

  if (use_ampm)
    {
      /* Translators: This is the time format used in 12-hour mode. */
      time_label = g_date_time_format (self->date, _("%l:%M %p"));
    }
  else
    {
      /* Translators: This is the time format used in 24-hour mode. */
      time_label = g_date_time_format (self->date, _("%R"));
    }

  /* Update the text bubble in the timezone map */
  /* Translators: "timezone (utc shift)" */
  tz_desc = g_strdup_printf (C_("timezone map", "%s (%s)"),
                             g_date_time_get_timezone_abbreviation (self->date),
                             utc_label);
  bubble_text = g_strdup_printf ("<b>%s</b>\n"
                                 "<small>%s</small>\n"
                                 "<b>%s</b>",
                                 tz_desc,
                                 city_country,
                                 time_label);
  cc_timezone_map_set_bubble_text (CC_TIMEZONE_MAP (self->map), bubble_text);
}

static void
location_changed_cb (CcDateTimePanel *self,
                     TzLocation      *location)
{
  g_autoptr(GDateTime) old_date = NULL;
  g_autoptr(GTimeZone) timezone = NULL;

  g_debug ("location changed to %s/%s", location->country, location->zone);

  self->current_location = location;

  timezone = g_time_zone_new (location->zone);
  old_date = self->date;
  self->date = g_date_time_to_timezone (old_date, timezone);

  update_timezone (self);
  queue_set_timezone (self);
}

static void
get_initial_timezone (CcDateTimePanel *self)
{
  const gchar *timezone;

  timezone = timedate1_get_timezone (self->dtm);

  if (timezone == NULL ||
      !cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->map), timezone))
    {
      g_warning ("Timezone '%s' is unhandled, setting %s as default", timezone ? timezone : "(null)", DEFAULT_TZ);
      cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->map), DEFAULT_TZ);
    }
  self->current_location = cc_timezone_map_get_location (CC_TIMEZONE_MAP (self->map));
  update_timezone (self);
}

static void
load_cities (TzLocation   *loc,
             GtkListStore *city_store)
{
  g_autofree gchar *human_readable = NULL;

  human_readable = translated_city_name (loc);
  gtk_list_store_insert_with_values (city_store, NULL, 0,
                                     CITY_COL_CITY_HUMAN_READABLE, human_readable,
                                     CITY_COL_ZONE, loc->zone,
                                     -1);
}

static void
load_regions_model (GtkListStore *cities)
{
  g_autoptr(TzDB) db = NULL;

  db = tz_load_db ();
  g_ptr_array_foreach (db->locations, (GFunc) load_cities, cities);
}

static void
day_changed (CcDateTimePanel *panel)
{
  change_date (panel);
}

static void
month_year_changed (CcDateTimePanel *self)
{
  guint mon, y;
  guint num_days;
  GtkAdjustment *adj;
  GtkSpinButton *day_spin;

  mon = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (self->month_combobox));
  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->year_spinbutton));

  /* Check the number of days in that month */
  num_days = g_date_get_days_in_month (mon, y);

  day_spin = GTK_SPIN_BUTTON (self->day_spinbutton);
  adj = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (day_spin));
  gtk_adjustment_set_upper (adj, num_days + 1);

  if (gtk_spin_button_get_value_as_int (day_spin) > num_days)
    gtk_spin_button_set_value (day_spin, num_days);

  change_date (self);
}

static void
on_clock_changed (CcDateTimePanel *panel,
		  GParamSpec      *pspec)
{
  g_date_time_unref (panel->date);
  panel->date = g_date_time_new_now_local ();
  update_time (panel);
  update_timezone (panel);
}

static void
change_time (CcDateTimePanel *self)
{
  guint h, m;
  g_autoptr(GDateTime) old_date = NULL;

  h = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->h_spinbutton));
  m = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (self->m_spinbutton));

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    {
      gboolean is_pm_time;
      GtkWidget *visible_child;

      visible_child = gtk_stack_get_visible_child (GTK_STACK (self->am_pm_stack));
      if (visible_child == self->pm_label)
        is_pm_time = TRUE;
      else
        is_pm_time = FALSE;

      if (h == 12)
        h = 0;
      if (is_pm_time)
        h += 12;
    }

  old_date = self->date;
  self->date = g_date_time_new_local (g_date_time_get_year (old_date),
                                      g_date_time_get_month (old_date),
                                      g_date_time_get_day_of_month (old_date),
                                      h, m,
                                      g_date_time_get_second (old_date));

  update_time (self);
  queue_set_datetime (self);
}

static void
change_ntp (CcDateTimePanel *self,
            GParamSpec      *pspec)
{
  queue_set_ntp (self);
}

static gboolean
is_ntp_available (CcDateTimePanel *self)
{
  g_autoptr(GVariant) value = NULL;
  gboolean ntp_available = TRUE;

  /* We need to access this directly so that we can default to TRUE if
   * it is not set.
   */
  value = g_dbus_proxy_get_cached_property (G_DBUS_PROXY (self->dtm), "CanNTP");
  if (value)
    {
      if (g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        ntp_available = g_variant_get_boolean (value);
    }

  return ntp_available;
}

static void
on_permission_changed (CcDateTimePanel *self)
{
  gboolean allowed, location_allowed, tz_allowed, auto_timezone, using_ntp;

  allowed = (self->permission != NULL && g_permission_get_allowed (self->permission));
  location_allowed = g_settings_get_boolean (self->location_settings, LOCATION_ENABLED);
  tz_allowed = (self->tz_permission != NULL && g_permission_get_allowed (self->tz_permission));
  using_ntp = gtk_switch_get_active (GTK_SWITCH (self->network_time_switch));
  auto_timezone = gtk_switch_get_active (GTK_SWITCH (self->auto_timezone_switch));

  /* All the widgets but the lock button and the 24h setting */
  gtk_widget_set_sensitive (self->auto_datetime_row, allowed);
  gtk_widget_set_sensitive (self->auto_timezone_row, location_allowed && (allowed || tz_allowed));
  gtk_widget_set_sensitive (self->datetime_button, allowed && !using_ntp);
  gtk_widget_set_sensitive (self->timezone_button, (allowed || tz_allowed) && (!auto_timezone || !location_allowed));

  /* Hide the subdialogs if we no longer have permissions */
  if (!allowed)
      gtk_widget_hide (GTK_WIDGET (self->datetime_dialog));
  if (!allowed && !tz_allowed)
      gtk_widget_hide (GTK_WIDGET (self->timezone_dialog));
}

static void
on_location_settings_changed (CcDateTimePanel *panel)
{
  on_permission_changed (panel);
}

static void
on_can_ntp_changed (CcDateTimePanel *self)
{
  gtk_widget_set_visible (self->auto_datetime_row, is_ntp_available (self));
}

static void
on_timezone_changed (CcDateTimePanel *self)
{
  g_signal_handlers_block_by_func (self->map, location_changed_cb, self);
  get_initial_timezone (self);
  g_signal_handlers_unblock_by_func (self->map, location_changed_cb, self);
}

static void
on_timedated_properties_changed (CcDateTimePanel  *self,
                                 GVariant         *changed_properties,
                                 const gchar     **invalidated_properties)
{
  guint i;

  if (invalidated_properties != NULL)
    for (i = 0; invalidated_properties[i] != NULL; i++) {
        g_autoptr(GVariant) variant = NULL;
        g_autoptr(GError) error = NULL;

        /* See https://bugs.freedesktop.org/show_bug.cgi?id=37632 for the reason why we're doing this */
        variant = g_dbus_proxy_call_sync (G_DBUS_PROXY (self->dtm),
                                          "org.freedesktop.DBus.Properties.Get",
                                          g_variant_new ("(ss)", "org.freedesktop.timedate1", invalidated_properties[i]),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
        if (variant == NULL)
                g_warning ("Failed to get property '%s': %s", invalidated_properties[i], error->message);
        else {
                GVariant *v;

                g_variant_get (variant, "(v)", &v);
                g_dbus_proxy_set_cached_property (G_DBUS_PROXY (self->dtm), invalidated_properties[i], v);
        }
    }
}

static gboolean
keynav_failed (GtkWidget        *listbox,
               GtkDirectionType  direction,
               CcDateTimePanel  *self)
{
  GList *item, *listboxes;

  /* Find the listbox in the list of GtkListBoxes */
  if (direction == GTK_DIR_DOWN)
    listboxes = self->listboxes;
  else
    listboxes = self->listboxes_reverse;

  item = g_list_find (listboxes, listbox);
  g_assert (item);
  if (item->next)
    {
      gtk_widget_child_focus (GTK_WIDGET (item->next->data), direction);
      return TRUE;
    }

  return FALSE;
}

static void
run_dialog (CcDateTimePanel *self,
            GtkWidget       *dialog)
{
  GtkWidget *parent;

  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_dialog_run (GTK_DIALOG (dialog));
}

static gboolean
tz_switch_to_row_transform_func (GBinding        *binding,
                                 const GValue    *source_value,
                                 GValue          *target_value,
                                 CcDateTimePanel *self)
{
  gboolean active;
  gboolean allowed;
  gboolean location_allowed;

  active = g_value_get_boolean (source_value);
  allowed = (self->permission != NULL && g_permission_get_allowed (self->permission)) ||
            (self->tz_permission != NULL && g_permission_get_allowed (self->tz_permission));
  location_allowed = g_settings_get_boolean (self->location_settings, LOCATION_ENABLED);

  g_value_set_boolean (target_value, allowed && (!active || !location_allowed));

  return TRUE;
}

static gboolean
switch_to_row_transform_func (GBinding        *binding,
                              const GValue    *source_value,
                              GValue          *target_value,
                              CcDateTimePanel *self)
{
  gboolean active;
  gboolean allowed;

  active = g_value_get_boolean (source_value);
  allowed = (self->permission != NULL && g_permission_get_allowed (self->permission));

  g_value_set_boolean (target_value, !active && allowed);

  return TRUE;
}

static void
bind_switch_to_row (CcDateTimePanel *self,
                    GtkWidget       *gtkswitch,
                    GtkWidget       *listrow)
{
  g_object_bind_property_full (gtkswitch, "active",
                               listrow, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) switch_to_row_transform_func,
                               NULL, self, NULL);
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
  gtk_list_box_select_row (listbox, NULL);

  if (row == GTK_LIST_BOX_ROW (self->auto_datetime_row))
    {
      toggle_switch (self->network_time_switch);
    }
  else if (row == GTK_LIST_BOX_ROW (self->auto_timezone_row))
    {
      toggle_switch (self->auto_timezone_switch);
    }
  else if (row == GTK_LIST_BOX_ROW (self->datetime_button))
    {
      run_dialog (self, self->datetime_dialog);
    }
  else if (row == GTK_LIST_BOX_ROW (self->timezone_button))
    {
      run_dialog (self, self->timezone_dialog);
    }
}

static void
setup_listbox (CcDateTimePanel *self,
               GtkWidget       *listbox)
{
  gtk_list_box_set_header_func (GTK_LIST_BOX (listbox), cc_list_box_update_header_func, NULL, NULL);
  self->listboxes = g_list_append (self->listboxes, listbox);
  self->listboxes_reverse = g_list_prepend (self->listboxes_reverse, listbox);
}

static gboolean
format_minutes_combobox (GtkSpinButton *spin,
                         gpointer       data)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  int value;

  adjustment = gtk_spin_button_get_adjustment (spin);
  value = (int)gtk_adjustment_get_value (adjustment);
  text = g_strdup_printf ("%02d", value);
  gtk_entry_set_text (GTK_ENTRY (spin), text);

  return TRUE;
}

static gboolean
format_hours_combobox (GtkSpinButton   *spin,
                       CcDateTimePanel *panel)
{
  GtkAdjustment *adjustment;
  g_autofree gchar *text = NULL;
  int hour;
  gboolean use_ampm;

  if (panel->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
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

  return TRUE;
}

static void
setup_timezone_dialog (CcDateTimePanel *self)
{
  g_autoptr(GtkEntryCompletion) completion = NULL;

  /* set up timezone map */
  self->map = (GtkWidget *) cc_timezone_map_new ();
  gtk_widget_show (self->map);
  gtk_container_add (GTK_CONTAINER (self->aspectmap),
                     self->map);

  /* Create the completion object */
  completion = gtk_entry_completion_new ();
  gtk_entry_set_completion (GTK_ENTRY (self->timezone_searchentry), completion);

  gtk_entry_completion_set_model (completion, GTK_TREE_MODEL (self->city_modelsort));

  gtk_entry_completion_set_text_column (completion, CITY_COL_CITY_HUMAN_READABLE);
}

static char *
format_am_label ()
{
  g_autoptr(GDateTime) date = NULL;

  /* Construct a time at midnight, and use it to get localized AM identifier */
  date = g_date_time_new_utc (1, 1, 1, 0, 0, 0);
  return g_date_time_format (date, "%p");
}

static char *
format_pm_label ()
{
  g_autoptr(GDateTime) date = NULL;

  /* Construct a time at noon, and use it to get localized PM identifier */
  date = g_date_time_new_utc (1, 1, 1, 12, 0, 0);
  return g_date_time_format (date, "%p");
}

static void
setup_am_pm_button (CcDateTimePanel *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;
  GtkStyleContext *context;
  g_autofree gchar *am_text = NULL;
  g_autofree gchar *pm_text = NULL;

  am_text = format_am_label ();
  self->am_label = gtk_label_new (am_text);
  gtk_widget_show (self->am_label);

  pm_text = format_pm_label ();
  self->pm_label = gtk_label_new (pm_text);
  gtk_widget_show (self->pm_label);

  gtk_container_add (GTK_CONTAINER (self->am_pm_stack), self->am_label);
  gtk_container_add (GTK_CONTAINER (self->am_pm_stack), self->pm_label);
  am_pm_stack_visible_child_changed_cb (self);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   ".gnome-control-center-ampm-toggle-button {\n"
                                   "    font-size: 150%;\n"
                                   "}", -1, NULL);
  context = gtk_widget_get_style_context (self->am_pm_button);
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
setup_datetime_dialog (CcDateTimePanel *self)
{
  GtkAdjustment *adjustment;
  GdkScreen *screen;
  g_autoptr(GtkCssProvider) provider = NULL;
  guint num_days;

  setup_am_pm_button (self);

  /* Big time buttons */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   ".gnome-control-center-datetime-setup-time>spinbutton,\n"
                                   ".gnome-control-center-datetime-setup-time>label {\n"
                                   "    font-size: 250%;\n"
                                   "}\n"
                                   ".gnome-control-center-datetime-setup-time>spinbutton>entry {\n"
                                   "    padding: 8px 13px;\n"
                                   "}", -1, NULL);
  screen = gdk_screen_get_default ();
  gtk_style_context_add_provider_for_screen (screen,
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* Force the direction for the time, so that the time
   * is presented correctly for RTL languages */
  gtk_widget_set_direction (self->time_grid, GTK_TEXT_DIR_LTR);

  /* Month */
  gtk_combo_box_set_active (GTK_COMBO_BOX (self->month_combobox),
                            g_date_time_get_month (self->date) - 1);
  g_signal_connect_object (G_OBJECT (self->month_combobox), "changed",
                           G_CALLBACK (month_year_changed), self, G_CONNECT_SWAPPED);

  /* Day */
  num_days = g_date_get_days_in_month (g_date_time_get_month (self->date),
                                       g_date_time_get_year (self->date));
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_day_of_month (self->date), 1,
                                                    num_days + 1, 1, 10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (self->day_spinbutton),
                                  adjustment);
  g_signal_connect_object (G_OBJECT (self->day_spinbutton), "value-changed",
                           G_CALLBACK (day_changed), self, G_CONNECT_SWAPPED);

  /* Year */
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_year (self->date),
                                                    1, G_MAXDOUBLE, 1,
                                                    10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (self->year_spinbutton),
                                  adjustment);
  g_signal_connect_object (G_OBJECT (self->year_spinbutton), "value-changed",
                           G_CALLBACK (month_year_changed), self, G_CONNECT_SWAPPED);

  /* Hours and minutes */
  gtk_spin_button_set_increments (GTK_SPIN_BUTTON (self->h_spinbutton), 1, 0);
  gtk_spin_button_set_increments (GTK_SPIN_BUTTON (self->m_spinbutton), 1, 0);

  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->h_spinbutton), 0, 23);
  gtk_spin_button_set_range (GTK_SPIN_BUTTON (self->m_spinbutton), 0, 59);
}

static void
cc_date_time_panel_class_init (CcDateTimePanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->constructed = cc_date_time_panel_constructed;
  object_class->dispose = cc_date_time_panel_dispose;

  panel_class->get_help_uri = cc_date_time_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/datetime/cc-datetime-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, am_pm_button);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, am_pm_stack);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, aspectmap);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, auto_datetime_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, auto_timezone_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, auto_timezone_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, city_liststore);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, city_modelsort);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, datetime_button);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, datetime_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, datetime_label);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, format_combobox);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, h_spinbutton);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, listbox1);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, listbox2);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, lock_button);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, m_spinbutton);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, month_liststore);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, network_time_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, time_box);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, time_grid);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_button);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_label);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_searchentry);

  gtk_widget_class_bind_template_callback (widget_class, list_box_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, keynav_failed);
  gtk_widget_class_bind_template_callback (widget_class, am_pm_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, format_hours_combobox);
  gtk_widget_class_bind_template_callback (widget_class, format_minutes_combobox);
  gtk_widget_class_bind_template_callback (widget_class, change_time);
  gtk_widget_class_bind_template_callback (widget_class, change_clock_settings);
  gtk_widget_class_bind_template_callback (widget_class, am_pm_stack_visible_child_changed_cb);

  bind_textdomain_codeset (GETTEXT_PACKAGE_TIMEZONES, "UTF-8");
}

static void
cc_date_time_panel_init (CcDateTimePanel *self)
{
  g_autoptr(GError) error = NULL;
  const char *date_grid_name;
  g_autofree gchar *tmp = NULL;

  g_resources_register (cc_datetime_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  error = NULL;
  self->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.freedesktop.timedate1",
                                                "/org/freedesktop/timedate1",
                                                self->cancellable,
                                                &error);
  if (self->dtm == NULL) {
        g_warning ("could not get proxy for DateTimeMechanism: %s", error->message);
        return;
  }

  switch (date_endian_get_default (FALSE)) {
  case DATE_ENDIANESS_BIG:
    date_grid_name = "big";
    break;
  case DATE_ENDIANESS_LITTLE:
    date_grid_name = "little";
    break;
  case DATE_ENDIANESS_MIDDLE:
    date_grid_name = "middle";
    break;
  case DATE_ENDIANESS_YDM:
    date_grid_name = "ydm";
    break;
  default:
    g_assert_not_reached ();
  }

  self->builder = gtk_builder_new ();
  tmp = g_strdup_printf ("/org/gnome/control-center/datetime/%s.ui", date_grid_name);
  gtk_builder_add_from_resource (self->builder, tmp, NULL);
  self->date_grid = GTK_WIDGET (gtk_builder_get_object (self->builder, "date_grid"));
  self->day_spinbutton = GTK_WIDGET (gtk_builder_get_object (self->builder, "day_spinbutton"));
  self->month_combobox = GTK_WIDGET (gtk_builder_get_object (self->builder, "month_combobox"));
  gtk_combo_box_set_model (GTK_COMBO_BOX (self->month_combobox), GTK_TREE_MODEL (self->month_liststore));
  self->year_spinbutton = GTK_WIDGET (gtk_builder_get_object (self->builder, "year_spinbutton"));

  gtk_box_pack_end (GTK_BOX (self->time_box), self->date_grid, FALSE, TRUE, 0);

  /* add the lock button */
  self->permission = polkit_permission_new_sync (DATETIME_PERMISSION, NULL, NULL, NULL);
  self->tz_permission = polkit_permission_new_sync (DATETIME_TZ_PERMISSION, NULL, NULL, NULL);
  if (self->permission != NULL)
    {
      g_signal_connect_object (self->permission, "notify",
                               G_CALLBACK (on_permission_changed), self, G_CONNECT_SWAPPED);
    }
  else
    {
      g_warning ("Your system does not have the '%s' PolicyKit files installed. Please check your installation",
                 DATETIME_PERMISSION);
    }
  gtk_lock_button_set_permission (GTK_LOCK_BUTTON (self->lock_button), self->permission);

  self->location_settings = g_settings_new (LOCATION_SETTINGS);
  g_signal_connect_object (self->location_settings, "changed",
                           G_CALLBACK (on_location_settings_changed), self, G_CONNECT_SWAPPED);
  on_location_settings_changed (self);

  self->date = g_date_time_new_now_local ();

  /* Top level windows from GtkBuilder that need to be destroyed explicitly */
  self->toplevels = g_list_append (self->toplevels, self->datetime_dialog);
  self->toplevels = g_list_append (self->toplevels, self->timezone_dialog);

  setup_timezone_dialog (self);
  setup_datetime_dialog (self);

  setup_listbox (self, self->listbox1);
  setup_listbox (self, self->listbox2);

  /* set up network time switch */
  bind_switch_to_row (self,
                      self->network_time_switch,
                      self->datetime_button);
  g_object_bind_property (self->dtm, "ntp",
                          self->network_time_switch, "active",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (self->network_time_switch, "notify::active",
                           G_CALLBACK (change_ntp), self, G_CONNECT_SWAPPED);

  gtk_widget_set_visible (self->auto_datetime_row, is_ntp_available (self));

  /* Timezone settings */
  g_object_bind_property_full (self->auto_timezone_switch, "active",
                               self->timezone_button, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) tz_switch_to_row_transform_func,
                               NULL, self, NULL);

  self->datetime_settings = g_settings_new (DATETIME_SCHEMA);
  g_settings_bind (self->datetime_settings, AUTO_TIMEZONE_KEY,
                   self->auto_timezone_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Clock settings */
  self->clock_settings = g_settings_new (CLOCK_SCHEMA);

  /* setup the time itself */
  self->clock_tracker = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
  g_signal_connect_object (self->clock_tracker, "notify::clock", G_CALLBACK (on_clock_changed), self, G_CONNECT_SWAPPED);

  clock_settings_changed_cb (self, CLOCK_FORMAT_KEY);
  g_signal_connect_object (self->clock_settings, "changed::" CLOCK_FORMAT_KEY,
                           G_CALLBACK (clock_settings_changed_cb), self, G_CONNECT_SWAPPED);

  update_time (self);

  load_regions_model (GTK_LIST_STORE (self->city_liststore));

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->city_modelsort), CITY_COL_CITY_HUMAN_READABLE,
                                        GTK_SORT_ASCENDING);

  /* After the initial setup, so we can be sure that
   * the model is filled up */
  get_initial_timezone (self);

  g_signal_connect_object (gtk_entry_get_completion (GTK_ENTRY (self->timezone_searchentry)),
                           "match-selected", G_CALLBACK (city_changed_cb), self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->map, "location-changed",
                           G_CALLBACK (location_changed_cb), self, G_CONNECT_SWAPPED);

  /* Watch changes of timedated remote service properties */
  g_signal_connect_object (self->dtm, "g-properties-changed",
                           G_CALLBACK (on_timedated_properties_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->dtm, "notify::can-ntp",
                           G_CALLBACK (on_can_ntp_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->dtm, "notify::timezone",
                           G_CALLBACK (on_timezone_changed), self, G_CONNECT_SWAPPED);
  /* We ignore UTC <--> LocalRTC changes at the moment */

  self->filechooser_settings = g_settings_new (FILECHOOSER_SCHEMA);
}
