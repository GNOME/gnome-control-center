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

#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <math.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"


typedef struct _BrowserDescription BrowserDescription;
typedef struct _EditorDescription EditorDescription;
typedef struct _HelpViewDescription HelpViewDescription;
typedef struct _TerminalDesciption TerminalDescription;
/* All defined below */
#include "default-application-structs.c"

static GConfClient *client = NULL;

static void
generic_guard (GtkWidget *toggle,
	       GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget, GTK_TOGGLE_BUTTON (toggle)->active);
}

static gboolean
mnemonic_activate (GtkWidget *toggle,
		   gboolean   group_cycling,
		   GtkWidget *widget)
{
	if (! group_cycling) {
		gtk_widget_grab_focus (widget);
		if (GTK_IS_ENTRY (widget)) {
			/* sorta evil hack, but it triggers a callback and is pretty harmless */
			gchar *text = g_strdup (gtk_entry_get_text (GTK_ENTRY (widget)));
			gtk_entry_set_text (GTK_ENTRY (widget), text);
			g_free (text);
		}
	}
	return FALSE;
}

static void
initialize_default_applications (void)
{
        gint i;

        for (i = 0; i < G_N_ELEMENTS (possible_editors); i++ ) {
                if (gnome_is_program_in_path (possible_editors[i].executable_name))
			possible_editors[i].in_path = TRUE;
        }
        for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ ) {
                if (gnome_is_program_in_path (possible_browsers[i].executable_name))
			possible_browsers[i].in_path = TRUE;
        }
        for (i = 0; i < G_N_ELEMENTS (possible_help_viewers); i++ ) {
                if (gnome_is_program_in_path (possible_help_viewers[i].executable_name))
			possible_help_viewers[i].in_path = TRUE;
        }
        for (i = 0; i < G_N_ELEMENTS (possible_terminals); i++ ) {
                if (gnome_is_program_in_path (possible_terminals[i].exec))
			possible_terminals[i].in_path = TRUE;
        }
}

static void
read_editor (GConfClient *client,
	     GladeXML    *dialog)
{
	GError *error = NULL;
	gchar *editor;
	gboolean needs_term;
	gboolean accepts_lineno;
	gint i;

	needs_term = gconf_client_get_bool (client, "/desktop/gnome/applications/editor/needs_term", &error);
	if (error) {
		/* hp will shoot me -- I'll do this later. */
		return;
	}
	accepts_lineno = gconf_client_get_bool (client, "/desktop/gnome/applications/editor/accepts_lineno", &error);
	if (error) {
		return;
	}
	editor = gconf_client_get_string (client, "/desktop/gnome/applications/editor/exec",&error);
	if (error) {
		return;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_terminal_toggle")), needs_term);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_line_toggle")), accepts_lineno);
	gtk_entry_set_text (GTK_ENTRY (WID ("text_custom_command_entry")), editor);

	for (i = 0; i < G_N_ELEMENTS (possible_editors); i++ ) {
		if (possible_editors[i].in_path == FALSE)
			continue;
		
		if (strcmp (editor, possible_editors[i].executable_name) == 0 &&
		    needs_term == possible_editors[i].needs_term &&
		    accepts_lineno == possible_editors[i].accepts_lineno) {
			gtk_entry_set_text (GTK_ENTRY (WID ("text_select_combo_entry")),
					    _(possible_editors[i].name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_select_radio")), TRUE);
			g_free (editor);
			return;
		}
        }
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_select_radio")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_radio")), TRUE);
	g_free (editor);
}

static void
write_editor (GladeXML *dialog)
{
	const gchar *command = gtk_entry_get_text (GTK_ENTRY (WID ("text_custom_command_entry")));
	gboolean needs_term = GTK_TOGGLE_BUTTON (WID ("text_custom_terminal_toggle"))->active;
	gboolean accepts_lineno = GTK_TOGGLE_BUTTON (WID ("text_custom_line_toggle"))->active;
		 
	gconf_client_set_bool (client, "/desktop/gnome/applications/editor/needs_term",
			       needs_term, NULL);
	gconf_client_set_bool (client, "/desktop/gnome/applications/editor/accepts_lineno",
			       accepts_lineno, NULL);
	gconf_client_set_string (client, "/desktop/gnome/applications/editor/exec",
				 command, NULL);
}

