/* -*- MODE: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Author: Benjamin Kahn <xkahn@zoned.net>
 * Based on capplets/gnome-edit-properties/gnome-edit-properties.c.
 */
#include <config.h>
#include "capplet-widget.h"
#include <stdio.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <locale.h>

#include "gnome.h"
#include "defaults.h"
#include "interface.h"

void fill_default_browser (void);
void fill_default_terminal (void);
void fill_default_help (void);
void fill_default_editor (void);

static void fill_editor_data (EditorDescription *info);
static void fill_browser_data (BrowserDescription *info);
static void fill_help_data (HelpViewDescription *info);
static void fill_term_data (TerminalDescription *info);

static void set_combo_terminal( gchar *string );
static void set_combo_help( gchar *string );
static void set_combo_editor( gchar *string );
static void set_combo_browser( gchar *string );

static void revert_all (void);
static void write_all (BrowserDescription *bd, EditorDescription *ed, HelpViewDescription *hd, TerminalDescription *td);
static void help_all (void);
static void apply_all (void);

void set_selected_terminal( gchar *string );
void set_selected_help( gchar *string );
void set_selected_editor( gchar *string );
void set_selected_browser( gchar *string );

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
        { "Emacs", "emacs", FALSE, "executable", TRUE },
        { "XEmacs", "xemacs", FALSE, "executable", TRUE },
        { "vi", "vi", TRUE, "executable", TRUE },
        { "Go", "go", FALSE, "executable", FALSE },
        { "gEdit", "gedit", FALSE, "executable", FALSE },
        { "GWP", "gwp", FALSE, "executable", FALSE },
        { "Jed", "jed", TRUE, "executable", TRUE },
        { "Joe", "joe", TRUE, "executable", TRUE },
        { "Pico", "pico", TRUE, "executable", TRUE },
        { "vim",  "vim", TRUE, "executable", TRUE },
        { "gvim",  "gvim", FALSE, "executable", TRUE },
        { "ed", "ed", TRUE, "executable", FALSE },
        { "GMC/CoolEdit", "gmc -e", FALSE, "mc-internal", FALSE },
	{ "Nedit", "nedit", FALSE, "executable", FALSE }
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
        gint term_argc;
        gchar **term_argv;
        gchar *check1, *check2;

        fill_default_help ();
        fill_default_terminal ();
        fill_default_browser ();
        fill_default_editor ();
        
        check1 = gnome_config_get_string("/editor/Editor/EDITNAME");
        check2 = gnome_config_get_string("/editor/Editor/EDITOR");
        if (check1 || !check2) {
                eoriginal_info.use_name = TRUE;
                ecurrent_info.use_name = TRUE;
        } else {
                eoriginal_info.use_name = FALSE;
                ecurrent_info.use_name = FALSE;
        }
        g_free (check1);
        g_free (check2);
        eoriginal_info.name = gnome_config_get_string("/editor/Editor/EDITNAME");
        eoriginal_info.executable_name = gnome_config_get_string("/editor/Editor/EDITOR");
        eoriginal_info.needs_term = gnome_config_get_bool_with_default("/editor/Editor/NEEDS_TERM", NULL);
        eoriginal_info.execution_type = gnome_config_get_string("/editor/Editor/EDITOR_TYPE");
        eoriginal_info.accepts_lineno = gnome_config_get_bool_with_default("/editor/Editor/ACCEPTS_LINE_NO", NULL);

        ecurrent_info.name = gnome_config_get_string("/editor/Editor/EDITNAME=Emacs");
        ecurrent_info.executable_name = gnome_config_get_string("/editor/Editor/EDITOR=emacs");
        ecurrent_info.needs_term = gnome_config_get_bool_with_default("/editor/Editor/NEEDS_TERM=FALSE", NULL);
        ecurrent_info.execution_type = gnome_config_get_string("/editor/Editor/EDITOR_TYPE=executable");
        ecurrent_info.accepts_lineno = gnome_config_get_bool_with_default("/editor/Editor/ACCEPTS_LINE_NO=TRUE", NULL);
        
        check1 = gnome_config_get_string("/gnome-moz-remote/Mozilla/BROWSER");
        check2 = gnome_config_get_string("/gnome-moz-remote/Mozilla/filename");
        if (check1 || !check2) {
                boriginal_info.use_name = TRUE;
                bcurrent_info.use_name = TRUE;
        } else {
                boriginal_info.use_name = FALSE;
                bcurrent_info.use_name = FALSE;
        }
        g_free (check1);
        g_free (check2);
        if (gnome_is_program_in_path ("mozilla")) {
                bcurrent_info.name = gnome_config_get_string("/gnome-moz-remote/Mozilla/BROWSER=Mozilla/Netscape 6");
                bcurrent_info.executable_name = gnome_config_get_string("/gnome-moz-remote/Mozilla/filename=mozilla");
        } else {
                bcurrent_info.name = gnome_config_get_string("/gnome-moz-remote/Mozilla/BROWSER=Netscape Communicator");
                bcurrent_info.executable_name = gnome_config_get_string("/gnome-moz-remote/Mozilla/filename=netscape");
        }
        bcurrent_info.needs_term = gnome_config_get_bool_with_default("/gnome-moz-remote/Mozilla/NEEDS_TERM=FALSE", NULL);
        bcurrent_info.nremote = gnome_config_get_bool_with_default("/gnome-moz-remote/Mozilla/NREMOTE=TRUE", NULL);

        boriginal_info.name = gnome_config_get_string("/gnome-moz-remote/Mozilla/BROWSER");
        boriginal_info.executable_name = gnome_config_get_string("/gnome-moz-remote/Mozilla/filename");
        boriginal_info.needs_term = gnome_config_get_bool_with_default("/gnome-moz-remote/Mozilla/NEEDS_TERM", NULL);
        boriginal_info.nremote = gnome_config_get_bool_with_default("/gnome-moz-remote/Mozilla/NREMOTE", NULL);
        
        /* An smarter person would wonder why we are setting values in different places.  It's because of the */
        /* silly way url-properties works.  Originally, I was going to replace that applet, but people seem to use it. */
        /* For simplicity, at first, we will only support the ghelp calls.  Not info or man. */
        check1 = gnome_config_get_string("/Gnome/URL Handler Data/GHELP");
        check2 = gnome_config_get_string("/Gnome/URL Handlers/ghelp-show");
        if (check1 || !check2) {
                horiginal_info.use_name = TRUE;
                hcurrent_info.use_name = TRUE;
        } else {
                horiginal_info.use_name = FALSE;
                hcurrent_info.use_name = FALSE;
        }
        g_free (check1);
        g_free (check2);
        if (gnome_is_program_in_path ("nautilus")) {
                hcurrent_info.name = gnome_config_get_string("/Gnome/URL Handler Data/GHELP=nautilus");
                hcurrent_info.executable_name = gnome_config_get_string ("/Gnome/URL Handlers/ghelp-show=nautilus");
        } else {
                hcurrent_info.name = gnome_config_get_string("/Gnome/URL Handler Data/GHELP=Gnome Help Browser");
                hcurrent_info.executable_name = gnome_config_get_string ("/Gnome/URL Handlers/ghelp-show=gnome-help-browser");
        }
        hcurrent_info.needs_term = gnome_config_get_bool_with_default("/Gnome/URL Handler Data/GHELP_TERM=FALSE", NULL);
        hcurrent_info.allows_urls = gnome_config_get_bool_with_default("/Gnome/URL Handler Data/GHELP_URLS=TRUE", NULL);

        horiginal_info.name = gnome_config_get_string("/Gnome/URL Handler Data/GHELP");
        horiginal_info.executable_name = gnome_config_get_string ("/Gnome/URL Handlers/ghelp-show");
        horiginal_info.needs_term = gnome_config_get_bool_with_default("/Gnome/URL Handler Data/GHELP_TERM", NULL);
        horiginal_info.allows_urls = gnome_config_get_bool_with_default("/Gnome/URL Handler Data/GHELP_URLS", NULL);

        /* Ugh.  This one is really complex. */
        check1 = gnome_config_get_string("/Gnome/Applications/TERMNAME");
        check2 = gnome_config_get_string("/Gnome/Applications/Terminal");
        if (check1 || !check2) {
                toriginal_info.use_name = TRUE;
                tcurrent_info.use_name = TRUE;
        } else {
                toriginal_info.use_name = FALSE;
                tcurrent_info.use_name = FALSE;
        }
        g_free (check1);
        g_free (check2);
        gnome_config_get_vector ("/Gnome/Applications/Terminal",
                                 &term_argc, &term_argv);
        if (term_argv == NULL) {
                toriginal_info.executable_name = NULL;
                toriginal_info.exec_app = NULL;

                if (gnome_is_program_in_path ("gnome-terminal")) {
                        tcurrent_info.name = g_strdup ("Gnome Terminal");
                        tcurrent_info.executable_name = g_strdup ("gnome-terminal");
                        tcurrent_info.exec_app = g_strdup ("-x");
                } else {
                        tcurrent_info.name = g_strdup ("Standard XTerminal");
                        tcurrent_info.executable_name = g_strdup ("xterm");
                        tcurrent_info.exec_app = g_strdup ("-e");
                }
        } else {
                tcurrent_info.name = gnome_config_get_string("/Gnome/Applications/TERMNAME");
                tcurrent_info.executable_name = term_argv[0];
                tcurrent_info.exec_app = term_argv[1];

                toriginal_info.executable_name = term_argv[0];
                toriginal_info.exec_app = term_argv[1];
        }

        toriginal_info.name = gnome_config_get_string("/Gnome/Applications/TERMNAME");

        /* Set sensitivity. */

        fill_editor_data (&ecurrent_info);
        fill_browser_data (&bcurrent_info);
        fill_help_data (&hcurrent_info);
        fill_term_data (&tcurrent_info);
}

