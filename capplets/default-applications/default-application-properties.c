/* -*- MODE: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Author: Benjamin Kahn <xkahn@zoned.net>
 * Based on capplets/gnome-edit-properties/gnome-edit-properties.c.
 */
#include <config.h>
#include <stdio.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <locale.h>

#include "gnome.h"
#include "defaults.h"
#include "interface.h"
#include <gconf/gconf-client.h>

#define GNOME_DESKTOP_PREFIX "/desktop/gnome/applications"

void fill_default_browser (void);
void fill_default_terminal (void);
void fill_default_help (void);
void fill_default_editor (void);

static void fill_editor_data (EditorDescription *info);
static void fill_browser_data (BrowserDescription *info);
static void fill_help_data (HelpViewDescription *info);
static void fill_term_data (TerminalDescription *info);

static int editor_find_index (EditorDescription *info);
static int browser_find_index (BrowserDescription *info);
static int help_find_index (HelpViewDescription *info);
static int term_find_index (TerminalDescription *info);

static void set_combo_terminal( int index );
static void set_combo_help( int index );
static void set_combo_editor( int index );
static void set_combo_browser( int index );

static void revert_all (void);
static void write_all (BrowserDescription *bd, EditorDescription *ed, HelpViewDescription *hd, TerminalDescription *td);
static void help_all (void);
static void apply_all (void);

int set_selected_terminal( gchar *string );
int set_selected_help( gchar *string );
int set_selected_editor( gchar *string );
int set_selected_browser( gchar *string );

gboolean ignore_changes = TRUE;

BrowserDescription boriginal_info = { NULL };
EditorDescription eoriginal_info = { NULL };
HelpViewDescription horiginal_info = { NULL };
TerminalDescription toriginal_info = { NULL };

BrowserDescription bcurrent_info = { NULL };
EditorDescription ecurrent_info = { NULL };
HelpViewDescription hcurrent_info = { NULL };
TerminalDescription tcurrent_info = { NULL };

EditorDescription possible_editors[] =
{
        { "gedit",        "gedit",  FALSE,  FALSE },
        { "Emacs",        "emacs",  FALSE,  TRUE },
        { "XEmacs",       "xemacs", FALSE,  TRUE },
        { "vi",           "vi",     TRUE,  TRUE },
        { "Go",           "go",     FALSE,  FALSE },
        { "GWP",          "gwp",    FALSE,  FALSE },
        { "Jed",          "jed",    TRUE,  TRUE },
        { "Joe",          "joe",    TRUE,  TRUE },
        { "Pico",         "pico",   TRUE,  TRUE },
        { "vim",          "vim",    TRUE,  TRUE },
        { "gvim",         "gvim",   FALSE,  TRUE },
        { "ed",           "ed",     TRUE,  FALSE },
        { "GMC/CoolEdit", "gmc -e", FALSE, FALSE },
	{ "Nedit",        "nedit",  FALSE, FALSE }
};

BrowserDescription possible_browsers[] =
{
        { "Lynx Text Browser", "lynx", TRUE, FALSE },
        { "Links Text Browser" , "links", TRUE, FALSE },
        { "Netscape Communicator", "netscape", FALSE, TRUE },
        { "Mozilla/Netscape 6", "mozilla", FALSE, TRUE },
        { "Galeon", "galeon", FALSE, FALSE },
        { "Encompass", "encompass", FALSE, FALSE },
        { "Konqueror", "konqueror", FALSE, FALSE }
};

HelpViewDescription possible_helpviewers[] = 
{ 
        { "Gnome Help Browser", "gnome-help-browser", FALSE, TRUE },
        { "Nautilus", "nautilus", FALSE, TRUE }
};

TerminalDescription possible_terminals[] = 
{ 
        { "Gnome Terminal", "gnome-terminal", "-x" },
        { "Standard XTerminal", "xterm", "-e" },
        { "NXterm", "nxterm", "-e" },
        { "RXVT", "rxvt", "-e" },
        { "ETerm", "Eterm", "-e" }
};



GtkWidget *capplet;
GtkWidget *combo = NULL;
GtkWidget *checkbox;

