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

#include "prefs-widget.h"
#include "applier.h"

#include <gdk-pixbuf/gdk-pixbuf-xlibrgb.h>

#define WID(str) (glade_xml_get_widget (prefs_widget->dialog_data, str))

enum {
	ARG_0,
	ARG_PREFERENCES
};

static CappletWidgetClass *parent_class;

static void prefs_widget_init       (PrefsWidget *prefs_widget);
static void prefs_widget_class_init (PrefsWidgetClass *class);

static void prefs_widget_set_arg    (GtkObject *object,
				     GtkArg *arg,
				     guint arg_id);
static void prefs_widget_get_arg    (GtkObject *object,
				     GtkArg *arg,
				     guint arg_id);

static void append_wallpaper        (GtkMenu *menu,
				     char *label,
				     char *path,
				     PrefsWidget *prefs_widget);
static void read_preferences        (PrefsWidget *prefs_widget,
				     Preferences *prefs);
static void setup_preview           (GtkWidget *widget,
				     PrefsWidget *prefs);

static void set_background_controls_sensitive (PrefsWidget *prefs_widget,
					       gboolean s);

static void wallpaper_entry_changed_cb   (GtkWidget *e,
					  PrefsWidget *prefs_widget);
static void color1_select_color_set_cb   (GnomeColorPicker *cp,
					  guint r, guint g,
					  guint b, guint a,
					  PrefsWidget *prefs_widget);
static void color2_select_color_set_cb   (GnomeColorPicker *cp,
					  guint r, guint g,
					  guint b, guint a,
					  PrefsWidget *prefs_widget);
static void disable_toggled_cb           (GtkToggleButton *tb,
					  PrefsWidget *prefs_widget);
static void color_effect_cb              (GtkWidget *w,
					  PrefsWidget *prefs_widget);
static void wallpaper_effect_cb          (GtkWidget *w,
					  PrefsWidget *prefs_widget);
static void browse_button_cb             (GtkWidget *w,
					  PrefsWidget *prefs_widget);
static void wp_selection_ok_cb           (GtkButton *button,
					  PrefsWidget *prefs_widget);
static void wp_selection_cancel_cb       (GtkButton *button,
					  PrefsWidget *prefs_widget);
static void auto_apply_toggled_cb        (GtkToggleButton *tb,
					  PrefsWidget *prefs_widget);
static void adjust_opacity_toggled_cb    (GtkToggleButton *tb,
					  PrefsWidget *prefs_widget);
static void opacity_adjust_changed_cb    (GtkAdjustment *adjustment,
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
	GtkWidget *widget;
	GtkAdjustment *adjustment;
	GList *node;
	int i;

	prefs_widget->dialog_data = 
		glade_xml_new (GNOMECC_GLADE_DIR "/background-properties.glade",
					"prefs_widget");
	if (prefs_widget->dialog_data == NULL) {
		g_warning ("Could not load \"%s\"\n",
				 GNOMECC_GLADE_DIR "/background-properties.glade");
		return;
	}

	widget = glade_xml_get_widget (prefs_widget->dialog_data, 
				       "prefs_widget");
	gtk_container_add (GTK_CONTAINER (prefs_widget), widget);

	widget = glade_xml_get_widget (prefs_widget->dialog_data,
				       "monitor_frame");

	prefs_widget->preview = applier_class_get_preview_widget ();
	gtk_container_add (GTK_CONTAINER (widget), prefs_widget->preview);

	glade_xml_signal_connect_data (prefs_widget->dialog_data, 
				       "color1_select_color_set_cb",
				       color1_select_color_set_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data, 
				       "color2_select_color_set_cb",
				       color2_select_color_set_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "disable_toggled_cb",
				       disable_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "browse_button_cb",
				       browse_button_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "auto_apply_toggled_cb",
				       auto_apply_toggled_cb,
				       prefs_widget);
	glade_xml_signal_connect_data (prefs_widget->dialog_data,
				       "adjust_opacity_toggled_cb",
				       adjust_opacity_toggled_cb,
				       prefs_widget);

	adjustment = gtk_range_get_adjustment
		(GTK_RANGE (WID ("opacity_adjust")));
	gtk_range_set_adjustment (GTK_RANGE (WID ("opacity_adjust")),
				  adjustment); 
	gtk_signal_connect (GTK_OBJECT (adjustment), "value-changed",
			    GTK_SIGNAL_FUNC (opacity_adjust_changed_cb),
			    prefs_widget);

	widget = WID ("color_option");
	node = GTK_MENU_SHELL (gtk_option_menu_get_menu
			       (GTK_OPTION_MENU (widget)))->children;

	for (i=0; node; i++, node = node->next) {
		gtk_signal_connect (GTK_OBJECT (node->data), "activate",
				    GTK_SIGNAL_FUNC (color_effect_cb),
				    prefs_widget);

		gtk_object_set_data (GTK_OBJECT (node->data), "index",
				     GINT_TO_POINTER (i));
	}


	widget = WID ("wp_effect_option");
	node = GTK_MENU_SHELL (gtk_option_menu_get_menu
			       (GTK_OPTION_MENU (widget)))->children;

	for (i=0; node; i++, node = node->next) {
		gtk_signal_connect (GTK_OBJECT (node->data), "activate",
				    GTK_SIGNAL_FUNC (wallpaper_effect_cb),
				    prefs_widget);

		gtk_object_set_data (GTK_OBJECT (node->data), "index",
				     GINT_TO_POINTER (i));
	}	
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
append_wallpaper (GtkMenu *menu, char *label, char *path,
		  PrefsWidget *prefs_widget)
{
	GtkWidget *item;

	item = gtk_menu_item_new_with_label (label);
	if (path != NULL)
		gtk_object_set_data (GTK_OBJECT (item),
				     "wallpaper_filename", g_strdup (path));
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (wallpaper_entry_changed_cb),
			    prefs_widget);
			    
	gtk_menu_append (menu, item);
}