void
edit_changed (GtkWidget *widget, gpointer data)
{
        if (!ignore_changes)
                capplet_widget_state_changed(CAPPLET_WIDGET (capplet), TRUE);
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
        
        if (info->use_name) {
                set_combo_editor (info->name);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "radiodefeditorm")), 
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
        

        if (info->use_name) {
                set_combo_browser (info->name);
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

        if (info->use_name) {
                set_combo_help (info->name);
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
                            info->exec_app);

        if (info->use_name) {
                set_combo_terminal (info->name); 
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "seldefterm")), 
                                              TRUE);
        } else {
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
                                              (gtk_object_get_data (GTK_OBJECT (capplet), "selcustterm")), 
                                              TRUE);
        }
}

int
main (int argc, char **argv)
{
				setlocale(LC_ALL, "");
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

        switch (gnome_capplet_init ("default-application-properties", VERSION, argc,
                                    argv, NULL, 0, NULL)) {
                
        case -1:
                return 0;
        default:
		break;
        }
        
        ignore_changes = TRUE;

        /* Display the application */
        edit_create ();

        /* Set up the rest of the application. */
        edit_read ();

        /* Connect the wrapper signals */

        gtk_signal_connect (GTK_OBJECT (capplet), "help",
                            GTK_SIGNAL_FUNC (help_all), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "try",
                            GTK_SIGNAL_FUNC (apply_all), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "revert",
                            GTK_SIGNAL_FUNC (revert_all), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "ok",
                            GTK_SIGNAL_FUNC (apply_all), NULL);
        gtk_signal_connect (GTK_OBJECT (capplet), "cancel",
                            GTK_SIGNAL_FUNC (revert_all), NULL);

        ignore_changes = FALSE;

        capplet_gtk_main ();

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

static void 
write_all (BrowserDescription *bd, EditorDescription *ed, HelpViewDescription *hd, TerminalDescription *td)
{
        gchar *av_term[2];

        if (ed->use_name && ed->name) {
                gnome_config_set_string ("/editor/Editor/EDITNAME", ed->name);
        } else {
                gnome_config_clean_key("/editor/Editor/EDITNAME");
        }
        if (bd->use_name && bd->name) {
                gnome_config_set_string ("/gnome-moz-remote/Mozilla/BROWSER", bd->name);
        } else {
                gnome_config_clean_key("/gnome-moz-remote/Mozilla/BROWSER");
        }
        if (hd->use_name && hd->name) {
                gnome_config_set_string ("/Gnome/URL Handler Data/GHELP", hd->name);
        } else {
                gnome_config_clean_key("/Gnome/URL Handler Data/GHELP");
        }
        if (td->use_name && td->name) {
                gnome_config_set_string ("/Gnome/Applications/TERMNAME", td->name);
        } else {
                gnome_config_clean_key("/Gnome/Applications/TERMNAME");
        }
        
        if (ed->executable_name)
                gnome_config_set_string("/editor/Editor/EDITOR", ed->executable_name);
        else
                gnome_config_clean_key("/editor/Editor/EDITOR");
        gnome_config_set_bool("/editor/Editor/NEEDS_TERM", ed->needs_term);
        if (ed->execution_type)
                gnome_config_set_string("/editor/Editor/EDITOR_TYPE", ed->execution_type);
        else
                gnome_config_clean_key("/editor/Editor/EDITOR_TYPE");
        gnome_config_set_bool("/editor/Editor/ACCEPTS_LINE_NO", ed->accepts_lineno);
        
        if (bd->executable_name)
                gnome_config_set_string("/gnome-moz-remote/Mozilla/filename", bd->executable_name);
        else 
                gnome_config_clean_key("/gnome-moz-remote/Mozilla/filename");
        
        if (bd->executable_name) {
                gnome_config_set_bool("/gnome-moz-remote/Mozilla/NEEDS_TERM", bd->needs_term);
                gnome_config_set_bool("/gnome-moz-remote/Mozilla/NREMOTE", bd->nremote);
        } else {
                gnome_config_clean_key ("/gnome-moz-remote/Mozilla/NEEDS_TERM");
                gnome_config_clean_key ("/gnome-moz-remote/Mozilla/NREMOTE");
        }
        
        if (hd->executable_name)
                gnome_config_set_string ("/Gnome/URL Handlers/ghelp-show", hd->executable_name);
        else
                gnome_config_clean_key("/Gnome/URL Handlers/ghelp-show");
        gnome_config_set_bool("/Gnome/URL Handler Data/GHELP_TERM", hd->needs_term);
        gnome_config_set_bool("/Gnome/URL Handler Data/GHELP_URLS", hd->allows_urls);        
        
        av_term[0] = td->executable_name;
        av_term[1] = td->exec_app;
        if (td->exec_app && td->executable_name)
                gnome_config_set_vector ("/Gnome/Applications/Terminal", 2, av_term);
        else if (td->executable_name) 
                gnome_config_set_string ("/Gnome/Applications/Terminal", td->executable_name);
        else
                gnome_config_clean_key("/Gnome/Applications/Terminal");

        gnome_config_sync ();
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
set_combo_browser( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "browserselect");
        
        if (!string)
                return;

        for ( i = 0; i < sizeof(possible_browsers) / sizeof(possible_browsers[0]); i++ ) {
                if ( ! strcmp( possible_browsers[ i ].executable_name, string ) ) {
                        gtk_entry_set_text ( GTK_ENTRY(GTK_COMBO(combo)->entry),
                                             possible_browsers[ i ].name ); 
                        return;
                }
        }
        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );
}

