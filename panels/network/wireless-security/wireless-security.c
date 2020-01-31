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

#include <glib/gi18n.h>

#include "helpers.h"
#include "wireless-security.h"
#include "wireless-security-resources.h"

G_DEFINE_INTERFACE (WirelessSecurity, wireless_security, G_TYPE_OBJECT)

enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
wireless_security_default_init (WirelessSecurityInterface *iface)
{
	g_resources_register (wireless_security_get_resource ());

        signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_INTERFACE (iface),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
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

	result = WIRELESS_SECURITY_GET_IFACE (self)->validate (self, error);
	if (!result && error && !*error)
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("Unknown error validating 802.1X security"));
	return result;
}

void
wireless_security_add_to_size_group (WirelessSecurity *self, GtkSizeGroup *group)
{
	g_return_if_fail (WIRELESS_IS_SECURITY (self));
	g_return_if_fail (GTK_IS_SIZE_GROUP (group));

	return WIRELESS_SECURITY_GET_IFACE (self)->add_to_size_group (self, group);
}

void
wireless_security_fill_connection (WirelessSecurity *self,
                                   NMConnection *connection)
{
	g_return_if_fail (WIRELESS_IS_SECURITY (self));
	g_return_if_fail (connection != NULL);

	return WIRELESS_SECURITY_GET_IFACE (self)->fill_connection (self, connection);
}

gboolean
wireless_security_adhoc_compatible (WirelessSecurity *self)
{
	if (WIRELESS_SECURITY_GET_IFACE (self)->adhoc_compatible)
		return WIRELESS_SECURITY_GET_IFACE (self)->adhoc_compatible (self);
	else
		return TRUE;
}
