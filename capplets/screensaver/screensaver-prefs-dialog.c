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
#   include "config.h"
#endif

#include <gnome.h>
#include <parser.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>

#include <tree.h>

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

static GnomeDialogClass *parent_class;

static void screensaver_prefs_dialog_init (ScreensaverPrefsDialog *dialog);
static void screensaver_prefs_dialog_class_init (ScreensaverPrefsDialogClass *dialog);

static void free_set_cb                  (gchar *key, 
					  PrefsDialogWidgetSet *set, 
					  gpointer data);

static void set_widgets_sensitive        (GTree *widget_db,
					  gchar *widgets_str, 
					  gboolean s);

static void activate_option_cb           (GtkWidget *widget);
static void toggle_check_cb              (GtkWidget *widget, 
					  xmlNodePtr node);

static gchar *write_boolean              (xmlNodePtr arg_def, 
					  GTree *widget_db);
static gchar *write_number               (xmlNodePtr arg_def, 
					  GTree *widget_db);
static gchar *write_select               (xmlNodePtr arg_def, 
					  GTree *widget_db);
static gchar *write_command_line         (gchar *name, 
					  xmlNodePtr arg_def, 
					  GTree *widget_db);

static GScanner *read_command_line       (char *command_line);
static xmlDocPtr get_argument_data       (Screensaver *saver);

static PrefsDialogWidgetSet *get_spinbutton (xmlNodePtr node,
					     GtkWidget **widget);
static PrefsDialogWidgetSet *get_check_button (ScreensaverPrefsDialog *dialog,
					       xmlNodePtr node, 
					       GtkWidget **widget);
static PrefsDialogWidgetSet *get_select_widget (ScreensaverPrefsDialog *dialog,
						xmlNodePtr select_data, 
						GtkWidget **widget);

static PrefsDialogWidgetSet *place_number (GtkTable *table, 
					   xmlNodePtr node, 
					   gint *row);
static PrefsDialogWidgetSet *place_boolean (ScreensaverPrefsDialog *dialog,
					    GtkTable *table, 
					    xmlNodePtr node, 
					    gint *row);
static void place_hgroup                 (ScreensaverPrefsDialog *dialog,
					  GtkTable *table, 
					  GTree *widget_db, 
					  xmlNodePtr hgroup_data, 
					  gint *row);
static PrefsDialogWidgetSet *place_select (ScreensaverPrefsDialog *dialog,
					   GtkTable *table, 
					   xmlNodePtr node, 
					   gint *row);

static void populate_table               (ScreensaverPrefsDialog *dialog,
					  GtkTable *table);

static GtkWidget *get_screensaver_widget (ScreensaverPrefsDialog *dialog);

static GtkWidget *get_basic_screensaver_widget 
                                         (ScreensaverPrefsDialog *dialog);

static gint arg_is_set                   (xmlNodePtr argument_data, 
					  GScanner *cli_db);
static void read_boolean                 (GTree *widget_db, 
					  xmlNodePtr argument_data,
					  GScanner *cli_db);
static void read_number                  (GTree *widget_db, 
					  xmlNodePtr argument_data,
					  GScanner *cli_db);
static void read_select                  (GTree *widget_db, 
					  xmlNodePtr argument_data, 
					  GScanner *cli_db);
static void place_screensaver_properties (ScreensaverPrefsDialog *dialog,
					  xmlNodePtr argument_data);

static gboolean arg_mapping_exists       (Screensaver *saver);

static void store_cli                    (ScreensaverPrefsDialog *dialog);

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

	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, TRUE);

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

	object_class->destroy = 
		(void (*) (GtkObject *)) screensaver_prefs_dialog_destroy;

	class->ok_clicked = NULL;

	parent_class = gtk_type_class (gnome_dialog_get_type ());
}

GtkWidget *
screensaver_prefs_dialog_new (Screensaver *saver) 
{
	GtkWidget *widget, *settings_widget;
	ScreensaverPrefsDialog *dialog;
	char *title;

	widget = gtk_type_new (screensaver_prefs_dialog_get_type ());
	dialog = SCREENSAVER_PREFS_DIALOG (widget);

	dialog->saver = saver;

	title = g_strdup_printf ("%s properties", saver->label);
	gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), 
			    saver->label);

	if (arg_mapping_exists (saver)) {
		dialog->cli_args_db = 
			read_command_line (saver->command_line);
		dialog->argument_doc = get_argument_data (saver);

		if (dialog->argument_doc)
			dialog->argument_data = 
				xmlDocGetRootElement (dialog->argument_doc);
	}

	if (dialog->cli_args_db && dialog->argument_data && dialog->argument_data->childs && dialog->argument_data->childs->next) {
		settings_widget = 
			get_screensaver_widget (dialog);
	} else {
		dialog->basic_widget = settings_widget =
			get_basic_screensaver_widget (dialog);
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

	gtk_container_add (GTK_CONTAINER (dialog->settings_dialog_frame),
			   settings_widget);

	gtk_label_set_text (GTK_LABEL (dialog->description), 
			    screensaver_get_desc (saver));

	if (dialog->argument_data)
		place_screensaver_properties (dialog, dialog->argument_data);

	return widget;
}

