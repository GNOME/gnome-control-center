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
#include "egg-list-box/egg-list-box.h"

#ifdef HAVE_BLUETOOTH
#include <bluetooth-killswitch.h>
#endif

#ifdef HAVE_NETWORK_MANAGER
#include <nm-client.h>
#endif

#include "cc-power-panel.h"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

CC_PANEL_REGISTER (CcPowerPanel, cc_power_panel)

#define POWER_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_POWER_PANEL, CcPowerPanelPrivate))

struct _CcPowerPanelPrivate
{
  GSettings     *gsd_settings;
  GCancellable  *cancellable;
  GtkBuilder    *builder;
  GDBusProxy    *proxy;
  UpClient      *up_client;
  GDBusProxy    *screen_proxy;
  gboolean       has_batteries;

  GtkWidget     *battery_section;
  GtkWidget     *battery_list;
  GtkWidget     *label_battery_addon;
  GtkSizeGroup  *row_sizegroup;
  GtkSizeGroup  *battery_sizegroup;
  GtkSizeGroup  *charge_sizegroup;
  GtkSizeGroup  *level_sizegroup;

  GtkWidget     *brightness_row;
  GtkWidget     *brightness_scale;
  gboolean       setting_brightness;

  GtkWidget     *automatic_suspend_row;
  GtkWidget     *automatic_suspend_label;
  GtkWidget     *critical_battery_row;
  GtkWidget     *critical_battery_combo;

#ifdef HAVE_BLUETOOTH
  BluetoothKillswitch *bt_killswitch;
  GtkWidget           *bt_switch;
#endif

#ifdef HAVE_NETWORK_MANAGER
  NMClient      *nm_client;
  GtkWidget     *wifi_switch;
  GtkWidget     *wifi_row;
#endif
};

enum
{
  ACTION_MODEL_TEXT,
  ACTION_MODEL_VALUE,
  ACTION_MODEL_SENSITIVE
};

static void
cc_power_panel_dispose (GObject *object)
{
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (object)->priv;

  g_clear_object (&priv->gsd_settings);
  if (priv->cancellable != NULL)
    {
      g_cancellable_cancel (priv->cancellable);
      g_object_unref (priv->cancellable);
      priv->cancellable = NULL;
    }
  g_clear_object (&priv->builder);
  g_clear_object (&priv->proxy);
  g_clear_object (&priv->screen_proxy);
  g_clear_object (&priv->up_client);
#ifdef HAVE_BLUETOOTH
  g_clear_object (&priv->bt_killswitch);
#endif
#ifdef HAVE_NETWORK_MANAGER
  g_clear_object (&priv->nm_client);
#endif

  G_OBJECT_CLASS (cc_power_panel_parent_class)->dispose (object);
}

static const char *
cc_power_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/power";
}

static void
cc_power_panel_class_init (CcPowerPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcPowerPanelPrivate));

  object_class->dispose = cc_power_panel_dispose;

  panel_class->get_help_uri = cc_power_panel_get_help_uri;
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
add_primary (CcPowerPanel *panel,
             const gchar  *name,
             const gchar  *details,
             gdouble       percentage)
{
  CcPowerPanelPrivate *priv = panel->priv;
  GtkWidget *box;
  GtkWidget *box2;
  GtkWidget *label;
  GtkWidget *levelbar;
  gchar *s;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  box2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_size_group_add_widget (priv->battery_sizegroup, box2);
  gtk_widget_set_margin_left (box2, 20);
  gtk_widget_set_margin_right (box2, 20);
  gtk_widget_set_margin_top (box2, 6);
  gtk_widget_set_margin_bottom (box2, 6);
  gtk_box_pack_start (GTK_BOX (box), box2, FALSE, TRUE, 0);

  label = gtk_label_new (name);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (box2), label, TRUE, TRUE, 0);

  label = gtk_label_new (details);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), label, TRUE, TRUE, 0);

  priv->label_battery_addon = label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), label, TRUE, TRUE, 0);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_left (box2, 20);
  gtk_widget_set_margin_right (box2, 20);
  s = g_strdup_printf ("%d%%", (int)percentage);
  label = gtk_label_new (s);
  g_free (s);
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);
  gtk_size_group_add_widget (priv->charge_sizegroup, label);

  levelbar = gtk_level_bar_new ();
  gtk_level_bar_set_value (GTK_LEVEL_BAR (levelbar), percentage / 100.0);
  gtk_widget_set_hexpand (levelbar, TRUE);
  gtk_widget_set_halign (levelbar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (levelbar, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box2), levelbar, TRUE, TRUE, 0);
  gtk_size_group_add_widget (priv->level_sizegroup, levelbar);
  gtk_box_pack_start (GTK_BOX (box), box2, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (priv->battery_list), box);
  gtk_size_group_add_widget (priv->row_sizegroup, box);
  gtk_widget_show_all (box);
  gtk_widget_hide (priv->label_battery_addon);

  g_object_set_data (G_OBJECT (box), "primary", GINT_TO_POINTER (TRUE));
}

