/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include <config.h>

#include <libupower-glib/upower.h>
#include <glib/gi18n.h>
#include <gnome-settings-daemon/gsd-enums.h>

#include "cc-power-panel.h"
#include "cc-strength-bar.h"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

G_DEFINE_DYNAMIC_TYPE (CcPowerPanel, cc_power_panel, CC_TYPE_PANEL)

#define POWER_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_POWER_PANEL, CcPowerPanelPrivate))

struct _CcPowerPanelPrivate
{
  GSettings     *lock_settings;
  GSettings     *gsd_settings;
  GCancellable  *cancellable;
  GtkBuilder    *builder;
  GDBusProxy    *proxy;
  UpClient      *up_client;
  CcStrengthBar *progressbar_primary;
};

enum
{
  ACTION_MODEL_TEXT,
  ACTION_MODEL_VALUE,
  ACTION_MODEL_SENSITIVE
};

static void
cc_power_panel_get_property (GObject    *object,
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
cc_power_panel_set_property (GObject      *object,
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
cc_power_panel_dispose (GObject *object)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (object)->priv;

  if (priv->gsd_settings)
    {
      g_object_unref (priv->gsd_settings);
      priv->gsd_settings = NULL;
    }
  if (priv->cancellable != NULL)
    {
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }
  if (priv->builder != NULL)
    {
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }
  if (priv->proxy != NULL)
    {
      g_object_unref (priv->proxy);
      priv->proxy = NULL;
    }
  if (priv->up_client != NULL)
    {
      g_object_unref (priv->up_client);
      priv->up_client = NULL;
    }

  G_OBJECT_CLASS (cc_power_panel_parent_class)->dispose (object);
}

static void
cc_power_panel_finalize (GObject *object)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (object)->priv;
  g_cancellable_cancel (priv->cancellable);
  G_OBJECT_CLASS (cc_power_panel_parent_class)->finalize (object);
}

static void
on_lock_settings_changed (GSettings     *settings,
                          const char    *key,
                          CcPowerPanel *panel)
{
}

static void
cc_power_panel_class_init (CcPowerPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcPowerPanelPrivate));

  object_class->get_property = cc_power_panel_get_property;
  object_class->set_property = cc_power_panel_set_property;
  object_class->dispose = cc_power_panel_dispose;
  object_class->finalize = cc_power_panel_finalize;
}

static void
cc_power_panel_class_finalize (CcPowerPanelClass *klass)
{
}

static gchar *
get_timestring (guint64 time_secs)
{
  gchar* timestring = NULL;
  gint  hours;
  gint  minutes;

  /* Add 0.5 to do rounding */
  minutes = (int) ( ( time_secs / 60.0 ) + 0.5 );

  if (minutes == 0)
    {
      timestring = g_strdup (_("Unknown time"));
      return timestring;
    }

  if (minutes < 60)
    {
      timestring = g_strdup_printf (ngettext ("%i minute",
                                    "%i minutes",
                                    minutes), minutes);
      return timestring;
    }

  hours = minutes / 60;
  minutes = minutes % 60;

  if (minutes == 0)
    {
      timestring = g_strdup_printf (ngettext (
                                    "%i hour",
                                    "%i hours",
                                    hours), hours);
      return timestring;
    }

  /* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
   * Swap order with "%2$s %2$i %1$s %1$i if needed */
  timestring = g_strdup_printf (_("%i %s %i %s"),
                                hours, ngettext ("hour", "hours", hours),
                                minutes, ngettext ("minute", "minutes", minutes));
  return timestring;
}