static void
text_setup_custom (GtkWidget *entry,
		   GladeXML  *dialog)
{
	gint i;
	const gchar *editor = gtk_entry_get_text (GTK_ENTRY (entry));

	for (i = 0; i < G_N_ELEMENTS (possible_editors); i++ ) {
		if (! strcmp (_(possible_editors[i].name), editor)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_terminal_toggle")),
						      possible_editors[i].needs_term);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("text_custom_line_toggle")),
						      possible_editors[i].accepts_lineno);
			gtk_entry_set_text (GTK_ENTRY (WID ("text_custom_command_entry")),
					    possible_editors[i].executable_name);
			return;
		}
	}
}


static void
read_browser (GConfClient *client,
	      GladeXML    *dialog)
{
	GError *error = NULL;
	gchar *browser;
	gboolean needs_term;
	gboolean nremote;
	gint i;

	needs_term = gconf_client_get_bool (client, "/desktop/gnome/applications/browser/needs_term", &error);
	if (error) {
		/* hp will shoot me -- I'll do this later. */
		return;
	}
	nremote = gconf_client_get_bool (client, "/desktop/gnome/applications/browser/nremote", &error);
	if (error) {
		return;
	}
	browser = gconf_client_get_string (client, "/desktop/gnome/applications/browser/exec",&error);
	if (error) {
		return;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_terminal_toggle")), needs_term);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_remote_toggle")), nremote);
	gtk_entry_set_text (GTK_ENTRY (WID ("web_custom_command_entry")), browser);

	for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ ) {
		if (possible_browsers[i].in_path == FALSE)
			continue;
		
		if (strcmp (browser, possible_browsers[i].executable_name) == 0 &&
		    needs_term == possible_browsers[i].needs_term &&
		    nremote == possible_browsers[i].nremote) {
			gtk_entry_set_text (GTK_ENTRY (WID ("web_select_combo_entry")),
					    _(possible_browsers[i].name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_select_radio")), TRUE);
			g_free (browser);
			return;
		}
        }
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_select_radio")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_radio")), TRUE);
	g_free (browser);

	
}

static void
write_browser (GladeXML *dialog)
{
	gconf_client_set_bool (client, "/desktop/gnome/applications/browser/needs_term",
			       GTK_TOGGLE_BUTTON (WID ("web_custom_terminal_toggle"))->active, NULL);
	gconf_client_set_bool (client, "/desktop/gnome/applications/browser/nremote",
			       GTK_TOGGLE_BUTTON (WID ("web_custom_remote_toggle"))->active, NULL);
	gconf_client_set_string (client, "/desktop/gnome/applications/browser/exec",
				 gtk_entry_get_text (GTK_ENTRY (WID ("web_custom_command_entry"))), NULL);
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
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("web_custom_remote_toggle")),
						      possible_browsers[i].nremote);
			gtk_entry_set_text (GTK_ENTRY (WID ("web_custom_command_entry")),
					    possible_browsers[i].executable_name);
			return;
		}
	}
}


static void
read_help_viewer (GConfClient *client,
	     GladeXML    *dialog)
{
	GError *error = NULL;
	gchar *help_viewer;
	gboolean needs_term;
	gboolean accepts_lineno;
	gint i;

	needs_term = gconf_client_get_bool (client, "/desktop/gnome/applications/help_viewer/needs_term", &error);
	if (error) {
		/* hp will shoot me -- I'll do this later. */
		return;
	}
	accepts_lineno = gconf_client_get_bool (client, "/desktop/gnome/applications/help_viewer/accepts_lineno", &error);
	if (error) {
		return;
	}
	help_viewer = gconf_client_get_string (client, "/desktop/gnome/applications/help_viewer/exec",&error);
	if (error) {
		return;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_custom_terminal_toggle")), needs_term);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_custom_url_toggle")), accepts_lineno);
	gtk_entry_set_text (GTK_ENTRY (WID ("help_custom_command_entry")), help_viewer);

	for (i = 0; i < G_N_ELEMENTS (possible_help_viewers); i++ ) {
		if (possible_help_viewers[i].in_path == FALSE)
			continue;
		
		if (strcmp (help_viewer, possible_help_viewers[i].executable_name) == 0 &&
		    needs_term == possible_help_viewers[i].needs_term &&
		    accepts_lineno == possible_help_viewers[i].accepts_urls) {
			gtk_entry_set_text (GTK_ENTRY (WID ("help_select_combo_entry")),
					    _(possible_help_viewers[i].name));
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_custom_radio")), TRUE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_select_radio")), TRUE);
			g_free (help_viewer);
			return;
		}
        }
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_select_radio")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_custom_radio")), TRUE);
	g_free (help_viewer);
}

