/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2010,2015 Richard Hughes <richard@hughsie.com>
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
 */

#include <config.h>

#include <libupower-glib/upower.h>
#include <glib/gi18n.h>
#include <gnome-settings-daemon/gsd-enums.h>

#ifdef HAVE_NETWORK_MANAGER
#include <NetworkManager.h>
#endif

#include "shell/cc-object-storage.h"
#include "list-box-helper.h"
#include "cc-power-panel.h"
#include "cc-power-resources.h"
#include "cc-util.h"

/* Uncomment this to test the behaviour of the panel in
 * battery-less situations:
 *
 * #define TEST_NO_BATTERIES
 */

/* Uncomment this to test the behaviour of the panel with
 * multiple appearing devices
 *
 * #define TEST_FAKE_DEVICES
 */

/* Uncomment this to test the behaviour of a desktop machine
 * with a UPS
 *
 * #define TEST_UPS
 */

struct _CcPowerPanel
{
  CcPanel        parent_instance;

  GSettings     *gsd_settings;
  GSettings     *session_settings;
  GtkWidget     *main_scroll;
  GtkWidget     *main_box;
  GtkWidget     *vbox_power;
  GtkWidget     *suspend_on_battery_switch;
  GtkWidget     *suspend_on_battery_label;
  GtkWidget     *suspend_on_battery_delay_label;
  GtkWidget     *suspend_on_battery_delay_combo;
  GtkWidget     *suspend_on_ac_switch;
  GtkWidget     *suspend_on_ac_label;
  GtkWidget     *suspend_on_ac_delay_combo;
  GtkWidget     *automatic_suspend_dialog;
  GtkListStore  *liststore_idle_time;
  GtkListStore  *liststore_power_button;
  UpClient      *up_client;
  GPtrArray     *devices;
  GDBusProxy    *screen_proxy;
  GDBusProxy    *kbd_proxy;
  gboolean       has_batteries;
  char          *chassis_type;

  GList         *boxes;
  GList         *boxes_reverse;

  GtkSizeGroup  *battery_row_sizegroup;
  GtkSizeGroup  *row_sizegroup;
  GtkSizeGroup  *battery_sizegroup;
  GtkSizeGroup  *charge_sizegroup;
  GtkSizeGroup  *level_sizegroup;

  GtkWidget     *battery_heading;
  GtkWidget     *battery_section;
  GtkWidget     *battery_list;

  GtkWidget     *device_heading;
  GtkWidget     *device_section;
  GtkWidget     *device_list;

  GtkWidget     *dim_screen_row;
  GtkWidget     *brightness_row;
  GtkWidget     *brightness_scale;
  gboolean       setting_brightness;
  GtkWidget     *kbd_brightness_row;
  GtkWidget     *kbd_brightness_scale;
  gboolean       kbd_setting_brightness;

  GtkWidget     *automatic_suspend_row;
  GtkWidget     *automatic_suspend_label;

  GDBusProxy    *bt_rfkill;
  GDBusProxy    *bt_properties;
  GtkWidget     *bt_switch;
  GtkWidget     *bt_row;

  GDBusProxy    *iio_proxy;
  guint          iio_proxy_watch_id;
  GtkWidget     *als_switch;
  GtkWidget     *als_row;

#ifdef HAVE_NETWORK_MANAGER
  NMClient      *nm_client;
  GtkWidget     *wifi_switch;
  GtkWidget     *wifi_row;
  GtkWidget     *mobile_switch;
  GtkWidget     *mobile_row;
#endif

  GtkAdjustment *focus_adjustment;
};

CC_PANEL_REGISTER (CcPowerPanel, cc_power_panel)

enum
{
  ACTION_MODEL_TEXT,
  ACTION_MODEL_VALUE
};

static void
cc_power_panel_dispose (GObject *object)
{
  CcPowerPanel *self = CC_POWER_PANEL (object);

  g_clear_pointer (&self->chassis_type, g_free);
  g_clear_object (&self->gsd_settings);
  g_clear_object (&self->session_settings);
  g_clear_pointer (&self->automatic_suspend_dialog, gtk_widget_destroy);
  g_clear_object (&self->screen_proxy);
  g_clear_object (&self->kbd_proxy);
  g_clear_pointer (&self->devices, g_ptr_array_unref);
  g_clear_object (&self->up_client);
  g_clear_object (&self->bt_rfkill);
  g_clear_object (&self->bt_properties);
  g_clear_object (&self->iio_proxy);
#ifdef HAVE_NETWORK_MANAGER
  g_clear_object (&self->nm_client);
#endif
  g_clear_pointer (&self->boxes, g_list_free);
  g_clear_pointer (&self->boxes_reverse, g_list_free);
  if (self->iio_proxy_watch_id != 0)
    g_bus_unwatch_name (self->iio_proxy_watch_id);
  self->iio_proxy_watch_id = 0;

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
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  object_class->dispose = cc_power_panel_dispose;

  panel_class->get_help_uri = cc_power_panel_get_help_uri;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/power/cc-power-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, main_scroll);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, main_box);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, vbox_power);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, automatic_suspend_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_delay_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_battery_delay_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_label);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, suspend_on_ac_delay_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, automatic_suspend_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, liststore_idle_time);
  gtk_widget_class_bind_template_child (widget_class, CcPowerPanel, liststore_power_button);
}

static GtkWidget *
no_prelight_row_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_LIST_BOX_ROW,
                                     "selectable", FALSE,
                                     "activatable", FALSE,
                                     NULL);
}

static GtkWidget *
row_box_new (void)
{
  return (GtkWidget *) g_object_new (GTK_TYPE_BOX,
                                     "margin-end", 12,
                                     "margin-start", 12,
                                     "spacing", 12,
                                     "visible", TRUE,
                                     NULL);
}

static GtkWidget *
row_title_new (const gchar  *title,
               const gchar  *subtitle,
               GtkWidget   **title_label)
{
  PangoAttrList *attributes;
  GtkWidget *box, *label;

  box = (GtkWidget *) g_object_new (GTK_TYPE_BOX,
                                    "spacing", 4,
                                    "margin-bottom", 6,
                                    "margin-top", 6,
                                    "orientation", GTK_ORIENTATION_VERTICAL,
                                    "valign", GTK_ALIGN_CENTER,
                                    "visible", TRUE,
                                    NULL);

  label = (GtkWidget *) g_object_new (GTK_TYPE_LABEL,
                                      "ellipsize", PANGO_ELLIPSIZE_END,
                                      "halign", GTK_ALIGN_START,
                                      "label", title,
                                      "use-markup", TRUE,
                                      "use-underline", TRUE,
                                      "visible", TRUE,
                                      "xalign", 0.0,
                                      NULL);
  if (title_label)
    *title_label = label;
  gtk_container_add (GTK_CONTAINER (box), label);

  if (subtitle == NULL)
    return box;

  attributes = pango_attr_list_new ();
  pango_attr_list_insert (attributes, pango_attr_scale_new (0.9));

  label = (GtkWidget *) g_object_new (GTK_TYPE_LABEL,
                                      "ellipsize", PANGO_ELLIPSIZE_END,
                                      "halign", GTK_ALIGN_START,
                                      "label", subtitle,
                                      "use-markup", TRUE,
                                      "use-underline", TRUE,
                                      "visible", TRUE,
                                      "xalign", 0.0,
                                      "attributes", attributes,
                                      NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (label),
                               GTK_STYLE_CLASS_DIM_LABEL);
  gtk_container_add (GTK_CONTAINER (box), label);

  pango_attr_list_unref (attributes);

  return box;
}

static char *
get_chassis_type (GCancellable *cancellable)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) inner = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GDBusConnection) connection = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                               cancellable,
                               &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("system bus not available: %s", error->message);
      return NULL;
    }

  variant = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.hostname1",
                                         "/org/freedesktop/hostname1",
                                         "org.freedesktop.DBus.Properties",
                                         "Get",
                                         g_variant_new ("(ss)",
                                                        "org.freedesktop.hostname1",
                                                        "Chassis"),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         cancellable,
                                         &error);
  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Failed to get property '%s': %s", "Chassis", error->message);
      return NULL;
    }

  g_variant_get (variant, "(v)", &inner);
  return g_variant_dup_string (inner, NULL);
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

