/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>,
 *            Richard Hestilow <tvgm@ximian.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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

#include "prefs-widget.h"
#include "preview.h"
#include "screensaver-prefs-dialog.h"
#include "selection-dialog.h" 
#include "rc-parse.h"
#include <gal/e-table/e-table.h>
#include <gal/e-table/e-table-simple.h>

#define WID(str) (glade_xml_get_widget (prefs_widget->priv->xml, str))

static GtkVBoxClass *prefs_widget_parent_class;

enum {
	STATE_CHANGED_SIGNAL,
	ACTIVATE_DEMO_SIGNAL,
	LAST_SIGNAL
};

struct _PrefsWidgetPrivate
{
	GladeXML *xml;
	ETableModel *etm;
	GtkWidget *table;
	guint random_timeout;
	GList *random_current;
	GtkWindow *parent;

	/* Copied from Preferences...for OK/Cancel support in dialog */
	gboolean  power_management;
	time_t    standby_time;
	time_t    suspend_time;
	time_t    power_down_time;
};
	
static gint prefs_widget_signals[LAST_SIGNAL] = { 0 };

static void prefs_widget_init             (PrefsWidget *prefs_widget);
static void prefs_widget_class_init       (PrefsWidgetClass *class);
static void prefs_widget_destroy          (PrefsWidget *prefs_widget);

static void pwr_state_changed_cb          (GtkWidget *widget,
					   PrefsWidget *prefs_widget);
static void state_changed_cb              (GtkWidget *widget,
					   PrefsWidget *prefs_widget);

static void option_menu_connect           (GtkOptionMenu *menu,
					   GtkSignalFunc func,
					   gpointer data);

static void mode_changed_cb               (GtkWidget *widget,
					   PrefsWidget *prefs_widget);

static void selection_changed_cb          (ETable *table,
					   PrefsWidget *prefs_widget);
static void selection_foreach_func        (int model_row, int *closure);

static gint random_timeout_cb             (PrefsWidget *prefs_widget);
static void set_random_timeout            (PrefsWidget *prefs_widget,
					   gboolean do_random);

static void popup_item_menu               (ETable *table,
					   int row, int col, GdkEvent *event,
					   PrefsWidget *prefs_widget);

static void about_cb                      (GtkWidget *widget,
					   PrefsWidget *prefs_widget);
static void settings_cb                   (GtkWidget *button,
					   PrefsWidget *widget);
static void pwr_manage_toggled_cb         (GtkWidget *button,
					   PrefsWidget *prefs_widget);
static void pwr_conf_cb                   (GtkWidget *button,
					   PrefsWidget *prefs_widget);
static void pwr_conf_button_cb            (GnomeDialog *dlg,
					   gint button,
					   PrefsWidget *prefs_widget);
static void pwr_save_prefs		  (PrefsWidget *prefs_widget);
static void pwr_restore_prefs             (PrefsWidget *prefs_widget);
static time_t pwr_get_toggled_entry       (PrefsWidget *prefs_widget,
					   const gchar *enable_str,
					   const gchar *entry_str);
static void pwr_set_toggled_entry         (PrefsWidget *prefs_widget,
					   const gchar *enable_str,
					   const gchar *entry_str,
					   time_t value);

static const gchar *table_compute_state   (SelectionMode mode);

static void add_select_cb                 (GtkWidget *widget,
					   Screensaver *saver,
					   PrefsWidget *prefs_widget);
static void screensaver_add_cb            (GtkWidget *button,
					   PrefsWidget *prefs_widget);
static void screensaver_remove_cb         (GtkWidget *button,
					   PrefsWidget *prefs_widget);

/* Model declarations */
static int model_col_count                (ETableModel *etm, void *data);
static int model_row_count                (ETableModel *etm, void *data);
static void* model_value_at               (ETableModel *etm, int col, int row,
 					   void *data);
static void model_set_value_at            (ETableModel *etm, int col, int row,
					   const void *val, void *data);
static gboolean model_is_cell_editable    (ETableModel *etm, int col, int row,
					   void *data);
