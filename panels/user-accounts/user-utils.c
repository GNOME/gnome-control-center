/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <unistd.h>
#include <utmpx.h>
#include <pwd.h>

#include <gio/gio.h>
#include <gio/gunixoutputstream.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "user-utils.h"

#define IMAGE_SIZE 512

typedef struct {
        gchar *text;
        gchar *placeholder_str;
        GIcon *icon;
        gunichar placeholder;
        gulong query_id;
} IconShapeData;

static IconShapeData *
icon_shape_data_new (const gchar *text,
                     const gchar *placeholder,
                     GIcon       *icon)
{
        IconShapeData *data;

        data = g_new0 (IconShapeData, 1);

        data->text = g_strdup (text);
        data->placeholder_str = g_strdup (placeholder);
        data->placeholder = g_utf8_get_char_validated (placeholder, -1);
        data->icon = g_object_ref (icon);

        return data;
}

static void
icon_shape_data_free (gpointer user_data)
{
        IconShapeData *data = user_data;

        g_free (data->text);
        g_free (data->placeholder_str);
        g_object_unref (data->icon);
        g_free (data);
}

static void
icon_shape_renderer (cairo_t        *cr,
                     PangoAttrShape *attr,
                     gboolean        do_path,
                     gpointer        user_data)
{
        IconShapeData *data = user_data;
        gdouble x, y;

        cairo_get_current_point (cr, &x, &y);
        if (GPOINTER_TO_UINT (attr->data) == data->placeholder) {
                gdouble ascent;
                gdouble height;
                GdkPixbuf *pixbuf;
                GtkIconInfo *info;

                ascent = pango_units_to_double (attr->ink_rect.y);
                height = pango_units_to_double (attr->ink_rect.height);
                info = gtk_icon_theme_lookup_by_gicon (gtk_icon_theme_get_default (),
                                                       data->icon,
                                                       (gint)height,
                                                       GTK_ICON_LOOKUP_FORCE_SIZE | GTK_ICON_LOOKUP_USE_BUILTIN);
                pixbuf = gtk_icon_info_load_icon (info, NULL);
                g_object_unref (info);

                cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
                cairo_reset_clip (cr);
                gdk_cairo_set_source_pixbuf (cr, pixbuf, x, y + ascent);
                cairo_paint (cr);
                g_object_unref (pixbuf);
        }
}

static PangoAttrList *
create_shape_attr_list_for_layout (PangoLayout   *layout,
                                   IconShapeData *data)
{
        PangoAttrList *attrs;
        PangoFontMetrics *metrics;
        gint ascent, descent;
        PangoRectangle ink_rect, logical_rect;
        const gchar *p;
        const gchar *text;
        gint placeholder_len;

        /* Get font metrics and prepare fancy shape size */
        metrics = pango_context_get_metrics (pango_layout_get_context (layout),
                                             pango_layout_get_font_description (layout),
                                             NULL);
        ascent = pango_font_metrics_get_ascent (metrics);
        descent = pango_font_metrics_get_descent (metrics);
        pango_font_metrics_unref (metrics);

        logical_rect.x = 0;
        logical_rect.y = - ascent;
        logical_rect.width = ascent + descent;
        logical_rect.height = ascent + descent;

        ink_rect = logical_rect;

        attrs = pango_attr_list_new ();
        text = pango_layout_get_text (layout);
        placeholder_len = strlen (data->placeholder_str);
        for (p = text; (p = strstr (p, data->placeholder_str)); p += placeholder_len) {
                PangoAttribute *attr;

                attr = pango_attr_shape_new_with_data (&ink_rect,
                                                       &logical_rect,
                                                       GUINT_TO_POINTER (g_utf8_get_char (p)),
                                                       NULL, NULL);

                attr->start_index = p - text;
                attr->end_index = attr->start_index + placeholder_len;

                pango_attr_list_insert (attrs, attr);
        }

        return attrs;
}

static gboolean
query_unlock_tooltip (GtkWidget  *widget,
                      gint        x,
                      gint        y,
                      gboolean    keyboard_tooltip,
                      GtkTooltip *tooltip,
                      gpointer    user_data)
{
        GtkWidget *label;
        PangoLayout *layout;
        PangoAttrList *attrs;
        IconShapeData *data;

        data = g_object_get_data (G_OBJECT (widget), "icon-shape-data");
        label = g_object_get_data (G_OBJECT (widget), "tooltip-label");
        if (label == NULL) {
                label = gtk_label_new (data->text);
                g_object_ref_sink (label);
                g_object_set_data_full (G_OBJECT (widget),
                                        "tooltip-label", label, g_object_unref);
        }

        layout = gtk_label_get_layout (GTK_LABEL (label));
        pango_cairo_context_set_shape_renderer (pango_layout_get_context (layout),
                                                icon_shape_renderer,
                                                data, NULL);

        attrs = create_shape_attr_list_for_layout (layout, data);
        gtk_label_set_attributes (GTK_LABEL (label), attrs);
        pango_attr_list_unref (attrs);

        gtk_tooltip_set_custom (tooltip, label);

        return TRUE;
}

