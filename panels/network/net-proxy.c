/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
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
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "net-proxy.h"

#define NET_PROXY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_PROXY, NetProxyPrivate))

typedef enum
{
        MODE_DISABLED,
        MODE_MANUAL,
        MODE_AUTOMATIC,
        N_MODES
} ProxyMode;

struct _NetProxyPrivate
{
        GSettings        *settings;
        GtkBuilder       *builder;
        GtkToggleButton  *mode_radios[3];
};

G_DEFINE_TYPE (NetProxy, net_proxy, NET_TYPE_OBJECT)

static const gchar *
panel_get_string_for_value (ProxyMode mode)
{
        switch (mode) {
        case MODE_DISABLED:
                return _("Off");
        case MODE_MANUAL:
                return _("Manual");
        case MODE_AUTOMATIC:
                return _("Automatic");
        default:
                g_assert_not_reached ();
        }
}

static inline void
panel_update_status_label (NetProxy  *self,
                           ProxyMode  mode)
{
        GtkLabel *label;

        /* update the label */
        label = GTK_LABEL (gtk_builder_get_object (self->priv->builder, "status_label"));
        gtk_label_set_label (label, panel_get_string_for_value (mode));
}

static void
check_wpad_warning (NetProxy *proxy)
{
        GtkWidget *widget;
        gchar *autoconfig_url = NULL;
        GString *string = NULL;
        gboolean ret = FALSE;
        guint mode;

        string = g_string_new ("");

        /* check we're using 'Automatic' */
        mode = g_settings_get_enum (proxy->priv->settings, "mode");
        if (mode != MODE_AUTOMATIC)
                goto out;

        /* see if the PAC is blank */
        autoconfig_url = g_settings_get_string (proxy->priv->settings,
                                                "autoconfig-url");
        ret = autoconfig_url == NULL ||
              autoconfig_url[0] == '\0';
        if (!ret)
                goto out;

        g_string_append (string, "<small>");

        /* TRANSLATORS: this is when the use leaves the PAC textbox blank */
        g_string_append (string, _("Web Proxy Autodiscovery is used when a Configuration URL is not provided."));

        g_string_append (string, "\n");

        /* TRANSLATORS: WPAD is bad: if you enable it on an untrusted
         * network, then anyone else on that network can tell your
         * machine that it should proxy all of your web traffic
         * through them. */
        g_string_append (string, _("This is not recommended for untrusted public networks."));
        g_string_append (string, "</small>");
out:
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "label_proxy_warning"));
        gtk_label_set_markup (GTK_LABEL (widget), string->str);
        gtk_widget_set_visible (widget, (string->len > 0));

        g_free (autoconfig_url);
        g_string_free (string, TRUE);
}

static void
settings_changed_cb (GSettings *settings,
                     const gchar *key,
                     NetProxy *proxy)
{
        check_wpad_warning (proxy);
}

static void
panel_proxy_mode_setup_widgets (NetProxy *proxy, ProxyMode value)
{
        GtkStack *stack;

        stack = GTK_STACK (gtk_builder_get_object (proxy->priv->builder, "stack"));

        /* hide or show the PAC text box */
        switch (value) {
        case MODE_DISABLED:
                gtk_stack_set_visible_child_name (stack, "disabled");
                break;
        case MODE_MANUAL:
                gtk_stack_set_visible_child_name (stack, "manual");
                break;
        case MODE_AUTOMATIC:
                gtk_stack_set_visible_child_name (stack, "automatic");
                break;
        default:
                g_assert_not_reached ();
        }

        /* perhaps show the wpad warning */
        check_wpad_warning (proxy);
}

static void
panel_proxy_mode_radio_changed_cb (GtkToggleButton *radio,
                                   NetProxy        *proxy)
{
        ProxyMode value;

        if (!gtk_toggle_button_get_active (radio))
                return;

        /* get selected radio */
        if (radio == proxy->priv->mode_radios[MODE_DISABLED])
                value = MODE_DISABLED;
        else if (radio == proxy->priv->mode_radios[MODE_MANUAL])
                value = MODE_MANUAL;
        else if (radio == proxy->priv->mode_radios[MODE_AUTOMATIC])
                value = MODE_AUTOMATIC;
        else
                g_assert_not_reached ();

        /* set */
        g_settings_set_enum (proxy->priv->settings, "mode", value);

        /* hide or show the correct widgets */
        panel_proxy_mode_setup_widgets (proxy, value);

        /* status label */
        panel_update_status_label (proxy, value);
}

static void
show_dialog_cb (GtkWidget *button,
                NetProxy  *self)
{
        GtkWidget *toplevel;
        GtkWindow *dialog;

        toplevel = gtk_widget_get_toplevel (button);
        dialog = GTK_WINDOW (gtk_builder_get_object (self->priv->builder, "dialog"));

        gtk_window_set_transient_for (dialog, GTK_WINDOW (toplevel));
        gtk_window_present (dialog);
}

static GtkWidget *
net_proxy_add_to_stack (NetObject    *object,
                        GtkStack     *stack,
                        GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        NetProxy *proxy = NET_PROXY (object);

        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "main_widget"));
        gtk_size_group_add_widget (heading_size_group, widget);
        gtk_stack_add_named (stack, widget, net_object_get_id (object));
        return widget;
}

static void
net_proxy_finalize (GObject *object)
{
        NetProxy *proxy = NET_PROXY (object);
        NetProxyPrivate *priv = proxy->priv;

        g_clear_object (&priv->settings);
        g_clear_object (&priv->builder);

        G_OBJECT_CLASS (net_proxy_parent_class)->finalize (object);
}

