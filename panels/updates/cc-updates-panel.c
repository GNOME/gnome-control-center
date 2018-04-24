/* cc-updates-panel.c
 *
 * Copyright © 2018 Endless, Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Georges Basile Stavracas Neto <georges@endlessm.com>
 */

#include "cc-tariff-editor.h"
#include "cc-updates-panel.h"
#include "cc-updates-resources.h"

#include <glib/gi18n.h>
#include <libmogwai-tariff/tariff-loader.h>
#include <NetworkManager.h>

#define NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED "connection.allow-downloads-when-metered"
#define NM_SETTING_ALLOW_DOWNLOADS              "connection.allow-downloads"
#define NM_SETTING_TARIFF_ENABLED               "connection.tariff-enabled"
#define NM_SETTING_TARIFF                       "connection.tariff"

struct _CcUpdatesPanel
{
  CcPanel             parent;

  GtkWidget          *automatic_updates_container;
  GtkWidget          *automatic_updates_switch;
  GtkWidget          *metered_data_label;
  GtkWidget          *network_name_label;
  GtkWidget          *network_settings_label;
  GtkWidget          *network_status_icon;
  GtkWidget          *scheduled_updates_container;
  GtkWidget          *scheduled_updates_switch;
  CcTariffEditor     *tariff_editor;

  /* Network Manager */
  NMClient           *nm_client;
  NMDevice           *current_device;
  NMConnection       *current_connection;

  /* Signal handlers */
  gulong              changed_id;
  gulong              connection_changed_id;
  guint               save_tariff_timeout_id;

  GCancellable       *cancellable;
};


static void          on_automatic_updates_switch_changed_cb      (GtkSwitch      *sw,
                                                                  GParamSpec     *pspec,
                                                                  CcUpdatesPanel *self);

static void          on_network_changed_cb                       (CcUpdatesPanel *self);

static void          on_network_changes_committed_cb             (GObject        *source,
                                                                  GAsyncResult   *result,
                                                                  gpointer        user_data);

static void          on_scheduled_updates_switch_changed_cb      (GtkSwitch      *sw,
                                                                  GParamSpec     *pspec,
                                                                  CcUpdatesPanel *self);

static void          on_tariff_changed_cb                        (CcTariffEditor *tariff_editor,
                                                                  CcUpdatesPanel *self);

static gboolean      save_tariff_cb                              (gpointer        user_data);

G_DEFINE_TYPE (CcUpdatesPanel, cc_updates_panel, CC_TYPE_PANEL)

enum
{
  PROP_PARAMETERS = 1,
  N_PROPS
};


/*
 * Auxiliary methods
 */

static void
cleanup_signals (CcUpdatesPanel *self)
{
  if (self->changed_id > 0)
    {
      g_signal_handler_disconnect (self->current_device, self->changed_id);
      self->changed_id = 0;
    }

  if (self->connection_changed_id > 0)
    {
      g_signal_handler_disconnect (self->current_connection, self->connection_changed_id);
      self->connection_changed_id = 0;
    }

  g_clear_object (&self->current_device);
  g_clear_object (&self->current_connection);
}

static NMSettingConnection *
ensure_setting_connection (NMConnection *connection)
{
  NMSettingConnection *setting = nm_connection_get_setting_connection (connection);

  g_assert (connection != NULL);

  if (!setting)
    {
      setting = NM_SETTING_CONNECTION (nm_setting_connection_new ());
      nm_connection_add_setting (connection, NM_SETTING (setting));
    }

  return setting;
}

static NMSettingUser *
ensure_setting_user (NMConnection *connection)
{
  NMSettingUser *setting_user;

  g_assert (connection != NULL);

  setting_user = NM_SETTING_USER (nm_connection_get_setting (connection, NM_TYPE_SETTING_USER));

  if (!setting_user)
    {
      setting_user = NM_SETTING_USER (nm_setting_user_new ());
      nm_connection_add_setting (connection, NM_SETTING (setting_user));
    }

  return setting_user;
}

