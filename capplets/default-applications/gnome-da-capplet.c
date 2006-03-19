/*
 *  Authors: Luca Cavalli <loopback@slackit.org>
 *
 *  Copyright 2005-2006 Luca Cavalli
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
#include <gnome.h>
#include <glib/gi18n.h>

#include "gnome-da-capplet.h"
#include "gnome-da-xml.h"
#include "gnome-da-item.h"

/* TODO: it doesn't use GConfPropertyEditor, use it when/if moved to control-center */

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
	GError *error = NULL;

	/* capplet_help (GTK_WINDOW (dialog), "user-guide.xml", "goscustdoc-2"); */
	gnome_help_display_desktop (NULL, "user-guide", "user-guide.xml",
				    "goscustdoc-2", &error);
	if (error != NULL) {
	    GtkWidget *dialog;

	    dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					     _("Could not display help"));
	    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						      _("Please make sure that the applet "
							"is properly installed"));
	    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	    gtk_dialog_run (GTK_DIALOG (dialog));
	    gtk_widget_destroy (dialog);
	}
    }
    else {
	gtk_widget_destroy (window);
	gtk_main_quit ();
    }
}

static gboolean
entry_focus_out_event_cb (GtkWidget *widget, GdkEventFocus *event, GnomeDACapplet *capplet)
{
    const gchar *text;
    GError *error = NULL;

    text = gtk_entry_get_text (GTK_ENTRY (widget));

    if (widget == capplet->web_browser_command_entry) {
	gconf_client_set_string (capplet->gconf, DEFAULT_APPS_KEY_HTTP_EXEC, text, &error);
    }
    else if (widget == capplet->mail_reader_command_entry) {
	gconf_client_set_string (capplet->gconf, DEFAULT_APPS_KEY_MAILER_EXEC, text, &error);
    }
    else if (widget == capplet->terminal_command_entry) {
	gconf_client_set_string (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_EXEC, text, &error);
    }
    else if (widget == capplet->terminal_exec_flag_entry) {
	gconf_client_set_string (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG, text, &error);
    }

    if (error != NULL) {
	g_warning (_("Error saving configuration: %s"), error->message);
	g_error_free (error);
	error = NULL;
    }

    return FALSE;
}

static void
terminal_checkbutton_toggled_cb (GtkToggleButton *togglebutton, GnomeDACapplet *capplet)
{
    gboolean is_active;
    GError *error = NULL;

    is_active = gtk_toggle_button_get_active (togglebutton);

    if (GTK_WIDGET (togglebutton) == capplet->web_browser_terminal_checkbutton) {
	gconf_client_set_bool (capplet->gconf, DEFAULT_APPS_KEY_HTTP_NEEDS_TERM, is_active, &error);
    }
    else if (GTK_WIDGET (togglebutton) == capplet->mail_reader_terminal_checkbutton) {
	gconf_client_set_bool (capplet->gconf, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM, is_active, &error);
    }

    if (error != NULL) {
	g_warning (_("Error saving configuration: %s"), error->message);
	g_error_free (error);
	error = NULL;
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
web_radiobutton_toggled_cb (GtkToggleButton *togglebutton, GnomeDACapplet *capplet)
{
    gint index;
    GnomeDAWebItem *item;
    gchar *command;
    GError *error = NULL;

    index = gtk_combo_box_get_active (GTK_COMBO_BOX (capplet->web_combo_box));

    if (index == -1)
	return;

    item = (GnomeDAWebItem*) g_list_nth_data (capplet->web_browsers, index);

    if (GTK_WIDGET (togglebutton) == capplet->new_win_radiobutton) {
	command = item->win_command;
    }
    else if (GTK_WIDGET (togglebutton) == capplet->new_tab_radiobutton) {
	command = item->tab_command;
    }
    else {
	command = item->generic.command;
    }

    gconf_client_set_string (capplet->gconf, DEFAULT_APPS_KEY_HTTP_EXEC, command, &error);

    if (error != NULL) {
	g_warning (_("Error saving configuration: %s"), error->message);
	g_error_free (error);
	error = NULL;
    }
}

static void
web_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    guint current_index;
    gboolean is_custom_active;
    gboolean has_net_remote;
    GnomeDAWebItem *item;
    GConfChangeSet *cs;
    GError *error = NULL;

    gtk_combo_box_get_active_iter (combo, &iter);
    path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo), &iter);
    current_index = gtk_tree_path_get_indices (path)[0];
    gtk_tree_path_free (path);

    if (current_index < g_list_length (capplet->web_browsers)) {
	gchar *command;

	item = (GnomeDAWebItem*) g_list_nth_data (capplet->web_browsers, current_index);
	has_net_remote = item->netscape_remote;
	is_custom_active = FALSE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (capplet->new_win_radiobutton)) && has_net_remote == TRUE)
	    command = item->win_command;
	else if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (capplet->new_tab_radiobutton)) && has_net_remote == TRUE)
	    command = item->tab_command;
	else
	    command = item->generic.command;

	cs = gconf_change_set_new ();

	gconf_change_set_set_string (cs, DEFAULT_APPS_KEY_HTTP_EXEC, command);
	gconf_change_set_set_bool (cs, DEFAULT_APPS_KEY_HTTP_NEEDS_TERM, item->run_in_terminal);

	gconf_client_commit_change_set (capplet->gconf, cs, TRUE, &error);

	if (error != NULL) {
	    g_warning (_("Error saving configuration: %s"), error->message);
	    g_error_free (error);
	    error = NULL;
	}

	gconf_change_set_unref (cs);
    }
    else {
	has_net_remote = FALSE;
	is_custom_active = TRUE;
    }

    gtk_entry_set_text (GTK_ENTRY (capplet->web_browser_command_entry),
			gconf_client_get_string (capplet->gconf, DEFAULT_APPS_KEY_HTTP_EXEC, NULL));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->web_browser_terminal_checkbutton),
				  gconf_client_get_bool (capplet->gconf, DEFAULT_APPS_KEY_HTTP_NEEDS_TERM, NULL));

    gtk_widget_set_sensitive (capplet->default_radiobutton, has_net_remote);
    gtk_widget_set_sensitive (capplet->new_win_radiobutton, has_net_remote);
    gtk_widget_set_sensitive (capplet->new_tab_radiobutton, has_net_remote);

    gtk_editable_set_editable (GTK_EDITABLE (capplet->web_browser_command_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->web_browser_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->web_browser_terminal_checkbutton, is_custom_active);
}