void
screensaver_prefs_dialog_destroy (ScreensaverPrefsDialog *dialog) 
{
	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_SCREENSAVER_PREFS_DIALOG (dialog));

	if (dialog->argument_doc)
		xmlFreeDoc (dialog->argument_doc);

	if (dialog->widget_db) {
		g_tree_traverse (dialog->widget_db, 
			 (GTraverseFunc) free_set_cb,
			 G_IN_ORDER, NULL);
		g_tree_destroy (dialog->widget_db);
	}

	if (dialog->cli_args_db)
		g_scanner_destroy (dialog->cli_args_db);

	GTK_OBJECT_CLASS (parent_class)->destroy (GTK_OBJECT (dialog));
}

static void
free_set_cb (gchar *key, PrefsDialogWidgetSet *set, gpointer data) 
{
	if (!set->alias)
		g_list_free (set->widgets);

	g_free (set);
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
set_widgets_sensitive (GTree *widget_db,
		       gchar *widgets_str, gboolean s) 
{
	char **widgets, *str;
	int i;
	PrefsDialogWidgetSet *set;
	GList *node;

	g_return_if_fail (widget_db != NULL);

	if (!widgets_str) return;

	widgets = g_strsplit (widgets_str, ",", -1);

	for (i = 0; widgets[i]; i++) {
		str = widgets[i];

		while (isspace (*str)) str++;
		set = g_tree_lookup (widget_db, str);
		if (!set) continue;

		set->enabled = s;

		for (node = set->widgets; node; node = node->next)
			gtk_widget_set_sensitive (GTK_WIDGET (node->data), s);
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
	ScreensaverPrefsDialog *dialog;
	xmlNodePtr select_data, option_def, node;

	dialog = gtk_object_get_data (GTK_OBJECT (widget), "dialog");
	option_def = gtk_object_get_data (GTK_OBJECT (widget), "option_def");
	select_data = gtk_object_get_data (GTK_OBJECT (widget), "select_data");

	node = select_data->childs;

	while (node) {
		if (node != option_def)
			set_widgets_sensitive (dialog->widget_db, 
					       xmlGetProp (node, "enable"), 
					       FALSE);
		node = node->next;
	}

	set_widgets_sensitive (dialog->widget_db, 
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
	ScreensaverPrefsDialog *dialog;
	gboolean set;

	dialog = gtk_object_get_data (GTK_OBJECT (widget), "dialog");
	set = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

	set_widgets_sensitive (dialog->widget_db, 
			       xmlGetProp (node, "enable"), set);
}

/*****************************************************************************/
/*                        Writing out command lines                          */
/*****************************************************************************/

static gchar *
write_boolean (xmlNodePtr argument_data, GTree *widget_db) 
{
	PrefsDialogWidgetSet *set;
	char *id;

	if (!(id = xmlGetProp (argument_data, "id"))) return NULL;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->enabled) return NULL;

	if (gtk_toggle_button_get_active 
	    (GTK_TOGGLE_BUTTON (set->value_widget)))
		return xmlGetProp (argument_data, "arg-set");
	else 
		return xmlGetProp (argument_data, "arg-unset");
}

static gchar *
write_number (xmlNodePtr argument_data, GTree *widget_db)
{
	PrefsDialogWidgetSet *set;
	GtkAdjustment *adjustment = NULL;
	gfloat value = 0.0;
	gchar *to_cli_expr;
	char *id, *arg, *ret_str;
	char *pos;

	if (!(id = xmlGetProp (argument_data, "id"))) return NULL;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->enabled) return NULL;

	if (GTK_IS_RANGE (set->value_widget))
		adjustment = gtk_range_get_adjustment 
			(GTK_RANGE (set->value_widget));
	else if (GTK_IS_SPIN_BUTTON (set->value_widget))
		adjustment = gtk_spin_button_get_adjustment 
			(GTK_SPIN_BUTTON (set->value_widget));
	if (adjustment)
		value = adjustment->value;

	to_cli_expr = xmlGetProp (argument_data, "to-cli-conv");
	if (to_cli_expr)
		value = parse_expr (to_cli_expr, value);

	arg = xmlGetProp (argument_data, "arg");

	if (!arg) return NULL;
	arg = g_strdup (arg);

	pos = strchr (arg, '%');

	if (!pos) return arg;
	*pos = '\0';

	ret_str = g_strdup_printf ("%s%d%s", arg, (int) value, pos + 1);
	g_free (arg);

	return ret_str;
}

/* Note to readers: *please* ignore the following function, for the
 * sake of your own mental health and my reputation as a sane
 * coder. Just accept that it returns a string containing the CLI
 * arguments corresponding with the selected option of an option
 * menu. *sigh* I really must ask someone if there's a better way to
 * do this...
 */

static gchar *
write_select (xmlNodePtr argument_data, GTree *widget_db) 
{
	PrefsDialogWidgetSet *set;
	xmlNodePtr node;
	int i = 0;
	GtkWidget *menu, *active;
	GList *menu_item;
	char *id;

	if (!(id = xmlGetProp (argument_data, "id"))) return NULL;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->enabled) return NULL;
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (set->value_widget));
	menu_item = GTK_MENU_SHELL (menu)->children;
	active = gtk_menu_get_active (GTK_MENU (menu));

	for (node = argument_data->childs; node && menu_item;
	     node = node->next) 
	{
		if (!xmlGetProp (node, "id")) continue;

		if (active == menu_item->data)
			return xmlGetProp (node, "arg-set");

		menu_item = menu_item->next; i++;
	}

	return NULL;
}

