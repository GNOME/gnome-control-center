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

typedef enum
{
        MODE_DISABLED,
        MODE_MANUAL,
        MODE_AUTOMATIC
} ProxyMode;

struct _NetProxy
{
        GtkFrame          parent;

        GtkRadioButton   *automatic_radio;
        GtkDialog        *dialog;
        GtkButton        *dialog_button;
        GtkRadioButton   *manual_radio;
        GtkRadioButton   *none_radio;
        GtkEntry         *proxy_ftp_entry;
        GtkEntry         *proxy_http_entry;
        GtkEntry         *proxy_https_entry;
        GtkEntry         *proxy_ignore_entry;
        GtkAdjustment    *proxy_port_ftp_adjustment;
        GtkAdjustment    *proxy_port_http_adjustment;
        GtkAdjustment    *proxy_port_https_adjustment;
        GtkAdjustment    *proxy_port_socks_adjustment;
        GtkEntry         *proxy_socks_entry;
        GtkEntry         *proxy_url_entry;
        GtkLabel         *proxy_warning_label;
        GtkStack         *stack;
        GtkLabel         *status_label;

        GSettings        *settings;
};

G_DEFINE_TYPE (NetProxy, net_proxy, GTK_TYPE_FRAME)

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
        gtk_label_set_label (self->status_label, panel_get_string_for_value (mode));
}

static void
check_wpad_warning (NetProxy *self)
{
        g_autofree gchar *autoconfig_url = NULL;
        GString *string = NULL;
        gboolean ret = FALSE;
        guint mode;

        string = g_string_new ("");

        /* check we're using 'Automatic' */
        mode = g_settings_get_enum (self->settings, "mode");
        if (mode != MODE_AUTOMATIC)
                goto out;

        /* see if the PAC is blank */
        autoconfig_url = g_settings_get_string (self->settings,
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
        gtk_label_set_markup (self->proxy_warning_label, string->str);
        gtk_widget_set_visible (GTK_WIDGET (self->proxy_warning_label), (string->len > 0));

        g_string_free (string, TRUE);
}

static void
settings_changed_cb (NetProxy *self)
{
        check_wpad_warning (self);
}

static void
panel_proxy_mode_setup_widgets (NetProxy *self, ProxyMode value)
{
        /* hide or show the PAC text box */
        switch (value) {
        case MODE_DISABLED:
                gtk_stack_set_visible_child_name (self->stack, "disabled");
                break;
        case MODE_MANUAL:
                gtk_stack_set_visible_child_name (self->stack, "manual");
                break;
        case MODE_AUTOMATIC:
                gtk_stack_set_visible_child_name (self->stack, "automatic");
                break;
        default:
                g_assert_not_reached ();
        }

        /* perhaps show the wpad warning */
        check_wpad_warning (self);
}

static void
panel_proxy_mode_radio_changed_cb (NetProxy *self, GtkRadioButton *radio)
{
        ProxyMode value;

        if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (radio)))
                return;

        /* get selected radio */
        if (radio == self->none_radio)
                value = MODE_DISABLED;
        else if (radio == self->manual_radio)
                value = MODE_MANUAL;
        else if (radio == self->automatic_radio)
                value = MODE_AUTOMATIC;
        else
                g_assert_not_reached ();

        /* set */
        g_settings_set_enum (self->settings, "mode", value);

        /* hide or show the correct widgets */
        panel_proxy_mode_setup_widgets (self, value);

        /* status label */
        panel_update_status_label (self, value);
}

static void
show_dialog_cb (NetProxy *self)
{
        gtk_window_set_transient_for (GTK_WINDOW (self->dialog), GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
        gtk_window_present (GTK_WINDOW (self->dialog));
}

static void
net_proxy_finalize (GObject *object)
{
        NetProxy *self = NET_PROXY (object);

        g_clear_object (&self->settings);

        G_OBJECT_CLASS (net_proxy_parent_class)->finalize (object);
}

static void
net_proxy_class_init (NetProxyClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = net_proxy_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-proxy.ui");

        gtk_widget_class_bind_template_child (widget_class, NetProxy, automatic_radio);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, dialog);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, dialog_button);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, manual_radio);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, none_radio);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_ftp_entry);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_http_entry);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_https_entry);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_ignore_entry);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_port_ftp_adjustment);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_port_http_adjustment);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_port_https_adjustment);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_port_socks_adjustment);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_socks_entry);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_url_entry);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, proxy_warning_label);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, stack);
        gtk_widget_class_bind_template_child (widget_class, NetProxy, status_label);
}