static void
store_automatic_updates_setting (CcUpdatesPanel *self,
                                 NMConnection   *connection,
                                 gboolean        automatic_updates_enabled,
                                 gboolean        tariff_enabled,
                                 GVariant       *tariff_variant)
{
  NMSettingUser *setting_user;
  g_autofree gchar *tariff_string = NULL;
  g_autoptr(GError) error = NULL;
  gboolean errored = FALSE;

  setting_user = ensure_setting_user (connection);
  g_assert (setting_user != NULL);

  /* Calling nm_setting_user_set_data() causes notifications from NM. */
  if (self->current_device && self->changed_id)
    g_signal_handler_block (self->current_device, self->changed_id);
  if (self->current_connection && self->connection_changed_id)
    g_signal_handler_block (self->current_connection, self->connection_changed_id);

  g_debug ("Setting "NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED" to 1");

  nm_setting_user_set_data (setting_user,
                            NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED,
                            "1",
                            &error);

  if (error)
    {
      g_warning ("Error storing "NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED": %s", error->message);
      g_clear_error (&error);
      errored = TRUE;
    }

  g_debug ("Setting "NM_SETTING_ALLOW_DOWNLOADS" to %d", automatic_updates_enabled);

  nm_setting_user_set_data (setting_user,
                            NM_SETTING_ALLOW_DOWNLOADS,
                            automatic_updates_enabled ? "1" : "0",
                            &error);

  if (error)
    {
      g_warning ("Error storing "NM_SETTING_ALLOW_DOWNLOADS": %s", error->message);
      g_clear_error (&error);
      errored = TRUE;
    }

  g_debug ("Setting "NM_SETTING_TARIFF_ENABLED" to %d", tariff_enabled);

  nm_setting_user_set_data (setting_user,
                            NM_SETTING_TARIFF_ENABLED,
                            tariff_enabled ? "1" : "0",
                            &error);
  if (error)
    {
      g_warning ("Error storing "NM_SETTING_TARIFF_ENABLED": %s", error->message);
      g_clear_error (&error);
      errored = TRUE;
    }

  tariff_string = tariff_variant ? g_variant_print (tariff_variant, TRUE) : NULL;

  g_debug ("Setting "NM_SETTING_TARIFF" to %s", tariff_string);
  nm_setting_user_set_data (setting_user, NM_SETTING_TARIFF, tariff_string, &error);

  if (error)
    {
      g_warning ("Error storing "NM_SETTING_TARIFF": %s", error->message);
      g_clear_error (&error);
      errored = TRUE;
    }

  if (self->current_device && self->changed_id)
    g_signal_handler_unblock (self->current_device, self->changed_id);
  if (self->current_connection && self->connection_changed_id)
    g_signal_handler_unblock (self->current_connection, self->connection_changed_id);

  /* Only commit the changes if there were no errors. */
  if (errored)
    return;

  /* Make sure the application does not exit before the callback is called */
  g_application_hold (g_application_get_default ());

  nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (connection),
                                             TRUE, /* save to disk */
                                             NULL,
                                             on_network_changes_committed_cb,
                                             NULL);
}

static void
get_active_connection_and_device (CcUpdatesPanel  *self,
                                  NMDevice       **out_device,
                                  NMConnection   **out_connection,
                                  NMAccessPoint  **out_ap)
{
  NMActiveConnection *active_connection = NULL;
  NMConnection *connection = NULL;
  const GPtrArray *active_devices = NULL;
  NMAccessPoint *ap = NULL;
  NMDevice *active_device = NULL;

  active_connection = nm_client_get_primary_connection (self->nm_client);

  /* If no primary connection is already present, try and use the connecting one */
  if (!active_connection)
    active_connection = nm_client_get_activating_connection (self->nm_client);

  if (active_connection)
    {
      connection = NM_CONNECTION (nm_active_connection_get_connection (active_connection));
      active_devices = nm_active_connection_get_devices (active_connection);

      if (active_devices && active_devices->len > 0)
        {
          /* This array is guaranteed to have only one element */
          active_device = g_ptr_array_index (active_devices, 0);

          if (NM_IS_DEVICE_WIFI (active_device))
            ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (active_device));
        }
    }

  if (out_device)
    *out_device = active_device;

  if (out_connection)
    *out_connection = connection;

  if (out_ap)
    *out_ap = ap;
}

static const gchar *
get_wifi_icon_from_strength (NMAccessPoint *ap)
{
  guint8 strength;

  if (!ap)
    return "network-wireless-signal-none-symbolic";

  strength = nm_access_point_get_strength (ap);

  if (strength < 20)
    return "network-wireless-signal-none-symbolic";
  else if (strength < 40)
    return "network-wireless-signal-weak-symbolic";
  else if (strength < 50)
    return "network-wireless-signal-ok-symbolic";
  else if (strength < 80)
    return "network-wireless-signal-good-symbolic";
  else
    return "network-wireless-signal-excellent-symbolic";
}

