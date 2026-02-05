/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* cc-qr-code.c
 *
 * Copyright 2019 Purism SPC
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-qr-code"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cc-qr-code.h"

static gchar *
escape_string (const gchar *str,
               gboolean     quote)
{
  GString *string;
  const char *next;

  if (!str)
    return NULL;

  string = g_string_new ("");
  if (quote)
    g_string_append_c (string, '"');

  while ((next = strpbrk (str, "\\;,:\"")))
    {
      g_string_append_len (string, str, next - str);
      g_string_append_c (string, '\\');
      g_string_append_c (string, *next);
      str = next + 1;
    }

  g_string_append (string, str);
  if (quote)
    g_string_append_c (string, '"');

  return g_string_free (string, FALSE);
}

static const gchar *
get_connection_security_type (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt;

  g_return_val_if_fail (c, "nopass");

  setting = nm_connection_get_setting_wireless_security (c);

  if (!setting)
    return "nopass";

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);

  if (g_strcmp0 (key_mgmt, "wpa-psk") == 0)
    return "WPA";

  if (g_strcmp0 (key_mgmt, "sae") == 0)
    return "SAE";

  return "nopass";
}

gboolean
is_qr_code_supported (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const char *key_mgmt;
  NMSettingConnection *s_con;
  guint64 timestamp;

  g_return_val_if_fail (c, TRUE);

  s_con = nm_connection_get_setting_connection (c);
  timestamp = nm_setting_connection_get_timestamp (s_con);

  /* Check timestamp to determine if connection was successful in the past */
  if (timestamp == 0)
    return FALSE;

  setting = nm_connection_get_setting_wireless_security (c);

  if (!setting)
    return TRUE;

  key_mgmt = nm_setting_wireless_security_get_key_mgmt (setting);

  if (g_str_equal (key_mgmt, "wpa-psk") ||
      g_str_equal (key_mgmt, "sae"))
    return TRUE;

  return FALSE;
}

gchar *
get_wifi_password (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const gchar *sec_type, *password;

  sec_type = get_connection_security_type (c);
  setting = nm_connection_get_setting_wireless_security (c);

  if (g_str_equal (sec_type, "nopass"))
    return NULL;

  password = nm_setting_wireless_security_get_psk (setting);

  return g_strdup (password);
}

/* Generate a string representing the connection
 * An example generated text:
 *     WIFI:S:ssid;T:WPA;P:my-valid-pass;H:true;
 * Where,
 *   S = ssid, T = security, P = password, H = hidden (Optional)
 *
 * See https://github.com/zxing/zxing/wiki/Barcode-Contents#wi-fi-network-config-android-ios-11
 */
gchar *
get_qr_string_for_connection (NMConnection *c)
{
  NMSettingWireless *setting;
  g_autofree char *ssid_text = NULL;
  g_autofree char *escaped_ssid = NULL;
  g_autofree char *password_str = NULL;
  g_autofree char *escaped_password = NULL;
  GString *string;
  GBytes *ssid;
  gboolean hidden;

  setting = nm_connection_get_setting_wireless (c);
  ssid = nm_setting_wireless_get_ssid (setting);

  if (!ssid)
    return NULL;

  string = g_string_new ("WIFI:S:");

  /* SSID */
  ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL),
                                     g_bytes_get_size (ssid));
  escaped_ssid = escape_string (ssid_text, FALSE);
  g_string_append (string, escaped_ssid);
  g_string_append_c (string, ';');

  /* Security type */
  g_string_append (string, "T:");
  g_string_append (string, get_connection_security_type (c));
  g_string_append_c (string, ';');

  /* Password */
  g_string_append (string, "P:");
  password_str = get_wifi_password (c);
  escaped_password = escape_string (password_str, FALSE);
  if (escaped_password)
    g_string_append (string, escaped_password);
  g_string_append_c (string, ';');

  /* WiFi Hidden */
  hidden = nm_setting_wireless_get_hidden (setting);
  if (hidden)
    g_string_append (string, "H:true");
  g_string_append_c (string, ';');

  return g_string_free (string, FALSE);
}
