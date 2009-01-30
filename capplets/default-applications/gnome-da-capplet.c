/*
 *  Authors: Luca Cavalli <loopback@slackit.org>
 *
 *  Copyright 2005-2006 Luca Cavalli
 *  Copyright 2008 Thomas Wood <thos@gnome.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "gconf-property-editor.h"
#include "gnome-da-capplet.h"
#include "gnome-da-xml.h"
#include "gnome-da-item.h"
#include "capplet-util.h"

enum
{
    PIXBUF_COL,
    TEXT_COL,
    N_COLUMNS
};

static void
close_cb (GtkWidget *window, gint response, gpointer user_data)
{
    if (response == GTK_RESPONSE_HELP) {
	capplet_help (GTK_WINDOW (window), "prefs-preferredapps");
    }
    else {
	gtk_widget_destroy (window);
	gtk_main_quit ();
    }
}

static void
set_icon (GtkImage *image, GtkIconTheme *theme, const char *name)
{
    GdkPixbuf *pixbuf;

    if ((pixbuf = gtk_icon_theme_load_icon (theme, name, 48, 0, NULL))) {
	gtk_image_set_from_pixbuf (image, pixbuf);
	g_object_unref (pixbuf);
    }
}

static void
web_radiobutton_toggled_cb (GtkWidget *togglebutton, GnomeDACapplet *capplet)
{
    gint index;
    GnomeDAWebItem *item;
    gchar *command;
    GError *error = NULL;

    index = gtk_combo_box_get_active (GTK_COMBO_BOX (capplet->web_combo_box));

    if (index == -1)
	return;

    item = (GnomeDAWebItem *) g_list_nth_data (capplet->web_browsers, index);
    if (item == NULL)
        return;

    if (togglebutton == capplet->new_win_radiobutton) {
	command = item->win_command;
    }
    else if (togglebutton == capplet->new_tab_radiobutton) {
	command = item->tab_command;
    }
    else {
	command = item->generic.command;
    }

    gconf_client_set_string (capplet->gconf, DEFAULT_APPS_KEY_HTTP_EXEC, command, &error);

    gtk_entry_set_text (GTK_ENTRY (capplet->web_browser_command_entry), command);

    if (error != NULL) {
	g_warning (_("Error saving configuration: %s"), error->message);
	g_error_free (error);
    }
}

static void
web_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;
    gboolean is_custom_active;
    gboolean has_net_remote;
    GnomeDAWebItem *item;
    GtkWidget *active = NULL;

    current_index = gtk_combo_box_get_active (combo);

    if (current_index < g_list_length (capplet->web_browsers)) {

	item = (GnomeDAWebItem*) g_list_nth_data (capplet->web_browsers, current_index);
	has_net_remote = item->netscape_remote;
	is_custom_active = FALSE;

    }
    else {
        has_net_remote = FALSE;
        is_custom_active = TRUE;
    }
    gtk_widget_set_sensitive (capplet->default_radiobutton, has_net_remote);
    gtk_widget_set_sensitive (capplet->new_win_radiobutton, has_net_remote);
    gtk_widget_set_sensitive (capplet->new_tab_radiobutton, has_net_remote);

    gtk_widget_set_sensitive (capplet->web_browser_command_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->web_browser_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->web_browser_terminal_checkbutton, is_custom_active);

    if (has_net_remote) {

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (capplet->new_win_radiobutton)))
            active = capplet->new_win_radiobutton;
        else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (capplet->new_tab_radiobutton)))
            active = capplet->new_tab_radiobutton;
        else
            active = capplet->default_radiobutton;
    }

    web_radiobutton_toggled_cb (active, capplet);
}

/* FIXME: Refactor these two functions below into one... */
static void
mail_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;
    gboolean is_custom_active;

    current_index = gtk_combo_box_get_active (combo);
    is_custom_active = (current_index >= g_list_length (capplet->mail_readers));

    gtk_widget_set_sensitive (capplet->mail_reader_command_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->mail_reader_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->mail_reader_terminal_checkbutton, is_custom_active);
}

static void
media_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;
    gboolean is_custom_active;

    current_index = gtk_combo_box_get_active (combo);
    is_custom_active = (current_index >= g_list_length (capplet->media_players));

    gtk_widget_set_sensitive (capplet->media_player_command_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->media_player_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->media_player_terminal_checkbutton, is_custom_active);
}