static gchar *
write_string (xmlNodePtr argument_data, GTree *widget_db) 
{
	PrefsDialogWidgetSet *set;
	gchar *str;
	char *id, *arg, *ret_str;
	char *pos;

	if (!(id = xmlGetProp (argument_data, "id"))) return NULL;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->enabled) return NULL;

	str = gtk_entry_get_text (GTK_ENTRY (set->value_widget));
	arg = xmlGetProp (argument_data, "arg");

	if (!arg) return NULL;
	arg = g_strdup (arg);

	pos = strchr (arg, '%');

	if (!pos) return arg;
	*pos = '\0';

	ret_str = g_strdup_printf ("%s\"%s\"%s", arg, str, pos + 1);
	g_free (arg);

	return ret_str;
}

/* write_command_line
 *
 * Scan through XML nodes in the argument definition file and write
 * out the parameter corresponding to each node in turn.
 */

static gchar *
write_command_line (gchar *name, xmlNodePtr argument_data, GTree *widget_db) 
{
	GString *line;
	xmlNodePtr node;
	gchar *arg, *ret;
	gboolean flag = FALSE;
	gboolean free_v = FALSE;

	line = g_string_new (name);
	node = argument_data->childs;

	for (node = argument_data->childs; node; node = node->next) {
		if (!strcmp (node->name, "boolean")) {
			arg = write_boolean (node, widget_db);
			free_v = FALSE;
		}
		else if (!strcmp (node->name, "number")) {
			arg = write_number (node, widget_db);
			free_v = TRUE;
		}
		else if (!strcmp (node->name, "select")) {
			arg = write_select (node, widget_db);
			free_v = FALSE;
		}
		else if (!strcmp (node->name, "string") ||
			 !strcmp (node->name, "file")) 
		{
			arg = write_string (node, widget_db);
			free_v = TRUE;
		}
		else if (!strcmp (node->name, "command")) {
			arg = xmlGetProp (node, "arg");
			free_v = FALSE;
		}
		else if (!strcmp (node->name, "hgroup")) {
			arg = write_command_line (NULL, node, widget_db);
			free_v = TRUE;
		} else {
			arg = NULL;
			free_v = FALSE;
		}

		if (arg) {
			if (*arg && (name || flag)) 
				g_string_append (line, " ");
			g_string_append (line, arg);
			flag = TRUE;

			if (free_v) g_free (arg);
		}
	}

	ret = line->str;
	g_string_free (line, FALSE);
	g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "Command line is %s", ret);
	return ret;
}

/*****************************************************************************/
/*                         Reading the command line                          */
/*****************************************************************************/

static GScanner *
read_command_line (char *command_line) 
{
	GScanner *cli_db;
	static GScannerConfig config;
	char *arg, *value, *argpos, *valpos, *endpos;

	config.cset_skip_characters = " \t\n";
	config.cset_identifier_first = "abcdefghijklmnopqrstuvwxyz";
	config.cset_identifier_nth = "abcdefghijklmnopqrstuvwxyz_";
	config.scan_symbols = TRUE;
	config.scan_identifier = TRUE;

	cli_db = g_scanner_new (&config);
	g_scanner_set_scope (cli_db, 0);

	g_scanner_scope_add_symbol (cli_db, 0, "and", SYMBOL_AND);
	g_scanner_scope_add_symbol (cli_db, 0, "or", SYMBOL_OR);
	g_scanner_scope_add_symbol (cli_db, 0, "not", SYMBOL_NOT);

	command_line = g_strdup (command_line);
	argpos = command_line;

	while (argpos && *argpos) {
		if (*argpos == '-') {
			valpos = strchr (argpos, ' ');
			if (valpos) *valpos = '\0';
			arg = g_strdup (argpos + 1);

			if (valpos) {
				valpos++;
				while (isspace (*valpos)) valpos++;

				if (*valpos == '\"') {
					endpos = strchr (valpos + 1, '\"');

					if (endpos) {
						*endpos = '\0';
						endpos++;
					}

					value = g_strdup (valpos + 1);

					if (endpos) {
						endpos++;
						while (isspace (*endpos)) 
							endpos++;
					}
				} else if (*valpos != '-' || 
					   isdigit(valpos[1])) 
				{
					endpos = strchr (valpos, ' ');
					if (endpos) *endpos = '\0';
					value = g_strdup (valpos);

					if (endpos) {
						endpos++;
						while (isspace (*endpos)) 
							endpos++;
					}
				} else {
					value = (char *) 1;
					endpos = valpos;
				}

				if (endpos)
					argpos = endpos;
				else
					argpos = NULL;
			} else {
				value = (char *) 1;
				argpos = NULL;
			}

			g_scanner_scope_add_symbol (cli_db, 0, arg, value);
		} else {
			argpos = strchr (argpos, ' ');
			if (argpos)
				while (isspace (*argpos)) argpos++;
		}
	}

	g_free (command_line);

	return cli_db;
}

/*****************************************************************************/
/*                    Getting the argument definition data                   */
/*****************************************************************************/

