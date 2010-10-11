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

#include "libgnome-control-center/gconf-property-editor.h"
#include "gnome-da-capplet.h"
#include "gnome-da-xml.h"
#include "gnome-da-item.h"

enum
{
    PIXBUF_COL,
    TEXT_COL,
    N_COLUMNS
};

/*
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
*/

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
web_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;

    current_index = gtk_combo_box_get_active (combo);

    if (current_index < g_list_length (capplet->web_browsers)) {
        GnomeDAURLItem *item;
	GError *error = NULL;

	item = (GnomeDAURLItem*) g_list_nth_data (capplet->web_browsers, current_index);
	if (item == NULL)
            return;

	if (!g_app_info_set_as_default_for_type (item->app_info, "x-scheme-handler/http", &error) ||
	    !g_app_info_set_as_default_for_type (item->app_info, "x-scheme-handler/https", &error)) {
            g_warning (_("Error setting default browser: %s"), error->message);
	    g_error_free (error);
	}
    }
}

/* FIXME: Refactor these two functions below into one... */
static void
mail_combo_changed_cb (GtkComboBox *combo, GnomeDACapplet *capplet)
{
    guint current_index;

    current_index = gtk_combo_box_get_active (combo);

    if (current_index < g_list_length (capplet->mail_readers)) {
        GnomeDAURLItem *item;
	GError *error = NULL;

	item = (GnomeDAURLItem*) g_list_nth_data (capplet->web_browsers, current_index);
	if (item == NULL)
            return;

	if (!g_app_info_set_as_default_for_type (item->app_info, "x-scheme-handler/mailto", &error)) {
            g_warning (_("Error setting default mailer: %s"), error->message);
	    g_error_free (error);
	}
    }

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
    GObject *icon;
    gint i;

    for (i = 0; i < G_N_ELEMENTS (icons); i++) {
	icon = gtk_builder_get_object (capplet->builder, icons[i].name);
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
    capplet->theme_changed_id = g_signal_connect (theme, "changed", G_CALLBACK (theme_changed_cb), capplet);
    theme_changed_cb (theme, capplet);

    capplet->icon_theme = theme;
}

static gint
generic_item_comp (gconstpointer list_item, gconstpointer command)
{
    return (strcmp (((GnomeDAItem *) list_item)->command, (gchar *) command));
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

static GConfValue*
combo_conv_from_widget_term_flag (GConfPropertyEditor *peditor, const GConfValue *value)
{
    GConfValue *ret;
    GList *handlers;
    gint index;
    GnomeDATermItem *item;

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
        gconf_value_set_string (ret, item->exec_flag);
        return ret;
    }
}

static GConfValue*
combo_conv_to_widget_term_flag (GConfPropertyEditor *peditor, const GConfValue *value)
{
    GConfValue *ret;
    GtkComboBox *combo;

    combo = GTK_COMBO_BOX (gconf_property_editor_get_ui_control (peditor));

    ret = gconf_value_new (GCONF_VALUE_INT);
    gconf_value_set_int (ret, gtk_combo_box_get_active (combo));
    return ret;
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
fill_combo_box (GtkIconTheme *theme, GtkComboBox *combo_box, GList *app_list, gboolean add_custom)
{
    GList *entry;
    GtkTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeIter iter;
    GdkPixbuf *pixbuf;

    if (theme == NULL) {
	theme = gtk_icon_theme_get_default ();
    }

    if (add_custom) {
	gtk_combo_box_set_row_separator_func (combo_box, is_separator,
					      GINT_TO_POINTER (g_list_length (app_list)), NULL);
    }

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

    if (add_custom) {
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter, -1);
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    PIXBUF_COL, NULL,
			    TEXT_COL, _("Custom"),
			    -1);
    }
}

static GtkWidget*
_gtk_builder_get_widget (GtkBuilder *builder, const gchar *name)
{
    return GTK_WIDGET (gtk_builder_get_object (builder, name));
}


