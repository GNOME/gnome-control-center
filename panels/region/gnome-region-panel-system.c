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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-common-language.h"
#include "gdm-languages.h"
#include "gnome-region-panel-system.h"

#define WID(s) GTK_WIDGET(gtk_builder_get_object (dialog, s))

static GSettings *locale_settings, *input_sources_settings;
static GDBusProxy *localed_proxy;
static GPermission *localed_permission;

static void
update_copy_button (GtkBuilder *dialog)
{
        GtkWidget *label;
        GtkWidget *button;
        const gchar *user_lang, *system_lang;
        const gchar *user_region, *system_region;
        const gchar *user_input_source, *system_input_source;
        const gchar *user_input_variants, *system_input_variants;
        gboolean layouts_differ;

        label = WID ("user_display_language");
        user_lang = g_object_get_data (G_OBJECT (label), "language");

        label = WID ("system_display_language");
        system_lang = g_object_get_data (G_OBJECT (label), "language");

        label = WID ("user_format");
        user_region = g_object_get_data (G_OBJECT (label), "region");

        label = WID ("system_format");
        system_region = g_object_get_data (G_OBJECT (label), "region");

        label = WID ("user_input_source");
        user_input_source = g_object_get_data (G_OBJECT (label), "input_source");
        user_input_variants = g_object_get_data (G_OBJECT (label), "input_variants");

        label = WID ("system_input_source");
        system_input_source = g_object_get_data (G_OBJECT (label), "input_source");
        system_input_variants = g_object_get_data (G_OBJECT (label), "input_variants");

        button = WID ("copy_settings_button");

        if (user_input_source && user_input_source[0]) {
                layouts_differ = (g_strcmp0 (user_input_source, system_input_source) != 0);
                if (layouts_differ == FALSE)
                        layouts_differ = (g_strcmp0 (user_input_variants, system_input_variants) != 0);
        } else {
                /* Nothing to copy */
                layouts_differ = FALSE;
        }

        if (g_strcmp0 (user_lang, system_lang) == 0 &&
            g_strcmp0 (user_region, system_region) == 0 &&
            !layouts_differ)
                gtk_widget_set_sensitive (button, FALSE);
        else
                gtk_widget_set_sensitive (button, TRUE);
}

static void
locale_settings_changed (GSettings *settings,
                         const gchar *key,
                         GtkBuilder *dialog)
{
        GtkWidget *label;
        gchar *region, *display_region;

        region = g_settings_get_string (locale_settings, "region");
        if (!region || !region[0]) {
                label = WID ("user_display_language");
                region = g_strdup ((gchar*)g_object_get_data (G_OBJECT (label), "language"));
        }

        display_region = gdm_get_region_from_name (region, NULL);
        label = WID ("user_format");
        gtk_label_set_text (GTK_LABEL (label), display_region);
        g_object_set_data_full (G_OBJECT (label), "region", g_strdup (region), g_free);
        g_free (region);
        g_free (display_region);

        update_copy_button (dialog);
}

void
system_update_language (GtkBuilder *dialog, const gchar *language)
{
        gchar *display_language;
        GtkWidget *label;

        display_language = gdm_get_language_from_name (language, NULL);
        label = WID ("user_display_language");
        gtk_label_set_text (GTK_LABEL (label), display_language);
        g_object_set_data_full (G_OBJECT (label), "language", g_strdup (language), g_free);
        g_free (display_language);

        /* need to update the region display in case the setting is '' */
        locale_settings_changed (locale_settings, "region", dialog);

        update_copy_button (dialog);
}

static void
input_sources_changed (GSettings *settings,
                       const gchar *key,
                       GtkBuilder *dialog)
{
        GString *disp, *list, *variants;
        GtkWidget *label;
        GnomeXkbInfo *xkb_info;
        GVariantIter iter;
        GVariant *sources;
        const gchar *type;
        const gchar *id;

        sources = g_settings_get_value (input_sources_settings, "sources");
        xkb_info = gnome_xkb_info_new ();

        label = WID ("user_input_source");
        disp = g_string_new ("");
        list = g_string_new ("");
        variants = g_string_new ("");

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &id)) {
                /* We can't copy non-XKB layouts to the system yet */
                if (g_str_equal (type, "xkb")) {
                        char **split;
                        gchar *layout, *variant;
                        const char *name;

                        gnome_xkb_info_get_layout_info (xkb_info, id, &name, NULL, NULL, NULL);
                        if (disp->str[0] != '\0')
                                g_string_append (disp, ", ");
                        g_string_append (disp, name);

                        split = g_strsplit (id, "+", 2);

                        if (split == NULL || split[0] == NULL)
                                continue;

                        layout = split[0];
                        variant = split[1];

                        if (list->str[0] != '\0') {
                                g_string_append (list, ",");
                                g_string_append (variants, ",");
                        }
                        g_string_append (list, layout);
                        g_string_append (variants, variant ? variant : "");

                        g_strfreev (split);
                }
        }
        g_variant_unref (sources);
        g_object_unref (xkb_info);

        g_object_set_data_full (G_OBJECT (label), "input_source", g_string_free (list, FALSE), g_free);
        g_object_set_data_full (G_OBJECT (label), "input_variants", g_string_free (variants, FALSE), g_free);

        gtk_label_set_text (GTK_LABEL (label), disp->str);
        g_string_free (disp, TRUE);

        update_copy_button (dialog);
}

