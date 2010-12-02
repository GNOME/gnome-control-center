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

G_DEFINE_DYNAMIC_TYPE (CcDateTimePanel, cc_date_time_panel, CC_TYPE_PANEL)

#define DATE_TIME_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DATE_TIME_PANEL, CcDateTimePanelPrivate))

enum {
  CITY_COL_CITY,
  CITY_COL_REGION,
  CITY_COL_ZONE,
  CITY_NUM_COLS
};

#define W(x) (GtkWidget*) gtk_builder_get_object (priv->builder, x)

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

struct _CcDateTimePanelPrivate
{
  GtkBuilder *builder;
  GtkWidget *map;

  TzLocation *current_location;

  guint timeout;

  GtkTreeModel *locations;
  GtkTreeModelFilter *city_filter;

  guint hour;
  guint minute;

  GSettings *settings;
};


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

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->dispose (object);
}

static void
cc_date_time_panel_finalize (GObject *object)
{
  CcDateTimePanelPrivate *priv = CC_DATE_TIME_PANEL (object)->priv;

  if (priv->timeout)
    {
      g_source_remove (priv->timeout);
      priv->timeout = 0;
    }

  G_OBJECT_CLASS (cc_date_time_panel_parent_class)->finalize (object);
}

static void
cc_date_time_panel_class_init (CcDateTimePanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcDateTimePanelPrivate));

  object_class->get_property = cc_date_time_panel_get_property;
  object_class->set_property = cc_date_time_panel_set_property;
  object_class->dispose = cc_date_time_panel_dispose;
  object_class->finalize = cc_date_time_panel_finalize;
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

  g_signal_handlers_block_by_func (priv->settings, clock_settings_changed_cb,
                                   panel);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (W ("12_radiobutton"))))
    g_settings_set_enum (priv->settings, CLOCK_FORMAT_KEY, G_DESKTOP_CLOCK_FORMAT_12H);
  else
    g_settings_set_enum (priv->settings, CLOCK_FORMAT_KEY, G_DESKTOP_CLOCK_FORMAT_24H);

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

  radio12 = W ("12_radiobutton");
  radio24 = W ("24_radiobutton");

  use_12_hour = (value == G_DESKTOP_CLOCK_FORMAT_12H);

  g_signal_handlers_block_by_func (radio12, change_clock_settings, panel);
  g_signal_handlers_block_by_func (radio24, change_clock_settings, panel);


  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio12), use_12_hour);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radio24), !use_12_hour);

  g_signal_handlers_unblock_by_func (radio12, change_clock_settings, panel);
  g_signal_handlers_unblock_by_func (radio24, change_clock_settings, panel);
}

static gboolean
update_time (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GTimeVal timeval;
  GtkWidget *widget;
  gchar label[32];
  time_t t;
  struct tm time_info;

  g_get_current_time (&timeval);

  t = time (NULL);

  localtime_r (&t, &time_info);

  priv->hour = time_info.tm_hour;
  priv->minute = time_info.tm_min;

  /* Update the hours label */
  strftime (label, 32, "%H", &time_info);
  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "hours_label");
  gtk_label_set_text (GTK_LABEL (widget), label);

  /* Update the minutes label */
  strftime (label, 32, "%M", &time_info);
  widget = (GtkWidget*) gtk_builder_get_object (priv->builder, "minutes_label");
  gtk_label_set_text (GTK_LABEL (widget), label);

  priv->settings = g_settings_new (CLOCK_SCHEMA);
  clock_settings_changed_cb (priv->settings, CLOCK_FORMAT_KEY, self);
  g_signal_connect (priv->settings, "changed::" CLOCK_FORMAT_KEY,
                    G_CALLBACK (clock_settings_changed_cb), self);

  g_signal_connect (W ("12_radiobutton"), "toggled",
                    G_CALLBACK (change_clock_settings), self);
  g_signal_connect (W ("24_radiobutton"), "toggled",
                    G_CALLBACK (change_clock_settings), self);

  return FALSE;
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
apply_button_clicked_cb (GtkButton       *button,
                         CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  guint mon, y, d;
  struct tm fulltime;
  time_t unixtime;
  gchar *filename;

  mon = 1 + gtk_combo_box_get_active (GTK_COMBO_BOX (W ("month-combobox")));

  y = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("year-spinbutton")));
  d = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (W ("day-spinbutton")));



  fulltime.tm_sec = 0;
  fulltime.tm_min = priv->minute;
  fulltime.tm_hour = priv->hour;
  fulltime.tm_mday = d;
  fulltime.tm_mon = mon;
  fulltime.tm_year = y - 1900;
  fulltime.tm_isdst = -1;


  unixtime = mktime (&fulltime);

  set_system_time_async (unixtime, (GFunc) set_time_cb, self, NULL);

  if (priv->current_location)
    {
      filename = g_build_filename (SYSTEM_ZONEINFODIR,
                                   priv->current_location->zone,
                                   NULL);
      set_system_timezone_async (filename, (GFunc) set_timezone_cb, self, NULL);
    }
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

      gtk_tree_model_get (model, &iter, 0, &string, -1);

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

      gtk_tree_model_get (model, &iter, 0, &string, -1);

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