static void
write_help_viewer (GladeXML *dialog)
{
	gconf_client_set_bool (client, "/desktop/gnome/applications/help_viewer/needs_term",
			       GTK_TOGGLE_BUTTON (WID ("help_custom_terminal_toggle"))->active, NULL);
	gconf_client_set_bool (client, "/desktop/gnome/applications/help_viewer/accepts_lineno",
			       GTK_TOGGLE_BUTTON (WID ("help_custom_url_toggle"))->active, NULL);
	gconf_client_set_string (client, "/desktop/gnome/applications/help_viewer/exec",
				 gtk_entry_get_text (GTK_ENTRY (WID ("help_custom_command_entry"))), NULL);
}

static void
help_setup_custom (GtkWidget *entry,
		   GladeXML  *dialog)
{
	gint i;
	const gchar *help_viewer = gtk_entry_get_text (GTK_ENTRY (entry));

	for (i = 0; i < G_N_ELEMENTS (possible_help_viewers); i++ ) {
		if (! strcmp (_(possible_help_viewers[i].name), help_viewer)) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_custom_terminal_toggle")),
						      possible_help_viewers[i].needs_term);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("help_custom_url_toggle")),
						      possible_help_viewers[i].accepts_urls);
			gtk_entry_set_text (GTK_ENTRY (WID ("help_custom_command_entry")),
					    possible_help_viewers[i].executable_name);
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

	exec = gconf_client_get_string (client, "/desktop/gnome/applications/terminal/exec",&error);
	if (error) {
		return;
	}
	exec_arg = gconf_client_get_string (client, "/desktop/gnome/applications/terminal/exec_arg",&error);
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
			return;
		}
        }
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("terminal_select_radio")), TRUE);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (WID ("terminal_custom_radio")), TRUE);
	g_free (exec);
	g_free (exec_arg);
}

static void
write_terminal (GladeXML *dialog)
{
	gconf_client_set_string (client, "/desktop/gnome/applications/terminal/exec",
				 gtk_entry_get_text (GTK_ENTRY (WID ("terminal_custom_command_entry"))), NULL);
	gconf_client_set_string (client, "/desktop/gnome/applications/terminal/exec_arg",
				 gtk_entry_get_text (GTK_ENTRY (WID ("terminal_custom_exec_entry"))), NULL);
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

	if (strncmp (key, "/desktop/gnome/applications/editor", strlen ("/desktop/gnome/applications/editor")) == 0) {
		read_editor (client, dialog);
	} else if (strncmp (key, "/desktop/gnome/applications/browser", strlen ("/desktop/gnome/applications/browser")) == 0) {
		read_browser (client, dialog);
	} else if (strncmp (key, "/desktop/gnome/applications/help_viewer", strlen ("/desktop/gnome/applications/help_viewer")) == 0) {
		read_help_viewer (client, dialog);
	} else if (strncmp (key, "/desktop/gnome/applications/terminal", strlen ("/desktop/gnome/applications/terminal")) == 0) {
		read_terminal (client, dialog);
	}
}

static void
dialog_response (GtkDialog *widget,
		 gint       response_id,
		 GladeXML  *dialog)
{

	switch (response_id) {
	case 0: /* Help */
		break;
	case 1: /* Apply */
		write_editor (dialog);
		write_browser (dialog);
		write_help_viewer (dialog);
		write_terminal (dialog);
		
		break;
	case 2: /* OK */
		write_editor (dialog);
		write_browser (dialog);
		write_help_viewer (dialog);
		write_terminal (dialog);

		gtk_main_quit ();
		break;
	case 3:  /* Close */
	case -4: /* keyboard esc or WM close */
		gtk_main_quit ();
		break;
	default:
		g_assert_not_reached ();
	};
}