static void
read_preferences (PrefsWidget *prefs_widget, Preferences *prefs) 
{
	GtkWidget *widget;
	GtkAdjustment *adjustment;
	GtkWidget *menu;
	gint i;
	gint thing;
	GSList *item;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gnome_color_picker_set_i16
		(GNOME_COLOR_PICKER (WID ("color1_select")),
		 prefs->color1->red, prefs->color1->green,
		 prefs->color1->blue, 0xffff);
	gnome_color_picker_set_i16
		(GNOME_COLOR_PICKER (WID ("color2_select")),
		 prefs->color2->red, prefs->color2->green,
		 prefs->color2->blue, 0xffff);

	gtk_widget_set_sensitive
		(glade_xml_get_widget (prefs_widget->dialog_data,
				       "color2_select"), 
		 prefs_widget->prefs->gradient_enabled);
	gtk_widget_set_sensitive
		(glade_xml_get_widget (prefs_widget->dialog_data,
				       "color2_label"), 
		 prefs_widget->prefs->gradient_enabled);

	thing = prefs->gradient_enabled 
		? (prefs->orientation == ORIENTATION_VERT
		   ? 1 : 2) : 0;

	gtk_option_menu_set_history (GTK_OPTION_MENU (WID ("color_option")),
				     thing);

	widget = WID ("wallpaper_entry");

#if 0
	if (prefs->wallpaper_sel_path)
		gnome_file_entry_set_default_path 
			(GNOME_FILE_ENTRY (widget),
			 prefs->wallpaper_sel_path);
#endif

	menu = gtk_menu_new ();
	append_wallpaper (GTK_MENU (menu), _("(None)"), NULL, prefs_widget);

#warning FIXME: add a small snapshot of the image?  that would rule.
	for (thing = 0, i = 1, item = prefs->wallpapers; item; 
	     i++, item = item->next) 
	{
		append_wallpaper (GTK_MENU (menu), g_basename (item->data), 
				  item->data, prefs_widget);
		if (prefs->wallpaper_filename && 
		    !strcmp (prefs->wallpaper_filename, item->data))
			thing = i;
	}

	if (!thing && prefs->wallpaper_filename) {
		thing = 1;
		append_wallpaper (GTK_MENU (menu),
				  g_basename (prefs->wallpaper_filename),
				  prefs->wallpaper_filename, prefs_widget);
	}

	gtk_option_menu_set_history
		(GTK_OPTION_MENU (WID ("wp_effect_option")),
		 prefs->wallpaper_type);

	gtk_option_menu_set_menu
		(GTK_OPTION_MENU (WID ("wp_file_option")), menu);
	gtk_option_menu_set_history
		(GTK_OPTION_MENU (WID ("wp_file_option")), thing);

	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON (WID ("disable_toggle")), prefs->enabled);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
				      (WID ("auto_apply")),
				      prefs->auto_apply);
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON 
				      (WID ("adjust_opacity_toggle")),
				      prefs->adjust_opacity);
	gtk_widget_set_sensitive (GTK_WIDGET (WID ("opacity_box")),
				  prefs->adjust_opacity && prefs->enabled);

	adjustment = gtk_range_get_adjustment
		(GTK_RANGE (WID ("opacity_adjust")));
	gtk_adjustment_set_value (adjustment, prefs->opacity);
	
	preferences_apply_preview (prefs);
}