static xmlDocPtr
get_argument_data (Screensaver *saver) 
{
	xmlDocPtr doc;
	gchar *file_name;
	gchar *lang;

	g_return_val_if_fail (saver != NULL, NULL);
	g_return_val_if_fail (saver->name != NULL, NULL);

	lang = g_getenv ("LANG");
	if (lang) 
		lang = g_strconcat (lang, "/", NULL);
	else
		lang = g_strdup ("");

	file_name = g_strconcat (GNOMECC_SCREENSAVERS_DIR "/screensavers/",
				 lang, saver->name, ".xml", NULL);
	doc = xmlParseFile (file_name);
	g_free (file_name);

	/* Fall back on default language if given language is not found */
	if (!doc && *lang != '\0') {
		file_name = g_strconcat (GNOMECC_SCREENSAVERS_DIR "/screensavers/",
					 saver->name, ".xml", NULL);
		doc = xmlParseFile (file_name);
		g_free (file_name);
	}

	g_free (lang);

	return doc;
}

/*****************************************************************************/
/*                       Creating the dialog proper                          */
/*****************************************************************************/

/* Form a spinbutton widget and return the widget set */

static PrefsDialogWidgetSet *
get_spinbutton (xmlNodePtr node, GtkWidget **widget) 
{
	char *label_str, *low_val, *high_val, *default_val;
	gdouble low, high, defaultv;
	GtkWidget *hbox, *label, *spinbutton;
	GtkAdjustment *adjustment;
	PrefsDialogWidgetSet *set;

	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (node != NULL, NULL);

	label_str = xmlGetProp (node, "_label");
	low_val = xmlGetProp (node, "low");
	high_val = xmlGetProp (node, "high");
	default_val = xmlGetProp (node, "default");

	if (!low_val || !high_val) return NULL;

	low = atof (low_val);
	high = atof (high_val);

	if (default_val)
		defaultv = atof (default_val);
	else
		defaultv = (high - low) / 2;

	adjustment = GTK_ADJUSTMENT
		(gtk_adjustment_new (defaultv, low, high, 1.0, 10.0, 10.0));
	spinbutton = gtk_spin_button_new (adjustment, 1.0, 0);
	
	set = g_new0 (PrefsDialogWidgetSet, 1);
	set->alias = FALSE;
	set->enabled = TRUE;
	set->value_widget = spinbutton;

	if (label_str) {
		hbox = gtk_hbox_new (FALSE, 5);
		label = gtk_label_new (_(label_str));
		gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), spinbutton, 
				    FALSE, TRUE, 0);

		set->widgets = g_list_append (NULL, hbox);
		g_list_append (set->widgets, label);
		g_list_append (set->widgets, spinbutton);

		*widget = hbox;
	} else {
		set->widgets = g_list_append (NULL, spinbutton);
		*widget = spinbutton;
	}

	return set;
}

/* Form a check button widget for a boolean value */

static PrefsDialogWidgetSet *
get_check_button (ScreensaverPrefsDialog *dialog, xmlNodePtr node, 
		  GtkWidget **widget) 
{
	char *label;
	GtkWidget *checkbutton;
	PrefsDialogWidgetSet *set;

	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (node != NULL, NULL);

	label = xmlGetProp (node, "_label");

	if (!label) return NULL;

	checkbutton = gtk_check_button_new_with_label (_(label));

	set = g_new0 (PrefsDialogWidgetSet, 1);
	set->alias = FALSE;
	set->enabled = TRUE;
	set->widgets = g_list_append (NULL, checkbutton);
	set->value_widget = checkbutton;

	gtk_object_set_data (GTK_OBJECT (checkbutton), "dialog", dialog);
	gtk_object_set_data (GTK_OBJECT (checkbutton), "option_def", node);

	gtk_signal_connect (GTK_OBJECT (checkbutton), "toggled",
			    GTK_SIGNAL_FUNC (toggle_check_cb), node);

	*widget = checkbutton;

	return set;
}

/* Form a selection widget for an option menu */

static PrefsDialogWidgetSet *
get_select_widget (ScreensaverPrefsDialog *dialog, xmlNodePtr select_data, 
		   GtkWidget **widget) 
{
	char *label_str, *option_str;
	GtkWidget *hbox, *label, *menu, *menu_item, *option_menu;
	PrefsDialogWidgetSet *set;
	xmlNodePtr node;

	g_return_val_if_fail (widget != NULL, NULL);

	label_str = xmlGetProp (select_data, "_label");

	option_menu = gtk_option_menu_new ();
	menu = gtk_menu_new ();
	
	set = g_new0 (PrefsDialogWidgetSet, 1);
	set->alias = FALSE;
	set->enabled = TRUE;
	set->value_widget = option_menu;

	gtk_object_set_data (GTK_OBJECT (option_menu), "dialog", dialog);
	gtk_object_set_data (GTK_OBJECT (option_menu), "option_def", 
			     select_data);

	for (node = select_data->childs; node; node = node->next) {
		option_str = xmlGetProp (node, "_label");
		if (!option_str) continue;

		menu_item = gtk_menu_item_new_with_label (_(option_str));
		gtk_widget_show (menu_item);
		gtk_object_set_data (GTK_OBJECT (menu_item), "dialog", dialog);
		gtk_object_set_data (GTK_OBJECT (menu_item),
				     "option_def", node);
		gtk_object_set_data (GTK_OBJECT (menu_item),
				     "select_data", select_data);

		gtk_signal_connect (GTK_OBJECT (menu_item),
				    "activate", 
				    GTK_SIGNAL_FUNC (activate_option_cb),
				    NULL);

		gtk_menu_append (GTK_MENU (menu), menu_item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

	if (label_str) {
		hbox = gtk_hbox_new (FALSE, 5);
		label = gtk_label_new (_(label_str));
		gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), option_menu, 
				    FALSE, TRUE, 0);

		set->widgets = g_list_append (NULL, hbox);
		g_list_append (set->widgets, label);
		g_list_append (set->widgets, option_menu);

		*widget = hbox;
	} else {
		set->widgets = g_list_append (NULL, option_menu);
		*widget = option_menu;
	}

	return set;
}

