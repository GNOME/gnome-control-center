/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2003-2004 Imendio HB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>
 * Copyright (C) 2002 CodeFactory AB
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-client.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include "drwright.h"
#include "drw-break-window.h"
#include "drw-monitor.h"
#include "drw-utils.h"
#include "eggtrayicon.h"
#include "egg-spawn.h"

#define BLINK_TIMEOUT        200
#define BLINK_TIMEOUT_MIN    120
#define BLINK_TIMEOUT_FACTOR 100

#define POPUP_ITEM_ENABLED 1
#define POPUP_ITEM_BREAK   2

typedef enum {
	STATE_START,
	STATE_IDLE,
	STATE_TYPE,
	STATE_WARN_TYPE,
	STATE_WARN_IDLE,
	STATE_BREAK_SETUP,
	STATE_BREAK,
	STATE_BREAK_DONE_SETUP,
	STATE_BREAK_DONE
} DrwState;

struct _DrWright {
	/* Widgets. */
	GtkWidget      *break_window;
	GList          *secondary_break_windows;
	
	DrwMonitor     *monitor;

	GtkItemFactory *popup_factory;
	
	DrwState        state;
	GTimer         *timer;
	GTimer         *idle_timer;

	gint            last_elapsed_time;
	
	gboolean        is_active;
	
	/* Time settings. */
	gint            type_time;
	gint            break_time;
	gint            warn_time;

	gboolean        enabled;

	guint           clock_timeout_id;
	guint           blink_timeout_id;

	gboolean        blink_on;

	EggTrayIcon    *icon;
	GtkWidget      *icon_image;
	GtkWidget      *icon_event_box;
	GtkTooltips    *tooltips;

	GdkPixbuf      *neutral_bar;
	GdkPixbuf      *red_bar;
	GdkPixbuf      *green_bar;
	GdkPixbuf      *disabled_bar;
	GdkPixbuf      *composite_bar;

	GtkWidget      *warn_dialog;
};

static void     activity_detected_cb           (DrwMonitor     *monitor,
						DrWright       *drwright);
static gboolean maybe_change_state             (DrWright       *drwright);
static gboolean update_tooltip                 (DrWright       *drwright);
static gboolean icon_button_press_cb           (GtkWidget      *widget,
						GdkEventButton *event,
						DrWright       *drwright);
static void     break_window_done_cb           (GtkWidget      *window,
						DrWright       *dr);
static void     break_window_postpone_cb       (GtkWidget      *window,
						DrWright       *dr);
static void     break_window_destroy_cb        (GtkWidget      *window,
						DrWright       *dr);
static void     popup_break_cb                 (gpointer        callback_data,
						guint           action,
						GtkWidget      *widget);
static void     popup_preferences_cb           (gpointer        callback_data,
						guint           action,
						GtkWidget      *widget);
static void     popup_about_cb                 (gpointer        callback_data,
						guint           action,
						GtkWidget      *widget);
static gchar *  item_factory_trans_cb          (const gchar    *path,
						gpointer        data);
static void     init_tray_icon                 (DrWright       *dr);
static GList *  create_secondary_break_windows (void);



#define GIF_CB(x) ((GtkItemFactoryCallback)(x))

static GtkItemFactoryEntry popup_items[] = {
/*	{ N_("/_Enabled"),      NULL, GIF_CB (popup_enabled_cb),     POPUP_ITEM_ENABLED, "<ToggleItem>", NULL },*/
	{ N_("/_Preferences"),  NULL, GIF_CB (popup_preferences_cb), 0,                  "<StockItem>",  GTK_STOCK_PREFERENCES },
	{ N_("/_About"),        NULL, GIF_CB (popup_about_cb),       0,                  "<StockItem>",  GNOME_STOCK_ABOUT },
	{ "/sep1",              NULL, 0,                             0,                  "<Separator>",  NULL },
	{ N_("/_Take a Break"), NULL, GIF_CB (popup_break_cb),       POPUP_ITEM_BREAK,   "<Item>",       NULL }
};