static void
get_regions (TzLocation             *loc,
             struct get_region_data *data)
{
  gchar **split;

  split = g_strsplit (loc->zone, "/", 2);

  /* remove underscores */
  g_strdelimit (split[1], "_", ' ');

  if (!g_hash_table_lookup_extended (data->table, split[0], NULL, NULL))
    {
      g_hash_table_insert (data->table, g_strdup (split[0]),
                           GINT_TO_POINTER (1));
      gtk_list_store_insert_with_values (data->region_store, NULL, 0, 0,
                                         split[0], -1);
    }

  gtk_list_store_insert_with_values (data->city_store, NULL, 0,
                                     CITY_COL_CITY, split[1],
                                     CITY_COL_REGION, split[0],
                                     CITY_COL_ZONE, loc->zone,
                                     -1);

  g_strfreev (split);
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
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (regions), 0,
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
change_time (GtkButton       *button,
             CcDateTimePanel *panel)
{
  CcDateTimePanelPrivate *priv = panel->priv;
  const gchar *widget_name;
  gchar *new_str;
  guint *value, max, min;
  GtkWidget *label;

  widget_name = gtk_buildable_get_name (GTK_BUILDABLE (button));

  min = 0;
  if (widget_name[0] == 'h')
    {
      /* change hour */
      label = W ("hours_label");
      max = 23;
      value = &priv->hour;
    }
  else
    {
      /* change minute */
      label = W ("minutes_label");
      max = 59;
      value = &priv->minute;
    }

  if (strstr (widget_name, "up"))
    *value = *value + 1;
  else
    *value = *value - 1;

  if (*value > max)
    *value = min;
  else if (*value < min)
    *value = max;

  new_str = g_strdup_printf ("%02d", *value);
  gtk_label_set_text (GTK_LABEL (label), new_str);
  g_free (new_str);
}

static void
cc_date_time_panel_init (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv;
  gchar *objects[] = { "datetime-panel", "region-liststore", "city-liststore",
      "month-liststore", "city-modelfilter", "city-modelsort", NULL };
  GtkWidget *widget;
  GtkAdjustment *adjustment;
  GError *err = NULL;
  GDate *date;
  GtkTreeModelFilter *city_modelfilter;
  GtkTreeModelSort *city_modelsort;
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

  /* set up time editing widgets */
  g_signal_connect (W("hour_up_button"), "clicked", G_CALLBACK (change_time),
                    self);
  g_signal_connect (W("hour_down_button"), "clicked", G_CALLBACK (change_time),
                    self);
  g_signal_connect (W("min_up_button"), "clicked", G_CALLBACK (change_time),
                    self);
  g_signal_connect (W("min_down_button"), "clicked", G_CALLBACK (change_time),
                    self);

  /* set up date editing widgets */
  date = g_date_new ();
  g_date_set_time_t (date, time (NULL));

  gtk_combo_box_set_active (GTK_COMBO_BOX (W ("month-combobox")),
                            g_date_get_month (date) - 1);

  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_get_day (date), 0,
                                                    31, 1, 10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (W ("day-spinbutton")),
                                  adjustment);

  adjustment = (GtkAdjustment*) gtk_adjustment_new (g_date_get_year (date),
                                                    G_MINDOUBLE, G_MAXDOUBLE, 1,
                                                    10, 1);
  gtk_spin_button_set_adjustment (GTK_SPIN_BUTTON (W ("year-spinbutton")),
                                  adjustment);
  g_date_free (date);
  date = NULL;

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
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (city_modelsort), 0,
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