static void
update_property (GDBusProxy *proxy,
                 const char *property)
{
        GError *error = NULL;
        GVariant *variant;

        /* Work around systemd-localed not sending us back
         * the property value when changing values */
        variant = g_dbus_proxy_call_sync (proxy,
                                          "org.freedesktop.DBus.Properties.Get",
                                          g_variant_new ("(ss)", "org.freedesktop.locale1", property),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
        if (variant == NULL) {
                g_warning ("Failed to get property '%s': %s", property, error->message);
                g_error_free (error);
        } else {
                GVariant *v;

                g_variant_get (variant, "(v)", &v);
                g_dbus_proxy_set_cached_property (proxy, property, v);
                g_variant_unref (variant);
        }
}

static void
on_localed_properties_changed (GDBusProxy   *proxy,
                               GVariant     *changed_properties,
                               const gchar **invalidated_properties,
                               GtkBuilder   *dialog)
{
        GVariant *v, *w;
        GtkWidget *label;
        GnomeXkbInfo *xkb_info;
        char **layouts;
        char **variants;
        GString *disp;
        guint i, n;

        if (invalidated_properties != NULL) {
                guint i;
                for (i = 0; invalidated_properties[i] != NULL; i++) {
                        if (g_str_equal (invalidated_properties[i], "Locale"))
                                update_property (proxy, "Locale");
                        else if (g_str_equal (invalidated_properties[i], "X11Layout"))
                                update_property (proxy, "X11Layout");
                        else if (g_str_equal (invalidated_properties[i], "X11Variant"))
                                update_property (proxy, "X11Variant");
                }
        }

        v = g_dbus_proxy_get_cached_property (proxy, "Locale");
        if (v) {
                const gchar **strv;
                gsize len;
                gint i;
                const gchar *lang, *messages, *time;
                gchar *name;
                GtkWidget *label;

                strv = g_variant_get_strv (v, &len);

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
                        label = WID ("system_display_language");
                        gtk_label_set_text (GTK_LABEL (label), name);
                        g_free (name);
                        g_object_set_data_full (G_OBJECT (label), "language", g_strdup (lang), g_free);
                }

                if (time) {
                        name = gdm_get_region_from_name (time, NULL);
                        label = WID ("system_format");
                        gtk_label_set_text (GTK_LABEL (label), name);
                        g_free (name);
                        g_object_set_data_full (G_OBJECT (label), "region", g_strdup (time), g_free);
                }
                g_variant_unref (v);
        }

        label = WID ("system_input_source");
        v = g_dbus_proxy_get_cached_property (proxy, "X11Layout");
        if (v) {
                layouts = g_strsplit (g_variant_get_string (v, NULL), ",", -1);
                g_object_set_data_full (G_OBJECT (label), "input_source",
                                        g_variant_dup_string (v, NULL), g_free);
                g_variant_unref (v);
        } else {
                g_object_set_data_full (G_OBJECT (label), "input_source", NULL, g_free);
                update_copy_button (dialog);
                return;
        }

        variants = NULL;
        g_object_set_data_full (G_OBJECT (label), "input_variants", NULL, g_free);

        w = g_dbus_proxy_get_cached_property (proxy, "X11Variant");
        if (w) {
                const char *variants_str;

                variants_str = g_variant_get_string (w, NULL);
                g_debug ("Got variants '%s'", variants_str);
                if (variants_str && *variants_str != '\0') {
                        variants = g_strsplit (variants_str, ",", -1);
                        g_object_set_data_full (G_OBJECT (label), "input_variants",
                                                g_strdup (variants_str), g_free);
                }
                g_variant_unref (w);
        }

        if (variants && variants[0])
                n = MIN (g_strv_length (layouts), g_strv_length (variants));
        else
                n = g_strv_length (layouts);

        xkb_info = gnome_xkb_info_new ();
        disp = g_string_new ("");
        for (i = 0; i < n && layouts[i][0]; i++) {
                const char *name;
                char *id;

                if (variants && variants[i] && variants[i][0])
                        id = g_strdup_printf ("%s+%s", layouts[i], variants[i]);
                else
                        id = g_strdup (layouts[i]);

                gnome_xkb_info_get_layout_info (xkb_info, id, &name, NULL, NULL, NULL);
                if (disp->str[0] != '\0')
                        disp = g_string_append (disp, ", ");
                disp = g_string_append (disp, name ? name : id);

                g_free (id);
        }
        gtk_label_set_text (GTK_LABEL (label), disp->str);
        g_string_free (disp, TRUE);

        g_strfreev (variants);
        g_strfreev (layouts);
        g_object_unref (xkb_info);

        update_copy_button (dialog);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     GtkBuilder   *dialog)
{
        GError *error = NULL;

        localed_proxy = g_dbus_proxy_new_finish (res, &error);

        if (!localed_proxy) {
                g_warning ("Failed to contact localed: %s\n", error->message);
                g_error_free (error);
                return;
        }

        g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, localed_proxy);

        g_signal_connect (localed_proxy, "g-properties-changed",
                          G_CALLBACK (on_localed_properties_changed), dialog);

        on_localed_properties_changed (localed_proxy, NULL, NULL, dialog);
}

