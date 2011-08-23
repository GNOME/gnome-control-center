/*
 * Copyright (C) 2011 Rodrigo Moya
 *
 * Written by: Rodrigo Moya <rodrigo@gnome.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <polkit/polkit.h>

#include <glib/gi18n.h>

#include <libgnomekbd/gkbd-keyboard-config.h>
#include "cc-common-language.h"
#include "gdm-languages.h"
#include "gnome-region-panel-system.h"
#include "gnome-region-panel-xkb.h"

static GSettings *locale_settings, *xkb_settings;
static GDBusProxy *localed_proxy;
static GPermission *localed_permission;

static void
locale_settings_changed (GSettings *settings,
			 const gchar *key,
			 gpointer user_data)
{
        GtkBuilder *builder = GTK_BUILDER (user_data);
        GtkWidget *label;
        gchar *region, *display_region;

        region = g_settings_get_string (locale_settings, "region");
        if (!region || !region[0]) {
                label = GTK_WIDGET (gtk_builder_get_object (builder, "user_display_language"));
                region = g_strdup ((gchar*)g_object_get_data (G_OBJECT (label), "language"));
        }

        display_region = gdm_get_region_from_name (region, NULL);
        label = GTK_WIDGET (gtk_builder_get_object (builder, "user_format"));
        gtk_label_set_text (GTK_LABEL (label), display_region);
        g_object_set_data_full (G_OBJECT (label), "region", g_strdup (region), g_free);
        g_free (region);
        g_free (display_region);
}

void
system_update_language (GtkBuilder *builder, const gchar *language)
{
        gchar *display_language;
        GtkWidget *label;

        display_language = gdm_get_language_from_name (language, NULL);
        label = (GtkWidget *)gtk_builder_get_object (builder, "user_display_language");
        gtk_label_set_text (GTK_LABEL (label), display_language);
        g_object_set_data_full (G_OBJECT (label), "language", g_strdup (language), g_free);
        g_free (display_language);

        /* need to update the region display in case the setting is '' */
        locale_settings_changed (locale_settings, "region", builder);
}

static void
xkb_settings_changed (GSettings *settings,
		      const gchar *key,
		      gpointer user_data)
{
	GtkBuilder *builder = GTK_BUILDER (user_data);
	gint i;
	GString *str = g_string_new ("");
	gchar **layouts = g_settings_get_strv (settings, "layouts");

	for (i = 0; i < G_N_ELEMENTS (layouts); i++) {
		gchar *utf_visible = xkb_layout_description_utf8 (layouts[i]);

		if (utf_visible != NULL) {
			if (str->str[0] != '\0') {
				str = g_string_append (str, ", ");
			}
			str = g_string_append (str, utf_visible);
			g_free (utf_visible);
		}
	}

	g_strfreev (layouts);

	gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (builder, "user_input_source")), str->str);
	g_string_free (str, TRUE);
}

static void
on_localed_properties_changed (GDBusProxy   *proxy,
                               GVariant     *changed_properties,
                               const gchar **invalidated_properties,
                               GtkBuilder   *builder)
{
        GVariant *res;
        GVariant *v;
	GError *error = NULL;

        res = g_dbus_connection_call_sync (g_dbus_proxy_get_connection (proxy),
                                           g_dbus_proxy_get_name (proxy),
                                            g_dbus_proxy_get_object_path (proxy),
                                           "org.freedesktop.DBus.Properties",
                                           "Get",
                                           g_variant_new ("(ss)",
                                                          g_dbus_proxy_get_interface_name (proxy),
                                                          "Locale"),
                                           NULL,
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1, NULL, &error);
	if (!res) {
		g_warning ("Failed to call Get method: %s", error->message);
		g_error_free (error);
		return;
	}

        v = g_variant_get_child_value (res, 0);
        if (v) {
                const gchar **strv;
                gsize len;
                gint i;
                const gchar *lang, *messages, *time;
                gchar *name;
                GtkWidget *label;
                GVariant *v2;

                v2 = g_variant_get_variant (v);
                strv = g_variant_get_strv (v2, &len);

                lang = messages = time = NULL;
                for (i = 0; strv[i]; i++) {
                        if (g_str_has_prefix (strv[i], "LANG=")) {
                                lang = strv[i] + strlen ("LANG=");
                        }
                        else if (g_str_has_prefix (strv[i], "LC_MESSAGES=")) {
                                messages = strv[i] + strlen ("LC_MESSAGES=");
                        }
                        else if (g_str_has_prefix (strv[i], "LC_TIME=")) {
                                time = strv[i] + strlen ("LC_TIME=");
                        }
                }
                if (!messages) {
                        messages = lang;
                }
                if (!time) {
                        time = lang;
                }

                if (messages) {
                        name = gdm_get_language_from_name (messages, NULL);
                        label = (GtkWidget*)gtk_builder_get_object (builder, "system_display_language");
                        gtk_label_set_text (GTK_LABEL (label), name);
                        g_free (name);
                }

                if (time) {
                        name = gdm_get_region_from_name (time, NULL);
                        label = (GtkWidget*)gtk_builder_get_object (builder, "system_format");
                        gtk_label_set_text (GTK_LABEL (label), name);
                        g_free (name);
                }
                g_variant_unref (v);
        }
        g_variant_unref (res);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      user_data)
{
        GtkBuilder *builder = user_data;
        GError *error = NULL;

        localed_proxy = g_dbus_proxy_new_finish (res, &error);

        if (!localed_proxy) {
                g_warning ("Failed to contact localed: %s\n", error->message);
                g_error_free (error);
                return;
        }

        g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, localed_proxy);

        g_signal_connect (localed_proxy, "g-properties-changed",
                          G_CALLBACK (on_localed_properties_changed), builder);

        on_localed_properties_changed (localed_proxy, NULL, NULL, builder);
}

