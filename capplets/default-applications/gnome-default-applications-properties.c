/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
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
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <math.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"


#define DEFAULT_APPS_KEY_BROWSER_PATH "/desktop/gnome/url-handlers/http"
#define DEFAULT_APPS_KEY_BROWSER_NEEDS_TERM DEFAULT_APPS_KEY_BROWSER_PATH"/needs_terminal"
#define DEFAULT_APPS_KEY_BROWSER_EXEC       DEFAULT_APPS_KEY_BROWSER_PATH"/command"

#define DEFAULT_APPS_KEY_MAILER_PATH "/desktop/gnome/url-handlers/mailto"
#define DEFAULT_APPS_KEY_MAILER_NEEDS_TERM DEFAULT_APPS_KEY_MAILER_PATH"/needs_terminal"
#define DEFAULT_APPS_KEY_MAILER_EXEC       DEFAULT_APPS_KEY_MAILER_PATH"/command"

#define DEFAULT_APPS_KEY_TERMINAL_PATH "/desktop/gnome/applications/terminal"
#define DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG DEFAULT_APPS_KEY_TERMINAL_PATH"/exec_arg"
#define DEFAULT_APPS_KEY_TERMINAL_EXEC DEFAULT_APPS_KEY_TERMINAL_PATH"/exec"

#define MIME_APPLICATION_ID "gnome-default-applications-editor"

typedef struct _BrowserDescription BrowserDescription;
typedef struct _MailerDescription MailerDescription;
typedef struct _TerminalDesciption TerminalDescription;

/* All defined below */
#include "gnome-default-applications-properties-structs.c"

static GList *text_editors = NULL;

static GConfClient *client = NULL;

static void
on_text_custom_properties_clicked (GtkWidget *w, GladeXML *dialog)
{
	GtkWidget *d;
	int res;
	GnomeVFSMimeApplication *mime_app;
	const char *command, *name;

	d = WID ("custom_editor_dialog");
	gtk_window_set_transient_for (GTK_WINDOW (d), GTK_WINDOW (WID ("default_applications_dialog")));

	mime_app = gnome_vfs_application_registry_get_mime_application (MIME_APPLICATION_ID);

	gtk_entry_set_text (GTK_ENTRY (WID ("text_custom_name_entry")),
			    mime_app ? _(mime_app->name) : "");
	gtk_entry_set_text (GTK_ENTRY (WID ("text_custom_command_entry")),
			    mime_app ? mime_app->command : "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_multi_toggle")),
				      mime_app ? mime_app->can_open_multiple_files : FALSE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_terminal_toggle")),
				      mime_app ? mime_app->requires_terminal : FALSE);

 run_properties_dialog:
	res = gtk_dialog_run (GTK_DIALOG (d));

	if (res != GTK_RESPONSE_OK) {
		gtk_widget_hide (d);
		gnome_vfs_mime_application_free (mime_app);
		return;
	}

	name    = gtk_entry_get_text (GTK_ENTRY (WID ("text_custom_name_entry")));
	command = gtk_entry_get_text (GTK_ENTRY (WID ("text_custom_command_entry")));

	if (!*name || !*command) {
		GtkWidget *d2;
		d2 = gtk_message_dialog_new (GTK_WINDOW (d),
					     0,
					     GTK_MESSAGE_ERROR,
					     GTK_BUTTONS_OK,
					     _("Please specify a name and a command for this editor."));
		gtk_dialog_run (GTK_DIALOG (d2));
		gtk_widget_destroy (d2);
		goto run_properties_dialog;
	}

	gtk_widget_hide (d);

	if (mime_app) {
		g_free (mime_app->name);
		g_free (mime_app->command);
	} else {
		mime_app = g_new0 (GnomeVFSMimeApplication, 1);
		mime_app->id = g_strdup (MIME_APPLICATION_ID);
	}

	mime_app->name    = g_strdup (name);
	mime_app->command = g_strdup (command);

	mime_app->can_open_multiple_files = GTK_TOGGLE_BUTTON (WID ("text_custom_multi_toggle"))->active;
	mime_app->requires_terminal = GTK_TOGGLE_BUTTON (WID ("text_custom_terminal_toggle"))->active;
	mime_app->expects_uris = GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_PATHS;