static void
set_device_battery_primary (CcPowerPanel *panel, GVariant *device)
{
  CcPowerPanelPrivate *priv = panel->priv;
  gchar *details = NULL;
  gchar *time_string = NULL;
  gdouble percentage;
  GtkWidget *widget;
  guint64 time;
  UpDeviceState state;

  /* set the device */
  g_variant_get (device,
                 "(susdut)",
                 NULL, /* object_path */
                 NULL, /* kind */
                 NULL, /* icon_name */
                 &percentage,
                 &state,
                 &time);

  /* set the percentage */
  cc_strength_bar_set_fraction (priv->progressbar_primary,
                                percentage / 100.0f);

  /* clear the warning */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "image_primary_warning"));
  gtk_widget_hide (widget);

  /* set the description */
  if (time > 0)
    {
      time_string = get_timestring (time);
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
            details = g_strdup_printf(_("Charging - %s until fully charged"),
                                      time_string);
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            if (percentage < 20)
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf(_("Caution low battery, %s remaining"),
                                          time_string);
                /* show the warning */
                gtk_widget_show (widget);
              }
            else
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf(_("Using battery power - %s remaining"),
                                          time_string);
              }
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }
  else
    {
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup(_("Charging"));
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup(_("Using battery power"));
            break;
          case UP_DEVICE_STATE_FULLY_CHARGED:
            /* TRANSLATORS: primary battery */
            details = g_strdup(_("Charging - fully charged"));
            break;
          case UP_DEVICE_STATE_EMPTY:
            /* TRANSLATORS: primary battery */
            details = g_strdup(_("Empty"));
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }
  if (details == NULL)
    goto out;
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_battery_primary"));
  gtk_label_set_label (GTK_LABEL (widget), details);

  /* show the primary device */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_primary"));
  gtk_widget_show (widget);

  /* hide the addon device until we stumble upon the device */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_battery_addon"));
  gtk_widget_hide (widget);
out:
  g_free (time_string);
  g_free (details);
}

static void
set_device_ups_primary (CcPowerPanel *panel, GVariant *device)
{
  CcPowerPanelPrivate *priv = panel->priv;
  gchar *details = NULL;
  gchar *time_string = NULL;
  gdouble percentage;
  GtkWidget *widget;
  guint64 time;
  UpDeviceState state;

  /* set the device */
  g_variant_get (device,
                 "(susdut)",
                 NULL, /* object_path */
                 NULL, /* kind */
                 NULL, /* icon_name */
                 &percentage,
                 &state,
                 &time);

  /* set the percentage */
  cc_strength_bar_set_fraction (priv->progressbar_primary,
                                percentage / 100.0f);

  /* always show the warning */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "image_primary_warning"));
  gtk_widget_show (widget);

  /* set the description */
  if (time > 0)
    {
      time_string = get_timestring (time);
      switch (state)
        {
          case UP_DEVICE_STATE_DISCHARGING:
            if (percentage < 20)
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf(_("Caution low UPS, %s remaining"),
                                          time_string);
              }
            else
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf(_("Using UPS power - %s remaining"),
                                          time_string);
              }
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }
  else
    {
      switch (state)
        {
          case UP_DEVICE_STATE_DISCHARGING:
            if (percentage < 20)
              {
                /* TRANSLATORS: UPS battery */
                details = g_strdup(_("Caution low UPS"));
              }
            else
              {
                /* TRANSLATORS: UPS battery */
                details = g_strdup(_("Using UPS power"));
              }
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }
  if (details == NULL)
    goto out;
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_battery_primary"));
  gtk_label_set_label (GTK_LABEL (widget), details);

  /* show the primary device */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_primary"));
  gtk_widget_show (widget);

  /* hide the addon device as extra UPS devices are not possible */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_battery_addon"));
  gtk_widget_hide (widget);
out:
  g_free (time_string);
  g_free (details);
}

static void
set_device_battery_additional (CcPowerPanel *panel, GVariant *device)
{
  CcPowerPanelPrivate *priv = panel->priv;
  gchar *details = NULL;
  GtkWidget *widget;
  UpDeviceState state;

  /* set the device */
  g_variant_get (device,
                 "(susdut)",
                 NULL, /* object_path */
                 NULL, /* kind */
                 NULL, /* icon_name */
                 NULL, /* percentage */
                 &state,
                 NULL /* time */);

  /* set the description */
  switch (state)
    {
      case UP_DEVICE_STATE_FULLY_CHARGED:
        /* TRANSLATORS: secondary battery is normally in the media bay */
        details = g_strdup(_("Your secondary battery is fully charged"));
        break;
      case UP_DEVICE_STATE_EMPTY:
        /* TRANSLATORS: secondary battery is normally in the media bay */
        details = g_strdup(_("Your secondary battery is empty"));
        break;
      default:
        break;
    }
  if (details == NULL)
    goto out;
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "label_battery_addon"));
  gtk_label_set_label (GTK_LABEL (widget), details);

  /* show the addon device */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_battery_addon"));
  gtk_widget_show (widget);
