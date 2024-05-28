/*
 * Copyright © 2018 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "cc-qr-code-dialog.h"
#include "cc-qr-code.h"

#define QR_IMAGE_SIZE 200

struct _CcQrCodeDialog
{
  AdwDialog     parent_instance;
  NMConnection *connection;
  AdwActionRow *network_name_row;
  AdwActionRow *network_password_row;
  GtkWidget    *qr_image;
};

enum
{
  PROP_0,
  PROP_CONNECTION,
  PROP_LAST
};

G_DEFINE_TYPE (CcQrCodeDialog, cc_qr_code_dialog, ADW_TYPE_DIALOG)

static GParamSpec *props[PROP_LAST];

static void
cc_qr_code_dialog_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  CcQrCodeDialog *self = CC_QR_CODE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_qr_code_dialog_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  CcQrCodeDialog *self = CC_QR_CODE_DIALOG (object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_assert (self->connection == NULL);
      self->connection = g_value_dup_object (value);
      g_assert (self->connection != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_qr_code_dialog_constructed (GObject *object)
{
  g_autoptr (CcQrCode) qr_code = NULL;
  g_autoptr (GVariant) variant = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree gchar *qr_connection_string = NULL;
  g_autofree gchar *subtitle_text = NULL;
  g_autofree gchar *ssid_text = NULL;
  g_autofree gchar *password_text = NULL;


  NMSettingWireless *setting;
  CcQrCodeDialog *self;
  GBytes *ssid;

  self = CC_QR_CODE_DIALOG (object);

  G_OBJECT_CLASS (cc_qr_code_dialog_parent_class)->constructed (object);

  variant = nm_remote_connection_get_secrets (NM_REMOTE_CONNECTION (self->connection),
                                              NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                              NULL,
                                              &error);
  if (variant)
    {
      if (!nm_connection_update_secrets (self->connection,
                                         NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                         variant,
                                         &error))
        {
          g_warning ("Couldn't update secrets: %s", error->message);
          return;
        }
    }
  else
    {
      if (!g_error_matches (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_SETTING_NOT_FOUND))
        {
          g_warning ("Couldn't get secrets: %s", error->message);
          return;
        }
    }

  setting = nm_connection_get_setting_wireless (self->connection);
  ssid = nm_setting_wireless_get_ssid (setting);
  ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
  password_text = get_wifi_password (self->connection);

  adw_action_row_set_subtitle (self->network_name_row, ssid_text);

  if (password_text)
    {
      adw_action_row_set_subtitle (self->network_password_row, password_text);
    }
  else
    gtk_widget_set_visible (GTK_WIDGET (self->network_password_row), FALSE);

  qr_code = cc_qr_code_new ();
  qr_connection_string = get_qr_string_for_connection (self->connection);
  if (cc_qr_code_set_text (qr_code, qr_connection_string))
    {
      gint scale = gtk_widget_get_scale_factor (self->qr_image);
      GdkPaintable *paintable = cc_qr_code_get_paintable (qr_code, QR_IMAGE_SIZE * scale);
      gtk_picture_set_paintable (GTK_PICTURE (self->qr_image), paintable);
    }
  else
    {
      // TODO what should happen in this case?
    }
}

static void
cc_qr_code_dialog_finalize (GObject *object)
{
  CcQrCodeDialog *self = CC_QR_CODE_DIALOG (object);

  g_clear_object (&self->connection);

  G_OBJECT_CLASS (cc_qr_code_dialog_parent_class)->finalize (object);
}

void
cc_qr_code_dialog_class_init (CcQrCodeDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = cc_qr_code_dialog_constructed;
  object_class->get_property = cc_qr_code_dialog_get_property;
  object_class->set_property = cc_qr_code_dialog_set_property;
  object_class->finalize = cc_qr_code_dialog_finalize;

  props[PROP_CONNECTION] = g_param_spec_object ("connection", "Connection",
                                                "The NMConnection for which to show a QR code",
                                                NM_TYPE_CONNECTION,
                                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, props);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-qr-code-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, CcQrCodeDialog, network_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcQrCodeDialog, network_password_row);
  gtk_widget_class_bind_template_child (widget_class, CcQrCodeDialog, qr_image);
}

void
cc_qr_code_dialog_init (CcQrCodeDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget *
cc_qr_code_dialog_new (NMConnection *connection)
{
  return g_object_new (CC_TYPE_QR_CODE_DIALOG,
                       "connection", connection,
                       NULL);
}