static void
set_combo_editor( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "editorselect");
        
        if (!string)
                return;

        for ( i = 0; i < sizeof(possible_editors) / sizeof(possible_editors[0]); i++ ) {
                if ( ! strcmp( possible_editors[ i ].executable_name, string ) ) {
                        gtk_entry_set_text ( GTK_ENTRY(GTK_COMBO(combo)->entry),
                                             possible_editors[ i ].name ); 
                        return;
                }
        }
        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string ); 
}

static void
set_combo_help( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "helpselect");
        
        if (!string)
                return;

        for ( i = 0;
              i < sizeof(possible_helpviewers) / sizeof(possible_helpviewers[0]);
              i++ ) {
                if ( ! strcmp( possible_helpviewers[ i ].executable_name, string ) )
                        {
                                gtk_entry_set_text
                                        ( GTK_ENTRY(GTK_COMBO(combo)->entry),
                                          possible_helpviewers[ i ].name ); 
                                return;
                        }
        }
        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );
}

static void
set_combo_terminal( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "termselect");
        
        if (!string)
                return;

        for ( i = 0;
              i < sizeof(possible_terminals) / sizeof(possible_terminals[0]);
              i++ ) {
                if ( ! strcmp( possible_terminals[ i ].executable_name, string ) )
                        {
                                gtk_entry_set_text
                                        ( GTK_ENTRY(GTK_COMBO(combo)->entry),
                                          possible_terminals[ i ].name ); 
                                return;
                        }
        }
        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );
}

