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
#   include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <capplet-widget.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "prefs-widget.h"
#include "preview.h"
#include "screensaver-prefs-dialog.h"
#include "selection-dialog.h"
#include "rc-parse.h"

#include "checked.xpm"
#include "unchecked.xpm"
#include "checked-disabled.xpm"
#include "unchecked-disabled.xpm"

enum {
	STATE_CHANGED_SIGNAL,
	ACTIVATE_DEMO_SIGNAL,
	LAST_SIGNAL
};

static gint prefs_widget_signals[LAST_SIGNAL] = { 0 };

static void prefs_widget_init             (PrefsWidget *prefs);
static void prefs_widget_class_init       (PrefsWidgetClass *class);

static void set_lock_controls_sensitive   (PrefsWidget *prefs, gboolean s);
static void set_fade_scales_sensitive     (PrefsWidget *prefs, gboolean s);
static void set_fade_controls_sensitive   (PrefsWidget *prefs, gboolean s);
static void set_standby_time_sensitive    (PrefsWidget *prefs, gboolean s);
static void set_suspend_time_sensitive    (PrefsWidget *prefs, gboolean s);
static void set_power_down_time_sensitive (PrefsWidget *prefs, gboolean s);
static void set_power_controls_sensitive  (PrefsWidget *prefs, gboolean s);

static void set_all_pixmaps               (PrefsWidget *prefs,
					   SelectionMode mode);

static void disable_screensaver_cb        (GtkToggleButton *button,
					   PrefsWidget *widget);
static void blank_screen_selected_cb      (GtkToggleButton *button,
					   PrefsWidget *widget);
static void one_screensaver_cb            (GtkToggleButton *button,
					   PrefsWidget *widget);
static void choose_from_selected_cb       (GtkToggleButton *button,
					   PrefsWidget *widget);
static void random_cb                     (GtkToggleButton *button,
					   PrefsWidget *widget);

static void select_saver_cb               (GtkCList *list, 
					   gint row, gint column, 
					   GdkEventButton *event, 
					   PrefsWidget *widget);
static void deselect_saver_cb             (GtkCList *list, 
					   gint row, gint column, 
					   GdkEventButton *event, 
					   PrefsWidget *widget);
static void settings_cb                   (GtkWidget *button,
					   PrefsWidget *widget);
static void screensaver_add_cb            (GtkWidget *button,
					   PrefsWidget *widget);
static void screensaver_remove_cb         (GtkWidget *button,
					   PrefsWidget *widget);
static void demo_cb                       (GtkWidget *button,
					   PrefsWidget *widget);
static void demo_next_cb                  (GtkWidget *button,
					   PrefsWidget *widget);
static void demo_prev_cb                  (GtkWidget *button,
					   PrefsWidget *widget);

static void require_password_changed_cb   (GtkCheckButton *button,
					   PrefsWidget *widget);
static void lock_timeout_changed_cb       (GtkCheckButton *button,
					   PrefsWidget *widget);
static void power_management_toggled_cb   (GtkCheckButton *button,
					   PrefsWidget *widget);
static void standby_monitor_toggled_cb    (GtkCheckButton *button,
					   PrefsWidget *widget);
static void suspend_monitor_toggled_cb    (GtkCheckButton *button,
					   PrefsWidget *widget);
static void shut_down_monitor_toggled_cb  (GtkCheckButton *button,
					   PrefsWidget *widget);

static void install_cmap_changed_cb       (GtkCheckButton *button,
					   PrefsWidget *widget);
static void fade_unfade_changed_cb        (GtkCheckButton *button,
					   PrefsWidget *widget);

static void state_changed_cb              (GtkWidget *widget,
					   PrefsWidget *prefs);

static void screensaver_prefs_ok_cb       (ScreensaverPrefsDialog *dialog, 
					   PrefsWidget *widget);

static void prefs_demo_cb                 (GtkWidget *widget,
					   PrefsWidget *prefs_widget);

static void add_select_cb                 (GtkWidget *widget,
					   Screensaver *saver,
					   PrefsWidget *prefs_widget);

static gint create_list_item              (Screensaver *saver,
					   SelectionMode mode,
					   PrefsWidget *prefs_widget);

static void set_description_text          (PrefsWidget *widget,
					   gchar *description);

static void set_screensavers_enabled      (PrefsWidget *widget,
					   gboolean s);
static void set_pixmap                    (GtkCList *clist, 
					   Screensaver *saver, gint row,
					   SelectionMode mode);
static void toggle_saver                  (PrefsWidget *widget, gint row,
					   Screensaver *saver);
static void select_row                    (GtkCList *clist, gint row);

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
			gtk_type_unique (gtk_notebook_get_type (), 
					 &prefs_widget_info);
	}

	return prefs_widget_type;
}