static void
terminal_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;
    gboolean is_custom_active;

    current_index = gtk_combo_box_get_active (combo);
    is_custom_active = (current_index >= g_list_length (capplet->terminals));

    gtk_widget_set_sensitive (capplet->terminal_command_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_exec_flag_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_exec_flag_label, is_custom_active);
}

static void
visual_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;
    gboolean is_custom_active;

    current_index = gtk_combo_box_get_active (combo);
    is_custom_active = (current_index >= g_list_length (capplet->visual_ats));

    gtk_widget_set_sensitive (capplet->visual_command_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->visual_command_label, is_custom_active);
}

static void
mobility_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;
    gboolean is_custom_active;

    current_index = gtk_combo_box_get_active (combo);
    is_custom_active = (current_index >= g_list_length (capplet->mobility_ats));

    gtk_widget_set_sensitive (capplet->mobility_command_entry, is_custom_active);
    gtk_widget_set_sensitive (capplet->mobility_command_label, is_custom_active);
}

static void
refresh_combo_box_icons (GtkIconTheme *theme, GtkComboBox *combo_box, GList *app_list)
{
    GList *entry;
    GnomeDAItem *item;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GdkPixbuf *pixbuf;

    for (entry = app_list; entry != NULL; entry = g_list_next (entry)) {
	item = (GnomeDAItem *) entry->data;

	model = gtk_combo_box_get_model (combo_box);

	if (item->icon_path && gtk_tree_model_get_iter_from_string (model, &iter, item->icon_path)) {
	    pixbuf = gtk_icon_theme_load_icon (theme, item->icon_name, 22, 0, NULL);

	    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				PIXBUF_COL, pixbuf,
				-1);

	    if (pixbuf)
		g_object_unref (pixbuf);
	}
    }
}

static struct {
    const gchar *name;
    const gchar *icon;
} icons[] = {
    { "web_browser_image", "web-browser"      },
    { "mail_reader_image", "emblem-mail"  },
    { "media_player_image", "applications-multimedia"     },
    { "visual_image",      "zoom-best-fit" },
    { "mobility_image",    "preferences-desktop-accessibility" },
/*    { "messenger_image",   "im"               },
 *    { "image_image",       "image-viewer"     },
 *    { "video_image",       "gnome-multimedia" },
 *    { "text_image",        "text-editor"      }, */
    { "terminal_image",    "gnome-terminal"   }
};

static void
theme_changed_cb (GtkIconTheme *theme, GnomeDACapplet *capplet)
{
    GtkWidget *icon;
    gint i;

    for (i = 0; i < G_N_ELEMENTS (icons); i++) {
	icon = glade_xml_get_widget (capplet->xml, icons[i].name);
	set_icon (GTK_IMAGE (icon), theme, icons[i].icon);
    }

    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->web_combo_box), capplet->web_browsers);
    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->mail_combo_box), capplet->mail_readers);
    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->media_combo_box), capplet->media_players);
    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->term_combo_box), capplet->terminals);
    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->visual_combo_box), capplet->visual_ats);
    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->mobility_combo_box), capplet->mobility_ats);
}

static void
screen_changed_cb (GtkWidget *widget, GdkScreen *screen, GnomeDACapplet *capplet)
{
    GtkIconTheme *theme;

    theme = gtk_icon_theme_get_for_screen (screen);

    if (capplet->icon_theme != NULL) {
	g_signal_handlers_disconnect_by_func (capplet->icon_theme, theme_changed_cb, capplet);
    }
    g_signal_connect (theme, "changed", G_CALLBACK (theme_changed_cb), capplet);
    theme_changed_cb (theme, capplet);

    capplet->icon_theme = theme;
}

static gint
generic_item_comp (gconstpointer list_item, gconstpointer command)
{
    return (strcmp (((GnomeDAItem *) list_item)->command, (gchar *) command));
}

static gint
web_item_comp (gconstpointer item, gconstpointer command)
{
    GnomeDAWebItem *web_list_item;

    web_list_item = (GnomeDAWebItem *) item;

    if (strcmp (web_list_item->generic.command, (gchar *) command) == 0)
	return 0;

    if (web_list_item->netscape_remote) {
	if (strcmp (web_list_item->tab_command, (gchar *) command) == 0)
	    return 0;

	if (strcmp (web_list_item->win_command, (gchar *) command) == 0)
	    return 0;
    }

    return (strcmp (web_list_item->generic.command, (gchar *) command));
}

