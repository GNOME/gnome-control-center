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
#include <stdlib.h>
#include <glade/glade.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <termios.h>
#include <pty.h>

#include "capplet-util.h"
#include "eel-alert-dialog.h"

typedef struct {
	GladeXML  *xml;

	GtkWidget *old_password;
	GtkWidget *new_password;
	GtkWidget *retyped_password;

        guint timeout_id;
        guint check_password_timeout_id;

        gboolean good_password;

	/* Communication with the passwd program */
        int   backend_pid;

        int   write_fd;
        int   read_fd;
        
	FILE *write_stream;
	FILE *read_stream;
           
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
	gint status, pid;
	gchar *msg, *details, *title;
	GladeXML *dialog;
	GtkWidget *wmessage, *wbulb;
	GtkWidget *wedialog;

	dialog = pdialog->xml;

	wmessage = WID ("message");
	wbulb = WID ("bulb");
	
	pid = waitpid (pdialog->backend_pid, &status, WNOHANG);
	passdlg_set_busy (pdialog, FALSE);

	if (pid > 0) {

		if (WIFEXITED (status) && (WEXITSTATUS(status) == 0)) {
			/* I need to quit here */

			return FALSE;
		} else if ((WIFEXITED (status)) && (WEXITSTATUS (status)) && (WEXITSTATUS(status) < 255)) {

			msg = g_strdup_printf ("<b>%s</b>", _("Old password is incorrect, please retype it"));

			gtk_label_set_markup (GTK_LABEL (wmessage), msg);
			g_free (msg);

			gtk_image_set_from_file (GTK_IMAGE (wbulb),
						 GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-off.png");


			return FALSE;
		} else if ((WIFEXITED (status)) && (WEXITSTATUS (status)) && (WEXITSTATUS (status) == 255)) {
			msg = g_strdup (_("System error has occurred"));
			details = g_strdup (_("Could not run /usr/bin/passwd"));
			title = g_strdup (_("Unable to launch backend"));
		} else {
			msg = g_strdup (_("Unexpected error has occurred"));
			title = g_strdup (_("Unexpected error has occurred"));
			details = NULL;
		}
		
		wedialog = eel_alert_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
						 msg, NULL, title);

		if (details != NULL)
			eel_alert_dialog_set_details_label (EEL_ALERT_DIALOG (wedialog), details);

			g_signal_connect (G_OBJECT (wedialog), "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);

		gtk_window_set_resizable (GTK_WINDOW (wedialog), FALSE);
		gtk_widget_show (wedialog);

		g_free (msg);
		g_free (title);
		g_free (details);

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
		if (g_strrstr (str, elem->data) != NULL) {
			return TRUE;
		}
  
	return FALSE;
}
 
static gchar*
read_everything (PasswordDialog *pdialog, gchar *needle, va_list ap)
{
	GString *str  = g_string_new ("");
	GSList  *list = NULL;
	gchar*arg, *ptr;
	int c;

	list = g_slist_prepend (list, needle);
  
	while ((arg = va_arg (ap, char*)) != NULL)
		list = g_slist_prepend (list, arg);
  
	va_end (ap);
 
	while (!is_string_complete (str->str, list)) {
		c = fgetc (pdialog->read_stream);
 
		if (c != EOF)
			g_string_append_c (str, c);
	}
 
        ptr = str->str;
        g_string_free (str, FALSE);
  
	return ptr;
}
 
static void
poll_backend (PasswordDialog *pdialog)
{
	struct pollfd fd;

	fd.fd = pdialog->read_fd;
	fd.events = POLLIN || POLLPRI;

	while (poll (&fd, 1, 100) <= 0) {
		while (gtk_events_pending ())
			gtk_main_iteration ();
	}
}

static char *
read_from_backend_va (PasswordDialog *pdialog, gchar *needle, va_list ap)
{
	poll_backend (pdialog);
	return read_everything (pdialog, needle, ap);
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

	do {
		ret = fputc (str [nread], pdialog->write_stream);
 
		usleep (1000);
 
		if (ret != EOF)
			nread++;
  
		/* ugly hack for redrawing UI */
		if (nread % REDRAW_NCHARS == 0)
			while (gtk_events_pending ())
		gtk_main_iteration ();
	} while (nread < strlen (str));
  
	while (fflush (pdialog->write_stream) != 0);
}

static void
passdlg_set_busy (PasswordDialog *pdialog, gboolean busy)
{
	GladeXML   *dialog;
	GtkWidget  *toplevel;
	GdkCursor  *cursor = NULL;
	GdkDisplay *display;

	dialog = pdialog->xml;
	toplevel = WID ("change-password");

	display = gtk_widget_get_display (toplevel);
	if (busy)
		cursor = gdk_cursor_new_for_display (display, GDK_WATCH);
	
	gdk_window_set_cursor (toplevel->window, cursor);
	gdk_display_flush (display);

	if (busy)
		gdk_cursor_unref (cursor);
}


static gint
update_password (PasswordDialog *pdialog, gchar **msg)
{
	GtkWidget *wopasswd, *wnpasswd, *wrnpasswd;
	char *new_password;
	char *retyped_password;
	char *old_password;
	gchar *s;
	GladeXML *dialog;
	gint retcode;
	
	dialog = pdialog->xml;
	
	wopasswd = WID ("old-password");
	wnpasswd = WID ("new-password");
	wrnpasswd = WID ("retyped-password");

	retcode = 0;

	/* */
	old_password = g_strdup_printf ("%s\n", gtk_entry_get_text (GTK_ENTRY (wopasswd)));
	new_password = g_strdup_printf ("%s\n", gtk_entry_get_text (GTK_ENTRY (wnpasswd)));
	retyped_password = g_strdup_printf ("%s\n", gtk_entry_get_text (GTK_ENTRY (wrnpasswd)));

	/* Set the busy cursor as this can be a long process */
	passdlg_set_busy (pdialog, TRUE);

	s = read_from_backend (pdialog, "assword: ", NULL);
	g_free (s);

	write_to_backend (pdialog, old_password);

	/* New password */
	s = read_from_backend (pdialog, "assword: ", "failure", NULL);
	if (g_strrstr (s, "failure") != NULL) {
		g_free (s);
		return -1;
	}
	g_free (s);

	write_to_backend (pdialog, new_password);

	/* Retype password */		
	s = read_from_backend (pdialog, "assword: ", NULL);
	g_free (s);
		
	write_to_backend (pdialog, retyped_password);
	
	s = read_from_backend (pdialog, "successfully", "short", "panlindrome", "simple", "similar", "wrapped", "recovered",  "unchanged", NULL);
	if (g_strrstr (s, "recovered") != NULL) {
		retcode = -2;
	} else if (g_strrstr (s, "short") != NULL) {
		*msg = g_strdup (_("Password is too short"));
		retcode = -3;
	} else if (g_strrstr (s, "panlindrome") != NULL) {
		*msg = g_strdup (_("Password is too simple"));
		retcode = -3;
	} else if (g_strrstr (s, "simple") != NULL) {
		*msg = g_strdup (_("Password is too simple"));
		retcode = -3;
	} else if ((g_strrstr (s, "similar") != NULL) || (g_strrstr (s, "wrapped") != NULL)) {
		*msg = g_strdup (_("Old and new passwords are too similar"));
		retcode = -3;
	} else if (g_strrstr (s, "unchanged") != NULL) {
		kill (pdialog->backend_pid, SIGKILL);
		*msg = g_strdup (_("Old and new password are the same"));
		retcode = -3;
	}

	g_free (s);
	
	return retcode;
}

static gint
spawn_passwd (PasswordDialog *pdialog)
{
	char *args[2];
	int p[2];

	/* Prepare the execution environment of passwd */
	args[0] = "/usr/bin/passwd";
	args[1] = NULL;

	pipe (p);
	pdialog->backend_pid = forkpty (&pdialog->write_fd, NULL, NULL, NULL);
	if (pdialog->backend_pid < 0) {
		g_warning ("could not fork to backend");
		return -1;
	} else if (pdialog->backend_pid == 0) {
		dup2 (p[1], 1);
		dup2 (p[1], 2);
		close (p[0]);

		unsetenv("LC_ALL");
		unsetenv("LC_MESSAGES");
		unsetenv("LANG");
		unsetenv("LANGUAGE");

		execv (args[0], args);
		exit (255);
	} else {
		close (p[1]);
		pdialog->read_fd = p[0];
		pdialog->timeout_id = g_timeout_add (4000, (GSourceFunc) wait_child, pdialog);

		pdialog->read_stream = fdopen (pdialog->read_fd, "r");;
		pdialog->write_stream = fdopen (pdialog->write_fd, "w");

		setvbuf (pdialog->read_stream, NULL, _IONBF, 0);
		fcntl (pdialog->read_fd, F_SETFL, 0);
	}

	return 1;
}


static gboolean
passdlg_check_password_timeout_cb (PasswordDialog *pdialog)
{
	const gchar *password;
	const gchar *retyped_password;
	char *msg;
	gboolean good_password;

	GtkWidget *wbulb, *wok, *wmessage;
	GtkWidget *wnpassword, *wrnpassword;
	GladeXML *dialog;

	dialog = pdialog->xml;

	wnpassword  = WID ("new-password");
	wrnpassword = WID ("retyped-password");
	wmessage = WID ("message");
	wbulb = WID ("bulb");
	wok   = WID ("ok");

	password = gtk_entry_get_text (GTK_ENTRY (wnpassword));
	retyped_password = gtk_entry_get_text (GTK_ENTRY (wrnpassword));

	if (strlen (password) == 0 || strlen (retyped_password) == 0) {
		gtk_image_set_from_file (GTK_IMAGE (wbulb),
					 GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-off.png");
		msg = g_strconcat ("<b>", _("Please type the passwords."), "</b>", NULL);
		gtk_label_set_markup (GTK_LABEL (wmessage), msg);
		g_free (msg);

		return FALSE;
	}

	if (strcmp (password, retyped_password) != 0) {
		msg = g_strconcat ("<b>", _("Please type the password again, it is wrong."), "</b>", NULL);
		good_password = FALSE;
	} else {
		msg = g_strconcat ("<b>", _("Click on Change Password to change the password."), "</b>", NULL);
		good_password = TRUE;
	}

	if (good_password && pdialog->good_password == FALSE) {
		gtk_image_set_from_file (GTK_IMAGE (wbulb), 
		 			 GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-on.png");
		gtk_widget_set_sensitive (wok, TRUE);
	} else if (good_password == FALSE && pdialog->good_password == TRUE) {
		gtk_image_set_from_file (GTK_IMAGE (wbulb), 
		 			 GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-off.png");
		gtk_widget_set_sensitive (wok, FALSE);
	}

	pdialog->good_password = good_password;

	gtk_label_set_markup (GTK_LABEL (wmessage), msg);
	g_free (msg);

	return FALSE;
}


static void 
passdlg_check_password (GtkEntry *entry, PasswordDialog *pdialog)
{
	if (pdialog->check_password_timeout_id) {
		g_source_remove (pdialog->check_password_timeout_id);
	}

	pdialog->check_password_timeout_id =
		g_timeout_add (500, (GSourceFunc) passdlg_check_password_timeout_cb, pdialog);
	
}

static gint
passdlg_process_response (PasswordDialog *pdialog, gint response_id) 
{
	GladeXML *dialog;
	GtkWidget *wmessage, *wbulb;
	gchar *msg, *msgerr;
	gint ret;

	dialog = pdialog->xml;

	wmessage = WID ("message");
	wbulb = WID ("bulb");

	msgerr = NULL;

	if (response_id == GTK_RESPONSE_OK) {
		ret = spawn_passwd (pdialog);
		if (ret < 0)
			return 1;

		ret = update_password (pdialog, &msgerr);
		passdlg_set_busy (pdialog, FALSE);

		/* No longer need the wait_child fallback, remove the timeout */
		g_source_remove (pdialog->timeout_id);

		if (ret == -1) {
			msg = g_strdup_printf ("<b>%s</b>", _("Old password is incorrect, please retype it"));
			gtk_label_set_markup (GTK_LABEL (wmessage), msg);
			g_free (msg);

			gtk_image_set_from_file (GTK_IMAGE (wbulb),
						 GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-off.png");

			return -1;
		} else if (ret == -3) {
			msg = g_strdup_printf ("<b>%s</b>", msgerr);
			gtk_label_set_markup (GTK_LABEL (wmessage), msg);
			g_free (msg);
			
			gtk_image_set_from_file (GTK_IMAGE (wbulb),
						 GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-off.png");
						 
			g_free (msgerr);
			
			return -1;
		}

		/* This is the standard way of returning from the dialog with passwd 
		 * If we return this way we can safely kill passwd as it has completed
		 * its task. In case of problems we still have the wait_child fallback
		 */
		fclose (pdialog->write_stream);
		fclose (pdialog->read_stream);
		
		close (pdialog->read_fd);
		close (pdialog->write_fd);

		kill (pdialog->backend_pid, 9);

		if (ret == 0)
			return 1;
	} else {
		return 1;
	}
}

void
gnome_about_me_password (GtkWindow *parent)
{
	PasswordDialog *pdialog;
	GtkWidget *wpassdlg;
	GladeXML *dialog;
	gint result;
	
	pdialog = g_new0 (PasswordDialog, 1);

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-about-me.glade", "change-password", NULL);

	pdialog->xml = dialog;

	wpassdlg = WID ("change-password");
	capplet_set_icon (wpassdlg, "user-info");

	pdialog->good_password = FALSE;

	g_signal_connect (G_OBJECT (WID ("new-password")), "changed", 
			  G_CALLBACK (passdlg_check_password), pdialog);
	g_signal_connect (G_OBJECT (WID ("retyped-password")), "changed", 
			  G_CALLBACK (passdlg_check_password), pdialog);

	gtk_image_set_from_file (GTK_IMAGE (WID ("bulb")), GNOMECC_DATA_DIR "/pixmaps/gnome-about-me-bulb-off.png");
	gtk_widget_set_sensitive (WID ("ok"), FALSE);
	
	gtk_window_set_resizable (GTK_WINDOW (wpassdlg), FALSE);
	gtk_window_set_transient_for (GTK_WINDOW (wpassdlg), GTK_WINDOW (parent));
	gtk_widget_show_all (wpassdlg);


	do {
		result = gtk_dialog_run (GTK_DIALOG (wpassdlg));
		result = passdlg_process_response (pdialog, result);
	} while (result <= 0);

	gtk_widget_destroy (wpassdlg);
	g_free (pdialog);
}