static void
prefs_widget_init (PrefsWidget *prefs)
{
	GtkWidget *table, *frame, *vbox, *vbox1, *hbox, *label;
	GtkWidget *scrolled_window, *button, *table1;
	GtkObject *adjustment;
	GSList *no_screensavers_group = NULL;
	GtkWidget *viewport;

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (prefs), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);
	gtk_table_set_row_spacings (GTK_TABLE (table), 10);
	gtk_table_set_col_spacings (GTK_TABLE (table), 10);

	frame = gtk_frame_new (_("Selection"));
	gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 0, 2,
			  GTK_EXPAND | GTK_FILL,
			  GTK_EXPAND | GTK_FILL, 0, 0);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, FALSE, FALSE, 0);

	prefs->disable_screensaver_widget = 
		gtk_radio_button_new_with_label
		(no_screensavers_group, _("Disable screensaver"));
	no_screensavers_group = 
		gtk_radio_button_group 
		(GTK_RADIO_BUTTON (prefs->disable_screensaver_widget));
	gtk_box_pack_start (GTK_BOX (vbox1), 
			    prefs->disable_screensaver_widget, 
			    FALSE, FALSE, 0);

	prefs->blank_screen_widget = 
		gtk_radio_button_new_with_label
		(no_screensavers_group, _("Black screen only"));
	no_screensavers_group = 
		gtk_radio_button_group 
		(GTK_RADIO_BUTTON (prefs->blank_screen_widget));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->blank_screen_widget, 
			    FALSE, FALSE, 0);

	prefs->one_screensaver_widget = 
		gtk_radio_button_new_with_label
		(no_screensavers_group, _("One screensaver all the time"));
	no_screensavers_group = 
		gtk_radio_button_group 
		(GTK_RADIO_BUTTON (prefs->one_screensaver_widget));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->one_screensaver_widget, 
			    FALSE, FALSE, 0);

	prefs->choose_from_list_widget = 
		gtk_radio_button_new_with_label 
		(no_screensavers_group, 
		 _("Choose randomly from those checked off"));
	no_screensavers_group = 
		gtk_radio_button_group 
		(GTK_RADIO_BUTTON (prefs->choose_from_list_widget));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->choose_from_list_widget, 
			    FALSE, FALSE, 0);

	prefs->choose_randomly_widget = 
		gtk_radio_button_new_with_label
		(no_screensavers_group, 
		 _("Choose randomly among all screensavers"));
	no_screensavers_group = 
		gtk_radio_button_group 
		(GTK_RADIO_BUTTON (prefs->choose_randomly_widget));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->choose_randomly_widget, 
			    FALSE, FALSE, 0);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), 
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	prefs->screensaver_list = gtk_clist_new (2);
	gtk_clist_column_titles_hide (GTK_CLIST (prefs->screensaver_list));
	gtk_clist_set_column_width (GTK_CLIST (prefs->screensaver_list), 
				    0, 16);
	gtk_clist_set_sort_column (GTK_CLIST (prefs->screensaver_list), 1);
	gtk_clist_set_auto_sort (GTK_CLIST (prefs->screensaver_list), TRUE);
	gtk_clist_set_selection_mode (GTK_CLIST (prefs->screensaver_list),
				      GTK_SELECTION_SINGLE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), 
			   prefs->screensaver_list);

	table1 = gtk_table_new (2, 3, TRUE);
	gtk_table_set_row_spacings (GTK_TABLE (table1), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table1), GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), table1, FALSE, FALSE, 0);

	button = gtk_button_new_with_label (_("Add..."));
	gtk_table_attach (GTK_TABLE (table1), button, 0, 1, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    screensaver_add_cb, prefs);

	prefs->remove_button = gtk_button_new_with_label (_("Remove"));
	gtk_table_attach (GTK_TABLE (table1), prefs->remove_button, 1, 2, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_signal_connect (GTK_OBJECT (prefs->remove_button), "clicked",
			    screensaver_remove_cb, prefs);
	gtk_widget_set_sensitive (prefs->remove_button, FALSE);

	prefs->settings_button = gtk_button_new_with_label (_("Settings..."));
	gtk_table_attach (GTK_TABLE (table1), prefs->settings_button, 
			  2, 3, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_set_sensitive (prefs->settings_button, FALSE);

	prefs->demo_button = gtk_button_new_with_label (_("Demo"));
	gtk_table_attach (GTK_TABLE (table1), prefs->demo_button, 0, 1, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_set_sensitive (prefs->demo_button, FALSE);

	button = gtk_button_new_with_label (_("Demo Next"));
	gtk_table_attach (GTK_TABLE (table1), button, 1, 2, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    demo_next_cb, prefs);

	button = gtk_button_new_with_label (_("Demo Previous"));
	gtk_table_attach (GTK_TABLE (table1), button, 2, 3, 1, 2,
			  GTK_FILL, GTK_FILL, 0, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked",
			    demo_prev_cb, prefs);

	frame = gtk_frame_new (_("Preview"));
	gtk_table_attach (GTK_TABLE (table), frame, 1, 2, 0, 1,
			  GTK_FILL, GTK_FILL, 0, 0);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (frame), scrolled_window);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 5);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), 
					GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);

	prefs->preview_window = gtk_drawing_area_new ();
	gtk_container_add (GTK_CONTAINER (viewport), prefs->preview_window);
	gtk_widget_set_usize (prefs->preview_window, 300, 200);

	frame = gtk_frame_new (_("Description"));

	gtk_table_attach (GTK_TABLE (table), frame, 1, 2, 1, 2,
			  GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_container_add (GTK_CONTAINER (frame), scrolled_window);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 5);

	prefs->description = gtk_text_new (NULL, NULL);
	gtk_text_set_word_wrap (GTK_TEXT (prefs->description), TRUE);
	gtk_container_add (GTK_CONTAINER (scrolled_window), 
			   prefs->description);

	gtk_widget_show_all (table);
	label = gtk_label_new (_("Screensaver Selection"));
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefs), 
				    gtk_notebook_get_nth_page 
				    (GTK_NOTEBOOK (prefs), 0), label);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (prefs), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	frame = gtk_frame_new (_("Basic"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

	vbox1 = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 5);

	table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	gtk_box_pack_start (GTK_BOX (vbox1), table, FALSE, FALSE, 0);

	label = gtk_label_new (_("Start screensaver after"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	adjustment = gtk_adjustment_new (1, 0, 1000, 1, 10, 10);
	prefs->timeout_widget = 
		gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_table_attach (GTK_TABLE (table), prefs->timeout_widget,
			  1, 2, 0, 1, 0, 0, 0, 0);

	label = gtk_label_new (_("minutes"));
	gtk_table_attach (GTK_TABLE (table), label,
			  2, 3, 0, 1, 0, 0, 0, 0);

	label = gtk_label_new (_("Switch screensavers every"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	adjustment = gtk_adjustment_new (1, 0, 6000, 1, 10, 10);
	prefs->cycle_length_widget = 
		gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_table_attach (GTK_TABLE (table), prefs->cycle_length_widget,
			  1, 2, 1, 2, 0, 0, 0, 0);

	label = gtk_label_new (_("minutes"));
	gtk_table_attach (GTK_TABLE (table), label,
			  2, 3, 1, 2, 0, 0, 0, 0);

	frame = gtk_frame_new (_("Security"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

	vbox1 = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 5);

	prefs->lock_widget = 
		gtk_check_button_new_with_label 
		(_("Require password to unlock"));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->lock_widget, 
			    TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 5);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, TRUE, TRUE, 0);

	prefs->enable_timeout_widget = 
		gtk_check_button_new_with_label 
		(_("Only after the screensaver has run for"));
	gtk_box_pack_start (GTK_BOX (hbox), prefs->enable_timeout_widget, 
			    FALSE, FALSE, 0);

	adjustment = gtk_adjustment_new (1, 0, 1000, 1, 10, 10);
	prefs->time_to_lock_widget = 
		gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_box_pack_start (GTK_BOX (hbox), prefs->time_to_lock_widget, 
			    FALSE, TRUE, 0);

	prefs->lock_timeout_seconds_label = gtk_label_new (_("minutes"));
	gtk_box_pack_start (GTK_BOX (hbox), prefs->lock_timeout_seconds_label, 
			    FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->lock_timeout_seconds_label), 
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (prefs->lock_timeout_seconds_label), 
				0, 0.5);

	frame = gtk_frame_new (_("Power Management"));
	gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, TRUE, 0);

	vbox1 = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (frame), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 5);

	prefs->pwr_manage_enable = 
		gtk_check_button_new_with_label (_("Enable power management"));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->pwr_manage_enable, 
			    FALSE, FALSE, 0);

	table = gtk_table_new (3, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);
	gtk_box_pack_start (GTK_BOX (vbox1), table, TRUE, TRUE, 0);

	prefs->standby_monitor_toggle = 
		gtk_check_button_new_with_label 
		(_("Go to standby mode after"));
	gtk_table_attach (GTK_TABLE (table), prefs->standby_monitor_toggle, 
			  0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	adjustment = gtk_adjustment_new (1, 0, 10000, 1, 10, 10);
	prefs->standby_time_widget = 
		gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_table_attach (GTK_TABLE (table), prefs->standby_time_widget, 
			  1, 2, 0, 1, 0, 0, 0, 0);

	prefs->standby_monitor_label2 = 
		gtk_label_new (_("minutes"));
	gtk_table_attach (GTK_TABLE (table), prefs->standby_monitor_label2, 
			  2, 3, 0, 1, 0, 0, 0, 0);

	prefs->suspend_monitor_toggle = 
		gtk_check_button_new_with_label 
		(_("Go to suspend mode after"));
	gtk_table_attach (GTK_TABLE (table), prefs->suspend_monitor_toggle, 
			  0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	adjustment = gtk_adjustment_new (1, 0, 10000, 1, 10, 10);
	prefs->suspend_time_widget = 
		gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_table_attach (GTK_TABLE (table), prefs->suspend_time_widget, 
			  1, 2, 1, 2, 0, 0, 0, 0);

	prefs->suspend_monitor_label2 = 
		gtk_label_new (_("minutes"));
	gtk_table_attach (GTK_TABLE (table), prefs->suspend_monitor_label2, 
			  2, 3, 1, 2, 0, 0, 0, 0);

	prefs->shut_down_monitor_toggle = 
		gtk_check_button_new_with_label
		(_("Shut down monitor after"));
	gtk_table_attach (GTK_TABLE (table), prefs->shut_down_monitor_toggle, 
			  0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	adjustment = gtk_adjustment_new (1, 0, 10000, 1, 10, 10);
	prefs->shut_down_time_widget = 
		gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), 1, 0);
	gtk_table_attach (GTK_TABLE (table), prefs->shut_down_time_widget, 
			  1, 2, 2, 3, 0, 0, 0, 0);

	prefs->shut_down_monitor_label2 = 
		gtk_label_new (_("minutes"));
	gtk_table_attach (GTK_TABLE (table), prefs->shut_down_monitor_label2, 
			  2, 3, 2, 3, 0, 0, 0, 0);

	gtk_widget_show_all (vbox);

	label = gtk_label_new (_("General Properties"));
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefs), 
				    gtk_notebook_get_nth_page 
				    (GTK_NOTEBOOK (prefs), 1), label);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (prefs), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	table = gtk_table_new (3, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table), 5);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);

	label = gtk_label_new (_("Priority"));
	gtk_table_attach (GTK_TABLE (table), label, 0, 3, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	label = gtk_label_new (_("Low"));
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	label = gtk_label_new (_("High"));
	gtk_table_attach (GTK_TABLE (table), label, 2, 3, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

	adjustment = gtk_adjustment_new (0, -20, 0, 0, 0, 0);
	gtk_signal_connect (adjustment, "value-changed", state_changed_cb,
			    prefs);
	prefs->nice_widget = 
		gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
	gtk_table_attach (GTK_TABLE (table), prefs->nice_widget, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_scale_set_draw_value (GTK_SCALE (prefs->nice_widget), FALSE);

	prefs->verbose_widget = 
		gtk_check_button_new_with_label (_("Be verbose"));
	gtk_box_pack_start (GTK_BOX (vbox), prefs->verbose_widget, 
			    FALSE, FALSE, 0);

	prefs->effects_frame = gtk_frame_new (_("Effects"));
	gtk_box_pack_start (GTK_BOX (vbox), prefs->effects_frame, 
			    FALSE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 5);
	gtk_container_add (GTK_CONTAINER (prefs->effects_frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, FALSE, TRUE, 0);

	prefs->install_cmap_widget = 
		gtk_check_button_new_with_label (_("Install colormap"));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->install_cmap_widget, 
			    FALSE, FALSE, 0);

	prefs->fade_widget = 
		gtk_check_button_new_with_label 
		(_("Fade to black when activating screensaver"));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->fade_widget, 
			    FALSE, FALSE, 0);

	prefs->unfade_widget = 
		gtk_check_button_new_with_label 
		(_("Fade desktop back when deactivating screensaver"));
	gtk_box_pack_start (GTK_BOX (vbox1), prefs->unfade_widget, 
			    FALSE, FALSE, 0);

	table = gtk_table_new (4, 3, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), 5);
	gtk_table_set_col_spacings (GTK_TABLE (table), 5);

	prefs->fade_duration_label = gtk_label_new (_("Fade Duration"));
	gtk_table_attach (GTK_TABLE (table),
			  prefs->fade_duration_label, 0, 3, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->fade_duration_label),
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (prefs->fade_duration_label), 0, 0.5);

	adjustment = gtk_adjustment_new (100, 0, 256, 0, 0, 0);
	gtk_signal_connect (adjustment, "value-changed", state_changed_cb,
			    prefs);
	prefs->fade_ticks_widget = 
		gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_ticks_widget, 1, 2, 3, 4,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_scale_set_draw_value (GTK_SCALE (prefs->fade_ticks_widget), FALSE);

	prefs->fade_ticks_label = gtk_label_new (_("Fade Smoothness"));
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_ticks_label, 0, 3, 2, 3,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->fade_ticks_label), 
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (prefs->fade_ticks_label), 0, 0);

	adjustment = gtk_adjustment_new (3, 0, 10, 0, 0, 0);
	prefs->fade_duration_widget = 
		gtk_hscale_new (GTK_ADJUSTMENT (adjustment));
	gtk_signal_connect (adjustment, "value-changed", state_changed_cb,
			    prefs);
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_duration_widget, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	gtk_scale_set_draw_value (GTK_SCALE (prefs->fade_duration_widget), 
				  FALSE);

	prefs->fade_duration_high_label = gtk_label_new (_("Long"));
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_duration_high_label, 2, 3, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->fade_duration_high_label), 
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (prefs->fade_duration_high_label), 
				0, 0.5);

	prefs->fade_ticks_high_label = gtk_label_new (_("Smooth"));
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_ticks_high_label, 2, 3, 3, 4,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->fade_ticks_high_label), 
			       GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (prefs->fade_ticks_high_label), 
				0, 0.5);

	prefs->fade_duration_low_label = gtk_label_new (_("Short"));
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_duration_low_label, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->fade_duration_low_label), 
			       GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (prefs->fade_duration_low_label), 
				1, 0.5);

	prefs->fade_ticks_low_label = gtk_label_new (_("Jerky"));
	gtk_table_attach (GTK_TABLE (table), 
			  prefs->fade_ticks_low_label, 0, 1, 3, 4,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify (GTK_LABEL (prefs->fade_ticks_low_label), 
			       GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (prefs->fade_ticks_low_label), 
				1, 0.5);

	gtk_widget_show_all (vbox);
	label = gtk_label_new (_("Advanced Properties"));
	gtk_notebook_set_tab_label (GTK_NOTEBOOK (prefs), 
				    gtk_notebook_get_nth_page 
				    (GTK_NOTEBOOK (prefs), 2), label);

	gtk_signal_connect (GTK_OBJECT (prefs->disable_screensaver_widget), 
			    "toggled",
			    GTK_SIGNAL_FUNC (disable_screensaver_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->blank_screen_widget), 
			    "toggled",
			    GTK_SIGNAL_FUNC (blank_screen_selected_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->one_screensaver_widget), 
			    "toggled",
			    GTK_SIGNAL_FUNC (one_screensaver_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->choose_from_list_widget), 
			    "toggled",
			    GTK_SIGNAL_FUNC (choose_from_selected_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->choose_randomly_widget), 
			    "toggled",
			    GTK_SIGNAL_FUNC (random_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->screensaver_list),
			    "select-row", GTK_SIGNAL_FUNC (select_saver_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->screensaver_list),
			    "unselect-row", 
			    GTK_SIGNAL_FUNC (deselect_saver_cb), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->demo_button), "clicked",
			    GTK_SIGNAL_FUNC (demo_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->settings_button), "clicked",
			    GTK_SIGNAL_FUNC (settings_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->timeout_widget), "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->cycle_length_widget), "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->lock_widget), "toggled",
			    GTK_SIGNAL_FUNC (require_password_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->enable_timeout_widget),
			    "toggled",
			    GTK_SIGNAL_FUNC (lock_timeout_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->time_to_lock_widget), "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->pwr_manage_enable), "toggled",
			    GTK_SIGNAL_FUNC (power_management_toggled_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->standby_monitor_toggle), 
			    "toggled", 
			    GTK_SIGNAL_FUNC (standby_monitor_toggled_cb), 
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->suspend_monitor_toggle), 
			    "toggled", 
			    GTK_SIGNAL_FUNC (suspend_monitor_toggled_cb), 
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->shut_down_monitor_toggle), 
			    "toggled", 
			    GTK_SIGNAL_FUNC (shut_down_monitor_toggled_cb), 
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->standby_time_widget), 
			    "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->suspend_time_widget), 
			    "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->shut_down_time_widget), 
			    "changed",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->verbose_widget), "toggled",
			    GTK_SIGNAL_FUNC (state_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->install_cmap_widget), "toggled",
			    GTK_SIGNAL_FUNC (install_cmap_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->fade_widget), "toggled",
			    GTK_SIGNAL_FUNC (fade_unfade_changed_cb),
			    prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->unfade_widget), "toggled",
			    GTK_SIGNAL_FUNC (fade_unfade_changed_cb),
			    prefs);
}