static void
set_device_battery_primary (CcPowerPanel *panel, GVariant *device)
{
  gchar *details = NULL;
  gchar *time_string = NULL;
  gdouble percentage;
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

  /* set the description */
  if (time > 0)
    {
      time_string = get_timestring (time);
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
            details = g_strdup_printf (_("%s until fully charged"), time_string);
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            if (percentage < 20)
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf (_("Caution: %s remaining"), time_string);
              }
            else
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf (_("%s remaining"), time_string);
              }
            break;
          default:
            details = g_strdup_printf ("error: %s", up_device_state_to_string (state));
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
            details = g_strdup (_("Charging"));
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Using battery power"));
            break;
          case UP_DEVICE_STATE_FULLY_CHARGED:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Fully charged"));
            break;
          case UP_DEVICE_STATE_EMPTY:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Empty"));
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }

  add_primary (panel, _("Battery"), details, percentage);

  g_free (time_string);
  g_free (details);
}

static void
set_device_ups_primary (CcPowerPanel *panel, GVariant *device)
{
  gchar *details = NULL;
  gchar *time_string = NULL;
  gdouble percentage;
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
                details = g_strdup_printf (_("Caution: %s remaining"), time_string);
              }
            else
              {
                /* TRANSLATORS: %1 is a time string, e.g. "1 hour 5 minutes" */
                details = g_strdup_printf (_("%s remaining"), time_string);
              }
            break;
          default:
            details = g_strdup_printf ("error: %s", up_device_state_to_string (state));
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
                details = g_strdup (_("Caution low UPS"));
              }
            else
              {
                /* TRANSLATORS: UPS battery */
                details = g_strdup (_("Using UPS power"));
              }
            break;
          default:
            details = g_strdup_printf ("error: %s",
                                       up_device_state_to_string (state));
            break;
        }
    }

  add_primary (panel, _("Uninterruptible power supply"), details, percentage);

  g_free (time_string);
  g_free (details);
}

static void
set_device_battery_additional (CcPowerPanel *panel, GVariant *device)
{
  CcPowerPanelPrivate *priv = panel->priv;
  gchar *details = NULL;
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
    {
      gtk_widget_hide (priv->label_battery_addon);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (priv->label_battery_addon), details);
      gtk_widget_show (priv->label_battery_addon);
    }

  g_free (details);
}

static void
add_device_secondary (CcPowerPanel *panel,
                      GVariant *device)
{
  CcPowerPanelPrivate *priv = panel->priv;
  gdouble percentage;
  guint64 time;
  UpDeviceKind kind;
  UpDeviceState state;
  GtkWidget *hbox;
  GtkWidget *box2;
  GtkWidget *widget;
  GString *status;
  GString *description;
  gchar *s;
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
        show_caution = TRUE;
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

  switch (state)
    {
      case UP_DEVICE_STATE_CHARGING:
      case UP_DEVICE_STATE_PENDING_CHARGE:
        /* TRANSLATORS: secondary battery */
        status = g_string_new(C_("Battery power", "Charging"));
        break;
      case UP_DEVICE_STATE_DISCHARGING:
      case UP_DEVICE_STATE_PENDING_DISCHARGE:
        if (percentage < 10 && show_caution)
          {
            /* TRANSLATORS: secondary battery */
            status = g_string_new (C_("Battery power", "Caution"));
          }
        else if (percentage < 30)
          {
            /* TRANSLATORS: secondary battery */
            status = g_string_new (C_("Battery power", "Low"));
          }
        else
          {
            /* TRANSLATORS: secondary battery */
            status = g_string_new (C_("Battery power", "Good"));
          }
        break;
      case UP_DEVICE_STATE_FULLY_CHARGED:
        /* TRANSLATORS: primary battery */
        status = g_string_new(C_("Battery power", "Fully charged"));
        break;
      case UP_DEVICE_STATE_EMPTY:
        /* TRANSLATORS: primary battery */
        status = g_string_new(C_("Battery power", "Empty"));
        break;
      default:
        status = g_string_new (up_device_state_to_string (state));
        break;
    }
  g_string_prepend (status, "<small>");
  g_string_append (status, "</small>");

  /* create the new widget */
  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  widget = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (widget), 0.0f, 0.5f);
  gtk_label_set_markup (GTK_LABEL (widget), description->str);
  gtk_widget_set_margin_left (widget, 20);
  gtk_widget_set_margin_right (widget, 20);
  gtk_widget_set_margin_top (widget, 6);
  gtk_widget_set_margin_bottom (widget, 6);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, TRUE, 0);
  gtk_size_group_add_widget (priv->battery_sizegroup, widget);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_set_margin_left (box2, 20);
  gtk_widget_set_margin_right (box2, 20);
  s = g_strdup_printf ("%d%%", (int)percentage);
  widget = gtk_label_new (s);
  g_free (s);
  gtk_misc_set_alignment (GTK_MISC (widget), 1, 0.5);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), widget, FALSE, TRUE, 0);
  gtk_size_group_add_widget (priv->charge_sizegroup, widget);

  widget = gtk_level_bar_new ();
  gtk_widget_set_halign (widget, TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), percentage / 100.0f);
  gtk_box_pack_start (GTK_BOX (box2), widget, TRUE, TRUE, 0);
  gtk_size_group_add_widget (priv->level_sizegroup, widget);
  gtk_box_pack_start (GTK_BOX (hbox), box2, TRUE, TRUE, 0);
  gtk_widget_show_all (hbox);

  gtk_container_add (GTK_CONTAINER (priv->battery_list), hbox);
  gtk_size_group_add_widget (priv->row_sizegroup, hbox);
  g_object_set_data (G_OBJECT (hbox), "kind", GINT_TO_POINTER (kind));

  g_string_free (description, TRUE);
  g_string_free (status, TRUE);
}