static void* model_duplicate_value        (ETableModel *etm, int col,
					   const void *value, void *data);
static void model_free_value              (ETableModel *etm, int col,
					   void *value, void *data);
static void* model_initialize_value       (ETableModel *etm, int col,
					   void *data);
static gboolean model_value_is_empty      (ETableModel *etm, int col,
					   const void *value, void *data);
static char* model_value_to_string        (ETableModel *etm, int col,
					   const void *value, void *data);

guint prefs_widget_get_type (void)
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
			(GtkArgGetFunc) NULL,
		};

		prefs_widget_type =
			gtk_type_unique (gtk_vbox_get_type (),
					 &prefs_widget_info);
	}
	
	return prefs_widget_type;
}

static void
prefs_widget_destroy (PrefsWidget *prefs_widget)
{
	if (prefs_widget->priv->xml)
		gtk_object_destroy (GTK_OBJECT (prefs_widget->priv->xml));
	g_free (prefs_widget->priv);

	if (GTK_OBJECT_CLASS (prefs_widget_parent_class)->destroy)
		(*GTK_OBJECT_CLASS (prefs_widget_parent_class)->destroy) (GTK_OBJECT (prefs_widget));
}

static void
prefs_widget_init (PrefsWidget *prefs_widget)
{
	GtkWidget *widget;
	GtkWidget *table;
	gchar *skel, *spec;
	gchar *titles[] = { N_("Use"), N_("Screensaver"), NULL };
	int i;

	prefs_widget->priv = g_new0 (PrefsWidgetPrivate, 1);
	prefs_widget->priv->xml =
		glade_xml_new (GNOMECC_GLADE_DIR "/screensaver-properties.glade",
			       NULL); 
	if (!prefs_widget->priv->xml)
		return;
	
	for (i = 0; titles[i] != NULL; i++)
		titles[i] = gettext (titles[i]);
	
	skel =
"<ETableSpecification cursor-mode=\"line\" selection-mode=\"single\" draw-focus=\"true\">"
" <ETableColumn model_col=\"0\" draw_grid=\"true\" _title=\"%s\" expansion=\"0.0\" minimum_width=\"20\" resizable=\"false\" cell=\"checkbox\" compare=\"integer\"/>"
" <ETableColumn model_col=\"1\" draw_grid=\"true\" _title=\"%s\" expansion=\"1.0\" resizable=\"true\" cell=\"string\" compare=\"string\"/>"
"  %s"
"  </ETableSpecification>";

	spec = g_strdup_printf (skel, titles[0], titles[1], table_compute_state (SM_CHOOSE_FROM_LIST));
	prefs_widget->priv->etm =
		e_table_simple_new (model_col_count, model_row_count,
				    model_value_at, model_set_value_at,
				    model_is_cell_editable,
				    model_duplicate_value, model_free_value,
				    model_initialize_value,
				    model_value_is_empty,
				    model_value_to_string,
				    prefs_widget);
		
	table = e_table_new (prefs_widget->priv->etm, NULL, spec, NULL);
	prefs_widget->priv->table = table;

	gtk_widget_show (table);
	g_free (spec);

	widget = WID ("etable_scrolled");
	gtk_container_add (GTK_CONTAINER (widget), table);
	
	widget = WID ("prefs_widget");
	gtk_object_ref (GTK_OBJECT (widget));
	gtk_container_remove (GTK_CONTAINER (WID ("throwaway_window")), widget);
	gtk_widget_show_all (widget);
	gtk_box_pack_start (GTK_BOX (prefs_widget), widget, TRUE, TRUE, 0);
	gtk_object_ref (GTK_OBJECT (widget));
	
	prefs_widget->preview_window = WID ("preview_window");

	prefs_widget->priv->random_timeout = 0;

	/* Signals */
	gtk_signal_connect (GTK_OBJECT (table), "selection_change",
			    GTK_SIGNAL_FUNC (selection_changed_cb),
			    prefs_widget);
	gtk_signal_connect (GTK_OBJECT (table), "right_click",
			    GTK_SIGNAL_FUNC (popup_item_menu),
			    prefs_widget);
	
	widget = WID ("mode_option");
	option_menu_connect (GTK_OPTION_MENU (widget),
			     GTK_SIGNAL_FUNC (mode_changed_cb),
			     prefs_widget);

	widget = WID ("timeout_widget");
	gtk_signal_connect (GTK_OBJECT (widget), "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs_widget);

	widget = WID ("cycle_length_widget");
	gtk_signal_connect (GTK_OBJECT (widget), "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs_widget);

	widget = WID ("lock_widget");
	gtk_signal_connect (GTK_OBJECT (widget), "toggled",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs_widget);

	widget = WID ("pwr_manage_enable");
	gtk_signal_connect (GTK_OBJECT (widget), "toggled",
			    GTK_SIGNAL_FUNC (pwr_manage_toggled_cb),
			    prefs_widget);

	widget = WID ("pwr_conf_button");
	gtk_signal_connect (GTK_OBJECT (widget), "clicked",
			    GTK_SIGNAL_FUNC (pwr_conf_cb),
			    prefs_widget);

	widget = WID ("pwr_conf_dialog");
	gtk_signal_connect (GTK_OBJECT (widget), "clicked",
			    GTK_SIGNAL_FUNC (pwr_conf_button_cb),
			    prefs_widget);
	gnome_dialog_close_hides (GNOME_DIALOG (widget), TRUE);
	gnome_dialog_set_sensitive (GNOME_DIALOG (widget), GNOME_OK, FALSE);

	widget = WID ("pwr_standby_enable");
	gtk_signal_connect (GTK_OBJECT (widget), "toggled",
			    GTK_SIGNAL_FUNC (pwr_state_changed_cb),
			    prefs_widget);

	widget = WID ("pwr_standby_entry");
	gtk_signal_connect (GTK_OBJECT (widget), "changed",
			    GTK_SIGNAL_FUNC (pwr_state_changed_cb),
			    prefs_widget);

	widget = WID ("pwr_suspend_enable");
	gtk_signal_connect (GTK_OBJECT (widget), "toggled",
			    GTK_SIGNAL_FUNC (pwr_state_changed_cb),
			    prefs_widget);

	widget = WID ("pwr_suspend_entry");
	gtk_signal_connect (GTK_OBJECT (widget), "changed",
			    GTK_SIGNAL_FUNC (pwr_state_changed_cb),
			    prefs_widget);

	widget = WID ("pwr_shutdown_enable");
	gtk_signal_connect (GTK_OBJECT (widget), "toggled",
			    GTK_SIGNAL_FUNC (pwr_state_changed_cb),
			    prefs_widget);

	widget = WID ("pwr_shutdown_entry");
	gtk_signal_connect (GTK_OBJECT (widget), "changed",
			    GTK_SIGNAL_FUNC (pwr_state_changed_cb),
			    prefs_widget);

	widget = WID ("popup_about");
	gtk_signal_connect (GTK_OBJECT (widget), "activate",
			    GTK_SIGNAL_FUNC (about_cb),
			    prefs_widget);

	widget = WID ("popup_settings");
	gtk_signal_connect (GTK_OBJECT (widget), "activate",
			    GTK_SIGNAL_FUNC (settings_cb),
			    prefs_widget);
	
	widget = WID ("popup_add");
	gtk_signal_connect (GTK_OBJECT (widget), "activate",
			    GTK_SIGNAL_FUNC (screensaver_add_cb),
			    prefs_widget);

	widget = WID ("popup_remove");
	gtk_signal_connect (GTK_OBJECT (widget), "activate",
			    GTK_SIGNAL_FUNC (screensaver_remove_cb),
			    prefs_widget);
}

static void
prefs_widget_class_init (PrefsWidgetClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	prefs_widget_signals[STATE_CHANGED_SIGNAL] =
		gtk_signal_new ("pref-changed", GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (PrefsWidgetClass,
						   state_changed),
				gtk_signal_default_marshaller,
				GTK_TYPE_NONE, 0);

	prefs_widget_signals[ACTIVATE_DEMO_SIGNAL] =
		gtk_signal_new ("activate_demo", GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (PrefsWidgetClass,
						   activate_demo),
				gtk_signal_default_marshaller,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, prefs_widget_signals,
				      LAST_SIGNAL);

	class->state_changed = NULL;
	object_class->destroy = prefs_widget_destroy;
}

GtkWidget *
prefs_widget_new (GtkWindow *parent)
{
	PrefsWidget *prefs_widget = gtk_type_new (prefs_widget_get_type ());
	prefs_widget->priv->parent = parent;
	return GTK_WIDGET (prefs_widget);
}

void
prefs_widget_store_prefs (PrefsWidget *prefs_widget, Preferences *prefs)
{
	GtkWidget *widget;

	prefs->selection_mode = prefs_widget->selection_mode;

	widget = WID ("timeout_widget");
	prefs->timeout = gtk_spin_button_get_value_as_float
		(GTK_SPIN_BUTTON (widget));

	widget = WID ("cycle_length_widget");
	prefs->cycle = gtk_spin_button_get_value_as_float
		(GTK_SPIN_BUTTON (widget));

	widget = WID ("lock_widget");
	prefs->lock = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON (widget));

	widget = WID ("pwr_manage_enable");	
	prefs->power_management = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON (widget));

	pwr_save_prefs (prefs_widget);

	prefs->power_management = prefs_widget->priv->power_management;
	prefs->standby_time = prefs_widget->priv->standby_time;
	prefs->suspend_time = prefs_widget->priv->suspend_time;
	prefs->power_down_time = prefs_widget->priv->power_down_time;
}