static void
prefs_widget_class_init (PrefsWidgetClass *class) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
    
	prefs_widget_signals[STATE_CHANGED_SIGNAL] =
		gtk_signal_new ("pref-changed", GTK_RUN_FIRST, 
				object_class->type,
				GTK_SIGNAL_OFFSET (PrefsWidgetClass, 
						   state_changed),
				gtk_signal_default_marshaller, 
				GTK_TYPE_NONE, 0);
    
	prefs_widget_signals[ACTIVATE_DEMO_SIGNAL] =
		gtk_signal_new ("activate-demo", GTK_RUN_FIRST, 
				object_class->type,
				GTK_SIGNAL_OFFSET (PrefsWidgetClass, 
						   activate_demo),
				gtk_signal_default_marshaller, 
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, prefs_widget_signals,
				      LAST_SIGNAL);

	class->state_changed = NULL;
}

GtkWidget *
prefs_widget_new (void) 
{
	return gtk_type_new (prefs_widget_get_type ());
}

void
prefs_widget_store_prefs (PrefsWidget *prefs_widget, Preferences *prefs)
{
	GtkAdjustment *adjustment;
	
	prefs->timeout = gtk_spin_button_get_value_as_float 
		(GTK_SPIN_BUTTON (prefs_widget->timeout_widget));

	prefs->cycle = gtk_spin_button_get_value_as_float
		(GTK_SPIN_BUTTON (prefs_widget->cycle_length_widget));

	prefs->lock = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->lock_widget));

	prefs->lock_timeout = gtk_spin_button_get_value_as_float
		(GTK_SPIN_BUTTON (prefs_widget->time_to_lock_widget));

	prefs->lock_timeout = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->enable_timeout_widget));

	prefs->power_management = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->pwr_manage_enable));

	prefs->standby_time = gtk_spin_button_get_value_as_int
		(GTK_SPIN_BUTTON (prefs_widget->standby_time_widget));

	prefs->suspend_time = gtk_spin_button_get_value_as_int
		(GTK_SPIN_BUTTON (prefs_widget->suspend_time_widget));

	prefs->power_down_time = gtk_spin_button_get_value_as_int
		(GTK_SPIN_BUTTON (prefs_widget->shut_down_time_widget));

	prefs->lock = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->lock_widget));
	
	adjustment = gtk_range_get_adjustment 
		(GTK_RANGE (prefs_widget->nice_widget));
	prefs->nice = -adjustment->value;

	prefs->verbose = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->verbose_widget));

	prefs->install_colormap = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->install_cmap_widget));

	prefs->fade = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->fade_widget));

	prefs->unfade = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->unfade_widget));

	adjustment = gtk_range_get_adjustment 
		(GTK_RANGE (prefs_widget->fade_duration_widget));
	prefs->fade_seconds = adjustment->value;

	adjustment = gtk_range_get_adjustment 
		(GTK_RANGE (prefs_widget->fade_ticks_widget));
	prefs->fade_ticks = adjustment->value;

	prefs->screensavers = prefs_widget->screensavers;
	prefs->selection_mode = prefs_widget->selection_mode;
}