out:
  g_free (details);
}

static void
add_device_secondary (CcPowerPanel *panel,
                      GVariant *device,
                      guint *secondary_devices_cnt)
{
  CcPowerPanelPrivate *priv = panel->priv;
  const gchar *icon_name = NULL;
  gdouble percentage;
  guint64 time;
  UpDeviceKind kind;
  UpDeviceState state;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *widget;
  GString *status;
  GString *description;
  gboolean show_caution = FALSE;

  g_variant_get (device,
                 "(susdut)",
                 NULL,
                 &kind,
                 NULL,
                 &percentage,
                 &state,
                 &time);

  switch (kind)
    {
      case UP_DEVICE_KIND_UPS:
        icon_name = "uninterruptible-power-supply";
        show_caution = TRUE;
        break;
      case UP_DEVICE_KIND_MOUSE:
        icon_name = "input-mouse";
        break;
      case UP_DEVICE_KIND_KEYBOARD:
        icon_name = "input-keyboard";
        break;
      case UP_DEVICE_KIND_TABLET:
        icon_name = "input-tablet";
        break;
      case UP_DEVICE_KIND_PDA:
        icon_name = "pda";
        break;
      case UP_DEVICE_KIND_PHONE:
        icon_name = "phone";
        break;
      case UP_DEVICE_KIND_MEDIA_PLAYER:
        icon_name = "multimedia-player";
        break;
      case UP_DEVICE_KIND_COMPUTER:
        icon_name = "computer";
        show_caution = TRUE;
        break;
      default:
        icon_name = "battery";
        break;
    }

  switch (kind)
    {
      case UP_DEVICE_KIND_MOUSE:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Wireless mouse"));
        break;
      case UP_DEVICE_KIND_KEYBOARD:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Wireless keyboard"));
        break;
      case UP_DEVICE_KIND_UPS:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Uninterruptible power supply"));
        break;
      case UP_DEVICE_KIND_PDA:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Personal digital assistant"));
        break;
      case UP_DEVICE_KIND_PHONE:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Cellphone"));
        break;
      case UP_DEVICE_KIND_MEDIA_PLAYER:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Media player"));
        break;
      case UP_DEVICE_KIND_TABLET:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Tablet"));
        break;
      case UP_DEVICE_KIND_COMPUTER:
        /* TRANSLATORS: secondary battery */
        description = g_string_new (_("Computer"));
        break;
      default:
        /* TRANSLATORS: secondary battery, misc */
        description = g_string_new (_("Battery"));
        break;
    }
  g_string_prepend (description, "<b>");
  g_string_append (description, "</b>");

  switch (state)
    {
      case UP_DEVICE_STATE_CHARGING:
      case UP_DEVICE_STATE_PENDING_CHARGE:
        /* TRANSLATORS: secondary battery */
        status = g_string_new(_("Charging"));
        break;
      case UP_DEVICE_STATE_DISCHARGING:
      case UP_DEVICE_STATE_PENDING_DISCHARGE:
        if (percentage < 10 && show_caution)
          {
            /* TRANSLATORS: secondary battery */
            status = g_string_new (_("Caution"));
          }
        else if (percentage < 30)
          {
            /* TRANSLATORS: secondary battery */
            status = g_string_new (_("Low"));
          }
        else
          {
            /* TRANSLATORS: secondary battery */
            status = g_string_new (_("Good"));
          }
        break;
      case UP_DEVICE_STATE_FULLY_CHARGED:
        /* TRANSLATORS: primary battery */
        status = g_string_new(_("Charging - fully charged"));
        break;
      case UP_DEVICE_STATE_EMPTY:
        /* TRANSLATORS: primary battery */
        status = g_string_new(_("Empty"));
        break;
      default:
        status = g_string_new (up_device_state_to_string (state));
        break;
    }
  g_string_prepend (status, "<small>");
  g_string_append (status, "</small>");

  /* create the new widget */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_hexpand (hbox, TRUE);
  widget = gtk_image_new ();
  gtk_misc_set_alignment (GTK_MISC (widget), 0.5f, 0.0f);
  gtk_image_set_from_icon_name (GTK_IMAGE (widget), icon_name, GTK_ICON_SIZE_DND);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  widget = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (widget), 0.0f, 0.5f);
  gtk_label_set_markup (GTK_LABEL (widget), description->str);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  widget = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (widget), 0.0f, 0.5f);
  gtk_label_set_markup (GTK_LABEL (widget), status->str);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
  widget = cc_strength_bar_new ();
  gtk_widget_set_margin_right (widget, 32);
  gtk_widget_set_margin_top (widget, 3);
  cc_strength_bar_set_fraction (CC_STRENGTH_BAR (widget), percentage / 100.0f);
  gtk_box_pack_start (GTK_BOX (vbox), widget, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

  /* add to the grid */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "grid_secondary"));

  /* two devices wide */
  gtk_grid_attach (GTK_GRID (widget), hbox,
                   *secondary_devices_cnt % 2,
                   (*secondary_devices_cnt / 2) - 1,
                   1, 1);
  (*secondary_devices_cnt)++;

  /* show panel */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_secondary"));
  gtk_widget_show_all (widget);

  g_string_free (description, TRUE);
  g_string_free (status, TRUE);
}

