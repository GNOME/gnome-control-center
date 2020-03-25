/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2020 Endless Mobile
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
 * Author: Umang Jain <umang@endlessm.com>
 */

#include "list-box-helper.h"
#include "cc-metrics-panel.h"
#include "cc-metrics-resources.h"
#include "cc-util.h"
#include "cc-list-row.h"
#include "shell/cc-application.h"

#include <glib/gi18n.h>
#include <polkit/polkit.h>

struct _CcMetricsPanel
{
  CcPanel        parent_instance;

  GtkStack      *stack;
  CcListRow     *metrics_identifier_row;
  GtkListBox    *metrics_list_box;
  GtkWidget     *enable_metrics_switch;

  gboolean       metrics_active;
  gboolean       changing_state;
  gboolean       pending_state;

  GDBusProxy    *metrics_proxy;
};

CC_PANEL_REGISTER (CcMetricsPanel, cc_metrics_panel)

static void
on_metrics_panel_set_enabled (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  CcMetricsPanel *self = CC_METRICS_PANEL (user_data);
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to enable/disable metrics: %s", error->message);

      return;
    }

  self->changing_state = FALSE;
  self->metrics_active = self->pending_state;
  gtk_switch_set_state (GTK_SWITCH (self->enable_metrics_switch),
                        self->metrics_active);
}

static gboolean
metrics_switch_active_changed_cb (GtkSwitch *widget,
                                  gboolean   new_state,
                                  gpointer   user_data)
{
  CcMetricsPanel *self = CC_METRICS_PANEL (user_data);

  if (self->changing_state)
    return TRUE;

  self->changing_state = TRUE;
  self->pending_state = new_state;
  g_dbus_proxy_call (self->metrics_proxy,
                     "SetEnabled",
                     g_variant_new ("(b)", self->pending_state),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     on_metrics_panel_set_enabled,
                     self);
  return TRUE;
}

static void
on_metrics_proxy_properties_changed (GDBusProxy     *proxy,
                                     GVariant       *changed_properties,
                                     GStrv           invalidated_properties,
                                     CcMetricsPanel *self)
{
  g_autoptr(GVariant) value = NULL;
  const gchar *tracking_id;
  gboolean metrics_active;

  value = g_variant_lookup_value (changed_properties, "Enabled", G_VARIANT_TYPE_BOOLEAN);
  if (value)
    {
      metrics_active = g_variant_get_boolean (value);

      /* If the D-Bus property is changed externally (for e.g. via d-feet), sync state */
      if (self->metrics_active != metrics_active)
        {
          gtk_switch_set_state (GTK_SWITCH (self->enable_metrics_switch), metrics_active);
          self->metrics_active = metrics_active;
        }
      g_clear_pointer (&value, g_variant_unref);
    }

  value = g_variant_lookup_value (changed_properties, "TrackingId", G_VARIANT_TYPE_STRING);
  if (value)
    {
      tracking_id = g_variant_get_string (value, NULL);
      cc_list_row_set_secondary_label (self->metrics_identifier_row, tracking_id);
      g_clear_pointer (&value, g_variant_unref);
    }
}

static gboolean
on_reset_metrics_id_button_clicked (GtkButton      *button,
                                    CcMetricsPanel *self)
{
  g_dbus_proxy_call (self->metrics_proxy,
                     "ResetTrackingId",
                     g_variant_new ("()"),
                     G_DBUS_CALL_FLAGS_NONE, -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     NULL, NULL);

  return TRUE;
}

static gboolean
on_attribution_label_link (GtkLinkButton  *link_button,
                           CcMetricsPanel *self)
{
  const gchar *uri = gtk_link_button_get_uri (link_button);

  if (g_strcmp0 (uri, "attribution-link") != 0)
    return FALSE;

  return cc_util_show_endless_terms_of_use (GTK_WIDGET (link_button));
}

