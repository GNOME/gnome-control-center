/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002      CodeFactory AB
 * Copyright (C) 2002-2003 Richard Hult <richard@imendio.com>

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
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gconf/gconf-client.h>
#include <libgnome/gnome-i18n.h>
#include "drwright.h"
#include "drw-utils.h"
#include "drw-break-window.h"

struct _DrwBreakWindowPriv {
	GtkWidget *clock_label;
	GtkWidget *break_label;
	GtkWidget *image;

	GtkWidget *postpone_entry;
	GtkWidget *postpone_button;
	
	GTimer    *timer;

	gint       break_time;
	
	gchar     *break_text;
	guint      clock_timeout_id;
	guint      postpone_timeout_id;
	guint      postpone_sensitize_id;
};

#define POSTPONE_CANCEL 30*1000

/* Signals */
enum {
	DONE,
	POSTPONE,
	LAST_SIGNAL
};

static void         drw_break_window_class_init    (DrwBreakWindowClass *klass);
static void         drw_break_window_init          (DrwBreakWindow      *window);
static void         drw_break_window_finalize      (GObject             *object);
static void         drw_break_window_dispose       (GObject             *object);
static gboolean     postpone_sensitize_cb          (DrwBreakWindow      *window);
static gboolean     clock_timeout_cb               (DrwBreakWindow      *window);
static void         postpone_clicked_cb            (GtkWidget           *button,
						    GtkWidget           *window);
static gboolean     label_expose_event_cb          (GtkLabel            *label,
						    GdkEventExpose      *event,
						    gpointer             user_data);
static void         label_size_request_cb          (GtkLabel            *label,
						    GtkRequisition      *requisition,
						    gpointer             user_data);


static GObjectClass *parent_class;
static guint signals[LAST_SIGNAL];

GType
drw_break_window_get_type (void)
{
	static GType object_type = 0;

	if (!object_type) {
		static const GTypeInfo object_info = {
			sizeof (DrwBreakWindowClass),
			NULL,		/* base_init */
			NULL,		/* base_finalize */
			(GClassInitFunc) drw_break_window_class_init,
			NULL,		/* class_finalize */
			NULL,		/* class_data */
			sizeof (DrwBreakWindow),
			0,              /* n_preallocs */
			(GInstanceInitFunc) drw_break_window_init,
		};
		
		object_type = g_type_register_static (GTK_TYPE_WINDOW,
                                                      "DrwBreakWindow", 
                                                      &object_info,
						      0);
	}

	return object_type;
}

