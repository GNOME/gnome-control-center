/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2018-2020 Purism SPC
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
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>

#include "cc-break-schedule-row.h"
#include "cc-duration-row.h"
#include "cc-screen-time-statistics-row.h"
#include "cc-wellbeing-panel.h"
#include "cc-wellbeing-resources.h"

/* All in seconds */
static const struct
  {
    guint duration_secs;
    guint interval_secs;
  }
movement_break_schedule_values[] =
  {
    { 1 * 60, 20 * 60 },
    { 2 * 60, 20 * 60 },
    { 3 * 60, 30 * 60 },
    { 5 * 60, 30 * 60 },
    { 0, 0 },  /* 0-terminator */
  };

struct _CcWellbeingPanel {
  CcPanel parent_instance;

  GSettings *screen_time_limits_settings;  /* (owned) */
  GSettings *break_reminders_settings;  /* (owned) */
  GSettings *eyesight_break_settings;  /* (owned) */
  GSettings *movement_break_settings;  /* (owned) */

  GListStore *movement_break_schedule_list;  /* (owned) */

  CcScreenTimeStatisticsRow *screen_time_statistics_row;

  GtkMenuButton *screen_time_statistics_info_button;
  GtkLabel *screen_time_statistics_disable_description;
  AdwPreferencesRow *screen_time_statistics_disabled_row;

  AdwSwitchRow *screen_time_limit_row;
  AdwComboRow *daily_time_limit_row;
  AdwSwitchRow *grayscale_row;

  AdwSwitchRow *eyesight_breaks_row;
  AdwSwitchRow *movement_breaks_row;
  AdwComboRow *movement_break_schedule_row;
  AdwSwitchRow *sounds_row;

  gulong screen_time_limits_settings_writable_changed_daily_limit_seconds_id;
  gulong screen_time_limits_settings_writable_changed_grayscale_id;
  gulong screen_time_limits_settings_changed_history_enabled_id;
  gulong screen_time_limits_settings_writable_changed_daily_limit_enabled_id;
  gulong screen_time_limits_settings_changed_daily_limit_enabled_id;

  gulong movement_break_schedule_notify_selected_item_id;
  gulong movement_break_settings_changed_duration_id;
  gulong movement_break_settings_changed_interval_id;
  gulong movement_break_settings_writable_changed_duration_id;
  gulong movement_break_settings_writable_changed_interval_id;

  gulong sounds_notify_active_id;
  gulong eyesight_break_settings_changed_play_sound_id;
  gulong movement_break_settings_changed_play_sound_id;
  gulong eyesight_break_settings_writable_changed_play_sound_id;
  gulong movement_break_settings_writable_changed_play_sound_id;
  gulong break_settings_changed_selected_breaks_id;
};

struct _CcWellbeingPanelClass {
  CcPanelClass parent;
};

CC_PANEL_REGISTER (CcWellbeingPanel, cc_wellbeing_panel);

static void enable_screen_time_recording_button_clicked_cb (GtkButton *button,
                                                            gpointer   user_data);
static void disable_screen_time_recording_button_clicked_cb (GtkButton *button,
                                                             gpointer   user_data);
static void update_screen_time_limits_enabled (CcWellbeingPanel *self);
static void update_daily_time_limit_and_grayscale_row_sensitivity (CcWellbeingPanel *self);

static void movement_break_schedule_notify_selected_item_cb (GObject    *object,
                                                             GParamSpec *pspec,
                                                             gpointer    user_data);
static void break_settings_changed_duration_or_interval_cb (GSettings  *settings,
                                                            const char *key,
                                                            gpointer    user_data);
static void break_settings_writable_changed_duration_or_interval_cb (GSettings  *settings,
                                                                     const char *key,
                                                                     gpointer    user_data);

static void sounds_notify_active_cb (GObject    *object,
                                     GParamSpec *pspec,
                                     gpointer    user_data);
