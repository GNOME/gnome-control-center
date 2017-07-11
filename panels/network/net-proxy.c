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

struct _NetProxyPrivate
{
        GSettings        *settings;
        GtkBuilder       *builder;
};

G_DEFINE_TYPE (NetProxy, net_proxy, NET_TYPE_OBJECT)

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
        if (mode != 2)
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
panel_proxy_mode_combo_setup_widgets (NetProxy *proxy, guint value)
{
        GtkWidget *widget;

        /* hide or show the PAC text box */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_url"));
        gtk_widget_set_visible (widget, value == 2);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_url"));
        gtk_widget_set_visible (widget, value == 2);

        /* hide or show the manual entry text boxes */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "spinbutton_proxy_http"));
        gtk_widget_set_visible (widget, value == 1);

        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_https"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_https"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "spinbutton_proxy_https"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "spinbutton_proxy_ftp"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "spinbutton_proxy_socks"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_ignore"));
        gtk_widget_set_visible (widget, value == 1);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_ignore"));
        gtk_widget_set_visible (widget, value == 1);

        /* perhaps show the wpad warning */
        check_wpad_warning (proxy);
}

static void
panel_set_value_for_combo (NetProxy *proxy, GtkComboBox *combo_box, gint value)
{
        gboolean ret;
        gint value_tmp;
        GtkTreeIter iter;
        GtkTreeModel *model;

        /* get entry */
        model = gtk_combo_box_get_model (combo_box);
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (!ret)
                return;

        /* try to make the UI match the setting */
        do {
                gtk_tree_model_get (model, &iter,
                                    1, &value_tmp,
                                    -1);
                if (value == value_tmp) {
                        gtk_combo_box_set_active_iter (combo_box, &iter);
                        break;
                }
        } while (gtk_tree_model_iter_next (model, &iter));

        /* hide or show the correct widgets */
        panel_proxy_mode_combo_setup_widgets (proxy, value);
}

static void
panel_proxy_mode_combo_changed_cb (GtkWidget *widget, NetProxy *proxy)
{
        gboolean ret;
        gint value;
        GtkTreeIter iter;
        GtkTreeModel *model;

        /* no selection */
        ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
        if (!ret)
                return;

        /* get entry */
        model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
        gtk_tree_model_get (model, &iter,
                            1, &value,
                            -1);

        /* set */
        g_settings_set_enum (proxy->priv->settings, "mode", value);

        /* hide or show the correct widgets */
        panel_proxy_mode_combo_setup_widgets (proxy, value);
}

static GtkWidget *
net_proxy_add_to_stack (NetObject    *object,
                        GtkStack     *stack,
                        GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        NetProxy *proxy = NET_PROXY (object);

        /* add widgets to size group */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "heading_proxy_method"));
        gtk_size_group_add_widget (heading_size_group, widget);

        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "grid5"));
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
        GError *error = NULL;
        gint value;
        GSettings *settings_tmp;
        GtkAdjustment *adjustment;
        GtkWidget *widget;

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
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "combobox_proxy_mode"));
        panel_set_value_for_combo (proxy, GTK_COMBO_BOX (widget), value);
        g_signal_connect (widget, "changed",
                          G_CALLBACK (panel_proxy_mode_combo_changed_cb),
                          proxy);

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

        /* set header to something sane */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "image_proxy_device"));
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      "preferences-system-network",
                                      GTK_ICON_SIZE_DIALOG);
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "label_proxy_device"));
        gtk_label_set_label (GTK_LABEL (widget),
                             _("Proxy"));
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "label_proxy_status"));
        gtk_label_set_label (GTK_LABEL (widget), "");

        /* bind the proxy ignore hosts */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "entry_proxy_ignore"));
        g_settings_bind_with_mapping (proxy->priv->settings, "ignore-hosts",
                                      widget, "text",
                                      G_SETTINGS_BIND_DEFAULT, get_ignore_hosts, set_ignore_hosts,
                                      NULL, NULL);

        /* hide the switch until we get some more detail in the mockup */
        widget = GTK_WIDGET (gtk_builder_get_object (proxy->priv->builder,
                                                     "device_proxy_off_switch"));
        if (widget != NULL)
                gtk_widget_hide (widget);
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