static void
web_gconf_changed_cb (GConfPropertyEditor *peditor, gchar *key, GConfValue *value, GnomeDACapplet *capplet)
{
    GConfChangeSet *cs;
    GError *error = NULL;
    GList *list_entry;

    /* This function is used to update HTTPS,ABOUT and UNKNOWN handlers, which
     * should also use the same value as HTTP
     */

    if (strcmp (key, DEFAULT_APPS_KEY_HTTP_EXEC) == 0) {
	gchar *short_browser, *pos;
	const gchar *value_str = gconf_value_get_string (value);

	cs = gconf_change_set_new ();

	gconf_change_set_set (cs, DEFAULT_APPS_KEY_HTTPS_EXEC, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_UNKNOWN_EXEC, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_ABOUT_EXEC, value);
	pos = strstr (value_str, " ");
	if (pos == NULL)
	    short_browser = g_strdup (value_str);
	else
	    short_browser = g_strndup (value_str, pos - value_str);
	gconf_change_set_set_string (cs, DEFAULT_APPS_KEY_BROWSER_EXEC, short_browser);
	g_free (short_browser);

	list_entry = g_list_find_custom (capplet->web_browsers,
					 value_str,
					 (GCompareFunc) web_item_comp);

	if (list_entry) {
	    GnomeDAWebItem *item = (GnomeDAWebItem *) list_entry->data;

	    gconf_change_set_set_bool (cs, DEFAULT_APPS_KEY_BROWSER_NREMOTE, item->netscape_remote);
	}

	gconf_client_commit_change_set (capplet->gconf, cs, TRUE, &error);

	if (error != NULL) {
	    g_warning (_("Error saving configuration: %s"), error->message);
	    g_error_free (error);
	    error = NULL;
	}

	gconf_change_set_unref (cs);
    }
    else if (strcmp (key, DEFAULT_APPS_KEY_HTTP_NEEDS_TERM) == 0) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->web_browser_terminal_checkbutton),
				      gconf_value_get_bool (value));

	cs = gconf_change_set_new ();

	gconf_change_set_set (cs, DEFAULT_APPS_KEY_HTTPS_NEEDS_TERM, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_UNKNOWN_NEEDS_TERM, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_ABOUT_NEEDS_TERM, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_BROWSER_NEEDS_TERM, value);

	gconf_client_commit_change_set (capplet->gconf, cs, TRUE, &error);

	if (error != NULL) {
	    g_warning (_("Error saving configuration: %s"), error->message);
	    g_error_free (error);
	    error = NULL;
	}

	gconf_change_set_unref (cs);
    }
}

static void
web_browser_update_radio_buttons (GnomeDACapplet *capplet, const gchar *command)
{
    GList *entry;
    gboolean has_net_remote;

    entry = g_list_find_custom (capplet->web_browsers, command, (GCompareFunc) web_item_comp);

    if (entry) {
	GnomeDAWebItem *item = (GnomeDAWebItem *) entry->data;

	has_net_remote = item->netscape_remote;

	if (has_net_remote) {
	    /* disable "toggle" signal emitting, thus preventing calling this function twice */
	    g_signal_handlers_block_matched (capplet->default_radiobutton, G_SIGNAL_MATCH_FUNC, 0,
					     0, NULL, G_CALLBACK (web_radiobutton_toggled_cb), NULL);
	    g_signal_handlers_block_matched (capplet->new_tab_radiobutton, G_SIGNAL_MATCH_FUNC, 0,
					     0, NULL, G_CALLBACK (web_radiobutton_toggled_cb), NULL);
	    g_signal_handlers_block_matched (capplet->new_win_radiobutton,G_SIGNAL_MATCH_FUNC, 0,
					     0, NULL, G_CALLBACK (web_radiobutton_toggled_cb), NULL);

	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->default_radiobutton),
					  strcmp (item->generic.command, command) == 0);
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->new_tab_radiobutton),
					  strcmp (item->tab_command, command) == 0);
	    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->new_win_radiobutton),
					  strcmp (item->win_command, command) == 0);

	    g_signal_handlers_unblock_matched (capplet->default_radiobutton, G_SIGNAL_MATCH_FUNC, 0,
					       0, NULL, G_CALLBACK (web_radiobutton_toggled_cb), NULL);
	    g_signal_handlers_unblock_matched (capplet->new_tab_radiobutton, G_SIGNAL_MATCH_FUNC, 0,
					       0, NULL, G_CALLBACK (web_radiobutton_toggled_cb), NULL);
	    g_signal_handlers_unblock_matched (capplet->new_win_radiobutton, G_SIGNAL_MATCH_FUNC, 0,
					       0, NULL, G_CALLBACK (web_radiobutton_toggled_cb), NULL);
	}
    }
    else {
	has_net_remote = FALSE;
    }

    gtk_widget_set_sensitive (capplet->default_radiobutton, has_net_remote);
    gtk_widget_set_sensitive (capplet->new_win_radiobutton, has_net_remote);
    gtk_widget_set_sensitive (capplet->new_tab_radiobutton, has_net_remote);
}