/* Form a GtkFileEntry from a string value */

static PrefsDialogWidgetSet *
get_file_entry (ScreensaverPrefsDialog *dialog, xmlNodePtr node, 
		GtkWidget **widget) 
{
	char *label_str, *default_str;
	GtkWidget *hbox, *label, *entry;
	PrefsDialogWidgetSet *set;

	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (node != NULL, NULL);

	label_str = xmlGetProp (node, "_label");
	default_str = xmlGetProp (node, "default");

	entry = gnome_file_entry_new (NULL, NULL);

	if (default_str)
		gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (entry), 
						   default_str);

	set = g_new0 (PrefsDialogWidgetSet, 1);
	set->alias = FALSE;
	set->enabled = TRUE;
	set->value_widget = 
		gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (entry));

	if (label_str) {
		hbox = gtk_hbox_new (FALSE, 5);
		label = gtk_label_new (_(label_str));
		gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
		gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), entry, 
				    FALSE, TRUE, 0);

		set->widgets = g_list_append (NULL, hbox);
		g_list_append (set->widgets, label);
		g_list_append (set->widgets, entry);

		*widget = hbox;
	} else {
		set->widgets = g_list_append (NULL, entry);
		*widget = entry;
	}

	return set;
}

/* Form a GtkEntry from a string value */

static PrefsDialogWidgetSet *
get_entry (ScreensaverPrefsDialog *dialog, xmlNodePtr node, 
	   GtkWidget **widget) 
{
	char *label_str, *default_str;
	GtkWidget *hbox, *label, *entry;
	PrefsDialogWidgetSet *set;

	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (node != NULL, NULL);

	label_str = xmlGetProp (node, "_label");
	default_str = xmlGetProp (node, "default");

	entry = gtk_entry_new ();

	if (default_str)
		gtk_entry_set_text (GTK_ENTRY (entry), default_str);

	set = g_new0 (PrefsDialogWidgetSet, 1);
	set->alias = FALSE;
	set->enabled = TRUE;
	set->value_widget = entry;

	if (label_str) {
		hbox = gtk_hbox_new (FALSE, 5);
		label = gtk_label_new (_(label_str));
		gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), entry, 
				    TRUE, TRUE, 0);

		set->widgets = g_list_append (NULL, hbox);
		g_list_append (set->widgets, label);
		g_list_append (set->widgets, entry);

		*widget = hbox;
	} else {
		set->widgets = g_list_append (NULL, entry);
		*widget = entry;
	}

	return set;
}

/* Place a set of widgets that configures a numerical value in the
 * next row of the table and update the row value
 */

static PrefsDialogWidgetSet *
place_number (GtkTable *table, xmlNodePtr node, gint *row) 
{
	char *type, *label_str, *high_str, *low_str;
	char *default_val, *high_val, *low_val;
	gdouble defaultv, high, low;
	GtkWidget *label, *hscale, *hbox;
	GtkAdjustment *adjustment;
	PrefsDialogWidgetSet *set;
	GList *list_tail;

	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TABLE (table), NULL);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (row != NULL, NULL);

	type = xmlGetProp (node, "type");

	if (!type) return NULL;

	if (!strcmp (type, "slider")) {
		label_str = xmlGetProp (node, "_label");
		low_str = xmlGetProp (node, "_low-label");
		high_str = xmlGetProp (node, "_high-label");
		default_val = xmlGetProp (node, "default");
		low_val = xmlGetProp (node, "low");
		high_val = xmlGetProp (node, "high");

		if (!label_str || !low_str || !high_str ||
		    !low_val || !high_val) 
			return NULL;

		set = g_new0 (PrefsDialogWidgetSet, 1);
		set->alias = FALSE;
		set->enabled = TRUE;
		label = gtk_label_new (_(label_str));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_table_attach (table, label, 0, 3, *row, *row + 1,
				  GTK_FILL, 0, 0, 0);
		set->widgets = list_tail = g_list_append (NULL, label);

		low = atof (low_val);
		high = atof (high_val);

		if (default_val)
			defaultv = atof (default_val);
		else
			defaultv = (high - low) / 2;

		adjustment = GTK_ADJUSTMENT
			(gtk_adjustment_new (defaultv, low, high,
					     (high - low) / 100,
					     (high - low) / 10,
					     (high - low) / 10));

		hscale = gtk_hscale_new (adjustment);
		gtk_table_attach (table, hscale, 1, 2, *row + 1, *row + 2,
				  GTK_EXPAND | GTK_FILL, 0, 0, 0);
		gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
		set->value_widget = hscale;
		list_tail = g_list_append (list_tail, hscale);
		list_tail = list_tail->next;

		label = gtk_label_new (_(low_str));
		gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
		gtk_table_attach (table, label, 0, 1, *row + 1, *row + 2,
				  GTK_FILL, 0, 0, 0);
		list_tail = g_list_append (list_tail, label);
		list_tail = list_tail->next;

		label = gtk_label_new (_(high_str));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
		gtk_table_attach (table, label, 2, 3, *row + 1, *row + 2,
				  GTK_FILL, 0, 0, 0);
		list_tail = g_list_append (list_tail, label);
		list_tail = list_tail->next;

		*row += 2;
	} 
	else if (!strcmp (type, "spinbutton")) {
		set = get_spinbutton (node, &hbox);

		if (set) {
			gtk_table_attach (table, hbox, 
					  0, 3, *row, *row + 1,
					  GTK_FILL, 0, 0, 0);
			*row += 1;
		}
	} else {
		set = NULL;
	}

	return set;
}

