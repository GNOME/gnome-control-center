/* -*- mode: c; style: linux -*- */

/* screensaver-prefs-dialog.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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
# include "config.h"
#endif

#include <gnome.h>
#include <parser.h>
#include <sys/stat.h>

#include <glade/glade.h>

#include "screensaver-prefs-dialog.h"
#include "preferences.h"
#include "expr.h"
#include "preview.h"

struct _cli_argument_t
{
	char *argument;
	char *value;
};

typedef struct _cli_argument_t cli_argument_t;

enum {
	OK_CLICKED_SIGNAL,
	DEMO_SIGNAL,
	LAST_SIGNAL
};

static gint screensaver_prefs_dialog_signals[LAST_SIGNAL] = { 0 };

static void screensaver_prefs_dialog_init (ScreensaverPrefsDialog *dialog);
static void screensaver_prefs_dialog_class_init (ScreensaverPrefsDialogClass *dialog);

static void set_widgets_sensitive        (GladeXML *prop_data, 
					  gchar *widgets_str, 
					  gboolean s);

static void activate_option_cb           (GtkWidget *widget);
static void toggle_check_cb              (GtkWidget *widget, 
					  xmlNodePtr node);

static gchar *write_boolean              (xmlNodePtr arg_def, 
					  GladeXML *prop_data);
static gchar *write_number               (xmlNodePtr arg_def, 
					  GladeXML *prop_data);
static gchar *write_select               (xmlNodePtr arg_def, 
					  GladeXML *prop_data);
static gchar *write_command_line         (gchar *name, 
					  xmlNodePtr arg_def, 
					  GladeXML *prop_data);

static GScanner *read_command_line     (char *command_line);
static xmlNodePtr get_argument_data      (Screensaver *saver);
static GladeXML *get_screensaver_widget  (Screensaver *saver,
					  xmlNodePtr argument_data);

static gint arg_is_set                   (xmlNodePtr argument_data, 
					  GScanner *cli_db);
static void read_boolean                 (GladeXML *widget_data, 
					  xmlNodePtr argument_data,
					  GScanner *cli_db);
static void read_number                  (GladeXML *widget_data, 
					  xmlNodePtr argument_data,
					  GScanner *cli_db);
static void read_select                  (GladeXML *widget_data, 
					  xmlNodePtr argument_data, 
					  GScanner *cli_db);
static void place_screensaver_properties (ScreensaverPrefsDialog *dialog);

static gboolean arg_mapping_exists       (Screensaver *saver);

static void store_cli                    (ScreensaverPrefsDialog *dialog);

static GtkWidget *get_basic_screensaver_widget (ScreensaverPrefsDialog *dialog,
						Screensaver *saver);

static void demo_cb                      (GtkWidget *widget,
					  ScreensaverPrefsDialog *dialog);
static void help_cb                      (GtkWidget *widget,
					  ScreensaverPrefsDialog *dialog);
static void screensaver_prop_ok_cb       (GtkWidget *widget,
					  ScreensaverPrefsDialog *dialog);
static void screensaver_prop_cancel_cb   (GtkWidget *widget,
					  ScreensaverPrefsDialog *dialog);

guint
screensaver_prefs_dialog_get_type (void)
{
	static guint screensaver_prefs_dialog_type = 0;

	if (!screensaver_prefs_dialog_type) {
		GtkTypeInfo screensaver_prefs_dialog_info = {
			"ScreensaverPrefsDialog",
			sizeof (ScreensaverPrefsDialog),
			sizeof (ScreensaverPrefsDialogClass),
			(GtkClassInitFunc) screensaver_prefs_dialog_class_init,
			(GtkObjectInitFunc) screensaver_prefs_dialog_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		screensaver_prefs_dialog_type = 
			gtk_type_unique (gnome_dialog_get_type (), 
					 &screensaver_prefs_dialog_info);
	}

	return screensaver_prefs_dialog_type;
}

static void
screensaver_prefs_dialog_init (ScreensaverPrefsDialog *dialog) 
{
	GtkWidget *global_vbox, *vbox, *hbox, *frame, *label;

	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, FALSE);

	global_vbox = GNOME_DIALOG (dialog)->vbox;

	hbox = gtk_hbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (global_vbox), hbox, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);

	label = gtk_label_new (_("Name:"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);

	dialog->name_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), dialog->name_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 10);
	gtk_box_pack_start (GTK_BOX (global_vbox), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);

	dialog->settings_dialog_frame = gtk_frame_new (_("Settings"));
	gtk_box_pack_start (GTK_BOX (hbox), dialog->settings_dialog_frame, 
			    TRUE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 10);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

	frame = gtk_frame_new (_("Description"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

	dialog->description = gtk_label_new (_("label1"));
	gtk_container_add (GTK_CONTAINER (frame), dialog->description);
	gtk_label_set_justify (GTK_LABEL (dialog->description), 
			       GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (dialog->description), TRUE);
	gtk_misc_set_alignment (GTK_MISC (dialog->description), 0, 0);
	gtk_misc_set_padding (GTK_MISC (dialog->description), 5, 5);

	gnome_dialog_append_button (GNOME_DIALOG (dialog), 
				    _("Demo"));
	gnome_dialog_append_button (GNOME_DIALOG (dialog), 
				    GNOME_STOCK_BUTTON_HELP);
	gnome_dialog_append_button (GNOME_DIALOG (dialog), 
				    GNOME_STOCK_BUTTON_OK);
	gnome_dialog_append_button (GNOME_DIALOG (dialog), 
				    GNOME_STOCK_BUTTON_CANCEL);

	gtk_widget_show_all (global_vbox);
}

static void
screensaver_prefs_dialog_class_init (ScreensaverPrefsDialogClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
    
	screensaver_prefs_dialog_signals[OK_CLICKED_SIGNAL] =
		gtk_signal_new ("ok-clicked", GTK_RUN_FIRST, 
				object_class->type,
				GTK_SIGNAL_OFFSET 
				(ScreensaverPrefsDialogClass, ok_clicked),
				gtk_signal_default_marshaller, 
				GTK_TYPE_NONE, 0);

	screensaver_prefs_dialog_signals[DEMO_SIGNAL] =
		gtk_signal_new ("demo", GTK_RUN_FIRST, 
				object_class->type,
				GTK_SIGNAL_OFFSET 
				(ScreensaverPrefsDialogClass, demo),
				gtk_signal_default_marshaller, 
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, 
				      screensaver_prefs_dialog_signals,
				      LAST_SIGNAL);

	class->ok_clicked = NULL;
}

GtkWidget *
screensaver_prefs_dialog_new (Screensaver *saver) 
{
	GtkWidget *widget;
	ScreensaverPrefsDialog *dialog;
	char *title;

	widget = gtk_type_new (screensaver_prefs_dialog_get_type ());
	dialog = SCREENSAVER_PREFS_DIALOG (widget);

	dialog->saver = saver;

	title = g_strconcat (saver->label, " properties", NULL);
	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), 
			    saver->label);

	if (arg_mapping_exists (saver)) {
		dialog->cli_args_db = 
			read_command_line (saver->command_line);
		dialog->argument_data = get_argument_data (saver);
		dialog->prefs_widget_data = 
			get_screensaver_widget (saver, 
						dialog->argument_data);
	} else {
		dialog->basic_widget = 
			get_basic_screensaver_widget (dialog, saver);
	}

	gtk_window_set_title (GTK_WINDOW (dialog), title);

	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0, 
				     demo_cb, dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1, 
				     help_cb, dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 2, 
				     screensaver_prop_ok_cb, dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 3, 
				     screensaver_prop_cancel_cb, dialog);

	if (dialog->basic_widget)
		gtk_container_add (GTK_CONTAINER
				   (dialog->settings_dialog_frame),
				   dialog->basic_widget);
	else if (dialog->prefs_widget_data)
		gtk_container_add (GTK_CONTAINER 
				   (dialog->settings_dialog_frame), 
				   glade_xml_get_widget 
				   (dialog->prefs_widget_data,
				    "widget"));

	gtk_label_set_text (GTK_LABEL (dialog->description), 
			    screensaver_get_desc (saver));

	if (dialog->argument_data)
		place_screensaver_properties (dialog);

	return widget;
}

/*****************************************************************************/
/*            Enabling/disabling widgets based on options selected           */
/*****************************************************************************/