static const gchar *
get_network_status_icon_name (CcUpdatesPanel *self,
                              NMDevice       *device,
                              NMAccessPoint  *ap)
{
  if (NM_IS_DEVICE_WIFI (device))
    {
      switch (nm_client_get_state (self->nm_client))
        {
        case NM_STATE_UNKNOWN:
        case NM_STATE_ASLEEP:
        case NM_STATE_DISCONNECTING:
          return "network-wireless-offline-symbolic";

        case NM_STATE_DISCONNECTED:
          return "network-wireless-offline-symbolic";

        case NM_STATE_CONNECTING:
          return "network-wireless-acquiring-symbolic";

        case NM_STATE_CONNECTED_LOCAL:
        case NM_STATE_CONNECTED_SITE:
          return "network-wireless-no-route-symbolic";

        case NM_STATE_CONNECTED_GLOBAL:
          return get_wifi_icon_from_strength (ap);
        }
    }
  else
    {
      switch (nm_client_get_state (self->nm_client))
        {
        case NM_STATE_UNKNOWN:
        case NM_STATE_ASLEEP:
        case NM_STATE_DISCONNECTING:
          return "network-wired-disconnected-symbolic";

        case NM_STATE_DISCONNECTED:
          return "network-wired-offline-symbolic";

        case NM_STATE_CONNECTING:
          return "network-wired-acquiring-symbolic";

        case NM_STATE_CONNECTED_LOCAL:
        case NM_STATE_CONNECTED_SITE:
          return "network-wired-no-route-symbolic";

        case NM_STATE_CONNECTED_GLOBAL:
          return "network-wired-symbolic";
        }
    }

  return "network-wireless-offline-symbolic";
}

static void
schedule_save_tariff (CcUpdatesPanel *self)
{
  g_debug ("Scheduling save tariff");

  if (self->save_tariff_timeout_id > 0)
    g_source_remove (self->save_tariff_timeout_id);

  self->save_tariff_timeout_id = g_timeout_add_seconds (2, save_tariff_cb, self);
}

static void
load_tariff_from_connection (CcUpdatesPanel *self,
                             NMConnection   *connection)
{
  g_autoptr(MwtTariff) tariff = NULL;
  g_autoptr(GError) error = NULL;
  const gchar *tariff_value = NULL;
  gboolean tariff_enabled = FALSE;

  g_debug ("Loading tariff from connection");

  if (connection)
    {
      NMSettingUser *setting_user;
      const gchar *tariff_enabled_value = NULL;

      setting_user = ensure_setting_user (connection);
      g_assert (setting_user != NULL);

      tariff_value = nm_setting_user_get_data (setting_user, NM_SETTING_TARIFF);
      tariff_enabled_value = nm_setting_user_get_data (setting_user, NM_SETTING_TARIFF_ENABLED);

      if (tariff_enabled_value)
        tariff_enabled = g_str_equal (tariff_enabled_value, "1");
    }

  /* Only load the tariff if there is a setting stored */
  if (tariff_value && *tariff_value != '\0')
    {
      g_autoptr(MwtTariffLoader) tariff_loader = NULL;
      g_autoptr(GVariant) variant = NULL;

      variant = g_variant_parse (NULL, tariff_value, NULL, NULL, &error);
      if (error)
        {
          g_warning ("Error loading tariff: %s", error->message);
          return;
        }

      tariff_loader = mwt_tariff_loader_new ();
      mwt_tariff_loader_load_from_variant (tariff_loader, variant, &error);
      if (error)
        {
          g_warning ("Error loading tariff: %s", error->message);
          return;
        }

      tariff = g_object_ref (mwt_tariff_loader_get_tariff (tariff_loader));
    }

  g_signal_handlers_block_by_func (self->tariff_editor, on_tariff_changed_cb, self);
  g_signal_handlers_block_by_func (self->scheduled_updates_switch, on_scheduled_updates_switch_changed_cb, self);

  cc_tariff_editor_load_tariff (self->tariff_editor, tariff, &error);

  if (error)
    g_warning ("Error loading tariff: %s", error->message);

  gtk_switch_set_active (GTK_SWITCH (self->scheduled_updates_switch), tariff_enabled);

  g_signal_handlers_unblock_by_func (self->scheduled_updates_switch, on_scheduled_updates_switch_changed_cb, self);
  g_signal_handlers_unblock_by_func (self->tariff_editor, on_tariff_changed_cb, self);
}