#if 0
	GTK_TOGGLE_BUTTON (WID ("text_custom_uri_toggle"))->active
		? GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_URIS_FOR_NON_FILES
		: GNOME_VFS_MIME_APPLICATION_ARGUMENT_TYPE_PATHS;
#endif

	gnome_vfs_application_registry_save_mime_application (mime_app);

	gnome_vfs_mime_set_default_application ("text/plain", mime_app->id);
	gnome_vfs_mime_application_free (mime_app);

	gnome_vfs_application_registry_sync ();
}

static void
on_text_default_viewer_toggle (GtkWidget *toggle, GladeXML *dialog)
{
	GnomeVFSMimeActionType old_action_type, new_action_type;

	old_action_type = gnome_vfs_mime_get_default_action_type ("text/plain");
	new_action_type = GTK_TOGGLE_BUTTON (toggle)->active
		? GNOME_VFS_MIME_ACTION_TYPE_APPLICATION
		: GNOME_VFS_MIME_ACTION_TYPE_COMPONENT;

	if (new_action_type == old_action_type)
		return;

	gnome_vfs_mime_set_default_action_type ("text/plain", new_action_type);
	gnome_vfs_application_registry_sync ();
}

static void
generic_guard (GtkWidget *toggle, GtkWidget *widget)
{

	gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (toggle), FALSE);
	gtk_widget_set_sensitive (widget, GTK_TOGGLE_BUTTON (toggle)->active);

	if (GTK_TOGGLE_BUTTON (toggle)->active) {
		GtkWidget *e;
		/* Get entry associated with us */
		e = g_object_get_data (G_OBJECT (toggle), "entry");
		if (e && GTK_WIDGET_REALIZED (e)) gtk_widget_grab_focus (e);
		if (e && GTK_IS_ENTRY (e)) {
			gchar *text;
			text = g_strdup (gtk_entry_get_text (GTK_ENTRY (e)));
			/* fixme: This is not nice, but it is the only way to force combo to emit 'changed' */
			gtk_entry_set_text (GTK_ENTRY (e), "");
			gtk_entry_set_text (GTK_ENTRY (e), text);
			g_free (text);
		}
	}
}

static gboolean
validate (GHashTable *unique, char const *name, char const *exec_name)
{
	if (NULL == g_hash_table_lookup (unique, name)) {
		char *path = g_find_program_in_path (exec_name);
		if (path != NULL) {
			g_hash_table_insert (unique, (gpointer)name, (gpointer)exec_name);
			g_free (path);
			return TRUE;
		}
	}
	return FALSE;
}

static void
initialize_default_applications (void)
{
        gint i;
	GHashTable *unique;

	text_editors = gnome_vfs_mime_get_all_applications ("text/plain");

	unique = g_hash_table_new (g_str_hash, g_str_equal);
        for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ )
		possible_browsers[i].in_path = validate (unique,
			possible_browsers[i].name, possible_browsers[i].executable_name);
	g_hash_table_destroy (unique);

	unique = g_hash_table_new (g_str_hash, g_str_equal);
        for (i = 0; i < G_N_ELEMENTS (possible_mailers); i++ )
		possible_mailers[i].in_path = validate (unique,
			possible_mailers[i].name, possible_mailers[i].executable_name);
	g_hash_table_destroy (unique);

	unique = g_hash_table_new (g_str_hash, g_str_equal);
        for (i = 0; i < G_N_ELEMENTS (possible_terminals); i++ )
		possible_terminals[i].in_path = validate (unique,
			possible_terminals[i].name, possible_terminals[i].exec);
	g_hash_table_destroy (unique);
}

static void
update_editor_sensitivity (GladeXML *dialog)
{
	gboolean predefined = GTK_TOGGLE_BUTTON (WID ("text_select_radio"))->active;

	gtk_widget_set_sensitive (WID ("text_select_combo"), predefined);
	gtk_widget_set_sensitive (WID ("text_custom_hbox"), !predefined);
}