static void
get_devices_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcPowerPanel *panel = CC_POWER_PANEL (user_data);
  CcPowerPanelPrivate *priv = panel->priv;
  gboolean got_primary = FALSE;
  gboolean ups_as_primary_device = FALSE;
  GError *error = NULL;
  gsize n_devices;
  GList *children;
  GList *l;
  GtkWidget *widget;
  guint i;
  guint secondary_devices_cnt = 0;
  GVariant *child;
  GVariant *result;
  GVariant *untuple;
  UpDeviceKind kind;
  UpDeviceState state;

  /* empty the secondary box */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "grid_secondary"));
  children = gtk_container_get_children (GTK_CONTAINER (widget));
  for (l = children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (widget), l->data);
  g_list_free (children);

  /* hide both panels initially */
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_primary"));
  gtk_widget_hide (widget);
  widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                               "box_secondary"));
  gtk_widget_hide (widget);

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (result == NULL)
    {
      g_printerr ("Error getting devices: %s\n", error->message);
      g_error_free (error);
      return;
    }

  untuple = g_variant_get_child_value (result, 0);
  n_devices = g_variant_n_children (untuple);

  /* first we look for a discharging UPS, which is promoted to the
   * primary device if it's discharging. Otherwise we use the first
   * listed laptop battery as the primary device */
  for (i = 0; i < n_devices; i++)
    {
      child = g_variant_get_child_value (untuple, i);
      g_variant_get (child,
                     "(susdut)",
                     NULL,
                     &kind,
                     NULL,
                     NULL,
                     &state,
                     NULL);
      if (kind == UP_DEVICE_KIND_UPS &&
          state == UP_DEVICE_STATE_DISCHARGING)
        {
          ups_as_primary_device = TRUE;
        }
      g_variant_unref (child);
    }

  /* add the devices now we know the state-of-play */
  for (i = 0; i < n_devices; i++)
    {
      child = g_variant_get_child_value (untuple, i);
      g_variant_get (child,
                     "(susdut)",
                     NULL,
                     &kind,
                     NULL,
                     NULL,
                     NULL,
                     NULL);
      if (kind == UP_DEVICE_KIND_LINE_POWER)
        {
          /* do nothing */
        }
      else if (kind == UP_DEVICE_KIND_UPS && ups_as_primary_device)
        {
          set_device_ups_primary (panel, child);
        }
      else if (kind == UP_DEVICE_KIND_BATTERY && !ups_as_primary_device)
        {
          if (!got_primary)
            {
              set_device_battery_primary (panel, child);
              got_primary = TRUE;
            }
          else
            {
              set_device_battery_additional (panel, child);
            }
        }
      else
        {
          add_device_secondary (panel, child, &secondary_devices_cnt);
        }
      g_variant_unref (child);
    }

  g_variant_unref (untuple);
  g_variant_unref (result);
}