static void
edit_read(void)
{
	GConfClient *client;
        gchar **term_argv;
        gchar *tmp;

        fill_default_help ();
        fill_default_terminal ();
        fill_default_browser ();
        fill_default_editor ();
       
	client = gconf_client_get_default ();
        
        eoriginal_info.executable_name = gconf_client_get_string (client, GNOME_DESKTOP_PREFIX "/editor/exec", NULL);
        eoriginal_info.needs_term = gconf_client_get_bool (client, GNOME_DESKTOP_PREFIX "/editor/needs_term", NULL);
        eoriginal_info.accepts_lineno = gconf_client_get_bool (client, GNOME_DESKTOP_PREFIX "/editor/accepts_lineno", NULL);
	eoriginal_info.index = editor_find_index (&eoriginal_info);
	if (eoriginal_info.index != -1)
		eoriginal_info.name = possible_editors[eoriginal_info.index].name;

	ecurrent_info.name = eoriginal_info.name;
        ecurrent_info.executable_name = g_strdup (eoriginal_info.executable_name);
        ecurrent_info.needs_term = eoriginal_info.needs_term;
        ecurrent_info.accepts_lineno = eoriginal_info.accepts_lineno;
	ecurrent_info.index = eoriginal_info.index;
        
        boriginal_info.executable_name = gconf_client_get_string (client, GNOME_DESKTOP_PREFIX "/browser/exec", NULL);
        boriginal_info.needs_term = gconf_client_get_bool (client, GNOME_DESKTOP_PREFIX "/browser/needs_term", NULL);
        boriginal_info.nremote = gconf_client_get_bool (client, GNOME_DESKTOP_PREFIX "/browser/nremote", NULL);
	boriginal_info.index = browser_find_index (&boriginal_info);
	if (boriginal_info.index != -1)
		boriginal_info.name = possible_browsers[boriginal_info.index].name;

	bcurrent_info.name = boriginal_info.name;
        bcurrent_info.executable_name = g_strdup (boriginal_info.executable_name);
        bcurrent_info.needs_term = boriginal_info.needs_term;
        bcurrent_info.nremote = boriginal_info.nremote;
	bcurrent_info.index = boriginal_info.index;
	
        horiginal_info.executable_name = gconf_client_get_string (client, GNOME_DESKTOP_PREFIX "/help_viewer/exec", NULL);
        horiginal_info.needs_term = gconf_client_get_bool (client, GNOME_DESKTOP_PREFIX "/help_viewer/needs_term", NULL);
        horiginal_info.allows_urls = gconf_client_get_bool (client, GNOME_DESKTOP_PREFIX "/help_viewer/accepts_urls", NULL);
	horiginal_info.index = help_find_index (&horiginal_info);
	if (horiginal_info.index != -1)
		horiginal_info.name = possible_helpviewers[horiginal_info.index].name;

	hcurrent_info.name = horiginal_info.name;
        hcurrent_info.executable_name = g_strdup (horiginal_info.executable_name);
        hcurrent_info.needs_term = horiginal_info.needs_term;
        hcurrent_info.allows_urls = horiginal_info.allows_urls;
	hcurrent_info.index = horiginal_info.index;

	tmp = gconf_client_get_string (client, GNOME_DESKTOP_PREFIX "/terminal", NULL);
	term_argv = g_strsplit (tmp, " ", 3);
        toriginal_info.executable_name = term_argv[0];
	toriginal_info.exec_app = term_argv[1];

	toriginal_info.index = term_find_index (&toriginal_info);
	g_free (term_argv);
	if (toriginal_info.index != -1)
		toriginal_info.name = possible_terminals[toriginal_info.index].name;

	tcurrent_info.name = toriginal_info.name;
        tcurrent_info.executable_name = g_strdup (toriginal_info.executable_name);
        tcurrent_info.executable_name = g_strdup (toriginal_info.exec_app);
	tcurrent_info.index = toriginal_info.index;

        /* Set sensitivity. */

        fill_editor_data (&ecurrent_info);
        fill_browser_data (&bcurrent_info);
        fill_help_data (&hcurrent_info);
        fill_term_data (&tcurrent_info);

	g_object_unref (G_OBJECT (client));
}

void
edit_changed (GtkWidget *widget, gpointer data)
{
//        if (!ignore_changes)
  //              capplet_widget_state_changed(CAPPLET_WIDGET (capplet), TRUE);
}

static void
get_all_data ()
{
        ecurrent_info.name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "combo_editor")));
        ecurrent_info.executable_name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "editorcommand")));
        ecurrent_info.needs_term = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorterminal")));
        ecurrent_info.accepts_lineno = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorlineno")));
        
        bcurrent_info.name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "combo_browser")));
        bcurrent_info.executable_name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "browsercommand")));
        bcurrent_info.needs_term = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserterminal")));
        bcurrent_info.nremote = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserremote")));

        hcurrent_info.name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "combo_help")));
        hcurrent_info.executable_name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "helpcommand")));
        hcurrent_info.needs_term = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "helpterminal")));
        hcurrent_info.allows_urls = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "helpurls")));

        tcurrent_info.name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "combo_term")));
        tcurrent_info.executable_name = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termcommand")));
        tcurrent_info.exec_app = gtk_entry_get_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termexec")));

}