void
prefs_widget_get_prefs (PrefsWidget *prefs_widget, Preferences *prefs)
{
	GtkWidget *widget;
	
	prefs_widget->selection_mode = prefs->selection_mode;

	/* Basic options */
	widget = WID ("mode_option");
	gtk_option_menu_set_history (GTK_OPTION_MENU (widget),
				     prefs_widget->selection_mode);

	widget = WID ("timeout_widget");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), prefs->timeout);

	widget = WID ("cycle_length_widget");
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), prefs->cycle);

	/* Locking controls */

	widget = WID ("lock_widget");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), prefs->lock);

	/* Power management controls */

	widget = WID ("pwr_manage_enable");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      prefs->power_management);
	/* I guess the above doesn't reliably trigger a signal, so... */
	pwr_manage_toggled_cb (widget, prefs_widget);

	/* Local copy */
	prefs_widget->priv->power_management = prefs->power_management;
	prefs_widget->priv->standby_time = prefs->standby_time;
	prefs_widget->priv->suspend_time = prefs->suspend_time;
	prefs_widget->priv->power_down_time = prefs->power_down_time;
	
	pwr_restore_prefs (prefs_widget);
#if 0
	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->standby_time_widget),
		 prefs->standby_time);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (prefs_widget->standby_monitor_toggle),
		 (gboolean) prefs->standby_time);

	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->suspend_time_widget),
		 prefs->suspend_time);