GConfClient *client = NULL;
extern gboolean debug;

static void
setup_debug_values (DrWright *dr)
{
	dr->type_time = 5;
	dr->warn_time = 4;
	dr->break_time = 10;
}

static void
update_icon (DrWright *dr)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *tmp_pixbuf;
	gint       width, height;
	gfloat     r;
	gint       offset;
	gboolean   set_pixbuf;

	if (!dr->enabled) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), dr->disabled_bar);
		return;
	}
	
	tmp_pixbuf = gdk_pixbuf_copy (dr->neutral_bar);

	width = gdk_pixbuf_get_width (tmp_pixbuf);
	height = gdk_pixbuf_get_height (tmp_pixbuf);

	set_pixbuf = TRUE;

	switch (dr->state) {
	case STATE_BREAK:
	case STATE_BREAK_SETUP:
		r = 1;
		break;
		
	case STATE_BREAK_DONE:
	case STATE_BREAK_DONE_SETUP:
	case STATE_START:
		r = 0;
		break;
		
	case STATE_WARN_IDLE:
	case STATE_WARN_TYPE:
		r = ((float)(dr->type_time - dr->warn_time) / dr->type_time) +
			(float) g_timer_elapsed (dr->timer, NULL) / (float) dr->warn_time;
		break;

	default:
		r = (float) g_timer_elapsed (dr->timer, NULL) / (float) dr->type_time;
		break;
	}

	offset = CLAMP ((height - 0) * (1.0 - r), 1, height - 0);
	
	switch (dr->state) {
	case STATE_WARN_TYPE:
	case STATE_WARN_IDLE:
		pixbuf = dr->red_bar;
		set_pixbuf = FALSE;
		break;

	case STATE_BREAK_SETUP:
	case STATE_BREAK:
		pixbuf = dr->red_bar;
		break;

	default:
		pixbuf = dr->green_bar;
	}		
	
	gdk_pixbuf_composite (pixbuf,
			      tmp_pixbuf,
			      0,
			      offset,
			      width,
			      height - offset,
			      0,
			      0,
			      1.0,
			      1.0,
			      GDK_INTERP_BILINEAR,
			      255);
	
	if (set_pixbuf) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), tmp_pixbuf);
	}
	
	if (dr->composite_bar) {
		g_object_unref (dr->composite_bar);
	}

	dr->composite_bar = tmp_pixbuf;
}

static gboolean
blink_timeout_cb (DrWright *dr)
{
	gfloat r;
	gint   timeout;
	
	r = (dr->warn_time - g_timer_elapsed (dr->timer, NULL)) / dr->warn_time;
	timeout = BLINK_TIMEOUT + BLINK_TIMEOUT_FACTOR * r;

	if (timeout < BLINK_TIMEOUT_MIN) {
		timeout = BLINK_TIMEOUT_MIN;
	}
	
	if (dr->blink_on || timeout == 0) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), dr->composite_bar);
	} else {
		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), dr->neutral_bar);
	}
	
	dr->blink_on = !dr->blink_on;

	if (timeout) {
		dr->blink_timeout_id = g_timeout_add (timeout,
						      (GSourceFunc) blink_timeout_cb,
						      dr);
	} else {
		dr->blink_timeout_id = 0;
	}
		
	return FALSE;
}

static void
start_blinking (DrWright *dr)
{
	if (!dr->blink_timeout_id) {
		dr->blink_on = TRUE;
		blink_timeout_cb (dr);
	}

	/*gtk_widget_show (GTK_WIDGET (dr->icon));*/
}

static void
stop_blinking (DrWright *dr)
{
	if (dr->blink_timeout_id) {
		g_source_remove (dr->blink_timeout_id);
		dr->blink_timeout_id = 0;
	}

	/*gtk_widget_hide (GTK_WIDGET (dr->icon));*/
}

