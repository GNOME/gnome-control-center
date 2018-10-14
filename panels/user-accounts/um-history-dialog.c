/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Written by: Ondrej Holy <oholy@redhat.com>
 */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <act/act.h>

#include "cc-util.h"

#include "um-history-dialog.h"
#include "um-utils.h"

struct _UmHistoryDialog {
        GtkWidget *dialog;
        GtkBuilder *builder;

        GDateTime *week;
        GDateTime *current_week;

        ActUser *user;
};

typedef struct {
	gint64 login_time;
	gint64 logout_time;
	const gchar *type;
} UmLoginHistory;

static GtkWidget *
get_widget (UmHistoryDialog *um,
            const char *name)
{
        return (GtkWidget *)gtk_builder_get_object (um->builder, name);
}

static void
show_week_label (UmHistoryDialog *um)
{
        gchar *label, *from, *to;
        GDateTime *date;
        GTimeSpan span;

        span = g_date_time_difference (um->current_week, um->week);
        if (span == 0) {
                label = g_strdup (_("This Week"));
        }
        else if (span == G_TIME_SPAN_DAY * 7) {
                label = g_strdup (_("Last Week"));
        }
        else {
                date = g_date_time_add_days (um->week, 6);
                /* Translators: This is a date format string in the style of "Feb 18",
                   shown as the first day of a week on login history dialog. */
                from = g_date_time_format (um->week, C_("login history week label","%b %e"));
                if (g_date_time_get_year (um->week) == g_date_time_get_year (um->current_week)) {
                        /* Translators: This is a date format string in the style of "Feb 24",
                           shown as the last day of a week on login history dialog. */
                        to = g_date_time_format (date, C_("login history week label","%b %e"));
                }
                else {
                        /* Translators: This is a date format string in the style of "Feb 24, 2013",
                           shown as the last day of a week on login history dialog. */
                        to = g_date_time_format (date, C_("login history week label","%b %e, %Y"));
                }

                /* Translators: This indicates a week label on a login history.
                   The first %s is the first day of a week, and the second %s the last day. */
                label = g_strdup_printf(C_("login history week label", "%s — %s"), from, to);

                g_date_time_unref (date);
                g_free (from);
                g_free (to);
        }

        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (get_widget (um, "dialog-header-bar")), label);

        g_free (label);
}

static void
clear_history (UmHistoryDialog *um)
{
        GtkWidget *box;
        GList *list, *it;

        box = get_widget (um, "history-box");
        list = gtk_container_get_children (GTK_CONTAINER (box));
        for (it = list; it != NULL;  it = it->next) {
                gtk_container_remove (GTK_CONTAINER (box), GTK_WIDGET (it->data));
        }
        g_list_free (list);
}

static GArray *
get_login_history (ActUser *user)
{
	GArray *login_history;
	GVariantIter *iter, *iter2;
	GVariant *variant;
	const GVariant *value;
	const gchar *key;
	UmLoginHistory history;

	login_history = NULL;
	value = act_user_get_login_history (user);
	g_variant_get ((GVariant *) value, "a(xxa{sv})", &iter);
	while (g_variant_iter_loop (iter, "(xxa{sv})", &history.login_time, &history.logout_time, &iter2)) {
		while (g_variant_iter_loop (iter2, "{sv}", &key, &variant)) {
			if (g_strcmp0 (key, "type") == 0) {
				history.type = g_variant_get_string (variant, NULL);
			}
		}

		if (login_history == NULL) {
			login_history = g_array_new (FALSE, TRUE, sizeof (UmLoginHistory));
		}

		g_array_append_val (login_history, history);
	}

	return login_history;
}

static void
set_sensitivity (UmHistoryDialog *um)
{
        GArray *login_history;
        UmLoginHistory history;
        gboolean sensitive = FALSE;

        login_history = get_login_history (um->user);
        if (login_history != NULL) {
                history = g_array_index (login_history, UmLoginHistory, 0);
                sensitive = g_date_time_to_unix (um->week) > history.login_time;
                g_array_free (login_history, TRUE);
        }
        gtk_widget_set_sensitive (get_widget (um, "previous-button"), sensitive);

        sensitive = (g_date_time_compare (um->current_week, um->week) == 1);
        gtk_widget_set_sensitive (get_widget (um, "next-button"), sensitive);
}

static void
add_record (GtkWidget *box, GDateTime *datetime, gchar *record_string, gint line)
{
        gchar *date, *time, *str;
        GtkWidget *label, *row;

        date = cc_util_get_smart_date (datetime);
        /* Translators: This is a time format string in the style of "22:58".
           It indicates a login time which follows a date. */
        time = g_date_time_format (datetime, C_("login date-time", "%k:%M"));
        /* Translators: This indicates a login date-time.
           The first %s is a date, and the second %s a time. */
        str = g_strdup_printf(C_("login date-time", "%s, %s"), date, time);

        row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_show (row);
        gtk_box_set_homogeneous (GTK_BOX (row), TRUE);
        gtk_container_set_border_width (GTK_CONTAINER (row), 6);

        label = gtk_label_new (record_string);
        gtk_widget_show (label);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);

        label = gtk_label_new (str);
        gtk_widget_show (label);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_box_pack_start (GTK_BOX (row), label, TRUE, TRUE, 0);
        g_free (str);
        g_free (date);
        g_free (time);
        g_date_time_unref (datetime);

        gtk_list_box_insert (GTK_LIST_BOX (box), row, line);
}

