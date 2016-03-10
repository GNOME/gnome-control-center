/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
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
 * (C) Copyright 2013 Red Hat, Inc.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "firewall-helpers.h"

typedef struct {
        gchar *zone;
        GtkWidget *combo;
        GtkWidget *label;
} GetZonesReplyData;

static void
get_zones_reply (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
        GDBusConnection *bus = G_DBUS_CONNECTION (source);
        GetZonesReplyData *d = user_data;
        GVariant *ret;
        GError *error = NULL;
        const gchar **zones;
        gint idx;
        gint i;

        ret = g_dbus_connection_call_finish (bus, res, &error);

        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->combo),
                                        C_("Firewall zone", "Default"));
        gtk_widget_set_tooltip_text (d->combo, _("The zone defines the trust level of the connection"));

        idx = 0;
        if (error) {
                gtk_widget_hide (d->combo);
                gtk_widget_hide (d->label);
                g_error_free (error);
        }
        else {
                gtk_widget_show (d->combo);
                gtk_widget_show (d->label);
                g_variant_get (ret, "(^a&s)", &zones);

                for (i = 0; zones[i]; i++) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->combo),
                                                        zones[i]);
                        if (g_strcmp0 (d->zone, zones[i]) == 0)
                                idx = i + 1;
                }
                if (d->zone && idx == 0) {
                        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->combo),
                                                        d->zone);
                        idx = i + 1;
                }
                g_variant_unref (ret);
        }
        gtk_combo_box_set_active (GTK_COMBO_BOX (d->combo), idx);

        g_free (d->zone);
        g_free (d);
}

void
firewall_ui_setup (NMSettingConnection *setting,
                   GtkWidget           *combo,
                   GtkWidget           *label,
                   GCancellable        *cancellable)
{
        GDBusConnection *bus;
        GetZonesReplyData *d;

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

        d = g_new0 (GetZonesReplyData, 1);
        d->zone = g_strdup (nm_setting_connection_get_zone (setting));
        d->combo = combo;
        d->label = label;

        g_dbus_connection_call (bus,
                                "org.fedoraproject.FirewallD1",
                                "/org/fedoraproject/FirewallD1",
                                "org.fedoraproject.FirewallD1.zone",
                                "getZones",
                                NULL,
                                NULL,
                                0,
                                G_MAXINT,
                                cancellable,
                                get_zones_reply, d);
        g_object_unref (bus);
}

void
firewall_ui_to_setting (NMSettingConnection *setting, GtkWidget *combo)
{
        gchar *zone;

        zone = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (combo));
        if (g_strcmp0 (zone, C_("Firewall zone", "Default")) == 0) {
                g_free (zone);
                zone = NULL;
        }

        g_object_set (setting, NM_SETTING_CONNECTION_ZONE, zone, NULL);
        g_free (zone);
}