static void
get_devices_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcPowerPanel *panel;
  CcPowerPanelPrivate *priv;
  gboolean got_primary = FALSE;
  gboolean ups_as_primary_device = FALSE;
  GError *error = NULL;
  gsize n_devices;
  GList *children;
  GList *l;
  guint i;
  GVariant *child;
  GVariant *result;
  GVariant *untuple;
  UpDeviceKind kind;
  UpDeviceState state;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_error_free (error);
      return; /* Must exit before accessing freed memory */
    }

  panel = CC_POWER_PANEL (user_data);
  priv = panel->priv;

  children = gtk_container_get_children (GTK_CONTAINER (priv->battery_list));
  for (l = children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (priv->battery_list), l->data);
  g_list_free (children);

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
          add_device_secondary (panel, child);
        }
      g_variant_unref (child);
    }

#if 0
  g_print ("adding fake devices\n");
  child = g_variant_new_parsed ("('/',%u,'',%d,%u,%t)",
                                UP_DEVICE_KIND_MOUSE, 100.0,
                                UP_DEVICE_STATE_DISCHARGING, 287);
  add_device_secondary (panel, child);
  g_variant_unref (child);
  child = g_variant_new_parsed ("('/',%u,'',%d,%u,%t)",
                                UP_DEVICE_KIND_KEYBOARD, 18.0,
                                UP_DEVICE_STATE_DISCHARGING, 287);
  add_device_secondary (panel, child);
  g_variant_unref (child);
  child = g_variant_new_parsed ("('/',%u,'',%d,%u,%t)",
                                UP_DEVICE_KIND_BATTERY, 11.0,
                                UP_DEVICE_STATE_FULLY_CHARGED, 200);
  set_device_battery_additional (panel, child);
  g_variant_unref (child);
#endif

  g_variant_unref (untuple);
  g_variant_unref (result);

  children = gtk_container_get_children (GTK_CONTAINER (priv->battery_list));
  gtk_widget_set_visible (priv->battery_section, children != NULL);
  gtk_widget_queue_resize (priv->battery_section);
  g_list_free (children);
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
set_brightness_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  GVariant *result;
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  /* not setting, so pay attention to changed signals */
  priv->setting_brightness = FALSE;
  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (result == NULL)
    {
      g_printerr ("Error setting brightness: %s\n", error->message);
      g_error_free (error);
      return;
    }
}

static void
brightness_slider_value_changed_cb (GtkRange *range, gpointer user_data)
{
  guint percentage;
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  /* do not loop */
  if (priv->setting_brightness)
    return;

  priv->setting_brightness = TRUE;

  /* push this to g-p-m */
  percentage = (guint) gtk_range_get_value (range);
  g_dbus_proxy_call (priv->screen_proxy,
                     "SetPercentage",
                     g_variant_new ("(u)",
                                    percentage),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     priv->cancellable,
                     set_brightness_cb,
                     user_data);
}

static void
get_brightness_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  GError *error = NULL;
  GVariant *result;
  guint brightness;
  GtkRange *range;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
  if (result == NULL)
    {
      /* We got cancelled, so we're probably exiting */
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          return;
        }

      gtk_widget_hide (self->priv->brightness_row);

      if (error->message &&
          strstr (error->message, "No backlight devices present") == NULL)
        {
          g_warning ("Error getting brightness: %s", error->message);
        }
      g_error_free (error);
      return;
    }

  /* set the slider */
  g_variant_get (result, "(u)", &brightness);
  range = GTK_RANGE (self->priv->brightness_scale);
  gtk_range_set_range (range, 0, 100);
  gtk_range_set_increments (range, 1, 10);
  gtk_range_set_value (range, brightness);
  g_signal_connect (range, "value-changed",
                    G_CALLBACK (brightness_slider_value_changed_cb), user_data);
  g_variant_unref (result);
}

