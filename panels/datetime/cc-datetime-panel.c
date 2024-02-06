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
#include "cc-list-row.h"
#include "cc-time-editor.h"
#include "cc-datetime-panel.h"
#include "cc-datetime-resources.h"
#include "cc-list-row.h"

#include <langinfo.h>
#include <sys/time.h>
#include "cc-tz-dialog.h"
#include "timedated.h"
#include "date-endian.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <gdesktop-enums.h>
#include "gdesktop-enums-types.h"
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

#define DATETIME_PERMISSION "org.gnome.controlcenter.datetime.configure"
#define DATETIME_TZ_PERMISSION "org.freedesktop.timedate1.set-timezone"
#define LOCATION_SETTINGS "org.gnome.system.location"
#define LOCATION_ENABLED "enabled"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"
#define CLOCK_SHOW_WEEKDAY_KEY "clock-show-weekday"
#define CLOCK_SHOW_DATE_KEY "clock-show-date"
#define CLOCK_SHOW_SECONDS_KEY "clock-show-seconds"

#define CALENDAR_SCHEMA "org.gnome.desktop.calendar"
#define CALENDAR_SHOW_WEEK_NUMBERS_KEY "show-weekdate"

#define FILECHOOSER_SCHEMA "org.gtk.Settings.FileChooser"

#define DATETIME_SCHEMA "org.gnome.desktop.datetime"
#define AUTO_TIMEZONE_KEY "automatic-timezone"

struct _CcDateTimePanel
{
  CcPanel parent_instance;

  GList *toplevels;

  TzLocation *current_location;

  GtkTreeModelFilter *city_filter;

  GDateTime *date;

  GSettings *clock_settings;
  GSettings *calendar_settings;
  GSettings *datetime_settings;
  GSettings *filechooser_settings;
  GDesktopClockFormat clock_format;
  AdwActionRow *auto_datetime_row;
  AdwActionRow *auto_timezone_row;
  GtkSwitch *auto_timezone_switch;
  AdwActionRow *datetime_row;
  GtkWindow *datetime_dialog;
  GtkLabel *datetime_label;
  GtkSpinButton *day_spinbutton;
  AdwComboRow *timeformat_row;
  GtkSpinButton *h_spinbutton;
  GtkWidget *weekday_row;
  GtkWidget *weekday_switch;
  GtkWidget *date_switch;
  GtkWidget *seconds_switch;
  GtkWidget *week_numbers_switch;
  GtkLockButton *lock_button;
  GtkListBox *date_box;
  AdwActionRow *day_row;
  GtkSingleSelection *month_model;
  GtkPopover  *month_popover;
  CcListRow *month_row;
  AdwActionRow *year_row;
  GtkSwitch *network_time_switch;
  CcTimeEditor *time_editor;
  AdwActionRow *timezone_row;
  CcTzDialog *timezone_dialog;
  GtkLabel *timezone_label;
  GtkSpinButton *year_spinbutton;

  GnomeWallClock *clock_tracker;

  Timedate1 *dtm;
  GCancellable *cancellable;

  gboolean pending_ntp_state;

  GPermission *permission;
  GPermission *tz_permission;
  GSettings *location_settings;

  int        month; /* index starts from 1 */
};

CC_PANEL_REGISTER (CcDateTimePanel, cc_date_time_panel)

static void update_time (CcDateTimePanel *self);

static void
cc_date_time_panel_dispose (GObject *object)
{
  CcDateTimePanel *self = CC_DATE_TIME_PANEL (object);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  if (self->toplevels)
    {
      g_list_free_full (self->toplevels, (GDestroyNotify) gtk_window_destroy);
      self->toplevels = NULL;
    }

  g_clear_object (&self->clock_tracker);
  g_clear_object (&self->dtm);
  g_clear_object (&self->permission);
  g_clear_object (&self->tz_permission);
  g_clear_object (&self->location_settings);
  g_clear_object (&self->clock_settings);
  g_clear_object (&self->calendar_settings);
  g_clear_object (&self->datetime_settings);
  g_clear_object (&self->filechooser_settings);

  g_clear_pointer (&self->date, g_date_time_unref);

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->dispose (object);
}