static void
net_proxy_class_init (NetProxyClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        NetObjectClass *parent_class = NET_OBJECT_CLASS (klass);

        object_class->finalize = net_proxy_finalize;
        parent_class->add_to_stack = net_proxy_add_to_stack;
        g_type_class_add_private (klass, sizeof (NetProxyPrivate));
}

static gboolean
get_ignore_hosts (GValue   *value,
                  GVariant *variant,
                  gpointer  user_data)
{
        GVariantIter iter;
        const gchar *s;
        gchar **av, **p;
        gsize n;

        n = g_variant_iter_init (&iter, variant);
        p = av = g_new0 (gchar *, n + 1);

        while (g_variant_iter_next (&iter, "&s", &s))
                if (s[0] != '\0') {
                        *p = (gchar *) s;
                        ++p;
                }

        g_value_take_string (value, g_strjoinv (", ", av));
        g_free (av);

        return TRUE;
}

static GVariant *
set_ignore_hosts (const GValue       *value,
                  const GVariantType *expected_type,
                  gpointer            user_data)
{
        GVariantBuilder builder;
        const gchar *sv;
        gchar **av, **p;

        sv = g_value_get_string (value);
        av = g_strsplit_set (sv, ", ", 0);

        g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);
        for (p = av; *p; ++p) {
                if (*p[0] != '\0')
                        g_variant_builder_add (&builder, "s", *p);
        }

        g_strfreev (av);

        return g_variant_builder_end (&builder);
}

static void
net_proxy_init (NetProxy *proxy)
{
        GtkAdjustment *adjustment;
        GSettings *settings_tmp;
        ProxyMode value;
        GtkWidget *widget;
        GError *error = NULL;
        guint i;

        proxy->priv = NET_PROXY_GET_PRIVATE (proxy);

        proxy->priv->builder = gtk_builder_new ();
        gtk_builder_add_from_resource (proxy->priv->builder,
                                       "/org/gnome/control-center/network/network-proxy.ui",
                                       &error);
        if (error != NULL) {
                g_warning ("Could not load interface file: %s", error->message);
                g_error_free (error);
                return;
        }

        proxy->priv->settings = g_settings_new ("org.gnome.system.proxy");
        g_signal_connect (proxy->priv->settings,
                          "changed",
                          G_CALLBACK (settings_changed_cb),
                          proxy);

        /* actions */
        value = g_settings_get_enum (proxy->priv->settings, "mode");

        /* bind the proxy values */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_url"));
        g_settings_bind (proxy->priv->settings, "autoconfig-url",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the HTTP proxy values */
        settings_tmp = g_settings_get_child (proxy->priv->settings, "http");
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_http"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (proxy->priv->builder,
                                                             "adjustment_proxy_port_http"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the HTTPS proxy values */
        settings_tmp = g_settings_get_child (proxy->priv->settings, "https");
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_https"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (proxy->priv->builder,
                                                             "adjustment_proxy_port_https"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the FTP proxy values */
        settings_tmp = g_settings_get_child (proxy->priv->settings, "ftp");
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_ftp"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (proxy->priv->builder,
                                                             "adjustment_proxy_port_ftp"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the SOCKS proxy values */
        settings_tmp = g_settings_get_child (proxy->priv->settings, "socks");
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_socks"));
        g_settings_bind (settings_tmp, "host",
                         widget, "text",
                         G_SETTINGS_BIND_DEFAULT);
        adjustment = GTK_ADJUSTMENT (gtk_builder_get_object (proxy->priv->builder,
                                                             "adjustment_proxy_port_socks"));
        g_settings_bind (settings_tmp, "port",
                         adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);
        g_object_unref (settings_tmp);

        /* bind the proxy ignore hosts */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_ignore"));
        g_settings_bind_with_mapping (proxy->priv->settings, "ignore-hosts",
                                      widget, "text",
                                      G_SETTINGS_BIND_DEFAULT, get_ignore_hosts, set_ignore_hosts,
                                      NULL, NULL);

        /* radio buttons */
        proxy->priv->mode_radios[MODE_DISABLED] =
                GTK_TOGGLE_BUTTON (gtk_builder_get_object (proxy->priv->builder, "radio_none"));
        proxy->priv->mode_radios[MODE_MANUAL] =
                GTK_TOGGLE_BUTTON (gtk_builder_get_object (proxy->priv->builder, "radio_manual"));
        proxy->priv->mode_radios[MODE_AUTOMATIC] =
                GTK_TOGGLE_BUTTON (gtk_builder_get_object (proxy->priv->builder, "radio_automatic"));

        /* setup the radio before connecting to the :toggled signal */
        gtk_toggle_button_set_active (proxy->priv->mode_radios[value], TRUE);
        panel_proxy_mode_setup_widgets (proxy, value);
        panel_update_status_label (proxy, value);

        for (i = MODE_DISABLED; i < N_MODES; i++) {
                g_signal_connect (proxy->priv->mode_radios[i],
                                  "toggled",
                                  G_CALLBACK (panel_proxy_mode_radio_changed_cb),
                                  proxy);
        }

        /* show dialog button */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder, "dialog_button"));

        g_signal_connect (widget,
                          "clicked",
                          G_CALLBACK (show_dialog_cb),
                          proxy);

        /* prevent the dialog from being destroyed */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder, "dialog"));

        g_signal_connect (widget,
                          "delete-event",
                          G_CALLBACK (gtk_widget_hide_on_delete),
                          widget);
}

NetProxy *
net_proxy_new (void)
{
        NetProxy *proxy;
        proxy = g_object_new (NET_TYPE_PROXY,
                              "removable", FALSE,
                              "id", "proxy",
                              NULL);
        return NET_PROXY (proxy);
}
