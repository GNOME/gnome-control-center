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

#include "cc-login-history-dialog.h"
#include "cc-user-accounts-resources.h"
#include "cc-util.h"
#include "user-utils.h"

struct _CcLoginHistoryDialog
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

G_DEFINE_TYPE (CcLoginHistoryDialog, cc_login_history_dialog, GTK_TYPE_DIALOG)

typedef struct {
	gint64 login_time;
	gint64 logout_time;
	const gchar *type;
} CcLoginHistory;

static void
show_week_label (CcLoginHistoryDialog *self)
{
        g_autofree gchar *label = NULL;
        GTimeSpan span;

        span = g_date_time_difference (self->current_week, self->week);
        if (span == 0) {
                label = g_strdup (_("This Week"));
        }
        else if (span == G_TIME_SPAN_DAY * 7) {
                label = g_strdup (_("Last Week"));
        }
        else {
                g_autofree gchar *from = NULL;
                g_autofree gchar *to = NULL;
                g_autoptr(GDateTime) date = NULL;

                date = g_date_time_add_days (self->week, 6);
                /* Translators: This is a date format string in the style of "Feb 18",
                   shown as the first day of a week on login history dialog. */
                from = g_date_time_format (self->week, C_("login history week label","%b %e"));
                if (g_date_time_get_year (self->week) == g_date_time_get_year (self->current_week)) {
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
        }

        gtk_header_bar_set_subtitle (self->header_bar, label);
}

static void
clear_history (CcLoginHistoryDialog *self)
{
        g_autoptr(GList) list = NULL;
        GList *it;

        list = gtk_container_get_children (GTK_CONTAINER (self->history_box));
        for (it = list; it != NULL; it = it->next) {
                gtk_container_remove (GTK_CONTAINER (self->history_box), GTK_WIDGET (it->data));
        }
}

static GArray *
get_login_history (ActUser *user)
{
	GArray *login_history;
	GVariantIter *iter, *iter2;
	GVariant *variant;
	const GVariant *value;
	const gchar *key;
	CcLoginHistory history;

	login_history = NULL;
	value = act_user_get_login_history (user);
	g_variant_get ((GVariant *) value, "a(xxa{sv})", &iter);
	while (g_variant_iter_loop (iter, "(xxa{sv})", &history.login_time, &history.logout_time, &iter2)) {
		while (g_variant_iter_loop (iter2, "{&sv}", &key, &variant)) {
			if (g_strcmp0 (key, "type") == 0) {
				history.type = g_variant_get_string (variant, NULL);
			}
		}

		if (login_history == NULL) {
			login_history = g_array_new (FALSE, TRUE, sizeof (CcLoginHistory));
		}

		g_array_append_val (login_history, history);
	}

	return login_history;
}

static void
set_sensitivity (CcLoginHistoryDialog *self)
{
        g_autoptr(GArray) login_history = NULL;
        CcLoginHistory history;
        gboolean sensitive = FALSE;

        login_history = get_login_history (self->user);
        if (login_history != NULL) {
                history = g_array_index (login_history, CcLoginHistory, 0);
                sensitive = g_date_time_to_unix (self->week) > history.login_time;
        }
        gtk_widget_set_sensitive (GTK_WIDGET (self->previous_button), sensitive);

        sensitive = (g_date_time_compare (self->current_week, self->week) == 1);
        gtk_widget_set_sensitive (GTK_WIDGET (self->next_button), sensitive);
}

static void
add_record (CcLoginHistoryDialog *self, GDateTime *datetime, gchar *record_string, gint line)
{
        g_autofree gchar *date = NULL;
        g_autofree gchar *time = NULL;
        g_autofree gchar *str = NULL;
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

        gtk_list_box_insert (self->history_box, row, line);
}

static void
show_week (CcLoginHistoryDialog *self)
{
        g_autoptr(GArray) login_history = NULL;
        g_autoptr(GDateTime) datetime = NULL;
        g_autoptr(GDateTime) temp = NULL;
        gint64 from, to;
        gint i, line;
        CcLoginHistory history;

        show_week_label (self);
        clear_history (self);
        set_sensitivity (self);

        login_history = get_login_history (self->user);
        if (login_history == NULL) {
                return;
        }

        /* Find first record for week */
        from = g_date_time_to_unix (self->week);
        temp = g_date_time_add_weeks (self->week, 1);
        to = g_date_time_to_unix (temp);
        for (i = login_history->len - 1; i >= 0; i--) {
                history = g_array_index (login_history, CcLoginHistory, i);
                if (history.login_time < to) {
                        break;
                }
        }

        /* Add new session records */
        line = 0;
        for (;i >= 0; i--) {
                history = g_array_index (login_history, CcLoginHistory, i);

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
                        add_record (self, datetime, _("Session Ended"), line);
                        line++;
                }

                if (history.login_time >= from) {
                        datetime = g_date_time_new_from_unix_local (history.login_time);
                        add_record (self, datetime, _("Session Started"), line);
                        line++;
                }
        }
}