static void
mail_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    guint current_index;
    gboolean is_custom_active;
    GnomeDAMailItem *item;
    GConfChangeSet *cs;
    GError *error = NULL;

    gtk_combo_box_get_active_iter (combo, &iter);
    path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo), &iter);
    current_index = gtk_tree_path_get_indices (path)[0];
    gtk_tree_path_free (path);

    if (current_index < g_list_length (capplet->mail_readers)) {
	item = (GnomeDAMailItem*) g_list_nth_data (capplet->mail_readers, current_index);
	is_custom_active = FALSE;

	cs = gconf_change_set_new ();

	gconf_change_set_set_string (cs, DEFAULT_APPS_KEY_MAILER_EXEC, item->generic.command);
	gconf_change_set_set_bool (cs, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM, item->run_in_terminal);

	gconf_client_commit_change_set (capplet->gconf, cs, TRUE, &error);

	if (error != NULL) {
	    g_warning (_("Error saving configuration: %s"), error->message);
	    g_error_free (error);
	    error = NULL;
	}

	gconf_change_set_unref (cs);
    }
    else {
	is_custom_active = TRUE;
    }

    gtk_entry_set_text (GTK_ENTRY (capplet->mail_reader_command_entry),
			gconf_client_get_string (capplet->gconf, DEFAULT_APPS_KEY_MAILER_EXEC, NULL));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->mail_reader_terminal_checkbutton),
				  gconf_client_get_bool (capplet->gconf, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM, NULL));

    gtk_editable_set_editable (GTK_EDITABLE (capplet->mail_reader_command_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->mail_reader_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->mail_reader_terminal_checkbutton, is_custom_active);
}