void
setup_tooltip_with_embedded_icon (GtkWidget   *widget,
                                  const gchar *text,
                                  const gchar *placeholder,
                                  GIcon       *icon)
{
        IconShapeData *data;

        data = g_object_get_data (G_OBJECT (widget), "icon-shape-data");
        if (data) {
                gtk_widget_set_has_tooltip (widget, FALSE);
                g_signal_handler_disconnect (widget, data->query_id);
                g_object_set_data (G_OBJECT (widget), "icon-shape-data", NULL);
                g_object_set_data (G_OBJECT (widget), "tooltip-label", NULL);
        }

        if (!placeholder) {
                gtk_widget_set_tooltip_text (widget, text);
                return;
        }

        data = icon_shape_data_new (text, placeholder, icon);
        g_object_set_data_full (G_OBJECT (widget),
                                "icon-shape-data",
                                data,
                                icon_shape_data_free);

        gtk_widget_set_has_tooltip (widget, TRUE);
        data->query_id = g_signal_connect (widget, "query-tooltip",
                                           G_CALLBACK (query_unlock_tooltip), NULL);

}

gboolean
show_tooltip_now (GtkWidget *widget,
                  GdkEvent  *event)
{
        GtkSettings *settings;
        gint timeout;

        settings = gtk_widget_get_settings (widget);

        g_object_get (settings, "gtk-tooltip-timeout", &timeout, NULL);
        g_object_set (settings, "gtk-tooltip-timeout", 1, NULL);
        gtk_tooltip_trigger_tooltip_query (gtk_widget_get_display (widget));
        g_object_set (settings, "gtk-tooltip-timeout", timeout, NULL);

        return FALSE;
}

static gboolean
query_tooltip (GtkWidget  *widget,
               gint        x,
               gint        y,
               gboolean    keyboard_mode,
               GtkTooltip *tooltip,
               gpointer    user_data)
{
        gchar *tip;

        if (GTK_ENTRY_ICON_SECONDARY == gtk_entry_get_icon_at_pos (GTK_ENTRY (widget), x, y)) {
                tip = gtk_entry_get_icon_tooltip_text (GTK_ENTRY (widget),
                                                       GTK_ENTRY_ICON_SECONDARY);
                gtk_tooltip_set_text (tooltip, tip);
                g_free (tip);

                return TRUE;
        }
        else {
                return FALSE;
        }
}

static void
icon_released (GtkEntry             *entry,
              GtkEntryIconPosition  pos,
              GdkEvent             *event,
              gpointer              user_data)
{
        GtkSettings *settings;
        gint timeout;

        settings = gtk_widget_get_settings (GTK_WIDGET (entry));

        g_object_get (settings, "gtk-tooltip-timeout", &timeout, NULL);
        g_object_set (settings, "gtk-tooltip-timeout", 1, NULL);
        gtk_tooltip_trigger_tooltip_query (gtk_widget_get_display (GTK_WIDGET (entry)));
        g_object_set (settings, "gtk-tooltip-timeout", timeout, NULL);
}



void
set_entry_validation_error (GtkEntry    *entry,
                            const gchar *text)
{
        g_object_set (entry, "caps-lock-warning", FALSE, NULL);
        gtk_entry_set_icon_from_icon_name (entry,
                                           GTK_ENTRY_ICON_SECONDARY,
                                           "dialog-warning-symbolic");
        gtk_entry_set_icon_activatable (entry,
                                        GTK_ENTRY_ICON_SECONDARY,
                                        TRUE);
        g_signal_connect (entry, "icon-release",
                          G_CALLBACK (icon_released), FALSE);
        g_signal_connect (entry, "query-tooltip",
                          G_CALLBACK (query_tooltip), NULL);
        g_object_set (entry, "has-tooltip", TRUE, NULL);
        gtk_entry_set_icon_tooltip_text (entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         text);
}

void
set_entry_generation_icon (GtkEntry *entry)
{
        g_object_set (entry, "caps-lock-warning", FALSE, NULL);
        gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "system-run-symbolic");
        gtk_entry_set_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY, TRUE);
}

void
set_entry_validation_checkmark (GtkEntry *entry)
{
        g_object_set (entry, "caps-lock-warning", FALSE, NULL);
        gtk_entry_set_icon_from_icon_name (entry, GTK_ENTRY_ICON_SECONDARY, "object-select-symbolic");
        gtk_entry_set_icon_activatable (entry, GTK_ENTRY_ICON_SECONDARY, FALSE);
}

