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

static CappletWidgetClass *parent_class;

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

guint
prefs_widget_get_type (void)
{
	static guint prefs_widget_type = 0;

	if (!prefs_widget_type) {
		GtkTypeInfo prefs_widget_info = {
			"PrefsWidget",
			sizeof (PrefsWidget),
			sizeof (PrefsWidgetClass),
			(GtkClassInitFunc) prefs_widget_class_init,
			(GtkObjectInitFunc) prefs_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		prefs_widget_type = 
			gtk_type_unique (capplet_widget_get_type (), 
					 &prefs_widget_info);
	}

	return prefs_widget_type;
}

static void
prefs_widget_init (PrefsWidget *prefs_widget)
{
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
				object_class->type,
				GTK_SIGNAL_OFFSET (PrefsWidgetClass, 
						   read_preferences),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, prefs_widget_signals,
				      LAST_SIGNAL);

	class->read_preferences = read_preferences;

	parent_class = CAPPLET_WIDGET_CLASS
		(gtk_type_class (capplet_widget_get_type ()));
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
			gtk_object_unref
				(GTK_OBJECT (prefs_widget->dialog_data));

		prefs_widget->dialog_data = GTK_VALUE_POINTER (*arg);

		if (prefs_widget->dialog_data) {
			gtk_object_ref 
				(GTK_OBJECT (prefs_widget->dialog_data));
			register_callbacks (prefs_widget,
					    prefs_widget->dialog_data);
			if (prefs_widget->prefs)
				gtk_signal_emit 
					(GTK_OBJECT (prefs_widget),
					 prefs_widget_signals
					 [READ_PREFERENCES], 
					 prefs_widget->prefs, NULL);
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
	g_return_val_if_fail (prefs == NULL || IS_PREFERENCES (prefs), NULL);

	return gtk_widget_new (prefs_widget_get_type (),
			       "preferences", prefs,
			       NULL);
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
		PREFS_WIDGET_CLASS (GTK_OBJECT
				    (prefs_widget)->klass)->widget_desc;

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

	widget_desc = PREFS_WIDGET_CLASS (GTK_OBJECT
					  (prefs_widget)->klass)->widget_desc;

	if (widget_desc == NULL)
		return;

	glade_xml_signal_connect_data (dialog_data, "toggled_cb",
				       GTK_SIGNAL_FUNC (toggled_cb), 
				       prefs_widget);

	for (i = 0; widget_desc[i].type != WDTYPE_NONE; i++) {
		g_return_if_fail (widget_desc[i].name != NULL);
		g_return_if_fail (widget_desc[i].get_func != NULL);
		g_return_if_fail (widget_desc[i].set_func != NULL);

		if (widget_desc[i].type == WDTYPE_OPTION) {
			menu = glade_xml_get_widget (dialog_data,
						     widget_desc[i].name);

			g_return_if_fail (menu != NULL);
			g_return_if_fail (GTK_IS_OPTION_MENU (menu));

			node = GTK_MENU_SHELL (gtk_option_menu_get_menu
					       (GTK_OPTION_MENU
						(menu)))->children;

			j = 0;

			while (node != NULL) {
				gtk_signal_connect (GTK_OBJECT (node->data),
						    "activate",
						    GTK_SIGNAL_FUNC
						    (selected_cb),
						    prefs_widget);
				gtk_object_set_data (GTK_OBJECT (node->data),
						     "index", (gpointer) j);
				gtk_object_set_data (GTK_OBJECT (node->data),
						     "name", 
						     widget_desc[i].name);
				j++;
				node = node->next;
			}
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
		PREFS_WIDGET_CLASS (GTK_OBJECT
				    (prefs_widget)->klass)->widget_desc;

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

	index = (gint) gtk_object_get_data (GTK_OBJECT (mi), "index");
	widget_name = gtk_object_get_data (GTK_OBJECT (mi), "name");
	g_return_if_fail (widget_name != NULL);
	widget_desc = find_widget_desc_with_name (prefs_widget, widget_name);
	g_return_if_fail (widget_desc != NULL);

	widget_desc->set_func (prefs_widget->prefs, index);

	preferences_changed (prefs_widget->prefs);
        capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}
