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
# include <config.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "prefs-widget.h"


#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

enum {
	ARG_0,
	ARG_PREFERENCES
};

typedef struct _pair_t { gpointer a; gpointer b; } pair_t;

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

static int place_category_cb              (Preferences *prefs, 
					   gchar *name, 
					   PrefsWidget *prefs_widget);
static int place_event_cb                 (Preferences *prefs, 
					   SoundEvent *event, 
					   pair_t *p);

static void set_events_toggle_sensitive   (PrefsWidget *prefs_widget,
					   gboolean s);
static void set_events_tab_sensitive      (PrefsWidget *prefs_widget,
					   gboolean s);

static void play_button_clicked_cb        (GtkButton *button,
					   PrefsWidget *prefs_widget);
static void enable_toggled_cb             (GtkToggleButton *tb,
					   PrefsWidget *prefs_widget);
static void events_toggled_cb             (GtkToggleButton *tb,
					   PrefsWidget *prefs_widget);
static void events_tree_select_cb         (GtkCTree *ctree,
					   GtkCTreeNode *row,
					   gint column,
					   PrefsWidget *prefs_widget);
static void sound_file_entry_changed_cb   (GtkEntry *entry, 
					   PrefsWidget *prefs_widget);

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
	GtkWidget *widget;
	GtkWidget *play_pixmap, *play_button, *label, *hbox;
	gchar *audio_icon_name;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	gtk_window_set_default_size (GTK_WINDOW (CAPPLET_WIDGET
						 (prefs_widget)->dialog),
				     500, 350);

	prefs_widget->dialog_data = 
		glade_xml_new (GNOMECC_GLADE_DIR "/sound-properties.glade",
			       "prefs_widget");

	widget = glade_xml_get_widget (prefs_widget->dialog_data, 
				       "prefs_widget");
	gtk_container_add (GTK_CONTAINER (prefs_widget), widget);

	audio_icon_name = gnome_pixmap_file ("gnome-audio2.png");
	pixbuf = gdk_pixbuf_new_from_file (audio_icon_name);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 1);
	gtk_pixmap_set (GTK_PIXMAP (WID ("audio_icon")), pixmap, mask);

	play_button = WID ("play_button");
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	play_pixmap = gnome_stock_pixmap_widget (play_button,
						 GNOME_STOCK_PIXMAP_VOLUME);
	gtk_box_pack_start (GTK_BOX (hbox), play_pixmap, FALSE, TRUE, 0);
	label = gtk_label_new (_("Play"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (play_button), hbox);

	prefs_widget->events_tree = GTK_CTREE (WID ("events_tree"));

	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "play_button_clicked_cb",
				       GTK_SIGNAL_FUNC 
				       (play_button_clicked_cb), prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "enable_toggled_cb",
				       GTK_SIGNAL_FUNC (enable_toggled_cb), 
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "events_toggled_cb",
				       GTK_SIGNAL_FUNC (events_toggled_cb),
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "events_tree_select_cb",
				       GTK_SIGNAL_FUNC (events_tree_select_cb),
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "sound_file_entry_changed_cb",
				       GTK_SIGNAL_FUNC
				       (sound_file_entry_changed_cb),
				       prefs_widget);
}

static void
prefs_widget_class_init (PrefsWidgetClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("PrefsWidget::preferences",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PREFERENCES);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->set_arg = prefs_widget_set_arg;
	object_class->get_arg = prefs_widget_get_arg;

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
			read_preferences (prefs_widget, prefs_widget->prefs);
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
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
				      (WID ("enable_toggle")),
				      prefs->enable_esd);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
				      (WID ("events_toggle")),
				      prefs->enable_sound_events);

	set_events_toggle_sensitive (prefs_widget,
				     prefs_widget->prefs->enable_esd);

	preferences_foreach_category (prefs,
				      (CategoryCallback) place_category_cb, 
				      prefs_widget);
}

static int
place_category_cb (Preferences *prefs, gchar *name, PrefsWidget *prefs_widget) 
{
	GtkCTreeNode *node;
	gchar *row[2];
	pair_t p;

	row[0] = name;
	row[1] = NULL;

	node = gtk_ctree_insert_node (prefs_widget->events_tree, NULL, NULL,
				      row, 8, NULL, NULL, NULL, NULL, FALSE,
				      TRUE);

	p.a = node; p.b = prefs_widget;
	preferences_foreach_event (prefs, name, 
				   (EventCallback) place_event_cb, &p);

	return 0;
}

static int
place_event_cb (Preferences *prefs, SoundEvent *event, pair_t *p) 
{
	GtkCTreeNode *event_node;
	PrefsWidget *prefs_widget;
	gchar *row[2];

	row[0] = event->description;
	row[1] = event->file;

	prefs_widget = PREFS_WIDGET (p->b);

	event_node = gtk_ctree_insert_node (prefs_widget->events_tree,
					    p->a, NULL, row, 8, NULL, NULL,
					    NULL, NULL, TRUE, FALSE);

	gtk_ctree_node_set_row_data (prefs_widget->events_tree,
				     event_node, event);

	return 0;
}

static void
set_events_toggle_sensitive (PrefsWidget *prefs_widget, gboolean s)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));

	gtk_widget_set_sensitive (WID ("events_toggle"), s);

	set_events_tab_sensitive (prefs_widget, s &&
				  prefs_widget->prefs->enable_sound_events);
}

static void
set_events_tab_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));

	gtk_widget_set_sensitive (WID ("events_tab"), s);
	gtk_widget_set_sensitive (WID ("events_tab_label"), s);
}

static void
play_button_clicked_cb (GtkButton *button, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));

	if (prefs_widget->current_event != NULL)
		gnome_sound_play (prefs_widget->current_event->file);
}

static void
enable_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (tb != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (tb));

	prefs_widget->prefs->enable_esd = gtk_toggle_button_get_active (tb);
	set_events_toggle_sensitive (prefs_widget,
				     prefs_widget->prefs->enable_esd);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
events_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	g_return_if_fail (tb != NULL);
	g_return_if_fail (GTK_IS_TOGGLE_BUTTON (tb));

	prefs_widget->prefs->enable_sound_events = 
		gtk_toggle_button_get_active (tb);

	set_events_tab_sensitive (prefs_widget,
				  prefs_widget->prefs->enable_esd &&
				  prefs_widget->prefs->enable_sound_events);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
events_tree_select_cb (GtkCTree *ctree, GtkCTreeNode *row,
		       gint column, PrefsWidget *prefs_widget)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->selected_node = row;
	prefs_widget->current_event = gtk_ctree_node_get_row_data (ctree, row);

	if (prefs_widget->current_event != NULL)
		gtk_entry_set_text
			(GTK_ENTRY (WID ("sound_entry")),
			 prefs_widget->current_event->file);
}

static void
sound_file_entry_changed_cb (GtkEntry *entry, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (prefs_widget->current_event != NULL) {
		if (prefs_widget->current_event->file != NULL)
			g_free (prefs_widget->current_event->file);
		prefs_widget->current_event->file = 
			g_strdup (gtk_entry_get_text (entry));
		gtk_ctree_node_set_text (prefs_widget->events_tree,
					 prefs_widget->selected_node, 1,
					 prefs_widget->current_event->file);

		preferences_changed (prefs_widget->prefs);
		capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), 
					      TRUE);
	}
}
