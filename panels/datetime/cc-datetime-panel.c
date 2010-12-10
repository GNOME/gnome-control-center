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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-datetime-panel.h"

#include "cc-timezone-map.h"
#include "set-timezone.h"
#include <gsettings-desktop-schemas/gdesktop-enums.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#define GETTEXT_PACKAGE_TIMEZONES GETTEXT_PACKAGE "-timezones"

G_DEFINE_DYNAMIC_TYPE (CcDateTimePanel, cc_date_time_panel, CC_TYPE_PANEL)

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
};

static void update_time (CcDateTimePanel *self);

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

  if (priv->builder)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }

  if (priv->settings)
    {
      g_object_unref (priv->settings);
      priv->settings = NULL;
    }

  if (priv->date)
    {
      g_date_time_unref (priv->date);
      priv->date = NULL;
    }

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->dispose (object);
}

static void
cc_date_time_panel_class_init (CcDateTimePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDateTimePanelPrivate));

  object_class->get_property = cc_date_time_panel_get_property;
  object_class->set_property = cc_date_time_panel_set_property;
  object_class->dispose = cc_date_time_panel_dispose;
}

static void
cc_date_time_panel_class_finalize (CcDateTimePanelClass *klass)
{

}

static void clock_settings_changed_cb (GSettings       *settings,
                                       gchar           *key,
                                       CcDateTimePanel *panel);

static void
change_clock_settings (GtkWidget       *widget,
                       CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  GDesktopClockFormat value;

  g_signal_handlers_block_by_func (priv->settings, clock_settings_changed_cb,
                                   panel);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (W ("12_radiobutton"))))
    value = G_DESKTOP_CLOCK_FORMAT_12H;
  else
    value = G_DESKTOP_CLOCK_FORMAT_24H;

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
  GtkWidget *radio12, *radio24;
  gboolean use_12_hour;
  GDesktopClockFormat value;

  value = g_settings_get_enum (settings, CLOCK_FORMAT_KEY);
  priv->clock_format = value;

  radio12 = W ("12_radiobutton");
  radio24 = W ("24_radiobutton");

  use_12_hour = (value == G_DESKTOP_CLOCK_FORMAT_12H);

  g_signal_handlers_block_by_func (radio12, change_clock_settings, panel);
  g_signal_handlers_block_by_func (radio24, change_clock_settings, panel);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio12), use_12_hour);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio24), !use_12_hour);

  update_time (panel);

  g_signal_handlers_unblock_by_func (radio12, change_clock_settings, panel);
  g_signal_handlers_unblock_by_func (radio24, change_clock_settings, panel);
}

static void
update_time (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  char *label;
  char *am_pm_widgets[] = {"ampm_up_button", "ampm_down_button", "ampm_label" };
  guint i;

  if (priv->clock_format == G_DESKTOP_CLOCK_FORMAT_24H)
    {
      /* Update the hours label */
      label = g_date_time_format (priv->date, "%H"); 
      gtk_label_set_text (GTK_LABEL (W("hours_label")), label);
      g_free (label);
    }
  else
    {
      /* Update the hours label */
      label = g_date_time_format (priv->date, "%I");
      gtk_label_set_text (GTK_LABEL (W("hours_label")), label);
      g_free (label);

      /* Set AM/PM */
      label = g_date_time_format (priv->date, "%p");
      gtk_label_set_text (GTK_LABEL (W("ampm_label")), label);
      g_free (label);
    }

  for (i = 0; i < G_N_ELEMENTS (am_pm_widgets); i++)
    gtk_widget_set_visible (W(am_pm_widgets[i]),
                            priv->clock_format == G_DESKTOP_CLOCK_FORMAT_12H);

  /* Update the minutes label */
  label = g_date_time_format (priv->date, "%M");
  gtk_label_set_text (GTK_LABEL (W("minutes_label")), label);
  g_free (label);
}

static void
set_time_cb (CcDateTimePanel *self,
             GError          *error)
{
  /* TODO: display any error in a user friendly way */
  if (error)
    {
      g_warning ("Could not set system time: %s", error->message);
    }
}

static void
set_timezone_cb (CcDateTimePanel *self,
                 GError          *error)
{
  /* TODO: display any error in a user friendly way */
  if (error)
    {
      g_warning ("Could not set system timezone: %s", error->message);
    }
}

static void
set_using_ntp_cb (CcDateTimePanel *self,
                  GError          *error)
{
  /* TODO: display any error in a user friendly way */
  if (error)
    {
      g_warning ("Could not set system to use NTP: %s", error->message);
    }
}