#endif

	/* Screensavers list */
	prefs_widget_set_screensavers (prefs_widget, prefs->screensavers);
	prefs_widget_set_mode (prefs_widget, prefs->selection_mode);
}

void
prefs_widget_set_mode (PrefsWidget *prefs_widget, SelectionMode mode)
{
	GList *l;
	Screensaver *saver;
	int count;

	prefs_widget->selection_mode = mode;
	e_table_set_state (E_TABLE (prefs_widget->priv->table),
			   table_compute_state (mode));

	count = e_table_selected_count (E_TABLE (prefs_widget->priv->table));

	/* Das blinkenpreviews -- if nothing else selected */	
	if (count || (mode != SM_CHOOSE_FROM_LIST && mode != SM_CHOOSE_RANDOMLY))
		set_random_timeout (prefs_widget, FALSE);
	else
		set_random_timeout (prefs_widget, TRUE);


	/* This could annoy the end-user, I dunno.
	 * This code's logic is a bit convoluted. Basically if we are
	 * in single-mode, then we need to clear the enabled flag on all but
	 * one saver (either the selected saver or the first enabled one, or 
	 * the first one by default), and we need the row index of that saver.
	 * This code does all that in a single loop. */	

	if (mode == SM_ONE_SCREENSAVER_ONLY)
	{
		int row = -1, i = 0;

		for (l = prefs_widget->screensavers; l != NULL; l = l->next)
		{
			saver = l->data;
			if (prefs_widget->selected_saver)
			{
				if (saver == prefs_widget->selected_saver)
				{
					saver->enabled = TRUE;
					row = i;
				}
				else
					saver->enabled = FALSE;
			}
			else
			{
				if (row == -1)
				{
					if (saver->enabled)
						row = i;
				}
				else
				{
					saver->enabled = FALSE;
				}
			}
			i++;
		}
			
		e_selection_model_select_single_row (
			E_SELECTION_MODEL (E_TABLE (prefs_widget->priv->table)->selection), row);
	}
}

