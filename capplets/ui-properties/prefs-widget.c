/* -*- mode: c; style: linux -*- */

/* prefs-widget.c
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

#include <glade/glade.h>

#include "prefs-widget.h"

#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

static widget_desc_t widget_desc[] = {
	WD_CHECK (menubar_detachable,       "menubar_detachable"),
	WD_CHECK (menubar_relief,           "menubar_relief"),
	WD_CHECK (menus_have_tearoff,       "menus_have_tearoff"),
	WD_CHECK (menus_have_icons,         "menus_have_icons"),

	WD_CHECK (statusbar_is_interactive, "statusbar_is_interactive"),
	WD_CHECK (statusbar_meter_on_left , "statusbar_meter_on_left"),
	WD_CHECK (statusbar_meter_on_right, "statusbar_meter_on_right"),

	WD_CHECK (toolbar_detachable,       "toolbar_detachable"),
	WD_CHECK (toolbar_relief,           "toolbar_relief"),
	WD_CHECK (toolbar_icons_only,       "toolbar_icons_only"),
	WD_CHECK (toolbar_text_below,       "toolbar_text_below"),

	WD_CHECK (dialog_icons,             "dialog_icons"),
	WD_CHECK (dialog_centered,          "dialog_centered"),

	WD_OPTION (dialog_position,         "dialog_position"),
	WD_OPTION (dialog_type,             "dialog_type"),
	WD_OPTION (dialog_buttons_style,    "dialog_buttons_style"),

	WD_OPTION (mdi_mode,                "mdi_mode"),
	WD_OPTION (mdi_tab_pos,             "mdi_tab_pos"),

	WD_END
};

enum {
	ARG_0,
	ARG_PREFERENCES,
	ARG_DIALOG_DATA
};

enum {
	READ_PREFERENCES,
	LAST_SIGNAL
};

static guint prefs_widget_signals[LAST_SIGNAL] = { 0 };

static GtkDialogClass *parent_class;

static void prefs_widget_init             (PrefsWidget *prefs_widget);
static void prefs_widget_class_init       (PrefsWidgetClass *class);

static void prefs_widget_set_arg          (GtkObject *object, 
					   GtkArg *arg, 
					   guint arg_id);
static void prefs_widget_get_arg          (GtkObject *object, 
					   GtkArg *arg, 
					   guint arg_id);

static void read_preferences              (PrefsWidget *prefs_widget,
					   Preferences *prefs);

static void register_callbacks            (PrefsWidget *prefs_widget,
					   GladeXML *dialog_data);

static widget_desc_t * const find_widget_desc_with_name
                                          (PrefsWidget *prefs_widget, 
					   const char *name);

static void toggled_cb                    (GtkToggleButton *tb,
					   PrefsWidget *prefs_widget);
static void selected_cb                   (GtkMenuItem *mi,
					   PrefsWidget *prefs_widget);
static void capplet_widget_state_changed  (GtkDialog *dialog, gboolean state);
static void prefs_widget_response_cb	  (PrefsWidget *prefs_widget, GtkResponseType response, gpointer data);

#define CAPPLET_WIDGET(x) GTK_DIALOG(x)

GType
prefs_widget_get_type (void)
{
	static GType prefs_widget_type = 0;

	if (!prefs_widget_type) {
		GtkTypeInfo prefs_widget_info = {
			"PrefsWidget",
			sizeof (PrefsWidget),
			sizeof (PrefsWidgetClass),
			(GtkClassInitFunc) prefs_widget_class_init,
			(GtkObjectInitFunc) prefs_widget_init,
			NULL,
			NULL
		};

		prefs_widget_type = 
			gtk_type_unique (gtk_dialog_get_type (), 
					 &prefs_widget_info);
	}

	return prefs_widget_type;
}

static void
prefs_widget_init (PrefsWidget *prefs_widget)
{
	gtk_dialog_add_buttons (GTK_DIALOG (prefs_widget),
				GTK_STOCK_HELP, GTK_RESPONSE_HELP,
				GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
				GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
				NULL);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (prefs_widget), GTK_RESPONSE_APPLY, FALSE);
	g_signal_connect (G_OBJECT (prefs_widget), "response",
			  prefs_widget_response_cb, NULL);
}

static void
prefs_widget_class_init (PrefsWidgetClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("PrefsWidget::preferences",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PREFERENCES);
	gtk_object_add_arg_type ("PrefsWidget::dialog_data",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_DIALOG_DATA);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->set_arg = prefs_widget_set_arg;
	object_class->get_arg = prefs_widget_get_arg;

	prefs_widget_signals[READ_PREFERENCES] =
		gtk_signal_new ("read-preferences",
				GTK_RUN_FIRST,
				G_OBJECT_CLASS_TYPE (G_OBJECT_CLASS (object_class)),
				GTK_SIGNAL_OFFSET (PrefsWidgetClass, 
						   read_preferences),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	class->read_preferences = read_preferences;

	parent_class = GTK_DIALOG_CLASS 
		(g_type_class_ref (gtk_dialog_get_type ()));

	class->widget_desc = widget_desc;
}

static void
prefs_widget_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	PrefsWidget *prefs_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (object));

	prefs_widget = PREFS_WIDGET (object);

	switch (arg_id) {
	case ARG_PREFERENCES:
		if (prefs_widget->prefs)
			gtk_object_unref (GTK_OBJECT (prefs_widget->prefs));

		prefs_widget->prefs = GTK_VALUE_POINTER (*arg);

		if (prefs_widget->prefs) {
			gtk_object_ref (GTK_OBJECT (prefs_widget->prefs));
			if (prefs_widget->dialog_data)
				gtk_signal_emit 
					(GTK_OBJECT (prefs_widget),
					 prefs_widget_signals
					 [READ_PREFERENCES], 
					 prefs_widget->prefs, NULL);
		}

		break;

	case ARG_DIALOG_DATA:
		if (prefs_widget->dialog_data)
			g_object_unref
				(G_OBJECT (prefs_widget->dialog_data));

		prefs_widget->dialog_data = GTK_VALUE_POINTER (*arg);

		if (prefs_widget->dialog_data) {
			g_object_ref 
				(G_OBJECT (prefs_widget->dialog_data));
			if (prefs_widget->prefs)
				gtk_signal_emit 
					(GTK_OBJECT (prefs_widget),
					 prefs_widget_signals
					 [READ_PREFERENCES], 
					 prefs_widget->prefs, NULL);
			register_callbacks (prefs_widget,
					    prefs_widget->dialog_data);
		}

		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
prefs_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	PrefsWidget *prefs_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (object));

	prefs_widget = PREFS_WIDGET (object);

	switch (arg_id) {
	case ARG_PREFERENCES:
		GTK_VALUE_POINTER (*arg) = prefs_widget->prefs;
		break;

	case ARG_DIALOG_DATA:
		GTK_VALUE_POINTER (*arg) = prefs_widget->dialog_data;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

GtkWidget *
prefs_widget_new (Preferences *prefs) 
{
	GtkWidget *widget, *dlg_widget;
	GladeXML *dialog_data;

	g_return_val_if_fail (prefs == NULL || IS_PREFERENCES (prefs), NULL);

	dialog_data = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/behavior-properties.glade",
						    "prefs_widget", NULL);

	widget = gtk_widget_new (prefs_widget_get_type (),
				 "preferences", prefs,
				 "dialog_data", dialog_data,
				 NULL);

	dlg_widget = glade_xml_get_widget (dialog_data, "prefs_widget");
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (widget)->vbox), dlg_widget,
			    TRUE, TRUE, 0);

	return widget;
}

void
prefs_widget_set_preferences (PrefsWidget *prefs_widget, Preferences *prefs)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gtk_object_set (GTK_OBJECT (prefs_widget), "preferences", prefs, NULL);
}

static void
read_preferences (PrefsWidget *prefs_widget, Preferences *prefs) 
{
	widget_desc_t *widget_desc;
	int i;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	widget_desc = 
		PREFS_WIDGET_CLASS (G_OBJECT_GET_CLASS (G_OBJECT
				    (prefs_widget)))->widget_desc;

	g_return_if_fail (widget_desc != NULL);

	for (i = 0; widget_desc[i].type != WDTYPE_NONE; i++) {
		g_return_if_fail (widget_desc[i].name != NULL);
		g_return_if_fail (widget_desc[i].get_func != NULL);
		g_return_if_fail (widget_desc[i].set_func != NULL);

		switch (widget_desc[i].type) {
		case WDTYPE_CHECK:
			gtk_toggle_button_set_active 
				(GTK_TOGGLE_BUTTON (WID (widget_desc[i].name)),
				 widget_desc[i].get_func (prefs));
					 
			break;

		case WDTYPE_OPTION:
			gtk_option_menu_set_history
				(GTK_OPTION_MENU (WID (widget_desc[i].name)),
				 widget_desc[i].get_func (prefs));
			break;

		case WDTYPE_NONE:
			g_assert_not_reached ();
			break;
		}
	}
}

static void
register_callbacks (PrefsWidget *prefs_widget, GladeXML *dialog_data) 
{
	widget_desc_t *widget_desc;
	int i, j;
	GtkWidget *menu;
	GList *node;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (dialog_data != NULL);
	g_return_if_fail (GLADE_IS_XML (dialog_data));

	widget_desc = PREFS_WIDGET_CLASS (G_OBJECT_GET_CLASS (G_OBJECT
					  (prefs_widget)))->widget_desc;

	if (widget_desc == NULL)
		return;

	glade_xml_signal_connect_data (dialog_data, "toggled_cb",
				       GTK_SIGNAL_FUNC (toggled_cb), 
				       prefs_widget);

	for (i = 0; widget_desc[i].type != WDTYPE_NONE; i++) {
		g_return_if_fail (widget_desc[i].name != NULL);
		g_return_if_fail (widget_desc[i].get_func != NULL);
		g_return_if_fail (widget_desc[i].set_func != NULL);

		if (widget_desc[i].type != WDTYPE_OPTION)
			continue;

		menu = glade_xml_get_widget (dialog_data,
					     widget_desc[i].name);
		
		g_return_if_fail (menu != NULL);
		g_return_if_fail (GTK_IS_OPTION_MENU (menu));
		
		node = GTK_MENU_SHELL (gtk_option_menu_get_menu
				       (GTK_OPTION_MENU
					(menu)))->children;
		
		for (j = 0; node; j++, node = node->next) {
			gtk_signal_connect (GTK_OBJECT (node->data),
					    "activate",
					    GTK_SIGNAL_FUNC
					    (selected_cb),
					    prefs_widget);
			gtk_object_set_data (GTK_OBJECT (node->data),
					     "index",
					     GINT_TO_POINTER (j));
			gtk_object_set_data (GTK_OBJECT (node->data),
					     "name", 
					     widget_desc[i].name);
		}
	}
}

static widget_desc_t * const
find_widget_desc_with_name (PrefsWidget *prefs_widget, const char *name) 
{
	widget_desc_t *widget_desc;
	int i;

	g_return_val_if_fail (prefs_widget != NULL, NULL);
	g_return_val_if_fail (IS_PREFS_WIDGET (prefs_widget), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	widget_desc = 
		PREFS_WIDGET_CLASS (G_OBJECT_GET_CLASS (G_OBJECT
				    (prefs_widget)))->widget_desc;

	g_return_val_if_fail (widget_desc != NULL, NULL);

	for (i = 0; widget_desc[i].type != WDTYPE_NONE; i++) {
		if (!strcmp (widget_desc[i].name, name))
			return &(widget_desc[i]);
	}

	return NULL;
}

static void
toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	const char *widget_name;
	widget_desc_t *widget_desc;

	g_return_if_fail (tb != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (tb));
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));

	widget_name = glade_get_widget_name (GTK_WIDGET (tb));
	g_return_if_fail (widget_name != NULL);
	widget_desc = find_widget_desc_with_name (prefs_widget, widget_name);
	g_return_if_fail (widget_desc != NULL);

	widget_desc->set_func (prefs_widget->prefs,
			       gtk_toggle_button_get_active (tb));

	preferences_changed (prefs_widget->prefs);
        capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
selected_cb (GtkMenuItem *mi, PrefsWidget *prefs_widget) 
{
	const char *widget_name;
	widget_desc_t *widget_desc;
	gint index = 0;

	g_return_if_fail (mi != NULL);
	g_return_if_fail (GTK_IS_MENU_ITEM (mi));
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));

	index = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (mi), "index"));
	widget_name = gtk_object_get_data (GTK_OBJECT (mi), "name");
	g_return_if_fail (widget_name != NULL);
	widget_desc = find_widget_desc_with_name (prefs_widget, widget_name);
	g_return_if_fail (widget_desc != NULL);

	/* Only set it if it really changed */
	if (widget_desc->get_func (prefs_widget->prefs) != index)
	{
		widget_desc->set_func (prefs_widget->prefs, index);

		preferences_changed (prefs_widget->prefs);
        	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
	}
}


static void
capplet_widget_state_changed  (GtkDialog *dialog, gboolean state)
{
	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_APPLY, state);
}

static void
prefs_widget_response_cb (PrefsWidget *prefs_widget, GtkResponseType response, gpointer data)
{
	switch (response)
	{
		case GTK_RESPONSE_APPLY:
			preferences_save (prefs_widget->prefs);
			break;
		case GTK_RESPONSE_CLOSE:
			gtk_main_quit ();
			break;
	}
}