static void
drw_break_window_class_init (DrwBreakWindowClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
        parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
        
        object_class->finalize = drw_break_window_finalize;
        object_class->dispose = drw_break_window_dispose;

	signals[POSTPONE] = 
		g_signal_new ("postpone",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[DONE] = 
		g_signal_new ("done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
drw_break_window_init (DrwBreakWindow *window)
{
        DrwBreakWindowPriv *priv;
	GtkWidget          *vbox;
	GtkWidget          *hbox;
	GtkWidget          *frame;
	GtkWidget          *align;
	gchar              *str;
	GtkWidget          *outer_vbox;
	GtkWidget          *button_box;
	gboolean            allow_postpone;

        priv = g_new0 (DrwBreakWindowPriv, 1);
        window->priv = priv;

	priv->break_time = 60 * gconf_client_get_int (gconf_client_get_default (),
						      GCONF_PATH "/break_time",
						      NULL);
	
	allow_postpone = gconf_client_get_bool (gconf_client_get_default (),
					      GCONF_PATH "/allow_postpone",
					      NULL);

	GTK_WINDOW (window)->type = GTK_WINDOW_POPUP;

	gtk_window_set_default_size (GTK_WINDOW (window),
				     gdk_screen_width (),
				     gdk_screen_height ());
	
	gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
	gtk_widget_realize (GTK_WIDGET (window));

	drw_setup_background (GTK_WIDGET (window));
	
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_widget_show (frame);
	
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show (align);

	outer_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (outer_vbox);
	
	gtk_container_add (GTK_CONTAINER (window), outer_vbox);

	gtk_box_pack_start (GTK_BOX (outer_vbox), align, TRUE, TRUE, 0);

	if (allow_postpone) {
		button_box = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (button_box);
		
		gtk_container_set_border_width (GTK_CONTAINER (button_box), 12);
		
		priv->postpone_button = gtk_button_new_with_mnemonic (_("_Postpone break"));
		gtk_widget_show (priv->postpone_button);

		gtk_widget_set_sensitive (priv->postpone_button, FALSE);

		if (priv->postpone_sensitize_id) {
			g_source_remove (priv->postpone_sensitize_id);
		}
		
		priv->postpone_sensitize_id = g_timeout_add (500,
							     (GSourceFunc) postpone_sensitize_cb,
							     window);
	
		g_signal_connect (priv->postpone_button,
				  "clicked",
				  G_CALLBACK (postpone_clicked_cb),
				  window);
		
		gtk_box_pack_end (GTK_BOX (button_box), priv->postpone_button, FALSE, TRUE, 0);

		priv->postpone_entry = gtk_entry_new ();
		gtk_entry_set_has_frame (GTK_ENTRY (priv->postpone_entry), FALSE);

		gtk_box_pack_end (GTK_BOX (button_box), priv->postpone_entry, FALSE, TRUE, 4);
		
		gtk_box_pack_end (GTK_BOX (outer_vbox), button_box, FALSE, TRUE, 0);
	}
	
	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	
	gtk_container_add (GTK_CONTAINER (align), frame);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	priv->break_label = gtk_label_new (NULL);
	gtk_widget_show (priv->break_label);

	g_signal_connect (priv->break_label,
			  "expose_event",
			  G_CALLBACK (label_expose_event_cb),
			  NULL);

	g_signal_connect_after (priv->break_label,
				"size_request",
				G_CALLBACK (label_size_request_cb),
				NULL);

	str = g_strdup_printf ("<span size=\"xx-large\" foreground=\"white\"><b>%s</b></span>",
			       _("Take a break!"));
	gtk_label_set_markup (GTK_LABEL (priv->break_label), str);
	g_free (str);
		
	gtk_box_pack_start (GTK_BOX (vbox), priv->break_label, FALSE, FALSE, 12);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0); 
	
	priv->image = gtk_image_new_from_file (IMAGEDIR "/stop.png");
	gtk_misc_set_alignment (GTK_MISC (priv->image), 1, 0.5);
	gtk_widget_show (priv->image);
	gtk_box_pack_start (GTK_BOX (hbox), priv->image, TRUE, TRUE, 8); 
	
	priv->clock_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (priv->clock_label), 0, 0.5);
	gtk_widget_show (priv->clock_label);
	gtk_box_pack_start (GTK_BOX (hbox), priv->clock_label, TRUE, TRUE, 8); 

	g_signal_connect (priv->clock_label,
			  "expose_event",
			  G_CALLBACK (label_expose_event_cb),
			  NULL);
	
	g_signal_connect_after (priv->clock_label,
				"size_request",
				G_CALLBACK (label_size_request_cb),
				NULL);

	gtk_window_stick (GTK_WINDOW (window));
	
	priv->timer = g_timer_new ();
	
	/* Make sure we have a valid time label from the start. */
	clock_timeout_cb (window);
	
	priv->clock_timeout_id = g_timeout_add (1000,
						(GSourceFunc) clock_timeout_cb,
						window);
}

static void
drw_break_window_finalize (GObject *object)
{
        DrwBreakWindow     *window = DRW_BREAK_WINDOW (object);
        DrwBreakWindowPriv *priv;
        
        priv = window->priv;

	if (priv->clock_timeout_id != 0) {
		g_source_remove (priv->clock_timeout_id);
	}

	if (priv->postpone_timeout_id != 0) {
		g_source_remove (priv->postpone_timeout_id);
	}

	if (priv->postpone_sensitize_id != 0) {
		g_source_remove (priv->postpone_sensitize_id);
	}

	g_free (priv);
	window->priv = NULL;

        if (G_OBJECT_CLASS (parent_class)->finalize) {
                (* G_OBJECT_CLASS (parent_class)->finalize) (object);
        }
}

static void
drw_break_window_dispose (GObject *object)
{
        DrwBreakWindow     *window = DRW_BREAK_WINDOW (object);
        DrwBreakWindowPriv *priv;
        
        priv = window->priv;

	if (priv->clock_timeout_id != 0) {
		g_source_remove (priv->clock_timeout_id);
		priv->clock_timeout_id = 0;
	}

	if (priv->postpone_timeout_id != 0) {
		g_source_remove (priv->postpone_timeout_id);
		priv->postpone_timeout_id = 0;
	}

	if (priv->postpone_sensitize_id != 0) {
		g_source_remove (priv->postpone_sensitize_id);
	}
	
        if (G_OBJECT_CLASS (parent_class)->dispose) {
                (* G_OBJECT_CLASS (parent_class)->dispose) (object);
        }
}

GtkWidget *
drw_break_window_new (void)
{
	return g_object_new (DRW_TYPE_BREAK_WINDOW, NULL);
}

static gboolean
postpone_sensitize_cb (DrwBreakWindow *window)
{
	DrwBreakWindowPriv *priv;

	priv = window->priv;

	gtk_widget_set_sensitive (priv->postpone_button, TRUE);

	priv->postpone_sensitize_id = 0;
	return FALSE;
}

static gboolean
clock_timeout_cb (DrwBreakWindow *window)
{
	DrwBreakWindowPriv *priv;
	gchar              *txt;
	gint                minutes;
	gint                seconds;

	g_return_val_if_fail (DRW_IS_BREAK_WINDOW (window), FALSE);

	priv = window->priv;
	
	seconds = 1 + priv->break_time - g_timer_elapsed (priv->timer, NULL);
	seconds = MAX (0, seconds);

	if (seconds == 0) {
		/* Zero this out so the finalizer doesn't try to remove the
		 * source, which would be done in the timeout callback ==
		 * no-no.
		 */
		priv->clock_timeout_id = 0;

		g_signal_emit (window, signals[DONE], 0, NULL);

		return FALSE;
	}

	minutes = seconds / 60;
	seconds -= minutes * 60;

	txt = g_strdup_printf ("<span size=\"25000\" foreground=\"white\"><b>%d:%02d</b></span>",
			       minutes,
			       seconds);
	gtk_label_set_markup (GTK_LABEL (priv->clock_label), txt);
	g_free (txt);

	return TRUE;
}

static void
postpone_entry_activate_cb (GtkWidget      *entry,
			  DrwBreakWindow *window)
{
	const gchar *str;
	const gchar *phrase;

	str = gtk_entry_get_text (GTK_ENTRY (entry));

	phrase = gconf_client_get_string (gconf_client_get_default (),
					  GCONF_PATH "/unlock_phrase",
					  NULL);
	
	if (!strcmp (str, phrase)) {
		g_signal_emit (window, signals[POSTPONE], 0, NULL);
		return;
	}

	gtk_entry_set_text (GTK_ENTRY (entry), "");
}

static gboolean
grab_on_window (GdkWindow *window,
		guint32    activate_time)
{
	if ((gdk_pointer_grab (window, TRUE,
			       GDK_BUTTON_PRESS_MASK |
			       GDK_BUTTON_RELEASE_MASK |
			       GDK_POINTER_MOTION_MASK,
			       NULL, NULL, activate_time) == 0)) {
		if (gdk_keyboard_grab (window, TRUE,
			       activate_time) == 0)
			return TRUE;
		else {
			gdk_pointer_ungrab (activate_time);
			return FALSE;
		}
	}

	return FALSE;
}

static gboolean
postpone_cancel_cb (DrwBreakWindow *window)
{
	DrwBreakWindowPriv *priv;

	priv = window->priv;

	gtk_entry_set_text (GTK_ENTRY (priv->postpone_entry), "");
	gtk_widget_hide (priv->postpone_entry);

	priv->postpone_timeout_id = 0;
	
	return FALSE;
}

static gboolean
postpone_entry_key_press_event_cb (GtkEntry       *entry,
				 GdkEventKey    *event,
				 DrwBreakWindow *window)
{
	DrwBreakWindowPriv *priv;

	priv = window->priv;

	if (event->keyval == GDK_Escape) {
		if (priv->postpone_timeout_id) {
			g_source_remove (priv->postpone_timeout_id);
		}
		
		postpone_cancel_cb (window);

		return TRUE;
	}
	
	g_source_remove (priv->postpone_timeout_id);
	
	priv->postpone_timeout_id = g_timeout_add (POSTPONE_CANCEL, (GSourceFunc) postpone_cancel_cb, window);

	return FALSE;
}

static void
postpone_clicked_cb (GtkWidget *button,
		   GtkWidget *window)
{
	DrwBreakWindow     *bw = DRW_BREAK_WINDOW (window);
	DrwBreakWindowPriv *priv = bw->priv;
	gchar              *phrase;
	
	/* Disable the phrase for now. */
	phrase = NULL; /*gconf_client_get_string (gconf_client_get_default (),
					  GCONF_PATH "/unlock_phrase",
					  NULL);*/

	if (!phrase || !phrase[0]) {
		g_signal_emit (window, signals[POSTPONE], 0, NULL);
		return;
	}

	if (GTK_WIDGET_VISIBLE (priv->postpone_entry)) {
		gtk_widget_activate (priv->postpone_entry);
		return;
	}
	
	gtk_widget_show (priv->postpone_entry);

	priv->postpone_timeout_id = g_timeout_add (POSTPONE_CANCEL, (GSourceFunc) postpone_cancel_cb, bw);
	
	grab_on_window (priv->postpone_entry->window,  gtk_get_current_event_time ());
	
	gtk_widget_grab_focus (priv->postpone_entry);

	g_signal_connect (priv->postpone_entry,
			  "activate",
			  G_CALLBACK (postpone_entry_activate_cb),
			  bw);

	g_signal_connect (priv->postpone_entry,
			  "key_press_event",
			  G_CALLBACK (postpone_entry_key_press_event_cb),
			  bw);
}

static void
get_layout_location (GtkLabel *label,
                     gint     *xp,
                     gint     *yp)
{
	GtkMisc   *misc;
	GtkWidget *widget;
	gfloat     xalign;
	gint       x, y;
	
	misc = GTK_MISC (label);
	widget = GTK_WIDGET (label);
	
	if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) {
		xalign = misc->xalign;
	} else {
		xalign = 1.0 - misc->xalign;
	}
	
	x = floor (widget->allocation.x + (int)misc->xpad
		   + ((widget->allocation.width - widget->requisition.width - 1) * xalign)
		   + 0.5);
	
	y = floor (widget->allocation.y + (int)misc->ypad 
		   + ((widget->allocation.height - widget->requisition.height - 1) * misc->yalign)
		   + 0.5);
	
	if (xp) {
		*xp = x;
	}
	
	if (yp) {
		*yp = y;
	}
}

static gboolean
label_expose_event_cb (GtkLabel       *label,
		       GdkEventExpose *event,
		       gpointer        user_data)
{
	gint       x, y;
	GdkColor   color;
	GtkWidget *widget;
	GdkGC     *gc;

	color.red = 0;
	color.green = 0;
	color.blue = 0;
	color.pixel = 0;

	get_layout_location (label, &x, &y);

	widget = GTK_WIDGET (label);
	gc = gdk_gc_new (widget->window);
	gdk_gc_set_rgb_fg_color (gc, &color);
	gdk_gc_set_clip_rectangle (gc, &event->area);

	gdk_draw_layout_with_colors (widget->window,
				     gc,
				     x + 1,
				     y + 1,
				     label->layout,
				     &color,
				     NULL);
	g_object_unref (gc);
	
	gtk_paint_layout (widget->style,
			  widget->window,
			  GTK_WIDGET_STATE (widget),
			  FALSE,
			  &event->area,
			  widget,
			  "label",
			  x, y,
			  label->layout);

	return TRUE;
}

static void
label_size_request_cb (GtkLabel       *label,
		       GtkRequisition *requisition,
		       gpointer        user_data)
{
	requisition->width += 1;
	requisition->height += 1;
}