static void
read_editor (GConfClient *client,
	     GladeXML    *dialog)
{
	GnomeVFSMimeApplication *mime_app;
	GList *li;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_default_viewer_toggle")),
				      gnome_vfs_mime_get_default_action_type ("text/plain") == GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);

	mime_app = gnome_vfs_mime_get_default_application ("text/plain");

	if (mime_app == NULL || !strcmp (mime_app->id, MIME_APPLICATION_ID))
		goto read_editor_custom;

	for (li = text_editors; li; li = li->next) {
		GnomeVFSMimeApplication *li_app = li->data;

		if (strcmp (mime_app->command, li_app->command) == 0 &&
		    mime_app->requires_terminal == li_app->requires_terminal) {
			gtk_entry_set_text (GTK_ENTRY (WID ("text_select_combo_entry")), _(mime_app->name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_select_radio")), TRUE);
			gnome_vfs_mime_application_free (mime_app);
			return;
		}
        }

	/*
	 * the default editor wasn't set by us, and it wasn't in the
	 * list.
	 */

	g_free (mime_app->id);
	mime_app->id = g_strdup (MIME_APPLICATION_ID);

	gnome_vfs_application_registry_save_mime_application (mime_app);

	gnome_vfs_mime_set_default_application ("text/plain", mime_app->id);
	gnome_vfs_application_registry_sync ();

 read_editor_custom:
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_select_radio")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_radio")), TRUE);
	if (mime_app)
		gnome_vfs_mime_application_free (mime_app);
}

static void
text_apply_editor (GtkWidget *entry,
		   GladeXML  *dialog)
{
	GList *li;
	GnomeVFSMimeApplication *mime_app;
	const gchar *editor;

	if (!GTK_TOGGLE_BUTTON (WID ("text_select_radio"))->active)
		return;

	update_editor_sensitivity (dialog);

	editor = gtk_entry_get_text (GTK_ENTRY (WID ("text_select_combo_entry")));

	/* don't do anything if it was cleared. */
	if (!*editor)
		return;

	for (li = text_editors; li; li = li->next) {
		mime_app = li->data;
		if (! strcmp (mime_app->name, editor)) {
			gnome_vfs_mime_set_default_application ("text/plain", mime_app->id);
			gnome_vfs_application_registry_sync ();
			return;
		}
	}

	g_assert_not_reached ();
}

static void
text_apply_custom (GtkWidget *entry,
		   GladeXML *dialog)
{
	GnomeVFSMimeApplication *mime_app;

	if (!GTK_TOGGLE_BUTTON (WID ("text_custom_radio"))->active)
		return;

	mime_app = gnome_vfs_application_registry_get_mime_application (MIME_APPLICATION_ID);
	if (!mime_app) {
		on_text_custom_properties_clicked (entry, dialog);
		mime_app = gnome_vfs_application_registry_get_mime_application (MIME_APPLICATION_ID);
		if (!mime_app) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_select_radio")), TRUE);
			return;
		}
	} else {
		gnome_vfs_mime_set_default_application ("text/plain", mime_app->id);
		gnome_vfs_mime_application_free (mime_app);

		gnome_vfs_application_registry_sync ();
	}

	update_editor_sensitivity (dialog);
}

static void
setup_peditors (GConfClient *client,
		GladeXML    *dialog)
{
        GConfChangeSet *changeset = NULL;

	gconf_peditor_new_boolean (changeset, DEFAULT_APPS_KEY_BROWSER_NEEDS_TERM,
				   WID ("web_custom_terminal_toggle"), NULL);
	gconf_peditor_new_string  (changeset, DEFAULT_APPS_KEY_BROWSER_EXEC,
				   WID ("web_custom_command_entry"), NULL);

	gconf_peditor_new_boolean (changeset, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM,
				   WID ("mail_custom_terminal_toggle"), NULL);
	gconf_peditor_new_string  (changeset, DEFAULT_APPS_KEY_MAILER_EXEC,
				   WID ("mail_custom_command_entry"), NULL);

	gconf_peditor_new_string  (changeset, DEFAULT_APPS_KEY_TERMINAL_EXEC,
				   WID ("terminal_custom_command_entry"), NULL);
	gconf_peditor_new_string  (changeset, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG,
				   WID ("terminal_custom_exec_entry"), NULL);
}

