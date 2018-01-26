/*
 * Copyright (C) 2013, 2015 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Matthias Clasen
 */

#include <config.h>
#include <glib/gi18n.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-input-options.h"

struct _CcInputOptions {
        GtkDialog parent_instance;
        GtkWidget *same_source;
        GtkWidget *per_window_source;
        GtkWidget *previous_source;
        GtkWidget *previous_source_label;
        GtkWidget *next_source;
        GtkWidget *next_source_label;
        GtkWidget *alt_next_source;
        GtkWidget *alt_next_source_label;
        GSettings *settings;
};

G_DEFINE_TYPE (CcInputOptions, cc_input_options, GTK_TYPE_DIALOG);

static void
update_shortcut_label (GtkWidget   *widget,
                       const gchar *value)
{
        g_autofree gchar *text = NULL;
        guint accel_key;
        g_autofree guint *keycode = NULL;
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
        gtk_label_set_text (GTK_LABEL (widget), text);
}

static void
update_shortcuts (CcInputOptions *self)
{
        g_auto(GStrv) previous = NULL;
        g_auto(GStrv) next = NULL;
        g_autofree gchar *previous_shortcut = NULL;
        g_autoptr(GSettings) settings = NULL;

        settings = g_settings_new ("org.gnome.desktop.wm.keybindings");

        previous = g_settings_get_strv (settings, "switch-input-source-backward");
        next = g_settings_get_strv (settings, "switch-input-source");

        previous_shortcut = g_strdup (previous[0]);

        update_shortcut_label (self->previous_source, previous_shortcut);
        update_shortcut_label (self->next_source, next[0]);
}

static void
update_modifiers_shortcut (CcInputOptions *self)
{
        g_auto(GStrv) options = NULL;
        gchar **p;
        g_autoptr(GSettings) settings = NULL;
        g_autoptr(GnomeXkbInfo) xkb_info = NULL;
        const gchar *text;

        xkb_info = gnome_xkb_info_new ();
        settings = g_settings_new ("org.gnome.desktop.input-sources");
        options = g_settings_get_strv (settings, "xkb-options");

        for (p = options; p && *p; ++p)
                if (g_str_has_prefix (*p, "grp:"))
                        break;

        if (p && *p) {
                text = gnome_xkb_info_description_for_option (xkb_info, "grp", *p);
                gtk_label_set_text (GTK_LABEL (self->alt_next_source), text);
        } else {
                gtk_widget_hide (self->alt_next_source);
        }
}

static void
cc_input_options_finalize (GObject *object)
{
        CcInputOptions *self = CC_INPUT_OPTIONS (object);

        g_object_unref (self->settings);
        G_OBJECT_CLASS (cc_input_options_parent_class)->finalize (object);
}

static void
cc_input_options_init (CcInputOptions *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        g_object_bind_property (self->previous_source, "visible",
                                self->previous_source_label, "visible",
                                G_BINDING_DEFAULT);
        g_object_bind_property (self->next_source, "visible",
                                self->next_source_label, "visible",
                                G_BINDING_DEFAULT);
        g_object_bind_property (self->alt_next_source, "visible",
                                self->alt_next_source_label, "visible",
                                G_BINDING_DEFAULT);

        self->settings = g_settings_new ("org.gnome.desktop.input-sources");
        g_settings_bind (self->settings, "per-window",
                         self->per_window_source, "active",
                         G_SETTINGS_BIND_DEFAULT);
        g_settings_bind (self->settings, "per-window",
                         self->same_source, "active",
                         G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

        update_shortcuts (self);
        update_modifiers_shortcut (self);
}

static void
cc_input_options_class_init (CcInputOptionsClass *klass)
{
        GObjectClass *object_klass = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_klass = GTK_WIDGET_CLASS (klass);

        object_klass->finalize = cc_input_options_finalize;

        gtk_widget_class_set_template_from_resource (widget_klass,
                                                     "/org/gnome/control-center/region/input-options.ui");
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, same_source);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, per_window_source);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, previous_source);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, previous_source_label);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, next_source);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, next_source_label);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, alt_next_source);
        gtk_widget_class_bind_template_child (widget_klass, CcInputOptions, alt_next_source_label);
}

GtkWidget *
cc_input_options_new (GtkWidget *parent)
{
        return g_object_new (CC_TYPE_INPUT_OPTIONS, "transient-for", parent, "use-header-bar", TRUE, NULL);
}