/* set_widgets_sensitive
 *
 * Given a string of comma-separated widget names, finds the widgets
 * in the given Glade XML definition and enables or disables them,
 * based on the value of s
 */

static void
set_widgets_sensitive (GladeXML *prop_data, gchar *widgets_str, gboolean s) 
{
	char **widgets;
	int i;
	GtkWidget *widget;

	if (!widgets_str) return;

	widgets = g_strsplit (widgets_str, ",", -1);

	for (i = 0; widgets[i]; i++) {
		widget = glade_xml_get_widget (prop_data, widgets[i]);
		if (widget) gtk_widget_set_sensitive (widget, s);
	}

	g_strfreev (widgets);
}

/* activate_option_cb
 *
 * Callback invoked when an option on an option menu is selected. The
 * callback scans the xml node corresponding with that option to see
 * what widgets are enabled by selecting that option and uses
 * set_widgets_sensitive to enable them. Iterates through all other
 * XML nodes and disables those widgets enabled by other nodes.
 */

static void
activate_option_cb (GtkWidget *widget) 
{
	GladeXML *prop_data;
	xmlNodePtr argument_data, option_def, node;

	prop_data = gtk_object_get_data (GTK_OBJECT (widget),
					 "prop_data");
	argument_data = gtk_object_get_data (GTK_OBJECT (widget),
					     "argument_data");
	option_def = gtk_object_get_data (GTK_OBJECT (widget),
					  "option_def");

	node = argument_data->childs;

	while (node) {
		if (node != option_def)
			set_widgets_sensitive (prop_data, 
					       xmlGetProp (node, "enable"), 
					       FALSE);
		node = node->next;
	}

	set_widgets_sensitive (prop_data, 
			       xmlGetProp (option_def, "enable"), TRUE);
}