static void
on_properties_changed (GDBusProxy *proxy,
                       GVariant   *changed_properties,
                       GStrv       invalidated_properties,
                       gpointer    user_data)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  /* get the new state */
  g_dbus_proxy_call (priv->proxy,
                     "GetDevices",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     priv->cancellable,
                     get_devices_cb,
                     user_data);
}

static void
got_power_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (priv->proxy == NULL)
    {
      g_printerr ("Error creating proxy: %s\n", error->message);
      g_error_free (error);
      return;
    }

  /* we want to change the primary device changes */
  g_signal_connect (priv->proxy,
                    "g-properties-changed",
                    G_CALLBACK (on_properties_changed),
                    user_data);

  /* get the initial state */
  g_dbus_proxy_call (priv->proxy,
                     "GetDevices",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     200, /* we don't want to randomly expand the dialog */
                     priv->cancellable,
                     get_devices_cb,
                     user_data);
}

static void
combo_time_changed_cb (GtkWidget *widget, CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;
  const gchar *key = (const gchar *)g_object_get_data (G_OBJECT(widget), "_gsettings_key");

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);

  /* set both keys */
  g_settings_set_int (self->priv->gsd_settings, key, value);
}

static void
combo_enum_changed_cb (GtkWidget *widget, CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;
  const gchar *key = (const gchar *)g_object_get_data (G_OBJECT(widget), "_gsettings_key");

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);

  /* set both battery and ac keys */
  g_settings_set_enum (self->priv->gsd_settings, key, value);
}

static void
disable_unavailable_combo_items (CcPowerPanel *self,
                                 GtkComboBox *combo_box)
{
  gboolean enabled;
  gboolean ret;
  gint value_tmp;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GtkTreeModel *model;

  /* setup the renderer */
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
                                  "text", ACTION_MODEL_TEXT,
                                  "sensitive", ACTION_MODEL_SENSITIVE,
                                  NULL);

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  /* disable any actions we cannot do */
  do
    {
      gtk_tree_model_get (model, &iter,
                          ACTION_MODEL_VALUE, &value_tmp,
                          -1);
      switch (value_tmp) {
      case GSD_POWER_ACTION_SUSPEND:
        enabled = up_client_get_can_suspend (self->priv->up_client);
        break;
      case GSD_POWER_ACTION_HIBERNATE:
        enabled = up_client_get_can_hibernate (self->priv->up_client);
        break;
      default:
        enabled = TRUE;
      }
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          ACTION_MODEL_SENSITIVE, enabled,
                          -1);
    } while (gtk_tree_model_iter_next (model, &iter));
}

static void
set_value_for_combo (GtkComboBox *combo_box, gint value)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value_tmp;
  gboolean ret;

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  /* try to make the UI match the setting */
  do
    {
      gtk_tree_model_get (model, &iter,
                          1, &value_tmp,
                          -1);
      if (value == value_tmp)
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          break;
        }
    } while (gtk_tree_model_iter_next (model, &iter));
}

static void
set_ac_battery_ui_mode (CcPowerPanel *self)
{
  gboolean has_batteries = FALSE;
  gboolean ret;
  GError *error = NULL;
  GPtrArray *devices;
  guint i;
  UpDevice *device;
  UpDeviceKind kind;
  CcPowerPanelPrivate *priv = self->priv;

  /* this is sync, but it's cached in the daemon and so quick */
  ret = up_client_enumerate_devices_sync (self->priv->up_client, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to get device list: %s", error->message);
      g_error_free (error);
      goto out;
    }

  devices = up_client_get_devices (self->priv->up_client);
  for (i=0; i<devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      g_object_get (device,
                    "kind", &kind,
                    NULL);
      if (kind == UP_DEVICE_KIND_BATTERY ||
          kind == UP_DEVICE_KIND_UPS)
        {
          has_batteries = TRUE;
          break;
        }
    }
  g_ptr_array_unref (devices);
out:
  gtk_widget_set_visible (WID (priv->builder, "label_header_battery"), has_batteries);
  gtk_widget_set_visible (WID (priv->builder, "label_header_ac"), has_batteries);
  gtk_widget_set_visible (WID (priv->builder, "combobox_sleep_battery"), has_batteries);
  gtk_widget_set_visible (WID (priv->builder, "label_critical"), has_batteries);
  gtk_widget_set_visible (WID (priv->builder, "combobox_critical"), has_batteries);
}