void
prefs_widget_get_prefs (PrefsWidget *prefs_widget, Preferences *prefs) 
{
	GtkWidget *widget = NULL;
	GtkAdjustment *adjustment;
	GdkVisual *visual;

	/* Selection mode */

	prefs_widget->selection_mode = prefs->selection_mode;

	switch (prefs->selection_mode) {
	case SM_DISABLE_SCREENSAVER:
		widget = prefs_widget->disable_screensaver_widget;
		break;
	case SM_BLANK_SCREEN:
		widget = prefs_widget->blank_screen_widget;
		break;
	case SM_ONE_SCREENSAVER_ONLY:
		widget = prefs_widget->one_screensaver_widget;
		break;
	case SM_CHOOSE_FROM_LIST:
		widget = prefs_widget->choose_from_list_widget;
		break;
	case SM_CHOOSE_RANDOMLY:
		widget = prefs_widget->choose_randomly_widget;
		break;
	}

	/* Basic options */

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	
	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->timeout_widget),
		 prefs->timeout);

	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->cycle_length_widget),
		 prefs->cycle);

	/* Locking controls */

	gtk_toggle_button_set_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->lock_widget), prefs->lock);

	gtk_widget_set_sensitive (prefs_widget->time_to_lock_widget, TRUE);
	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->time_to_lock_widget),
		 prefs->lock_timeout);

	gtk_widget_set_sensitive (prefs_widget->time_to_lock_widget, 
				  (gboolean) prefs->lock_timeout);
			
	gtk_toggle_button_set_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->enable_timeout_widget),
		 (gboolean) prefs->lock_timeout);

	set_lock_controls_sensitive (prefs_widget, prefs->lock);

	/* Power management controls */
	
	gtk_toggle_button_set_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->pwr_manage_enable), 
		 prefs->power_management);

	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->standby_time_widget),
		 prefs->standby_time);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (prefs_widget->standby_monitor_toggle),
		 (gboolean) prefs->standby_time);

	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->suspend_time_widget),
		 prefs->suspend_time);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (prefs_widget->suspend_monitor_toggle),
		 (gboolean) prefs->suspend_time);

	gtk_spin_button_set_value 
		(GTK_SPIN_BUTTON (prefs_widget->shut_down_time_widget),
		 prefs->power_down_time);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (prefs_widget->shut_down_monitor_toggle),
		 (gboolean) prefs->power_down_time);

	if (prefs->power_management) {
		set_power_controls_sensitive (prefs_widget, TRUE);

		set_standby_time_sensitive (prefs_widget, 
					    (gboolean) prefs->standby_time);
		set_suspend_time_sensitive (prefs_widget, 
					    (gboolean) prefs->suspend_time);
		set_power_down_time_sensitive 
			(prefs_widget, (gboolean) prefs->power_down_time);

	} else {
		set_power_controls_sensitive (prefs_widget, FALSE);
	}

	/* Advanced options */

	adjustment = gtk_range_get_adjustment 
		(GTK_RANGE (prefs_widget->nice_widget));
	gtk_adjustment_set_value (adjustment, -prefs->nice);

	gtk_toggle_button_set_active 
		(GTK_TOGGLE_BUTTON (prefs_widget->verbose_widget),
		 prefs->verbose);

	/* Colormap and fade controls */

	visual = gdk_visual_get_system ();

	if (visual->type == GDK_VISUAL_GRAYSCALE ||
	    visual->type == GDK_VISUAL_PSEUDO_COLOR) 
	{
		gtk_toggle_button_set_active 
			(GTK_TOGGLE_BUTTON (prefs_widget->install_cmap_widget),
			 prefs->install_colormap);

		if (prefs->install_colormap) {
			set_fade_controls_sensitive (prefs_widget, TRUE);

			gtk_toggle_button_set_active 
				(GTK_TOGGLE_BUTTON (prefs_widget->fade_widget),
				 prefs->fade);

			gtk_toggle_button_set_active 
				(GTK_TOGGLE_BUTTON 
				 (prefs_widget->unfade_widget),
				 prefs->unfade);

			if (prefs->fade || prefs->unfade) {
				set_fade_scales_sensitive (prefs_widget, TRUE);

				adjustment = 
					gtk_range_get_adjustment 
					(GTK_RANGE (prefs_widget->
						    fade_duration_widget));
				gtk_adjustment_set_value 
					(adjustment, prefs->fade_seconds);

				adjustment = 
					gtk_range_get_adjustment 
					(GTK_RANGE (prefs_widget->
						    fade_ticks_widget));
				gtk_adjustment_set_value 
					(adjustment,
					 prefs->fade_ticks);
			} else {
				set_fade_scales_sensitive
					(prefs_widget, FALSE);
			}
		} else {
			set_fade_controls_sensitive (prefs_widget, FALSE);
		}
	} else {
		gtk_widget_set_sensitive
			(prefs_widget->install_cmap_widget, FALSE);

		set_fade_controls_sensitive (prefs_widget, FALSE);
	}

	/* Screensavers list */

	prefs_widget_set_screensavers (prefs_widget,
				       prefs->screensavers,
				       prefs->selection_mode);
}