static void
load_automatic_updates_from_connection (CcUpdatesPanel *self,
                                        NMConnection   *connection)
{
  gtk_widget_set_sensitive (self->automatic_updates_container, connection != NULL);

  g_signal_handlers_block_by_func (self->automatic_updates_switch, on_automatic_updates_switch_changed_cb, self);

  if (connection)
    {
      const gchar * const * keys = NULL;
      NMSettingUser *setting_user;
      const gchar *value;
      gboolean should_save;
      gboolean enabled;

      setting_user = ensure_setting_user (connection);
      g_assert (setting_user != NULL);

      /* This is the upgrade path from the old NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED
       * to NM_SETTING_ALLOW_DOWNLOADS. For now, downloads are always allowed when we're
       * on a metered connection. */
      keys = nm_setting_user_get_keys (setting_user, NULL);

      if (g_strv_contains (keys, NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED) &&
          !g_strv_contains (keys, NM_SETTING_ALLOW_DOWNLOADS))
        {
          g_debug ("Upgrading setting from "NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED" to "NM_SETTING_ALLOW_DOWNLOADS);

          value = nm_setting_user_get_data (setting_user, NM_SETTING_ALLOW_DOWNLOADS_WHEN_METERED);
          should_save = TRUE;
        }
      else
        {
          value = nm_setting_user_get_data (setting_user, NM_SETTING_ALLOW_DOWNLOADS);
          should_save = FALSE;
        }

      if (value != NULL)
        {
          enabled = g_strcmp0 (value, "1") == 0;
        }
      else
        {
          NMSettingConnection *setting;
          NMMetered metered;

          /* The default value depends on the metered state of the connection */
          setting = ensure_setting_connection (connection);
          g_assert (setting != NULL);

          metered = nm_setting_connection_get_metered (setting);
          enabled = metered != NM_METERED_YES && metered != NM_METERED_GUESS_YES;
        }

      gtk_switch_set_active (GTK_SWITCH (self->automatic_updates_switch), enabled);

      if (should_save)
        schedule_save_tariff (self);
    }
  else
    {
      gtk_switch_set_active (GTK_SWITCH (self->automatic_updates_switch), FALSE);
    }

  g_signal_handlers_unblock_by_func (self->automatic_updates_switch, on_automatic_updates_switch_changed_cb, self);
}

static void
load_metered_label_from_connection (CcUpdatesPanel *self,
                                    NMConnection   *connection)
{
  NMSettingConnection *setting;
  NMMetered metered;

  gtk_widget_set_visible (self->metered_data_label, connection != NULL);

  if (!connection)
    return;

  setting = ensure_setting_connection (connection);
  g_assert (setting != NULL);

  metered = nm_setting_connection_get_metered (setting);

  if (metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES)
    gtk_label_set_label (GTK_LABEL (self->metered_data_label), _("Limited data plan connection"));
  else
    gtk_label_set_label (GTK_LABEL (self->metered_data_label), _("Unlimited data plan connection"));
}

static void
update_active_network (CcUpdatesPanel *self)
{
  NMAccessPoint *ap;
  NMConnection *connection;
  NMDevice *device;
  const gchar *icon_name;

  get_active_connection_and_device (self, &device, &connection, &ap);

  /* Setup the new device... */
  if (self->current_device != device || self->current_connection != connection)
    cleanup_signals (self);

  if (g_set_object (&self->current_device, device) && device)
    {
      self->changed_id = g_signal_connect_swapped (device,
                                                   "state-changed",
                                                   G_CALLBACK (on_network_changed_cb),
                                                   self);
    }

  /* ... and the new connection */
  if (g_set_object (&self->current_connection, connection) && connection)
    {
      self->connection_changed_id = g_signal_connect_swapped (connection,
                                                              "changed",
                                                              G_CALLBACK (on_network_changed_cb),
                                                              self);
    }

  /* Icon */
  icon_name = get_network_status_icon_name (self, device, ap);
  gtk_image_set_from_icon_name (GTK_IMAGE (self->network_status_icon), icon_name, GTK_ICON_SIZE_BUTTON);

  /* Name */
  if (ap)
    {
      GBytes *ssid = nm_access_point_get_ssid (ap);
      g_autofree gchar *title = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));

      gtk_label_set_label (GTK_LABEL (self->network_name_label), title);
    }
  else
    {
      gtk_label_set_label (GTK_LABEL (self->network_name_label),
                           connection ? _("Connected") : _("No active connection"));
    }

  load_metered_label_from_connection (self, connection);
  load_automatic_updates_from_connection (self, connection);
  load_tariff_from_connection (self, connection);
}

static void
save_connection_settings (CcUpdatesPanel *self)
{
  NMConnection *connection;
  GVariant *tariff_variant;
  gboolean automatic_updates_enabled;
  gboolean tariff_enabled;

  g_debug ("Saving connection settings");

  get_active_connection_and_device (self, NULL, &connection, NULL);

  automatic_updates_enabled = gtk_switch_get_active (GTK_SWITCH (self->automatic_updates_switch));
  tariff_enabled = gtk_switch_get_active (GTK_SWITCH (self->scheduled_updates_switch));
  tariff_variant = cc_tariff_editor_get_tariff_as_variant (self->tariff_editor);

  store_automatic_updates_setting (self, connection, automatic_updates_enabled, tariff_enabled, tariff_variant);
}