static void
setup_preview (GtkWidget *widget, PrefsWidget *prefs)
{
	char *p;
	GList *l;
	GtkWidget *pp = NULL;
	GdkImlibImage *im;
	int w,h;
	GtkWidget *frame;
	GtkFileSelection *fs;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_WIDGET (widget));

	frame = gtk_object_get_data (GTK_OBJECT (widget), "frame");
	fs = gtk_object_get_data (GTK_OBJECT (frame), "fs");

	if ((l = gtk_container_children (GTK_CONTAINER (frame))) != NULL) {
		pp = l->data;
		g_list_free (l);
	}

	if (pp) gtk_widget_destroy (pp);
	
	p = gtk_file_selection_get_filename (fs);
	if (!p || !g_file_test (p, G_FILE_TEST_ISLINK | G_FILE_TEST_ISFILE) ||
	   !(im = gdk_imlib_load_image (p)))
		return;

	w = im->rgb_width;
	h = im->rgb_height;
	if (w > h) {
		if (w > 100) {
			h = h * (100.0 / w);
			w = 100;
		}
	} else {
		if (h > 100) {
			w = w * (100.0 / h);
			h = 100;
		}
	}
	pp = gnome_pixmap_new_from_imlib_at_size (im, w, h);
	gtk_widget_show (pp);
	gtk_container_add (GTK_CONTAINER (frame), pp);

	gdk_imlib_destroy_image (im);
}

static void
set_background_controls_sensitive (PrefsWidget *prefs_widget, gboolean s) 
{
	gtk_widget_set_sensitive (WID ("color_frame"), s);
	gtk_widget_set_sensitive (WID ("wallpaper_frame"), s);
	gtk_widget_set_sensitive (WID ("adjust_opacity_toggle"), s);
	gtk_widget_set_sensitive (WID ("opacity_box"),
				  s && prefs_widget->prefs->adjust_opacity);
	gtk_widget_set_sensitive (WID ("auto_apply"), s);
}