static gchar *
get_details_string (gdouble percentage, UpDeviceState state, guint64 time)
{
  gchar *details;

  if (time > 0)
    {
      g_autofree gchar *time_string = NULL;

      time_string = get_timestring (time);
      switch (state)
        {
          case UP_DEVICE_STATE_CHARGING:
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
          case UP_DEVICE_STATE_FULLY_CHARGED:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Fully charged"));
            break;
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Not charging"));
            break;
          case UP_DEVICE_STATE_EMPTY:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Empty"));
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
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Charging"));
            break;
          case UP_DEVICE_STATE_DISCHARGING:
          case UP_DEVICE_STATE_PENDING_DISCHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Discharging"));
            break;
          case UP_DEVICE_STATE_FULLY_CHARGED:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Fully charged"));
            break;
          case UP_DEVICE_STATE_PENDING_CHARGE:
            /* TRANSLATORS: primary battery */
            details = g_strdup (_("Not charging"));
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

  return details;
}

static void
set_primary (CcPowerPanel *panel, UpDevice *device)
{
  g_autofree gchar *details = NULL;
  gdouble percentage;
  guint64 time_empty, time_full, time;
  UpDeviceState state;
  GtkWidget *box, *box2, *label;
  GtkWidget *levelbar, *row;
  g_autofree gchar *s = NULL;
  gdouble energy_full, energy_rate;

  g_object_get (device,
                "state", &state,
                "percentage", &percentage,
                "time-to-empty", &time_empty,
                "time-to-full", &time_full,
                "energy-full", &energy_full,
                "energy-rate", &energy_rate,
                NULL);
  if (state == UP_DEVICE_STATE_DISCHARGING)
    time = time_empty;
  else
    time = time_full;

  /* Sometimes the reported state is fully charged but battery is at 99%,
     refusing to reach 100%. In these cases, just assume 100%. */
  if (state == UP_DEVICE_STATE_FULLY_CHARGED && (100.0 - percentage <= 1.0))
    percentage = 100.0;

  details = get_details_string (percentage, state, time);

  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (row), box);

  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 16);
  gtk_widget_set_margin_bottom (box, 14);

  levelbar = gtk_level_bar_new ();
  gtk_widget_show (levelbar);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (levelbar), percentage / 100.0);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (levelbar), GTK_LEVEL_BAR_OFFSET_LOW, 0.03);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (levelbar), GTK_LEVEL_BAR_OFFSET_HIGH, 0.1);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (levelbar), GTK_LEVEL_BAR_OFFSET_FULL, 0.8);
  gtk_widget_set_hexpand (levelbar, TRUE);
  gtk_widget_set_halign (levelbar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (levelbar, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), levelbar, TRUE, TRUE, 0);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box2);
  gtk_box_pack_start (GTK_BOX (box), box2, FALSE, TRUE, 0);

  label = gtk_label_new (details);
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_pack_start (GTK_BOX (box2), label, TRUE, TRUE, 0);

  s = g_strdup_printf ("%d%%", (int)(percentage + 0.5));
  label = gtk_label_new (s);
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);

  atk_object_add_relationship (gtk_widget_get_accessible (levelbar),
                               ATK_RELATION_LABELLED_BY,
                               gtk_widget_get_accessible (label));

  gtk_container_add (GTK_CONTAINER (panel->battery_list), row);
  gtk_size_group_add_widget (panel->battery_row_sizegroup, row);

  g_object_set_data (G_OBJECT (row), "primary", GINT_TO_POINTER (TRUE));

  gtk_widget_set_visible (panel->battery_section, TRUE);
}

static void
add_battery (CcPowerPanel *panel, UpDevice *device)
{
  gdouble percentage;
  UpDeviceKind kind;
  UpDeviceState state;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *box2;
  GtkWidget *label;
  GtkWidget *title;
  GtkWidget *levelbar;
  GtkWidget *widget;
  g_autofree gchar *s = NULL;
  g_autofree gchar *icon_name = NULL;
  const gchar *name;

  g_object_get (device,
                "kind", &kind,
                "state", &state,
                "percentage", &percentage,
                "icon-name", &icon_name,
                NULL);

  if (g_object_get_data (G_OBJECT (device), "is-main-battery") != NULL)
    name = C_("Battery name", "Main");
  else
    name = C_("Battery name", "Extra");

  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_box_set_spacing (GTK_BOX (box), 10);
  gtk_container_add (GTK_CONTAINER (row), box);

  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 12);
  gtk_widget_set_margin_top (box, 16);
  gtk_widget_set_margin_bottom (box, 14);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box2);
  title = row_title_new (name, NULL, NULL);
  gtk_size_group_add_widget (panel->battery_sizegroup, box2);
  gtk_box_pack_start (GTK_BOX (box2), title, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), box2, FALSE, TRUE, 0);

#if 1
  if (icon_name != NULL && *icon_name != '\0')
    {
      widget = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
      gtk_widget_show (widget);
      gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_DIM_LABEL);
      gtk_widget_set_halign (widget, GTK_ALIGN_END);
      gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (box2), widget, TRUE, TRUE, 0);
    }
#endif

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box2);

  s = g_strdup_printf ("%d%%", (int)percentage);
  label = gtk_label_new (s);
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), label, FALSE, TRUE, 0);
  gtk_size_group_add_widget (panel->charge_sizegroup, label);

  levelbar = gtk_level_bar_new ();
  gtk_widget_show (levelbar);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (levelbar), percentage / 100.0);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (levelbar), GTK_LEVEL_BAR_OFFSET_LOW, 0.05);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (levelbar), GTK_LEVEL_BAR_OFFSET_HIGH, 0.1);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (levelbar), GTK_LEVEL_BAR_OFFSET_FULL, 0.8);
  gtk_widget_set_hexpand (levelbar, TRUE);
  gtk_widget_set_halign (levelbar, GTK_ALIGN_FILL);
  gtk_widget_set_valign (levelbar, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box2), levelbar, TRUE, TRUE, 0);
  gtk_size_group_add_widget (panel->level_sizegroup, levelbar);
  gtk_box_pack_start (GTK_BOX (box), box2, TRUE, TRUE, 0);

  atk_object_add_relationship (gtk_widget_get_accessible (levelbar),
                               ATK_RELATION_LABELLED_BY,
                               gtk_widget_get_accessible (label));


  g_object_set_data (G_OBJECT (row), "kind", GINT_TO_POINTER (kind));
  gtk_container_add (GTK_CONTAINER (panel->battery_list), row);
  gtk_size_group_add_widget (panel->battery_row_sizegroup, row);

  gtk_widget_set_visible (panel->battery_section, TRUE);
}

static const char *
kind_to_description (UpDeviceKind kind)
{
  switch (kind)
    {
      case UP_DEVICE_KIND_MOUSE:
        /* TRANSLATORS: secondary battery */
        return N_("Wireless mouse");
      case UP_DEVICE_KIND_KEYBOARD:
        /* TRANSLATORS: secondary battery */
        return N_("Wireless keyboard");
      case UP_DEVICE_KIND_UPS:
        /* TRANSLATORS: secondary battery */
        return N_("Uninterruptible power supply");
      case UP_DEVICE_KIND_PDA:
        /* TRANSLATORS: secondary battery */
        return N_("Personal digital assistant");
      case UP_DEVICE_KIND_PHONE:
        /* TRANSLATORS: secondary battery */
        return N_("Cellphone");
      case UP_DEVICE_KIND_MEDIA_PLAYER:
        /* TRANSLATORS: secondary battery */
        return N_("Media player");
      case UP_DEVICE_KIND_TABLET:
        /* TRANSLATORS: secondary battery */
        return N_("Tablet");
      case UP_DEVICE_KIND_COMPUTER:
        /* TRANSLATORS: secondary battery */
        return N_("Computer");
      case UP_DEVICE_KIND_GAMING_INPUT:
        /* TRANSLATORS: secondary battery */
        return N_("Gaming input device");
      default:
        /* TRANSLATORS: secondary battery, misc */
        return N_("Battery");
    }

  g_assert_not_reached ();
}

static UpDeviceLevel
get_battery_level (UpDevice *device)
{
  UpDeviceLevel battery_level;

  if (!g_object_class_find_property (G_OBJECT_CLASS (G_OBJECT_GET_CLASS (device)), "battery-level"))
    return UP_DEVICE_LEVEL_NONE;

  g_object_get (device, "battery-level", &battery_level, NULL);
  return battery_level;
}