static void
terminal_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    guint current_index;
    gboolean is_custom_active;
    GnomeDATermItem *item;
    GConfChangeSet *cs;
    GError *error = NULL;

    gtk_combo_box_get_active_iter (combo, &iter);
    path = gtk_tree_model_get_path (gtk_combo_box_get_model (combo), &iter);
    current_index = gtk_tree_path_get_indices (path)[0];
    gtk_tree_path_free (path);

    if (current_index < g_list_length (capplet->terminals)) {
	item = (GnomeDATermItem*) g_list_nth_data (capplet->terminals, current_index);
	is_custom_active = FALSE;

	cs = gconf_change_set_new ();

	gconf_change_set_set_string (cs, DEFAULT_APPS_KEY_TERMINAL_EXEC, item->generic.command);
	gconf_change_set_set_string (cs, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG, item->exec_flag);

	gconf_client_commit_change_set (capplet->gconf, cs, TRUE, &error);

	if (error != NULL) {
	    g_warning (_("Error saving configuration: %s"), error->message);
	    g_error_free (error);
	    error = NULL;
	}

	gconf_change_set_unref (cs);
    }
    else {
	is_custom_active = TRUE;
    }

    gtk_entry_set_text (GTK_ENTRY (capplet->terminal_command_entry),
			gconf_client_get_string (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_EXEC, NULL));
    gtk_entry_set_text (GTK_ENTRY (capplet->terminal_exec_flag_entry),
			gconf_client_get_string (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG, NULL));

    gtk_editable_set_editable (GTK_EDITABLE (capplet->terminal_command_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_command_label, is_custom_active);
    gtk_editable_set_editable (GTK_EDITABLE (capplet->terminal_exec_flag_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_exec_flag_label, is_custom_active);
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
    { "mail_reader_image", "stock_mail-open"  },
/*    { "messenger_image",   "im"               },
 *    { "image_image",       "image-viewer"     },
 *    { "sound_image",       "gnome-audio"      },
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
    refresh_combo_box_icons (theme, GTK_COMBO_BOX (capplet->term_combo_box), capplet->terminals);
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
web_browser_update_combo_box (GnomeDACapplet *capplet, const gchar *command)
{
    GList *entry;
    gint index;
    gboolean is_custom_active;

    entry = g_list_find_custom (capplet->web_browsers, command, (GCompareFunc) web_item_comp);

    if (entry) {
	index = g_list_position (capplet->web_browsers, entry);
	is_custom_active = FALSE;
    }
    else {
	/* index of 'Custom' combo box entry */
	index = g_list_length (capplet->web_browsers) + 1;
	is_custom_active = TRUE;
    }

    /* TODO: Remove when GConfPropertyEditor will be used */
    gtk_entry_set_text (GTK_ENTRY (capplet->web_browser_command_entry), command);

    gtk_editable_set_editable (GTK_EDITABLE (capplet->web_browser_command_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->web_browser_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->web_browser_terminal_checkbutton, is_custom_active);

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (capplet->web_combo_box)) != index)
	gtk_combo_box_set_active (GTK_COMBO_BOX (capplet->web_combo_box), index);
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

static void
mail_reader_update_combo_box (GnomeDACapplet *capplet, const gchar *command)
{
    GList *entry;
    gint index;
    gboolean is_custom_active;

    entry = g_list_find_custom (capplet->mail_readers, command, (GCompareFunc) generic_item_comp);

    if (entry) {
	index = g_list_position (capplet->mail_readers, entry);
	is_custom_active = FALSE;
    }
    else {
	/* index of 'Custom' combo box entry */
	index = g_list_length (capplet->mail_readers) + 1;
	is_custom_active = TRUE;
    }

    gtk_entry_set_text (GTK_ENTRY (capplet->mail_reader_command_entry), command);

    gtk_editable_set_editable (GTK_EDITABLE (capplet->mail_reader_command_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->mail_reader_command_label, is_custom_active);
    gtk_widget_set_sensitive (capplet->mail_reader_terminal_checkbutton, is_custom_active);

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (capplet->mail_combo_box)) != index)
	gtk_combo_box_set_active (GTK_COMBO_BOX (capplet->mail_combo_box), index);
}

