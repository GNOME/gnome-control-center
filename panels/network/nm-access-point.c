/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "nm-access-point.h"

#define NM_ACCESS_POINT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_ACCESS_POINT, NmAccessPointPrivate))

/**
 * NmAccessPointPrivate:
 *
 * Private #NmAccessPoint data
 **/
struct _NmAccessPointPrivate
{
        GCancellable                        *cancellable;
        NmAccessPointMode                 mode;
        gchar                                *object_path;
        GDBusProxy                      *proxy;
        gchar                           *ssid;
        guint                            strength;
};

enum {
        SIGNAL_CHANGED,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
G_DEFINE_TYPE (NmAccessPoint, nm_access_point, G_TYPE_OBJECT)

/**
 * nm_access_point_mode_to_localized_string:
 **/
const gchar *
nm_access_point_mode_to_localized_string (NmAccessPointMode mode)
{
        const gchar *value = NULL;
        switch (mode) {
        case NM_ACCESS_POINT_MODE_UNKNOWN:
                /* TRANSLATORS: AP type */
                value = _("Unknown");
                break;
        case NM_ACCESS_POINT_MODE_ADHOC:
                /* TRANSLATORS: AP type */
                value = _("Ad-hoc");
                break;
        case NM_ACCESS_POINT_MODE_INFRA:
                /* TRANSLATORS: AP type */
                value = _("Infrastructure");
                break;
        default:
                break;
        }
        return value;
}

/**
 * nm_access_point_get_mode:
 **/
NmAccessPointMode
nm_access_point_get_mode (NmAccessPoint *access_point)
{
        GVariant *variant;

        g_return_val_if_fail (NM_IS_ACCESS_POINT (access_point), 0);

        /* get the mode */
        variant = g_dbus_proxy_get_cached_property (access_point->priv->proxy,
                                                    "Mode");
        access_point->priv->mode = g_variant_get_uint32 (variant);
        g_variant_unref (variant);
        return access_point->priv->mode;
}

/**
 * nm_access_point_get_strength:
 **/
guint
nm_access_point_get_strength (NmAccessPoint *access_point)
{
        GVariant *variant;

        g_return_val_if_fail (NM_IS_ACCESS_POINT (access_point), 0);

        /* get the strength */
        variant = g_dbus_proxy_get_cached_property (access_point->priv->proxy,
                                                    "Strength");
        access_point->priv->strength = g_variant_get_byte (variant);
        g_variant_unref (variant);

        return access_point->priv->strength;
}

/**
 * nm_access_point_get_object_path:
 **/
const gchar *
nm_access_point_get_object_path (NmAccessPoint *access_point)
{
        g_return_val_if_fail (NM_IS_ACCESS_POINT (access_point), NULL);
        return access_point->priv->object_path;
}

/**
 * nm_access_point_get_ssid:
 **/
const gchar *
nm_access_point_get_ssid (NmAccessPoint *access_point)
{
        gchar tmp;
        gsize len;
        guint i = 0;
        GVariantIter iter;
        GVariant *value = NULL;
        NmAccessPointPrivate *priv = access_point->priv;

        g_return_val_if_fail (NM_IS_ACCESS_POINT (access_point), NULL);

        /* clear */
        g_free (priv->ssid);
        priv->ssid = NULL;

        /* get the (non NULL terminated, urgh) SSID */
        value = g_dbus_proxy_get_cached_property (priv->proxy, "Ssid");
        len = g_variant_iter_init (&iter, value);
        if (len == 0) {
                g_warning ("invalid ssid?!");
                goto out;
        }

        /* decode each byte */
        priv->ssid = g_new0 (gchar, len + 1);
        while (g_variant_iter_loop (&iter, "y", &tmp))
                priv->ssid[i++] = tmp;
        g_debug ("adding access point %s (%i%%) [%i]",
                 priv->ssid,
                 priv->strength,
                 priv->mode);

out:
        if (value != NULL)
                g_variant_unref (value);
        return access_point->priv->ssid;
}

/**
 * nm_access_point_emit_changed:
 **/
static void
nm_access_point_emit_changed (NmAccessPoint *access_point)
{
        g_debug ("NmAccessPoint: emit 'changed' for %s",
                 access_point->priv->object_path);
        g_signal_emit (access_point, signals[SIGNAL_CHANGED], 0);
}

/**
 * nm_access_point_got_proxy_cb:
 **/
static void
nm_access_point_got_proxy_cb (GObject *source_object,
                              GAsyncResult *res,
                              gpointer user_data)
{
        GError *error = NULL;
        NmAccessPoint *access_point = (NmAccessPoint *) user_data;
        NmAccessPointPrivate *priv = access_point->priv;

        priv->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (priv->proxy == NULL) {
                g_printerr ("Error creating proxy: %s\n", error->message);
                g_error_free (error);
                goto out;
        }

        /* emit changed */
        nm_access_point_emit_changed (access_point);
out:
        return;
}

/**
 * nm_access_point_refresh:
 *
 * 100% async.
 **/
void
nm_access_point_refresh (NmAccessPoint *access_point,
                         const gchar *object_path,
                         GCancellable *cancellable)
{
        access_point->priv->object_path = g_strdup (object_path);
        if (cancellable != NULL)
                access_point->priv->cancellable = g_object_ref (cancellable);
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.NetworkManager",
                                  object_path,
                                  "org.freedesktop.NetworkManager.AccessPoint",
                                  access_point->priv->cancellable,
                                  nm_access_point_got_proxy_cb,
                                  access_point);
}

/**
 * nm_access_point_finalize:
 **/
static void
nm_access_point_finalize (GObject *object)
{
        NmAccessPoint *access_point = NM_ACCESS_POINT (object);
        NmAccessPointPrivate *priv = access_point->priv;

        if (priv->proxy != NULL)
                g_object_unref (priv->proxy);
        if (priv->cancellable != NULL)
                g_object_unref (priv->cancellable);
        g_free (priv->ssid);
        g_free (priv->object_path);

        G_OBJECT_CLASS (nm_access_point_parent_class)->finalize (object);
}

/**
 * nm_access_point_class_init:
 **/
static void
nm_access_point_class_init (NmAccessPointClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = nm_access_point_finalize;

        /**
         * NmAccessPoint::changed:
         **/
        signals[SIGNAL_CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NmAccessPointClass, changed),
                              NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_type_class_add_private (klass, sizeof (NmAccessPointPrivate));
}

/**
 * nm_access_point_init:
 **/
static void
nm_access_point_init (NmAccessPoint *access_point)
{
        access_point->priv = NM_ACCESS_POINT_GET_PRIVATE (access_point);
}

/**
 * nm_access_point_new:
 **/
NmAccessPoint *
nm_access_point_new (void)
{
        NmAccessPoint *access_point;
        access_point = g_object_new (NM_TYPE_ACCESS_POINT, NULL);
        return NM_ACCESS_POINT (access_point);
}