static void
add_device (CcPowerPanel *panel, UpDevice *device)
{
  UpDeviceKind kind;
  UpDeviceState state;
  GtkWidget *row;
  GtkWidget *hbox;
  GtkWidget *box2;
  GtkWidget *widget;
  GtkWidget *title;
  g_autoptr(GString) status = NULL;
  g_autoptr(GString) description = NULL;
  gdouble percentage;
  g_autofree gchar *name = NULL;
  gboolean show_caution = FALSE;
  gboolean is_present;
  UpDeviceLevel battery_level;

  g_object_get (device,
                "kind", &kind,
                "percentage", &percentage,
                "state", &state,
                "model", &name,
                "is-present", &is_present,
                NULL);
  battery_level = get_battery_level (device);

  if (!is_present)
    return;

  if (kind == UP_DEVICE_KIND_UPS)
    show_caution = TRUE;

  if (name == NULL || *name == '\0')
    description = g_string_new (_(kind_to_description (kind)));
  else
    description = g_string_new (name);

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
        status = g_string_new (C_("Battery power", "Fully charged"));
        break;
      case UP_DEVICE_STATE_EMPTY:
        /* TRANSLATORS: primary battery */
        status = g_string_new (C_("Battery power", "Empty"));
        break;
      default:
        status = g_string_new (up_device_state_to_string (state));
        break;
    }
  g_string_prepend (status, "<small>");
  g_string_append (status, "</small>");

  /* create the new widget */
  row = no_prelight_row_new ();
  gtk_widget_show (row);
  hbox = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), hbox);
  title = row_title_new (description->str, NULL, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), title, FALSE, TRUE, 0);
  gtk_size_group_add_widget (panel->battery_sizegroup, title);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box2);

  if (battery_level == UP_DEVICE_LEVEL_NONE)
    {
      g_autofree gchar *s = NULL;

      s = g_strdup_printf ("%d%%", (int)(percentage + 0.5));
      widget = gtk_label_new (s);
    }
  else
    {
      widget = gtk_label_new ("");
    }

  gtk_widget_show (widget);
  gtk_widget_set_halign (widget, GTK_ALIGN_END);
  gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), GTK_STYLE_CLASS_DIM_LABEL);
  gtk_box_pack_start (GTK_BOX (box2), widget, FALSE, TRUE, 0);
  gtk_size_group_add_widget (panel->charge_sizegroup, widget);

  widget = gtk_level_bar_new ();
  gtk_widget_show (widget);
  gtk_widget_set_halign (widget, TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_FILL);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), percentage / 100.0f);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (widget), GTK_LEVEL_BAR_OFFSET_LOW, 0.03);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (widget), GTK_LEVEL_BAR_OFFSET_HIGH, 0.1);
  gtk_level_bar_add_offset_value (GTK_LEVEL_BAR (widget), GTK_LEVEL_BAR_OFFSET_FULL, 0.8);
  gtk_box_pack_start (GTK_BOX (box2), widget, TRUE, TRUE, 0);
  gtk_size_group_add_widget (panel->level_sizegroup, widget);
  gtk_box_pack_start (GTK_BOX (hbox), box2, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (panel->device_list), row);
  gtk_size_group_add_widget (panel->row_sizegroup, row);
  g_object_set_data (G_OBJECT (row), "kind", GINT_TO_POINTER (kind));

  gtk_widget_set_visible (panel->device_section, TRUE);
}

static void
up_client_changed (UpClient     *client,
                   UpDevice     *device,
                   CcPowerPanel *self)
{
  g_autoptr(GList) battery_children = NULL;
  g_autoptr(GList) device_children = NULL;
  GList *l;
  gint i;
  UpDeviceKind kind;
  guint n_batteries;
  gboolean on_ups;
  g_autoptr(UpDevice) composite = NULL;
  g_autofree gchar *s = NULL;

  battery_children = gtk_container_get_children (GTK_CONTAINER (self->battery_list));
  for (l = battery_children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (self->battery_list), l->data);
  gtk_widget_hide (self->battery_section);

  device_children = gtk_container_get_children (GTK_CONTAINER (self->device_list));
  for (l = device_children; l != NULL; l = l->next)
    gtk_container_remove (GTK_CONTAINER (self->device_list), l->data);
  gtk_widget_hide (self->device_section);

#ifdef TEST_FAKE_DEVICES
  {
    static gboolean fake_devices_added = FALSE;

    if (!fake_devices_added)
      {
        fake_devices_added = TRUE;
        g_print ("adding fake devices\n");
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_MOUSE,
                      "native-path", "dummy:native-path1",
                      "model", "My mouse",
                      "percentage", 71.0,
                      "state", UP_DEVICE_STATE_DISCHARGING,
                      "time-to-empty", 287,
                      "icon-name", "battery-full-symbolic",
                      NULL);
        g_ptr_array_add (self->devices, device);
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_KEYBOARD,
                      "native-path", "dummy:native-path2",
                      "model", "My keyboard",
                      "percentage", 59.0,
                      "state", UP_DEVICE_STATE_DISCHARGING,
                      "time-to-empty", 250,
                      "icon-name", "battery-good-symbolic",
                      NULL);
        g_ptr_array_add (self->devices, device);
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_BATTERY,
                      "native-path", "dummy:native-path3",
                      "model", "Battery from some factory",
                      "percentage", 100.0,
                      "state", UP_DEVICE_STATE_FULLY_CHARGED,
                      "energy", 55.0,
                      "energy-full", 55.0,
                      "energy-rate", 15.0,
                      "time-to-empty", 400,
                      "icon-name", "battery-full-charged-symbolic",
                      NULL);
        g_ptr_array_add (self->devices, device);
      }
  }
#endif

#ifdef TEST_UPS
  {
    static gboolean fake_devices_added = FALSE;

    if (!fake_devices_added)
      {
        fake_devices_added = TRUE;
        g_print ("adding fake UPS\n");
        device = up_device_new ();
        g_object_set (device,
                      "kind", UP_DEVICE_KIND_UPS,
                      "native-path", "dummy:usb-hiddev0",
                      "model", "APC UPS",
                      "percentage", 70.0,
                      "state", UP_DEVICE_STATE_DISCHARGING,
                      "is-present", TRUE,
                      "power-supply", TRUE,
                      "battery-level", UP_DEVICE_LEVEL_NONE,
                      NULL);
        g_ptr_array_add (self->devices, device);
      }
  }
#endif

  on_ups = FALSE;
  n_batteries = 0;
  composite = up_client_get_display_device (self->up_client);
  g_object_get (composite, "kind", &kind, NULL);
  if (kind == UP_DEVICE_KIND_UPS)
    {
      on_ups = TRUE;
    }
  else
    {
      gboolean is_extra_battery = FALSE;

      /* Count the batteries */
      for (i = 0; self->devices != NULL && i < self->devices->len; i++)
        {
          UpDevice *device = (UpDevice*) g_ptr_array_index (self->devices, i);
          gboolean is_power_supply = FALSE;
          g_object_get (device,
                        "kind", &kind,
                        "power-supply", &is_power_supply,
                        NULL);
          if (kind == UP_DEVICE_KIND_BATTERY &&
              is_power_supply)
            {
              n_batteries++;
              if (is_extra_battery == FALSE)
                {
                  is_extra_battery = TRUE;
                  g_object_set_data (G_OBJECT (device), "is-main-battery", GINT_TO_POINTER(TRUE));
                }
            }
        }
    }

  if (n_batteries > 1)
    s = g_strdup_printf ("<b>%s</b>", _("Batteries"));
  else
    s = g_strdup_printf ("<b>%s</b>", _("Battery"));
  gtk_label_set_label (GTK_LABEL (self->battery_heading), s);

  if (!on_ups && n_batteries > 1)
    set_primary (self, composite);

  for (i = 0; self->devices != NULL && i < self->devices->len; i++)
    {
      UpDevice *device = (UpDevice*) g_ptr_array_index (self->devices, i);
      gboolean is_power_supply = FALSE;
      g_object_get (device,
                    "kind", &kind,
                    "power-supply", &is_power_supply,
                    NULL);
      if (kind == UP_DEVICE_KIND_LINE_POWER)
        {
          /* do nothing */
        }
      else if (kind == UP_DEVICE_KIND_UPS && on_ups)
        {
          set_primary (self, device);
        }
      else if (kind == UP_DEVICE_KIND_BATTERY && is_power_supply && !on_ups && n_batteries == 1)
        {
          set_primary (self, device);
        }
      else if (kind == UP_DEVICE_KIND_BATTERY && is_power_supply)
        {
          add_battery (self, device);
        }
      else
        {
          add_device (self, device);
        }
    }
}

static void
up_client_device_removed (UpClient     *client,
                          const char   *object_path,
                          CcPowerPanel *self)
{
  guint i;

  if (self->devices == NULL)
    return;

  for (i = 0; i < self->devices->len; i++)
    {
      UpDevice *device = g_ptr_array_index (self->devices, i);

      if (g_strcmp0 (object_path, up_device_get_object_path (device)) == 0)
        {
          g_ptr_array_remove_index (self->devices, i);
          break;
        }
    }

  up_client_changed (self->up_client, NULL, self);
}