static void
terminal_update_combo_box (GnomeDACapplet *capplet, const gchar *command)
{
    GList *entry;
    gint index;
    gboolean is_custom_active;

    entry = g_list_find_custom (capplet->terminals, command, (GCompareFunc) generic_item_comp);

    if (entry) {
	index = g_list_position (capplet->terminals, entry);
	is_custom_active = FALSE;
    }
    else {
	/* index of 'Custom' combo box entry */
	index = g_list_length (capplet->terminals) + 1;
	is_custom_active = TRUE;
    }

    gtk_entry_set_text (GTK_ENTRY (capplet->terminal_command_entry), command);

    gtk_editable_set_editable (GTK_EDITABLE (capplet->terminal_command_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_command_label, is_custom_active);
    gtk_editable_set_editable (GTK_EDITABLE (capplet->terminal_exec_flag_entry), is_custom_active);
    gtk_widget_set_sensitive (capplet->terminal_exec_flag_label, is_custom_active);

    if (gtk_combo_box_get_active (GTK_COMBO_BOX (capplet->term_combo_box)) != index)
	gtk_combo_box_set_active (GTK_COMBO_BOX (capplet->term_combo_box), index);
}

static void
web_gconf_changed_cb (GConfClient *client, guint id, GConfEntry *entry, GnomeDACapplet *capplet)
{
    GConfValue *value;
    GConfChangeSet *cs;
    GError *error = NULL;

    g_return_if_fail (gconf_entry_get_key (entry) != NULL);

    if (!(value = gconf_entry_get_value (entry)))
	return;

    if (strcmp (entry->key, DEFAULT_APPS_KEY_HTTP_EXEC) == 0) {
	web_browser_update_combo_box (capplet, gconf_value_get_string (value));
	web_browser_update_radio_buttons (capplet, gconf_value_get_string (value));

	cs = gconf_change_set_new ();

	gconf_change_set_set (cs, DEFAULT_APPS_KEY_HTTPS_EXEC, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_UNKNOWN_EXEC, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_ABOUT_EXEC, value);

	gconf_client_commit_change_set (capplet->gconf, cs, TRUE, &error);

	if (error != NULL) {
	    g_warning (_("Error saving configuration: %s"), error->message);
	    g_error_free (error);
	    error = NULL;
	}

	gconf_change_set_unref (cs);
    }
    /* TODO: Remove when GConfPropertyEditor will be used */
    else if (strcmp (entry->key, DEFAULT_APPS_KEY_HTTP_NEEDS_TERM) == 0) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->web_browser_terminal_checkbutton),
				      gconf_value_get_bool (value));

	cs = gconf_change_set_new ();

	gconf_change_set_set (cs, DEFAULT_APPS_KEY_HTTPS_NEEDS_TERM, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_UNKNOWN_NEEDS_TERM, value);
	gconf_change_set_set (cs, DEFAULT_APPS_KEY_ABOUT_NEEDS_TERM, value);

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
mail_gconf_changed_cb (GConfClient *client, guint id, GConfEntry *entry, GnomeDACapplet *capplet)
{
    GConfValue *value;

    g_return_if_fail (gconf_entry_get_key (entry) != NULL);

    if (!(value = gconf_entry_get_value (entry)))
	return;

    if (strcmp (entry->key, DEFAULT_APPS_KEY_MAILER_EXEC) == 0) {
	mail_reader_update_combo_box (capplet, gconf_value_get_string (value));
    }
    /* TODO: Remove when GConfPropertyEditor will be used */
    else if (strcmp (entry->key, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM) == 0) {
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->mail_reader_terminal_checkbutton),
				      gconf_value_get_bool (value));
    }
}

