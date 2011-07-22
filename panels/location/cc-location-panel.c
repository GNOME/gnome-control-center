/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
 */

/* TODO:
   - use geocode-glib to ask for locations ;
   - use an indicator for the home location (add a pixbuf column) ;
   - fallback to libgweather locations.xml when no Internet available ;
   - draw pretty Cairos shapes to spot locations on the map (otherwise the map is useless) ;
   - think about how to handle timezone change (are they just an hour offset?) ;
   - think about how location change affect manual timing ;
   - who takes care of daylight saving and summer time and all the fun stuff? ;
   - think about where does my panel fit (seperate panel? replace date & time?) ;
   - define a nice schema for GSettings ;
*/

#include "cc-location-panel.h"
#include "location-utils.h"
#include <gdesktop-enums.h>

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

G_DEFINE_DYNAMIC_TYPE (CcLocationPanel, cc_location_panel, CC_TYPE_PANEL)

#define LOCATION_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_LOCATION_PANEL, CcLocationPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

struct _CcLocationPanelPrivate
{
  GtkBuilder *builder;
  GSettings  *settings;
  GSettings  *location_settings;
  GVariant   *locations;
  GDesktopClockFormat clock_format;

  GtkListStore *location_store;
  /* that's where private vars go I guess */
};

static void
populate_locations (GtkListStore *store,
                    GVariant     *locations)
{
  GtkTreeIter iter;
  GVariantIter viter;
  GVariant *dict;
  GVariant *value;

  g_variant_iter_init (&viter, locations);
  while ((dict = g_variant_iter_next_value (&viter))) {
    gtk_list_store_append (store, &iter);
    value = g_variant_lookup_value (dict,
                                    "city",
                                    G_VARIANT_TYPE_STRING);
    gtk_list_store_set (store, &iter, 0, g_variant_get_string (value, NULL), -1);

    value = g_variant_lookup_value (dict,
                                    "country",
                                    G_VARIANT_TYPE_STRING);
    gtk_list_store_set (store, &iter, 1, g_variant_get_string (value, NULL), -1);

    value = g_variant_lookup_value (dict,
                                    "timezone",
                                    G_VARIANT_TYPE_INT16);
    int timezone = g_variant_get_int16 (value);
    char *tz;
    if (timezone > 0)
      tz = g_strdup_printf ("GMT +%i", timezone);
    else
      tz = g_strdup_printf ("GMT -%i", timezone);
    gtk_list_store_set (store, &iter, 2, tz, -1);
    g_free (tz);

    gtk_list_store_set (store, &iter, 3, "20:00", -1);

    /*value = g_variant_lookup_value (dict,
                                    "longitude",
                                    G_VARIANT_TYPE_DOUBLE);
    gtk_list_store_set (store, &iter, 1, g_variant_get_double (value), -1);

    value = g_variant_lookup_value (dict,
                                    "latitude",
                                    G_VARIANT_TYPE_DOUBLE);
    gtk_list_store_set (store, &iter, 1, g_variant_get_double (value), -1);
    */
  }
}

static void
_on_add_location (GtkToolButton   *bt,
                  CcLocationPanel *self)
{
  CcLocationPanelPrivate *priv = self->priv;
  GError *e = NULL;

  gtk_builder_add_from_file (self->priv->builder, GNOMECC_UI_DIR "/add-location.ui", &e);
  g_assert_no_error (e);

  GtkWidget *dialog = WID ("add-location-dialog");
  int res = gtk_dialog_run (GTK_DIALOG (dialog));

  switch (res) {
    case GTK_RESPONSE_OK:
      printf ("Dialog response OK\n");
      // FIXME: column ids
      const char *city = gtk_entry_get_text (GTK_ENTRY (WID ("city-entry")));
      const char *ctry = gtk_entry_get_text (GTK_ENTRY (WID ("country-entry")));
      //const char *tz = gtk_entry_get_text (GTK_ENTRY (WID ("tz-entry")));
      const double longitude = 0.23;
      const double latitude = 0.32;
      GVariant *newloc = g_variant_location_new (city, ctry,
                                                 3, longitude, latitude);
      priv->locations = g_variant_array_add_value (priv->locations, newloc);
      g_assert (g_settings_set_value (priv->location_settings,
                                      "locations",
                                      priv->locations));

      gtk_list_store_clear (priv->location_store);
      populate_locations (priv->location_store,
                          priv->locations);
      break;
    default:
      break;
    }
  gtk_widget_destroy (dialog);
}