/*
 * Callbacks
 */

static gboolean
save_tariff_cb (gpointer user_data)
{
  CcUpdatesPanel *self = CC_UPDATES_PANEL (user_data);

  save_connection_settings (self);
  self->save_tariff_timeout_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_automatic_updates_switch_changed_cb (GtkSwitch      *sw,
                                        GParamSpec     *pspec,
                                        CcUpdatesPanel *self)
{
  save_connection_settings (self);
}

static gboolean
on_change_network_link_activated_cb (GtkLabel       *label,
                                     gchar          *uri,
                                     CcUpdatesPanel *self)
{
  g_autoptr(GError) error = NULL;
  NMDevice *device;
  CcShell *shell;

  shell = cc_panel_get_shell (CC_PANEL (self));

  get_active_connection_and_device (self, &device, NULL, NULL);

  if (!device || !NM_IS_DEVICE_WIFI (device))
    cc_shell_set_active_panel_from_id (shell, "network", NULL, &error);
  else
    cc_shell_set_active_panel_from_id (shell, "wifi", NULL, &error);

  if (error)
    {
      g_warning ("Error activating panel: %s", error->message);
      return FALSE;
    }

  return TRUE;
}

static void
on_network_changed_cb (CcUpdatesPanel *self)
{
  g_debug ("NetworkManager changed state");

  update_active_network (self);
}

static void
on_network_changes_committed_cb (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  g_autoptr(GError) error = NULL;

  g_application_release (g_application_get_default ());

  nm_remote_connection_commit_changes_finish (NM_REMOTE_CONNECTION (source), result, &error);

  if (error)
    g_warning ("Error storing connection settings: %s", error->message);
}

static void
on_scheduled_updates_switch_changed_cb (GtkSwitch      *sw,
                                        GParamSpec     *pspec,
                                        CcUpdatesPanel *self)
{
  g_debug ("Scheduled Updates changed state");

  schedule_save_tariff (self);
}

static void
on_tariff_changed_cb (CcTariffEditor *tariff_editor,
                      CcUpdatesPanel *self)
{
  g_debug ("The saved tariff changed");

  schedule_save_tariff (self);
}


/*
 * GObject overrides
 */

static void
cc_updates_panel_dispose (GObject *object)
{
  CcUpdatesPanel *self = (CcUpdatesPanel *)object;

  if (self->save_tariff_timeout_id > 0)
    {
      save_connection_settings (self);
      g_source_remove (self->save_tariff_timeout_id);
      self->save_tariff_timeout_id = 0;
    }

  G_OBJECT_CLASS (cc_updates_panel_parent_class)->dispose (object);
}

static void
cc_updates_panel_finalize (GObject *object)
{
  CcUpdatesPanel *self = (CcUpdatesPanel *)object;

  cleanup_signals (self);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->nm_client);

  G_OBJECT_CLASS (cc_updates_panel_parent_class)->finalize (object);
}

static void
cc_updates_panel_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_PARAMETERS:
      /* Nothing to do */
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_updates_panel_class_init (CcUpdatesPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_updates_panel_dispose;
  object_class->finalize = cc_updates_panel_finalize;
  object_class->set_property = cc_updates_panel_set_property;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/updates/cc-updates-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, automatic_updates_container);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, automatic_updates_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, metered_data_label);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, network_name_label);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, network_settings_label);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, network_status_icon);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, scheduled_updates_container);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, scheduled_updates_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUpdatesPanel, tariff_editor);

  gtk_widget_class_bind_template_callback (widget_class, on_automatic_updates_switch_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_change_network_link_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_scheduled_updates_switch_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_tariff_changed_cb);

  g_type_ensure (CC_TYPE_TARIFF_EDITOR);
}

static void
cc_updates_panel_init (CcUpdatesPanel *self)
{
  g_autofree gchar *settings_text = NULL;

  g_resources_register (cc_updates_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  /* "Change Network Settings" label */
  settings_text = g_strdup_printf ("<a href=\"\">%s</a>", _("Change Network Settings…"));
  gtk_label_set_markup (GTK_LABEL (self->network_settings_label), settings_text);

 /* FIXME: Rework this to be async. This will be done at the same time as
  * reworking all the other panels to be async. */
  self->nm_client = nm_client_new (NULL, NULL);

  update_active_network (self);

  g_signal_connect_swapped (self->nm_client, "notify", G_CALLBACK (on_network_changed_cb), self);
}