static void
copy_settings (GtkButton *button, GtkBuilder *builder)
{
        const gchar *language;
        const gchar *region;
        GtkWidget *label;
        GVariantBuilder *b;
        gchar *s;

        label = GTK_WIDGET (gtk_builder_get_object (builder, "user_display_language"));
        language = g_object_get_data (G_OBJECT (label), "language");
        label = GTK_WIDGET (gtk_builder_get_object (builder, "user_format"));
        region = g_object_get_data (G_OBJECT (label), "region");

        b = g_variant_builder_new (G_VARIANT_TYPE ("as"));
        s = g_strconcat ("LANG=", language, NULL);
        g_variant_builder_add (b, "s", s);
        g_free (s);
        if (g_strcmp0 (language, region) != 0) {
                s = g_strconcat ("LC_TIME=", region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_NUMERIC=", region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_MONETARY=", region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
                s = g_strconcat ("LC_MEASUREMENT=", region, NULL);
                g_variant_builder_add (b, "s", s);
                g_free (s);
        }

        g_dbus_proxy_call (localed_proxy,
                           "SetLocale",
                           g_variant_new ("(asb)", b, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
        g_variant_builder_unref (b);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       GtkBuilder  *builder)
{
        GtkWidget *button;
        GtkWidget *label;
        gboolean can_acquire;
        gboolean allowed;

        if (permission) {
                can_acquire = g_permission_get_can_acquire (permission);
                allowed = g_permission_get_allowed (permission);
        }
        else {
                can_acquire = FALSE;
                allowed = FALSE;
        }

        button = (GtkWidget *)gtk_builder_get_object (builder, "copy_settings_button");
        label = (GtkWidget *)gtk_builder_get_object (builder, "system-title");

        if (!allowed && !can_acquire) {
                gtk_label_set_text (GTK_LABEL (label),
                                    _("The login screen, system accounts and new user accounts use the system-wide Region and Language settings."));
                gtk_widget_hide (button);
        }
        else {
                gtk_label_set_text (GTK_LABEL (label),
                                    _("The login screen, system accounts and new user accounts use the system-wide Region and Language settings. You may change the system settings to match yours."));
                gtk_widget_show (button);
                if (allowed) {
                        gtk_button_set_label (GTK_BUTTON (button), _("Copy Settings"));
                }
                else {
                        gtk_button_set_label (GTK_BUTTON (button), _("Copy Settings..."));
                }
        }
}

void
setup_system (GtkBuilder *builder)
{
	gchar *language;
        GDBusConnection *bus;
        GtkWidget *button;

        button = (GtkWidget *)gtk_builder_get_object (builder, "copy_settings_button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (copy_settings), builder);

        localed_permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, NULL);
        if (localed_permission != NULL) {
	        g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, localed_permission);

                g_signal_connect (localed_permission, "notify",
                                  G_CALLBACK (on_permission_changed), builder);
        }
        on_permission_changed (localed_permission, NULL, builder);

	locale_settings = g_settings_new ("org.gnome.system.locale");
	g_signal_connect (locale_settings, "changed::region",
			  G_CALLBACK (locale_settings_changed), builder);
	g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, locale_settings);

	xkb_settings = g_settings_new (GKBD_KEYBOARD_SCHEMA);
	g_signal_connect (xkb_settings, "changed::layouts",
			  G_CALLBACK (xkb_settings_changed), builder);
	g_object_weak_ref (G_OBJECT (builder), (GWeakNotify) g_object_unref, xkb_settings);

        /* Display user settings */
        language = cc_common_language_get_current_language ();
        system_update_language (builder, language);
        g_free (language);

        locale_settings_changed (locale_settings, "region", builder);

        xkb_settings_changed (xkb_settings, "layouts", builder);

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_proxy_new (bus,
                           G_DBUS_PROXY_FLAGS_NONE,
                           NULL,
                           "org.freedesktop.locale1",
                           "/org/freedesktop/locale1",
                           "org.freedesktop.locale1",
                           NULL,
                           localed_proxy_ready,
                           builder);
        g_object_unref (bus);
}