static void
on_signal (GDBusProxy *proxy,
           gchar      *sender_name,
           gchar      *signal_name,
           GVariant   *parameters,
           gpointer    user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);

  if (g_strcmp0 (signal_name, "Changed") == 0)
    {
      /* changed, but ignoring */
      if (self->priv->setting_brightness)
        return;

      /* retrieve the value again from g-s-d */
      g_dbus_proxy_call (self->priv->screen_proxy,
                         "GetPercentage",
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         200, /* we don't want to randomly move the bar */
                         self->priv->cancellable,
                         get_brightness_cb,
                         user_data);
    }
}

static void
got_power_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  GDBusProxy *proxy;
  CcPowerPanelPrivate *priv;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      g_printerr ("Error creating proxy: %s\n", error->message);
      g_error_free (error);
      return;
    }

  /* Access user_data after checking for error because user_data might be
     disposed already. */
  priv = CC_POWER_PANEL (user_data)->priv;
  priv->proxy = proxy;

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
got_screen_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  CcPowerPanelPrivate *priv = CC_POWER_PANEL (user_data)->priv;

  priv->screen_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (priv->screen_proxy == NULL)
    {
      g_printerr ("Error creating proxy: %s\n", error->message);
      g_error_free (error);
      return;
    }

  /* we want to change the bar if the user presses brightness buttons */
  g_signal_connect (priv->screen_proxy, "g-signal",
                    G_CALLBACK (on_signal), user_data);

  /* get the initial state */
  g_dbus_proxy_call (priv->screen_proxy,
                     "GetPercentage",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     200, /* we don't want to randomly move the bar */
                     priv->cancellable,
                     get_brightness_cb,
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
                          ACTION_MODEL_VALUE, &value_tmp,
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

  /* this is sync, but it's cached in the daemon and so quick */
  ret = up_client_enumerate_devices_sync (self->priv->up_client, NULL, &error);
  if (!ret)
    {
      g_warning ("failed to get device list: %s", error->message);
      g_error_free (error);
      goto out;
    }

  devices = up_client_get_devices (self->priv->up_client);
  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      g_object_get (device, "kind", &kind, NULL);
      if (kind == UP_DEVICE_KIND_BATTERY ||
          kind == UP_DEVICE_KIND_UPS)
        {
          has_batteries = TRUE;
          break;
        }
    }
  g_ptr_array_unref (devices);

#if 0
  g_print ("forcing no batteries\n");
  has_batteries = FALSE;
#endif

out:
  self->priv->has_batteries = has_batteries;

  gtk_widget_set_visible (self->priv->critical_battery_row, has_batteries);

  if (!has_batteries)
    {
      gtk_widget_hide (WID (self->priv->builder, "suspend_on_battery_switch"));
      gtk_widget_hide (WID (self->priv->builder, "suspend_on_battery_label"));
      gtk_widget_hide (WID (self->priv->builder, "suspend_on_battery_delay_label"));
      gtk_widget_hide (WID (self->priv->builder, "suspend_on_battery_delay_combo"));
      gtk_label_set_label (GTK_LABEL (WID (self->priv->builder, "suspend_on_ac_label")),
                           _("When _idle"));
    }
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
  if (before == NULL)
    return;

  if (*separator == NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      g_object_ref_sink (*separator);
      gtk_widget_show (*separator);
    }
}

#ifdef HAVE_BLUETOOTH
static void
bt_switch_changed (GtkSwitch    *sw,
                   GParamSpec   *pspec,
                   CcPowerPanel *panel)
{
  BluetoothKillswitchState state;

  if (gtk_switch_get_active (sw))
    state = BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
  else
    state = BLUETOOTH_KILLSWITCH_STATE_SOFT_BLOCKED;

  g_debug ("Setting bt killswitch to %s", bluetooth_killswitch_state_to_string (state));

  bluetooth_killswitch_set_state (panel->priv->bt_killswitch, state);
}

static void
bt_killswitch_state_changed (BluetoothKillswitch      *killswitch,
                             BluetoothKillswitchState  state,
                             CcPowerPanel             *panel)
{
  CcPowerPanelPrivate *priv = panel->priv;
  gboolean active;
  gboolean sensitive;

  g_debug ("bt killswitch state changed to %s", bluetooth_killswitch_state_to_string (state));

  active = state == BLUETOOTH_KILLSWITCH_STATE_UNBLOCKED;
  sensitive = state != BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED;

  g_signal_handlers_block_by_func (priv->bt_switch, bt_switch_changed, panel);
  gtk_switch_set_active (GTK_SWITCH (priv->bt_switch), active);
  gtk_widget_set_sensitive (priv->bt_switch, sensitive);
  g_signal_handlers_unblock_by_func (priv->bt_switch, bt_switch_changed, panel);
}
#endif

