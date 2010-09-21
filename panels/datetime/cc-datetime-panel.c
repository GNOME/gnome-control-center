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

G_DEFINE_DYNAMIC_TYPE (CcDateTimePanel, cc_date_time_panel, CC_TYPE_PANEL)

#define DATE_TIME_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_DATE_TIME_PANEL, CcDateTimePanelPrivate))

struct _CcDateTimePanelPrivate
{
  GtkBuilder *builder;
  GtkWidget *map;

  TzLocation *current_location;

  guint timeout;

  GtkTreeModel *locations;
  GtkTreeModelFilter *city_filter;
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

static gboolean
update_time (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv = self->priv;
  GTimeVal timeval;
  GtkWidget *widget;
  gchar label[32];
  time_t t;

  g_get_current_time (&timeval);

  priv->timeout = gdk_threads_add_timeout (1000 - timeval.tv_usec / 1000,
                                           (GSourceFunc) update_time, self);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "label_current_time");
  t = time (NULL);
  strftime (label, 32, "%X", localtime (&t));
  gtk_label_set_text (GTK_LABEL (widget), label);

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
  GtkWidget *widget;
  CcDateTimePanelPrivate *priv = self->priv;
  guint h, mon, y, min, d;
  struct tm fulltime;
  time_t unixtime;
  gchar *filename;

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "spin_hour");
  h = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "spin_minute");
  min = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "calendar");
  gtk_calendar_get_date (GTK_CALENDAR (widget), &y, &mon, &d);

  fulltime.tm_sec = 0;
  fulltime.tm_min = min;
  fulltime.tm_hour = h;
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
  gchar slabel[32];
  gchar **split;
  GtkTreeIter iter;
  GtkTreeModel *model;

  priv->current_location = location;

  /* tz.c updates the local timezone, which means the spin buttons can be
   * updated with the current time of the new location */

  t = time (NULL);
  ltime = localtime (&t);

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "spin_hour");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), ltime->tm_hour);
  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "spin_minute");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), ltime->tm_min);

  widget = (GtkWidget*) gtk_builder_get_object (priv->builder,
                                                "label_current_time");
  strftime (slabel, 32, "%X", localtime (&t));
  gtk_label_set_text (GTK_LABEL (widget), slabel);

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
                                     0, split[1],
                                     1, split[0],
                                     2, loc->zone,
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
                      0, &active_region, -1);

  gtk_tree_model_get (model, iter,
                      1, &city_region, -1);

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
  TzDB *db = tz_load_db ();
  GHashTable *table;


  table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  data.table = table;
  data.region_store = regions;
  data.city_store = cities;

  g_ptr_array_foreach (db->locations, (GFunc) get_regions, &data);

  g_hash_table_destroy (table);

  /* sort the models */
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (regions), 0,
                                        GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cities), 0,
                                        GTK_SORT_ASCENDING);

  tz_db_free (db);
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
                          2, &zone, -1);

      cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (self->priv->map), zone);

      g_free (zone);
    }

  inside = FALSE;
}

static void
cc_date_time_panel_init (CcDateTimePanel *self)
{
  CcDateTimePanelPrivate *priv;
  gchar *objects[] = { "datetime-panel", "adjustment_min", "adjustment_hour",
      "adjustment_sec", "region-liststore", "city-liststore",
      "city-modelfilter", NULL };
  GtkWidget *widget;
  GError *err = NULL;
  GDate *date;
  struct tm *ltime;
  time_t t;
  GtkTreeModelFilter *city_modelfilter;
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

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "calendar");
  date = g_date_new ();
  g_date_set_time_t (date, time (NULL));
  gtk_calendar_select_day (GTK_CALENDAR (widget), g_date_get_day (date));
  gtk_calendar_select_month (GTK_CALENDAR (widget), g_date_get_month (date) -1,
                             g_date_get_year (date));
  g_date_free (date);
  date = NULL;

  update_time (self);

  t = time (NULL);
  ltime = localtime (&t);

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "spin_hour");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), ltime->tm_hour);
  widget = (GtkWidget *) gtk_builder_get_object (priv->builder, "spin_minute");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), ltime->tm_min);

  g_signal_connect ((GtkWidget*) gtk_builder_get_object (priv->builder, "button_apply"),
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