static GladeXML *
create_dialog (GConfClient *client)
{
	GladeXML *dialog;
	GList *strings = NULL;
	gint i;
	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-default-applications-properties.glade",
				"default_applications_dialog",
				NULL);
	

	/* Text page */
	for (i = 0; i < G_N_ELEMENTS (possible_editors); i++ ) {
		if (possible_editors[i].in_path)
			strings = g_list_append (strings, _(possible_editors[i].name));
	}
	if (strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO(WID ("text_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	}


	g_signal_connect (G_OBJECT (WID ("text_select_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("text_select_combo_entry"));
	g_signal_connect (G_OBJECT (WID ("text_select_combo_entry")),
			  "changed", (GCallback) text_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("text_select_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("text_select_combo"));
	g_signal_connect (G_OBJECT (WID ("text_custom_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("text_custom_command_entry"));
	g_signal_connect (G_OBJECT (WID ("text_custom_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("text_custom_vbox"));
	read_editor (client, dialog);

	/* Web page */
	for (i = 0; i < G_N_ELEMENTS (possible_browsers); i++ ) {
		if (possible_browsers[i].in_path)
			strings = g_list_append (strings, _(possible_browsers[i].name));
	}
	if (strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO(WID ("web_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	}


	g_signal_connect (G_OBJECT (WID ("web_select_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("web_select_combo_entry"));
	g_signal_connect (G_OBJECT (WID ("web_select_combo_entry")),
			  "changed", (GCallback) browser_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("web_select_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("web_select_combo"));
	g_signal_connect (G_OBJECT (WID ("web_custom_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("web_custom_command_entry"));
	g_signal_connect (G_OBJECT (WID ("web_custom_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("web_custom_vbox"));
	read_browser (client, dialog);

	/* Help page */
	
	for (i = 0; i < G_N_ELEMENTS (possible_help_viewers); i++ ) {
		if (possible_help_viewers[i].in_path)
			strings = g_list_append (strings, _(possible_help_viewers[i].name));
	}
	if (strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO(WID ("help_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	}


	g_signal_connect (G_OBJECT (WID ("help_select_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("help_select_combo_entry"));
	g_signal_connect (G_OBJECT (WID ("help_select_combo_entry")),
			  "changed", (GCallback) help_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("help_select_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("help_select_combo"));
	g_signal_connect (G_OBJECT (WID ("help_custom_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("help_custom_command_entry"));
	g_signal_connect (G_OBJECT (WID ("help_custom_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("help_custom_vbox"));
	read_help_viewer (client, dialog);	


	/* Terminal */
	
	for (i = 0; i < G_N_ELEMENTS (possible_terminals); i++ ) {
		if (possible_terminals[i].in_path)
			strings = g_list_append (strings, _(possible_terminals[i].name));
	}
	if (strings) {
		gtk_combo_set_popdown_strings (GTK_COMBO (WID ("terminal_select_combo")), strings);
		g_list_free (strings);
		strings = NULL;
	}


	g_signal_connect (G_OBJECT (WID ("terminal_select_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("terminal_select_combo_entry"));
	g_signal_connect (G_OBJECT (WID ("terminal_select_combo_entry")),
			  "changed", (GCallback) terminal_setup_custom,
			  dialog);
	g_signal_connect (G_OBJECT (WID ("terminal_select_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("terminal_select_combo"));
	g_signal_connect (G_OBJECT (WID ("terminal_custom_radio")),
			  "mnemonic_activate", (GCallback) mnemonic_activate,
			  WID ("terminal_custom_command_entry"));
	g_signal_connect (G_OBJECT (WID ("terminal_custom_radio")),
			  "toggled", (GCallback) generic_guard,
			  WID ("terminal_custom_table"));
	read_terminal (client, dialog);	

	gtk_notebook_remove_page (GTK_NOTEBOOK (WID ("notebook")), 4);
	
	g_signal_connect (G_OBJECT (client), "value-changed", (GCallback) value_changed_cb, dialog);
	g_signal_connect (G_OBJECT (WID ("default_applications_dialog")), "response", (GCallback) dialog_response, dialog);
	
	gtk_widget_show_all (WID ("default_applications_dialog"));

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

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
	textdomain (PACKAGE);

	gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	client = gconf_client_get_default ();

	gconf_client_add_dir (client, "/desktop/gnome/applications/editor", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/desktop/gnome/applications/browser", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	gconf_client_add_dir (client, "/desktop/gnome/applications/help_viewer", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
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