/* toggle_check_cb
 *
 * Callback invoked when a check button is toggled. Works in a manner
 * analagous to select_option_cb and deselect_option_cb above.
 */

static void
toggle_check_cb (GtkWidget *widget, xmlNodePtr node) 
{
	GladeXML *prop_data;
	gboolean set;

	prop_data = gtk_object_get_data (GTK_OBJECT (widget),
					 "prop_data");
	set = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	set_widgets_sensitive (prop_data, 
			       xmlGetProp (node, "enable"),
			       set);
}

/*****************************************************************************/
/*                        Writing out command lines                          */
/*****************************************************************************/

static gchar *
write_boolean (xmlNodePtr argument_data, GladeXML *prop_data) 
{
	char *widget_name;
	GtkWidget *widget;

	widget_name = g_strconcat (xmlGetProp (argument_data, "id"),
				   "_widget", NULL);
	widget = glade_xml_get_widget (prop_data, widget_name);
	g_free (widget_name);

	if (!widget || !GTK_WIDGET_IS_SENSITIVE (widget)) return NULL;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
		return xmlGetProp (argument_data, "arg-set");
	else 
		return NULL;
}

static gchar *
write_number (xmlNodePtr argument_data, GladeXML *prop_data)
{
	char *widget_name;
	GtkWidget *widget;
	GtkAdjustment *adjustment = NULL;
	gfloat value = 0.0;
	gchar *to_cli_expr;

	widget_name = g_strconcat (xmlGetProp (argument_data, "id"),
				   "_widget", NULL);
	widget = glade_xml_get_widget (prop_data, widget_name);
	g_free (widget_name);

	if (!widget || !GTK_WIDGET_IS_SENSITIVE (widget)) return NULL;

	if (GTK_IS_RANGE (widget))
		adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
	else if (GTK_IS_SPIN_BUTTON (widget))
		adjustment = gtk_spin_button_get_adjustment 
			(GTK_SPIN_BUTTON (widget));
	if (adjustment)
		value = adjustment->value;

	to_cli_expr = xmlGetProp (argument_data, "to-cli-conv");
	if (to_cli_expr)
		value = parse_expr (to_cli_expr, value);

	return g_strdup_printf (xmlGetProp (argument_data, "arg"), (int) value);
}

