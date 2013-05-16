/*
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Matthias Clasen
 */

#include <config.h>
#include <glib/gi18n.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-util.h"
#include "cc-input-options.h"

typedef struct {
        GtkWidget *dialog;
        GtkWidget *same_source;
        GtkWidget *per_window_source;
        GtkWidget *previous_source;
        GtkWidget *next_source;
        GtkWidget *alt_next_source;
        GSettings *settings;
} CcInputOptionsPrivate;

#define GET_PRIVATE(options) ((CcInputOptionsPrivate *) g_object_get_data (G_OBJECT (options), "private"))

static void
cc_input_options_private_free (gpointer data)
{
        CcInputOptionsPrivate *priv = data;

        g_clear_object (&priv->settings);
        g_free (priv);
}

static void
update_shortcut_label (GtkWidget   *widget,
                       const gchar *value)
{
        gchar *text;
        guint accel_key, *keycode;
        GdkModifierType mods;

        if (value == NULL || *value == '\0') {
                gtk_widget_hide (widget);
                return;
        }

        gtk_accelerator_parse_with_keycode (value, &accel_key, &keycode, &mods);
        if (accel_key == 0 && keycode == NULL && mods == 0) {
                g_warning ("Failed to parse keyboard shortcut: '%s'", value);
                gtk_widget_hide (widget);
                return;
        }

        text = gtk_accelerator_get_label_with_keycode (gtk_widget_get_display (widget), accel_key, *keycode, mods);
        g_free (keycode);
        gtk_label_set_text (GTK_LABEL (widget), text);
        g_free (text);
}

static void
update_shortcuts (GtkWidget *options)
{
        CcInputOptionsPrivate *priv = GET_PRIVATE (options);
        gchar **previous;
        gchar **next;
        gchar *previous_shortcut;
        GSettings *settings;

        settings = g_settings_new ("org.gnome.desktop.wm.keybindings");

        previous = g_settings_get_strv (settings, "switch-input-source-backward");
        next = g_settings_get_strv (settings, "switch-input-source");

        previous_shortcut = g_strdup (previous[0]);
        if (!previous_shortcut && next[0] && *next[0])
                previous_shortcut = g_strconcat ("<Shift>", next[0], NULL);

        update_shortcut_label (priv->previous_source, previous_shortcut);
        update_shortcut_label (priv->next_source, next[0]);

        g_free (previous_shortcut);

        g_strfreev (previous);
        g_strfreev (next);

        g_object_unref (settings);
}

static void
update_modifiers_shortcut (GtkWidget *dialog)
{
        CcInputOptionsPrivate *priv = GET_PRIVATE (dialog);
        gchar **options, **p;
        GSettings *settings;
        GnomeXkbInfo *xkb_info;
        const gchar *text;

        xkb_info = gnome_xkb_info_new ();
        settings = g_settings_new ("org.gnome.desktop.input-sources");
        options = g_settings_get_strv (settings, "xkb-options");

        for (p = options; p && *p; ++p)
                if (g_str_has_prefix (*p, "grp:"))
                        break;

        if (p && *p) {
                text = cc_util_xkb_info_description_for_option (xkb_info, "grp", *p);
                gtk_label_set_text (GTK_LABEL (priv->alt_next_source), text);
        } else {
                gtk_widget_hide (priv->alt_next_source);
        }

        g_strfreev (options);
        g_object_unref (settings);
        g_object_unref (xkb_info);
}

#define WID(name) ((GtkWidget *) gtk_builder_get_object (builder, name))

GtkWidget *
cc_input_options_new (GtkWidget *parent)
{
        GtkBuilder *builder;
        GtkWidget *options;
        CcInputOptionsPrivate *priv;
        GError *error = NULL;

        builder = gtk_builder_new ();
        if (gtk_builder_add_from_resource (builder, "/org/gnome/control-center/region/input-options.ui", &error) == 0) {
                g_object_unref (builder);
                g_warning ("failed to load input options: %s", error->message);
                g_error_free (error);
                return NULL;
        }

        options = WID ("dialog");
        priv = g_new0 (CcInputOptionsPrivate, 1);
        g_object_set_data_full (G_OBJECT (options), "private", priv, cc_input_options_private_free);
        g_object_set_data_full (G_OBJECT (options), "builder", builder, g_object_unref);

        priv->same_source = WID ("same-source");
        priv->per_window_source = WID ("per-window-source");
        priv->previous_source = WID ("previous-source");
        priv->next_source = WID ("next-source");
        priv->alt_next_source = WID ("alt-next-source");

        g_object_bind_property (priv->previous_source, "visible",
                                WID ("previous-source-label"), "visible",
                                G_BINDING_DEFAULT);
        g_object_bind_property (priv->next_source, "visible",
                                WID ("next-source-label"), "visible",
                                G_BINDING_DEFAULT);
        g_object_bind_property (priv->alt_next_source, "visible",
                                WID ("alt-next-source-label"), "visible",
                                G_BINDING_DEFAULT);

        priv->settings = g_settings_new ("org.gnome.desktop.input-sources");
        g_settings_bind (priv->settings, "per-window",
                         priv->per_window_source, "active",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (priv->settings, "per-window",
                         priv->same_source, "active",
                         G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

        update_shortcuts (options);
        update_modifiers_shortcut (options);

        gtk_window_set_transient_for (GTK_WINDOW (options), GTK_WINDOW (parent));

        return options;
}