/* Place a widget for a boolean value in a table */

static PrefsDialogWidgetSet *
place_boolean (ScreensaverPrefsDialog *dialog, GtkTable *table,
	       xmlNodePtr node, gint *row)
{
	PrefsDialogWidgetSet *set;
	GtkWidget *widget;

	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TABLE (table), NULL);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (row != NULL, NULL);

	set = get_check_button (dialog, node, &widget);

	if (set) {
		gtk_table_attach (table, widget, 0, 3, *row, *row + 1,
				  GTK_FILL, 0, 0, 0);
		*row += 1;
	}

	return set;
}

/* Place a horizontal group in a table */

static void
place_hgroup (ScreensaverPrefsDialog *dialog, 
	      GtkTable *table, GTree *widget_db, 
	      xmlNodePtr hgroup_data, gint *row) 
{
	PrefsDialogWidgetSet *set, *set1;
	GtkWidget *hbox, *widget;
	xmlNodePtr node;
	gchar *id, *same_as;

	g_return_if_fail (table != NULL);
	g_return_if_fail (GTK_IS_TABLE (table));
	g_return_if_fail (widget_db != NULL);
	g_return_if_fail (hgroup_data != NULL);
	g_return_if_fail (row != NULL);

	hbox = gtk_hbox_new (FALSE, 5);

	for (node = hgroup_data->childs; node; node = node->next) {
		id = xmlGetProp (node, "id");
		if (!id) continue;

		same_as = xmlGetProp (node, "same-as");

		if (same_as != NULL && *same_as != '\0') {
			set1 = g_tree_lookup (dialog->widget_db, same_as);
			if (set1 == NULL) continue;
			set = g_new0 (PrefsDialogWidgetSet, 1);
			set->alias = TRUE;
			set->enabled = TRUE;
			set->value_widget = set1->value_widget;
			set->widgets = set1->widgets;
			g_tree_insert (dialog->widget_db, id, set);
			continue;
		}

		if (!strcmp (node->name, "number"))
			set = get_spinbutton (node, &widget);
		else if (!strcmp (node->name, "boolean"))
			set = get_check_button (dialog, node, &widget);
		else if (!strcmp (node->name, "select"))
			set = get_select_widget (dialog, node, &widget);
		else continue;

		if (set != NULL && widget != NULL) {
			g_tree_insert (widget_db, id, set);
			gtk_box_pack_start (GTK_BOX (hbox), widget,
					    FALSE, TRUE, 0);
		}
	}

	gtk_table_attach (table, hbox, 0, 3, *row, *row + 1, 
			  0, 0, 0, 0);
	*row += 1;
}

/* Place a selection list (option menu) in a table */

static PrefsDialogWidgetSet *
place_select (ScreensaverPrefsDialog *dialog, GtkTable *table,
	      xmlNodePtr node, gint *row)
{
	PrefsDialogWidgetSet *set;
	GtkWidget *widget;

	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TABLE (table), NULL);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (row != NULL, NULL);

	set = get_select_widget (dialog, node, &widget);

	if (set) {
		gtk_table_attach (table, widget, 0, 3, *row, *row + 1,
				  GTK_FILL, 0, 0, GNOME_PAD_SMALL);
		*row += 1;
	}

	return set;
}

/* Place a GtkEntry or a GnomeFileEntry in a table */

static PrefsDialogWidgetSet *
place_entry (ScreensaverPrefsDialog *dialog, GtkTable *table,
	     xmlNodePtr node, gint *row, gboolean is_file) 
{
	PrefsDialogWidgetSet *set;
	GtkWidget *widget;

	g_return_val_if_fail (table != NULL, NULL);
	g_return_val_if_fail (GTK_IS_TABLE (table), NULL);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (row != NULL, NULL);

	if (is_file)
		set = get_file_entry (dialog, node, &widget);
	else
		set = get_entry (dialog, node, &widget);

	if (set) {
		gtk_table_attach (table, widget, 0, 3, *row, *row + 1,
				  GTK_FILL, 0, 0, 0);
		*row += 1;
	}

	return set;
}

/* Fill a GtkTable with widgets based on the XML description of the
 * screensaver 
 */