static void
read_browser (GConfClient *client,
	      GladeXML    *dialog)
{
	GError *error = NULL;
	gchar *browser;
	gboolean needs_term;
	gint i;

	needs_term = gconf_client_get_bool (client, DEFAULT_APPS_KEY_BROWSER_NEEDS_TERM, &error);
	if (error) {
		/* hp will shoot me -- I'll do this later. */
		return;
	}
	browser = gconf_client_get_string (client, DEFAULT_APPS_KEY_BROWSER_EXEC, &error);
	if (error) {
		return;
	}

	for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ ) {
		if (possible_browsers[i].in_path == FALSE)
			continue;

		if (browser && strcmp (browser, possible_browsers[i].command) == 0 &&
		    needs_term == possible_browsers[i].needs_term) {
			gtk_entry_set_text (GTK_ENTRY (WID ("web_select_combo_entry")),
					    _(possible_browsers[i].name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_select_radio")), TRUE);
			g_free (browser);
			return;
		}
	}
	if (strlen(browser) != 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_select_radio")), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_radio")), TRUE);
	} else {
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (WID ("web_select_radio")), TRUE);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (WID ("web_custom_radio")), TRUE);
	}
	
	g_free (browser);
}

static void
read_mailer (GConfClient *client,
	     GladeXML    *dialog)
{
	GError *error = NULL;
	gchar *mailer;
	gboolean needs_term;
	gint i;

	needs_term = gconf_client_get_bool (client, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM, &error);
	if (error) {
		/* hp will shoot me -- I'll do this later. */
		return;
	}
	mailer = gconf_client_get_string (client, DEFAULT_APPS_KEY_MAILER_EXEC, &error);
	if (error) {
		return;
	}

	for (i = 0; i < G_N_ELEMENTS (possible_mailers); i++ ) {
		if (possible_mailers[i].in_path == FALSE)
			continue;

		if (mailer && strcmp (mailer, possible_mailers[i].command) == 0 &&
		    needs_term == possible_mailers[i].needs_term) {
			gtk_entry_set_text (GTK_ENTRY (WID ("mail_select_combo_entry")),
					    _(possible_mailers[i].name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("mail_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("mail_select_radio")), TRUE);
			g_free (mailer);
			return;
		}
        }
		
	if (strlen(mailer) != 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("mail_select_radio")), TRUE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("mail_custom_radio")), TRUE);
	} else {
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (WID ("mail_select_radio")), TRUE);
		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (WID ("mail_custom_radio")), TRUE);
	}
	g_free (mailer);


}

static void
browser_setup_custom (GtkWidget *entry,
		      GladeXML  *dialog)
{
	gint i;
	const gchar *browser = gtk_entry_get_text (GTK_ENTRY (entry));

	for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ ) {
		if (! strcmp (_(possible_browsers[i].name), browser)) {
		        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_terminal_toggle")),
						      possible_browsers[i].needs_term);
			gtk_entry_set_text (GTK_ENTRY (WID ("web_custom_command_entry")),
					    possible_browsers[i].command);
			return;
		}
	}
}

static void
mailer_setup_custom (GtkWidget *entry,
		     GladeXML  *dialog)
{
	gint i;
	const gchar *mailer = gtk_entry_get_text (GTK_ENTRY (entry));

	for (i = 0; i < G_N_ELEMENTS (possible_mailers); i++ ) {
		if (! strcmp (_(possible_mailers[i].name), mailer)) {
		        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("mail_custom_terminal_toggle")),
						      possible_mailers[i].needs_term);
			gtk_entry_set_text (GTK_ENTRY (WID ("mail_custom_command_entry")),
					    possible_mailers[i].command);
			return;
		}
	}
}