static gboolean
grab_keyboard_on_window (GdkWindow *window,
			 guint32    activate_time)
{
	GdkGrabStatus status;
	
	status = gdk_keyboard_grab (window, TRUE, activate_time);
	if (status == GDK_GRAB_SUCCESS) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
maybe_change_state (DrWright *dr)
{
	gint elapsed_time;
	gint elapsed_idle_time;

	if (debug) {
		g_timer_reset (dr->idle_timer);
	}
	
	elapsed_time = g_timer_elapsed (dr->timer, NULL);
	elapsed_idle_time = g_timer_elapsed (dr->idle_timer, NULL);

	if (elapsed_time > dr->last_elapsed_time + dr->warn_time) {
		/* If the timeout is delayed by the amount of warning time, then
		 * we must have been suspended or stopped, so we just start
		 * over.
		 */
		dr->state = STATE_START;
	}

	switch (dr->state) {
	case STATE_START:
		if (dr->break_window) {
			gtk_widget_destroy (dr->break_window);
			dr->break_window = NULL;
		}

		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), dr->neutral_bar);

		g_timer_start (dr->timer);
		g_timer_start (dr->idle_timer);

		if (dr->enabled) {
			dr->state = STATE_IDLE;
		}
		
		update_tooltip (dr);
		stop_blinking (dr);
		break;

	case STATE_IDLE:
		if (elapsed_idle_time >= dr->break_time) {
			g_timer_start (dr->timer);
			g_timer_start (dr->idle_timer);
		} else if (dr->is_active) {
			dr->state = STATE_TYPE;
		}
		break;

	case STATE_TYPE:
		if (elapsed_time >= dr->type_time - dr->warn_time) {
			dr->state = STATE_WARN_TYPE;
			g_timer_start (dr->timer);

			start_blinking (dr);
 		} else if (elapsed_time >= dr->type_time) {
			dr->state = STATE_BREAK_SETUP;
		}
		else if (!dr->is_active) {
			dr->state = STATE_IDLE;
			g_timer_start (dr->idle_timer);
		}
		break;

	case STATE_WARN_TYPE:
		if (elapsed_time >= dr->warn_time) {
			dr->state = STATE_BREAK_SETUP;
		}
		else if (!dr->is_active) {
			dr->state = STATE_WARN_IDLE;
		}
		break;

	case STATE_WARN_IDLE:
		if (elapsed_idle_time >= dr->break_time) {
			dr->state = STATE_BREAK_DONE_SETUP;
		}
		else if (dr->is_active) {
			dr->state = STATE_WARN_TYPE;
		}
		
		break;
		
	case STATE_BREAK_SETUP:
		/* Don't allow more than one break window to coexist, can happen
		 * if a break is manually enforced.
		 */
		if (dr->break_window) {
			dr->state = STATE_BREAK;
			break;
		}
		
		stop_blinking (dr);
		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), dr->red_bar);

		g_timer_start (dr->timer);

		dr->break_window = drw_break_window_new ();

		g_signal_connect (dr->break_window,
				  "done",
				  G_CALLBACK (break_window_done_cb),
				  dr);

		g_signal_connect (dr->break_window,
				  "postpone",
				  G_CALLBACK (break_window_postpone_cb),
				  dr);

		g_signal_connect (dr->break_window,
				  "destroy",
				  G_CALLBACK (break_window_destroy_cb),
				  dr);

		dr->secondary_break_windows = create_secondary_break_windows ();

		gtk_widget_show (dr->break_window);

		grab_keyboard_on_window (dr->break_window->window, gtk_get_current_event_time ());
		
		dr->state = STATE_BREAK;
		break;
	       
	case STATE_BREAK:
		if (elapsed_time >= dr->break_time) {
			dr->state = STATE_BREAK_DONE_SETUP;
		}
		break;

	case STATE_BREAK_DONE_SETUP:
		stop_blinking (dr);
		gtk_image_set_from_pixbuf (GTK_IMAGE (dr->icon_image), dr->green_bar);

		dr->state = STATE_BREAK_DONE;
		break;
			
	case STATE_BREAK_DONE:
		if (dr->is_active) {
			dr->state = STATE_START;
			if (dr->break_window) {
				gtk_widget_destroy (dr->break_window);
				dr->break_window = NULL;
			}
		}
		break;
	}

	dr->is_active = FALSE;
	dr->last_elapsed_time = elapsed_time;
	
	update_icon (dr);
		
	return TRUE;
}

