/* -*- MODE: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Author: Benjamin Kahn <xkahn@zoned.net>
 * Based on capplets/gnome-edit-properties/gnome-edit-properties.c.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "defaults.h"

extern GtkWidget *capplet;

extern BrowserDescription bcurrent_info;
extern EditorDescription ecurrent_info;
extern HelpViewDescription hcurrent_info;
extern TerminalDescription tcurrent_info;

extern int set_selected_terminal( gchar *string );
extern int set_selected_help( gchar *string );
extern int set_selected_editor( gchar *string );
extern int set_selected_browser( gchar *string );

void
on_radiodefeditor_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        gint a = GTK_TOGGLE_BUTTON (togglebutton)->active;
	int index = -1;
        /* Editor Custom */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "table17"), !a);
        /* Editor Default */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "editorselect"), a);

        if (a)
		index =	set_selected_editor (gtk_entry_get_text (gtk_object_get_data (GTK_OBJECT(capplet), "combo_editor")));

        ecurrent_info.index = index;

        edit_changed (togglebutton, user_data);

}


void
on_combo_editor_changed                (GtkEditable     *editable,
                                        gpointer         user_data)
{
        
        set_selected_editor (gtk_entry_get_text (editable));

        edit_changed (editable, user_data);

}


void
on_radiocusteditor_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        edit_changed (togglebutton, user_data);

}


void
on_editorterminal_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        edit_changed (togglebutton, user_data);

}


void
on_editorlineno_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        edit_changed (togglebutton, user_data);

}


void
on_editorcommand_changed               (GtkEditable     *editable,
                                        gpointer         user_data)
{
        edit_changed (editable, user_data);

}


void
on_seldefbrowser_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        gint a = GTK_TOGGLE_BUTTON (togglebutton)->active;
	int index = -1;
        /* Browser Custom */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "table20"), !a);
        /* Browser Default */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "browserselect"), a);        

        if (a)
        	index = set_selected_browser (gtk_entry_get_text (gtk_object_get_data (GTK_OBJECT(capplet), "combo_browser")));        

        bcurrent_info.index = index;

        edit_changed (togglebutton, user_data);
}


void
on_combo_browser_changed               (GtkEditable     *editable,
                                        gpointer         user_data)
{
        
        set_selected_browser (gtk_entry_get_text (editable));

        edit_changed (editable, user_data);

}


void
on_selcustbrowser_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        edit_changed (togglebutton, user_data);

}


void
on_browserterminal_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        edit_changed (togglebutton, user_data);

}


void
on_browserremote_toggled               (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

        edit_changed (togglebutton, user_data);
}


void
on_browsercommand_changed              (GtkEditable     *editable,
                                        gpointer         user_data)
{
        edit_changed (editable, user_data);

}


void
on_seldefview_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        gint a = GTK_TOGGLE_BUTTON (togglebutton)->active;
	int index = -1;
        /* Help Custom */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "table23"), !a);
        /* Help Default */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "helpselect"), a);

	g_print ("view toggled\n");
        if (a)
        	index =set_selected_help (gtk_entry_get_text (gtk_object_get_data (GTK_OBJECT(capplet), "combo_help")));

        hcurrent_info.index = index;

        edit_changed (togglebutton, user_data);
}


void
on_combo_help_changed                  (GtkEditable     *editable,
                                        gpointer         user_data)
{
        set_selected_help (gtk_entry_get_text (editable)); 
        
        edit_changed (editable, user_data);

}


void
on_selcustview_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

        edit_changed (togglebutton, user_data);
}


void
on_helpterminal_toggled                (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

        edit_changed (togglebutton, user_data);
}


void
on_helpurls_toggled                    (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{

        edit_changed (togglebutton, user_data);
}


void
on_helpcommand_changed                 (GtkEditable     *editable,
                                        gpointer         user_data)
{
        edit_changed (editable, user_data);

}


void
on_seldefterm_toggled                  (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        gint a = GTK_TOGGLE_BUTTON (togglebutton)->active;
	int index = -1;
        /* Terminal Custom */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "table26"), !a);
        /* Terminal Default */
        gtk_widget_set_sensitive(gtk_object_get_data (GTK_OBJECT (capplet), "termselect"), a);
        
        if (a)
        	index = set_selected_terminal (gtk_entry_get_text (gtk_object_get_data (GTK_OBJECT(capplet), "combo_term")));

        tcurrent_info.index = index;

        edit_changed (togglebutton, user_data);
        
}


void
on_combo_term_changed                  (GtkEditable     *editable,
                                        gpointer         user_data)
{
        set_selected_terminal (gtk_entry_get_text (editable));

        edit_changed (editable, user_data);
}


void
on_selcustterm_toggled                 (GtkToggleButton *togglebutton,
                                        gpointer         user_data)
{
        edit_changed (togglebutton, user_data);

}


void
on_termexec_changed                    (GtkEditable     *editable,
                                        gpointer         user_data)
{
        edit_changed (editable, user_data);

}


void
on_termcommand_changed                 (GtkEditable     *editable,
                                        gpointer         user_data)
{

        edit_changed (editable, user_data);
}