static void break_settings_changed_play_sound_cb (GSettings  *settings,
                                                  const char *key,
                                                  gpointer    user_data);
static void break_settings_writable_changed_play_sound_cb (GSettings  *settings,
                                                           const char *key,
                                                           gpointer    user_data);
static void break_settings_changed_selected_breaks_cb (GSettings  *settings,
                                                       const char *key,
                                                       gpointer    user_data);
static gboolean seconds_to_minutes (GValue   *value,
                                    GVariant *variant,
                                    gpointer  user_data);
static GVariant *minutes_to_seconds (const GValue       *value,
                                     const GVariantType *expected_type,
                                     gpointer            user_data);

/* A set of functions to use with g_settings_bind_with_mapping() which bind a
 * boolean to the existence of a given `expected_member` in a strv-typed setting.
 * They unfortunately have to use an auxiliary struct since they need to query
 * the current value of the setting when building its new value. */
typedef struct {
  GSettings *settings;  /* (not owned) (not nullable) */
  const char *key;  /* must be statically allocated */
  const char *expected_member;  /* must be statically allocated */
} SettingsStrvBindingData;

static void
settings_strv_binding_data_free (SettingsStrvBindingData *data)
{
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SettingsStrvBindingData, settings_strv_binding_data_free)

static SettingsStrvBindingData *
settings_strv_binding_data_new (GSettings  *settings,
                                const char *key,
                                const char *expected_member)
{
  g_autoptr(SettingsStrvBindingData) data = g_new0 (SettingsStrvBindingData, 1);

  data->settings = settings;  /* the binding which owns this struct is guaranteed to keep the GSettings alive */
  data->key = key;
  data->expected_member = expected_member;

  return g_steal_pointer (&data);
}

static gboolean
settings_strv_contains_str (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  g_autofree const char **strv = g_variant_get_strv (variant, NULL);
  const SettingsStrvBindingData *binding_data = user_data;

  g_value_set_boolean (value, g_strv_contains (strv, binding_data->expected_member));

  return TRUE;
}

static GVariant *
settings_strv_add_or_remove_str (const GValue       *value,
                                 const GVariantType *expected_type,
                                 gpointer            user_data)
{
  gboolean should_add = g_value_get_boolean (value);
  const SettingsStrvBindingData *binding_data = user_data;
  g_auto(GVariantBuilder) builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE_STRING_ARRAY);
  g_autoptr(GVariantIter) iter = NULL;
  const char *member;
  gboolean match_seen = FALSE;

  g_assert (g_variant_type_equal (expected_type, G_VARIANT_TYPE_STRING_ARRAY));

  /* Iterate through the current value of the setting. If the expected_member
   * is to be removed (`!should_add`) then skip it if it’s seen in the current
   * value. Otherwise, add all values. If the expected_member is to be added,
   * and it hasn’t been by the time the loop ends, add it. */
  g_settings_get (binding_data->settings, binding_data->key, "as", &iter);

  while (g_variant_iter_loop (iter, "&s", &member))
    {
      gboolean match = g_str_equal (member, binding_data->expected_member);
      if (should_add || !match)
        g_variant_builder_add (&builder, "s", member);
      if (match)
        match_seen = TRUE;
    }

  if (should_add && !match_seen)
    g_variant_builder_add (&builder, "s", binding_data->expected_member);

  return g_variant_builder_end (&builder);
}