static void
populate_table (ScreensaverPrefsDialog *dialog, GtkTable *table) 
{
	char *id, *same_as;
	PrefsDialogWidgetSet *set, *set1;
	xmlNodePtr node;
	gint row = 0;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (IS_SCREENSAVER_PREFS_DIALOG (dialog));
	g_return_if_fail (dialog->widget_db != NULL);
	g_return_if_fail (dialog->argument_data != NULL);
	g_return_if_fail (table != NULL);
	g_return_if_fail (GTK_IS_TABLE (table));

	for (node = dialog->argument_data->childs; node; node = node->next) {
		id = xmlGetProp (node, "id");
		if (!id && strcmp (node->name, "hgroup")) continue;

		set = NULL;

		same_as = xmlGetProp (node, "same-as");

		if (same_as != NULL && *same_as != '\0') {
			set1 = g_tree_lookup (dialog->widget_db, same_as);
			if (set1 == NULL) continue;
			set = g_new0 (PrefsDialogWidgetSet, 1);
			set->alias = TRUE;
			set->enabled = TRUE;
			set->value_widget = set1->value_widget;
			set->widgets = set1->widgets;
			g_tree_insert (dialog->widget_db, id, set);
			continue;
		}

		if (!strcmp (node->name, "number"))
			set = place_number (table, node, &row);
		else if (!strcmp (node->name, "boolean"))
			set = place_boolean (dialog, table, node, &row);
		else if (!strcmp (node->name, "hgroup"))
			place_hgroup (dialog, table, dialog->widget_db, 
				      node, &row);
		else if (!strcmp (node->name, "select"))
			set = place_select (dialog, table, node, &row);
		else if (!strcmp (node->name, "string"))
			set = place_entry (dialog, table, node, &row, FALSE);
		else if (!strcmp (node->name, "file"))
			set = place_entry (dialog, table, node, &row, TRUE);
		else continue;

		if (set) g_tree_insert (dialog->widget_db, id, set);
	}
}

static GtkWidget *
get_screensaver_widget (ScreensaverPrefsDialog *dialog) 
{
	GtkWidget *table;

	dialog->widget_db = g_tree_new ((GCompareFunc) strcmp);

	table = gtk_table_new (1, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (table), 
					GNOME_PAD_SMALL);
	populate_table (dialog, GTK_TABLE (table));

	return table;
}

/* Fallback when Glade definition not found */

static GtkWidget *
get_basic_screensaver_widget (ScreensaverPrefsDialog *dialog) 
{
	GtkWidget *vbox, *label;
	GList *node;

	vbox = gtk_vbox_new (FALSE, 10);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	label = gtk_label_new (_("There are no configurable settings for this screensaver."));

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 5);
	gtk_widget_show (label);

#if 0
	if (dialog->saver->name) {
		label = gtk_label_new 
			(_("Cannot find the data to configure this " \
			   "screensaver. Please edit the command line " \
			   "below."));
	} else {
		label = gtk_label_new 
			(_("Please enter a command line below."));
	}

	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 5);
	dialog->cli_entry = gtk_entry_new ();

	if (dialog->saver->command_line) {
		gtk_entry_set_text (GTK_ENTRY (dialog->cli_entry), 
				    dialog->saver->command_line);
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
	if (dialog->saver->visual)
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO 
					       (dialog->visual_combo)->entry),
				    dialog->saver->visual);
	else
		gtk_entry_set_text (GTK_ENTRY (GTK_COMBO 
					       (dialog->visual_combo)->entry),
				    _("Any"));

	gtk_box_pack_start (GTK_BOX (vbox), dialog->visual_combo,
			    TRUE, FALSE, 5);
#endif
	return vbox;
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

	g_return_val_if_fail (argument_data != NULL, 0);
	g_return_val_if_fail (cli_db != NULL, 0);

	test = xmlGetProp (argument_data, "test");

	if (test) {
		return parse_sentence (test, cli_db) ? 1 : -1;
	} else {
		return 0;
	}
}

static void
read_boolean (GTree *widget_db, xmlNodePtr argument_data, GScanner *cli_db) 
{
	char *id;
	PrefsDialogWidgetSet *set;
	gint found;

	g_return_if_fail (widget_db != NULL);
	g_return_if_fail (argument_data != NULL);
	g_return_if_fail (cli_db != NULL);

	found = arg_is_set (argument_data, cli_db);

	if (!(id = xmlGetProp (argument_data, "id"))) return;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->value_widget || (set->alias && !set->enabled))
		return;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (set->value_widget), 
				      found >= 0);

	if (found >= 0) {
		set_widgets_sensitive (widget_db,
				       xmlGetProp (argument_data, "enable"),
				       TRUE);
	} else {
		set_widgets_sensitive (widget_db,
				       xmlGetProp (argument_data, "enable"),
				       FALSE);
	}
}

static void
read_number (GTree *widget_db, xmlNodePtr argument_data, GScanner *cli_db) 
{
	PrefsDialogWidgetSet *set;
	char *arg, *id;
	char *arg_line;
	char **args;
	char *from_cli_conv;
	GtkAdjustment *adjustment;
	gfloat value;

	g_return_if_fail (widget_db != NULL);
	g_return_if_fail (argument_data != NULL);
	g_return_if_fail (cli_db != NULL);

	arg_line = xmlGetProp (argument_data, "arg");
	args = g_strsplit (arg_line, " ", -1);

	arg = g_scanner_scope_lookup_symbol (cli_db, 0, args[0] + 1);

	if (!arg || arg == (char *) 1)
		arg = xmlGetProp (argument_data, "default");
	if (!arg)
		return;

	if (!(id = xmlGetProp (argument_data, "id"))) return;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->value_widget || (set->alias && !set->enabled))
		return;

	from_cli_conv = xmlGetProp (argument_data, "from-cli-conv");

	if (from_cli_conv)
		value = parse_expr (from_cli_conv, atof (arg));
	else
		value = atof (arg);

	if (GTK_IS_RANGE (set->value_widget)) {
		adjustment = gtk_range_get_adjustment 
			(GTK_RANGE (set->value_widget));
		gtk_adjustment_set_value (adjustment, value);
	}
	else if (GTK_IS_SPIN_BUTTON (set->value_widget)) {
		gtk_spin_button_set_value 
			(GTK_SPIN_BUTTON (set->value_widget), value);
	}

	g_strfreev (args);
}