static void 
fill_editor_data (EditorDescription *info) 
{
        
        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "editorcommand")), 
                            info->executable_name);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorterminal")),
                                      info->needs_term);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorlineno")),
                                      info->accepts_lineno);
        
        if (info->index != -1) {
                set_combo_editor (info->index);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "radiodefeditor")), 
                                              TRUE);
        } else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "radiocusteditor")), 
                                              TRUE);
        }
        

}

static void
fill_browser_data (BrowserDescription *info)
{
        
        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "browsercommand")), 
                            info->executable_name);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserterminal")),
                                      info->needs_term);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserremote")),
                                      info->nremote);
        

        if (info->index != -1) {
                set_combo_browser (info->index);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "seldefbrowser")), 
                                              TRUE);
        } else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "selcustbrowser")), 
                                              TRUE);
        }

}

static void
fill_help_data (HelpViewDescription *info)
{
        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "helpcommand")), 
                            info->executable_name);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "helpterminal")),
                                      info->needs_term);

        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "helpurls")),
                                      info->allows_urls);

        if (info->index != -1) {
                set_combo_help (info->index);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "seldefview")), 
                                              TRUE);
        } else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "selcustview")), 
                                              TRUE);
        }
}
static void
fill_term_data (TerminalDescription *info)
{
        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termcommand")), 
                            info->executable_name);

        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termexec")),
                            (info->exec_app) ? info->exec_app : "");

        if (info->index != -1) {
                set_combo_terminal (info->index); 
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "seldefterm")), 
                                              TRUE);
        } else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "selcustterm")), 
                                              TRUE);
        }
}

static void
response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
	switch (response_id)
	{
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	case GTK_RESPONSE_APPLY:
		apply_all ();
		break;
	case GTK_RESPONSE_HELP:
		help_all ();
		break;
	}
}