static const char *
cc_date_time_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/clock";
}

static void clock_settings_changed_cb (CcDateTimePanel *self,
                                       gchar           *key);

static char *
format_clock_name_cb (AdwEnumListItem *item,
                      gpointer         user_data)
{

  switch (adw_enum_list_item_get_value (item))
    {
    case G_DESKTOP_CLOCK_FORMAT_24H:
      return g_strdup (_("24-hour"));
    case G_DESKTOP_CLOCK_FORMAT_12H:
      return g_strdup (_("AM / PM"));
    default:
      return NULL;
    }
}

static void
change_clock_settings (CcDateTimePanel *self)
{
  GDesktopClockFormat value;
  AdwEnumListItem *item;

  g_signal_handlers_block_by_func (self->clock_settings, clock_settings_changed_cb,
                                   self);

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (self->timeformat_row));
  value = adw_enum_list_item_get_value (item);

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

  g_signal_handlers_block_by_func (self->timeformat_row, change_clock_settings, self);

  adw_combo_row_set_selected (self->timeformat_row, value);

  cc_time_editor_set_am_pm (self->time_editor,
                            value == G_DESKTOP_CLOCK_FORMAT_12H);
  update_time (self);

  g_signal_handlers_unblock_by_func (self->timeformat_row, change_clock_settings, self);
}