/* Note to readers: *please* ignore the following function, for the
 * sake of your own mental health and my reputation as a sane
 * coder. Just accept that it returns a string containing the CLI
 * arguments corresponding with the selected option of an option
 * menu. *sigh* I really must ask someone if there's a better way to
 * do this...
 */

static gchar *
write_select (xmlNodePtr argument_data, GladeXML *prop_data) 
{
	xmlNodePtr node;
	int i = 0;
	GtkWidget *widget, *menu, *active;
	char *widget_name;
	GList *menu_item;

	node = argument_data->childs;
	widget_name = g_strconcat (xmlGetProp (argument_data, "id"),
				   "_widget", NULL);
	widget = glade_xml_get_widget (prop_data, widget_name);
	if (!widget || !GTK_WIDGET_IS_SENSITIVE (widget)) return NULL;
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget));
	menu_item = GTK_MENU_SHELL (menu)->children;
	active = gtk_menu_get_active (GTK_MENU (menu));

	while (node && menu_item) {
		if (active == menu_item->data &&
		    !xmlGetProp (node, "no-output"))
			return xmlGetProp (node, "arg-set");

		node = node->next; menu_item = menu_item->next; i++;
	}

	return NULL;
}

/* write_command_line
 *
 * Scan through XML nodes in the argument definition file and write
 * out the parameter corresponding to each node in turn.
 */

static gchar *
write_command_line (gchar *name, xmlNodePtr argument_data, GladeXML *prop_data) 
{
	GString *line;
	xmlNodePtr node;
	gchar *arg, *ret;

	line = g_string_new (name);
	node = argument_data->childs;

	for (node = argument_data->childs; node; node = node->next) {
		if (xmlGetProp (node, "no-output"))
			continue;

		if (!strcmp (node->name, "boolean"))
			arg = write_boolean (node, prop_data);
		else if (!strcmp (node->name, "number"))
			arg = write_number (node, prop_data);
		else if (!strcmp (node->name, "select"))
			arg = write_select (node, prop_data);
		else if (!strcmp (node->name, "command"))
			arg = xmlGetProp (node, "arg");
		else
			arg = NULL;

		if (arg) {
			g_string_append (line, " ");
			g_string_append (line, arg);
		}
	}

	ret = line->str;
	g_string_free (line, FALSE);
	return ret;
}

/*****************************************************************************/
/*                         Reading the command line                          */
/*****************************************************************************/

static GScanner *
read_command_line (char *command_line) 
{
	char **args;
	int i;
	GScanner *cli_db;
	static GScannerConfig config;
	char *arg, *value;

	config.cset_skip_characters = " \t\n";
	config.cset_identifier_first = "abcdefghijklmnopqrstuvwxyz";
	config.cset_identifier_nth = "abcdefghijklmnopqrstuvwxyz_";
	config.scan_symbols = TRUE;
	config.scan_identifier = TRUE;

	cli_db = g_scanner_new (&config);
	g_scanner_set_scope (cli_db, 0);

	args = g_strsplit (command_line, " ", -1);

	g_scanner_scope_add_symbol (cli_db, 0, "and", SYMBOL_AND);
	g_scanner_scope_add_symbol (cli_db, 0, "or", SYMBOL_OR);
	g_scanner_scope_add_symbol (cli_db, 0, "not", SYMBOL_NOT);

	for (i = 0; args[i]; i++) {
		if (args[i][0] == '-') {
			arg = g_strdup (args[i] + 1);

			if (args[i + 1] && args[i + 1][0] != '-') {
				value = g_strdup (args[i + 1]);
				i++;
			} else {
				value = (char *) 1;
			}

			g_scanner_scope_add_symbol (cli_db, 0, arg, value);
		}
	}

	g_strfreev (args);

	return cli_db;
}