static void
term_gconf_changed_cb (GConfClient *client, guint id, GConfEntry *entry, GnomeDACapplet *capplet)
{
    GConfValue *value;

    g_return_if_fail (gconf_entry_get_key (entry) != NULL);

    if (!(value = gconf_entry_get_value (entry)))
	return;

    if (strcmp (entry->key, DEFAULT_APPS_KEY_TERMINAL_EXEC) == 0) {
	terminal_update_combo_box (capplet, gconf_value_get_string (value));
    }
    /* TODO: Remove when GConfPropertyEditor will be used */
    else if (strcmp (entry->key, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG) == 0) {
	gtk_entry_set_text (GTK_ENTRY (capplet->terminal_exec_flag_entry),
			    gconf_value_get_string (value));
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
    gchar *label;

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
	label = g_strdup (item->name);

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    PIXBUF_COL, pixbuf,
			    TEXT_COL, label,
			    -1);

	item->icon_path = gtk_tree_model_get_string_from_iter (model, &iter);

	if (pixbuf)
	    g_object_unref (pixbuf);
	g_free (label);
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
show_dialog (GnomeDACapplet *capplet)
{
    GConfValue *value;

    if (g_file_test (GLADEDIR "/gnome-default-applications-properties.glade", G_FILE_TEST_EXISTS) != FALSE) {
	capplet->xml = glade_xml_new (GLADEDIR "/gnome-default-applications-properties.glade", NULL, NULL);
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

    capplet->web_combo_box = glade_xml_get_widget (capplet->xml, "web_browser_combobox");
    capplet->mail_combo_box = glade_xml_get_widget (capplet->xml, "mail_reader_combobox");
    capplet->term_combo_box = glade_xml_get_widget (capplet->xml, "terminal_combobox");

    g_signal_connect (capplet->window, "screen-changed", G_CALLBACK (screen_changed_cb), capplet);
    screen_changed_cb (capplet->window, gdk_screen_get_default (), capplet);

    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->web_combo_box), capplet->web_browsers);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->mail_combo_box), capplet->mail_readers);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->term_combo_box), capplet->terminals);

    /* update ui to gconf content */
    value = gconf_client_get (capplet->gconf, DEFAULT_APPS_KEY_HTTP_EXEC, NULL);
    web_browser_update_combo_box (capplet, gconf_value_get_string (value));
    web_browser_update_radio_buttons (capplet, gconf_value_get_string (value));

    value = gconf_client_get (capplet->gconf, DEFAULT_APPS_KEY_HTTP_NEEDS_TERM, NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->web_browser_terminal_checkbutton),
				  gconf_value_get_bool (value));

    value = gconf_client_get (capplet->gconf, DEFAULT_APPS_KEY_MAILER_EXEC, NULL);
    mail_reader_update_combo_box (capplet, gconf_value_get_string (value));

    value = gconf_client_get (capplet->gconf, DEFAULT_APPS_KEY_MAILER_NEEDS_TERM, NULL);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (capplet->mail_reader_terminal_checkbutton),
				  gconf_value_get_bool (value));

    value = gconf_client_get (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_EXEC, NULL);
    terminal_update_combo_box (capplet, gconf_value_get_string (value));

    value = gconf_client_get (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG, NULL);
    gtk_entry_set_text (GTK_ENTRY (capplet->terminal_exec_flag_entry),
			gconf_value_get_string (value));

    g_signal_connect (capplet->web_combo_box, "changed", G_CALLBACK (web_combo_changed_cb), capplet);
    g_signal_connect (capplet->mail_combo_box, "changed", G_CALLBACK (mail_combo_changed_cb), capplet);
    g_signal_connect (capplet->term_combo_box, "changed", G_CALLBACK (terminal_combo_changed_cb), capplet);

    /* TODO: Remove when GConfPropertyEditor will be used */
    g_signal_connect (capplet->web_browser_terminal_checkbutton, "toggled",
		      G_CALLBACK (terminal_checkbutton_toggled_cb), capplet);
    g_signal_connect (capplet->mail_reader_terminal_checkbutton, "toggled",
		      G_CALLBACK (terminal_checkbutton_toggled_cb), capplet);

    /* TODO: Remove when GConfPropertyEditor will be used */
    g_signal_connect (capplet->web_browser_command_entry, "focus-out-event", G_CALLBACK (entry_focus_out_event_cb), capplet);
    g_signal_connect (capplet->mail_reader_command_entry, "focus-out-event", G_CALLBACK (entry_focus_out_event_cb), capplet);
    g_signal_connect (capplet->terminal_command_entry, "focus-out-event", G_CALLBACK (entry_focus_out_event_cb), capplet);
    g_signal_connect (capplet->terminal_exec_flag_entry, "focus-out-event", G_CALLBACK (entry_focus_out_event_cb), capplet);

    g_signal_connect (capplet->default_radiobutton, "toggled", G_CALLBACK (web_radiobutton_toggled_cb), capplet);
    g_signal_connect (capplet->new_win_radiobutton, "toggled", G_CALLBACK (web_radiobutton_toggled_cb), capplet);
    g_signal_connect (capplet->new_tab_radiobutton, "toggled", G_CALLBACK (web_radiobutton_toggled_cb), capplet);

    /* capplet_set_icon (GTK_WINDOW (main_dlg), "gnome-settings-default-applications"); */
    gtk_window_set_icon_name (GTK_WINDOW (capplet->window),
			      "gnome-settings-default-applications");

    gtk_widget_show (capplet->window);
}

int
main (int argc, char **argv)
{
    GnomeDACapplet *capplet;

    capplet = g_new0 (GnomeDACapplet, 1);

#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv,
			GNOME_PARAM_NONE);

    glade_init ();

    capplet->gconf = gconf_client_get_default ();

    gconf_client_add_dir (capplet->gconf, "/desktop/gnome/applications/browser", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
    gconf_client_add_dir (capplet->gconf, "/desktop/gnome/applications/terminal", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
    gconf_client_add_dir (capplet->gconf, "/desktop/gnome/url-handlers", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

    gconf_client_notify_add (capplet->gconf, DEFAULT_APPS_KEY_HTTP_PATH,
			     (GConfClientNotifyFunc) web_gconf_changed_cb,
			     capplet, NULL, NULL);
    gconf_client_notify_add (capplet->gconf, DEFAULT_APPS_KEY_MAILER_PATH,
			     (GConfClientNotifyFunc) mail_gconf_changed_cb,
			     capplet, NULL, NULL);
    gconf_client_notify_add (capplet->gconf, DEFAULT_APPS_KEY_TERMINAL_PATH,
			     (GConfClientNotifyFunc) term_gconf_changed_cb,
			     capplet, NULL, NULL);

    gnome_da_xml_load_list (capplet);

    show_dialog (capplet);

    gtk_main ();

    g_object_unref (capplet->gconf);
    g_object_unref (capplet->xml);

    return 0;
}
