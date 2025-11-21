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

#include <gnome-qr/gnome-qr.h>

/**
 * SECTION: cc-qr_code
 * @title: CcQrCode
 * @short_description: A Simple QR Code wrapper around gnome-qr
 * @include: "cc-qr-code.h"
 *
 * Generate a QR image from a given text.
 */

struct _CcQrCode
{
  GObject     parent_instance;

  gchar      *text;
  GdkTexture *texture;
  gint        size;
};

G_DEFINE_TYPE (CcQrCode, cc_qr_code, G_TYPE_OBJECT)

static void
cc_qr_code_finalize (GObject *object)
{
  CcQrCode *self = (CcQrCode *) object;

  g_clear_object (&self->texture);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (cc_qr_code_parent_class)->finalize (object);
}

static void
cc_qr_code_class_init (CcQrCodeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_qr_code_finalize;
}

static void
cc_qr_code_init (CcQrCode *self)
{
}

CcQrCode *
cc_qr_code_new (void)
{
  return g_object_new (CC_TYPE_QR_CODE, NULL);
}

gboolean
cc_qr_code_set_text (CcQrCode    *self,
                     const gchar *text)
{
  g_return_val_if_fail (CC_IS_QR_CODE (self), FALSE);
  g_return_val_if_fail (!text || *text, FALSE);

  if (g_strcmp0 (text, self->text) == 0)
    return FALSE;

  g_clear_object (&self->texture);
  g_free (self->text);
  self->text = g_strdup (text);

  return TRUE;
}

GdkPaintable *
cc_qr_code_get_paintable (CcQrCode *self,
                          gint      size)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GError) error = NULL;
  static const GnomeQrPixelFormat format = GNOME_QR_PIXEL_FORMAT_RGB_888;
  size_t pixel_size;

  g_return_val_if_fail (CC_IS_QR_CODE (self), NULL);
  g_return_val_if_fail (size > 0, NULL);

  if (!self->text || !*self->text)
    {
      g_warn_if_reached ();
      cc_qr_code_set_text (self, "invalid text");
    }

  if (self->texture && self->size == size)
    return GDK_PAINTABLE (self->texture);

  self->size = size;

  bytes = gnome_qr_generate_qr_code_sync (self->text,
                                          size,
                                          NULL,
                                          NULL,
                                          format,
                                          GNOME_QR_ECC_LEVEL_LOW,
                                          &pixel_size,
                                          NULL,
                                          &error);
  if (!bytes)
    {
      g_warning ("Failed to generate QR code: %s", error->message);
      return NULL;
    }

  g_clear_object (&self->texture);
  self->texture = gdk_memory_texture_new (pixel_size,
                                          pixel_size,
                                          GDK_MEMORY_R8G8B8,
                                          bytes,
                                          pixel_size *
                                          GNOME_QR_BYTES_PER_FORMAT (format));

  return GDK_PAINTABLE (self->texture);
}

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

  /* No IEEE 802.1x */
  if (g_strcmp0 (key_mgmt, "none") == 0)
    return "WEP";

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

  if (g_str_equal (key_mgmt, "none") ||
      g_str_equal (key_mgmt, "wpa-psk") ||
      g_str_equal (key_mgmt, "sae"))
    return TRUE;

  return FALSE;
}

gchar *
get_wifi_password (NMConnection *c)
{
  NMSettingWirelessSecurity *setting;
  const gchar *sec_type, *password;
  gint wep_index;

  sec_type = get_connection_security_type (c);
  setting = nm_connection_get_setting_wireless_security (c);

  if (g_str_equal (sec_type, "nopass"))
    return NULL;

  if (g_str_equal (sec_type, "WEP"))
    {
      wep_index = nm_setting_wireless_security_get_wep_tx_keyidx (setting);
      password = nm_setting_wireless_security_get_wep_key (setting, wep_index);
    }
  else
    {
      password = nm_setting_wireless_security_get_psk (setting);
    }

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
