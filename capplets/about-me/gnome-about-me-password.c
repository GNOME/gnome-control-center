/* gnome-about-me.c
 * Copyright (C) 2002 Diego Gonzalez 
 *
 * Written by: Diego Gonzalez <diego@pemas.net>
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
 *
 * Parts of this code come from Gnome-System-Tools.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <pwd.h>
#include <glade/glade.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <pty.h>

#include "capplet-util.h"

typedef struct {
	GtkWidget *dialog;
	GtkWidget *old_password;
	GtkWidget *new_password;
	GtkWidget *retyped_password;

	/* Communication with the passwd program */
        FILE *backend_stream;
        int   backend_master_fd;
        int   backend_pid;
        guint timeout_id;
} PasswordDialog;

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static void passdlg_set_busy (PasswordDialog *dlg, gboolean busy);

#define REDRAW_NCHARS 1

static gboolean
wait_child (PasswordDialog *pdialog)
{
	GtkWidget *dialog;
	gint status, pid;
	gchar *primary_text   = NULL;
	gchar *secondary_text = NULL;

	pid = waitpid (pdialog->backend_pid, &status, WNOHANG);

	if (pid > 0) {

		if (WIFEXITED (status) && (WEXITSTATUS(status) == 0)) {
			passdlg_set_busy (pdialog, FALSE);
			primary_text = g_strdup (_("Password changed successfully"));

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_INFO,
						 	 GTK_BUTTONS_CLOSE,
							 primary_text, secondary_text);
			g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_widget_show (dialog);

			return FALSE;
		} else if ((WIFEXITED (status)) && (WEXITSTATUS (status)) && (WEXITSTATUS(status) < 255)) {
			/* the proccess was running su */
			primary_text   = g_strdup (_("The entered password is invalid"));
			secondary_text = g_strdup (_("Check that you typed it correctly "
						     "and that you haven't activated the \"caps lock\" key"));
		} else if ((WIFEXITED (status)) && (WEXITSTATUS (status)) && (WEXITSTATUS (status) == 255)) {
			primary_text = g_strdup (_("Could not run passwd"));
			secondary_text = g_strdup (_("Check that you have permissions to run this command"));
		} else {
			primary_text = g_strdup (_("An unexpected error has ocurred"));
		}
		
		if (primary_text) {
			passdlg_set_busy (pdialog, FALSE);
			dialog = gtk_message_dialog_new (NULL,
						         GTK_DIALOG_DESTROY_WITH_PARENT,
							 GTK_MESSAGE_ERROR,
							 GTK_BUTTONS_CLOSE,
							 primary_text, secondary_text);

			g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
			gtk_widget_show (dialog);

			g_free (primary_text);
			g_free (secondary_text);
		}

		return FALSE;
	}

	return TRUE;
}
  
static gboolean
is_string_complete (gchar *str, GSList *list)
{
	GSList *elem;
  
	if (strlen (str) == 0)
		return FALSE;
  
	for (elem = list; elem; elem = g_slist_next (elem))
		if (g_strrstr (str, elem->data) != NULL)
			return TRUE;
  
	return FALSE;
}
 
static char *
read_from_backend_va (PasswordDialog *pdialog, gchar *needle, va_list ap)
{
	GString *str = g_string_new ("");
	gboolean may_exit = FALSE;
	gint i = 0;
	gchar c, *ptr, *arg;
	GSList *list = NULL;
 
	list = g_slist_prepend (list, needle);

	while ((arg = va_arg (ap, char*)) != NULL)
		list = g_slist_prepend (list, arg);
	va_end (ap);

	while (!is_string_complete (str->str, list)) {
		c = fgetc (pdialog->backend_stream);
		i++;

		if (*str->str)
			g_string_append_c (str, c);
		else {
			/* the string is still empty, read with O_NONBLOCK until
			 *  it gets a char, this is done for not blocking the UI
			 */
			if (c != EOF) {
				g_string_append_c (str, c);
				fcntl (pdialog->backend_master_fd, F_SETFL, 0);
			}
			usleep (500);
		}

		/* ugly hack for redrawing UI without too much overload */
		if (i == REDRAW_NCHARS) {
			while (gtk_events_pending ())
				gtk_main_iteration ();
			i = 0;
		}
	}
 
	fcntl (pdialog->backend_master_fd, F_SETFL, O_NONBLOCK);
	ptr = str->str;
	g_string_free (str, FALSE);
 
	return ptr;
}

static gchar*
read_from_backend (PasswordDialog *pdialog, gchar *needle, ...)
{
	va_list ap;
  
	va_start (ap, needle);
	return read_from_backend_va (pdialog, needle, ap);
}