#ifdef HAVE_NETWORK_MANAGER
static gboolean
has_wifi_devices (NMClient *client)
{
  const GPtrArray *devices;
  NMDevice *device;
  gint i;

  if (!nm_client_get_manager_running (client))
    return FALSE;

  devices = nm_client_get_devices (client);
  if (devices == NULL)
    return FALSE;

  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      switch (nm_device_get_device_type (device))
        {
        case NM_DEVICE_TYPE_WIFI:
        case NM_DEVICE_TYPE_WIMAX:
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}

static void
wifi_switch_changed (GtkSwitch    *sw,
                     GParamSpec   *pspec,
                     CcPowerPanel *panel)
{
  gboolean enabled;

  enabled = gtk_switch_get_active (sw);
  g_debug ("Setting wifi %s", enabled ? "enabled" : "disabled");
  nm_client_wireless_set_enabled (panel->priv->nm_client, enabled);
}

static void
nm_client_state_changed (NMClient     *client,
                         GParamSpec   *pspec,
                         CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;
  gboolean visible;
  gboolean active;
  gboolean sensitive;

  visible = has_wifi_devices (priv->nm_client);
  active = nm_client_networking_get_enabled (client) &&
           nm_client_wireless_get_enabled (client) &&
           nm_client_wireless_hardware_get_enabled (client);
  sensitive = nm_client_networking_get_enabled (client) &&
              nm_client_wireless_hardware_get_enabled (client);

  g_debug ("wifi state changed to %s", active ? "enabled" : "disabled");

  g_signal_handlers_block_by_func (priv->wifi_switch, wifi_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (priv->wifi_switch), active);
  gtk_widget_set_sensitive (priv->wifi_switch, sensitive);
  gtk_widget_set_visible (priv->wifi_row, visible);
  g_signal_handlers_unblock_by_func (priv->wifi_switch, wifi_switch_changed, self);
}

static void
nm_device_changed (NMClient     *client,
                   NMDevice     *device,
                   CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;

  gtk_widget_set_visible (priv->wifi_row, has_wifi_devices (priv->nm_client));
}
#endif

static void
add_power_saving_section (CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;
  GtkWidget *vbox;
  GtkWidget *widget, *box, *label, *scale;
  GtkWidget *box2;
  GtkWidget *sw;
  gchar *s;

  vbox = WID (priv->builder, "vbox_power");

  s = g_strdup_printf ("<b>%s</b>", _("Power Saving"));
  widget = gtk_label_new (s);
  g_free (s);
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
  gtk_widget_set_margin_left (widget, 56);
  gtk_widget_set_margin_right (widget, 56);
  gtk_widget_set_margin_bottom (widget, 6);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);
  gtk_widget_show (widget);

  widget = GTK_WIDGET (egg_list_box_new ());
  egg_list_box_set_selection_mode (EGG_LIST_BOX (widget), GTK_SELECTION_NONE);
  egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                    update_separator_func,
                                    NULL, NULL);

  box = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_left (box, 50);
  gtk_widget_set_margin_right (box, 50);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 0);

  priv->brightness_row = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  label = gtk_label_new (_("_Screen Brightness"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_left (label, 20);
  gtk_widget_set_margin_right (label, 20);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
  gtk_size_group_add_widget (priv->battery_sizegroup, label);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);
  gtk_size_group_add_widget (priv->charge_sizegroup, label);

  priv->brightness_scale = scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_widget_set_margin_left (scale, 20);
  gtk_widget_set_margin_right (scale, 20);
  gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
  gtk_size_group_add_widget (priv->level_sizegroup, scale);

  gtk_box_pack_start (GTK_BOX (box), box2, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (widget), box);
  gtk_size_group_add_widget (priv->row_sizegroup, box);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  label = gtk_label_new (_("Screen _Power Saving"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_left (label, 20);
  gtk_widget_set_margin_right (label, 20);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  /* FIXME: implement and use a screen-power-saving setting */
  sw = gtk_switch_new ();
  g_settings_bind (priv->gsd_settings, "idle-dim-battery",
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_widget_set_margin_left (sw, 20);
  gtk_widget_set_margin_right (sw, 20);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
  gtk_container_add (GTK_CONTAINER (widget), box);
  gtk_size_group_add_widget (priv->row_sizegroup, box);

#ifdef HAVE_NETWORK_MANAGER
  priv->wifi_row = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  label = gtk_label_new (_("_Wi-Fi"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_left (label, 20);
  gtk_widget_set_margin_right (label, 20);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  priv->wifi_switch = sw = gtk_switch_new ();
  gtk_widget_set_margin_left (sw, 20);
  gtk_widget_set_margin_right (sw, 20);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
  gtk_container_add (GTK_CONTAINER (widget), box);
  gtk_size_group_add_widget (priv->row_sizegroup, box);

  priv->nm_client = nm_client_new ();
  g_signal_connect (priv->nm_client, "notify",
                    G_CALLBACK (nm_client_state_changed), self);
  g_signal_connect (priv->nm_client, "device-added",
                    G_CALLBACK (nm_device_changed), self);
  g_signal_connect (priv->nm_client, "device-removed",
                    G_CALLBACK (nm_device_changed), self);
  nm_device_changed (priv->nm_client, NULL, self);

  g_signal_connect (G_OBJECT (priv->wifi_switch), "notify::active",
                    G_CALLBACK (wifi_switch_changed), self);
#endif

#ifdef HAVE_BLUETOOTH
  {
    BluetoothKillswitchState bt_state;
    priv->bt_killswitch = bluetooth_killswitch_new ();
    bt_state = bluetooth_killswitch_get_state (priv->bt_killswitch);
    if (bt_state != BLUETOOTH_KILLSWITCH_STATE_NO_ADAPTER)
      {
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
        label = gtk_label_new (_("_Bluetooth"));
        gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_widget_set_margin_left (label, 20);
        gtk_widget_set_margin_right (label, 20);
        gtk_widget_set_margin_top (label, 6);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

        priv->bt_switch = sw = gtk_switch_new ();
        gtk_widget_set_sensitive (priv->bt_switch, bt_state == BLUETOOTH_KILLSWITCH_STATE_HARD_BLOCKED);
        gtk_widget_set_margin_left (sw, 20);
        gtk_widget_set_margin_right (sw, 20);
        gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
        gtk_container_add (GTK_CONTAINER (widget), box);
        gtk_size_group_add_widget (priv->row_sizegroup, box);

        bt_killswitch_state_changed (priv->bt_killswitch, bt_state, self);
        g_signal_connect (G_OBJECT (priv->bt_killswitch), "state-changed",
                          G_CALLBACK (bt_killswitch_state_changed), self);
        g_signal_connect (G_OBJECT (priv->bt_switch), "notify::active",
                          G_CALLBACK (bt_switch_changed), self);
      }
    }
#endif

  gtk_widget_show_all (widget);
}

static void
update_automatic_suspend_label (CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;
  gint ac_action;
  gint battery_action;
  gint ac_timeout;
  gint battery_timeout;
  const gchar *s;

  ac_action = g_settings_get_enum (priv->gsd_settings, "sleep-inactive-ac-type");
  battery_action = g_settings_get_enum (priv->gsd_settings, "sleep-inactive-battery-type");
  ac_timeout = g_settings_get_int (priv->gsd_settings, "sleep-inactive-ac-timeout");
  battery_timeout = g_settings_get_int (priv->gsd_settings, "sleep-inactive-battery-timeout");

  if (ac_action == 5)
    ac_timeout = 0;
  if (battery_action == 5)
    battery_timeout = 0;

  if (priv->has_batteries)
    {
      if (ac_timeout == 0 && battery_timeout == 0)
        s = _("Off");
      else if (ac_timeout == 0 && battery_timeout != 0)
        s = _("When on battery power");
      else if (ac_timeout != 0 && battery_timeout == 0)
        s = _("When plugged in");
      else
        s = _("On");
    }
  else
    {
      if (ac_timeout == 0)
        s = _("Off");
      else
        s = _("On");
    }

  gtk_label_set_label (GTK_LABEL (priv->automatic_suspend_label), s);
}

static void
on_suspend_settings_changed (GSettings    *settings,
                             const char   *key,
                             CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;
  gint value;

  if (g_strcmp0 (key, "critical-battery-action") == 0)
    {
      value = g_settings_get_enum (settings, "critical-battery-action");
      set_value_for_combo (GTK_COMBO_BOX (priv->critical_battery_combo), value);
    }
  if (g_str_has_prefix (key, "sleep-inactive-"))
    {
      update_automatic_suspend_label (self);
    }
}

static void
activate_child (CcPowerPanel *self,
                GtkWidget    *child)
{
  CcPowerPanelPrivate *priv = self->priv;
  GtkWidget *w;
  GtkWidget *toplevel;

  if (child == priv->automatic_suspend_row)
    {
      w = WID (priv->builder, "automatic_suspend_dialog");
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
      gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (toplevel));
      gtk_window_set_modal (GTK_WINDOW (w), TRUE);
      gtk_window_present (GTK_WINDOW (w));
    }
}

static gboolean
get_sleep_type (GValue   *value,
                GVariant *variant,
                gpointer  data)
{
  gboolean enabled;

  if (g_strcmp0 (g_variant_get_string (variant, NULL), "nothing") == 0)
    enabled = FALSE;
  else
    enabled = TRUE;

  g_value_set_boolean (value, enabled);

  return TRUE;
}

static GVariant *
set_sleep_type (const GValue       *value,
                const GVariantType *expected_type,
                gpointer            data)
{
  GVariant *res;

  if (g_value_get_boolean (value))
    res = g_variant_new_string ("suspend");
  else
    res = g_variant_new_string ("nothing");

  return res;
}

static void
add_automatic_suspend_section (CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;
  GtkWidget *vbox;
  GtkWidget *widget, *box, *label;
  GtkWidget *sw;
  gchar *s;
  gint value;
  GtkTreeModel *model;
  GtkWidget *dialog;
  GtkWidget *combo;

  vbox = WID (priv->builder, "vbox_power");

  s = g_markup_printf_escaped ("<b>%s</b>", _("Suspend & Power Off"));
  widget = gtk_label_new (s);
  g_free (s);
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);
  gtk_widget_set_margin_left (widget, 56);
  gtk_widget_set_margin_right (widget, 50);
  gtk_widget_set_margin_bottom (widget, 6);
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, TRUE, 0);
  gtk_widget_show (widget);

  widget = GTK_WIDGET (egg_list_box_new ());
  egg_list_box_set_selection_mode (EGG_LIST_BOX (widget), GTK_SELECTION_NONE);
  egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                    update_separator_func,
                                    NULL, NULL);
  g_signal_connect_swapped (widget, "child-activated",
                            G_CALLBACK (activate_child), self);

  box = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_left (box, 50);
  gtk_widget_set_margin_right (box, 50);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 0);

  self->priv->automatic_suspend_row = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  label = gtk_label_new (_("_Automatic Suspend"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_left (label, 20);
  gtk_widget_set_margin_right (label, 20);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  priv->automatic_suspend_label = sw = gtk_label_new ("");
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
  gtk_misc_set_alignment (GTK_MISC (sw), 1, 0.5);
  gtk_widget_set_margin_left (sw, 24);
  gtk_widget_set_margin_right (sw, 24);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (widget), box);
  gtk_size_group_add_widget (priv->row_sizegroup, box);
  update_automatic_suspend_label (self);

  priv->critical_battery_row = box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  label = gtk_label_new (_("When Battery Power is _Critical"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
  gtk_widget_set_margin_left (label, 20);
  gtk_widget_set_margin_right (label, 20);
  gtk_widget_set_margin_top (label, 6);
  gtk_widget_set_margin_bottom (label, 6);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

  model = (GtkTreeModel*)gtk_builder_get_object (priv->builder, "liststore_critical");
  priv->critical_battery_combo = sw = gtk_combo_box_new_with_model (model);
  gtk_widget_set_margin_left (sw, 20);
  gtk_widget_set_margin_right (sw, 20);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);

  g_object_set_data (G_OBJECT (sw), "_gsettings_key", "critical-battery-action");
  disable_unavailable_combo_items (self, GTK_COMBO_BOX (sw));
  value = g_settings_get_enum (priv->gsd_settings, "critical-battery-action");
  set_value_for_combo (GTK_COMBO_BOX (sw), value);
  g_signal_connect (sw, "changed",
                    G_CALLBACK (combo_enum_changed_cb), self);

  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);

  g_signal_connect (priv->gsd_settings, "changed",
                    G_CALLBACK (on_suspend_settings_changed), self);

  gtk_container_add (GTK_CONTAINER (widget), box);
  gtk_size_group_add_widget (priv->row_sizegroup, box);
  gtk_widget_show_all (widget);

  dialog = GTK_WIDGET (gtk_builder_get_object (priv->builder, "automatic_suspend_dialog"));
  sw = GTK_WIDGET (gtk_builder_get_object (priv->builder, "automatic_suspend_close"));
  g_signal_connect_swapped (sw, "clicked", G_CALLBACK (gtk_widget_hide), dialog);
  g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  sw = GTK_WIDGET (gtk_builder_get_object (priv->builder, "suspend_on_battery_switch"));
  g_settings_bind_with_mapping (priv->gsd_settings, "sleep-inactive-battery-type",
                                sw, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                get_sleep_type, set_sleep_type, NULL, NULL);

  combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "suspend_on_battery_delay_combo"));
  g_object_set_data (G_OBJECT (combo), "_gsettings_key", "sleep-inactive-battery-timeout");
  value = g_settings_get_int (priv->gsd_settings, "sleep-inactive-battery-timeout");
  set_value_for_combo (GTK_COMBO_BOX (combo), value);
  g_signal_connect (combo, "changed",
                    G_CALLBACK (combo_time_changed_cb), self);
  g_object_bind_property (sw, "active", combo, "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  sw = GTK_WIDGET (gtk_builder_get_object (priv->builder, "suspend_on_ac_switch"));
  g_settings_bind_with_mapping (priv->gsd_settings, "sleep-inactive-ac-type",
                                sw, "active",
                                G_SETTINGS_BIND_DEFAULT,
                                get_sleep_type, set_sleep_type, NULL, NULL);

  combo = GTK_WIDGET (gtk_builder_get_object (priv->builder, "suspend_on_ac_delay_combo"));
  g_object_set_data (G_OBJECT (combo), "_gsettings_key", "sleep-inactive-ac-timeout");
  value = g_settings_get_int (priv->gsd_settings, "sleep-inactive-ac-timeout");
  set_value_for_combo (GTK_COMBO_BOX (combo), value);
  g_signal_connect (combo, "changed",
                    G_CALLBACK (combo_time_changed_cb), self);
  g_object_bind_property (sw, "active", combo, "sensitive",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
}

static gint
device_sort_func (gconstpointer a, gconstpointer b, gpointer data)
{
  GObject *row_a = (GObject*)a;
  GObject *row_b = (GObject*)b;
  gboolean a_primary;
  gboolean b_primary;
  gint a_kind;
  gint b_kind;

  a_primary = GPOINTER_TO_INT (g_object_get_data (row_a, "primary"));
  b_primary = GPOINTER_TO_INT (g_object_get_data (row_b, "primary"));

  if (a_primary)
    return -1;
  else if (b_primary)
    return 1;

  a_kind = GPOINTER_TO_INT (g_object_get_data (row_a, "kind"));
  b_kind = GPOINTER_TO_INT (g_object_get_data (row_b, "kind"));

  return a_kind - b_kind;
}

static void
add_battery_section (CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv = self->priv;
  GtkWidget *vbox;
  GtkWidget *widget, *box;

  vbox = WID (priv->builder, "vbox_power");

  priv->battery_list = widget = GTK_WIDGET (egg_list_box_new ());
  egg_list_box_set_selection_mode (EGG_LIST_BOX (widget), GTK_SELECTION_NONE);
  egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                    update_separator_func,
                                    NULL, NULL);
  egg_list_box_set_sort_func (EGG_LIST_BOX (widget),
                              device_sort_func, NULL, NULL);
  gtk_widget_show (widget);

  priv->battery_section = box = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_left (box, 50);
  gtk_widget_set_margin_right (box, 50);
  gtk_widget_set_margin_top (box, 24);
  gtk_widget_set_margin_bottom (box, 24);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 0);
  gtk_widget_show (box);
}