static void
up_client_device_added (UpClient     *client,
                        UpDevice     *device,
                        CcPowerPanel *self)
{
  g_ptr_array_add (self->devices, g_object_ref (device));
  g_signal_connect (G_OBJECT (device), "notify",
                    G_CALLBACK (up_client_changed), self);
  up_client_changed (self->up_client, NULL, self);
}

static void
set_brightness_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) result = NULL;
  GDBusProxy *proxy = G_DBUS_PROXY (source_object);

  /* not setting, so pay attention to changed signals */
  if (proxy == self->screen_proxy)
    self->setting_brightness = FALSE;
  else if (proxy == self->kbd_proxy)
    self->kbd_setting_brightness = FALSE;

  result = g_dbus_proxy_call_finish (proxy, res, &error);
  if (result == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error setting brightness: %s\n", error->message);
      return;
    }
}

static void
brightness_slider_value_changed_cb (GtkRange *range, gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  guint percentage;
  g_autoptr(GVariant) variant = NULL;
  GDBusProxy *proxy;

  percentage = (guint) gtk_range_get_value (range);

  if (range == GTK_RANGE (self->brightness_scale))
    {
      /* do not loop */
      if (self->setting_brightness)
        return;

      self->setting_brightness = TRUE;
      proxy = self->screen_proxy;

      variant = g_variant_new_parsed ("('org.gnome.SettingsDaemon.Power.Screen',"
                                      "'Brightness', %v)",
                                      g_variant_new_int32 (percentage));
    }
  else
    {
      /* do not loop */
      if (self->kbd_setting_brightness)
        return;

      self->kbd_setting_brightness = TRUE;
      proxy = self->kbd_proxy;

      variant = g_variant_new_parsed ("('org.gnome.SettingsDaemon.Power.Keyboard',"
                                      "'Brightness', %v)",
                                      g_variant_new_int32 (percentage));
    }

  /* push this to g-s-d */
  g_dbus_proxy_call (proxy,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_ref_sink (variant),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     set_brightness_cb,
                     user_data);
}

static void
sync_kbd_brightness (CcPowerPanel *self)
{
  g_autoptr(GVariant) result = NULL;
  gint brightness;
  gboolean visible;
  GtkRange *range;

  result = g_dbus_proxy_get_cached_property (self->kbd_proxy, "Brightness");
  if (result)
    {
      /* set the slider */
      brightness = g_variant_get_int32 (result);
      visible = brightness >= 0.0;
    }
  else
    {
      visible = FALSE;
    }

  gtk_widget_set_visible (self->kbd_brightness_row, visible);

  if (visible)
    {
      range = GTK_RANGE (self->kbd_brightness_scale);
      gtk_range_set_range (range, 0, 100);
      gtk_range_set_increments (range, 1, 10);
      self->kbd_setting_brightness = TRUE;
      gtk_range_set_value (range, brightness);
      self->kbd_setting_brightness = FALSE;
    }
}

static void
sync_screen_brightness (CcPowerPanel *self)
{
  g_autoptr(GVariant) result = NULL;
  gint brightness;
  gboolean visible;
  GtkRange *range;

  result = g_dbus_proxy_get_cached_property (self->screen_proxy, "Brightness");

  if (result)
    {
      /* set the slider */
      brightness = g_variant_get_int32 (result);
      visible = brightness >= 0.0;
    }
  else
    {
      visible = FALSE;
    }

  gtk_widget_set_visible (self->brightness_row, visible);
  gtk_widget_set_visible (self->dim_screen_row, visible);

  if (visible)
    {
      range = GTK_RANGE (self->brightness_scale);
      gtk_range_set_range (range, 0, 100);
      gtk_range_set_increments (range, 1, 10);
      self->setting_brightness = TRUE;
      gtk_range_set_value (range, brightness);
      self->setting_brightness = FALSE;
    }
}

static void
als_switch_changed (GtkSwitch    *sw,
                    GParamSpec   *pspec,
                    CcPowerPanel *panel)
{
  gboolean enabled;
  enabled = gtk_switch_get_active (sw);
  g_debug ("Setting ALS enabled %s", enabled ? "on" : "off");
  g_settings_set_boolean (panel->gsd_settings, "ambient-enabled", enabled);
}

static void
als_enabled_state_changed (CcPowerPanel *self)
{
  gboolean enabled;
  gboolean has_brightness = FALSE;
  gboolean visible = FALSE;

  if (self->screen_proxy != NULL)
    {
      g_autoptr(GVariant) v = g_dbus_proxy_get_cached_property (self->screen_proxy, "Brightness");
      if (v != NULL)
        has_brightness = g_variant_get_int32 (v) >= 0.0;
    }

  if (self->iio_proxy != NULL)
    {
      g_autoptr(GVariant) v = g_dbus_proxy_get_cached_property (self->iio_proxy, "HasAmbientLight");
      if (v != NULL)
        visible = g_variant_get_boolean (v);
    }

  enabled = g_settings_get_boolean (self->gsd_settings, "ambient-enabled");
  g_debug ("ALS enabled: %s", enabled ? "on" : "off");
  g_signal_handlers_block_by_func (self->als_switch, als_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (self->als_switch), enabled);
  gtk_widget_set_visible (self->als_row, visible && has_brightness);
  g_signal_handlers_unblock_by_func (self->als_switch, als_switch_changed, self);
}

static void
on_screen_property_change (GDBusProxy *proxy,
                           GVariant   *changed_properties,
                           GVariant   *invalidated_properties,
                           gpointer    user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  sync_screen_brightness (self);
}

static void
got_screen_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  CcPowerPanel *self;
  GDBusProxy *screen_proxy;

  screen_proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (screen_proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating screen proxy: %s\n", error->message);
      return;
    }

  self = CC_POWER_PANEL (user_data);
  self->screen_proxy = screen_proxy;

  /* we want to change the bar if the user presses brightness buttons */
  g_signal_connect_object (screen_proxy, "g-properties-changed",
                           G_CALLBACK (on_screen_property_change), self, 0);

  sync_screen_brightness (self);
  als_enabled_state_changed (self);
}

static void
on_kbd_property_change (GDBusProxy *proxy,
                        GVariant   *changed_properties,
                        GVariant   *invalidated_properties,
                        gpointer    user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  sync_kbd_brightness (self);
}

static void
got_kbd_proxy_cb (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  CcPowerPanel *self;
  g_autoptr(GError) error = NULL;
  GDBusProxy *kbd_proxy;

  kbd_proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (kbd_proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating keyboard proxy: %s\n", error->message);
      return;
    }

  self = CC_POWER_PANEL (user_data);
  self->kbd_proxy = kbd_proxy;

  /* we want to change the bar if the user presses brightness buttons */
  g_signal_connect_object (kbd_proxy, "g-properties-changed",
                           G_CALLBACK (on_kbd_property_change), self, 0);

  sync_kbd_brightness (self);
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
  g_settings_set_int (self->gsd_settings, key, value);
}

static void
set_value_for_combo (GtkComboBox *combo_box, gint value)
{
  GtkTreeIter iter;
  g_autoptr(GtkTreeIter) insert = NULL;
  GtkTreeIter new;
  GtkTreeModel *model;
  gint value_tmp;
  gint value_last = 0;
  g_autofree gchar *text = NULL;
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
      if (value_tmp == value)
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          return;
        }

      /* Insert before if the next value is larger or the value is lower
       * again (i.e. "Never" is zero and last). */
      if (!insert && (value_tmp > value || value_last > value_tmp))
        insert = gtk_tree_iter_copy (&iter);

      value_last = value_tmp;
    } while (gtk_tree_model_iter_next (model, &iter));

  /* The value is not listed, so add it at the best point (or the end). */
  gtk_list_store_insert_before (GTK_LIST_STORE (model), &new, insert);

  text = cc_util_time_to_string_text (value * 1000);
  gtk_list_store_set (GTK_LIST_STORE (model), &new,
                      ACTION_MODEL_TEXT, text,
                      ACTION_MODEL_VALUE, value,
                      -1);
  gtk_combo_box_set_active_iter (combo_box, &new);
}