static void
write_to_backend (PasswordDialog *pdialog, char *str)
{
	gint nread = 0;
	int ret;

	/* turn the descriptor blocking for writing the configuration */
	fcntl (pdialog->backend_master_fd, F_SETFL, 0);
 
	do {
		ret = fputc (str [nread], pdialog->backend_stream);
 
		if (ret != EOF)
			nread++;
  
		/* ugly hack for redrawing UI */
		if (nread % REDRAW_NCHARS == 0)
			while (gtk_events_pending ())
		gtk_main_iteration ();
	} while (nread < strlen (str));
  
	while (fflush (pdialog->backend_stream) != 0);
 
	fcntl (pdialog->backend_master_fd, F_SETFL, O_NONBLOCK);
}

static void
passdlg_set_busy (PasswordDialog *pdialog, gboolean busy)
{
	GtkWindow  *toplevel = GTK_WINDOW (pdialog->dialog);
	GdkCursor  *cursor = NULL;
	GdkDisplay *display;
                        
	display = gtk_widget_get_display (GTK_WIDGET (toplevel));
	if (busy)
		cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
	
	gdk_window_set_cursor (GTK_WIDGET (toplevel)->window, cursor);
	gdk_display_flush (display);

	if (busy)
		gdk_cursor_unref (cursor);
}


static void
passdlg_button_clicked_cb (GtkDialog *dialog, gint response_id, PasswordDialog *pdialog) 
{
	      
	char *new_password;
	char *retyped_password;
	char *old_password;
	char *args[2];
	gchar *s;

	if (response_id == GTK_RESPONSE_OK) {
		/* */
		old_password = g_strdup_printf ("%s\n", gtk_entry_get_text (GTK_ENTRY (pdialog->old_password)));
		new_password = g_strdup_printf ("%s\n", gtk_entry_get_text (GTK_ENTRY (pdialog->new_password)));
		retyped_password = g_strdup_printf ("%s\n", gtk_entry_get_text (GTK_ENTRY (pdialog->retyped_password)));

		/* Set the busy cursor as this can be a long process */
		passdlg_set_busy (pdialog, TRUE);

		/* Prepare the execution environment of passwd */
		args[0] = "/usr/bin/passwd";
		args[1] = NULL;

		pdialog->backend_pid = forkpty (&pdialog->backend_master_fd, NULL, NULL, NULL);
		if (pdialog->backend_pid < 0) {
			g_warning ("could not fork to backend");
			gtk_main_quit ();
		} else if (pdialog->backend_pid == 0) {
			execv (args[0], args);
			exit (255);
		} else {
			fcntl (pdialog->backend_master_fd, F_SETFL, O_NONBLOCK);
			pdialog->timeout_id = g_timeout_add (1000, (GSourceFunc) wait_child, pdialog);
			pdialog->backend_stream = fdopen (pdialog->backend_master_fd, "a+");
		}

		/* Send current password to backend */
		s = read_from_backend (pdialog, "assword:", ": ", NULL);
		write_to_backend (pdialog, old_password);

		s = read_from_backend (pdialog, "assword:", ": ", "\n", NULL);
		while (strlen(s) < 4) {
			usleep(1000);
			s = read_from_backend (pdialog, "assword:", ": ", "\n", NULL);
		}

		/* Send new password to backend */
		write_to_backend (pdialog, new_password);

		s = read_from_backend (pdialog, "assword:", ": ", "\n", NULL);
		while (strlen(s) < 4) {
			usleep(1000);
			s = read_from_backend (pdialog, "assword:", ": ", "\n", NULL);
		}

		/* Send new and retyped password to backend */
		write_to_backend (pdialog, retyped_password);
		s = read_from_backend (pdialog, "assword:", ": ", "\n", NULL);
		while (strlen(s) < 4) {
			usleep(1000);
			s = read_from_backend (pdialog, "\n", NULL);
		}

	} else {
		gtk_main_quit ();
	}
}

void
gnome_about_me_password (void)
{
	PasswordDialog *pdialog;
	GladeXML *dialog;
	
	pdialog = g_new0 (PasswordDialog, 1);

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-about-me.glade", "change-password", NULL);

	pdialog->dialog = WID ("change-password");
	g_signal_connect (G_OBJECT (pdialog->dialog), "response",
			  G_CALLBACK (passdlg_button_clicked_cb), pdialog);

	pdialog->old_password = WID ("old-password");
	pdialog->new_password = WID ("new-password");
	pdialog->retyped_password = WID ("retyped-password");

	gtk_window_set_resizable (GTK_WINDOW (pdialog->dialog), FALSE);
	gtk_widget_show_all (pdialog->dialog);
	gtk_main ();

	gtk_widget_destroy (pdialog->dialog);
	g_object_unref (G_OBJECT (dialog));
	g_free (pdialog);
}