static gboolean
get_ignore_hosts (GValue   *value,
                  GVariant *variant,
                  gpointer  user_data)
{
        GVariantIter iter;
        const gchar *s;
        g_autofree gchar **av = NULL;
        gchar **p;
        gsize n;

        n = g_variant_iter_init (&iter, variant);
        p = av = g_new0 (gchar *, n + 1);

        while (g_variant_iter_next (&iter, "&s", &s))
                if (s[0] != '\0') {
                        *p = (gchar *) s;
                        ++p;
                }

        g_value_take_string (value, g_strjoinv (", ", av));

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
net_proxy_init (NetProxy *self)
{
        g_autoptr(GSettings) http_settings = NULL;
        g_autoptr(GSettings) https_settings = NULL;
        g_autoptr(GSettings) ftp_settings = NULL;
        g_autoptr(GSettings) socks_settings = NULL;
        ProxyMode value;

        gtk_widget_init_template (GTK_WIDGET (self));

        self->settings = g_settings_new ("org.gnome.system.proxy");
        g_signal_connect_object (self->settings,
                                 "changed",
                                 G_CALLBACK (settings_changed_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

        /* actions */
        value = g_settings_get_enum (self->settings, "mode");

        /* bind the proxy values */
        g_settings_bind (self->settings, "autoconfig-url",
                         self->proxy_url_entry, "text",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the HTTP proxy values */
        http_settings = g_settings_get_child (self->settings, "http");
        g_settings_bind (http_settings, "host",
                         self->proxy_http_entry, "text",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (http_settings, "port",
                         self->proxy_port_http_adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the HTTPS proxy values */
        https_settings = g_settings_get_child (self->settings, "https");
        g_settings_bind (https_settings, "host",
                         self->proxy_https_entry, "text",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (https_settings, "port",
                         self->proxy_port_https_adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the FTP proxy values */
        ftp_settings = g_settings_get_child (self->settings, "ftp");
        g_settings_bind (ftp_settings, "host",
                         self->proxy_ftp_entry, "text",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (ftp_settings, "port",
                         self->proxy_port_ftp_adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the SOCKS proxy values */
        socks_settings = g_settings_get_child (self->settings, "socks");
        g_settings_bind (socks_settings, "host",
                         self->proxy_socks_entry, "text",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (socks_settings, "port",
                         self->proxy_port_socks_adjustment, "value",
                         G_SETTINGS_BIND_DEFAULT);

        /* bind the proxy ignore hosts */
        g_settings_bind_with_mapping (self->settings, "ignore-hosts",
                                      self->proxy_ignore_entry, "text",
                                      G_SETTINGS_BIND_DEFAULT, get_ignore_hosts, set_ignore_hosts,
                                      NULL, NULL);

        /* setup the radio before connecting to the :toggled signal */
        switch (value) {
        case MODE_DISABLED:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->none_radio), TRUE);
                break;
        case MODE_MANUAL:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->manual_radio), TRUE);
                break;
        case MODE_AUTOMATIC:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->automatic_radio), TRUE);
                break;
        default:
                g_assert_not_reached ();
        }
        panel_proxy_mode_setup_widgets (self, value);
        panel_update_status_label (self, value);

        g_signal_connect_object (self->none_radio, "toggled", G_CALLBACK (panel_proxy_mode_radio_changed_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->manual_radio, "toggled", G_CALLBACK (panel_proxy_mode_radio_changed_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->automatic_radio, "toggled", G_CALLBACK (panel_proxy_mode_radio_changed_cb), self, G_CONNECT_SWAPPED);

        /* show dialog button */
        g_signal_connect_object (self->dialog_button,
                                 "clicked",
                                 G_CALLBACK (show_dialog_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

        /* prevent the dialog from being destroyed */
        g_signal_connect_object (self->dialog,
                                 "delete-event",
                                 G_CALLBACK (gtk_widget_hide_on_delete),
                                 self->dialog,
                                 G_CONNECT_SWAPPED);
}

NetProxy *
net_proxy_new (void)
{
        return g_object_new (net_proxy_get_type (), NULL);
}