static void
set_ac_battery_ui_mode (CcPowerPanel *self)
{
  gboolean has_batteries = FALSE;
  GPtrArray *devices;
  guint i;

  devices = up_client_get_devices2 (self->up_client);
  g_debug ("got %d devices from upower\n", devices ? devices->len : 0);

  for (i = 0; devices != NULL && i < devices->len; i++)
    {
      UpDevice *device;
      gboolean is_power_supply;
      UpDeviceKind kind;

      device = g_ptr_array_index (devices, i);
      g_object_get (device,
                    "kind", &kind,
                    "power-supply", &is_power_supply,
                    NULL);
      if (kind == UP_DEVICE_KIND_UPS ||
          (kind == UP_DEVICE_KIND_BATTERY && is_power_supply))
        {
          has_batteries = TRUE;
          break;
        }
    }
  g_clear_pointer (&devices, g_ptr_array_unref);

#ifdef TEST_NO_BATTERIES
  g_print ("forcing no batteries\n");
  has_batteries = FALSE;
#endif

  self->has_batteries = has_batteries;

  if (!has_batteries)
    {
      gtk_widget_hide (self->suspend_on_battery_switch);
      gtk_widget_hide (self->suspend_on_battery_label);
      gtk_widget_hide (self->suspend_on_battery_delay_label);
      gtk_widget_hide (self->suspend_on_battery_delay_combo);
      gtk_label_set_label (GTK_LABEL (self->suspend_on_ac_label),
                           _("When _idle"));
    }
}

static void
bt_set_powered (CcPowerPanel *self,
                gboolean      powered)
{
  g_dbus_proxy_call (self->bt_properties,
		     "Set",
		     g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', %v)",
					   g_variant_new_boolean (!powered)),
		     G_DBUS_CALL_FLAGS_NONE,
		     -1,
		     cc_panel_get_cancellable (CC_PANEL (self)),
		     NULL, NULL);
}

static void
bt_switch_changed (GtkSwitch    *sw,
                   GParamSpec   *pspec,
                   CcPowerPanel *panel)
{
  gboolean powered;

  powered = gtk_switch_get_active (sw);

  g_debug ("Setting bt power %s", powered ? "on" : "off");

  bt_set_powered (panel, powered);
}

static void
bt_powered_state_changed (CcPowerPanel *panel)
{
  gboolean powered, has_airplane_mode;
  g_autoptr(GVariant) v1 = NULL;
  g_autoptr(GVariant) v2 = NULL;

  v1 = g_dbus_proxy_get_cached_property (panel->bt_rfkill, "BluetoothHasAirplaneMode");
  has_airplane_mode = g_variant_get_boolean (v1);

  if (!has_airplane_mode)
    {
      g_debug ("BluetoothHasAirplaneMode is false, hiding Bluetooth power row");
      gtk_widget_hide (panel->bt_row);
      return;
    }

  v2 = g_dbus_proxy_get_cached_property (panel->bt_rfkill, "BluetoothAirplaneMode");
  powered = !g_variant_get_boolean (v2);

  g_debug ("bt powered state changed to %s", powered ? "on" : "off");

  gtk_widget_show (panel->bt_row);

  g_signal_handlers_block_by_func (panel->bt_switch, bt_switch_changed, panel);
  gtk_switch_set_active (GTK_SWITCH (panel->bt_switch), powered);
  g_signal_handlers_unblock_by_func (panel->bt_switch, bt_switch_changed, panel);
}

#ifdef HAVE_NETWORK_MANAGER
static gboolean
has_wifi_devices (NMClient *client)
{
  const GPtrArray *devices;
  NMDevice *device;
  gint i;

  if (!nm_client_get_nm_running (client))
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
  nm_client_wireless_set_enabled (panel->nm_client, enabled);
}

static gboolean
has_mobile_devices (NMClient *client)
{
  const GPtrArray *devices;
  NMDevice *device;
  gint i;

  if (!nm_client_get_nm_running (client))
    return FALSE;

  devices = nm_client_get_devices (client);
  if (devices == NULL)
    return FALSE;

  for (i = 0; i < devices->len; i++)
    {
      device = g_ptr_array_index (devices, i);
      switch (nm_device_get_device_type (device))
        {
        case NM_DEVICE_TYPE_WIMAX:
        case NM_DEVICE_TYPE_MODEM:
          return TRUE;
        default:
          break;
        }
    }

  return FALSE;
}

static void
mobile_switch_changed (GtkSwitch    *sw,
                       GParamSpec   *pspec,
                       CcPowerPanel *panel)
{
  gboolean enabled;

  enabled = gtk_switch_get_active (sw);
  g_debug ("Setting wwan %s", enabled ? "enabled" : "disabled");
  nm_client_wwan_set_enabled (panel->nm_client, enabled);
  g_debug ("Setting wimax %s", enabled ? "enabled" : "disabled");
  nm_client_wimax_set_enabled (panel->nm_client, enabled);
}

static void
nm_client_state_changed (NMClient     *client,
                         GParamSpec   *pspec,
                         CcPowerPanel *self)
{
  gboolean visible;
  gboolean active;
  gboolean sensitive;

  visible = has_wifi_devices (self->nm_client);
  active = nm_client_networking_get_enabled (client) &&
           nm_client_wireless_get_enabled (client) &&
           nm_client_wireless_hardware_get_enabled (client);
  sensitive = nm_client_networking_get_enabled (client) &&
              nm_client_wireless_hardware_get_enabled (client);

  g_debug ("wifi state changed to %s", active ? "enabled" : "disabled");

  g_signal_handlers_block_by_func (self->wifi_switch, wifi_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (self->wifi_switch), active);
  gtk_widget_set_sensitive (self->wifi_switch, sensitive);
  gtk_widget_set_visible (self->wifi_row, visible);
  g_signal_handlers_unblock_by_func (self->wifi_switch, wifi_switch_changed, self);

  visible = has_mobile_devices (self->nm_client);

  /* Set the switch active, if either of wimax or wwan is enabled. */
  active = nm_client_networking_get_enabled (client) &&
           ((nm_client_wimax_get_enabled (client) &&
             nm_client_wimax_hardware_get_enabled (client)) ||
            (nm_client_wwan_get_enabled (client) &&
             nm_client_wwan_hardware_get_enabled (client)));
  sensitive = nm_client_networking_get_enabled (client) &&
              (nm_client_wwan_hardware_get_enabled (client) ||
               nm_client_wimax_hardware_get_enabled (client));

  g_debug ("mobile state changed to %s", active ? "enabled" : "disabled");

  g_signal_handlers_block_by_func (self->mobile_switch, mobile_switch_changed, self);
  gtk_switch_set_active (GTK_SWITCH (self->mobile_switch), active);
  gtk_widget_set_sensitive (self->mobile_switch, sensitive);
  gtk_widget_set_visible (self->mobile_row, visible);
  g_signal_handlers_unblock_by_func (self->mobile_switch, mobile_switch_changed, self);
}

static void
nm_device_changed (NMClient     *client,
                   NMDevice     *device,
                   CcPowerPanel *self)
{
  gtk_widget_set_visible (self->wifi_row, has_wifi_devices (self->nm_client));
  gtk_widget_set_visible (self->mobile_row, has_mobile_devices (self->nm_client));
}

static void
setup_nm_client (CcPowerPanel *self,
                 NMClient     *client)
{
  self->nm_client = client;

  g_signal_connect_object (self->nm_client, "notify",
                           G_CALLBACK (nm_client_state_changed), self, 0);
  g_signal_connect_object (self->nm_client, "device-added",
                           G_CALLBACK (nm_device_changed), self, 0);
  g_signal_connect_object (self->nm_client, "device-removed",
                           G_CALLBACK (nm_device_changed), self, 0);

  nm_client_state_changed (self->nm_client, NULL, self);
  nm_device_changed (self->nm_client, NULL, self);
}

static void
nm_client_ready_cb (GObject *source_object,
                    GAsyncResult *res,
                    gpointer user_data)
{
  CcPowerPanel *self;
  NMClient *client;
  g_autoptr(GError) error = NULL;

  client = nm_client_new_finish (res, &error);
  if (!client)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to create NetworkManager client: %s",
                     error->message);

          self = user_data;
          gtk_widget_set_sensitive (self->wifi_row, FALSE);
          gtk_widget_set_sensitive (self->mobile_row, FALSE);
        }
      return;
    }

  self = user_data;

  /* Setup the client */
  setup_nm_client (self, client);

  /* Store the object in the cache too */
  cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
}

#endif

static gboolean
keynav_failed (GtkWidget *list, GtkDirectionType direction, CcPowerPanel *self)
{
  GtkWidget *next_list = NULL;
  GList *item, *boxes_list;
  gdouble value, lower, upper, page;

  /* Find the list in the list of GtkListBoxes */
  if (direction == GTK_DIR_DOWN)
    boxes_list = self->boxes;
  else
    boxes_list = self->boxes_reverse;

  item = g_list_find (boxes_list, list);
  g_assert (item);
  item = item->next;
  while (1)
    {
      if (item == NULL)
        item = boxes_list;

      /* Avoid looping */
      if (item->data == list)
        break;

      if (gtk_widget_is_visible (item->data))
        {
          next_list = item->data;
          break;
        }

    item = item->next;
  }

  if (next_list)
    {
      gtk_widget_child_focus (next_list, direction);
      return TRUE;
    }

  value = gtk_adjustment_get_value (self->focus_adjustment);
  lower = gtk_adjustment_get_lower (self->focus_adjustment);
  upper = gtk_adjustment_get_upper (self->focus_adjustment);
  page  = gtk_adjustment_get_page_size (self->focus_adjustment);

  if (direction == GTK_DIR_UP && value > lower)
    {
      gtk_adjustment_set_value (self->focus_adjustment, lower);
      return TRUE;
    }
  else if (direction == GTK_DIR_DOWN && value < upper - page)
    {
      gtk_adjustment_set_value (self->focus_adjustment, upper - page);
      return TRUE;
    }

  return FALSE;
}