static gboolean
to_child_name (GBinding     *binding,
               const GValue *from,
               GValue       *to,
               gpointer      user_data)
{
  if (g_value_get_boolean (from))
    g_value_set_string (to, "content");
  else
    g_value_set_string (to, "empty");
  return TRUE;
}

static void
cc_metrics_panel_dispose (GObject *object)
{
  CcMetricsPanel *self = CC_METRICS_PANEL (object);

  if (self->metrics_proxy)
    {
      g_signal_handlers_disconnect_by_func (self->metrics_proxy,
                                            on_metrics_proxy_properties_changed,
                                            self);
      g_clear_object (&self->metrics_proxy);
    }

  G_OBJECT_CLASS (cc_metrics_panel_parent_class)->dispose (object);
}

static void
cc_metrics_panel_constructed (GObject *object)
{
  CcMetricsPanel *self = CC_METRICS_PANEL (object);
  g_autoptr(GError) error = NULL;
  const gchar *tracking_id;
  gboolean metrics_can_change;
  GtkWidget *box;
  g_autoptr(GPermission) permission = NULL;
  g_autoptr(GVariant) value = NULL;

  G_OBJECT_CLASS (cc_metrics_panel_parent_class)->constructed (object);

  self->metrics_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       NULL,
                                                       "com.endlessm.Metrics",
                                                       "/com/endlessm/Metrics",
                                                       "com.endlessm.Metrics.EventRecorderServer",
                                                       NULL, &error);
  if (error != NULL)
    {
      g_warning ("Unable to create a D-Bus proxy for the metrics daemon: %s", error->message);
    }
  else
    {
      g_signal_connect (self->metrics_proxy, "g-properties-changed",
                        G_CALLBACK (on_metrics_proxy_properties_changed), self);

      value = g_dbus_proxy_get_cached_property (self->metrics_proxy, "Enabled");
      self->metrics_active = g_variant_get_boolean (value);
      g_clear_pointer (&value, g_variant_unref);

      value = g_dbus_proxy_get_cached_property (self->metrics_proxy, "TrackingId");
      tracking_id = g_variant_get_string (value, NULL);
      g_clear_pointer (&value, g_variant_unref);
    }

  permission = polkit_permission_new_sync ("com.endlessm.Metrics.SetEnabled",
                                           NULL, NULL, NULL);
  if (!permission)
    metrics_can_change = FALSE;
  else
    metrics_can_change = g_permission_get_allowed (permission);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (box);

  self->enable_metrics_switch = gtk_switch_new ();
  gtk_widget_set_valign (self->enable_metrics_switch, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), self->enable_metrics_switch, FALSE, FALSE, 4);
  gtk_widget_show (self->enable_metrics_switch);

  gtk_widget_set_sensitive (GTK_WIDGET (self->enable_metrics_switch), metrics_can_change);
  gtk_switch_set_state (GTK_SWITCH (self->enable_metrics_switch), self->metrics_active);
  g_signal_connect (self->enable_metrics_switch, "state-set",
                    G_CALLBACK (metrics_switch_active_changed_cb), self);

  cc_list_row_set_secondary_label (self->metrics_identifier_row, tracking_id);

  g_object_bind_property_full (self->enable_metrics_switch,
                               "active",
                               self->stack,
                               "visible-child-name",
                               G_BINDING_SYNC_CREATE,
                               to_child_name,
                               NULL,
                               NULL, NULL);

  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (self)),
                                   box,
                                   GTK_POS_RIGHT);
}

static void
cc_metrics_panel_class_init (CcMetricsPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->constructed = cc_metrics_panel_constructed;
  oclass->dispose = cc_metrics_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/metrics/cc-metrics-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMetricsPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcMetricsPanel, metrics_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcMetricsPanel, metrics_identifier_row);

  gtk_widget_class_bind_template_callback (widget_class, on_attribution_label_link);
  gtk_widget_class_bind_template_callback (widget_class, on_reset_metrics_id_button_clicked);
}

static void
cc_metrics_panel_init (CcMetricsPanel *self)
{
  g_resources_register (cc_metrics_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->metrics_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);
}