void
clear_entry_validation_error (GtkEntry *entry)
{
        gboolean warning;

        g_object_get (entry, "caps-lock-warning", &warning, NULL);

        if (warning)
                return;

        g_object_set (entry, "has-tooltip", FALSE, NULL);
        gtk_entry_set_icon_from_pixbuf (entry,
                                        GTK_ENTRY_ICON_SECONDARY,
                                        NULL);
        g_object_set (entry, "caps-lock-warning", TRUE, NULL);
}

/* Taken from defines.h in shadow-utils. On Linux, this value is much smaller
 * than the sysconf limit LOGIN_NAME_MAX, and values larger than this will
 * result in failure when running useradd. We could check UT_NAMESIZE instead,
 * but that is nonstandard. Better to use POSIX utmpx.
 */
gsize
get_username_max_length (void)
{
        return sizeof (((struct utmpx *)NULL)->ut_user);
}

gboolean
is_username_used (const gchar *username)
{
        struct passwd *pwent;

        if (username == NULL || username[0] == '\0') {
                return FALSE;
        }

        pwent = getpwnam (username);

        return pwent != NULL;
}

gboolean
is_valid_name (const gchar *name)
{
        gboolean is_empty = TRUE;
        const gchar *c;

        /* Valid names must contain:
         *   1) at least one character.
         *   2) at least one non-"space" character.
         */
        for (c = name; *c; c++) {
                gunichar unichar;

                unichar = g_utf8_get_char_validated (c, -1);

                /* Partial UTF-8 sequence or end of string */
                if (unichar == (gunichar) -1 || unichar == (gunichar) -2)
                        break;

                /* Check for non-space character */
                if (!g_unichar_isspace (unichar)) {
                        is_empty = FALSE;
                        break;
                }
        }

        return !is_empty;
}

gboolean
is_valid_username (const gchar *username, gchar **tip)
{
        gboolean empty;
        gboolean in_use;
        gboolean too_long;
        gboolean valid;
        const gchar *c;

        if (username == NULL || username[0] == '\0') {
                empty = TRUE;
                in_use = FALSE;
                too_long = FALSE;
        } else {
                empty = FALSE;
                in_use = is_username_used (username);
                too_long = strlen (username) > get_username_max_length ();
        }
        valid = TRUE;

        if (!in_use && !empty && !too_long) {
                /* First char must be a letter, and it must only composed
                 * of ASCII letters, digits, and a '.', '-', '_'
                 */
                for (c = username; *c; c++) {
                        if (! ((*c >= 'a' && *c <= 'z') ||
                               (*c >= 'A' && *c <= 'Z') ||
                               (*c >= '0' && *c <= '9') ||
                               (*c == '_') || (*c == '.') ||
                               (*c == '-' && c != username)))
                           valid = FALSE;
                }
        }

        valid = !empty && !in_use && !too_long && valid;

        if (!empty && (in_use || too_long || !valid)) {
                if (in_use) {
                        *tip = g_strdup (_("Sorry, that user name isn’t available. Please try another."));
                }
                else if (too_long) {
                        *tip = g_strdup_printf (_("The username is too long."));
                }
                else if (username[0] == '-') {
                        *tip = g_strdup (_("The username cannot start with a “-”."));
                }
                else {
                        *tip = g_strdup (_("The username should only consist of upper and lower case letters from a-z, digits and the following characters: . - _"));
                }
        }
        else {
                *tip = g_strdup (_("This will be used to name your home folder and can’t be changed."));
        }

        return valid;
}

GdkPixbuf *
round_image (GdkPixbuf *pixbuf,
             gint       icon_size)
{
        GdkPixbuf *dest = NULL;
        cairo_surface_t *surface;
        cairo_t *cr;
        gint size;

        size = gdk_pixbuf_get_width (pixbuf);
        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
        cr = cairo_create (surface);

        /* Clip a circle */
        cairo_arc (cr, size/2, size/2, size/2, 0, 2 * G_PI);
        cairo_clip (cr);
        cairo_new_path (cr);

        gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
        cairo_paint (cr);

        dest = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
        cairo_surface_destroy (surface);
        cairo_destroy (cr);

        return dest;
}

static gchar *
extract_initials_from_name (const gchar *name)
{
        GString *initials;
        g_autofree gchar *p = NULL;
        g_autofree gchar *normalized = NULL;
        gunichar unichar;
        gpointer q = NULL;

        p = g_utf8_strup (name, -1);
        normalized = g_utf8_normalize (g_strstrip (p), -1, G_NORMALIZE_DEFAULT_COMPOSE);
        if (normalized == NULL) {
                return NULL;
        }

        initials = g_string_new ("");

        unichar = g_utf8_get_char (normalized);
        g_string_append_unichar (initials, unichar);

        q = g_utf8_strrchr (normalized, -1, ' ');
        if (q != NULL && g_utf8_next_char (q) != NULL) {
                q = g_utf8_next_char (q);

                unichar = g_utf8_get_char (q);
                g_string_append_unichar (initials, unichar);
        }

        return g_string_free (initials, FALSE);
}