static gboolean
update_tooltip (DrWright *dr)
{
	gint   elapsed_time, min;
	gchar *str;

	if (!dr->enabled) {
		gtk_tooltips_set_tip (GTK_TOOLTIPS (dr->tooltips),
				      dr->icon_event_box,
				      _("Disabled"), _("Disabled"));
		return TRUE;
	}
	
	elapsed_time = g_timer_elapsed (dr->timer, NULL);

	switch (dr->state) {
	case STATE_WARN_TYPE:
	case STATE_WARN_IDLE:
		min = floor (0.5 + (dr->warn_time - elapsed_time) / 60.0);
		break;
		
	default:
		min = floor (0.5 + (dr->type_time - elapsed_time) / 60.0);
		break;
	}

	if (min >= 1) {
		str = g_strdup_printf (ngettext("%d minute until the next break",
						"%d minutes until the next break", 
						min), min);
	} else {
		str = g_strdup_printf (_("Less than one minute until the next break"));
	}
	
	gtk_tooltips_set_tip (GTK_TOOLTIPS (dr->tooltips),
			      dr->icon_event_box,
			      str, str);

	g_free (str);

	return TRUE;
}

static void
activity_detected_cb (DrwMonitor *monitor,
		      DrWright   *dr)
{
	dr->is_active = TRUE;
	g_timer_start (dr->idle_timer);
}

static void
gconf_notify_cb (GConfClient *client,
		 guint        cnxn_id,
		 GConfEntry  *entry,
		 gpointer     user_data)
{
	DrWright  *dr = user_data;
	GtkWidget *item;
	
	if (!strcmp (entry->key, GCONF_PATH "/type_time")) {
		if (entry->value->type == GCONF_VALUE_INT) {
			dr->type_time = 60 * gconf_value_get_int (entry->value);
			dr->warn_time = MIN (dr->type_time / 10, 5*60);
			
			dr->state = STATE_START;
		}
	}
	else if (!strcmp (entry->key, GCONF_PATH "/break_time")) {
		if (entry->value->type == GCONF_VALUE_INT) {
			dr->break_time = 60 * gconf_value_get_int (entry->value);
			dr->state = STATE_START;
		}
	}
	else if (!strcmp (entry->key, GCONF_PATH "/enabled")) {
		if (entry->value->type == GCONF_VALUE_BOOL) {
			dr->enabled = gconf_value_get_bool (entry->value);
			dr->state = STATE_START;

			item = gtk_item_factory_get_widget_by_action (dr->popup_factory,
								      POPUP_ITEM_BREAK);
			gtk_widget_set_sensitive (item, dr->enabled);
			
			update_tooltip (dr);
		}
	}
		
	maybe_change_state (dr);
}

static void
popup_break_cb (gpointer   callback_data,
		guint      action,
		GtkWidget *widget)
{
	DrWright  *dr = callback_data;

	if (dr->enabled) {
		dr->state = STATE_BREAK_SETUP;
		maybe_change_state (dr);
	}
}