static GConfValue*
web_combo_conv_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
    GConfValue *ret;
    GList *entry, *handlers;
    const gchar *command;
    gint index;
    GnomeDACapplet *capplet;

    g_object_get (G_OBJECT (peditor), "data", &capplet, NULL);

    command = gconf_value_get_string (value);
    handlers = capplet->web_browsers;

    if (handlers)
    {
      entry = g_list_find_custom (handlers, command, (GCompareFunc) web_item_comp);
      if (entry)
          index = g_list_position (handlers, entry);
      else
          index = g_list_length (handlers) + 1;
    }
    else
    {
      /* if the item has no handlers lsit then select the Custom item */
      index = 1;
    }

    web_browser_update_radio_buttons (capplet, command);

    ret = gconf_value_new (GCONF_VALUE_INT);
    gconf_value_set_int (ret, index);

    return ret;
}

static GConfValue*
web_combo_conv_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
    GConfValue *ret;
    GList *handlers;
    gint index;
    GnomeDAWebItem *item;
    const gchar *command;
    GnomeDACapplet *capplet;

    g_object_get (G_OBJECT (peditor), "data", &capplet, NULL);

    index = gconf_value_get_int (value);
    handlers = capplet->web_browsers;

    item = g_list_nth_data (handlers, index);

    ret = gconf_value_new (GCONF_VALUE_STRING);
    if (!item)
    {
        /* if item was not found, this is probably the "Custom" item */

        /* XXX: returning "" as the value here is not ideal, but required to
         * prevent the combo box from jumping back to the previous value if the
         * user has selected Custom */
        gconf_value_set_string (ret, "");
        return ret;
    }
    else
    {
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (capplet->new_win_radiobutton)) && item->netscape_remote == TRUE)
            command = item->win_command;
        else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (capplet->new_tab_radiobutton)) && item->netscape_remote == TRUE)
            command = item->tab_command;
        else
            command = item->generic.command;

        gconf_value_set_string (ret, command);
        return ret;
    }
}

static GConfValue*
combo_conv_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
    GConfValue *ret;
    GList *entry, *handlers;
    const gchar *command;
    gint index;

    g_object_get (G_OBJECT (peditor), "data", &handlers, NULL);

    command = gconf_value_get_string (value);

    if (handlers)
    {
        entry = g_list_find_custom (handlers, command, (GCompareFunc) generic_item_comp);
        if (entry)
            index = g_list_position (handlers, entry);
        else
            index = g_list_length (handlers) + 1;
    }
    else
    {
        /* if the item has no handlers lsit then select the Custom item */
        index = 1;
    }

    ret = gconf_value_new (GCONF_VALUE_INT);
    gconf_value_set_int (ret, index);
    return ret;
}

static GConfValue*
combo_conv_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
    GConfValue *ret;
    GList *handlers;
    gint index;
    GnomeDAItem *item;

    g_object_get (G_OBJECT (peditor), "data", &handlers, NULL);
    index = gconf_value_get_int (value);

    item = g_list_nth_data (handlers, index);
    ret = gconf_value_new (GCONF_VALUE_STRING);

    if (!item)
    {
        /* if item was not found, this is probably the "Custom" item */

        /* XXX: returning "" as the value here is not ideal, but required to
         * prevent the combo box from jumping back to the previous value if the
         * user has selected Custom */
        gconf_value_set_string (ret, "");
        return ret;
    }
    else
    {
        gconf_value_set_string (ret, item->command);
        return ret;
    }
}