static GdkRGBA
get_color_for_name (const gchar *name)
{
        // https://gitlab.gnome.org/Community/Design/HIG-app-icons/blob/master/GNOME%20HIG.gpl
        static gdouble gnome_color_palette[][3] = {
                {  98, 160, 234 },
                {  53, 132, 228 },
                {  28, 113, 216 },
                {  26,  95, 180 },
                {  87, 227, 137 },
                {  51, 209, 122 },
                {  46, 194, 126 },
                {  38, 162, 105 },
                { 248, 228,  92 },
                { 246, 211,  45 },
                { 245, 194,  17 },
                { 229, 165,  10 },
                { 255, 163,  72 },
                { 255, 120,   0 },
                { 230,  97,   0 },
                { 198,  70,   0 },
                { 237,  51,  59 },
                { 224,  27,  36 },
                { 192,  28,  40 },
                { 165,  29,  45 },
                { 192,  97, 203 },
                { 163,  71, 186 },
                { 129,  61, 156 },
                {  97,  53, 131 },
                { 181, 131,  90 },
                { 152, 106,  68 },
                { 134,  94,  60 },
                {  99,  69,  44 }
        };

        GdkRGBA color = { 255, 255, 255, 1.0 };
        guint hash;
        gint number_of_colors;
        gint idx;

        if (name == NULL || strlen (name) == 0)
                return color;

        hash = g_str_hash (name);
        number_of_colors = G_N_ELEMENTS (gnome_color_palette);
        idx = hash % number_of_colors;

        color.red   = gnome_color_palette[idx][0];
        color.green = gnome_color_palette[idx][1];
        color.blue  = gnome_color_palette[idx][2];

        return color;
}

static cairo_surface_t *
generate_user_picture (const gchar *name)
{
        PangoFontDescription *font_desc;
        g_autofree gchar *initials = extract_initials_from_name (name);
        g_autofree gchar *font = g_strdup_printf ("Sans %d", (int)ceil (IMAGE_SIZE / 2.5));
        PangoLayout *layout;
        GdkRGBA color = get_color_for_name (name);
        cairo_surface_t *surface;
        gint width, height;
        cairo_t *cr;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                              IMAGE_SIZE,
                                              IMAGE_SIZE);
        cr = cairo_create (surface);
        cairo_rectangle (cr, 0, 0, IMAGE_SIZE, IMAGE_SIZE);
        cairo_set_source_rgb (cr, color.red/255.0, color.green/255.0, color.blue/255.0);
        cairo_fill (cr);

        /* Draw the initials on top */
        cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);
        layout = pango_cairo_create_layout (cr);
        pango_layout_set_text (layout, initials, -1);
        font_desc = pango_font_description_from_string (font);
        pango_layout_set_font_description (layout, font_desc);
        pango_font_description_free (font_desc);

        pango_layout_get_size (layout, &width, &height);
        cairo_translate (cr, IMAGE_SIZE/2, IMAGE_SIZE/2);
        cairo_move_to (cr, - ((double)width / PANGO_SCALE)/2, - ((double)height/PANGO_SCALE)/2);
        pango_cairo_show_layout (cr, layout);
        cairo_destroy (cr);

        return surface;
}

void
set_user_icon_data (ActUser   *user,
                    GdkPixbuf *pixbuf)
{
        gchar *path;
        gint fd;
        GOutputStream *stream;
        GError *error;

        path = g_build_filename (g_get_tmp_dir (), "gnome-control-center-user-icon-XXXXXX", NULL);
        fd = g_mkstemp (path);

        if (fd == -1) {
                g_warning ("failed to create temporary file for image data");
                g_free (path);
                return;
        }

        stream = g_unix_output_stream_new (fd, TRUE);

        error = NULL;
        if (!gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, &error, NULL)) {
                g_warning ("failed to save image: %s", error->message);
                g_error_free (error);
                g_object_unref (stream);
                return;
        }

        g_object_unref (stream);

        act_user_set_icon_file (user, path);

        /* if we ever make the dbus call async, the g_remove call needs
         * to wait for its completion
         */
        g_remove (path);

        g_free (path);
}

void
generate_user_avatar (ActUser *user)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;
        cairo_surface_t *surface;

        surface = generate_user_picture (act_user_get_real_name (user));

        pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, IMAGE_SIZE, IMAGE_SIZE);
        cairo_surface_destroy (surface);

        set_user_icon_data (user, pixbuf);
}