/*****************************************************************************/
/*                    Getting the argument definition data                   */
/*****************************************************************************/

static xmlNodePtr
get_argument_data (Screensaver *saver) 
{
	xmlDocPtr doc;
	xmlNodePtr root_node, node;
	gchar *name;

	doc = xmlParseFile (SSPROP_DATADIR "/hacks.xml");
	root_node = xmlDocGetRootElement (doc);
	g_assert (root_node != NULL);
	node = root_node->childs;

	while (node) {
		name = xmlGetProp (node, "name");
		if (!strcmp (name, saver->name)) break;
		node = node->next;
	}

	return node;
}

/*****************************************************************************/
/*                       Creating the dialog proper                          */
/*****************************************************************************/

static GladeXML *
get_screensaver_widget (Screensaver *saver, 
			xmlNodePtr argument_data) 
{
	GladeXML *screensaver_prop_data;
	gchar *file_name;
	
	file_name = g_strconcat (SSPROP_DATADIR "/",
				 saver->name, "-settings.glade", NULL);
	screensaver_prop_data = glade_xml_new (file_name, "widget");
	g_free (file_name);

	return screensaver_prop_data;
}

/*****************************************************************************/
/*                  Setting dialog properties from the CLI                   */
/*****************************************************************************/

/* arg_is_set
 *
 * Determines if an argument or set of arguments specified by the
 * argument definition entry is set on a command line. Returns -1 if
 * the argument is definitely not set, 0 if it may be, and 1 if it
 * definitely is.
 */

static gint
arg_is_set (xmlNodePtr argument_data, GScanner *cli_db) 
{
	char *test;

	test = xmlGetProp (argument_data, "test");

	if (test) {
		return parse_sentence (test, cli_db) ? 1 : -1;
	} else {
		return 0;
	}
}

static void
read_boolean (GladeXML *widget_data, xmlNodePtr argument_data,
	      GScanner *cli_db) 
{
	char *widget_name;
	GtkWidget *widget;
	gint found;

	found = arg_is_set (argument_data, cli_db);

	widget_name = g_strconcat (xmlGetProp (argument_data, "id"),
				   "_widget", NULL);
	widget = glade_xml_get_widget (widget_data, widget_name);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), 
				      found >= 0);

	gtk_object_set_data (GTK_OBJECT (widget), "prop_data",
			     widget_data);
	gtk_signal_connect (GTK_OBJECT (widget), "toggled",
			    GTK_SIGNAL_FUNC (toggle_check_cb),
			    argument_data);

	if (found >= 0) {
		set_widgets_sensitive (widget_data,
				       xmlGetProp (argument_data, "enable"),
				       TRUE);
	} else {
		set_widgets_sensitive (widget_data,
				       xmlGetProp (argument_data, "enable"),
				       FALSE);
	}
}

static void
read_number (GladeXML *widget_data, xmlNodePtr argument_data,
	     GScanner *cli_db) 
{
	char *arg;
	char *arg_line;
	char **args;
	char *widget_name;
	char *from_cli_conv;
	GtkWidget *widget;
	GtkAdjustment *adjustment;
	gfloat value;

	arg_line = xmlGetProp (argument_data, "arg");
	args = g_strsplit (arg_line, " ", -1);

	arg = g_scanner_scope_lookup_symbol (cli_db, 0, args[0] + 1);
	if (!arg) return;

	widget_name = g_strconcat (xmlGetProp (argument_data, "id"),
				   "_widget", NULL);
	widget = glade_xml_get_widget (widget_data, widget_name);

	from_cli_conv = xmlGetProp (argument_data, "from-cli-conv");

	if (from_cli_conv)
		value = parse_expr (from_cli_conv, atof (arg));
	else
		value = atof (arg);

	if (GTK_IS_RANGE (widget)) {
		adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
		gtk_adjustment_set_value (adjustment, value);
	}
	else if (GTK_IS_SPIN_BUTTON (widget)) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
	}

	g_strfreev (args);
}

