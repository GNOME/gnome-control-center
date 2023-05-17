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
#include <sys/wait.h>
#include <limits.h>
#include <unistd.h>
#include <utmpx.h>
#include <pwd.h>

#ifdef __FreeBSD__
#include <sysexits.h>
#endif

#include <gio/gio.h>
#include <gio/gunixoutputstream.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "user-utils.h"

#define IMAGE_SIZE 512

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
        gboolean found_comma = FALSE;
        const gchar *c;

        if (name == NULL)
                return is_empty;

        /* Valid names must contain:
         *   1) at least one character.
         *   2) at least one non-"space" character.
         *   3) comma character not allowed. Issue #888
         */
        for (c = name; *c; c++) {
                gunichar unichar;

                unichar = g_utf8_get_char_validated (c, -1);

                /* Partial UTF-8 sequence or end of string */
                if (unichar == (gunichar) -1 || unichar == (gunichar) -2)
                        break;

                /* Check for non-space character */
                if (is_empty && !g_unichar_isspace (unichar)) {
                        is_empty = FALSE;
                }

                if (unichar == ',') {
                        found_comma = TRUE;
                        break;
                }
        }

        return !is_empty && !found_comma;
}

typedef struct {
        gchar *username;
        gchar *tip;
} isValidUsernameData;

static void
is_valid_username_data_free (isValidUsernameData *data)
{
        g_clear_pointer (&data->username, g_free);
        g_clear_pointer (&data->tip, g_free);
        g_free (data);
}

#ifdef __FreeBSD__
/* Taken from pw(8) man page. */
#define E_SUCCESS EX_OK
#define E_BAD_ARG EX_DATAERR
#define E_NOTFOUND EX_NOUSER
#else
/* Taken from usermod.c in shadow-utils. */
#define E_SUCCESS 0
#define E_BAD_ARG 3
#define E_NOTFOUND 6
#endif

static void
is_valid_username_child_watch_cb (GPid pid,
                                  gint status,
                                  gpointer user_data)
{
        g_autoptr(GTask) task = G_TASK (user_data);
        isValidUsernameData *data = g_task_get_task_data (task);
        GError *error = NULL;
        gboolean valid = FALSE;
        const gchar *tip = NULL;

        if (WIFEXITED (status)) {
                switch (WEXITSTATUS (status)) {
                        case E_NOTFOUND:
                                valid = TRUE;
                                break;
                        case E_BAD_ARG:
                                tip = _("Usernames can only include lower case letters, numbers, hyphens and underscores.");
                                valid = FALSE;
                                break;
                        case E_SUCCESS:
                                tip = _("Sorry, that user name isn’t available. Please try another.");
                                valid = FALSE;
                                break;
                }
        }

        if (valid || tip != NULL) {
                data->tip = g_strdup (tip);
                g_task_return_boolean (task, valid);
        }
        else {
                g_spawn_check_wait_status (status, &error);
                g_task_return_error (task, error);
        }

        g_spawn_close_pid (pid);
}

void
is_valid_username_async (const gchar *username,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer callback_data)
{
        g_autoptr(GTask) task = NULL;
        isValidUsernameData *data;
        gchar *argv[6];
        GPid pid;
        GError *error = NULL;

        task = g_task_new (NULL, cancellable, callback, callback_data);
        g_task_set_source_tag (task, is_valid_username_async);

        data = g_new0 (isValidUsernameData, 1);
        data->username = g_strdup (username);
        g_task_set_task_data (task, data, (GDestroyNotify) is_valid_username_data_free);

        if (username == NULL || username[0] == '\0') {
                g_task_return_boolean (task, FALSE);
                return;
        }
        else if (strlen (username) > get_username_max_length ()) {
                data->tip = g_strdup (_("The username is too long."));
                g_task_return_boolean (task, FALSE);
                return;
        }

#ifdef __FreeBSD__
        /* Abuse "pw usershow -n <name>" in the same way as the code below. We
         * don't use "pw usermod -n <name> -N -l <newname>" here because it has
         * a special case for "root" to reject changes to the root user.
         */
        argv[0] = "pw";
        argv[1] = "usershow";
        argv[2] = "-n";
        argv[3] = data->username;
        argv[4] = NULL;
#else
        /* "usermod --login" is meant to be used to change a username, but the
         * exit codes can be safely abused to check the validity of username.
         * However, the current "usermod" implementation may change in the
         * future, so it would be nice to have some official way for this
         * instead of relying on the current "--login" implementation.
         */
        argv[0] = "/usr/sbin/usermod";
        argv[1] = "--login";
        argv[2] = data->username;
        argv[3] = "--";
        argv[4] = data->username;
        argv[5] = NULL;
#endif

        if (!g_spawn_async (NULL, argv, NULL,
                            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD |
                            G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                            NULL, NULL, &pid, &error)) {
                g_task_return_error (task, error);
                return;
        }

        g_child_watch_add (pid, (GChildWatchFunc) is_valid_username_child_watch_cb, task);
        g_steal_pointer (&task);
}