static gboolean
is_separator (GtkTreeModel *model, GtkTreeIter *iter, gpointer sep_index)
{
    GtkTreePath *path;
    gboolean result;

    path = gtk_tree_model_get_path (model, iter);
    result = gtk_tree_path_get_indices (path)[0] == GPOINTER_TO_INT (sep_index);
    gtk_tree_path_free (path);

    return result;
}

static void
fill_combo_box (GtkIconTheme *theme, GtkComboBox *combo_box, GList *app_list)
{
    GList *entry;
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    GdkPixbuf *pixbuf;

    if (theme == NULL) {
	theme = gtk_icon_theme_get_default ();
    }

    gtk_combo_box_set_row_separator_func (combo_box, is_separator,
					  GINT_TO_POINTER (g_list_length (app_list)), NULL);

    model = GTK_TREE_MODEL (gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING));
    gtk_combo_box_set_model (combo_box, model);

    renderer = gtk_cell_renderer_pixbuf_new ();

    /* not all cells have a pixbuf, this prevents the combo box to shrink */
    gtk_cell_renderer_set_fixed_size (renderer, -1, 22);
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
				    "pixbuf", PIXBUF_COL,
				    NULL);

    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
				    "text", TEXT_COL,
				    NULL);

    for (entry = app_list; entry != NULL; entry = g_list_next (entry)) {
	GnomeDAItem *item;
	item = (GnomeDAItem *) entry->data;

	pixbuf = gtk_icon_theme_load_icon (theme, item->icon_name, 22, 0, NULL);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    PIXBUF_COL, pixbuf,
			    TEXT_COL, item->name,
			    -1);

	item->icon_path = gtk_tree_model_get_string_from_iter (model, &iter);

	if (pixbuf)
	    g_object_unref (pixbuf);
    }

    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, -1);
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);
    gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			PIXBUF_COL, NULL,
			TEXT_COL, _("Custom"),
			-1);
}