static void
previous_button_clicked_cb (CcLoginHistoryDialog *self)
{
        g_autoptr(GDateTime) temp = NULL;

        temp = self->week;
        self->week = g_date_time_add_weeks (self->week, -1);

        show_week (self);
}

static void
next_button_clicked_cb (CcLoginHistoryDialog *self)
{
        g_autoptr(GDateTime) temp = NULL;

        temp = self->week;
        self->week = g_date_time_add_weeks (self->week, 1);

        show_week (self);
}

static void
cc_login_history_dialog_dispose (GObject *object)
{
        CcLoginHistoryDialog *self = CC_LOGIN_HISTORY_DIALOG (object);

        g_clear_object (&self->user);
        g_clear_pointer (&self->week, g_date_time_unref);
        g_clear_pointer (&self->current_week, g_date_time_unref);

        G_OBJECT_CLASS (cc_login_history_dialog_parent_class)->dispose (object);
}

void
cc_login_history_dialog_class_init (CcLoginHistoryDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = cc_login_history_dialog_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/user-accounts/cc-login-history-dialog.ui");

        gtk_widget_class_bind_template_child (widget_class, CcLoginHistoryDialog, header_bar);
        gtk_widget_class_bind_template_child (widget_class, CcLoginHistoryDialog, history_box);
        gtk_widget_class_bind_template_child (widget_class, CcLoginHistoryDialog, next_button);
        gtk_widget_class_bind_template_child (widget_class, CcLoginHistoryDialog, previous_button);

        gtk_widget_class_bind_template_callback (widget_class, next_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, previous_button_clicked_cb);
}

void
cc_login_history_dialog_init (CcLoginHistoryDialog *self)
{
        g_resources_register (cc_user_accounts_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));
}

CcLoginHistoryDialog *
cc_login_history_dialog_new (ActUser *user)
{
        CcLoginHistoryDialog *self;
        g_autoptr(GDateTime) temp = NULL;
        g_autoptr(GDateTime) local = NULL;
        g_autofree gchar *title = NULL;

        g_return_val_if_fail (ACT_IS_USER (user), NULL);

        self = g_object_new (CC_TYPE_LOGIN_HISTORY_DIALOG,
                             "use-header-bar", 1,
                             NULL);

        self->user = g_object_ref (user);

        /* Set the first day of this week */
        local = g_date_time_new_now_local ();
        temp = g_date_time_new_local (g_date_time_get_year (local),
                                      g_date_time_get_month (local),
                                      g_date_time_get_day_of_month (local),
                                      0, 0, 0);
        self->week = g_date_time_add_days (temp, 1 - g_date_time_get_day_of_week (temp));
        self->current_week = g_date_time_ref (self->week);

        /* Translators: This is the title of the "Account Activity" dialog.
           The %s is the user real name. */
        title = g_strdup_printf (_("%s — Account Activity"),
                                 act_user_get_real_name (self->user));
        gtk_header_bar_set_title (self->header_bar, title);

        show_week (self);

        return self;
}