gboolean
is_valid_username_finish (GAsyncResult *result,
                          gchar **tip,
                          gchar **username,
                          GError **error)
{
        GTask *task;
        isValidUsernameData *data;

        g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

        task = G_TASK (result);
        data = g_task_get_task_data (task);

        if (tip != NULL) {
                *tip = g_steal_pointer (&data->tip);
        }

        if (username != NULL)
                *username = g_steal_pointer (&data->username);

        return g_task_propagate_boolean (task, error);
}

GdkPixbuf *
round_image (GdkPixbuf *pixbuf)
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

        g_return_val_if_fail (name != NULL, NULL);

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
generate_user_picture (const gchar *name, gint size)
{
        PangoFontDescription *font_desc;
        g_autofree gchar *initials = extract_initials_from_name (name);
        g_autofree gchar *font = g_strdup_printf ("Sans %d", (int)ceil (size / 2.5));
        PangoLayout *layout;
        GdkRGBA color = get_color_for_name (name);
        cairo_surface_t *surface;
        gint width, height;
        cairo_t *cr;

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                              size,
                                              size);
        cr = cairo_create (surface);
        cairo_rectangle (cr, 0, 0, size, size);
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
        cairo_translate (cr, size/2, size/2);
        cairo_move_to (cr, - ((double)width / PANGO_SCALE)/2, - ((double)height/PANGO_SCALE)/2);
        pango_cairo_show_layout (cr, layout);
        cairo_destroy (cr);

        return surface;
}

void
set_user_icon_data (ActUser   *user,
                    GdkPixbuf *pixbuf)
{
        g_autofree gchar *path = NULL;
        gint fd;
        g_autoptr(GOutputStream) stream = NULL;
        g_autoptr(GError) error = NULL;

        path = g_build_filename (g_get_tmp_dir (), "gnome-control-center-user-icon-XXXXXX", NULL);
        fd = g_mkstemp (path);

        if (fd == -1) {
                g_warning ("failed to create temporary file for image data");
                return;
        }

        stream = g_unix_output_stream_new (fd, TRUE);

        if (!gdk_pixbuf_save_to_stream (pixbuf, stream, "png", NULL, &error, NULL)) {
                g_warning ("failed to save image: %s", error->message);
                return;
        }

        act_user_set_icon_file (user, path);

        /* if we ever make the dbus call async, the g_remove call needs
         * to wait for its completion
         */
        g_remove (path);
}

GdkPixbuf *
generate_default_avatar (ActUser *user, gint size)
{
        const gchar *name;
        GdkPixbuf *pixbuf = NULL;
        cairo_surface_t *surface;

        name = act_user_get_real_name (user);
        if (name == NULL)
                name = "";
        surface = generate_user_picture (name, size);

        pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0, size, size);
        cairo_surface_destroy (surface);

        return pixbuf;
}

void
set_default_avatar (ActUser *user)
{
        g_autoptr(GdkPixbuf) pixbuf = NULL;

        pixbuf = generate_default_avatar (user, IMAGE_SIZE);

        set_user_icon_data (user, pixbuf);
}