/* Update the widgets based on the system time */
static void
update_time (CcDateTimePanel *self)
{
  g_autofree gchar *label = NULL;
  gboolean use_ampm;

  if (self->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    use_ampm = TRUE;
  else
    use_ampm = FALSE;

  cc_time_editor_set_time (self->time_editor,
                           g_date_time_get_hour (self->date),
                           g_date_time_get_minute (self->date));

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

  self->month = g_date_time_get_month (self->date);
  gtk_single_selection_set_selected (self->month_model, self->month - 1);
  gtk_label_set_text (self->datetime_label, label);
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
  else
    {
      gtk_switch_set_state (self->network_time_switch, self->pending_ntp_state);
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
queue_set_ntp (CcDateTimePanel *self,
               gboolean         using_ntp)
{
  self->pending_ntp_state = using_ntp;
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
  guint y, d;
  g_autoptr(GDateTime) old_date = NULL;

  y = gtk_spin_button_get_value_as_int (self->year_spinbutton);
  d = gtk_spin_button_get_value_as_int (self->day_spinbutton);

  old_date = self->date;
  self->date = g_date_time_new_local (y, self->month, d,
                                      g_date_time_get_hour (old_date),
                                      g_date_time_get_minute (old_date),
                                      g_date_time_get_second (old_date));
  cc_time_editor_set_time (self->time_editor,
                           g_date_time_get_hour (self->date),
                           g_date_time_get_minute (self->date));

  queue_set_datetime (self);
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
  g_autofree gchar *city_country = NULL;
  g_autofree gchar *label = NULL;

  city_country = translated_city_name (self->current_location);

  /* Update the timezone on the listbow row */
  /* Translators: "timezone (details)" */
  label = g_strdup_printf (C_("timezone desc", "%s (%s)"),
                           g_date_time_get_timezone_abbreviation (self->date),
                           city_country);
  gtk_label_set_text (self->timezone_label, label);
}

static void
get_initial_timezone (CcDateTimePanel *self)
{
  const gchar *timezone;

  timezone = timedate1_get_timezone (self->dtm);

  if (timezone == NULL ||
      !cc_tz_dialog_set_tz (self->timezone_dialog, timezone))
    {
      g_warning ("Timezone '%s' is unhandled, setting %s as default", timezone ? timezone : "(null)", DEFAULT_TZ);
      cc_tz_dialog_set_tz (self->timezone_dialog, DEFAULT_TZ);
    }

  self->current_location = cc_tz_dialog_get_selected_location (self->timezone_dialog);
  update_timezone (self);
}

static void
day_changed (CcDateTimePanel *self)
{
  change_date (self);
}

static void
month_year_changed (CcDateTimePanel *self)
{
  guint y;
  guint num_days;
  GtkAdjustment *adj;

  y = gtk_spin_button_get_value_as_int (self->year_spinbutton);

  /* Check the number of days in that month */
  num_days = g_date_get_days_in_month (self->month, y);

  adj = GTK_ADJUSTMENT (gtk_spin_button_get_adjustment (self->day_spinbutton));
  gtk_adjustment_set_upper (adj, num_days + 1);

  if (gtk_spin_button_get_value_as_int (self->day_spinbutton) > num_days)
    gtk_spin_button_set_value (self->day_spinbutton, num_days);

  change_date (self);
}

static void
on_date_box_row_activated_cb (CcDateTimePanel *self,
                              GtkListBoxRow   *row)
{
  g_assert (CC_IS_DATE_TIME_PANEL (self));

  if (row == GTK_LIST_BOX_ROW (self->month_row))
    gtk_popover_popup (self->month_popover);
}

static void
on_month_selection_changed_cb (CcDateTimePanel *self)
{
  guint i;

  g_assert (CC_IS_DATE_TIME_PANEL (self));

  i = gtk_single_selection_get_selected (self->month_model);
  g_assert (i >= 0 && i < 12);

  self->month = i + 1;
  month_year_changed (self);

  gtk_popover_popdown (self->month_popover);
}

static void
on_clock_changed (CcDateTimePanel *self,
		  GParamSpec      *pspec)
{
  g_date_time_unref (self->date);
  self->date = g_date_time_new_now_local ();
  update_time (self);
  update_timezone (self);
}

static gboolean
change_ntp (CcDateTimePanel *self,
            gboolean         state)
{
  queue_set_ntp (self, state);

  /* The new state will be visible once we see the reply. */
  return TRUE;
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
  using_ntp = gtk_switch_get_active (self->network_time_switch);
  auto_timezone = gtk_switch_get_active (self->auto_timezone_switch);

  /* All the widgets but the lock button and the 24h setting */
  gtk_widget_set_sensitive (GTK_WIDGET (self->auto_datetime_row), allowed);
  gtk_widget_set_sensitive (GTK_WIDGET (self->auto_timezone_row), location_allowed && (allowed || tz_allowed));
  gtk_widget_set_sensitive (GTK_WIDGET (self->datetime_row), allowed && !using_ntp);
  gtk_widget_set_sensitive (GTK_WIDGET (self->timezone_row), (allowed || tz_allowed) && (!auto_timezone || !location_allowed));

  /* Hide the subdialogs if we no longer have permissions */
  if (!allowed)
      gtk_widget_set_visible (GTK_WIDGET (self->datetime_dialog), FALSE);
  if (!allowed && !tz_allowed)
      gtk_widget_set_visible (GTK_WIDGET (self->timezone_dialog), FALSE);
}

static void
on_location_settings_changed (CcDateTimePanel *self)
{
  on_permission_changed (self);
}

static void
on_can_ntp_changed (CcDateTimePanel *self)
{
  gtk_widget_set_visible (GTK_WIDGET (self->auto_datetime_row), is_ntp_available (self));
}

static void
on_timezone_changed (CcDateTimePanel *self)
{
  get_initial_timezone (self);
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

static void
present_window (CcDateTimePanel *self,
                GtkWindow       *window)
{
  GtkWidget *parent;

  parent = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

  gtk_window_set_transient_for (window, GTK_WINDOW (parent));
  gtk_window_present (window);
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
                    GtkSwitch       *gtkswitch,
                    GtkWidget       *listrow)
{
  g_object_bind_property_full (gtkswitch, "active",
                               listrow, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               (GBindingTransformFunc) switch_to_row_transform_func,
                               NULL, self, NULL);
}

static void
panel_tz_selection_changed_cb (CcDateTimePanel *self)
{
  g_assert (CC_IS_DATE_TIME_PANEL (self));

  self->current_location = cc_tz_dialog_get_selected_location (self->timezone_dialog);
  queue_set_timezone (self);
}

static void
list_box_row_activated (CcDateTimePanel *self,
                        GtkListBoxRow   *row)

{
  if (row == GTK_LIST_BOX_ROW (self->datetime_row))
    {
      present_window (self, self->datetime_dialog);
    }
  else if (row == GTK_LIST_BOX_ROW (self->timezone_row))
    {
      present_window (self, GTK_WINDOW (self->timezone_dialog));
    }
}

static void
time_changed_cb (CcDateTimePanel *self,
                 CcTimeEditor    *editor)
{
  g_autoptr(GDateTime) old_date = NULL;

  g_assert (CC_IS_DATE_TIME_PANEL (self));
  g_assert (CC_IS_TIME_EDITOR (editor));

  old_date = self->date;
  self->date = g_date_time_new_local (g_date_time_get_year (old_date),
                                      g_date_time_get_month (old_date),
                                      g_date_time_get_day_of_month (old_date),
                                      cc_time_editor_get_hour (self->time_editor),
                                      cc_time_editor_get_minute (self->time_editor),
                                      g_date_time_get_second (old_date));

  update_time (self);
  queue_set_datetime (self);
}

static void
setup_datetime_dialog (CcDateTimePanel *self)
{
  GtkAdjustment *adjustment;
  GdkDisplay *display;
  g_autoptr(GtkCssProvider) provider = NULL;
  guint num_days;

  /* Big time buttons */
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (provider),
                                   ".gnome-control-center-datetime-setup-time>spinbutton,\n"
                                   ".gnome-control-center-datetime-setup-time>label {\n"
                                   "    font-size: 250%;\n"
                                   "}\n"
                                   "gridview.month-grid > child {\n"
                                   "  background: transparent;\n"
                                   "}\n"
                                   ".gnome-control-center-datetime-setup-time>spinbutton>entry {\n"
                                   "    padding: 8px 13px;\n"
                                   "}", -1);
  display = gdk_display_get_default ();
  gtk_style_context_add_provider_for_display (display,
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* Day */
  num_days = g_date_get_days_in_month (g_date_time_get_month (self->date),
                                       g_date_time_get_year (self->date));
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_day_of_month (self->date), 1,
                                                    num_days + 1, 1, 10, 1);
  gtk_spin_button_set_adjustment (self->day_spinbutton, adjustment);
  g_signal_connect_object (G_OBJECT (self->day_spinbutton), "value-changed",
                           G_CALLBACK (day_changed), self, G_CONNECT_SWAPPED);

  /* Year */
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_year (self->date),
                                                    1, G_MAXDOUBLE, 1,
                                                    10, 1);
  gtk_spin_button_set_adjustment (self->year_spinbutton, adjustment);
  g_signal_connect_object (G_OBJECT (self->year_spinbutton), "value-changed",
                           G_CALLBACK (month_year_changed), self, G_CONNECT_SWAPPED);

  /* Month */
  self->month = g_date_time_get_month (self->date);
  gtk_single_selection_set_selected (self->month_model, self->month - 1);
}

static int
sort_date_box (GtkListBoxRow   *a,
               GtkListBoxRow   *b,
               CcDateTimePanel *self)
{
  GtkListBoxRow *day_row, *month_row, *year_row;

  g_assert (CC_IS_DATE_TIME_PANEL (self));

  day_row = GTK_LIST_BOX_ROW (self->day_row);
  month_row = GTK_LIST_BOX_ROW (self->month_row);
  year_row = GTK_LIST_BOX_ROW (self->year_row);

  switch (date_endian_get_default (FALSE)) {
  case DATE_ENDIANESS_BIG:
    /* year, month, day */
    if (a == year_row || b == day_row)
      return -1;
    if (a == day_row || b == year_row)
      return 1;

  case DATE_ENDIANESS_LITTLE:
    /* day, month, year */
    if (a == day_row || b == year_row)
      return -1;
    if (a == year_row || b == day_row)
      return 1;

  case DATE_ENDIANESS_MIDDLE:
    /* month, day, year */
    if (a == month_row || b == year_row)
      return -1;
    if (a == year_row || b == month_row)
      return 1;

  case DATE_ENDIANESS_YDM:
    /* year, day, month */
    if (a == year_row || b == month_row)
      return -1;
    if (a == month_row || b == year_row)
      return 1;
  }

  return 0;
}

static void
cc_date_time_panel_class_init (CcDateTimePanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_date_time_panel_dispose;

  panel_class->get_help_uri = cc_date_time_panel_get_help_uri;

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_TZ_DIALOG);
  g_type_ensure (G_DESKTOP_TYPE_DESKTOP_CLOCK_FORMAT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/datetime/cc-datetime-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, auto_datetime_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, auto_timezone_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, auto_timezone_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, date_box);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, datetime_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, datetime_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, datetime_label);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, day_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, day_spinbutton);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timeformat_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, weekday_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, date_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, seconds_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, week_numbers_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, lock_button);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, month_model);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, month_popover);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, month_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, network_time_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, time_editor);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, timezone_label);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, year_row);
  gtk_widget_class_bind_template_child (widget_class, CcDateTimePanel, year_spinbutton);

  gtk_widget_class_bind_template_callback (widget_class, panel_tz_selection_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, list_box_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, time_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, change_clock_settings);
  gtk_widget_class_bind_template_callback (widget_class, format_clock_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_date_box_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_month_selection_changed_cb);

  bind_textdomain_codeset (GETTEXT_PACKAGE_TIMEZONES, "UTF-8");

  g_type_ensure (CC_TYPE_TIME_EDITOR);
}