static void
combo_idle_delay_changed_cb (GtkWidget *widget, CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

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
  g_settings_set_uint (self->session_settings, "idle-delay", value);
}

static void
combo_power_button_changed_cb (GtkWidget *widget, CcPowerPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

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
  g_settings_set_enum (self->gsd_settings, "power-button-action", value);
}

static GtkWidget *
add_brightness_row (CcPowerPanel  *self,
		    const char    *text,
		    GtkWidget    **brightness_scale)
{
  GtkWidget *row, *box, *label, *title, *box2, *w, *scale;

  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (text, NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, FALSE, TRUE, 0);
  gtk_size_group_add_widget (self->battery_sizegroup, title);
  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_show (box2);
  w = gtk_label_new ("");
  gtk_widget_show (w);
  gtk_box_pack_start (GTK_BOX (box2), w, FALSE, TRUE, 0);
  gtk_size_group_add_widget (self->charge_sizegroup, w);

  *brightness_scale = scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_widget_show (scale);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), scale);
  gtk_scale_set_draw_value (GTK_SCALE (scale), FALSE);
  gtk_box_pack_start (GTK_BOX (box2), scale, TRUE, TRUE, 0);
  gtk_size_group_add_widget (self->level_sizegroup, scale);
  gtk_range_set_round_digits (GTK_RANGE (scale), 0);
  g_signal_connect (scale, "value-changed",
                    G_CALLBACK (brightness_slider_value_changed_cb), self);

  gtk_box_pack_start (GTK_BOX (box), box2, TRUE, TRUE, 0);

  return row;
}

static void
als_enabled_setting_changed (GSettings    *settings,
                             const gchar  *key,
                             CcPowerPanel *self)
{
  als_enabled_state_changed (self);
}

static void
iio_proxy_appeared_cb (GDBusConnection *connection,
                       const gchar *name,
                       const gchar *name_owner,
                       gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  g_autoptr(GError) error = NULL;

  self->iio_proxy =
    cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SYSTEM,
                                              G_DBUS_PROXY_FLAGS_NONE,
                                              "net.hadess.SensorProxy",
                                              "/net/hadess/SensorProxy",
                                              "net.hadess.SensorProxy",
                                              NULL, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not create IIO sensor proxy: %s", error->message);
      return;
    }

  g_signal_connect_object (G_OBJECT (self->iio_proxy), "g-properties-changed",
                           G_CALLBACK (als_enabled_state_changed), self,
                           G_CONNECT_SWAPPED);
  als_enabled_state_changed (self);
}

static void
iio_proxy_vanished_cb (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data)
{
  CcPowerPanel *self = CC_POWER_PANEL (user_data);
  g_clear_object (&self->iio_proxy);
  als_enabled_state_changed (self);
}

static void
add_power_saving_section (CcPowerPanel *self)
{
  GtkWidget *widget, *box, *label, *row;
  GtkWidget *combo;
  GtkWidget *title;
  GtkWidget *sw;
  int value;
  g_autofree gchar *s = NULL;

  s = g_strdup_printf ("<b>%s</b>", _("Power Saving"));
  label = gtk_label_new (s);
  gtk_widget_show (label);
  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (label, 12);
  gtk_box_pack_start (GTK_BOX (self->vbox_power), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  widget = gtk_list_box_new ();
  gtk_widget_show (widget);
  self->boxes_reverse = g_list_prepend (self->boxes_reverse, widget);
  g_signal_connect (widget, "keynav-failed", G_CALLBACK (keynav_failed), self);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (label)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (widget)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (widget)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (label)));

  box = gtk_frame_new (NULL);
  gtk_widget_show (box);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (GTK_BOX (self->vbox_power), box, FALSE, TRUE, 0);

  row = add_brightness_row (self, _("_Screen Brightness"), &self->brightness_scale);
  gtk_widget_show (row);
  self->brightness_row = row;

  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  /* ambient light sensor */
  self->iio_proxy_watch_id =
    g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                      "net.hadess.SensorProxy",
                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                      iio_proxy_appeared_cb,
                      iio_proxy_vanished_cb,
                      self, NULL);
  g_signal_connect (self->gsd_settings, "changed",
                    G_CALLBACK (als_enabled_setting_changed), self);
  self->als_row = row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("Automatic Brightness"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->als_switch = gtk_switch_new ();
  gtk_widget_show (self->als_switch);
  gtk_widget_set_valign (self->als_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->als_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->als_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);
  g_signal_connect (G_OBJECT (self->als_switch), "notify::active",
                    G_CALLBACK (als_switch_changed), self);

  row = add_brightness_row (self, _("_Keyboard Brightness"), &self->kbd_brightness_scale);
  gtk_widget_show (row);
  self->kbd_brightness_row = row;

  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  self->dim_screen_row = row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Dim Screen When Inactive"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  sw = gtk_switch_new ();
  gtk_widget_show (sw);
  g_settings_bind (self->gsd_settings, "idle-dim",
                   sw, "active",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), sw, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), sw);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Blank Screen"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  combo = gtk_combo_box_text_new ();
  gtk_widget_show (combo);
  gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (combo), 0);
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo),
                           GTK_TREE_MODEL (self->liststore_idle_time));
  value = g_settings_get_uint (self->session_settings, "idle-delay");
  set_value_for_combo (GTK_COMBO_BOX (combo), value);
  g_signal_connect (combo, "changed",
                    G_CALLBACK (combo_idle_delay_changed_cb), self);
  gtk_widget_set_valign (combo, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), combo, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