static void
show_week (UmHistoryDialog *um)
{
        GArray *login_history;
        GDateTime *datetime, *temp;
        gint64 from, to;
        gint i, line;
        GtkWidget *box;
        UmLoginHistory history;

        show_week_label (um);
        clear_history (um);
        set_sensitivity (um);

        login_history = get_login_history (um->user);
        if (login_history == NULL) {
                return;
        }

        /* Find first record for week */
        from = g_date_time_to_unix (um->week);
        temp = g_date_time_add_weeks (um->week, 1);
        to = g_date_time_to_unix (temp);
        g_date_time_unref (temp);
        for (i = login_history->len - 1; i >= 0; i--) {
                history = g_array_index (login_history, UmLoginHistory, i);
                if (history.login_time < to) {
                        break;
                }
        }

        /* Add new session records */
        box = get_widget (um, "history-box");
        gtk_widget_show (box);
        line = 0;
        for (;i >= 0; i--) {
                history = g_array_index (login_history, UmLoginHistory, i);

                /* Display only x-session and tty records */
                if (!g_str_has_prefix (history.type, ":") &&
                    !g_str_has_prefix (history.type, "tty")) {
                        continue;
                }

                if (history.logout_time > 0 && history.logout_time < from) {
                        break;
                }

                if (history.logout_time > 0 && history.logout_time < to) {
                        datetime = g_date_time_new_from_unix_local (history.logout_time);
                        add_record (box, datetime, _("Session Ended"), line);
                        line++;
                }

                if (history.login_time >= from) {
                        datetime = g_date_time_new_from_unix_local (history.login_time);
                        add_record (box, datetime, _("Session Started"), line);
                        line++;
                }
        }

        g_array_free (login_history, TRUE);
}

static void
show_previous (GtkButton       *button,
               UmHistoryDialog *um)
{
        GDateTime *temp;

        temp = um->week;
        um->week = g_date_time_add_weeks (um->week, -1);
        g_date_time_unref (temp);

        show_week (um);
}

static void
show_next (GtkButton       *button,
           UmHistoryDialog *um)
{
        GDateTime *temp;

        temp = um->week;
        um->week = g_date_time_add_weeks (um->week, 1);
        g_date_time_unref (temp);

        show_week (um);
}

static void
update_dialog_title (UmHistoryDialog *um)
{
        gchar *title;

        /* Translators: This is the title of the "Account Activity" dialog.
           The %s is the user real name. */
        title = g_strdup_printf (_("%s — Account Activity"),
                                 act_user_get_real_name (um->user));

        gtk_window_set_title (GTK_WINDOW (um->dialog), title);

        g_free (title);
}

void
um_history_dialog_set_user (UmHistoryDialog *um,
                            ActUser         *user)
{
        if (um->user) {
                g_clear_object (&um->user);
        }

        if (user) {
                um->user = g_object_ref (user);
        }

        update_dialog_title (um);
}

void
um_history_dialog_show (UmHistoryDialog *um,
                        GtkWindow       *parent)
{
        GDateTime *temp, *local;
        gint parent_width;

        if (um->week)
                g_date_time_unref (um->week);
        if (um->current_week)
                g_date_time_unref (um->current_week);

        /* Set the first day of this week */
        local = g_date_time_new_now_local ();
        temp = g_date_time_new_local (g_date_time_get_year (local),
                                      g_date_time_get_month (local),
                                      g_date_time_get_day_of_month (local),
                                      0, 0, 0);
        um->week = g_date_time_add_days (temp, 1 - g_date_time_get_day_of_week (temp));
        um->current_week = g_date_time_ref (um->week);
        g_date_time_unref (local);
        g_date_time_unref (temp);

        show_week (um);

        gtk_window_get_size (parent, &parent_width, NULL);
        gtk_window_set_default_size (GTK_WINDOW (um->dialog), parent_width * 0.6, -1);
        gtk_window_set_transient_for (GTK_WINDOW (um->dialog), parent);
        gtk_window_present (GTK_WINDOW (um->dialog));
}

UmHistoryDialog *
um_history_dialog_new (void)
{
        GError *error = NULL;
        UmHistoryDialog *um;
        GtkWidget *widget;

        um = g_new0 (UmHistoryDialog, 1);
        um->builder = gtk_builder_new ();

        if (!gtk_builder_add_from_resource (um->builder, "/org/gnome/control-center/user-accounts/history-dialog.ui", &error)) {
                g_error ("%s", error->message);
                g_error_free (error);
                g_free (um);

                return NULL;
        }

        um->dialog = get_widget (um, "dialog");
        g_signal_connect (um->dialog, "delete-event", G_CALLBACK (gtk_widget_hide_on_delete), NULL);

        widget = get_widget (um, "next-button");
        g_signal_connect (widget, "clicked", G_CALLBACK (show_next), um);

        widget = get_widget (um, "previous-button");
        g_signal_connect (widget, "clicked", G_CALLBACK (show_previous), um);

        widget = get_widget (um, "next-image");
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      "go-next-symbolic",
                                      GTK_ICON_SIZE_MENU);

        widget = get_widget (um, "previous-image");
        gtk_image_set_from_icon_name (GTK_IMAGE (widget),
                                      "go-previous-symbolic",
                                      GTK_ICON_SIZE_MENU);

        return um;
}

void
um_history_dialog_free (UmHistoryDialog *um)
{
        gtk_widget_destroy (um->dialog);

        g_clear_object (&um->user);
        g_clear_object (&um->builder);

        if (um->week) {
                g_date_time_unref (um->week);
        }

        if (um->current_week) {
                g_date_time_unref (um->current_week);
        }

        g_free (um);
}