static void
wallpaper_entry_changed_cb (GtkWidget *e, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	if (prefs_widget->prefs->wallpaper_filename != NULL)
		g_free (prefs_widget->prefs->wallpaper_filename);

	prefs_widget->prefs->wallpaper_filename =
		g_strdup (gtk_object_get_data (GTK_OBJECT (e),
					       "wallpaper_filename"));

	prefs_widget->prefs->wallpaper_enabled = 
		prefs_widget->prefs->wallpaper_filename &&
		g_file_test (prefs_widget->prefs->wallpaper_filename,
			     G_FILE_TEST_ISFILE);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
color1_select_color_set_cb (GnomeColorPicker *cp, guint r, guint g,
			    guint b, guint a, PrefsWidget *prefs_widget) 
{
	guint32 rgb;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->color1->red = r;
	prefs_widget->prefs->color1->green = g;
	prefs_widget->prefs->color1->blue = b;
	rgb = ((r >> 8) << 16) || ((g >> 8) << 8) || (b >> 8);
	prefs_widget->prefs->color1->pixel = xlib_rgb_xpixel_from_rgb (rgb);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
color2_select_color_set_cb (GnomeColorPicker *cp, guint r, guint g,
			    guint b, guint a, PrefsWidget *prefs_widget) 
{
	guint32 rgb;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->color2->red = r;
	prefs_widget->prefs->color2->green = g;
	prefs_widget->prefs->color2->blue = b;
	rgb = ((r >> 8) << 16) || ((g >> 8) << 8) || (b >> 8);
	prefs_widget->prefs->color2->pixel = xlib_rgb_xpixel_from_rgb (rgb);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
disable_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->enabled = gtk_toggle_button_get_active (tb);

	set_background_controls_sensitive (prefs_widget, 
					   prefs_widget->prefs->enabled);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
color_effect_cb (GtkWidget *w, PrefsWidget *prefs_widget)
{
	switch (GPOINTER_TO_INT
		(gtk_object_get_data (GTK_OBJECT (w), "index")))
	{
	case 0:
		prefs_widget->prefs->gradient_enabled = FALSE;
		break;
	case 1:
		prefs_widget->prefs->gradient_enabled = TRUE;
		prefs_widget->prefs->orientation = ORIENTATION_VERT;
		break;
	case 2:
		prefs_widget->prefs->gradient_enabled = TRUE;
		prefs_widget->prefs->orientation = ORIENTATION_HORIZ;
		break;
	default:
		break;
	}

	gtk_widget_set_sensitive
		(glade_xml_get_widget (prefs_widget->dialog_data,
				       "color2_select"), 
		 prefs_widget->prefs->gradient_enabled);
	gtk_widget_set_sensitive
		(glade_xml_get_widget (prefs_widget->dialog_data,
				       "color2_label"), 
		 prefs_widget->prefs->gradient_enabled);

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
wallpaper_effect_cb (GtkWidget *w, PrefsWidget *prefs_widget)
{
	gint i;

	i = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (w), "index"));

	prefs_widget->prefs->wallpaper_type = i;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
browse_button_cb (GtkWidget *w, PrefsWidget *prefs_widget)
{
	GtkWidget *hbox, *widg;
	GtkFileSelection *filesel;
	
	if (prefs_widget->filesel != NULL) {
		gtk_widget_show (prefs_widget->filesel);
		gdk_window_raise (prefs_widget->filesel->window);
		return;
	}
	
	prefs_widget->filesel =
		gtk_file_selection_new (_("Wallpaper Selection"));
	filesel = GTK_FILE_SELECTION (prefs_widget->filesel);
	hbox = filesel->file_list;

	do {
		hbox = hbox->parent;
		if(!hbox) {
			g_warning(_("Can't find an hbox, using a normal file "
				    "selection"));
			goto signal_setup;
		}
	} while (!GTK_IS_HBOX (hbox));

	widg = gtk_frame_new (_("Preview"));
	gtk_widget_show (widg);
	gtk_box_pack_end (GTK_BOX (hbox), widg, FALSE, FALSE, 0);
	gtk_widget_set_usize (widg, 110, 110);

	gtk_object_set_data (GTK_OBJECT (widg), "fs", filesel);
	gtk_object_set_data (GTK_OBJECT (filesel->file_list), "frame", widg);
	gtk_object_set_data (GTK_OBJECT (filesel->selection_entry),
			     "frame", widg);
			     
	gtk_signal_connect (GTK_OBJECT (filesel->file_list),"select_row",
			    GTK_SIGNAL_FUNC (setup_preview), prefs_widget);

	gtk_signal_connect (GTK_OBJECT (filesel->selection_entry), "changed",
			    GTK_SIGNAL_FUNC (setup_preview), prefs_widget);
			    

 signal_setup:

	if (prefs_widget->prefs->wallpaper_filename != NULL)
		gtk_file_selection_set_filename
			(filesel, prefs_widget->prefs->wallpaper_filename);

	gtk_signal_connect (GTK_OBJECT (filesel), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &prefs_widget->filesel);
	gtk_signal_connect (GTK_OBJECT (filesel->ok_button), "clicked",
			    GTK_SIGNAL_FUNC (wp_selection_ok_cb), prefs_widget);
	
	gtk_signal_connect (GTK_OBJECT (filesel->cancel_button), "clicked",
			    GTK_SIGNAL_FUNC (wp_selection_cancel_cb),
			    prefs_widget);

	gtk_widget_show (prefs_widget->filesel);
}

static void
wp_selection_ok_cb (GtkButton *button, PrefsWidget *prefs_widget) 
{
	gchar *filename;

	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->filesel != NULL);
	g_return_if_fail (GTK_IS_FILE_SELECTION (prefs_widget->filesel));

	/* FIXME: This should really be an interface in Preferences */
	if (prefs_widget->prefs->wallpaper_filename != NULL)
		g_free (prefs_widget->prefs->wallpaper_filename);

	filename = gtk_file_selection_get_filename
		(GTK_FILE_SELECTION (prefs_widget->filesel));

	if (g_file_test (filename, G_FILE_TEST_ISFILE)) {
		prefs_widget->prefs->wallpaper_filename = g_strdup (filename);

		preferences_changed (prefs_widget->prefs);
		capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget),
					      TRUE);
	}

	gtk_widget_hide (prefs_widget->filesel);
}

static void
wp_selection_cancel_cb (GtkButton *button, PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (GTK_IS_FILE_SELECTION (prefs_widget->filesel));

	gtk_widget_hide (prefs_widget->filesel);
}

static void
auto_apply_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	
	if (gtk_toggle_button_get_active (tb))
		prefs_widget->prefs->auto_apply = TRUE;
	else
		prefs_widget->prefs->auto_apply = FALSE;
	
	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
adjust_opacity_toggled_cb (GtkToggleButton *tb, PrefsWidget *prefs_widget)
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));
	
	prefs_widget->prefs->adjust_opacity = gtk_toggle_button_get_active (tb);
	gtk_widget_set_sensitive
		(WID ("opacity_box"),
		 prefs_widget->prefs->enabled &&
		 prefs_widget->prefs->adjust_opacity);
	
	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}

static void
opacity_adjust_changed_cb (GtkAdjustment *adjustment,
			   PrefsWidget *prefs_widget) 
{
	g_return_if_fail (prefs_widget != NULL);
	g_return_if_fail (IS_PREFS_WIDGET (prefs_widget));
	g_return_if_fail (prefs_widget->prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs_widget->prefs));

	prefs_widget->prefs->opacity = adjustment->value;

	preferences_changed (prefs_widget->prefs);
	capplet_widget_state_changed (CAPPLET_WIDGET (prefs_widget), TRUE);
}