void 
prefs_widget_set_screensavers (PrefsWidget *prefs,
			       GList *screensavers,
			       SelectionMode mode) 
{
	GList *node;
	gint row;
	GtkCList *clist;
	Screensaver *saver;

	prefs->screensavers = screensavers;

	clist = GTK_CLIST (prefs->screensaver_list);

	gtk_clist_freeze (clist);

	gtk_clist_clear (clist);
	for (node = screensavers; node; node = node->next)
		create_list_item (SCREENSAVER (node->data), mode, prefs);

	gtk_clist_thaw (clist);

	if (mode == SM_ONE_SCREENSAVER_ONLY) {
		for (row = 0; row < clist->rows; row++) {
			saver = gtk_clist_get_row_data (clist, row);
			if (saver->enabled) break;
		}

		select_row (clist, row);
	}
}

static void
set_fade_scales_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (prefs_widget->fade_duration_label, s);
	gtk_widget_set_sensitive (prefs_widget->fade_duration_low_label, s);
	gtk_widget_set_sensitive (prefs_widget->fade_duration_high_label, s);
	gtk_widget_set_sensitive (prefs_widget->fade_duration_widget, s);
	gtk_widget_set_sensitive (prefs_widget->fade_ticks_label, s);
	gtk_widget_set_sensitive (prefs_widget->fade_ticks_low_label, s);
	gtk_widget_set_sensitive (prefs_widget->fade_ticks_high_label, s);
	gtk_widget_set_sensitive (prefs_widget->fade_ticks_widget, s);
}