#ifdef HAVE_NETWORK_MANAGER
  self->wifi_row = row = no_prelight_row_new ();
  gtk_widget_hide (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Wi-Fi"),
                         _("Wi-Fi can be turned off to save power."),
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->wifi_switch = gtk_switch_new ();
  gtk_widget_show (self->wifi_switch);
  gtk_widget_set_valign (self->wifi_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->wifi_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->wifi_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  self->mobile_row = row = no_prelight_row_new ();
  gtk_widget_hide (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Mobile Broadband"),
                         _("Mobile broadband (LTE, 4G, 3G, etc.) can be turned off to save power."),
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->mobile_switch = gtk_switch_new ();
  gtk_widget_show (self->mobile_switch);
  gtk_widget_set_valign (self->mobile_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->mobile_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->mobile_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);

  g_signal_connect (G_OBJECT (self->mobile_switch), "notify::active",
                    G_CALLBACK (mobile_switch_changed), self);

  /* Create and store a NMClient instance if it doesn't exist yet */
  if (cc_object_storage_has_object (CC_OBJECT_NMCLIENT))
    setup_nm_client (self, cc_object_storage_get_object (CC_OBJECT_NMCLIENT));
  else
    nm_client_new_async (cc_panel_get_cancellable (CC_PANEL (self)), nm_client_ready_cb, self);

  g_signal_connect (G_OBJECT (self->wifi_switch), "notify::active",
                    G_CALLBACK (wifi_switch_changed), self);
#endif

#ifdef HAVE_BLUETOOTH

  self->bt_rfkill = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
                                                              G_DBUS_PROXY_FLAGS_NONE,
                                                              "org.gnome.SettingsDaemon.Rfkill",
                                                              "/org/gnome/SettingsDaemon/Rfkill",
                                                              "org.gnome.SettingsDaemon.Rfkill",
                                                              NULL,
                                                              NULL);

  if (self->bt_rfkill)
    {
      self->bt_properties = cc_object_storage_create_dbus_proxy_sync (G_BUS_TYPE_SESSION,
                                                                      G_DBUS_PROXY_FLAGS_NONE,
                                                                      "org.gnome.SettingsDaemon.Rfkill",
                                                                      "/org/gnome/SettingsDaemon/Rfkill",
                                                                      "org.freedesktop.DBus.Properties",
                                                                      NULL,
                                                                      NULL);
    }

  row = no_prelight_row_new ();
  gtk_widget_hide (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);
  title = row_title_new (_("_Bluetooth"),
                         _("Bluetooth can be turned off to save power."),
                         NULL);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  self->bt_switch = gtk_switch_new ();
  gtk_widget_show (self->bt_switch);
  gtk_widget_set_valign (self->bt_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->bt_switch, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->bt_switch);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);
  self->bt_row = row;
  g_signal_connect_object (self->bt_rfkill, "g-properties-changed",
                           G_CALLBACK (bt_powered_state_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect (G_OBJECT (self->bt_switch), "notify::active",
		G_CALLBACK (bt_switch_changed), self);

  bt_powered_state_changed (self);
#endif
}

#define NEVER 0

static void
update_automatic_suspend_label (CcPowerPanel *self)
{
  GsdPowerActionType ac_action;
  GsdPowerActionType battery_action;
  gint ac_timeout;
  gint battery_timeout;
  const gchar *s;

  ac_action = g_settings_get_enum (self->gsd_settings, "sleep-inactive-ac-type");
  battery_action = g_settings_get_enum (self->gsd_settings, "sleep-inactive-battery-type");
  ac_timeout = g_settings_get_int (self->gsd_settings, "sleep-inactive-ac-timeout");
  battery_timeout = g_settings_get_int (self->gsd_settings, "sleep-inactive-battery-timeout");

  if (ac_timeout < 0)
    g_warning ("Invalid negative timeout for 'sleep-inactive-ac-timeout': %d", ac_timeout);
  if (battery_timeout < 0)
    g_warning ("Invalid negative timeout for 'sleep-inactive-battery-timeout': %d", battery_timeout);

  if (ac_action == GSD_POWER_ACTION_NOTHING || ac_timeout < 0)
    ac_timeout = NEVER;
  if (battery_action == GSD_POWER_ACTION_NOTHING || battery_timeout < 0)
    battery_timeout = NEVER;

  if (self->has_batteries)
    {
      if (ac_timeout == NEVER && battery_timeout == NEVER)
        s = _("Off");
      else if (ac_timeout == NEVER && battery_timeout > 0)
        s = _("When on battery power");
      else if (ac_timeout > 0 && battery_timeout == NEVER)
        s = _("When plugged in");
      else
        s = _("On");
    }
  else
    {
      if (ac_timeout == NEVER)
        s = _("Off");
      else
        s = _("On");
    }

  if (self->automatic_suspend_label)
    gtk_label_set_label (GTK_LABEL (self->automatic_suspend_label), s);
}

static void
on_suspend_settings_changed (GSettings    *settings,
                             const char   *key,
                             CcPowerPanel *self)
{
  if (g_str_has_prefix (key, "sleep-inactive-"))
    {
      update_automatic_suspend_label (self);
    }
}

static void
activate_row (CcPowerPanel *self,
              GtkListBoxRow *row)
{
  GtkWidget *w;
  GtkWidget *toplevel;

  if (row == GTK_LIST_BOX_ROW (self->automatic_suspend_row))
    {
      w = self->automatic_suspend_dialog;
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
      gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (toplevel));
      gtk_window_set_modal (GTK_WINDOW (w), TRUE);
      gtk_window_present (GTK_WINDOW (w));
    }
}

static gboolean
automatic_suspend_activate (GtkWidget    *widget,
                            gboolean      cycle,
                            CcPowerPanel *self)
{
  activate_row (self, GTK_LIST_BOX_ROW (self->automatic_suspend_row));
  return TRUE;
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
populate_power_button_model (GtkTreeModel *model,
                             gboolean      can_suspend,
                             gboolean      can_hibernate)
{
  struct {
    char *name;
    GsdPowerButtonActionType value;
  } actions[] = {
    { N_("Suspend"), GSD_POWER_BUTTON_ACTION_SUSPEND },
    { N_("Power Off"), GSD_POWER_BUTTON_ACTION_INTERACTIVE },
    { N_("Hibernate"), GSD_POWER_BUTTON_ACTION_HIBERNATE },
    { N_("Nothing"), GSD_POWER_BUTTON_ACTION_NOTHING }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (actions); i++)
    {
      if (!can_suspend && actions[i].value == GSD_POWER_BUTTON_ACTION_SUSPEND)
        continue;

      if (!can_hibernate && actions[i].value == GSD_POWER_BUTTON_ACTION_HIBERNATE)
        continue;

      gtk_list_store_insert_with_values (GTK_LIST_STORE (model),
                                         NULL, -1,
                                         0, _(actions[i].name),
                                         1, actions[i].value,
                                         -1);
    }
}

static gboolean
can_suspend_or_hibernate (CcPowerPanel *self,
                          const char   *method_name)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  const char *s;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                               cc_panel_get_cancellable (CC_PANEL (self)),
                               &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("system bus not available: %s", error->message);
      return FALSE;
    }

  variant = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.login1",
                                         "/org/freedesktop/login1",
                                         "org.freedesktop.login1.Manager",
                                         method_name,
                                         NULL,
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         cc_panel_get_cancellable (CC_PANEL (self)),
                                         &error);

  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("Failed to call %s(): %s", method_name, error->message);
      return FALSE;
    }

  g_variant_get (variant, "(&s)", &s);
  return g_strcmp0 (s, "yes") == 0;
}