static void
apply_button_clicked_cb (GtkButton       *button,
                         CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  guint mon, y, d;
  time_t unixtime;
  GDateTime *old_date;
  gboolean using_ntp;

  old_date = priv->date;

  mon = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (W ("month-combobox")));
  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("year-spinbutton")));
  d = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("day-spinbutton")));

  priv->date = g_date_time_new_local (y, mon, d,
                                      g_date_time_get_hour (old_date),
                                      g_date_time_get_minute (old_date),
                                      g_date_time_get_second (old_date));
  g_date_time_unref (old_date);

  unixtime = g_date_time_to_unix (priv->date);

  set_system_time_async (unixtime, (GFunc) set_time_cb, self, NULL);

  if (priv->current_location)
    {
      set_system_timezone_async (priv->current_location->zone, (GFunc) set_timezone_cb, self, NULL);
    }

  using_ntp = gtk_switch_get_active (GTK_SWITCH (W("network_time_switch")));
  set_using_ntp_async (using_ntp, (GFunc) set_using_ntp_cb, self, NULL);
}

static void
location_changed_cb (CcTimezoneMap   *map,
                     TzLocation      *location,
                     CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GtkWidget *widget;
  time_t t;
  struct tm *ltime;
  gchar **split;
  GtkTreeIter iter;
  GtkTreeModel *model;

  priv->current_location = location;

  /* tz.c updates the local timezone, which means the spin buttons can be
   * updated with the current time of the new location */

  t = time (NULL);
  ltime = localtime (&t);

  split = g_strsplit (location->zone, "/", 2);

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
}