static void
popup_preferences_cb (gpointer   callback_data,
		      guint      action,
		      GtkWidget *widget)
{
	GdkScreen *screen;
	GError    *error = NULL;

	screen = gtk_widget_get_screen (widget);

	if (!egg_spawn_command_line_async_on_screen ("gnome-keyboard-properties --typing-break", screen, &error)) {
		GtkWidget *error_dialog;

		error_dialog = gtk_message_dialog_new (NULL, 0,
						       GTK_MESSAGE_ERROR,
						       GTK_BUTTONS_CLOSE,
						       _("Unable to bring up the typing break properties dialog with the following error: %s"),
						       error->message);
		g_signal_connect (error_dialog,
				  "response",
				  G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_window_set_resizable (GTK_WINDOW (error_dialog), FALSE);
		gtk_widget_show (error_dialog);
		
		g_error_free (error);
	}
}

static void
about_response_cb (GtkWidget *dialog,
		   gint       response,
		   gpointer   user_data)
{
	gtk_widget_destroy (dialog);
}
	
static void
popup_about_cb (gpointer   callback_data,
		guint      action,
		GtkWidget *widget)
{
	static GtkWidget *about_window;
	GtkWidget        *vbox;
	GtkWidget        *label;
	GdkPixbuf        *icon;
	gchar            *markup;

	if (about_window) {
		gtk_window_present (GTK_WINDOW (about_window));
		return;
	}
	
	about_window = gtk_dialog_new ();

	g_signal_connect (about_window,
			  "destroy",
                          G_CALLBACK (gtk_widget_destroyed),
			  &about_window);
	
	gtk_dialog_add_button (GTK_DIALOG (about_window),
			       GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (about_window),
					 GTK_RESPONSE_OK);
	
	gtk_window_set_title (GTK_WINDOW (about_window), _("About GNOME Typing Monitor"));
	icon = NULL; /*gdk_pixbuf_new_from_file (IMAGEDIR "/bar.png", NULL);*/
	if (icon != NULL) {
		gtk_window_set_icon (GTK_WINDOW (about_window), icon);
		g_object_unref (icon);
	}
	
	gtk_window_set_resizable (GTK_WINDOW (about_window), FALSE);
	gtk_window_set_position (GTK_WINDOW (about_window), 
				 GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_type_hint (GTK_WINDOW (about_window), 
				  GDK_WINDOW_TYPE_HINT_DIALOG);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (about_window)->vbox), vbox, FALSE, FALSE, 0);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_CENTER);
	markup = g_strdup_printf ("<span size=\"xx-large\" weight=\"bold\">Typing Monitor " VERSION "</span>\n\n"
				  "%s\n\n"
				  "<span size=\"small\">%s</span>\n"
				  "<span size=\"small\">%s</span>\n",
				  _("A computer break reminder."),
				  _("Written by Richard Hult &lt;richard@imendio.com&gt;"),
				  _("Eye candy added by Anders Carlsson"));
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
	
	gtk_widget_show_all (about_window);

	g_signal_connect (about_window,
			  "response", G_CALLBACK (about_response_cb),
			  NULL);
}

static void
popup_menu_position_cb (GtkMenu  *menu,
			gint     *x,
			gint     *y,
			gboolean *push_in,
			gpointer  data)
{
	GtkWidget      *w = data;
	GtkRequisition  requisition;
	gint            wx, wy;

	g_return_if_fail (w != NULL);

	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	gdk_window_get_origin (w->window, &wx, &wy);

	if (*x < wx)
		*x = wx;
	else if (*x > wx + w->allocation.width)
		*x = wx + w->allocation.width;

	if (*x + requisition.width > gdk_screen_width())
		*x = gdk_screen_width() - requisition.width;

	if (*y < wy)
		*y = wy;
	 else if (*y > wy + w->allocation.height)
		*y = wy + w->allocation.height;

	if (*y + requisition.height > gdk_screen_height())
		*y = gdk_screen_height() - requisition.height;

	*push_in = TRUE;
}

static gboolean
icon_button_press_cb (GtkWidget      *widget,
		      GdkEventButton *event,
		      DrWright       *dr)
{
	GtkWidget *menu;

	if (event->button == 3) {
		menu = gtk_item_factory_get_widget (dr->popup_factory, "");

		gtk_menu_popup (GTK_MENU (menu),
				NULL,
				NULL,
				popup_menu_position_cb,
				dr->icon,
				event->button,
				event->time);

		return TRUE;
	}
	
	return FALSE;
}