static void
add_suspend_and_power_off_section (CcPowerPanel *self)
{
  GtkWidget *widget, *box, *label, *title;
  GtkWidget *row;
  g_autofree gchar *s = NULL;
  gint value;
  GtkWidget *dialog;
  GtkWidget *combo;
  GtkTreeModel *model;
  GsdPowerButtonActionType button_value;
  gboolean can_suspend, can_hibernate;

  can_suspend = can_suspend_or_hibernate (self, "CanSuspend");
  can_hibernate = can_suspend_or_hibernate (self, "CanHibernate");

  /* If the machine can neither suspend nor hibernate, we have nothing to do */
  if (!can_suspend && !can_hibernate)
    return;

  /* The default values for these settings are unfortunate for us;
   * timeout == 0, action == suspend means 'do nothing' - just
   * as timout === anything, action == nothing.
   * For our switch/combobox combination, the second choice works
   * much better, so translate the first to the second here.
   */
  if (g_settings_get_int (self->gsd_settings, "sleep-inactive-ac-timeout") == 0)
    {
      g_settings_set_enum (self->gsd_settings, "sleep-inactive-ac-type", GSD_POWER_ACTION_NOTHING);
      g_settings_set_int (self->gsd_settings, "sleep-inactive-ac-timeout", 3600);
    }
  if (g_settings_get_int (self->gsd_settings, "sleep-inactive-battery-timeout") == 0)
    {
      g_settings_set_enum (self->gsd_settings, "sleep-inactive-battery-type", GSD_POWER_ACTION_NOTHING);
      g_settings_set_int (self->gsd_settings, "sleep-inactive-battery-timeout", 1800);
    }

  /* Frame header */
  s = g_markup_printf_escaped ("<b>%s</b>", _("Suspend & Power Button"));
  label = gtk_label_new (s);
  gtk_widget_show (label);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (label, 12);
  gtk_box_pack_start (GTK_BOX (self->vbox_power), label, FALSE, TRUE, 0);
  gtk_widget_show (label);

  widget = gtk_list_box_new ();
  gtk_widget_show (widget);
  self->boxes_reverse = g_list_prepend (self->boxes_reverse, widget);
  g_signal_connect (widget, "keynav-failed", G_CALLBACK (keynav_failed), self);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);
  g_signal_connect_swapped (widget, "row-activated",
                            G_CALLBACK (activate_row), self);

  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (label)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (widget)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (widget)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (label)));

  box = gtk_frame_new (NULL);
  gtk_widget_show (box);
  gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_IN);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_container_add (GTK_CONTAINER (box), widget);
  gtk_box_pack_start (GTK_BOX (self->vbox_power), box, FALSE, TRUE, 0);

  /* Automatic suspend row */
  if (can_suspend)
    {
      self->automatic_suspend_row = row = gtk_list_box_row_new ();
      gtk_widget_show (row);
      box = row_box_new ();
      gtk_container_add (GTK_CONTAINER (row), box);
      title = row_title_new (_("_Automatic Suspend"), NULL, NULL);
      atk_object_set_name (ATK_OBJECT (gtk_widget_get_accessible (self->automatic_suspend_row)), _("Automatic suspend"));
      gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

      self->automatic_suspend_label = gtk_label_new ("");
      gtk_widget_show (self->automatic_suspend_label);
      gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->automatic_suspend_label);
      g_signal_connect (self->automatic_suspend_label, "mnemonic-activate",
                        G_CALLBACK (automatic_suspend_activate), self);
      gtk_widget_set_halign (self->automatic_suspend_label, GTK_ALIGN_END);
      gtk_box_pack_start (GTK_BOX (box), self->automatic_suspend_label, FALSE, TRUE, 0);
      gtk_container_add (GTK_CONTAINER (widget), row);
      gtk_size_group_add_widget (self->row_sizegroup, row);

      dialog = self->automatic_suspend_dialog;
      g_signal_connect (dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);
      g_signal_connect (self->gsd_settings, "changed", G_CALLBACK (on_suspend_settings_changed), self);

      g_settings_bind_with_mapping (self->gsd_settings, "sleep-inactive-battery-type",
                                    self->suspend_on_battery_switch, "active",
                                    G_SETTINGS_BIND_DEFAULT,
                                    get_sleep_type, set_sleep_type, NULL, NULL);

      g_object_set_data (G_OBJECT (self->suspend_on_battery_delay_combo), "_gsettings_key", "sleep-inactive-battery-timeout");
      value = g_settings_get_int (self->gsd_settings, "sleep-inactive-battery-timeout");
      set_value_for_combo (GTK_COMBO_BOX (self->suspend_on_battery_delay_combo), value);
      g_signal_connect (self->suspend_on_battery_delay_combo, "changed",
                        G_CALLBACK (combo_time_changed_cb), self);
      g_object_bind_property (self->suspend_on_battery_switch, "active", self->suspend_on_battery_delay_combo, "sensitive",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      g_settings_bind_with_mapping (self->gsd_settings, "sleep-inactive-ac-type",
                                    self->suspend_on_ac_switch, "active",
                                    G_SETTINGS_BIND_DEFAULT,
                                    get_sleep_type, set_sleep_type, NULL, NULL);

      g_object_set_data (G_OBJECT (self->suspend_on_ac_delay_combo), "_gsettings_key", "sleep-inactive-ac-timeout");
      value = g_settings_get_int (self->gsd_settings, "sleep-inactive-ac-timeout");
      set_value_for_combo (GTK_COMBO_BOX (self->suspend_on_ac_delay_combo), value);
      g_signal_connect (self->suspend_on_ac_delay_combo, "changed",
                        G_CALLBACK (combo_time_changed_cb), self);
      g_object_bind_property (self->suspend_on_ac_switch, "active", self->suspend_on_ac_delay_combo, "sensitive",
                              G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

      set_ac_battery_ui_mode (self);
      update_automatic_suspend_label (self);
    }

  if (g_strcmp0 (self->chassis_type, "vm") == 0 ||
      g_strcmp0 (self->chassis_type, "tablet") == 0 ||
      g_strcmp0 (self->chassis_type, "handset") == 0)
    return;

  /* Power button row */
  row = no_prelight_row_new ();
  gtk_widget_show (row);
  box = row_box_new ();
  gtk_container_add (GTK_CONTAINER (row), box);

  title = row_title_new (_("Po_wer Button Action"), NULL, &label);
  gtk_box_pack_start (GTK_BOX (box), title, TRUE, TRUE, 0);

  combo = gtk_combo_box_text_new ();
  gtk_widget_show (combo);
  gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (combo), 0);
  model = GTK_TREE_MODEL (self->liststore_power_button);
  populate_power_button_model (model, can_suspend, can_hibernate);
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo), model);
  button_value = g_settings_get_enum (self->gsd_settings, "power-button-action");
  set_value_for_combo (GTK_COMBO_BOX (combo), button_value);
  g_signal_connect (combo, "changed",
                    G_CALLBACK (combo_power_button_changed_cb), self);
  gtk_widget_set_valign (combo, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), combo, FALSE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
  gtk_container_add (GTK_CONTAINER (widget), row);
  gtk_size_group_add_widget (self->row_sizegroup, row);
}

static gint
battery_sort_func (gconstpointer a, gconstpointer b, gpointer data)
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
  GtkWidget *widget, *box;
  GtkWidget *frame;
  g_autofree gchar *s = NULL;

  self->battery_section = box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_box_pack_start (GTK_BOX (self->vbox_power), box, FALSE, TRUE, 0);

  s = g_markup_printf_escaped ("<b>%s</b>", _("Battery"));
  self->battery_heading = widget = gtk_label_new (s);
  gtk_widget_show (widget);
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (widget, 12);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);

  self->battery_list = widget = GTK_WIDGET (gtk_list_box_new ());
  gtk_widget_show (widget);
  self->boxes_reverse = g_list_prepend (self->boxes_reverse, self->battery_list);
  g_signal_connect (widget, "keynav-failed", G_CALLBACK (keynav_failed), self);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (widget),
                              (GtkListBoxSortFunc)battery_sort_func, NULL, NULL);

  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (self->battery_heading)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (self->battery_list)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (self->battery_list)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (self->battery_heading)));

  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame), widget);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
}

static void
add_device_section (CcPowerPanel *self)
{
  GtkWidget *widget, *box;
  GtkWidget *frame;
  g_autofree gchar *s = NULL;

  self->device_section = box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);
  gtk_widget_set_margin_top (box, 6);
  gtk_widget_set_margin_bottom (box, 32);
  gtk_box_pack_start (GTK_BOX (self->vbox_power), box, FALSE, TRUE, 0);

  s = g_markup_printf_escaped ("<b>%s</b>", _("Devices"));
  self->device_heading = widget = gtk_label_new (s);
  gtk_widget_show (widget);
  gtk_label_set_ellipsize (GTK_LABEL (widget), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (widget), 0.0);
  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_widget_set_halign (widget, GTK_ALIGN_START);
  gtk_widget_set_margin_bottom (widget, 12);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);

  self->device_list = widget = gtk_list_box_new ();
  gtk_widget_show (widget);
  self->boxes_reverse = g_list_prepend (self->boxes_reverse, self->device_list);
  g_signal_connect (widget, "keynav-failed", G_CALLBACK (keynav_failed), self);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);
  gtk_list_box_set_sort_func (GTK_LIST_BOX (widget),
                              (GtkListBoxSortFunc)battery_sort_func, NULL, NULL);

  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (self->device_heading)),
                               ATK_RELATION_LABEL_FOR,
                               ATK_OBJECT (gtk_widget_get_accessible (self->device_list)));
  atk_object_add_relationship (ATK_OBJECT (gtk_widget_get_accessible (self->device_list)),
                               ATK_RELATION_LABELLED_BY,
                               ATK_OBJECT (gtk_widget_get_accessible (self->device_heading)));

  frame = gtk_frame_new (NULL);
  gtk_widget_show (frame);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame), widget);
  gtk_box_pack_start (GTK_BOX (box), frame, FALSE, TRUE, 0);
}

static void
cc_power_panel_init (CcPowerPanel *self)
{
  guint i;

  g_resources_register (cc_power_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Power",
                                       "/org/gnome/SettingsDaemon/Power",
                                       "org.gnome.SettingsDaemon.Power.Screen",
                                       cc_panel_get_cancellable (CC_PANEL (self)),
                                       got_screen_proxy_cb,
                                       self);
  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Power",
                                       "/org/gnome/SettingsDaemon/Power",
                                       "org.gnome.SettingsDaemon.Power.Keyboard",
                                       cc_panel_get_cancellable (CC_PANEL (self)),
                                       got_kbd_proxy_cb,
                                       self);

  self->chassis_type = get_chassis_type (cc_panel_get_cancellable (CC_PANEL (self)));

  self->up_client = up_client_new ();

  self->gsd_settings = g_settings_new ("org.gnome.settings-daemon.plugins.power");
  self->session_settings = g_settings_new ("org.gnome.desktop.session");

  self->battery_row_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  self->row_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  self->battery_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  self->charge_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  self->level_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  add_battery_section (self);
  add_device_section (self);
  add_power_saving_section (self);
  add_suspend_and_power_off_section (self);

  self->boxes = g_list_copy (self->boxes_reverse);
  self->boxes = g_list_reverse (self->boxes);

  /* populate batteries */
  g_signal_connect (self->up_client, "device-added", G_CALLBACK (up_client_device_added), self);
  g_signal_connect (self->up_client, "device-removed", G_CALLBACK (up_client_device_removed), self);

  self->devices = up_client_get_devices2 (self->up_client);
  for (i = 0; self->devices != NULL && i < self->devices->len; i++) {
    UpDevice *device = g_ptr_array_index (self->devices, i);
    g_signal_connect (G_OBJECT (device), "notify",
                      G_CALLBACK (up_client_changed), self);
  }
  up_client_changed (self->up_client, NULL, self);

  self->focus_adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (self->main_scroll));
  gtk_container_set_focus_vadjustment (GTK_CONTAINER (self->main_box), self->focus_adjustment);
}
