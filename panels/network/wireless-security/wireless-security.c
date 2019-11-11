/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
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
 * Copyright 2007 - 2014 Red Hat, Inc.
 */

#include "nm-default.h"

#include <string.h>

#include "wireless-security.h"
#include "wireless-security-resources.h"
#include "utils.h"

typedef struct  {
	char *username, *password;
	gboolean always_ask, show_password;
} WirelessSecurityPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (WirelessSecurity, wireless_security, G_TYPE_OBJECT)

enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
wireless_security_dispose (GObject *object)
{
	WirelessSecurity *self = WIRELESS_SECURITY (object);
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	if (priv->password)
		memset (priv->password, 0, strlen (priv->password));

	g_clear_pointer (&priv->username, g_free);
	g_clear_pointer (&priv->password, g_free);

	G_OBJECT_CLASS (wireless_security_parent_class)->dispose (object);
}

void
wireless_security_init (WirelessSecurity *self)
{
	g_resources_register (wireless_security_get_resource ());
}

void
wireless_security_class_init (WirelessSecurityClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = wireless_security_dispose;

        signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
}

GtkWidget *
wireless_security_get_widget (WirelessSecurity *self)
{
	g_return_val_if_fail (WIRELESS_IS_SECURITY (self), NULL);

	return WIRELESS_SECURITY_GET_CLASS (self)->get_widget (self);
}

void
wireless_security_notify_changed (WirelessSecurity *self)
{
        g_return_if_fail (WIRELESS_IS_SECURITY (self));

        g_signal_emit (self, signals[CHANGED], 0);
}

gboolean
wireless_security_validate (WirelessSecurity *self, GError **error)
{
	gboolean result;

	g_return_val_if_fail (WIRELESS_IS_SECURITY (self), FALSE);
	g_return_val_if_fail (!error || !*error, FALSE);

	result = WIRELESS_SECURITY_GET_CLASS (self)->validate (self, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Unknown error validating 802.1X security"));
	return result;
}

void
wireless_security_add_to_size_group (WirelessSecurity *self, GtkSizeGroup *group)
{
	g_return_if_fail (WIRELESS_IS_SECURITY (self));
	g_return_if_fail (GTK_IS_SIZE_GROUP (group));

	return WIRELESS_SECURITY_GET_CLASS (self)->add_to_size_group (self, group);
}

void
wireless_security_fill_connection (WirelessSecurity *self,
                                   NMConnection *connection)
{
	g_return_if_fail (WIRELESS_IS_SECURITY (self));
	g_return_if_fail (connection != NULL);

	return WIRELESS_SECURITY_GET_CLASS (self)->fill_connection (self, connection);
}

gboolean
wireless_security_adhoc_compatible (WirelessSecurity *self)
{
	if (WIRELESS_SECURITY_GET_CLASS (self)->adhoc_compatible)
		return WIRELESS_SECURITY_GET_CLASS (self)->adhoc_compatible (self);
	else
		return TRUE;
}

void
wireless_security_set_username (WirelessSecurity *self, const gchar *username)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_if_fail (WIRELESS_IS_SECURITY (self));

	g_clear_pointer (&priv->username, g_free);
	priv->username = g_strdup (username);
}

const gchar *
wireless_security_get_username (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_val_if_fail (WIRELESS_IS_SECURITY (self), NULL);

	return priv->username;
}

void
wireless_security_set_password (WirelessSecurity *self, const gchar *password)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_if_fail (WIRELESS_IS_SECURITY (self));

	if (priv->password)
		memset (priv->password, 0, strlen (priv->password));

	g_clear_pointer (&priv->password, g_free);
	priv->password = g_strdup (password);
}

const gchar *
wireless_security_get_password (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_val_if_fail (WIRELESS_IS_SECURITY (self), NULL);

	return priv->password;
}

void
wireless_security_set_always_ask (WirelessSecurity *self, gboolean always_ask)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_if_fail (WIRELESS_IS_SECURITY (self));

	priv->always_ask = always_ask;
}

gboolean
wireless_security_get_always_ask (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_val_if_fail (WIRELESS_IS_SECURITY (self), FALSE);

	return priv->always_ask;
}

void
wireless_security_set_show_password (WirelessSecurity *self, gboolean show_password)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_if_fail (WIRELESS_IS_SECURITY (self));

	priv->show_password = show_password;
}

gboolean
wireless_security_get_show_password (WirelessSecurity *self)
{
	WirelessSecurityPrivate *priv = wireless_security_get_instance_private (self);

	g_return_val_if_fail (WIRELESS_IS_SECURITY (self), FALSE);

	return priv->show_password;
}

void
wireless_security_clear_ciphers (NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;

	g_return_if_fail (connection != NULL);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	nm_setting_wireless_security_clear_protos (s_wireless_sec);
	nm_setting_wireless_security_clear_pairwise (s_wireless_sec);
	nm_setting_wireless_security_clear_groups (s_wireless_sec);
}