static void
read_select (GladeXML *widget_data, xmlNodePtr argument_data, 
	     GScanner *cli_db)
{
	xmlNodePtr node;
	gchar *widget_name;
	GtkWidget *widget, *menu;
	GList *menu_item_node;
	gint found, max_found = -1;
	int set_idx = 0, i = 0;

	widget_name = g_strconcat (xmlGetProp (argument_data, "id"),
				   "_widget", NULL);
	widget = glade_xml_get_widget (widget_data, widget_name);
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget));

	node = argument_data->childs;

	/* Get the index of the selected option */

	while (node) {
		found = arg_is_set (node, cli_db);

		if (found > max_found) {
			set_idx = i;
			max_found = found;
		}

		node = node->next; i++;
	}

	/* Enable widgets enabled by selected option and disable
	 * widgets enabled by other options; connect select and
	 * deselect signals to do the same when an option is selected 
	 */

	menu_item_node = GTK_MENU_SHELL (menu)->children;
	node = argument_data->childs; i = 0;

	while (node) {
		if (i == set_idx) {
			gtk_option_menu_set_history 
				(GTK_OPTION_MENU (widget), i);
			set_widgets_sensitive (widget_data,
					       xmlGetProp (node, "enable"),
					       TRUE);
		} else {
			set_widgets_sensitive (widget_data,
					       xmlGetProp (node, "enable"),
					       FALSE);
		}

		gtk_object_set_data (GTK_OBJECT (menu_item_node->data),
				     "prop_data", widget_data);
		gtk_object_set_data (GTK_OBJECT (menu_item_node->data),
				     "option_def", node);
		gtk_object_set_data (GTK_OBJECT (menu_item_node->data),
				     "argument_data", argument_data);

		gtk_signal_connect (GTK_OBJECT (menu_item_node->data),
				    "activate", 
				    GTK_SIGNAL_FUNC (activate_option_cb),
				    NULL);

		node = node->next; menu_item_node = menu_item_node->next; i++;
	}
}

static void
place_screensaver_properties (ScreensaverPrefsDialog *dialog)
{
	xmlNodePtr node;

	node = dialog->argument_data->childs;

	while (node) {
		if (!strcmp (node->name, "boolean"))
			read_boolean (dialog->prefs_widget_data, node, 
				      dialog->cli_args_db);
		else if (!strcmp (node->name, "number"))
			read_number (dialog->prefs_widget_data, node, 
				     dialog->cli_args_db);
		else if (!strcmp (node->name, "select"))
			read_select (dialog->prefs_widget_data, node, 
				     dialog->cli_args_db);

		node = node->next;
	}
}

static gboolean
arg_mapping_exists (Screensaver *saver) 
{
	struct stat buf;
	char *filename;
	gboolean ret;

	if (!saver->name) return FALSE;

	filename = g_strconcat (SSPROP_DATADIR "/",
				saver->name, "-settings.glade", NULL);

	if (stat (filename, &buf))
		ret = FALSE;
	else
		ret = TRUE;

	g_free (filename);
	return ret;
}

static void
store_cli (ScreensaverPrefsDialog *dialog) 
{
	char *str;

	g_free (dialog->saver->command_line);

	if (dialog->prefs_widget_data) {
		dialog->saver->command_line = 
			write_command_line (dialog->saver->name, 
					    dialog->argument_data,
					    dialog->prefs_widget_data);
	} else {
		dialog->saver->command_line = 
			g_strdup (gtk_entry_get_text 
				  (GTK_ENTRY (dialog->cli_entry)));
		str = gtk_entry_get_text 
			(GTK_ENTRY (GTK_COMBO
				    (dialog->visual_combo)->entry));
		if (!strcmp (str, "Any")) {
			dialog->saver->visual = NULL;
		} else {
			dialog->saver->visual = g_strdup (str);
			g_strdown (dialog->saver->visual);
		}
	}
}