static void
set_fade_controls_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (prefs_widget->fade_widget, s);
	gtk_widget_set_sensitive (prefs_widget->unfade_widget, s);

	set_fade_scales_sensitive (prefs_widget, s);
}

static void
set_lock_controls_sensitive (PrefsWidget *prefs, gboolean s) 
{
	gtk_widget_set_sensitive (prefs->enable_timeout_widget, s);

	if (gtk_toggle_button_get_active 
	    (GTK_TOGGLE_BUTTON (prefs->enable_timeout_widget)))
		gtk_widget_set_sensitive (prefs->time_to_lock_widget, s);

	gtk_widget_set_sensitive (prefs->lock_timeout_seconds_label, s);
}

static void
set_standby_time_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (prefs_widget->standby_time_widget, s);
}

static void
set_suspend_time_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (prefs_widget->suspend_time_widget, s);
}

static void
set_power_down_time_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (prefs_widget->shut_down_time_widget, s);
}

static void
set_power_controls_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gboolean value;

	value = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON (prefs_widget->standby_monitor_toggle));
	set_standby_time_sensitive (prefs_widget, s & value);
	value = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON (prefs_widget->suspend_monitor_toggle));
	set_suspend_time_sensitive (prefs_widget, s & value);
	value = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON (prefs_widget->shut_down_monitor_toggle));
	set_power_down_time_sensitive (prefs_widget, s & value);

	gtk_widget_set_sensitive (prefs_widget->standby_monitor_toggle, s);
	gtk_widget_set_sensitive (prefs_widget->standby_monitor_label2, s);
	gtk_widget_set_sensitive (prefs_widget->suspend_monitor_toggle, s);
	gtk_widget_set_sensitive (prefs_widget->suspend_monitor_label2, s);
	gtk_widget_set_sensitive (prefs_widget->shut_down_monitor_toggle, s);
	gtk_widget_set_sensitive (prefs_widget->shut_down_monitor_label2, s);
}

static void
set_all_pixmaps (PrefsWidget *prefs, SelectionMode mode) 
{
	GtkCList *list;
	Screensaver *saver;
	gint i;

	list = GTK_CLIST (prefs->screensaver_list);

	for (i = 0; i < list->rows; i++) {
		saver = gtk_clist_get_row_data (list, i);
		set_pixmap (list, saver, i, mode);
	}
}

static void
demo_cb (GtkWidget *button, PrefsWidget *widget) 
{
	if (!widget->selected_saver) return;

	gtk_signal_emit (GTK_OBJECT (widget),
			 prefs_widget_signals[ACTIVATE_DEMO_SIGNAL]);
	show_demo (widget->selected_saver);
}

static void
demo_next_cb (GtkWidget *button, PrefsWidget *widget) 
{
	gint row;

	if (GTK_CLIST (widget->screensaver_list)->rows == 0) return;

	if (widget->selected_saver) {
		row = gtk_clist_find_row_from_data
			(GTK_CLIST (widget->screensaver_list),
			 widget->selected_saver) + 1;
		if (row >= GTK_CLIST (widget->screensaver_list)->rows)
			row = 0;
	} else {
		row = 0;
	}

	select_row (GTK_CLIST (widget->screensaver_list), row);

	gtk_signal_emit (GTK_OBJECT (widget),
			 prefs_widget_signals[ACTIVATE_DEMO_SIGNAL]);
	show_demo (widget->selected_saver);
}

static void
demo_prev_cb (GtkWidget *button, PrefsWidget *widget) 
{
	gint row;

	if (GTK_CLIST (widget->screensaver_list)->rows == 0) return;

	if (widget->selected_saver) {
		row = gtk_clist_find_row_from_data
			(GTK_CLIST (widget->screensaver_list),
			 widget->selected_saver) - 1;
		if (row < 0)
			row = GTK_CLIST (widget->screensaver_list)->rows - 1;
	} else {
		row = GTK_CLIST (widget->screensaver_list)->rows - 1;
	}

	select_row (GTK_CLIST (widget->screensaver_list), row);

	gtk_signal_emit (GTK_OBJECT (widget),
			 prefs_widget_signals[ACTIVATE_DEMO_SIGNAL]);
	show_demo (widget->selected_saver);
}

static void 
screensaver_add_cb (GtkWidget *button, PrefsWidget *widget) 
{
	GtkWidget *dialog;

	dialog = selection_dialog_new (widget);

	gtk_signal_connect (GTK_OBJECT (dialog), "ok-clicked",
			    GTK_SIGNAL_FUNC (add_select_cb), widget);
}

static void 
screensaver_remove_cb (GtkWidget *button, PrefsWidget *widget)
{
	GtkCList *clist;
	Screensaver *rm;
	gint row;

	if (!widget->selected_saver) return;

	rm = widget->selected_saver;
	clist = GTK_CLIST (widget->screensaver_list);

	row = gtk_clist_find_row_from_data (clist, widget->selected_saver);
	gtk_clist_remove (GTK_CLIST (widget->screensaver_list), row);

	/* Find another screensaver to select */
	if (clist->rows == 0) {
		widget->selected_saver = NULL;
		gtk_widget_set_sensitive (widget->demo_button, FALSE);
		gtk_widget_set_sensitive (widget->remove_button, FALSE);
		gtk_widget_set_sensitive (widget->settings_button, FALSE);
	} else {
		if (row >= clist->rows)
			row = clist->rows - 1;
		widget->selected_saver =
			gtk_clist_get_row_data (clist, row);
		select_row (clist, row);
	}

	widget->screensavers = screensaver_remove (rm, widget->screensavers);
	screensaver_destroy (rm);

	state_changed_cb (button, widget);
}