static gboolean
grr (gpointer data)
{
  CcPowerPanel *self = data;

  gtk_widget_queue_resize (GTK_WIDGET (self));

  return G_SOURCE_REMOVE;
}

static void
cc_power_panel_init (CcPowerPanel *self)
{
  CcPowerPanelPrivate *priv;
  GError     *error;
  GtkWidget  *widget;
  GtkWidget  *box;

  priv = self->priv = POWER_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_file (priv->builder,
                             GNOMECC_UI_DIR "/power.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  priv->cancellable = g_cancellable_new ();

  /* get initial icon state */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.SettingsDaemon",
                            "/org/gnome/SettingsDaemon/Power",
                            "org.gnome.SettingsDaemon.Power",
                            priv->cancellable,
                            got_power_proxy_cb,
                            self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.SettingsDaemon",
                            "/org/gnome/SettingsDaemon/Power",
                            "org.gnome.SettingsDaemon.Power.Screen",
                            priv->cancellable,
                            got_screen_proxy_cb,
                            self);

  priv->up_client = up_client_new ();

  priv->gsd_settings = g_settings_new ("org.gnome.settings-daemon.plugins.power");

  priv->row_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  priv->battery_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  priv->charge_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  priv->level_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  add_battery_section (self);
  add_power_saving_section (self);
  add_automatic_suspend_section (self);

  set_ac_battery_ui_mode (self);
  update_automatic_suspend_label (self);

  widget = WID (priv->builder, "vbox_power");
  box = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (box),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (box), 490);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (self), box);
  g_object_ref (widget);
  gtk_widget_unparent (widget);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (box), widget);
  gtk_style_context_add_class (gtk_widget_get_style_context (gtk_widget_get_parent (widget)), "view");
  gtk_style_context_add_class (gtk_widget_get_style_context (gtk_widget_get_parent (widget)), "content-view");
  g_object_unref (widget);

  g_idle_add (grr, self);
}