/*****************************************************************************/
/*                   Fallback when Glade definition not found                */
/*****************************************************************************/

static GtkWidget *
get_basic_screensaver_widget (ScreensaverPrefsDialog *dialog,
			      Screensaver *saver) 
{
	GtkWidget *vbox, *label;
	GList *node;

	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	if (saver->name) {
		label = gtk_label_new (_("Cannot find the data to configure this screensaver. Please edit the command line below."));
	} else {
		label = gtk_label_new (_("Please enter a command line below."));
	}

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 5);
	dialog->cli_entry = gtk_entry_new ();

	if (saver->command_line) {
		gtk_entry_set_text (GTK_ENTRY (dialog->cli_entry), 
				    saver->command_line);
	}

	gtk_box_pack_start (GTK_BOX (vbox), dialog->cli_entry,
			    TRUE, FALSE, 5);

	label = gtk_label_new (_("Visual:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1.0);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 5);

	dialog->visual_combo = gtk_combo_new ();

	node = g_list_alloc ();
	node->data = "Any";
	g_list_append (node, "Best");
	g_list_append (node, "Default");
	g_list_append (node, "Default-N");
	g_list_append (node, "GL");
	g_list_append (node, "TrueColor");
	g_list_append (node, "PseudoColor");
	g_list_append (node, "StaticGray");
	g_list_append (node, "GrayScale");
	g_list_append (node, "DirectColor");
	g_list_append (node, "Color");
	g_list_append (node, "Gray");
	g_list_append (node, "Mono");

	gtk_combo_set_popdown_strings (GTK_COMBO (dialog->visual_combo), node);
	if (saver->visual)
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO 
					       (dialog->visual_combo)->entry),
				    saver->visual);
	else
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO 
					       (dialog->visual_combo)->entry),
				    "Any");	

	gtk_box_pack_start (GTK_BOX (vbox), dialog->visual_combo,
			    TRUE, FALSE, 5);
	return vbox;
}

/*****************************************************************************/
/*                          Global dialog callbacks                          */
/*****************************************************************************/

static void
demo_cb (GtkWidget *widget, ScreensaverPrefsDialog *dialog) 
{
	store_cli (dialog);
	gtk_signal_emit (GTK_OBJECT (dialog),
			 screensaver_prefs_dialog_signals[DEMO_SIGNAL]);
	show_demo (dialog->saver);
}

static void
help_cb (GtkWidget *widget, ScreensaverPrefsDialog *dialog) 
{
	GnomeHelpMenuEntry entry;
	gchar *url;

	if (!dialog->saver->name) return;

	if (dialog->prefs_widget_data) {
		entry.name = "screensaver-properties-capplet";
		entry.path = g_strconcat (dialog->saver->name, ".html", NULL);

		if (entry.path) {
			gnome_help_display (NULL, &entry);
			g_free (entry.path);
		}
	} else {
		url = g_strconcat ("man:", dialog->saver->name, NULL);
		gnome_url_show (url);
		g_free (url);
	}
}

static void
screensaver_prop_ok_cb (GtkWidget *widget, ScreensaverPrefsDialog *dialog)
{
	store_cli (dialog);

	g_free (dialog->saver->label);
	dialog->saver->label = 
		g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)));

	gtk_signal_emit (GTK_OBJECT (dialog),
			 screensaver_prefs_dialog_signals[OK_CLICKED_SIGNAL]);

	gnome_dialog_close (GNOME_DIALOG (dialog));
}

static void
screensaver_prop_cancel_cb (GtkWidget *widget, ScreensaverPrefsDialog *dialog)
{
	gnome_dialog_close (GNOME_DIALOG (dialog));
}