static void
show_dialog (GnomeDACapplet *capplet, const gchar *start_page)
{
    GtkBuilder *builder;
    guint builder_result;

    capplet->builder = builder = gtk_builder_new ();

    if (g_file_test (GNOMECC_UI_DIR "/gnome-default-applications-properties.ui", G_FILE_TEST_EXISTS) != FALSE) {
	builder_result = gtk_builder_add_from_file (builder, GNOMECC_UI_DIR "/gnome-default-applications-properties.ui", NULL);
    }
    else {
	builder_result = gtk_builder_add_from_file (builder, "./gnome-default-applications-properties.ui", NULL);
    }

    if (builder_result == 0) {
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

    capplet->window = _gtk_builder_get_widget (builder,"preferred_apps_dialog");

    capplet->terminal_command_entry = _gtk_builder_get_widget (builder, "terminal_command_entry");
    capplet->terminal_command_label = _gtk_builder_get_widget (builder, "terminal_command_label");
    capplet->terminal_exec_flag_entry = _gtk_builder_get_widget (builder, "terminal_exec_flag_entry");
    capplet->terminal_exec_flag_label = _gtk_builder_get_widget (builder, "terminal_exec_flag_label");

    capplet->media_player_command_entry = _gtk_builder_get_widget (builder, "media_player_command_entry");
    capplet->media_player_command_label = _gtk_builder_get_widget (builder, "media_player_command_label");
    capplet->media_player_terminal_checkbutton = _gtk_builder_get_widget (builder, "media_player_terminal_checkbutton");

    capplet->visual_command_entry = _gtk_builder_get_widget (builder, "visual_command_entry");
    capplet->visual_command_label = _gtk_builder_get_widget (builder, "visual_command_label");
    capplet->visual_startup_checkbutton = _gtk_builder_get_widget (builder, "visual_start_checkbutton");

    capplet->mobility_command_entry = _gtk_builder_get_widget (builder, "mobility_command_entry");
    capplet->mobility_command_label = _gtk_builder_get_widget (builder, "mobility_command_label");
    capplet->mobility_startup_checkbutton = _gtk_builder_get_widget (builder, "mobility_start_checkbutton");

    capplet->web_combo_box = _gtk_builder_get_widget (builder, "web_browser_combobox");
    capplet->mail_combo_box = _gtk_builder_get_widget (builder, "mail_reader_combobox");
    capplet->term_combo_box = _gtk_builder_get_widget (builder, "terminal_combobox");
    capplet->media_combo_box = _gtk_builder_get_widget (builder, "media_player_combobox");
    capplet->visual_combo_box = _gtk_builder_get_widget (builder, "visual_combobox");
    capplet->mobility_combo_box = _gtk_builder_get_widget (builder, "mobility_combobox");

    g_signal_connect (capplet->window, "screen-changed", G_CALLBACK (screen_changed_cb), capplet);
    screen_changed_cb (capplet->window, gdk_screen_get_default (), capplet);

    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->web_combo_box), capplet->web_browsers, FALSE);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->mail_combo_box), capplet->mail_readers, FALSE);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->term_combo_box), capplet->terminals, TRUE);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->media_combo_box), capplet->media_players, TRUE);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->visual_combo_box), capplet->visual_ats, TRUE);
    fill_combo_box (capplet->icon_theme, GTK_COMBO_BOX (capplet->mobility_combo_box), capplet->mobility_ats, TRUE);

    g_signal_connect (capplet->web_combo_box, "changed", G_CALLBACK (web_combo_changed_cb), capplet);
    g_signal_connect (capplet->mail_combo_box, "changed", G_CALLBACK (mail_combo_changed_cb), capplet);
    g_signal_connect (capplet->term_combo_box, "changed", G_CALLBACK (terminal_combo_changed_cb), capplet);
    g_signal_connect (capplet->media_combo_box, "changed", G_CALLBACK (media_combo_changed_cb), capplet);
    g_signal_connect (capplet->visual_combo_box, "changed", G_CALLBACK (visual_combo_changed_cb), capplet);
    g_signal_connect (capplet->mobility_combo_box, "changed", G_CALLBACK (mobility_combo_changed_cb), capplet);

    /* Setup GConfPropertyEditors */

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

    gconf_peditor_new_combo_box (NULL,
        DEFAULT_APPS_KEY_TERMINAL_EXEC_ARG,
        capplet->term_combo_box,
        "conv-from-widget-cb", combo_conv_from_widget_term_flag,
        "conv-to-widget-cb", combo_conv_to_widget_term_flag,
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

        w = _gtk_builder_get_widget (builder, page_name);
        if (w != NULL) {
            GtkNotebook *nb;
            gint pindex;

            nb = GTK_NOTEBOOK (_gtk_builder_get_widget (builder,
                                                        "preferred_apps_notebook"));
            pindex = gtk_notebook_page_num (nb, w);
            if (pindex != -1)
                gtk_notebook_set_current_page (nb, pindex);
        }
        g_free (page_name);
    }
}

void
gnome_default_applications_panel_init (GnomeDACapplet *capplet)
{
    gconf_client_add_dir (capplet->gconf, "/desktop/gnome/url-handlers",
                          GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
    gconf_client_add_dir (capplet->gconf, 
                          "/desktop/gnome/applications",
                          GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);

    gnome_da_xml_load_list (capplet);

    show_dialog (capplet, 0);
}