static void
copy_settings (GtkButton *button, GtkBuilder *dialog)
{
        const gchar *language;
        const gchar *region;
        const gchar *layout;
        const gchar *variants;
        GtkWidget *label;
        GVariantBuilder *b;
        gchar *s;

        label = WID ("user_display_language");
        language = g_object_get_data (G_OBJECT (label), "language");
        label = WID ("user_format");
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

        label = WID ("user_input_source");
        layout = g_object_get_data (G_OBJECT (label), "input_source");
        variants = g_object_get_data (G_OBJECT (label), "input_variants");

        if (layout == NULL || layout[0] == '\0') {
                g_debug ("Not calling SetX11Keyboard, as there are no XKB input sources in the user's settings");
                return;
        }

        g_dbus_proxy_call (localed_proxy,
                           "SetX11Keyboard",
                           g_variant_new ("(ssssbb)", layout, "", variants ? variants : "", "", TRUE, TRUE),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
on_permission_changed (GPermission *permission,
                       GParamSpec  *pspec,
                       GtkBuilder  *dialog)
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

        button = WID ("copy_settings_button");
        label = WID ("system-title");

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
setup_system (GtkBuilder *dialog)
{
        gchar *language;
        GDBusConnection *bus;
        GtkWidget *button;

        localed_permission = polkit_permission_new_sync ("org.freedesktop.locale1.set-locale", NULL, NULL, NULL);
        if (localed_permission == NULL) {
                GtkWidget *tab_widget, *notebook;
                int num;

                tab_widget = WID ("table3");
                notebook = WID ("region_notebook");
                num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), tab_widget);
                gtk_notebook_remove_page (GTK_NOTEBOOK (notebook), num);
                return;
        }

        g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, localed_permission);
        g_signal_connect (localed_permission, "notify",
                          G_CALLBACK (on_permission_changed), dialog);
        on_permission_changed (localed_permission, NULL, dialog);


        button = WID ("copy_settings_button");
        g_signal_connect (button, "clicked",
                          G_CALLBACK (copy_settings), dialog);


        locale_settings = g_settings_new ("org.gnome.system.locale");
        g_signal_connect (locale_settings, "changed::region",
                          G_CALLBACK (locale_settings_changed), dialog);
        g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, locale_settings);

        input_sources_settings = g_settings_new ("org.gnome.desktop.input-sources");
        g_signal_connect (input_sources_settings, "changed::sources",
                          G_CALLBACK (input_sources_changed), dialog);
        g_object_weak_ref (G_OBJECT (dialog), (GWeakNotify) g_object_unref, input_sources_settings);

        /* Display user settings */
        language = cc_common_language_get_current_language ();
        system_update_language (dialog, language);
        g_free (language);

        locale_settings_changed (locale_settings, "region", dialog);

        input_sources_changed (input_sources_settings, "sources", dialog);

        bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
        g_dbus_proxy_new (bus,
                           G_DBUS_PROXY_FLAGS_NONE,
                           NULL,
                           "org.freedesktop.locale1",
                           "/org/freedesktop/locale1",
                           "org.freedesktop.locale1",
                           NULL,
                           (GAsyncReadyCallback) localed_proxy_ready,
                           dialog);
        g_object_unref (bus);
}