void
prefs_widget_set_screensavers (PrefsWidget *prefs_widget, GList *screensavers)
{
	prefs_widget->screensavers = screensavers;
	e_table_model_changed (prefs_widget->priv->etm);
}

static void
set_random_timeout (PrefsWidget *prefs_widget, gboolean do_random)
{
	if (do_random && !prefs_widget->priv->random_timeout)
	{
		prefs_widget->priv->random_timeout = 
			gtk_timeout_add (5000,
					 (GtkFunction) random_timeout_cb,
					 prefs_widget);
		random_timeout_cb (prefs_widget);
	}
	else if (!do_random && prefs_widget->priv->random_timeout)
	{
		gtk_timeout_remove (prefs_widget->priv->random_timeout);
		prefs_widget->priv->random_timeout = 0;
	}
}

static const gchar *
table_compute_state (SelectionMode mode)
{
	if (mode != SM_CHOOSE_FROM_LIST)
		return "<ETableState><column source=\"1\"/><grouping></grouping></ETableState>";
	else return "<ETableState><column source=\"0\"/><column source=\"1\"/><grouping></grouping></ETableState>";
}

static int
model_col_count (ETableModel *etm, void *data)
{
	return 2;
}

static int
model_row_count (ETableModel *etm, void *data)
{
	PrefsWidget *prefs_widget = PREFS_WIDGET (data);

	return g_list_length (prefs_widget->screensavers);
}

static void *
model_value_at (ETableModel *etm, int col, int row, void *data)
{
	PrefsWidget *prefs_widget = PREFS_WIDGET (data);
	Screensaver *saver = g_list_nth_data (prefs_widget->screensavers, row);

	if (!saver)
		return NULL;
	
	if (col == 0)
		return GINT_TO_POINTER (saver->enabled);
	else
		return saver->label;
}

static void
model_set_value_at (ETableModel *etm,
		    int col, int row,
		    const void *val, void *data)
{
	PrefsWidget *prefs_widget = PREFS_WIDGET (data);
	Screensaver *saver = g_list_nth_data (prefs_widget->screensavers, row);
	
	g_assert (col == 0);
	
	if (!saver)
		return;

	saver->enabled = GPOINTER_TO_INT (val);
	state_changed_cb (GTK_WIDGET (prefs_widget), prefs_widget);
}

static gboolean
model_is_cell_editable (ETableModel *etm, int col, int row, void *data)
{
	return (col == 0);
}

static void *
model_duplicate_value (ETableModel *etm, int col, const void *value, void *data)
{
	if (col == 0)
	{
		int tmp = GPOINTER_TO_INT (value);
		return GINT_TO_POINTER (tmp);
	}
	else
		return g_strdup (value);
}

static void
model_free_value (ETableModel *etm, int col, void *value, void *data)
{
	if (col != 0)
		g_free (value);
}