static void
get_timezone_cb (CcDateTimePanel *self,
                 const gchar     *timezone,
                 GError          *error)
{
  if (error)
    g_warning ("Could not get current timezone: %s", error->message);
  else
    cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->priv->map), timezone);
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

  zone = g_strdup (loc->zone);
  g_strdelimit (zone, "_", ' ');
  split = g_strsplit (zone, "/", 2);
  g_free (zone);

  /* Load the translation for it */
  zone = g_strdup (dgettext (GETTEXT_PACKAGE_TIMEZONES, loc->zone));
  g_strdelimit (zone, "_", ' ');
  split_translated = g_strsplit_set (zone, TRANSLATION_SPLIT, 2);
  g_free (zone);

  if (!g_hash_table_lookup_extended (data->table, split[0], NULL, NULL))
    {
      g_hash_table_insert (data->table, g_strdup (split[0]),
                           GINT_TO_POINTER (1));
      gtk_list_store_insert_with_values (data->region_store, NULL, 0,
                                         REGION_COL_REGION, split[0],
                                         REGION_COL_REGION_TRANSLATED, split_translated[0], -1);
    }


  gtk_list_store_insert_with_values (data->city_store, NULL, 0,
                                     CITY_COL_CITY, split[1],
                                     CITY_COL_CITY_TRANSLATED, split_translated[1],
                                     CITY_COL_REGION, split[0],
                                     CITY_COL_REGION_TRANSLATED, split_translated[0],
                                     CITY_COL_ZONE, loc->zone,
                                     -1);

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


  combo_model = gtk_combo_box_get_model (combo);
  gtk_combo_box_get_active_iter (combo, &combo_iter);
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
region_changed_cb (GtkComboBox        *box,
                   GtkTreeModelFilter *modelfilter)
{
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
update_widget_state_for_ntp (CcDateTimePanel *panel,
                             gboolean         using_ntp)
{
  CcDateTimePanelPrivate *priv = panel->priv;

  gtk_widget_set_sensitive (W("table1"), !using_ntp);
  gtk_widget_set_sensitive (W("table2"), !using_ntp);
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
}

static void
change_time (GtkButton       *button,
             CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  const gchar *widget_name;
  gint direction;
  GDateTime *old_date;

  old_date = priv->date;

  widget_name = gtk_buildable_get_name (GTK_BUILDABLE (button));

  if (strstr (widget_name, "up"))
    direction = 1;
  else
    direction = -1;

  if (widget_name[0] == 'h')
    {
      priv->date = g_date_time_add_hours (old_date, direction);
    }
  else if (widget_name[0] == 'm')
    {
      priv->date = g_date_time_add_minutes (old_date, direction);
    }
  else
    {
      int hour;
      hour = g_date_time_get_hour (old_date);
      if (hour >= 12)
        priv->date = g_date_time_add_hours (old_date, -12);
      else
        priv->date = g_date_time_add_hours (old_date, 12);
    }
  g_date_time_unref (old_date);

  update_time (panel);
}

static void
change_ntp (GObject    *gobject,
            GParamSpec *pspec,
            gpointer    user_data)
{
  update_widget_state_for_ntp (user_data, gtk_switch_get_active (GTK_SWITCH (gobject)));
}

static void
cc_date_time_panel_init (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv;
  gchar *objects[] = { "datetime-panel", "region-liststore", "city-liststore",
      "month-liststore", "city-modelfilter", "city-modelsort", NULL };
  char *buttons[] = { "hour_up_button", "hour_down_button", "min_up_button",
          "min_down_button", "ampm_up_button", "ampm_down_button" };
  GtkWidget *widget;
  GtkAdjustment *adjustment;
  GError *err = NULL;
  GtkTreeModelFilter *city_modelfilter;
  GtkTreeModelSort *city_modelsort;
  guint i, num_days;
  gboolean using_ntp;
  int ret;

  priv = self->priv = DATE_TIME_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  ret = gtk_builder_add_objects_from_file (priv->builder, DATADIR"/datetime.ui",
                                           objects, &err);

  if (ret == 0)
    {
      g_warning ("Could not load ui: %s", err ? err->message : "No reason");
      if (err)
        g_error_free (err);
      return;
    }

  /* set up network time button */
  using_ntp = get_using_ntp ();
  gtk_switch_set_active (GTK_SWITCH (W("network_time_switch")), using_ntp);
  update_widget_state_for_ntp (self, using_ntp);
  g_signal_connect (W("network_time_switch"), "notify::active",
                    G_CALLBACK (change_ntp), self);

  /* set up time editing widgets */
  for (i = 0; i < G_N_ELEMENTS (buttons); i++)
    {
      g_signal_connect (W(buttons[i]), "clicked",
                        G_CALLBACK (change_time), self);
    }

  /* set up date editing widgets */
  priv->date = g_date_time_new_now_local ();

  gtk_combo_box_set_active (GTK_COMBO_BOX (W ("month-combobox")),
                            g_date_time_get_month (priv->date) - 1);
  g_signal_connect (G_OBJECT (W("month-combobox")), "changed",
                    G_CALLBACK (month_year_changed), self);

  num_days = g_date_get_days_in_month (g_date_time_get_month (priv->date),
                                       g_date_time_get_year (priv->date));
  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_day_of_month (priv->date), 1,
                                                    num_days + 1, 1, 10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (W ("day-spinbutton")),
                                  adjustment);

  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_time_get_year (priv->date),
                                                    G_MINDOUBLE, G_MAXDOUBLE, 1,
                                                    10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (W ("year-spinbutton")),
                                  adjustment);
  g_signal_connect (G_OBJECT (W("year-spinbutton")), "value-changed",
                    G_CALLBACK (month_year_changed), self);

  /* set up timezone map */
  priv->map = widget = (GtkWidget *) cc_timezone_map_new ();
  g_signal_connect (widget, "location-changed",
                    G_CALLBACK (location_changed_cb), self);
  gtk_widget_show (widget);

  gtk_container_add (GTK_CONTAINER (gtk_builder_get_object (priv->builder,
                                                            "aspectmap")),
                     widget);

  gtk_container_add (GTK_CONTAINER (self),
                     GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                         "datetime-panel")));


  /* setup the time itself */
  priv->settings = g_settings_new (CLOCK_SCHEMA);
  clock_settings_changed_cb (priv->settings, CLOCK_FORMAT_KEY, self);
  g_signal_connect (priv->settings, "changed::" CLOCK_FORMAT_KEY,
                    G_CALLBACK (clock_settings_changed_cb), self);

  g_signal_connect (W ("12_radiobutton"), "toggled",
                    G_CALLBACK (change_clock_settings), self);
  g_signal_connect (W ("24_radiobutton"), "toggled",
                    G_CALLBACK (change_clock_settings), self);

  update_time (self);

  g_signal_connect ((GtkWidget*) gtk_builder_get_object (priv->builder,
                                                         "button_apply"),
                    "clicked",
                    G_CALLBACK (apply_button_clicked_cb),
                    self);

  get_system_timezone_async ((GetTimezoneFunc) get_timezone_cb, self, NULL);

  priv->locations = (GtkTreeModel*) gtk_builder_get_object (priv->builder,
                                                            "region-liststore");

  load_regions_model (GTK_LIST_STORE (priv->locations),
                      GTK_LIST_STORE (gtk_builder_get_object (priv->builder,
                                                              "city-liststore")));

  city_modelfilter = GTK_TREE_MODEL_FILTER (gtk_builder_get_object (priv->builder, "city-modelfilter"));

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "region_combobox");
  g_signal_connect (widget, "changed", G_CALLBACK (region_changed_cb),
                    city_modelfilter);

  city_modelsort = GTK_TREE_MODEL_SORT (gtk_builder_get_object (priv->builder, "city-modelsort"));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (city_modelsort), CITY_COL_CITY_TRANSLATED,
                                        GTK_SORT_ASCENDING);

  gtk_tree_model_filter_set_visible_func (city_modelfilter,
                                          (GtkTreeModelFilterVisibleFunc) city_model_filter_func,
                                          widget,
                                          NULL);
  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "city_combobox");
  g_signal_connect (widget, "changed", G_CALLBACK (city_changed_cb), self);
}

void
cc_date_time_panel_register (GIOModule *module)
{
  cc_date_time_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_DATE_TIME_PANEL,
                                  "datetime", 0);
}