int
main (int argc, char **argv)
{
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (PACKAGE, "UTF-8");
        textdomain (PACKAGE);

	gnome_program_init ("default-applications-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, NULL);
        
        ignore_changes = TRUE;

        /* Display the application */
        edit_create ();

        /* Set up the rest of the application. */
        edit_read ();

        /* Connect the wrapper signals */
	g_signal_connect (G_OBJECT (capplet), "response",
			  G_CALLBACK (response_cb), NULL);

        ignore_changes = FALSE;

        gtk_main ();

	return 0;
}

static void 
revert_all (void)
{
        ignore_changes = TRUE;
        write_all (&boriginal_info, &eoriginal_info, &horiginal_info, &toriginal_info);

        edit_read ();

        ignore_changes = FALSE;
}

static void
apply_all (void)
{
        get_all_data();
        write_all (&bcurrent_info, &ecurrent_info, &hcurrent_info, &tcurrent_info);     
}

static void 
help_all (void)
{
}

static gchar*
compose_full_term_exec (TerminalDescription *td, gchar *execstr)
{
	g_return_val_if_fail (td != NULL, NULL);
	g_return_val_if_fail (execstr != NULL, NULL);

	if (td->exec_app)
		return g_strconcat (td->executable_name, " ", td->exec_app, " ", execstr, " ", "\"%s\"", NULL);
	else
		return g_strconcat (td->executable_name, " ", execstr, " ", "\"%s\"", NULL);
}

static void 
write_all (BrowserDescription *bd, EditorDescription *ed, HelpViewDescription *hd, TerminalDescription *td)
{
        gchar *tmp;
	GConfClient *client = gconf_client_get_default ();
        
        if (ed->executable_name)
		gconf_client_set_string (client, GNOME_DESKTOP_PREFIX "/editor/exec", ed->executable_name, NULL);
        else
		gconf_client_unset (client, GNOME_DESKTOP_PREFIX "/editor/exec", NULL);
	gconf_client_set_bool (client, GNOME_DESKTOP_PREFIX "/editor/needs_term", ed->needs_term, NULL);
	gconf_client_set_bool (client, GNOME_DESKTOP_PREFIX "/editor/accepts_lineno", ed->accepts_lineno, NULL);
        
         if (bd->executable_name)
		gconf_client_set_string (client, GNOME_DESKTOP_PREFIX "/browser/exec", bd->executable_name, NULL);
        else
		gconf_client_unset (client, GNOME_DESKTOP_PREFIX "/browser/exec", NULL);
	gconf_client_set_bool (client, GNOME_DESKTOP_PREFIX "/browser/needs_term", bd->needs_term, NULL);
	gconf_client_set_bool (client, GNOME_DESKTOP_PREFIX "/browser/nremote", bd->nremote, NULL);
       
	if (bd->needs_term)
		tmp = compose_full_term_exec (td, bd->executable_name);
	else
		tmp = g_strconcat (bd->executable_name, " \"%s\"", NULL);
	gconf_client_set_string (client, "/desktop/gnome/url-handlers/default-show", tmp, NULL);
	g_free (tmp);

	if (hd->executable_name)
		gconf_client_set_string (client, GNOME_DESKTOP_PREFIX "/help_viewer/exec", hd->executable_name, NULL);
        else
		gconf_client_unset (client, GNOME_DESKTOP_PREFIX "/help_viewer/exec", NULL);
	gconf_client_set_bool (client, GNOME_DESKTOP_PREFIX "/help_viewer/needs_term", hd->needs_term, NULL);
	gconf_client_set_bool (client, GNOME_DESKTOP_PREFIX "/help_viewer/accepts_urls", hd->allows_urls, NULL);

	if (hd->needs_term)
		tmp = compose_full_term_exec (td, hd->executable_name);
	else
		tmp = g_strconcat (hd->executable_name, " \"%s\"", NULL);
	gconf_client_set_string (client, "/desktop/gnome/url-handlers/ghelp-show", tmp, NULL);
	g_free (tmp);

        if (td->exec_app && td->executable_name)
	{
		tmp = g_strconcat (td->executable_name, " ", td->exec_app, NULL);
		gconf_client_set_string (client, GNOME_DESKTOP_PREFIX "/terminal", tmp, NULL);
		g_free (tmp);
	}
        else if (td->executable_name)
		gconf_client_set_string (client, GNOME_DESKTOP_PREFIX "/terminal", td->executable_name, NULL);
        else
		gconf_client_unset (client, GNOME_DESKTOP_PREFIX "/terminal", NULL);
	
	g_object_unref (G_OBJECT (client));
}

void fill_default_browser ()
{
        gint i;
        GtkWidget *listitem;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "browserselect");
        
        /*        printf ("%p %p\n", capplet, combo);*/

        for ( i = 0; i < sizeof(possible_browsers) / sizeof(possible_browsers[0]); i++ ) {
                if (gnome_is_program_in_path (possible_browsers[i].executable_name)) {
                        listitem = gtk_list_item_new_with_label ( possible_browsers[ i ].name );
                        gtk_widget_show( listitem );
                        gtk_container_add( GTK_CONTAINER( GTK_COMBO( combo )->list ), listitem );
                }
        }

        return;
}

void fill_default_terminal ()
{
        gint i;
        GtkWidget *listitem;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "termselect");
        
        for ( i = 0; i < sizeof(possible_terminals) / sizeof(possible_terminals[0]); i++ ) {
                if (gnome_is_program_in_path (possible_terminals[i].executable_name)) {
                        listitem = gtk_list_item_new_with_label ( possible_terminals[ i ].name );
                        gtk_widget_show( listitem );
                        gtk_container_add( GTK_CONTAINER( GTK_COMBO( combo )->list ), listitem );
                }
        }

        return;
}

void fill_default_help ()
{
        gint i;
        GtkWidget *listitem;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "helpselect");
        
        for ( i = 0; i < sizeof(possible_helpviewers) / sizeof(possible_helpviewers[0]); i++ ) {
                if (gnome_is_program_in_path (possible_helpviewers[i].executable_name)) {
                        listitem = gtk_list_item_new_with_label ( possible_helpviewers[ i ].name );
                        gtk_widget_show( listitem );
                        gtk_container_add( GTK_CONTAINER( GTK_COMBO( combo )->list ), listitem );
                }
        }

        return;
}

void fill_default_editor ()
{
        gint i;
        GtkWidget *listitem;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "editorselect");
        
        for ( i = 0; i < sizeof(possible_editors) / sizeof(possible_editors[0]); i++ ) {
                if (gnome_is_program_in_path (possible_editors[i].executable_name)) {
                        listitem = gtk_list_item_new_with_label ( possible_editors[ i ].name );
                        gtk_widget_show( listitem );
                        gtk_container_add( GTK_CONTAINER( GTK_COMBO( combo )->list ), listitem );
                }
        }

        return;
}


static void
set_combo_browser(int index)
{
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "browserselect");
        
	gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(combo)->entry),
			    possible_browsers[ index ].name ); 
}

static void
set_combo_editor(int index)
{
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "editorselect");
        
	gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(combo)->entry),
			    possible_editors[ index ].name ); 
}