void
set_selected_browser( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "browserselect");
        
        if (!string)
                return;

        for ( i = 0; i < sizeof(possible_browsers) / sizeof(possible_browsers[0]); i++ ) {
                if ( ! strcmp( possible_browsers[ i ].name, string ) ) {
                        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "browsercommand")),
                                            possible_browsers[ i ].executable_name);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserterminal")),
                                                      possible_browsers[ i ].needs_term);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "browserremote")),
                                                      possible_browsers[ i ].nremote);
                        return;
                }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );*/
}

void
set_selected_editor( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "editorselect");
        
        if (!string)
                return;

        for ( i = 0; i < sizeof(possible_editors) / sizeof(possible_editors[0]); i++ ) {
                if ( ! strcmp( possible_editors[ i ].name, string ) ) {
                        gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "editorcommand")),
                                            possible_editors[ i ].executable_name);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorterminal")),
                                                      possible_editors[ i ].needs_term);
                        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_object_get_data (GTK_OBJECT (capplet), "editorlineno")),
                                                      possible_editors[ i ].accepts_lineno);
                        return;
                }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string ); */
}

void
set_selected_help( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "helpselect");
        
        if (!string)
                return;

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
                                return;
                        }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );*/
}

void
set_selected_terminal( gchar *string )
{
        gint i;
        GtkWidget *combo = gtk_object_get_data (GTK_OBJECT (capplet), "termselect");
        
        if (!string)
                return;

        for ( i = 0;
              i < sizeof(possible_terminals) / sizeof(possible_terminals[0]);
              i++ ) {
                if ( ! strcmp( possible_terminals[ i ].name, string ) )
                        {
                                gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termcommand")),
                                                    possible_terminals[ i ].executable_name);
                                gtk_entry_set_text (GTK_ENTRY (gtk_object_get_data (GTK_OBJECT (capplet), "termexec")),
                                                    possible_terminals[ i ].exec_app);
                                return;
                        }
        }
        /*        gtk_entry_set_text( GTK_ENTRY(GTK_COMBO(combo)->entry), string );*/
}