static void
read_terminal (GConfClient *client,
	       GladeXML    *dialog)
{
	GError *error = NULL;
	gchar *exec;
	gchar *exec_arg;
	gint i;

	exec = gconf_client_get_string (client, DEFAULT_APPS_KEY_TERMINAL_EXEC, &error);
	if (error) {
		return;
	}
	exec_arg = gconf_client_get_string (client, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG, &error);
	if (error) {
		exec_arg = NULL;
	}

	gtk_entry_set_text (GTK_ENTRY (WID ("terminal_custom_command_entry")), exec?exec:"");
	gtk_entry_set_text (GTK_ENTRY (WID ("terminal_custom_exec_entry")), exec_arg?exec_arg:"");

	for (i = 0; i < G_N_ELEMENTS (possible_terminals); i++ ) {
		if (possible_terminals[i].in_path == FALSE)
			continue;

		if (strcmp (exec?exec:"", possible_terminals[i].exec) == 0 &&
		    strcmp (exec_arg?exec_arg:"", possible_terminals[i].exec_arg) == 0) {
			gtk_entry_set_text (GTK_ENTRY (WID ("terminal_select_combo_entry")),
					    _(possible_terminals[i].name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("terminal_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("terminal_select_radio")), TRUE);
			g_free (exec);
			g_free (exec_arg);
			return;
		}
        }
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("terminal_select_radio")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("terminal_custom_radio")), TRUE);
	g_free (exec);
	g_free (exec_arg);
}

static void
terminal_setup_custom (GtkWidget *entry,
		       GladeXML  *dialog)
{
	gint i;
	const gchar *terminal = gtk_entry_get_text (GTK_ENTRY (entry));

	for (i = 0; i < G_N_ELEMENTS (possible_terminals); i++ ) {
		if (! strcmp (_(possible_terminals[i].name), terminal)) {
			gtk_entry_set_text (GTK_ENTRY (WID ("terminal_custom_command_entry")), possible_terminals[i].exec);
			gtk_entry_set_text (GTK_ENTRY (WID ("terminal_custom_exec_entry")), possible_terminals[i].exec_arg);
			return;
		}
	}
}

static void
value_changed_cb (GConfClient *client,
		  const gchar *key,
		  GConfValue  *value,
		  GladeXML    *dialog)
{
	g_return_if_fail (key != NULL);
	if (strncmp (key, DEFAULT_APPS_KEY_MAILER_PATH, strlen (DEFAULT_APPS_KEY_MAILER_PATH)) == 0) {
		gconf_client_set_bool (client, DEFAULT_APPS_KEY_MAILER_PATH"/enabled", TRUE, NULL);
	} else if (strncmp (key, DEFAULT_APPS_KEY_BROWSER_PATH, strlen (DEFAULT_APPS_KEY_BROWSER_PATH)) == 0) {
	} else if (strncmp (key, DEFAULT_APPS_KEY_TERMINAL_PATH, strlen (DEFAULT_APPS_KEY_TERMINAL_PATH)) == 0) {
	}
}

static void
dialog_response (GtkDialog *widget,
		 gint       response_id,
		 GladeXML  *dialog)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (widget), "user-guide.xml", "goscustdoc-2");
	else
		gtk_main_quit ();
}