static void
popup_menu_cb (GtkWidget *widget,
	       DrWright  *dr)
{
	GtkWidget *menu;
	
	menu = gtk_item_factory_get_widget (dr->popup_factory, "");
	
	gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			popup_menu_position_cb,
			dr->icon,
			0,
			gtk_get_current_event_time());
}

static void
break_window_done_cb (GtkWidget *window,
		      DrWright  *dr)
{
	gtk_widget_destroy (dr->break_window);
	
	dr->state = STATE_BREAK_DONE_SETUP;
	dr->break_window = NULL;
	
	maybe_change_state (dr);
}

static void
break_window_postpone_cb (GtkWidget *window,
			  DrWright  *dr)
{
	gtk_widget_destroy (dr->break_window);

	dr->state = STATE_WARN_TYPE;
	dr->break_window = NULL;

	g_timer_start (dr->timer);
	start_blinking (dr);
	update_icon (dr);
	update_tooltip (dr);
}

static void
break_window_destroy_cb (GtkWidget *window,
			 DrWright  *dr)
{
	GList *l;

	for (l = dr->secondary_break_windows; l; l = l->next) {
		gtk_widget_destroy (l->data);
	}
	
	g_list_free (dr->secondary_break_windows);
	dr->secondary_break_windows = NULL;
}

static char *
item_factory_trans_cb (const gchar *path,
		       gpointer     data)
{
	return _((gchar*) path);
}

static void
icon_event_box_destroy_cb (GtkWidget *widget,
			   DrWright  *dr)
{
	gtk_widget_destroy (GTK_WIDGET (dr->icon));
	init_tray_icon (dr);
}

static gboolean
icon_event_box_expose_event_cb (GtkWidget      *widget,
				GdkEventExpose *event,
				DrWright       *dr)
{
	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		gint focus_width, focus_pad;
		gint x, y, width, height;
		
		gtk_widget_style_get (widget,
				      "focus-line-width", &focus_width,
				      "focus-padding", &focus_pad,
				      NULL);
		x = widget->allocation.x + focus_pad;
		y = widget->allocation.y + focus_pad;
		width = widget->allocation.width - 2 * focus_pad;
		height = widget->allocation.height - 2 * focus_pad;

		gtk_paint_focus (widget->style, widget->window,
				 GTK_WIDGET_STATE (widget),
				 &event->area, widget, "button",
				 x, y, width, height);
	}

	return FALSE;
}

static void
init_tray_icon (DrWright *dr)
{
	dr->icon = egg_tray_icon_new (_("Break reminder"));

	dr->icon_event_box = gtk_event_box_new ();
	dr->icon_image = gtk_image_new_from_pixbuf (dr->neutral_bar);
	gtk_container_add (GTK_CONTAINER (dr->icon_event_box), dr->icon_image);
		
	gtk_widget_add_events (GTK_WIDGET (dr->icon), GDK_BUTTON_PRESS_MASK | GDK_FOCUS_CHANGE_MASK);
	gtk_container_add (GTK_CONTAINER (dr->icon), dr->icon_event_box);
	gtk_widget_show_all (GTK_WIDGET (dr->icon));

	GTK_WIDGET_SET_FLAGS (dr->icon_event_box, GTK_CAN_FOCUS);
	
	update_tooltip (dr);
	update_icon (dr);

	g_signal_connect (dr->icon,
			  "button_press_event",
			  G_CALLBACK (icon_button_press_cb),
			  dr);

	g_signal_connect (dr->icon,
			  "destroy",
			  G_CALLBACK (icon_event_box_destroy_cb),
			  dr);

	g_signal_connect (dr->icon,
			  "popup_menu",
			  G_CALLBACK (popup_menu_cb),
			  dr);
	
	g_signal_connect_after (dr->icon_event_box,
				"expose_event",
				G_CALLBACK (icon_event_box_expose_event_cb),
				dr);
}