static void *
model_initialize_value (ETableModel *etm, int col, void *data)
{
	if (col == 0)
		return GINT_TO_POINTER (0);
	else
		return g_strdup ("");
}

static gboolean
model_value_is_empty (ETableModel *etm, int col, const void *value, void *data)
{
	if (col == 0)
		return (!GPOINTER_TO_INT (value));
	else
		return (!(value && strcmp (value, "") != 0));
}

static char*
model_value_to_string (ETableModel *etm, int col, const void *value, void *data)
{
	if (col == 0)
		return g_strdup ("");
	else
		return g_strdup (value);
}

static void
selection_changed_cb (ETable *table, PrefsWidget *prefs_widget)
{
	Screensaver *saver;
	int row = -1;

	e_table_selected_row_foreach (table, selection_foreach_func, &row);
	
	if (row == -1
	    && (prefs_widget->selection_mode == SM_CHOOSE_RANDOMLY
		|| prefs_widget->selection_mode == SM_CHOOSE_FROM_LIST))
	{
		set_random_timeout (prefs_widget, TRUE);
		return;
	}
	else
		set_random_timeout (prefs_widget, FALSE);

	saver = g_list_nth_data (prefs_widget->screensavers, row);
	if (!saver)
		return;

	if (prefs_widget->selection_mode == SM_ONE_SCREENSAVER_ONLY)
	{
		if (prefs_widget->selected_saver)
			prefs_widget->selected_saver->enabled = FALSE;
		saver->enabled = TRUE;
	}

	prefs_widget->selected_saver = saver;	
	if (prefs_widget->selection_mode == SM_ONE_SCREENSAVER_ONLY)
		state_changed_cb (GTK_WIDGET (table), prefs_widget);
	show_preview (saver);
}

static void
selection_foreach_func (int model_row, int *closure)
{
	g_return_if_fail (closure != NULL);
	
	/* Selection mode is "single */
	*closure = model_row;
}

static gint
random_timeout_cb (PrefsWidget *prefs_widget)
{
	GList *l;
 
  	g_return_val_if_fail (prefs_widget != NULL, FALSE);
	
	l = prefs_widget->priv->random_current;
	
	/* Choose the next one in the list */
	if (prefs_widget->selection_mode == SM_CHOOSE_RANDOMLY)
	{
		if (l)
			l = l->next;
		/* Handles both "l initially NULL" and "end of list" */
		if (!l)
			l = prefs_widget->screensavers;
	}
	else
	/* Skip the non-enabled ones */
	{
		if (!l)
			l = prefs_widget->screensavers;
		else
		{
			l = l->next;
			
			if (!l)
				l = prefs_widget->screensavers;
		}
		
		while (l)
		{
			/* Are we back to where we started? */
			if (((Screensaver*) l->data)->enabled
			    || l == prefs_widget->priv->random_current)
				break;
			
			l = l->next;
			
			if (!l)
				l = prefs_widget->screensavers;
		}
	}

	/* Huh? */
	if (!l)
		return FALSE;

	prefs_widget->priv->random_current = l;

	show_preview (l->data);
}

static void
state_changed_cb (GtkWidget *widget, PrefsWidget *prefs_widget)
{
	gtk_signal_emit (GTK_OBJECT (prefs_widget),
			 prefs_widget_signals[STATE_CHANGED_SIGNAL]);
}

static void
option_menu_connect (GtkOptionMenu *menu, GtkSignalFunc func, gpointer data)
{
	GtkWidget *menushell;
	GList *l;
	guint i;
	
	g_return_if_fail (GTK_IS_OPTION_MENU (menu));
	g_return_if_fail (func != NULL);

	menushell = gtk_option_menu_get_menu (menu);
	i = 1;
	
	for (l = GTK_MENU_SHELL (menushell)->children; l != NULL; l = l->next)
	{
		gtk_object_set_data (GTK_OBJECT (l->data),
				     "index", GUINT_TO_POINTER (i));
		gtk_signal_connect (GTK_OBJECT (l->data), "activate",
				    func, data);
		i++;
	}
}

