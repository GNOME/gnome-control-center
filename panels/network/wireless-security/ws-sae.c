/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Jonathan Kang <songchuan.kang@suse.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (c) 2020 SUSE LINUX GmbH, Nuernberg, Germany.
 */

#include <ctype.h>
#include <glib/gi18n.h>

#include "helpers.h"
#include "nma-ui-utils.h"
#include "ui-helpers.h"
#include "ws-sae.h"
#include "wireless-security.h"

#define WPA_PMK_LEN 32

struct _WirelessSecuritySAE
{
  GtkGrid         parent;

  GtkEntry       *password_entry;
  GtkLabel       *password_label;
  GtkCheckButton *show_password_check;
  GtkComboBox    *type_combo;
  GtkLabel       *type_label;
};

static void wireless_security_iface_init (WirelessSecurityInterface *);

G_DEFINE_TYPE_WITH_CODE (WirelessSecuritySAE, ws_sae, GTK_TYPE_GRID,
                         G_IMPLEMENT_INTERFACE (wireless_security_get_type (), wireless_security_iface_init));

static void
show_toggled_cb (WirelessSecuritySAE *self)
{
  gboolean visible;

  visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
  gtk_entry_set_visibility (self->password_entry, visible);
}

static gboolean
validate (WirelessSecurity *security, GError **error)
{
  WirelessSecuritySAE *self = WS_SAE (security);
  NMSettingSecretFlags secret_flags;
  const char *key;

  secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
  if (secret_flags & NM_SETTING_SECRET_FLAG_NOT_SAVED)
    {
      widget_unset_error (GTK_WIDGET (self->password_entry));
      return TRUE;
  }

  key = gtk_entry_get_text (self->password_entry);

  if (key == NULL || key[0] == '\0')
    {
      widget_set_error (GTK_WIDGET (self->password_entry));
      g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Wi-Fi password is missing."));
      return FALSE;
    }
  widget_unset_error (GTK_WIDGET (self->password_entry));

  return TRUE;
}

static void
add_to_size_group (WirelessSecurity *security, GtkSizeGroup *group)
{
  WirelessSecuritySAE *self = WS_SAE (security);

  gtk_size_group_add_widget (group, GTK_WIDGET (self->type_label));
  gtk_size_group_add_widget (group, GTK_WIDGET (self->password_label));
}

static void
fill_connection (WirelessSecurity *security, NMConnection *connection)
{
  WirelessSecuritySAE *self = WS_SAE (security);
  const char *key;
  NMSettingWireless *s_wireless;
  NMSettingWirelessSecurity *s_wireless_sec;
  NMSettingSecretFlags secret_flags;
  const char *mode;
  gboolean is_adhoc = FALSE;

  s_wireless = nm_connection_get_setting_wireless (connection);
  g_assert (s_wireless);

  mode = nm_setting_wireless_get_mode (s_wireless);
  if (mode && !g_strcmp0 (mode, "adhoc"))
    is_adhoc = TRUE;

  /* Blow away the old security setting by adding a clear one */
  s_wireless_sec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
  nm_connection_add_setting (connection, (NMSetting *) s_wireless_sec);

  key = gtk_entry_get_text (self->password_entry);
  g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_PSK, key, NULL);

  /* Save PSK_FLAGS to the connection */
  secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
  nm_setting_set_secret_flags (NM_SETTING (s_wireless_sec), NM_SETTING_WIRELESS_SECURITY_PSK,
                               secret_flags, NULL);

  /* Update secret flags and popup when editing the connection */
  nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), secret_flags,
                                     NM_SETTING (s_wireless_sec), NM_SETTING_WIRELESS_SECURITY_PSK);

  wireless_security_clear_ciphers (connection);
  if (is_adhoc)
	  {
	    /* Ad-Hoc settings as specified by the supplicant */
	    g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "sae", NULL);
	    nm_setting_wireless_security_add_proto (s_wireless_sec, "rsn");
	    nm_setting_wireless_security_add_pairwise (s_wireless_sec, "ccmp");
	    nm_setting_wireless_security_add_group (s_wireless_sec, "ccmp");
	  }
  else
	  {
	    g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "sae", NULL);

	    /* Just leave ciphers and protocol empty, the supplicant will
	    * figure that out magically based on the AP IEs and card capabilities.
	    */
	  }
}

static gboolean
adhoc_compatible (WirelessSecurity *security)
{
  return FALSE;
}

static void
changed_cb (WirelessSecuritySAE *self)
{
  wireless_security_notify_changed ((WirelessSecurity *) self);
}

void
ws_sae_init (WirelessSecuritySAE *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
ws_sae_class_init (WirelessSecuritySAEClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/ControlCenter/network/ws-sae.ui");

  gtk_widget_class_bind_template_child (widget_class, WirelessSecuritySAE, password_entry);
  gtk_widget_class_bind_template_child (widget_class, WirelessSecuritySAE, password_label);
  gtk_widget_class_bind_template_child (widget_class, WirelessSecuritySAE, show_password_check);
  gtk_widget_class_bind_template_child (widget_class, WirelessSecuritySAE, type_combo);
  gtk_widget_class_bind_template_child (widget_class, WirelessSecuritySAE, type_label);
}

static void
wireless_security_iface_init (WirelessSecurityInterface *iface)
{
  iface->validate = validate;
  iface->add_to_size_group = add_to_size_group;
  iface->fill_connection = fill_connection;
  iface->adhoc_compatible = adhoc_compatible;
}

WirelessSecuritySAE *
ws_sae_new (NMConnection *connection)
{
  WirelessSecuritySAE *self;
  NMSetting *setting = NULL;

  self = g_object_new (ws_sae_get_type (), NULL);

  g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);
  gtk_entry_set_width_chars (self->password_entry, 28);

  /* Create password-storage popup menu for password entry under entry's secondary icon */
  if (connection)
	setting = (NMSetting *) nm_connection_get_setting_wireless_security (connection);
  nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry),
                                    0, setting, NM_SETTING_WIRELESS_SECURITY_PSK,
                                    FALSE, FALSE);

  /* Fill secrets, if any */
  if (connection)
	  {
	    helper_fill_secret_entry (connection,
                                self->password_entry,
                                NM_TYPE_SETTING_WIRELESS_SECURITY,
                                (HelperSecretFunc) nm_setting_wireless_security_get_psk);
	  }

  g_signal_connect (self->show_password_check, "toggled",
                    G_CALLBACK (show_toggled_cb), self);

  /* Hide WPA/RSN for now since this can be autodetected by NM and the
   * supplicant when connecting to the AP.
   */
  gtk_widget_hide (GTK_WIDGET (self->type_combo));
  gtk_widget_hide (GTK_WIDGET (self->type_label));

  return self;
}