static GList *
create_secondary_break_windows (void)
{
	GdkDisplay *display;
	GdkScreen  *screen;
	GtkWidget  *window;
	gint        i;
	GList      *windows = NULL;

	display = gdk_display_get_default ();
	
	for (i = 0; i < gdk_display_get_n_screens (display); i++) {
		screen = gdk_display_get_screen (display, i);
		
		if (screen == gdk_screen_get_default ()) {
			/* Handled by DrwBreakWindow. */
			continue;
		}
		
		window = gtk_window_new (GTK_WINDOW_POPUP);
		
		windows = g_list_prepend (windows, window);
		
		gtk_window_set_screen (GTK_WINDOW (window), screen);
		
		gtk_window_set_default_size (GTK_WINDOW (window),
					     gdk_screen_get_width (screen),
					     gdk_screen_get_height (screen));
		
		gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
		gtk_widget_realize (GTK_WIDGET (window));
		
		drw_setup_background (GTK_WIDGET (window));
		gtk_window_stick (GTK_WINDOW (window));
		gtk_widget_show (window);
	}
	
	return windows;
}

DrWright *
drwright_new (void)
{
	DrWright  *dr;
	GtkWidget *item;

        dr = g_new0 (DrWright, 1);

	client = gconf_client_get_default ();
	
	gconf_client_add_dir (client,
			      GCONF_PATH,
			      GCONF_CLIENT_PRELOAD_NONE,
			      NULL);

	gconf_client_notify_add (client, GCONF_PATH,
				 gconf_notify_cb,
				 dr,
				 NULL,
				 NULL);
	
	dr->type_time = 60 * gconf_client_get_int (
		client, GCONF_PATH "/type_time", NULL);
	
	dr->warn_time = MIN (dr->type_time / 12, 60*3);
	
	dr->break_time = 60 * gconf_client_get_int (
		client, GCONF_PATH "/break_time", NULL);

	dr->enabled = gconf_client_get_bool (
		client,
		GCONF_PATH "/enabled",
		NULL);

	if (debug) {
		setup_debug_values (dr);
	}
	
	dr->popup_factory = gtk_item_factory_new (GTK_TYPE_MENU,
						      "<main>",
						      NULL);
	gtk_item_factory_set_translate_func (dr->popup_factory,
					     item_factory_trans_cb,
					     NULL,
					     NULL);
	
	gtk_item_factory_create_items (dr->popup_factory,
				       G_N_ELEMENTS (popup_items),
				       popup_items,
				       dr);

	/*item = gtk_item_factory_get_widget_by_action (dr->popup_factory, POPUP_ITEM_ENABLED);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), dr->enabled);*/

	item = gtk_item_factory_get_widget_by_action (dr->popup_factory, POPUP_ITEM_BREAK);
	gtk_widget_set_sensitive (item, dr->enabled);
	
	dr->timer = g_timer_new ();
	dr->idle_timer = g_timer_new ();
	
	dr->state = STATE_START;

	dr->monitor = drw_monitor_new ();

	g_signal_connect (dr->monitor,
			  "activity",
			  G_CALLBACK (activity_detected_cb),
			  dr);

	dr->neutral_bar = gdk_pixbuf_new_from_file (IMAGEDIR "/bar.png", NULL);
	dr->red_bar = gdk_pixbuf_new_from_file (IMAGEDIR "/bar-red.png", NULL);
	dr->green_bar = gdk_pixbuf_new_from_file (IMAGEDIR "/bar-green.png", NULL);
	dr->disabled_bar = gdk_pixbuf_new_from_file (IMAGEDIR "/bar-disabled.png", NULL);

	dr->tooltips = gtk_tooltips_new ();

	init_tray_icon (dr);
	
	g_timeout_add (15*1000,
		       (GSourceFunc) update_tooltip,
		       dr);
	g_timeout_add (500,
		       (GSourceFunc) maybe_change_state,
		       dr);

	return dr;
}
       