static void
mode_changed_cb (GtkWidget *widget, PrefsWidget *prefs_widget)
{
	guint data = GPOINTER_TO_UINT (
			gtk_object_get_data (GTK_OBJECT (widget), "index"));

	prefs_widget_set_mode (prefs_widget, data - 1);
	
	state_changed_cb (widget, prefs_widget);
}

static void
popup_item_menu (ETable *table,
		 int row, int col, GdkEvent *event,
		 PrefsWidget *prefs_widget)
{
	GtkWidget *menu = WID ("popup_menu");   
	gtk_widget_show (menu);
	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
			prefs_widget,
			event->button.button, event->button.time);
}

static void
about_cb (GtkWidget *widget, PrefsWidget *prefs_widget)
{
	gchar *title;
	GtkWidget *dlg, *label;
	gchar *desc, *name;

	desc = screensaver_get_desc (prefs_widget->selected_saver);
	label = gtk_label_new (desc);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	
	name = screensaver_get_label (prefs_widget->selected_saver->name);
	title = g_strdup_printf ("About %s\n", name);
	g_free (name);
	
	dlg = gnome_dialog_new (title, GNOME_STOCK_BUTTON_CLOSE, NULL);
	g_free (title);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox), label,
			    FALSE, FALSE, 0);

	gtk_signal_connect (GTK_OBJECT (dlg), "clicked",
			    gtk_object_destroy, NULL);
	
	gnome_dialog_set_default (GNOME_DIALOG (dlg), 0);
	gnome_dialog_set_parent (GNOME_DIALOG (dlg),
				 prefs_widget->priv->parent);

	gtk_widget_show_all (dlg);
}

static void
settings_cb (GtkWidget *button, PrefsWidget *widget) 
{
	GtkWidget *dialog;

	if (!widget->selected_saver) return;

	dialog = screensaver_prefs_dialog_new (widget->selected_saver);
#if 0
	gtk_signal_connect (GTK_OBJECT (dialog), "ok-clicked",
			    GTK_SIGNAL_FUNC (screensaver_prefs_ok_cb), 
			    widget);
	gtk_signal_connect (GTK_OBJECT (dialog), "demo",
			    GTK_SIGNAL_FUNC (prefs_demo_cb), 
			    widget);
#endif
	gtk_widget_show_all (dialog);
}

static void
pwr_manage_toggled_cb (GtkWidget *button, PrefsWidget *prefs_widget)
{
	GtkWidget *conf_button = WID ("pwr_conf_button");
	gtk_widget_set_sensitive (conf_button,
			          gtk_toggle_button_get_active
				  	(GTK_TOGGLE_BUTTON (button)));
	state_changed_cb (button, prefs_widget);
}

static void
pwr_conf_cb (GtkWidget *button, PrefsWidget *prefs_widget)
{
	GtkWidget *dlg = WID ("pwr_conf_dialog");

	gtk_widget_show (dlg);
}

static void
pwr_conf_button_cb (GnomeDialog *dlg, gint button, PrefsWidget *prefs_widget)
{
	if (button == GNOME_OK)
		pwr_save_prefs (prefs_widget);
	else
		pwr_restore_prefs (prefs_widget);

	gnome_dialog_close (dlg);
}

static time_t
pwr_get_toggled_entry (PrefsWidget *prefs_widget,
		       const gchar *enable_str, const gchar *entry_str)
{
	GtkWidget *widget;
       
	g_return_val_if_fail (enable_str != NULL, 0);
	g_return_val_if_fail (entry_str != NULL, 0);
	
	widget = WID (enable_str);
	if (!widget)
		return 0;
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
	{
		widget = WID (entry_str);
		if (!widget)
			return 0;
		return gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
	}
	else
	{
		return prefs_widget->priv->standby_time = 0;
	}
}

