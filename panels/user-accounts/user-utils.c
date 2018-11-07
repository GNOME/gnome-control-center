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
#include <glib/gi18n.h>

#include "user-utils.h"

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