static void
cc_wellbeing_panel_init (CcWellbeingPanel *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;
  g_autoptr(GFile) history_file = NULL;

  g_resources_register (cc_wellbeing_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/wellbeing/wellbeing.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* Set up the movement break schedule combo row. It’s a highly customised
   * #AdwComboRow because it needs to represent two settings at once. */
  self->movement_break_schedule_list = g_list_store_new (CC_TYPE_BREAK_SCHEDULE);

  for (gsize i = 0; movement_break_schedule_values[i].duration_secs != 0; i++)
    {
      g_autoptr(CcBreakSchedule) schedule = cc_break_schedule_new (movement_break_schedule_values[i].duration_secs,
                                                                   movement_break_schedule_values[i].interval_secs);
      g_list_store_append (self->movement_break_schedule_list, schedule);
    }

  adw_combo_row_set_model (self->movement_break_schedule_row, G_LIST_MODEL (self->movement_break_schedule_list));

  /* Set up settings bindings for screen time limits. */
  self->screen_time_limits_settings = g_settings_new ("org.gnome.desktop.screen-time-limits");

  g_settings_bind (self->screen_time_limits_settings,
                   "daily-limit-enabled",
                   self->screen_time_limit_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
  g_settings_bind_with_mapping (self->screen_time_limits_settings,
                                "daily-limit-seconds",
                                self->daily_time_limit_row,
                                "duration",
                                G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY,
                                seconds_to_minutes,
                                minutes_to_seconds,
                                NULL,
                                NULL);
  g_settings_bind (self->screen_time_limits_settings,
                   "grayscale",
                   self->grayscale_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

  self->screen_time_limits_settings_changed_history_enabled_id =
      g_signal_connect_swapped (self->screen_time_limits_settings, "changed::history-enabled",
                                G_CALLBACK (update_screen_time_limits_enabled), self);

  update_screen_time_limits_enabled (self);

  /* Sensitivity has to be handled separately for the screen time settings
   * because it’s the combination of multiple inputs. */
  self->screen_time_limits_settings_writable_changed_daily_limit_seconds_id =
      g_signal_connect_swapped (self->screen_time_limits_settings, "writable-changed::daily-limit-seconds",
                                G_CALLBACK (update_daily_time_limit_and_grayscale_row_sensitivity), self);
  self->screen_time_limits_settings_writable_changed_grayscale_id =
      g_signal_connect_swapped (self->screen_time_limits_settings, "writable-changed::grayscale",
                                G_CALLBACK (update_daily_time_limit_and_grayscale_row_sensitivity), self);
  self->screen_time_limits_settings_writable_changed_daily_limit_enabled_id =
      g_signal_connect_swapped (self->screen_time_limits_settings, "writable-changed::daily-limit-enabled",
                                G_CALLBACK (update_daily_time_limit_and_grayscale_row_sensitivity), self);
  self->screen_time_limits_settings_changed_daily_limit_enabled_id =
      g_signal_connect_swapped (self->screen_time_limits_settings, "changed::daily-limit-enabled",
                                G_CALLBACK (update_daily_time_limit_and_grayscale_row_sensitivity), self);

  update_daily_time_limit_and_grayscale_row_sensitivity (self);

  /* Set up settings bindings for break reminders. */
  self->break_reminders_settings = g_settings_new ("org.gnome.desktop.break-reminders");
  self->eyesight_break_settings = g_settings_new ("org.gnome.desktop.break-reminders.eyesight");
  self->movement_break_settings = g_settings_new ("org.gnome.desktop.break-reminders.movement");

  /* Put the movement break settings object into delayed-apply mode so that
   * changes to the movement schedule are sent atomically.
   * Note this means that all settings changes to it must be explicitly committed
   * using g_settings_apply(). */
  g_settings_delay (self->movement_break_settings);

  g_settings_bind_with_mapping (self->break_reminders_settings,
                                "selected-breaks",
                                self->eyesight_breaks_row,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                settings_strv_contains_str,
                                settings_strv_add_or_remove_str,
                                settings_strv_binding_data_new (self->break_reminders_settings, "selected-breaks", "eyesight"),
                                (GDestroyNotify) settings_strv_binding_data_free);
  g_settings_bind_with_mapping (self->break_reminders_settings,
                                "selected-breaks",
                                self->movement_breaks_row,
                                "active",
                                G_SETTINGS_BIND_DEFAULT,
                                settings_strv_contains_str,
                                settings_strv_add_or_remove_str,
                                settings_strv_binding_data_new (self->break_reminders_settings, "selected-breaks", "movement"),
                                (GDestroyNotify) settings_strv_binding_data_free);

  /* We can’t use GSettings bindings for the movement break schedule, as the one
   * widget needs to represent two settings. */
  self->movement_break_schedule_notify_selected_item_id =
      g_signal_connect (self->movement_break_schedule_row, "notify::selected-item",
                        G_CALLBACK (movement_break_schedule_notify_selected_item_cb), self);
  self->movement_break_settings_changed_duration_id =
      g_signal_connect (self->movement_break_settings, "changed::duration",
                        G_CALLBACK (break_settings_changed_duration_or_interval_cb), self);
  self->movement_break_settings_changed_interval_id =
      g_signal_connect (self->movement_break_settings, "changed::interval",
                        G_CALLBACK (break_settings_changed_duration_or_interval_cb), self);
  self->movement_break_settings_writable_changed_duration_id =
      g_signal_connect (self->movement_break_settings, "writable-changed::duration",
                        G_CALLBACK (break_settings_writable_changed_duration_or_interval_cb), self);
  self->movement_break_settings_writable_changed_interval_id =
      g_signal_connect (self->movement_break_settings, "writable-changed::interval",
                        G_CALLBACK (break_settings_writable_changed_duration_or_interval_cb), self);

  break_settings_changed_duration_or_interval_cb (NULL, NULL, self);
  break_settings_writable_changed_duration_or_interval_cb (NULL, NULL, self);

  /* We can’t use GSettings bindings for sound, as there can only be one binding
   * per object::property yet it needs to set play-sound on two child schemas
   * (one for eyesight breaks and one for movement breaks). */
  self->sounds_notify_active_id =
      g_signal_connect (self->sounds_row, "notify::active",
                        G_CALLBACK (sounds_notify_active_cb), self);
  self->eyesight_break_settings_changed_play_sound_id =
      g_signal_connect (self->eyesight_break_settings, "changed::play-sound",
                        G_CALLBACK (break_settings_changed_play_sound_cb), self);
  self->movement_break_settings_changed_play_sound_id =
      g_signal_connect (self->movement_break_settings, "changed::play-sound",
                        G_CALLBACK (break_settings_changed_play_sound_cb), self);
  self->eyesight_break_settings_writable_changed_play_sound_id =
      g_signal_connect (self->eyesight_break_settings, "writable-changed::play-sound",
                        G_CALLBACK (break_settings_writable_changed_play_sound_cb), self);
  self->movement_break_settings_writable_changed_play_sound_id =
      g_signal_connect (self->movement_break_settings, "writable-changed::play-sound",
                        G_CALLBACK (break_settings_writable_changed_play_sound_cb), self);
  self->break_settings_changed_selected_breaks_id =
      g_signal_connect (self->break_reminders_settings, "changed::selected-breaks",
                        G_CALLBACK (break_settings_changed_selected_breaks_cb), self);

  break_settings_changed_play_sound_cb (NULL, NULL, self);
  break_settings_writable_changed_play_sound_cb (NULL, NULL, self);
  break_settings_changed_selected_breaks_cb (NULL, NULL, self);

  /* Load the screen time history from gnome-shell. */
  history_file = g_file_new_build_filename (g_get_user_data_dir (), "gnome-shell", "session-active-history.json", NULL);
  cc_screen_time_statistics_row_set_history_file (self->screen_time_statistics_row, history_file);
}

static void
enable_screen_time_recording_button_clicked_cb (GtkButton *button,
                                                gpointer   user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);

  g_settings_set_boolean (self->screen_time_limits_settings, "history-enabled", TRUE);
}

static void
disable_screen_time_recording_button_clicked_cb (GtkButton *button,
                                                 gpointer   user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);

  g_settings_set_boolean (self->screen_time_limits_settings, "history-enabled", FALSE);
}

static void
update_screen_time_limits_enabled (CcWellbeingPanel *self)
{
  gboolean history_enabled = g_settings_get_boolean (self->screen_time_limits_settings, "history-enabled");

  gtk_widget_set_visible (GTK_WIDGET (self->screen_time_statistics_row), history_enabled);
  gtk_widget_set_visible (GTK_WIDGET (self->screen_time_statistics_disabled_row), !history_enabled);
  gtk_widget_set_visible (GTK_WIDGET (self->screen_time_statistics_info_button), history_enabled);

  if (!history_enabled)
    g_settings_set_boolean (self->screen_time_limits_settings, "daily-limit-enabled", FALSE);

  update_daily_time_limit_and_grayscale_row_sensitivity (self);
}

static void
update_daily_time_limit_and_grayscale_row_sensitivity (CcWellbeingPanel *self)
{
  gboolean history_enabled = g_settings_get_boolean (self->screen_time_limits_settings, "history-enabled");
  gboolean daily_limit_enabled_writable = g_settings_is_writable (self->screen_time_limits_settings, "daily-limit-enabled");
  gboolean daily_limit_enabled = g_settings_get_boolean (self->screen_time_limits_settings, "daily-limit-enabled");
  gboolean daily_limit_seconds_writable = g_settings_is_writable (self->screen_time_limits_settings, "daily-limit-seconds");
  gboolean grayscale_writable = g_settings_is_writable (self->screen_time_limits_settings, "grayscale");

  gtk_widget_set_sensitive (GTK_WIDGET (self->screen_time_limit_row),
                            history_enabled && daily_limit_enabled_writable);
  gtk_widget_set_sensitive (GTK_WIDGET (self->daily_time_limit_row),
                            history_enabled && daily_limit_enabled && daily_limit_seconds_writable);
  gtk_widget_set_sensitive (GTK_WIDGET (self->grayscale_row),
                            history_enabled && daily_limit_enabled && grayscale_writable);
}

static void
screen_time_statistics_disable_popover_show_cb (CcWellbeingPanel *self)
{
  const char *text = gtk_label_get_label (self->screen_time_statistics_disable_description);

  gtk_accessible_announce (GTK_ACCESSIBLE (self),
                           text,
                           GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
}

static void
movement_break_schedule_notify_selected_item_cb (GObject    *object,
                                                 GParamSpec *pspec,
                                                 gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  CcBreakSchedule *selected_item  = CC_BREAK_SCHEDULE (adw_combo_row_get_selected_item (self->movement_break_schedule_row));

  g_settings_set_uint (self->movement_break_settings, "duration-seconds", cc_break_schedule_get_duration_secs (selected_item));
  g_settings_set_uint (self->movement_break_settings, "interval-seconds", cc_break_schedule_get_interval_secs (selected_item));
  g_settings_apply (self->movement_break_settings);
}

static gboolean
break_schedule_equal_cb (gconstpointer a,
                         gconstpointer b)
{
  return cc_break_schedule_compare ((CcBreakSchedule *) a, (CcBreakSchedule *) b) == 0;
}

static gint
break_schedule_compare_cb (gconstpointer a,
                           gconstpointer b,
                           gpointer      user_data)
{
  return cc_break_schedule_compare ((CcBreakSchedule *) a, (CcBreakSchedule *) b);
}

static void
break_settings_changed_duration_or_interval_cb (GSettings  *settings,
                                                const char *key,
                                                gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  guint duration_seconds = g_settings_get_uint (self->movement_break_settings, "duration-seconds");
  guint interval_seconds = g_settings_get_uint (self->movement_break_settings, "interval-seconds");
  g_autoptr(CcBreakSchedule) settings_schedule = cc_break_schedule_new (duration_seconds, interval_seconds);
  guint selected_index;

  if (!g_list_store_find_with_equal_func (self->movement_break_schedule_list, settings_schedule, break_schedule_equal_cb, &selected_index))
    {
      gboolean again;

      /* Temporarily add the new schedule to the list, so that the user’s chosen
       * settings can be represented in the UI. */
      g_list_store_append (self->movement_break_schedule_list, settings_schedule);
      g_list_store_sort (self->movement_break_schedule_list, break_schedule_compare_cb, NULL);
      again = g_list_store_find_with_equal_func (self->movement_break_schedule_list, settings_schedule, break_schedule_equal_cb, &selected_index);
      g_assert (again);
    }

  adw_combo_row_set_selected (self->movement_break_schedule_row, selected_index);
}

static void
update_movement_break_schedule_row_sensitivity (CcWellbeingPanel *self)
{
  g_auto(GStrv) selected_breaks = g_settings_get_strv (self->break_reminders_settings, "selected-breaks");
  gboolean writable = (g_settings_is_writable (self->movement_break_settings, "duration-seconds") &&
                       g_settings_is_writable (self->movement_break_settings, "interval-seconds"));
  gboolean enabled = g_strv_contains ((const char * const *) selected_breaks, "movement");

  gtk_widget_set_sensitive (GTK_WIDGET (self->movement_break_schedule_row), writable && enabled);
}

static void
break_settings_writable_changed_duration_or_interval_cb (GSettings  *settings,
                                                         const char *key,
                                                         gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  update_movement_break_schedule_row_sensitivity (self);
}

static void
sounds_notify_active_cb (GObject    *object,
                         GParamSpec *pspec,
                         gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  gboolean play_sound = adw_switch_row_get_active (self->sounds_row);

  /* Unfortunately we can’t delay/apply the GSettings changes here so that the
   * results are notified atomically, as the changes apply to two different
   * schemas. So instead, block the changed signals. */
  g_signal_handler_block (self->eyesight_break_settings, self->eyesight_break_settings_changed_play_sound_id);
  g_signal_handler_block (self->movement_break_settings, self->movement_break_settings_changed_play_sound_id);

  g_settings_set_boolean (self->eyesight_break_settings, "play-sound", play_sound);
  g_settings_set_boolean (self->movement_break_settings, "play-sound", play_sound);
  g_settings_apply (self->movement_break_settings);

  g_signal_handler_unblock (self->movement_break_settings, self->movement_break_settings_changed_play_sound_id);
  g_signal_handler_unblock (self->eyesight_break_settings, self->eyesight_break_settings_changed_play_sound_id);
}

static void
break_settings_changed_play_sound_cb (GSettings  *settings,
                                      const char *key,
                                      gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  gboolean play_sound = (g_settings_get_boolean (self->eyesight_break_settings, "play-sound") &&
                         g_settings_get_boolean (self->movement_break_settings, "play-sound"));

  adw_switch_row_set_active (self->sounds_row, play_sound);
}

static void
update_sounds_row_sensitivity (CcWellbeingPanel *self)
{
  g_auto(GStrv) selected_breaks = g_settings_get_strv (self->break_reminders_settings, "selected-breaks");
  gboolean writable = (g_settings_is_writable (self->eyesight_break_settings, "play-sound") &&
                       g_settings_is_writable (self->movement_break_settings, "play-sound"));
  gboolean enabled = (g_strv_contains ((const char * const *) selected_breaks, "eyesight") ||
                      g_strv_contains ((const char * const *) selected_breaks, "movement"));

  gtk_widget_set_sensitive (GTK_WIDGET (self->sounds_row), writable && enabled);
}

static void
break_settings_writable_changed_play_sound_cb (GSettings  *settings,
                                               const char *key,
                                               gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  update_sounds_row_sensitivity (self);
}

static void
break_settings_changed_selected_breaks_cb (GSettings  *settings,
                                           const char *key,
                                           gpointer    user_data)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (user_data);
  update_sounds_row_sensitivity (self);
  update_movement_break_schedule_row_sensitivity (self);
}

static gboolean
seconds_to_minutes (GValue   *value,
                    GVariant *variant,
                    gpointer  user_data)
{
  /* Round it up rather than truncating; this is probably more natural for the user */
  g_value_set_uint (value, (g_variant_get_uint32 (variant) + (60 - 1)) / 60);

  return TRUE;
}

static GVariant *
minutes_to_seconds (const GValue       *value,
                    const GVariantType *expected_type,
                    gpointer            user_data)
{
  g_assert (g_variant_type_equal (expected_type, G_VARIANT_TYPE_UINT32));

  return g_variant_new_uint32 (g_value_get_uint (value) * 60);
}

static void
cc_wellbeing_panel_dispose (GObject *object)
{
  CcWellbeingPanel *self = CC_WELLBEING_PANEL (object);

  g_clear_signal_handler (&self->screen_time_limits_settings_writable_changed_daily_limit_seconds_id, self->screen_time_limits_settings);
  g_clear_signal_handler (&self->screen_time_limits_settings_writable_changed_grayscale_id, self->screen_time_limits_settings);
  g_clear_signal_handler (&self->screen_time_limits_settings_writable_changed_daily_limit_enabled_id, self->screen_time_limits_settings);
  g_clear_signal_handler (&self->screen_time_limits_settings_changed_daily_limit_enabled_id, self->screen_time_limits_settings);
  g_clear_signal_handler (&self->movement_break_schedule_notify_selected_item_id, self->movement_break_schedule_row);
  g_clear_signal_handler (&self->movement_break_settings_changed_duration_id, self->movement_break_settings);
  g_clear_signal_handler (&self->movement_break_settings_changed_interval_id, self->movement_break_settings);
  g_clear_signal_handler (&self->movement_break_settings_writable_changed_duration_id, self->movement_break_settings);
  g_clear_signal_handler (&self->movement_break_settings_writable_changed_interval_id, self->movement_break_settings);
  g_clear_signal_handler (&self->sounds_notify_active_id, self->sounds_row);
  g_clear_signal_handler (&self->eyesight_break_settings_changed_play_sound_id, self->eyesight_break_settings);
  g_clear_signal_handler (&self->movement_break_settings_changed_play_sound_id, self->movement_break_settings);
  g_clear_signal_handler (&self->eyesight_break_settings_writable_changed_play_sound_id, self->eyesight_break_settings);
  g_clear_signal_handler (&self->movement_break_settings_writable_changed_play_sound_id, self->movement_break_settings);
  g_clear_signal_handler (&self->break_settings_changed_selected_breaks_id, self->break_reminders_settings);

  g_clear_object (&self->movement_break_schedule_list);

  g_clear_object (&self->eyesight_break_settings);
  g_clear_object (&self->movement_break_settings);
  g_clear_object (&self->break_reminders_settings);
  g_clear_object (&self->screen_time_limits_settings);

  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_WELLBEING_PANEL);

  G_OBJECT_CLASS (cc_wellbeing_panel_parent_class)->dispose (object);
}

static const char *
cc_wellbeing_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/shell-wellbeing";
}

static void
cc_wellbeing_panel_class_init (CcWellbeingPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class  = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_wellbeing_panel_dispose;

  panel_class->get_help_uri = cc_wellbeing_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/wellbeing/cc-wellbeing-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, screen_time_statistics_info_button);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, screen_time_statistics_disable_description);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, screen_time_statistics_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, screen_time_statistics_disabled_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, screen_time_limit_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, daily_time_limit_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, grayscale_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, eyesight_breaks_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, movement_breaks_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, movement_break_schedule_row);
  gtk_widget_class_bind_template_child (widget_class, CcWellbeingPanel, sounds_row);

  gtk_widget_class_bind_template_callback (widget_class, enable_screen_time_recording_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, disable_screen_time_recording_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, screen_time_statistics_disable_popover_show_cb);

  g_type_ensure (CC_TYPE_BREAK_SCHEDULE_ROW);
  g_type_ensure (CC_TYPE_DURATION_ROW);
  g_type_ensure (CC_TYPE_SCREEN_TIME_STATISTICS_ROW);
}

