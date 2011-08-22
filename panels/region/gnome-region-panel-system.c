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

#include <libgnomekbd/gkbd-keyboard-config.h>
#include "cc-common-language.h"
#include "gdm-languages.h"
#include "gnome-region-panel-system.h"
#include "gnome-region-panel-xkb.h"

static GSettings *locale_settings, *xkb_settings;

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

void
setup_system (GtkBuilder *builder)
{
	gchar *language;

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
}