static void
set_combo_help(int index)
{
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "helpselect");
	gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(combo)->entry),
			    possible_helpviewers[ index ].name ); 
}

static void
set_combo_terminal(int index)
{
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "termselect");
	gtk_entry_set_text (GTK_ENTRY(GTK_COMBO(combo)->entry),
			    possible_terminals[ index ].name ); 
}

int
set_selected_browser( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "browserselect");
        
        if (!string)
                return -1;

        for ( i = 0; i < sizeof(possible_browsers) / sizeof(possible_browsers[0]); i++ ) {
                if ( ! strcmp( possible_browsers[ i ].name, string ) ) {
                        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "browsercommand")),
                                            possible_browsers[ i ].executable_name);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserterminal")),
                                                      possible_browsers[ i ].needs_term);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserremote")),
                                                      possible_browsers[ i ].nremote);
                        return i;
                }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );*/
	return -1;
}

int 
set_selected_editor( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "editorselect");
        
        if (!string)
                return -1;

        for ( i = 0; i < sizeof(possible_editors) / sizeof(possible_editors[0]); i++ ) {
                if ( ! strcmp( possible_editors[ i ].name, string ) ) {
                        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "editorcommand")),
                                            possible_editors[ i ].executable_name);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorterminal")),
                                                      possible_editors[ i ].needs_term);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorlineno")),
                                                      possible_editors[ i ].accepts_lineno);
                        return i;
                }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string ); */
	return -1;
}

int
set_selected_help( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "helpselect");
        
        if (!string)
                return -1;

        for ( i = 0;
              i < sizeof(possible_helpviewers) / sizeof(possible_helpviewers[0]);
              i++ ) {
                if ( ! strcmp( possible_helpviewers[ i ].name, string ) )
                        {
                                gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "helpcommand")),
                                                    possible_helpviewers[ i ].executable_name);
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "helpterminal")),
                                                              possible_helpviewers[ i ].needs_term);
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "helpurls")),
                                                              possible_helpviewers[ i ].allows_urls);
                                return i;
                        }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );*/
	return -1;
}

int
set_selected_terminal( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "termselect");
        
        if (!string)
                return -1;

        for ( i = 0;
              i < sizeof(possible_terminals) / sizeof(possible_terminals[0]);
              i++ ) {
                if ( ! strcmp( possible_terminals[ i ].name, string ) )
                        {
                                gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termcommand")),
                                                    possible_terminals[ i ].executable_name);
                                gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termexec")),
                                                    possible_terminals[ i ].exec_app);
                                return i;
                        }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );*/
	return -1;
}

static int editor_find_index (EditorDescription *info)
{
	int i;

	g_return_val_if_fail (info != NULL, -1);

        for ( i = 0;
              i < sizeof(possible_editors) / sizeof(possible_editors[0]);
              i++ ) {
		if (!strcmp (possible_editors[i].executable_name, info->executable_name) &&
		    possible_editors[i].needs_term == info->needs_term &&
		    possible_editors[i].accepts_lineno == info->accepts_lineno)
			return i;
	}
	
	return -1;
}

static int browser_find_index (BrowserDescription *info)
{
	int i;

	g_return_val_if_fail (info != NULL, -1);

        for ( i = 0;
              i < sizeof(possible_browsers) / sizeof(possible_browsers[0]);
              i++ ) {
		if (!strcmp (possible_browsers[i].executable_name, info->executable_name) &&
		    possible_browsers[i].needs_term == info->needs_term &&
		    possible_browsers[i].nremote == info->nremote)
			return i;
	}
	
	return -1;

}

static int help_find_index (HelpViewDescription *info)
{
	int i;

	g_return_val_if_fail (info != NULL, -1);

        for ( i = 0;
              i < sizeof(possible_helpviewers) / sizeof(possible_helpviewers[0]);
              i++ ) {
		if (!strcmp (possible_helpviewers[i].executable_name, info->executable_name) &&
		    possible_helpviewers[i].needs_term == info->needs_term &&
		    possible_helpviewers[i].allows_urls == info->allows_urls)
			return i;
	}
	
	return -1;
}

static int term_find_index (TerminalDescription *info)
{
	int i;

	g_return_val_if_fail (info != NULL, -1);

        for ( i = 0;
              i < sizeof(possible_terminals) / sizeof(possible_terminals[0]);
              i++ ) {
		if (!strcmp (possible_terminals[i].executable_name, info->executable_name) &&
		    !strcmp (possible_terminals[i].exec_app, info->exec_app))
			return i;
	}
	
	return -1;
}