static void
show_dialog (GnomeDACapplet *capplet, const gchar *start_page)
{
    GObject *obj;

    if (g_file_test (GNOMECC_GLADE_DIR "/gnome-default-applications-properties.glade", G_FILE_TEST_EXISTS) != FALSE) {
	capplet->xml = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-default-applications-properties.glade", NULL, NULL);
    }
    else {
	capplet->xml = glade_xml_new ("./gnome-default-applications-properties.glade", NULL, NULL);
    }

    if (capplet->xml == NULL) {
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					 _("Could not load the main interface"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Please make sure that the applet "
						    "is properly installed"));
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	exit (EXIT_FAILURE);
    }

    capplet->window = glade_xml_get_widget (capplet->xml, "preferred_apps_dialog");
    g_signal_connect (capplet->window, "response", G_CALLBACK (close_cb), NULL);

    capplet->web_browser_command_entry = glade_xml_get_widget (capplet->xml, "web_browser_command_entry");
    capplet->web_browser_command_label = glade_xml_get_widget (capplet->xml, "web_browser_command_label");
    capplet->web_browser_terminal_checkbutton = glade_xml_get_widget (capplet->xml, "web_browser_terminal_checkbutton");
    capplet->default_radiobutton = glade_xml_get_widget (capplet->xml, "web_browser_default_radiobutton");
    capplet->new_win_radiobutton = glade_xml_get_widget (capplet->xml, "web_browser_new_win_radiobutton");
    capplet->new_tab_radiobutton = glade_xml_get_widget (capplet->xml, "web_browser_new_tab_radiobutton");

    capplet->mail_reader_command_entry = glade_xml_get_widget (capplet->xml, "mail_reader_command_entry");
    capplet->mail_reader_command_label = glade_xml_get_widget (capplet->xml, "mail_reader_command_label");
    capplet->mail_reader_terminal_checkbutton = glade_xml_get_widget (capplet->xml, "mail_reader_terminal_checkbutton");

    capplet->terminal_command_entry = glade_xml_get_widget (capplet->xml, "terminal_command_entry");
    capplet->terminal_command_label = glade_xml_get_widget (capplet->xml, "terminal_command_label");
    capplet->terminal_exec_flag_entry = glade_xml_get_widget (capplet->xml, "terminal_exec_flag_entry");
    capplet->terminal_exec_flag_label = glade_xml_get_widget (capplet->xml, "terminal_exec_flag_label");

    capplet->media_player_command_entry = glade_xml_get_widget (capplet->xml, "media_player_command_entry");
    capplet->media_player_command_label = glade_xml_get_widget (capplet->xml, "media_player_command_label");
    capplet->media_player_terminal_checkbutton = glade_xml_get_widget (capplet->xml, "media_player_terminal_checkbutton");

    capplet->visual_command_entry = glade_xml_get_widget (capplet->xml, "visual_command_entry");
    capplet->visual_command_label = glade_xml_get_widget (capplet->xml, "visual_command_label");
    capplet->visual_startup_checkbutton = glade_xml_get_widget (capplet->xml, "visual_start_checkbutton");

    capplet->mobility_command_entry = glade_xml_get_widget (capplet->xml, "mobility_command_entry");
    capplet->mobility_command_label = glade_xml_get_widget (capplet->xml, "mobility_command_label");
    capplet->mobility_startup_checkbutton = glade_xml_get_widget (capplet->xml, "mobility_start_checkbutton");

    capplet->web_combo_box = glade_xml_get_widget (capplet->xml, "web_browser_combobox");
    capplet->mail_combo_box = glade_xml_get_widget (capplet->xml, "mail_reader_combobox");
    capplet->term_combo_box = glade_xml_get_widget (capplet->xml, "terminal_combobox");
    capplet->media_combo_box = glade_xml_get_widget (capplet->xml, "media_player_combobox");
    capplet->visual_combo_box = glade_xml_get_widget (capplet->xml, "visual_combobox");
    capplet->mobility_combo_box = glade_xml_get_widget (capplet->xml, "mobility_combobox");

    g_signal_connect (capplet->window, "screen-changed", G_CALLBACK (screen_changed_cb), capplet);
    screen_changed_cb (capplet->window, gdk_screen_get_default (), capplet);

    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->web_combo_box), capplet->web_browsers);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->mail_combo_box), capplet->mail_readers);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->term_combo_box), capplet->terminals);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->media_combo_box), capplet->media_players);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->visual_combo_box), capplet->visual_ats);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->mobility_combo_box), capplet->mobility_ats);

    g_signal_connect (capplet->web_combo_box, "changed", G_CALLBACK (web_combo_changed_cb), capplet);
    g_signal_connect (capplet->mail_combo_box, "changed", G_CALLBACK (mail_combo_changed_cb), capplet);
    g_signal_connect (capplet->term_combo_box, "changed", G_CALLBACK (terminal_combo_changed_cb), capplet);
    g_signal_connect (capplet->media_combo_box, "changed", G_CALLBACK (media_combo_changed_cb), capplet);
    g_signal_connect (capplet->visual_combo_box, "changed", G_CALLBACK (visual_combo_changed_cb), capplet);
    g_signal_connect (capplet->mobility_combo_box, "changed", G_CALLBACK (mobility_combo_changed_cb), capplet);


    g_signal_connect (capplet->default_radiobutton, "toggled", G_CALLBACK (web_radiobutton_toggled_cb), capplet);
    g_signal_connect (capplet->new_win_radiobutton, "toggled", G_CALLBACK (web_radiobutton_toggled_cb), capplet);
    g_signal_connect (capplet->new_tab_radiobutton, "toggled", G_CALLBACK (web_radiobutton_toggled_cb), capplet);

    /* Setup GConfPropertyEditors */

    /* Web Browser */
    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_HTTP_EXEC,
        capplet->web_combo_box,
        "conv-from-widget-cb", web_combo_conv_from_widget,
        "conv-to-widget-cb", web_combo_conv_to_widget,
        "data", capplet,
        NULL);

    obj = gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_HTTP_EXEC,
        capplet->web_browser_command_entry,
        NULL);
    g_signal_connect (obj, "value-changed", G_CALLBACK (web_gconf_changed_cb), capplet);

    obj = gconf_peditor_new_boolean (NULL,
        DEFAULT_APPS_KEY_HTTP_NEEDS_TERM,
        capplet->web_browser_terminal_checkbutton,
        NULL);
    g_signal_connect (obj, "value-changed", G_CALLBACK (web_gconf_changed_cb), capplet);

    /* Mailer */
    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_MAILER_EXEC,
        capplet->mail_combo_box,
        "conv-from-widget-cb", combo_conv_from_widget,
        "conv-to-widget-cb", combo_conv_to_widget,
        "data", capplet->mail_readers,
        NULL);

    gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_MAILER_EXEC,
        capplet->mail_reader_command_entry,
        NULL);

    gconf_peditor_new_boolean (NULL,
        DEFAULT_APPS_KEY_MAILER_NEEDS_TERM,
        capplet->mail_reader_terminal_checkbutton,
        NULL);

    /* Media player */
    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_MEDIA_EXEC,
        capplet->media_combo_box,
        "conv-from-widget-cb", combo_conv_from_widget,
        "conv-to-widget-cb", combo_conv_to_widget,
        "data", capplet->media_players,
        NULL);

    gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_MEDIA_EXEC,
        capplet->media_player_command_entry,
        NULL);

    gconf_peditor_new_boolean (NULL,
        DEFAULT_APPS_KEY_MEDIA_NEEDS_TERM,
        capplet->media_player_terminal_checkbutton,
        NULL);

    /* Terminal */
    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_TERMINAL_EXEC,
        capplet->term_combo_box,
        "conv-from-widget-cb", combo_conv_from_widget,
        "conv-to-widget-cb", combo_conv_to_widget,
        "data", capplet->terminals,
        NULL);

    gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_TERMINAL_EXEC,
        capplet->terminal_command_entry,
        NULL);
    gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG,
        capplet->terminal_exec_flag_entry,
        NULL);


    /* Visual */
    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_VISUAL_EXEC,
        capplet->visual_combo_box,
        "conv-from-widget-cb", combo_conv_from_widget,
        "conv-to-widget-cb", combo_conv_to_widget,
        "data", capplet->visual_ats,
        NULL);

    gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_VISUAL_EXEC,
        capplet->visual_command_entry,
        NULL);

    gconf_peditor_new_boolean (NULL,
        DEFAULT_APPS_KEY_VISUAL_STARTUP,
        capplet->visual_startup_checkbutton,
        NULL);


    /* Mobility */
    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_MOBILITY_EXEC,
        capplet->mobility_combo_box,
        "conv-from-widget-cb", combo_conv_from_widget,
        "conv-to-widget-cb", combo_conv_to_widget,
        "data", capplet->mobility_ats,
        NULL);

    gconf_peditor_new_string (NULL,
        DEFAULT_APPS_KEY_MOBILITY_EXEC,
        capplet->mobility_command_entry,
        NULL);

    gconf_peditor_new_boolean (NULL,
        DEFAULT_APPS_KEY_MOBILITY_STARTUP,
        capplet->mobility_startup_checkbutton,
        NULL);

    gtk_window_set_icon_name (GTK_WINDOW (capplet->window),
			      "gnome-settings-default-applications");

    if (start_page != NULL) {
        gchar *page_name;
        GtkWidget *w;

        page_name = g_strconcat (start_page, "_vbox", NULL);

        w = glade_xml_get_widget (capplet->xml, page_name);
        if (w != NULL) {
            GtkNotebook *nb;
            gint pindex;

            nb = GTK_NOTEBOOK (glade_xml_get_widget (capplet->xml, "preferred_apps_notebook"));
            pindex = gtk_notebook_page_num (nb, w);
            if (pindex != -1)
                gtk_notebook_set_current_page (nb, pindex);
        }
        g_free (page_name);
    }

    gtk_widget_show (capplet->window);
}

int
main (int argc, char **argv)
{
    GnomeDACapplet *capplet;

    gchar *start_page = NULL;
    GOptionContext *context;
    GOptionEntry option_entries[] = {
        { "show-page",
          'p',
          G_OPTION_FLAG_IN_MAIN,
          G_OPTION_ARG_STRING,
          &start_page,
          /* TRANSLATORS: don't translate the terms in brackets */
          N_("Specify the name of the page to show (internet|multimedia|system|a11y)"),
          N_("page") },
        { NULL }
    };

    context = g_option_context_new (_("- GNOME Default Applications"));
    g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);

    capplet_init (context, &argc, &argv);

    glade_init ();

    capplet = g_new0 (GnomeDACapplet, 1);
    capplet->gconf = gconf_client_get_default ();
    gnome_da_xml_load_list (capplet);

    show_dialog (capplet, start_page);
    g_free (start_page);

    gtk_main ();

    g_object_unref (capplet->gconf);

    gnome_da_xml_free (capplet);

    return 0;
}