static void
read_select (GTree *widget_db, xmlNodePtr argument_data, 
	     GScanner *cli_db)
{
	PrefsDialogWidgetSet *set;
	xmlNodePtr node;
	GtkWidget *menu;
	gint found, max_found = -1;
	int set_idx = 0, i = 0;
	char *id;

	g_return_if_fail (widget_db != NULL);
	g_return_if_fail (argument_data != NULL);
	g_return_if_fail (cli_db != NULL);

	if (!(id = xmlGetProp (argument_data, "id"))) return;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->value_widget || (set->alias && !set->enabled))
		return;

	menu = gtk_option_menu_get_menu 
		(GTK_OPTION_MENU (set->value_widget));

	/* Get the index of the selected option */

	for (node = argument_data->childs; node; node = node->next) {
		if (!xmlGetProp (node, "id")) continue;

		found = arg_is_set (node, cli_db);

		if (found > max_found) {
			set_idx = i;
			max_found = found;
		}

		i++;
	}

	/* Enable widgets enabled by selected option and disable
	 * widgets enabled by other options; connect select and
	 * deselect signals to do the same when an option is selected 
	 */

	i = 0;

	for (node = argument_data->childs; node; node = node->next) {
		if (!xmlGetProp (node, "id")) continue;

		if (i != set_idx)
			set_widgets_sensitive (widget_db,
					       xmlGetProp (node, "enable"),
					       FALSE);
		i++;
	}

	i = 0;

	for (node = argument_data->childs; node; node = node->next) {
		if (!xmlGetProp (node, "id")) continue;

		if (i == set_idx) {
			gtk_option_menu_set_history 
				(GTK_OPTION_MENU (set->value_widget), i);
			set_widgets_sensitive (widget_db,
					       xmlGetProp (node, "enable"),
					       TRUE);
			break;
		}

		i++;
	}
}

static void
read_string (GTree *widget_db, xmlNodePtr argument_data, GScanner *cli_db,
	     gboolean is_file) 
{
	PrefsDialogWidgetSet *set;
	char *arg, *id;
	char *arg_line;
	char **args;

	g_return_if_fail (widget_db != NULL);
	g_return_if_fail (argument_data != NULL);
	g_return_if_fail (cli_db != NULL);

	arg_line = xmlGetProp (argument_data, "arg");
	args = g_strsplit (arg_line, " ", -1);

	arg = g_scanner_scope_lookup_symbol (cli_db, 0, args[0] + 1);

	if (!arg || arg == (char *) 1)
		arg = xmlGetProp (argument_data, "default");
	if (!arg)
		return;

	if (!(id = xmlGetProp (argument_data, "id"))) return;
	set = g_tree_lookup (widget_db, id);

	if (!set || !set->value_widget || (set->alias && !set->enabled))
		return;

	gtk_entry_set_text (GTK_ENTRY (set->value_widget), arg);

	g_strfreev (args);
}

static void
place_screensaver_properties (ScreensaverPrefsDialog *dialog,
			      xmlNodePtr argument_data)
{
	xmlNodePtr node;

	for (node = argument_data->childs; node; node = node->next) {
		if (!strcmp (node->name, "boolean"))
			read_boolean (dialog->widget_db, node, 
				      dialog->cli_args_db);
		else if (!strcmp (node->name, "number"))
			read_number (dialog->widget_db, node, 
				     dialog->cli_args_db);
		else if (!strcmp (node->name, "select"))
			read_select (dialog->widget_db, node, 
				     dialog->cli_args_db);
		else if (!strcmp (node->name, "string"))
			read_string (dialog->widget_db, node, 
				     dialog->cli_args_db, FALSE);
		else if (!strcmp (node->name, "file"))
			read_string (dialog->widget_db, node, 
				     dialog->cli_args_db, TRUE);
		else if (!strcmp (node->name, "hgroup"))
			place_screensaver_properties (dialog, node);
	}
}

static gboolean
arg_mapping_exists (Screensaver *saver) 
{
	struct stat buf;
	char *filename;
	gboolean ret;

	if (!saver->name) return FALSE;

	filename = g_strconcat (GNOMECC_SCREENSAVERS_DIR "/screensavers/",
				saver->name, ".xml", NULL);

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

	if (dialog->widget_db) {
		dialog->saver->command_line = 
			write_command_line (dialog->saver->name, 
					    dialog->argument_data,
					    dialog->widget_db);
	} else {
		dialog->saver->command_line = 
			g_strdup (gtk_entry_get_text 
				  (GTK_ENTRY (dialog->cli_entry)));
		str = gtk_entry_get_text 
			(GTK_ENTRY (GTK_COMBO
				    (dialog->visual_combo)->entry));
		if (!strcmp (str, _("Any"))) {
			dialog->saver->visual = NULL;
		} else {
			dialog->saver->visual = g_strdup (str);
			g_strdown (dialog->saver->visual);
		}
	}
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

	if (dialog->widget_db) {
		entry.name = "screensaver-properties-capplet";
		entry.path = g_strconcat (dialog->saver->name, ".xml", NULL);

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