static void
_on_24hr_time_switch (GObject         *gobject,
                      GParamSpec      *pspec,
                      CcLocationPanel *self)
{
  CcLocationPanelPrivate *priv = self->priv;
  GDesktopClockFormat value;

  //g_signal_handlers_block_by_func (priv->settings, clock_settings_changed_cb,
  //                                 panel);

  if (gtk_switch_get_active (GTK_SWITCH (WID ("24h_time_switch"))))
    value = G_DESKTOP_CLOCK_FORMAT_24H;
  else
    value = G_DESKTOP_CLOCK_FORMAT_12H;

  g_settings_set_enum (priv->settings, CLOCK_FORMAT_KEY, value);
  priv->clock_format = value;

  /* update_time (panel); */

  /* g_signal_handlers_unblock_by_func (priv->settings, clock_settings_changed_cb, */
  /*                                    panel); */
}

static void
cc_location_panel_get_property (GObject    *object,
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
cc_location_panel_set_property (GObject      *object,
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
cc_location_panel_dispose (GObject *object)
{
  CcLocationPanelPrivate *priv = CC_LOCATION_PANEL (object)->priv;
  G_OBJECT_CLASS (cc_location_panel_parent_class)->dispose (object);

  if (priv->settings) {
    g_object_unref (priv->settings);
    priv->settings = NULL;
  }
}

static void
cc_location_panel_finalize (GObject *object)
{
  CcLocationPanelPrivate *priv = CC_LOCATION_PANEL (object)->priv;
  G_OBJECT_CLASS (cc_location_panel_parent_class)->finalize (object);
}

static void
cc_location_panel_class_init (CcLocationPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcLocationPanelPrivate));

  object_class->get_property = cc_location_panel_get_property;
  object_class->set_property = cc_location_panel_set_property;
  object_class->dispose = cc_location_panel_dispose;
  object_class->finalize = cc_location_panel_finalize;
}

static void
cc_location_panel_class_finalize (CcLocationPanelClass *klass)
{
}

static void
cc_location_panel_init (CcLocationPanel *self)
{
  GError     *error;
  GtkWidget  *widget;
  GtkStyleContext *context;
  self->priv = LOCATION_PANEL_PRIVATE (self);

  self->priv->settings = g_settings_new (CLOCK_SCHEMA);
  self->priv->builder  = gtk_builder_new ();
  self->priv->location_settings = g_settings_new ("org.gnome.desktop.location");

  error = NULL;
  gtk_builder_add_from_file (self->priv->builder,
                             GNOMECC_UI_DIR "/location.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  gtk_builder_connect_signals (self->priv->builder, NULL);

  self->priv->location_store = (GtkListStore *) gtk_builder_get_object (self->priv->builder,
                                                                        "locations-store");
  widget = WID ("locations-scrolledwindow");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

  widget = WID ("location-edit-toolbar");
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_add_class (context, "inline-toolbar");
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  widget = WID ("24h_time_switch");
  gtk_switch_set_active (GTK_SWITCH (widget),
                         g_settings_get_enum (self->priv->settings, CLOCK_FORMAT_KEY) ==
                         G_DESKTOP_CLOCK_FORMAT_24H);
  g_signal_connect (widget, "notify::active",
                    G_CALLBACK (_on_24hr_time_switch), self);

  widget = WID ("location-add-button");
  g_signal_connect (widget, "clicked",
                    G_CALLBACK (_on_add_location), self);

  widget = WID ("location-vbox");
  gtk_widget_reparent (widget, (GtkWidget *) self);

  self->priv->locations = g_settings_get_value (self->priv->location_settings,
                                                "locations");
  populate_locations (self->priv->location_store, self->priv->locations);
}

void
cc_location_panel_register (GIOModule *module)
{
  cc_location_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_LOCATION_PANEL,
                                  "location", 0);
}
