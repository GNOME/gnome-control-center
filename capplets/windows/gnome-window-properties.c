/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998-1999 Redhat Software Inc.
 * Code available under the Gnu GPL.
 * Authors: Jonathan Blandford <jrb@redhat.com>
 *          Owen Taylor <otaylor@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <ctype.h>

#include <gmodule.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"

#include <gnome-wm-manager.h>

#define THEME_KEY                "/desktop/gnome/applications/window_manager/theme"
#define TITLEBAR_FONT_KEY        "/desktop/gnome/applications/window_manager/titlebar_font"
#define FOCUS_FOLLOWS_MOUSE_KEY  "/desktop/gnome/applications/window_manager/focus_follows_mouse"

GnomeClient *client = NULL;

/* structures */

typedef struct {
        GtkWidget *dialog;
        GtkWidget *name_entry;
        GtkWidget *exec_entry;
        GtkWidget *config_entry;
        GtkWidget *sm_toggle;
} WMDialog;

/* vars. */
static GtkWidget *wm_widget;
static GtkWidget *apply_now_button;
static GtkWidget *properties_box;

static void state_changed (void);

static GtkWidget *wm_menu;
static GtkWidget *option_menu;
static GList *wm_menu_window_managers;

GnomeWindowManager *selected_wm;

gboolean in_fill;

static void
wm_selection_changed (GtkOptionMenu *option_menu, gpointer data)
{
        int index;
        GnomeWindowManager *wm;

        index = gtk_option_menu_get_history (option_menu);
        wm = (GnomeWindowManager *) g_list_nth (wm_menu_window_managers, index)->data;

        if (!in_fill) {
                if (wm != selected_wm) {
                        selected_wm = wm;
			state_changed ();
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

        if (wm == selected_wm)
                gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), 0);
}

static void
set_wm_change_pending (gboolean pending)
{
        gtk_widget_set_sensitive (apply_now_button, pending);
        gtk_widget_set_sensitive (properties_box, !pending);
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
state_changed (void)
{
        set_wm_change_pending (TRUE);
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
setup_dialog (GladeXML *dialog)
{  
        GObject *peditor;

        peditor = gconf_peditor_new_font (NULL, TITLEBAR_FONT_KEY,
                                          WID ("titlebar_font"),
                                          PEDITOR_FONT_COMBINED, NULL);

        peditor = gconf_peditor_new_boolean (NULL, FOCUS_FOLLOWS_MOUSE_KEY,
                                             WID ("focus_follows_mouse"),
                                             NULL);
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

        dialog = create_dialog ();
        setup_dialog (dialog);
        
        dialog_win = gtk_dialog_new_with_buttons 
                (_("Window Preferences"), NULL, -1,
                 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                 NULL);
        g_signal_connect (G_OBJECT (dialog_win), "response", (GCallback)response_cb, NULL);
        
        gnome_wm_manager_init (dialog_win);
        update_gui ();
        
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