static void
settings_cb (GtkWidget *button, PrefsWidget *widget) 
{
	GtkWidget *dialog;

	if (!widget->selected_saver) return;

	dialog = screensaver_prefs_dialog_new (widget->selected_saver);
	gtk_signal_connect (GTK_OBJECT (dialog), "ok-clicked",
			    GTK_SIGNAL_FUNC (screensaver_prefs_ok_cb), 
			    widget);
	gtk_signal_connect (GTK_OBJECT (dialog), "demo",
			    GTK_SIGNAL_FUNC (prefs_demo_cb), 
			    widget);
	gtk_widget_show_all (dialog);
}

static void 
disable_screensaver_cb (GtkToggleButton *button, PrefsWidget *widget) 
{
	if (gtk_toggle_button_get_active (button)) {
		widget->selection_mode = SM_DISABLE_SCREENSAVER;
		set_screensavers_enabled (widget, FALSE);
	}

	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void 
blank_screen_selected_cb (GtkToggleButton *button, PrefsWidget *widget) 
{
	if (gtk_toggle_button_get_active (button)) {
		widget->selection_mode = SM_BLANK_SCREEN;
		set_screensavers_enabled (widget, FALSE);
	}

	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void
one_screensaver_cb (GtkToggleButton *button, PrefsWidget *widget) 
{
	if (gtk_toggle_button_get_active (button)) {
		widget->selection_mode = SM_ONE_SCREENSAVER_ONLY;
		set_screensavers_enabled (widget, FALSE);

		if (!widget->selected_saver && widget->screensavers) {
			widget->selected_saver = 
				SCREENSAVER
				(widget->screensavers->data);
			select_row (GTK_CLIST (widget->screensaver_list), 0);
		} else if (widget->screensavers) {
			widget->selected_saver->enabled = TRUE;
		}
	}

	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void
choose_from_selected_cb (GtkToggleButton *button, PrefsWidget *widget)
{
	if (gtk_toggle_button_get_active (button)) {
		widget->selection_mode = SM_CHOOSE_FROM_LIST;
		set_all_pixmaps (widget, SM_CHOOSE_FROM_LIST);
	}

	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void
random_cb (GtkToggleButton *button, PrefsWidget *widget) 
{
	if (gtk_toggle_button_get_active (button)) {
		widget->selection_mode = SM_CHOOSE_RANDOMLY;
		set_screensavers_enabled (widget, TRUE);
	}

	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void
select_saver_cb (GtkCList *list, gint row, gint column, 
		 GdkEventButton *event, PrefsWidget *widget) 
{
	Screensaver *saver;

	saver = gtk_clist_get_row_data (list, row);

	if (widget->selection_mode == SM_ONE_SCREENSAVER_ONLY) {
		if (widget->selected_saver)
			widget->selected_saver->enabled = FALSE;
		saver->enabled = TRUE;
		state_changed_cb (GTK_WIDGET (widget), widget);
	}

	widget->selected_saver = saver;

	set_description_text (widget, screensaver_get_desc (saver));

	show_preview (saver);

	gtk_widget_set_sensitive (widget->demo_button, TRUE);
	gtk_widget_set_sensitive (widget->remove_button, TRUE);
	gtk_widget_set_sensitive (widget->settings_button, TRUE);

	if (column == 0 && widget->selection_mode == SM_CHOOSE_FROM_LIST)
		toggle_saver (widget, row, saver);
}

static void
deselect_saver_cb (GtkCList *list, gint row, gint column, 
		   GdkEventButton *event, PrefsWidget *widget) 
{
	Screensaver *saver;
	int r, c;

	if (event && column == 0 && 
	    widget->selection_mode == SM_CHOOSE_FROM_LIST) 
	{
		gtk_clist_get_selection_info (list, event->x, event->y, 
					      &r, &c);

		if (r == row) {
			saver = gtk_clist_get_row_data (list, row);
			toggle_saver (widget, row, saver);
		}
	}
}

static void
require_password_changed_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean state;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	set_lock_controls_sensitive (widget, state);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
lock_timeout_changed_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean lock_timeout_enabled;

	lock_timeout_enabled = 
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_widget_set_sensitive (widget->time_to_lock_widget, 
				  lock_timeout_enabled);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
install_cmap_changed_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean state;

	state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	set_fade_controls_sensitive (widget, state);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
fade_unfade_changed_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean fade, unfade;

	fade = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (widget->fade_widget));
	unfade = gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (widget->unfade_widget));
	
	set_fade_scales_sensitive (widget, fade || unfade);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
power_management_toggled_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean s;

	s = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	set_power_controls_sensitive (widget, s);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
standby_monitor_toggled_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean s;

	s = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	set_standby_time_sensitive (widget, s);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
suspend_monitor_toggled_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean s;

	s = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	set_suspend_time_sensitive (widget, s);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void
shut_down_monitor_toggled_cb (GtkCheckButton *button, PrefsWidget *widget) 
{
	gboolean s;

	s = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	set_power_down_time_sensitive (widget, s);

	state_changed_cb (GTK_WIDGET (button), widget);
}

static void 
state_changed_cb (GtkWidget *widget, PrefsWidget *prefs) 
{
	gtk_signal_emit (GTK_OBJECT (prefs), 
			 prefs_widget_signals[STATE_CHANGED_SIGNAL]);
}

static void 
prefs_demo_cb (GtkWidget *widget, PrefsWidget *prefs_widget) 
{
	gtk_signal_emit (GTK_OBJECT (prefs_widget),
			 prefs_widget_signals[ACTIVATE_DEMO_SIGNAL]);
}

static void
add_select_cb (GtkWidget *widget, Screensaver *saver, 
	       PrefsWidget *prefs_widget) 
{
	gint row;

	prefs_widget->screensavers = 
		screensaver_add (saver, prefs_widget->screensavers);

	row = create_list_item (saver, prefs_widget->selection_mode,
				prefs_widget);
	select_row (GTK_CLIST (prefs_widget->screensaver_list), row);

	prefs_widget->selected_saver = saver;

	settings_cb (widget, prefs_widget);
}

static void
screensaver_prefs_ok_cb (ScreensaverPrefsDialog *dialog, 
			 PrefsWidget *widget) 
{
	gint row;

	if (dialog->saver == widget->selected_saver) {
		show_preview (dialog->saver);
	}

	row = gtk_clist_find_row_from_data 
		(GTK_CLIST (widget->screensaver_list), dialog->saver);
	gtk_clist_set_text (GTK_CLIST (widget->screensaver_list), 
			    row, 1, dialog->saver->label);
	gtk_clist_sort (GTK_CLIST (widget->screensaver_list));

	row = gtk_clist_find_row_from_data 
		(GTK_CLIST (widget->screensaver_list), dialog->saver);
	gtk_clist_moveto (GTK_CLIST (widget->screensaver_list), 
			  row, 0, 0.5, 0);
	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void 
set_description_text (PrefsWidget *widget, gchar *description) 
{
	guint length;

	length = strlen (description);

	gtk_text_freeze (GTK_TEXT (widget->description));
	gtk_text_set_point (GTK_TEXT (widget->description), 0);
	length = gtk_text_get_length (GTK_TEXT (widget->description));
	gtk_text_forward_delete (GTK_TEXT (widget->description), length);
	gtk_text_insert (GTK_TEXT (widget->description),
			 NULL, NULL, NULL, description, strlen (description));
	gtk_text_set_point (GTK_TEXT (widget->description), 0);
	gtk_text_thaw (GTK_TEXT (widget->description));
}

static gint
create_list_item (Screensaver *saver, SelectionMode mode, 
		  PrefsWidget *prefs_widget) 
{
	char *text[2];
	gint row;

	text[0] = NULL; text[1] = saver->label;
	row = gtk_clist_prepend (GTK_CLIST (prefs_widget->screensaver_list), 
				 text);
	gtk_clist_set_row_data (GTK_CLIST (prefs_widget->screensaver_list),
				row, saver);
	set_pixmap (GTK_CLIST (prefs_widget->screensaver_list), saver,
		    row, mode);

	return row;
}

static void 
set_screensavers_enabled (PrefsWidget *widget, gboolean s) 
{
	GList *node;

	for (node = widget->screensavers; node; node = node->next)
		SCREENSAVER (node->data)->enabled = s;

	set_all_pixmaps (widget, widget->selection_mode);
}

static void
set_pixmap (GtkCList *clist, Screensaver *saver, gint row, SelectionMode mode) 
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	static GdkPixmap *checked_pixmap = NULL, *unchecked_pixmap = NULL;
	static GdkPixmap *checked_disabled_pixmap = NULL;
	static GdkPixmap *unchecked_disabled_pixmap = NULL;
	static GdkBitmap *checked_bitmap = NULL, *unchecked_bitmap = NULL;
	static GdkBitmap *checked_disabled_bitmap = NULL;
	static GdkBitmap *unchecked_disabled_bitmap = NULL;

	if (mode == SM_CHOOSE_FROM_LIST && saver->enabled) {
		if (!checked_pixmap) {
			pixbuf = gdk_pixbuf_new_from_xpm_data 
				((const char **) checked_xpm);
			gdk_pixbuf_render_pixmap_and_mask
				(pixbuf, &checked_pixmap,
				 &checked_bitmap, 1);
			gdk_pixbuf_unref (pixbuf);
			gdk_pixmap_ref (checked_pixmap);
			gdk_bitmap_ref (checked_bitmap);
		}
 		pixmap = checked_pixmap;
 		bitmap = checked_bitmap;
	}
	else if (mode == SM_CHOOSE_FROM_LIST && !saver->enabled) {
		if (!unchecked_pixmap) {
			pixbuf = gdk_pixbuf_new_from_xpm_data
				((const char **) unchecked_xpm);
			gdk_pixbuf_render_pixmap_and_mask
				(pixbuf, &unchecked_pixmap,
				 &unchecked_bitmap, 1);
			gdk_pixbuf_unref (pixbuf);
			gdk_pixmap_ref (unchecked_pixmap);
			gdk_bitmap_ref (unchecked_bitmap);
		}
 		pixmap = unchecked_pixmap;
 		bitmap = unchecked_bitmap;
	}
	else if (mode == SM_CHOOSE_RANDOMLY) {
		if (!checked_disabled_pixmap) {
			pixbuf = gdk_pixbuf_new_from_xpm_data
				((const char **) checked_disabled_xpm);
			gdk_pixbuf_render_pixmap_and_mask
				(pixbuf, &checked_disabled_pixmap,
				 &checked_disabled_bitmap, 1);
			gdk_pixbuf_unref (pixbuf);
			gdk_pixmap_ref (checked_disabled_pixmap);
			gdk_bitmap_ref (checked_disabled_bitmap);
		}
 		pixmap = checked_disabled_pixmap;
 		bitmap = checked_disabled_bitmap;
	} else {
		if (!unchecked_disabled_pixmap) {
			pixbuf = gdk_pixbuf_new_from_xpm_data
				((const char **) unchecked_disabled_xpm);
			gdk_pixbuf_render_pixmap_and_mask
				(pixbuf, &unchecked_disabled_pixmap,
				 &unchecked_disabled_bitmap, 1);
			gdk_pixbuf_unref (pixbuf);
			gdk_pixmap_ref (unchecked_disabled_pixmap);
			gdk_bitmap_ref (unchecked_disabled_bitmap);
		}
 		pixmap = unchecked_disabled_pixmap;
 		bitmap = unchecked_disabled_bitmap;
	}

	gtk_clist_set_pixmap (clist, row, 0, pixmap, bitmap);
}

static void
toggle_saver (PrefsWidget *widget, gint row, Screensaver *saver)
{
	saver->enabled = !saver->enabled;

	set_pixmap (GTK_CLIST (widget->screensaver_list),
		    saver, row, SM_CHOOSE_FROM_LIST);

	state_changed_cb (GTK_WIDGET (widget), widget);
}

static void
select_row (GtkCList *clist, gint row) 
{
	gtk_clist_select_row (clist, row, 1);
	if (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto (clist, row, 0, 0.5, 0);
}