static void
cc_date_time_panel_init (CcDateTimePanel *self)
{
  g_autoptr(GError) error = NULL;

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

  gtk_list_box_set_sort_func (self->date_box,
                              (GtkListBoxSortFunc)sort_date_box,
                              self, NULL);
  gtk_list_box_invalidate_sort (self->date_box);

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

  /* setup_timezone_dialog (self); */
  setup_datetime_dialog (self);

  /* set up network time switch */
  bind_switch_to_row (self,
                      self->network_time_switch,
                      GTK_WIDGET (self->datetime_row));
  g_object_bind_property (self->dtm, "ntp",
                          self->network_time_switch, "active",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  g_signal_connect_object (self->network_time_switch, "state-set",
                           G_CALLBACK (change_ntp), self, G_CONNECT_SWAPPED);

  gtk_widget_set_visible (GTK_WIDGET (self->auto_datetime_row), is_ntp_available (self));

  /* Timezone settings */
  g_object_bind_property_full (self->auto_timezone_switch, "active",
                               self->timezone_row, "sensitive",
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

  /* setup top bar clock setting switches */
  g_settings_bind (self->clock_settings, CLOCK_SHOW_WEEKDAY_KEY,
                   self->weekday_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->clock_settings, CLOCK_SHOW_DATE_KEY,
                   self->date_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->clock_settings, CLOCK_SHOW_SECONDS_KEY,
                   self->seconds_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Calendar settings */
  self->calendar_settings = g_settings_new (CALENDAR_SCHEMA);

  g_settings_bind (self->calendar_settings, CALENDAR_SHOW_WEEK_NUMBERS_KEY,
                   self->week_numbers_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  update_time (self);

  /* After the initial setup, so we can be sure that
   * the model is filled up */
  get_initial_timezone (self);

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
