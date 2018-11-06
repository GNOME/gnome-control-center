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
#include "um-resources.h"
#include "um-utils.h"

struct _UmHistoryDialog
{
        GtkDialog     parent_instance;

        GtkHeaderBar *header_bar;
        GtkListBox   *history_box;
        GtkButton    *next_button;
        GtkButton    *previous_button;

        GDateTime    *week;
        GDateTime    *current_week;

        ActUser      *user;
};

G_DEFINE_TYPE (UmHistoryDialog, um_history_dialog, GTK_TYPE_DIALOG)

typedef struct {
	gint64 login_time;
	gint64 logout_time;
	const gchar *type;
} UmLoginHistory;

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

        gtk_header_bar_set_subtitle (um->header_bar, label);

        g_free (label);
}

static void
clear_history (UmHistoryDialog *um)
{
        GList *list, *it;

        list = gtk_container_get_children (GTK_CONTAINER (um->history_box));
        for (it = list; it != NULL; it = it->next) {
                gtk_container_remove (GTK_CONTAINER (um->history_box), GTK_WIDGET (it->data));
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
        gtk_widget_set_sensitive (GTK_WIDGET (um->previous_button), sensitive);

        sensitive = (g_date_time_compare (um->current_week, um->week) == 1);
        gtk_widget_set_sensitive (GTK_WIDGET (um->next_button), sensitive);
}

static void
add_record (UmHistoryDialog *um, GDateTime *datetime, gchar *record_string, gint line)
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

        gtk_list_box_insert (um->history_box, row, line);
}

static void
show_week (UmHistoryDialog *um)
{
        GArray *login_history;
        GDateTime *datetime, *temp;
        gint64 from, to;
        gint i, line;
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
                        add_record (um, datetime, _("Session Ended"), line);
                        line++;
                }

                if (history.login_time >= from) {
                        datetime = g_date_time_new_from_unix_local (history.login_time);
                        add_record (um, datetime, _("Session Started"), line);
                        line++;
                }
        }

        g_array_free (login_history, TRUE);
}

static void
previous_button_clicked_cb (UmHistoryDialog *um)
{
        GDateTime *temp;

        temp = um->week;
        um->week = g_date_time_add_weeks (um->week, -1);
        g_date_time_unref (temp);

        show_week (um);
}

static void
next_button_clicked_cb (UmHistoryDialog *um)
{
        GDateTime *temp;

        temp = um->week;
        um->week = g_date_time_add_weeks (um->week, 1);
        g_date_time_unref (temp);

        show_week (um);
}

static void
um_history_dialog_dispose (GObject *object)
{
        UmHistoryDialog *um = UM_HISTORY_DIALOG (object);

        g_clear_object (&um->user);
        g_clear_pointer (&um->week, g_date_time_unref);
        g_clear_pointer (&um->current_week, g_date_time_unref);

        G_OBJECT_CLASS (um_history_dialog_parent_class)->dispose (object);
}

void
um_history_dialog_class_init (UmHistoryDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = um_history_dialog_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/user-accounts/um-history-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, UmHistoryDialog, header_bar);
        gtk_widget_class_bind_template_child (widget_class, UmHistoryDialog, history_box);
        gtk_widget_class_bind_template_child (widget_class, UmHistoryDialog, next_button);
        gtk_widget_class_bind_template_child (widget_class, UmHistoryDialog, previous_button);

        gtk_widget_class_bind_template_callback (widget_class, next_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, previous_button_clicked_cb);
}

void
um_history_dialog_init (UmHistoryDialog *um)
{
        g_resources_register (um_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (um));
}

UmHistoryDialog *
um_history_dialog_new (ActUser *user)
{
        UmHistoryDialog *um;
        GDateTime *temp, *local;
        g_autofree gchar *title = NULL;

        g_return_val_if_fail (ACT_IS_USER (user), NULL);

        um = g_object_new (UM_TYPE_HISTORY_DIALOG,
                           "use-header-bar", 1,
                           NULL);

        um->user = g_object_ref (user);

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

        /* Translators: This is the title of the "Account Activity" dialog.
           The %s is the user real name. */
        title = g_strdup_printf (_("%s — Account Activity"),
                                 act_user_get_real_name (um->user));
        gtk_header_bar_set_title (um->header_bar, title);

        show_week (um);

        return um;
}