static GladeXML *
create_dialog (GConfClient *client)
{
	GladeXML *dialog;
	GList *strings = NULL, *li;
	gint i;
	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-default-applications-properties.glade", NULL, NULL);
	capplet_set_icon (WID ("default_applications_dialog"), "gnome-settings-default-applications");

	setup_peditors (client, dialog);

	/* Editors page */
	for (li = text_editors; li; li = li->next) {
		strings = g_list_append (strings, _(((GnomeVFSMimeApplication *)li->data)->name));
	}
	if (strings) {
		/* We have default editors */
		gtk_combo_set_popdown_strings (GTK_COMBO(WID ("text_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	} else {
		/* No default editors */
		gtk_widget_set_sensitive (WID ("text_select_radio"), FALSE);
	}

	read_editor (client, dialog);
	update_editor_sensitivity (dialog);

	g_signal_connect (G_OBJECT (WID ("text_select_combo_entry")),
			  "changed", G_CALLBACK (text_apply_editor),
			  dialog);
	g_signal_connect (WID ("text_custom_properties"), "clicked",
			  G_CALLBACK (on_text_custom_properties_clicked),
			  dialog);
	g_signal_connect (WID ("text_default_viewer_toggle"), "toggled",
			  G_CALLBACK (on_text_default_viewer_toggle),
			  dialog);
	g_signal_connect_after (G_OBJECT (WID ("text_select_radio")), "toggled",
				G_CALLBACK (text_apply_editor), dialog);
	g_signal_connect_after (G_OBJECT (WID ("text_custom_radio")), "toggled",
				G_CALLBACK (text_apply_custom), dialog);

	/* Web browsers page */
	for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ ) {
		if (possible_browsers[i].in_path)
			strings = g_list_append (strings, _(possible_browsers[i].name));
	}
	if (strings) {
		/* We have default browsers */
		gtk_combo_set_popdown_strings (GTK_COMBO(WID ("web_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	} else {
		/* No default browsers */
		gtk_widget_set_sensitive (WID ("web_select_radio"), FALSE);
	}

	/* Source of command string */
	g_object_set_data (G_OBJECT (WID ("web_select_radio")), "entry", WID ("web_select_combo_entry"));
	/* Source of command string */
	g_object_set_data (G_OBJECT (WID ("web_custom_radio")), "entry", WID ("web_custom_command_entry"));

	g_signal_connect (G_OBJECT (WID ("web_select_combo_entry")),
			  "changed", (GCallback) browser_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("web_select_radio")),
			  "clicked", (GCallback) generic_guard,
			  WID ("web_select_combo"));
	g_signal_connect (G_OBJECT (WID ("web_custom_radio")),
			  "clicked", (GCallback) generic_guard,
			  WID ("web_custom_vbox"));

	read_browser (client, dialog);


	/* Mail readers page */
	for (i = 0; i < G_N_ELEMENTS (possible_mailers); i++ ) {
		if (possible_mailers[i].in_path)
			strings = g_list_append (strings, _(possible_mailers[i].name));
	}
	if (strings) {
		/* We have default browsers */
		gtk_combo_set_popdown_strings (GTK_COMBO(WID ("mail_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	} else {
		/* No default browsers */
		gtk_widget_set_sensitive (WID ("mail_select_radio"), FALSE);
	}

	/* Source of command string */
	g_object_set_data (G_OBJECT (WID ("mail_select_radio")), "entry", WID ("mail_select_combo_entry"));
	/* Source of command string */
	g_object_set_data (G_OBJECT (WID ("mail_custom_radio")), "entry", WID ("mail_custom_command_entry"));

	g_signal_connect (G_OBJECT (WID ("mail_select_combo_entry")),
			  "changed", (GCallback) mailer_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("mail_select_radio")),
			  "clicked", (GCallback) generic_guard,
			  WID ("mail_select_combo"));
	g_signal_connect (G_OBJECT (WID ("mail_custom_radio")),
			  "clicked", (GCallback) generic_guard,
			  WID ("mail_custom_vbox"));

	read_mailer (client, dialog);


	/* Terminal */

	for (i = 0; i < G_N_ELEMENTS (possible_terminals); i++ ) {
		if (possible_terminals[i].in_path)
			strings = g_list_append (strings, _(possible_terminals[i].name));
	}
	if (strings) {
		/* We have default terminals */
		gtk_combo_set_popdown_strings (GTK_COMBO (WID ("terminal_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	} else {
		/* No default terminals */
		gtk_widget_set_sensitive (WID ("terminal_select_radio"), FALSE);
	}

	/* Source of command string */
	g_object_set_data (G_OBJECT (WID ("terminal_select_radio")), "entry", WID ("terminal_select_combo_entry"));
	/* Source of command string */
	g_object_set_data (G_OBJECT (WID ("terminal_custom_radio")), "entry", WID ("terminal_custom_command_entry"));

	g_signal_connect (G_OBJECT (WID ("terminal_select_combo_entry")),
			  "changed", (GCallback) terminal_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("terminal_select_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("terminal_select_combo"));
	g_signal_connect (G_OBJECT (WID ("terminal_custom_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("terminal_custom_table"));
	read_terminal (client, dialog);


	g_signal_connect (G_OBJECT (client), "value-changed", (GCallback) value_changed_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("default_applications_dialog")), "response", (GCallback) dialog_response, dialog);

	gtk_widget_show (WID ("default_applications_dialog"));

	return dialog;
}

static void
get_legacy_settings (void)
{
}

int
main (int argc, char **argv)
{
	GladeXML       *dialog;

	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-default-applications-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	client = gconf_client_get_default ();

	gconf_client_add_dir (client, "/desktop/gnome/applications/browser", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/desktop/gnome/applications/terminal", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		initialize_default_applications ();
		dialog = create_dialog (client);
		gtk_main ();
	}

	return 0;
}