static gboolean
activate_link_cb (GtkLabel *label, gchar *uri, CcPowerPanel *self)
{
  CcShell *shell;
  GError *error = NULL;

  shell = cc_panel_get_shell (CC_PANEL (self));
  if (cc_shell_set_active_panel_from_id (shell, uri, NULL, &error) == FALSE)
    {
      g_warning ("Failed to activate %s panel: %s", uri, error->message);
      g_error_free (error);
    }
  return TRUE;
}

static void
cc_power_panel_init (CcPowerPanel *self)
{
  GError     *error;
  GtkWidget  *widget;
  gint        value;
  gchar      *tmp;

  self->priv = POWER_PANEL_PRIVATE (self);

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_file (self->priv->builder,
                             GNOMECC_UI_DIR "/power.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  /* add custom progressbar */
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "progressbar_primary"));
  gtk_widget_hide (widget);
  self->priv->progressbar_primary = CC_STRENGTH_BAR (cc_strength_bar_new ());
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "box_primary"));
  gtk_box_pack_end (GTK_BOX (widget),
                    GTK_WIDGET (self->priv->progressbar_primary),
                    FALSE,
                    TRUE,
                    0);
  gtk_widget_set_visible (GTK_WIDGET (self->priv->progressbar_primary), TRUE);

  self->priv->cancellable = g_cancellable_new ();

  /* get initial icon state */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.SettingsDaemon",
                            "/org/gnome/SettingsDaemon/Power",
                            "org.gnome.SettingsDaemon.Power",
                            self->priv->cancellable,
                            got_power_proxy_cb,
                            self);

  /* find out if there are any battery or UPS devices attached
   * and setup UI accordingly */
  self->priv->up_client = up_client_new ();
  set_ac_battery_ui_mode (self);

  self->priv->gsd_settings = g_settings_new ("org.gnome.settings-daemon.plugins.power");
  g_signal_connect (self->priv->gsd_settings,
                    "changed",
                    G_CALLBACK (on_lock_settings_changed),
                    self);

  /* auto-sleep time */
  value = g_settings_get_int (self->priv->gsd_settings, "sleep-inactive-ac-timeout");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep_ac"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "sleep-inactive-ac-timeout");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_time_changed_cb),
                    self);
  value = g_settings_get_int (self->priv->gsd_settings, "sleep-inactive-battery-timeout");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_sleep_battery"));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "sleep-inactive-battery-timeout");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_time_changed_cb),
                    self);

  /* actions */
  value = g_settings_get_enum (self->priv->gsd_settings, "critical-battery-action");
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "combobox_critical"));
  disable_unavailable_combo_items (self, GTK_COMBO_BOX (widget));
  set_value_for_combo (GTK_COMBO_BOX (widget), value);
  g_object_set_data (G_OBJECT(widget), "_gsettings_key", "critical-battery-action");
  g_signal_connect (widget, "changed",
                    G_CALLBACK (combo_enum_changed_cb),
                    self);

  /* set screen link */
  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                               "label_screen_settings"));
  /* TRANSLATORS: this is a link to the "Brightness and Lock" control center panel */
  tmp = g_strdup_printf ("<span size=\"small\">%s "
                         "<a href=\"screen\">%s</a> %s</span>",
                         _("Tip:"),
                         _("Brightness Settings"),
                         _("affect how much power is used"));
  gtk_label_set_markup (GTK_LABEL (widget), tmp);
  g_free (tmp);
  g_signal_connect (widget, "activate-link",
                    G_CALLBACK (activate_link_cb),
                    self);

  widget = WID (self->priv->builder, "vbox_power");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

void
cc_power_panel_register (GIOModule *module)
{
  cc_power_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_POWER_PANEL,
                                  "power", 0);
}