static void
pwr_save_prefs (PrefsWidget *prefs_widget)
{
	GtkWidget *widget;

	widget = WID ("pwr_manage_enable");
	prefs_widget->priv->power_management =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	
	prefs_widget->priv->standby_time =
		pwr_get_toggled_entry (prefs_widget,
				       "pwr_standby_enable",
				       "pwr_standby_entry");
	prefs_widget->priv->suspend_time =
		pwr_get_toggled_entry (prefs_widget,
				       "pwr_suspend_enable",
				       "pwr_suspend_entry");
	prefs_widget->priv->power_down_time =
		pwr_get_toggled_entry (prefs_widget,
				       "pwr_shutdown_enable",
				       "pwr_shutdown_entry");
}

static void
pwr_set_toggled_entry (PrefsWidget *prefs_widget,
		       const gchar *enable_str, const gchar *entry_str,
		       time_t value)
{
	GtkWidget *enable, *entry;

	g_return_if_fail (enable_str != NULL);
	g_return_if_fail (entry_str != NULL);

	enable = WID (enable_str);
	entry = WID (entry_str);
	
	if (!(enable && entry))
		return;

	if (value)
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable),
					      TRUE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (entry), value);
	}
	else
	{
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enable),
					      FALSE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (entry), 10);
	}
}

static void
pwr_restore_prefs (PrefsWidget *prefs_widget)
{
	GtkWidget *widget;
	
	widget = WID ("pwr_manage_enable");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				      prefs_widget->priv->power_management);
	
	pwr_set_toggled_entry (prefs_widget,
			       "pwr_standby_enable",
			       "pwr_standby_entry",
			       prefs_widget->priv->standby_time);
	pwr_set_toggled_entry (prefs_widget,
			       "pwr_suspend_enable",
			       "pwr_suspend_entry",
			       prefs_widget->priv->suspend_time);
	pwr_set_toggled_entry (prefs_widget,
			       "pwr_shutdown_enable",
			       "pwr_shutdown_entry",
			       prefs_widget->priv->power_down_time);
}

static void
pwr_state_changed_cb (GtkWidget *widget, PrefsWidget *prefs_widget)
{
	GtkWidget *dlg = WID ("pwr_conf_dialog");
	
	gnome_dialog_set_sensitive (GNOME_DIALOG (dlg), GNOME_OK, TRUE);
	state_changed_cb (widget, prefs_widget);
}
	
static void 
screensaver_add_cb (GtkWidget *button, PrefsWidget *prefs_widget) 
{
	GtkWidget *dialog;

	dialog = selection_dialog_new (prefs_widget);

	gtk_signal_connect (GTK_OBJECT (dialog), "ok-clicked",
			    GTK_SIGNAL_FUNC (add_select_cb), prefs_widget);
}

static void 
screensaver_remove_cb (GtkWidget *button, PrefsWidget *prefs_widget)
{
	Screensaver *rm;
	GList *l;
	gint row;

	if (!prefs_widget->selected_saver) return;

	rm = prefs_widget->selected_saver;

	/* Find another screensaver to select */
	row = 0;
	for (l = prefs_widget->screensavers; l != NULL; l = l->next)
	{
		if (l->data == rm)
			break;
		row++;
	}
	
	prefs_widget->screensavers =
		screensaver_remove (rm, prefs_widget->screensavers);
	screensaver_destroy (rm);

	if (!prefs_widget->screensavers)
		prefs_widget->selected_saver = NULL;
	else
	{
		if (row < 0)
			row = 0;
		else
			row--;
		
		e_selection_model_select_single_row (
			E_SELECTION_MODEL (E_TABLE (prefs_widget->priv->table)->selection), row);
	}

	state_changed_cb (button, prefs_widget);
}

static void
add_select_cb (GtkWidget *widget, Screensaver *saver, 
	       PrefsWidget *prefs_widget) 
{
	gint row;

	prefs_widget->screensavers = 
		screensaver_add (saver, prefs_widget->screensavers);

	row = g_list_length (prefs_widget->screensavers) - 1;
	e_selection_model_select_single_row (
		E_SELECTION_MODEL (E_TABLE (prefs_widget->priv->table)->selection), row);

	settings_cb (widget, prefs_widget);
}
