/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/* background-properties-capplet.c
 * Copyright (C) 2002 Seth Nickell
 *
 * Written by: Seth Nickell <snickell@stanford.edu>
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
#   include <config.h>
#endif

#include <ctype.h>

#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <gnome-wm-manager.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"


#define THEME_KEY                "/desktop/gnome/applications/window_manager/theme"
#define TITLEBAR_FONT_KEY        "/desktop/gnome/applications/window_manager/titlebar_font"
#define FOCUS_FOLLOWS_MOUSE_KEY  "/desktop/gnome/applications/window_manager/focus_follows_mouse"

static GtkWidget *wm_widget;
static GtkWidget *apply_now_button;
static GtkWidget *properties_box;
static GtkWidget *wm_menu;
static GtkWidget *option_menu;
static GList     *wm_menu_window_managers;

static GtkWidget *appearance_option_menu;
static GList     *theme_list;

static GnomeWindowManager *selected_wm;

static gboolean in_fill;

static GConfClient *gconf_client;

static void setup_appearance_option_menu (GtkWidget *appearance_option_menu, GnomeWindowManager *wm);

static void
set_wm_change_pending (gboolean pending)
{
        gtk_widget_set_sensitive (apply_now_button, pending);
        gtk_widget_set_sensitive (properties_box,  !pending);
}

static void
wm_selection_changed (GtkOptionMenu *option_menu, gpointer data)
{
        int index;
        GnomeWindowManager *wm;

        index = gtk_option_menu_get_history (option_menu);
        wm = (GnomeWindowManager *) g_list_nth (wm_menu_window_managers, index)->data;

        if (!in_fill) {
                if (!gnome_wm_manager_same_wm (wm, selected_wm)) {
                        selected_wm = wm;
			set_wm_change_pending (TRUE);
                }
        }
}

static void
wm_widget_clear ()
{
        wm_menu_window_managers = NULL;

        wm_menu = gtk_menu_new ();
        gtk_widget_show_all (wm_menu);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), wm_menu);
}

GtkWidget *
wm_widget_new ()
{
        option_menu = gtk_option_menu_new ();
        g_signal_connect (G_OBJECT (option_menu), "changed",
                          (GCallback)wm_selection_changed, NULL);

        wm_widget_clear ();

        gtk_widget_show_all (option_menu);
        return option_menu;
}

static void
wm_widget_add_wm (GnomeWindowManager *wm)
{
        GtkWidget *menu_item;
        const char *row_text;

        row_text = gnome_window_manager_get_name (wm);

        menu_item = gtk_menu_item_new_with_label (row_text);
        gtk_widget_show_all (menu_item);

        gtk_menu_shell_prepend (GTK_MENU_SHELL (wm_menu), menu_item);
        wm_menu_window_managers = g_list_prepend (wm_menu_window_managers, wm);

        /* If this is supposed to be the selected window manager, do so */
        if (gnome_wm_manager_same_wm (wm, selected_wm))
                gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 0);
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
	}
}

static void
update_gui (void)
{
        GList *tmp_list;
        GnomeWindowManager *wm;

        wm_widget_clear ();

        in_fill = TRUE;

        tmp_list = gnome_wm_manager_get_list ();

        printf ("got a list of %d wms\n", g_list_length (tmp_list));

        for (tmp_list = gnome_wm_manager_get_list (); tmp_list != NULL; tmp_list = tmp_list->next) {
                wm = tmp_list->data;
                wm_widget_add_wm (wm);
        }
        
        in_fill = FALSE;

}

static void
apply_wm (GObject *object, gpointer data)
{
        gnome_wm_manager_set_current (selected_wm);
        setup_appearance_option_menu (appearance_option_menu, selected_wm);
        set_wm_change_pending (FALSE);
}

static GladeXML *
create_dialog (void)
{
        GladeXML *dialog;
        
	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-window-properties.glade", "prefs_widget", NULL);
        
        apply_now_button = WID ("apply_now_button");
        g_signal_connect (G_OBJECT (apply_now_button), "clicked", (GCallback)apply_wm, NULL);        

        properties_box = WID ("properties_box");

        set_wm_change_pending (FALSE);

        wm_widget = wm_widget_new ();
        
        gtk_box_pack_start (GTK_BOX (WID ("wm_widget_box")), wm_widget, TRUE, TRUE, 0);
                            
        return dialog;
}

static void
setup_appearance_option_menu (GtkWidget *appearance_option_menu, GnomeWindowManager *wm)
{
        GtkWidget *menu, *menu_item;
        GList *themes, *node;
        char *theme_name;

        menu = gtk_menu_new ();
        gtk_widget_show_all (menu);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (appearance_option_menu), menu);        

        themes = gnome_window_manager_get_theme_list (wm);

        for (node = themes; node != NULL; node = node->next) {
                theme_name = (char *)node->data;

                menu_item = gtk_menu_item_new_with_label (theme_name);
                gtk_widget_show_all (menu_item);

                gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
        }

        theme_list = themes;
}

static void
appearance_changed (GtkOptionMenu *option_menu, gpointer data)
{
        int index;
        const char *theme_name;

        index = gtk_option_menu_get_history (option_menu);
        theme_name = (const char *) g_list_nth (theme_list, index)->data;

        printf ("Setting theme to %s\n", theme_name);
        
        gconf_client_set_string (gconf_client, THEME_KEY, theme_name, NULL);
}

static void
setup_dialog (GladeXML *dialog)
{  
        GObject *peditor;

        update_gui ();

        peditor = gconf_peditor_new_font (NULL, TITLEBAR_FONT_KEY,
                                          WID ("titlebar_font"),
                                          PEDITOR_FONT_COMBINED, NULL);

        peditor = gconf_peditor_new_boolean (NULL, FOCUS_FOLLOWS_MOUSE_KEY,
                                             WID ("focus_follows_mouse"),
                                             NULL);

        appearance_option_menu = WID ("window_border_appearance");
        setup_appearance_option_menu (appearance_option_menu, selected_wm);
        g_signal_connect (G_OBJECT (appearance_option_menu), "changed",
                          (GCallback)appearance_changed, NULL);
        gtk_widget_show_all (appearance_option_menu);
}



int
main (int argc, char **argv)
{
        GladeXML *dialog;
        GtkWidget *dialog_win;

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

  	gnome_program_init ("wm-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, 
			    NULL);

        gconf_client = gconf_client_get_default ();

        dialog_win = gtk_dialog_new_with_buttons 
                (_("Window Preferences"), NULL, -1,
                 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                 NULL);
        g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback)response_cb, NULL);
        
        gnome_wm_manager_init (dialog_win);
        selected_wm = gnome_wm_manager_get_current ();

        dialog = create_dialog ();
        setup_dialog (dialog);
        
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog_win)->vbox), WID ("prefs_widget"), TRUE, TRUE, GNOME_PAD_SMALL);
        gtk_widget_show_all (dialog_win);
        
        gtk_main ();
        
        /* FIXME: we need to handle this through the library somehow
           if (restart_pending) {
           quit_pending = TRUE;
           gtk_main();
           }
        */

        return 0;
}
