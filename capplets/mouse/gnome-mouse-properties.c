/* -*- mode: c; style: linux -*- */

/* mouse-properties-capplet.c
 * Copyright (C) 2001 Red Hat, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by: Jonathon Blandford <jrb@redhat.com>,
 *             Bradford Hovinen <hovinen@ximian.com>,
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

#include <config.h>

#include <string.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <gdk/gdkx.h>
#include <math.h>

#include "capplet-util.h"
#include "gconf-property-editor.h"
#include "activate-settings-daemon.h"
#include "capplet-stock-icons.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef HAVE_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

/******************************************************************************/
/* A quick custom widget to ensure that the left handed toggle works no matter
 * which button is pressed.
 */
typedef struct { GtkCheckButton parent; } MouseCappletCheckButton;
typedef struct { GtkCheckButtonClass parent; } MouseCappletCheckButtonClass;
GNOME_CLASS_BOILERPLATE (MouseCappletCheckButton, mouse_capplet_check_button,
			 GtkCheckButton, GTK_TYPE_CHECK_BUTTON)
static void mouse_capplet_check_button_instance_init (MouseCappletCheckButton *obj) { }

static gboolean
mouse_capplet_check_button_button_press (GtkWidget *widget, GdkEventButton *event)
{
	if (event->type == GDK_BUTTON_PRESS) {
		if (!GTK_WIDGET_HAS_FOCUS (widget))
			gtk_widget_grab_focus (widget);
		gtk_button_pressed (GTK_BUTTON (widget));
	}
	return TRUE;
}
static gboolean
mouse_capplet_check_button_button_release (GtkWidget *widget, GdkEventButton *event)
{
      gtk_button_released (GTK_BUTTON (widget));
      return TRUE;
}

static void
mouse_capplet_check_button_class_init (MouseCappletCheckButtonClass *klass)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *)klass;
	widget_class->button_press_event   = mouse_capplet_check_button_button_press;
	widget_class->button_release_event = mouse_capplet_check_button_button_release;
}
/******************************************************************************/

enum
{
	DOUBLE_CLICK_TEST_OFF,
	DOUBLE_CLICK_TEST_MAYBE,
	DOUBLE_CLICK_TEST_ON
};

enum
{
	COLUMN_PIXBUF,
	COLUMN_TEXT,
	COLUMN_FONT_PATH,
	COLUMN_SIZE,
	N_COLUMNS
};

/* We use this in at least half a dozen places, so it makes sense just to
 * define the macro */

#define DOUBLE_CLICK_KEY "/desktop/gnome/peripherals/mouse/double_click"
#define CURSOR_FONT_KEY  "/desktop/gnome/peripherals/mouse/cursor_font"
#define CURSOR_SIZE_KEY  "/desktop/gnome/peripherals/mouse/cursor_size"
#define CURSOR_THEME_KEY "/desktop/gnome/peripherals/mouse/cursor_theme"

/* State in testing the double-click speed. Global for a great deal of
 * convenience
 */
gint double_click_state = DOUBLE_CLICK_TEST_OFF;

/* normalilzation routines */
/* All of our scales but double_click are on the range 1->10 as a result, we
 * have a few routines to convert from whatever the gconf key is to our range.
 */
static GConfValue *
double_click_from_gconf (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_INT);
	gconf_value_set_int (new_value, CLAMP ((int) floor ((gconf_value_get_int (value) + 50) / 100.0) * 100, 100, 1000));
	return new_value;
}

static gboolean
delay_value_changed_cb (GtkWidget *range, GtkScrollType scroll, gdouble value,
			gpointer   dialog)
{
	gchar *message = g_strdup_printf ("%.1f %s", value / 1000.0, _("seconds"));
	gtk_label_set_label ((GtkLabel*) WID ("delay_label"), message);
	g_free (message);

	return FALSE;
}

static void
get_default_mouse_info (int *default_numerator, int *default_denominator, int *default_threshold)
{
	int numerator, denominator;
	int threshold;
	int tmp_num, tmp_den, tmp_threshold;
	
	/* Query X for the default value */
	XGetPointerControl (GDK_DISPLAY (), &numerator, &denominator,
			    &threshold);
	XChangePointerControl (GDK_DISPLAY (), True, True, -1, -1, -1);
	XGetPointerControl (GDK_DISPLAY (), &tmp_num, &tmp_den, &tmp_threshold);
	XChangePointerControl (GDK_DISPLAY (), True, True, numerator, denominator, threshold);

	if (default_numerator)
		*default_numerator = tmp_num;

	if (default_denominator)
		*default_denominator = tmp_den;

	if (default_threshold)
		*default_threshold = tmp_threshold;

}

static GConfValue *
motion_acceleration_from_gconf (GConfPropertyEditor *peditor, 
				const GConfValue *value)
{
	GConfValue *new_value;
	gfloat motion_acceleration;

	new_value = gconf_value_new (GCONF_VALUE_FLOAT);
	
	if (gconf_value_get_float (value) == -1.0) {
		int numerator, denominator;
		
		get_default_mouse_info (&numerator, &denominator, NULL);

		motion_acceleration = CLAMP ((gfloat)(numerator / denominator), 0.2, 6.0);
	}
	else {
		motion_acceleration = CLAMP (gconf_value_get_float (value), 0.2, 6.0);
	}

	if (motion_acceleration >= 1)
		gconf_value_set_float (new_value, motion_acceleration + 4);
	else
		gconf_value_set_float (new_value, motion_acceleration * 5);

	return new_value;
}

static GConfValue *
motion_acceleration_to_gconf (GConfPropertyEditor *peditor, 
			      const GConfValue *value)
{
	GConfValue *new_value;
	gfloat motion_acceleration;

	new_value = gconf_value_new (GCONF_VALUE_FLOAT);
	motion_acceleration = CLAMP (gconf_value_get_float (value), 1.0, 10.0);

	if (motion_acceleration < 5)
		gconf_value_set_float (new_value, motion_acceleration / 5.0);
	else
		gconf_value_set_float (new_value, motion_acceleration - 4);

	return new_value;
}

static GConfValue *
threshold_from_gconf (GConfPropertyEditor *peditor, 
		      const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_FLOAT);

	if (gconf_value_get_int (value) == -1) {
		int threshold;

		get_default_mouse_info (NULL, NULL, &threshold);
		gconf_value_set_float (new_value, CLAMP (threshold, 1, 10));
	}
	else {
		gconf_value_set_float (new_value, CLAMP (gconf_value_get_int (value), 1, 10));
	}

	return new_value;
}

static GConfValue *
drag_threshold_from_gconf (GConfPropertyEditor *peditor, 
			   const GConfValue *value)
{
	GConfValue *new_value;

	new_value = gconf_value_new (GCONF_VALUE_FLOAT);

	gconf_value_set_float (new_value, CLAMP (gconf_value_get_int (value), 1, 10));

	return new_value;
}

static GConfValue *
cursor_size_to_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;
	gint widget_val;

	widget_val = gconf_value_get_int (value);

	new_value = gconf_value_new (GCONF_VALUE_INT);
	switch (widget_val) {
	case 12:
		gconf_value_set_int (new_value, 0);
		break;
	case 24:
		gconf_value_set_int (new_value, 1);
		break;
	case 36:
		gconf_value_set_int (new_value, 2);
		break;
	default:
		gconf_value_set_int (new_value, -1);
		break;
	}

	return new_value;
}

static GConfValue *
cursor_size_from_widget (GConfPropertyEditor *peditor, const GConfValue *value)
{
	GConfValue *new_value;
	gint radio_val;

	radio_val = gconf_value_get_int (value);

	new_value = gconf_value_new (GCONF_VALUE_INT);

	switch (radio_val) {
	case 0:
		gconf_value_set_int (new_value, 12);
		break;
	case 1:
		gconf_value_set_int (new_value, 24);
		break;
	case 2:
		gconf_value_set_int (new_value, 36);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	return new_value;
}

/* Retrieve legacy settings */

static void
get_legacy_settings (void) 
{
}

/* Double Click handling */

struct test_data_t 
{
	gint *timeout_id;
	GtkWidget *image;
};

/* Timeout for the double click test */

static gboolean
test_maybe_timeout (struct test_data_t *data)
{
	double_click_state = DOUBLE_CLICK_TEST_OFF;

	gtk_image_set_from_stock (GTK_IMAGE (data->image), 
				  MOUSE_DBLCLCK_OFF, mouse_capplet_dblclck_icon_get_size());

	*data->timeout_id = 0;

	return FALSE;
}

/* Callback issued when the user clicks the double click testing area. */

static gboolean
event_box_button_press_event (GtkWidget   *widget,
			      GdkEventButton *event,
			      GConfChangeSet *changeset)
{
	gint                       double_click_time;
	GConfValue                *value;
	static struct test_data_t  data;
	static gint                test_on_timeout_id     = 0;
	static gint                test_maybe_timeout_id  = 0;
	static guint32             double_click_timestamp = 0;
	GtkWidget                  *image;
	GConfClient               *client;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	image = g_object_get_data (G_OBJECT (widget), "image");
	
	if (!(changeset && gconf_change_set_check_value (changeset, DOUBLE_CLICK_KEY, &value))) {
		client = gconf_client_get_default();
		double_click_time = gconf_client_get_int (client, DOUBLE_CLICK_KEY, NULL);
		g_object_unref (client);

	} else
		double_click_time = gconf_value_get_int (value);

	if (test_maybe_timeout_id != 0)
		gtk_timeout_remove  (test_maybe_timeout_id);
	if (test_on_timeout_id != 0)
		gtk_timeout_remove (test_on_timeout_id);

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_OFF:
		double_click_state = DOUBLE_CLICK_TEST_MAYBE;
		data.image = image;
		data.timeout_id = &test_maybe_timeout_id;
		test_maybe_timeout_id = gtk_timeout_add (double_click_time, (GtkFunction) test_maybe_timeout, &data);
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		if (event->time - double_click_timestamp < double_click_time) {
			double_click_state = DOUBLE_CLICK_TEST_ON;
			data.image = image;
			data.timeout_id = &test_on_timeout_id;
			test_on_timeout_id = gtk_timeout_add (2500, (GtkFunction) test_maybe_timeout, &data);
		}
		break;
	case DOUBLE_CLICK_TEST_ON:
		double_click_state = DOUBLE_CLICK_TEST_OFF;
		break;
	}

	double_click_timestamp = event->time;

	switch (double_click_state) {
	case DOUBLE_CLICK_TEST_ON:
		gtk_image_set_from_stock (GTK_IMAGE (image), 
					  MOUSE_DBLCLCK_ON, mouse_capplet_dblclck_icon_get_size());
		break;
	case DOUBLE_CLICK_TEST_MAYBE:
		gtk_image_set_from_stock (GTK_IMAGE (image), 
					  MOUSE_DBLCLCK_MAYBE, mouse_capplet_dblclck_icon_get_size());
		break;
	case DOUBLE_CLICK_TEST_OFF:
		gtk_image_set_from_stock (GTK_IMAGE (image), 
					  MOUSE_DBLCLCK_OFF, mouse_capplet_dblclck_icon_get_size());
		break;
	}

	return TRUE;
}

/* Callback issued when the user switches between left- and right-handed button
 * mappings. Updates the appropriate image on the dialog.
 */

static void
left_handed_toggle_cb (GConfPropertyEditor *peditor, const gchar *key, const GConfValue *value, GtkWidget *image)
{
	if (value && gconf_value_get_bool (value)) {
		gtk_image_set_from_stock (GTK_IMAGE (image), MOUSE_LEFT_HANDED, mouse_capplet_icon_get_size ());
	}
	else {
		gtk_image_set_from_stock (GTK_IMAGE (image), MOUSE_RIGHT_HANDED, mouse_capplet_icon_get_size ());
	}
}

static gchar *
read_cursor_font (void)
{
	DIR *dir;
	gchar *dir_name;
	struct dirent *file_dirent;

	dir_name = g_build_path (G_DIR_SEPARATOR_S, g_get_home_dir (), ".gnome2/share/cursor-fonts", NULL);
	if (! g_file_test (dir_name, G_FILE_TEST_EXISTS))
		return NULL;

	dir = opendir (dir_name);
  
	while ((file_dirent = readdir (dir)) != NULL) {
		struct stat st;
		gchar *link_name;

		link_name = g_build_filename (dir_name, file_dirent->d_name, NULL);
		if (lstat (link_name, &st)) {
			g_free (link_name);
			continue;
		}
	  
		if (S_ISLNK (st.st_mode)) {
			gint length;
			gchar target[256];

			length = readlink (link_name, target, 255);
			if (length > 0) {
				gchar *retval;
				target[length] = '\0';
				retval = g_strdup (target);
				g_free (link_name);
				closedir (dir);
				return retval;
			}
			
		}
		g_free (link_name);
	}
	g_free (dir_name);
	closedir (dir);
	return NULL;
}

#ifdef HAVE_XCURSOR
static void
cursor_theme_changed (GConfClient *client,
		      guint        cnxn_id,
		      GConfEntry  *entry,
		      gpointer     user_data)
{
	GtkTreeIter       iter;
	GtkTreeView     * view      = GTK_TREE_VIEW(user_data);
	GtkTreeSelection* selection = gtk_tree_view_get_selection(view);
	GtkTreeModel    * smodel    = gtk_tree_view_get_model(view);
	GtkTreeModel    * model     = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT(smodel));
	gchar           * theme;
	gint              size      = gconf_client_get_int (client, CURSOR_SIZE_KEY, NULL);

	g_return_if_fail (gtk_tree_model_get_iter_first (model, &iter));
	theme = gconf_client_get_string (client, CURSOR_THEME_KEY, NULL);

	do {
		gchar* theme_name;
		gint cursor_size;

		gtk_tree_model_get(model, &iter,
				   COLUMN_FONT_PATH, &theme_name,
				   COLUMN_SIZE, &cursor_size,
				   -1);

		if(theme_name && theme && !strcmp(theme_name, theme) && cursor_size == size) {
			GtkTreeIter sort_iter;
			gtk_tree_model_sort_convert_child_iter_to_iter
				(GTK_TREE_MODEL_SORT(smodel), &sort_iter, &iter);
			gtk_tree_selection_select_iter(selection, &sort_iter);
			g_free(theme);
			g_free(theme_name);
			return;
		}
		g_free(theme_name);
	} while (gtk_tree_model_iter_next(model, &iter));

	/* we didn't find it */
	gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(view));
	g_free(theme);
}

#else /* !HAVE_XCURSOR */

static void
cursor_font_changed (GConfClient *client,
		     guint        cnxn_id,
		     GConfEntry  *entry,
		     gpointer     user_data)
{
	GtkTreeView *tree_view;
	gchar *cursor_font;
	gchar *cursor_text;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeModel *smodel;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (user_data);
	selection = gtk_tree_view_get_selection (tree_view);
	smodel = gtk_tree_view_get_model (tree_view);
	model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT(smodel));

	cursor_font = gconf_client_get_string (client, CURSOR_FONT_KEY, NULL);
	gtk_tree_model_get_iter_root (model, &iter);

	do {
		gchar *temp_cursor_font;
		GtkTreeIter sort_iter;
		gtk_tree_model_get (model, &iter,
				    COLUMN_FONT_PATH, &temp_cursor_font,
				    -1);
		gtk_tree_model_sort_convert_child_iter_to_iter
			(GTK_TREE_MODEL_SORT(smodel), &sort_iter, &iter);
		if ((temp_cursor_font == NULL && cursor_font == NULL) ||
		    ((temp_cursor_font != NULL && cursor_font != NULL) &&
		     (!strcmp (cursor_font, temp_cursor_font)))) {
			if (!gtk_tree_selection_iter_is_selected (selection, &sort_iter))
				gtk_tree_selection_select_iter (selection, &sort_iter);
			g_free (temp_cursor_font);
			g_free (cursor_font);
			return;
		}
		g_free (temp_cursor_font);
	} while (gtk_tree_model_iter_next (model, &iter));

	/* we didn't find it; we add it to the end. */
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	cursor_text = g_strdup_printf ("<b>%s</b>\n%s", _("Unknown Cursor"), cursor_font);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    COLUMN_TEXT, cursor_text,
			    COLUMN_FONT_PATH, cursor_font,
			    -1);
	gtk_tree_selection_select_iter (selection, &iter);

	g_free (cursor_font);
	g_free (cursor_text);
}
#endif /* !HAVE_XCURSOR */

static void
cursor_changed (GtkTreeSelection *selection,
		gpointer          data)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gchar *cursor_font = NULL;
	gint cursor_size;
	GConfClient *client = gconf_client_get_default ();

	if (! gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter,
			    COLUMN_FONT_PATH, &cursor_font,
			    COLUMN_SIZE, &cursor_size,
			    -1);
	if (cursor_font != NULL) {
#ifdef HAVE_XCURSOR
		gconf_client_set_string (client,
					 CURSOR_THEME_KEY, cursor_font, NULL);
		gconf_client_set_int (client,
				      CURSOR_SIZE_KEY, cursor_size, NULL);
#else
		gconf_client_set_string (client,
					 CURSOR_FONT_KEY, cursor_font, NULL);
#endif
		g_free (cursor_font);
	} else {
#ifdef HAVE_XCURSOR
		gconf_client_unset (client,
				    CURSOR_THEME_KEY, NULL);
		gconf_client_unset (client,
				    CURSOR_SIZE_KEY, NULL);
#else
		gconf_client_unset (client,
				    CURSOR_FONT_KEY, NULL);
#endif
	}
	g_object_unref (client);
}

#ifdef HAVE_XCURSOR
static GdkPixbuf*
gdk_pixbuf_from_xcursor_image(XcursorImage* cursor) {
	GdkPixbuf* pixbuf;
#define BUF_SIZE sizeof(guint32) * cursor->width * cursor->height
	guchar* buf = malloc(BUF_SIZE);
	guchar* it;
	memset(buf, '\0', BUF_SIZE);

	for(it = buf; it < (buf + BUF_SIZE); it += 4) {
		// can we get rid of this by using guint32 ?
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
		// on little endianess it's BGRA to RGBA
		it[0] = ((guchar*)(cursor->pixels))[it - buf + 2];
		it[1] = ((guchar*)(cursor->pixels))[it - buf + 1];
		it[2] = ((guchar*)(cursor->pixels))[it - buf + 0];
		it[3] = ((guchar*)(cursor->pixels))[it - buf + 3];
#else
		// on big endianess it's ARGB to RGBA
		it[0] = ((guchar*)cursor->pixels)[it - buf + 1];
		it[1] = ((guchar*)cursor->pixels)[it - buf + 2];
		it[2] = ((guchar*)cursor->pixels)[it - buf + 3];
		it[3] = ((guchar*)cursor->pixels)[it - buf + 0];
#endif
	}

	pixbuf = gdk_pixbuf_new_from_data((const guchar *)buf,
			GDK_COLORSPACE_RGB, TRUE, 8,
			cursor->width, cursor->height,
			cursor->width * 4,
			(GdkPixbufDestroyNotify)g_free,
			NULL);

	if(!pixbuf) {
		g_free(buf);
	}

	return pixbuf;
}
#endif

static void
populate_tree_model(GtkTreeModelSort* model, GtkTreeSelection* selection) {
#ifdef HAVE_XCURSOR
	// add the XCursor themes - paths taken from <xc/lib/Xcursor.library.c>
	const gchar* const path_defs[] = {
		"%s/.icons/",
		"/usr/share/icons/",
		"/usr/share/pixmaps/",
		"/usr/X11R6/lib/X11/icons/",
		NULL
	};

	gchar **iterator = NULL;
	gchar **paths = NULL;

	GConfClient* client = gconf_client_get_default();
	GtkListStore* store = GTK_LIST_STORE(gtk_tree_model_sort_get_model(model));
	gchar* current_theme = gconf_client_get_string(client, CURSOR_THEME_KEY, NULL);
	gint current_size = gconf_client_get_int(client, CURSOR_SIZE_KEY, NULL);
	g_object_unref(client);
	client = NULL;

	const gchar *xpaths = g_getenv ("XCURSOR_PATH");

	if (xpaths != NULL) {
		paths = g_strsplit (xpaths, ":", 0);
	} else {
		paths = g_strdupv ((gchar **) path_defs);
	}

	for(iterator = paths; *iterator; iterator++) {
		gchar*       fname = NULL;
		GDir*        folder = NULL;
		const gchar* name;
		
		if(strchr(*iterator, '%')) {
			fname = g_strdup_printf(*iterator, g_getenv("HOME"));
		} else {
			fname = g_strdup(*iterator);
		}

		folder = g_dir_open(fname, 0, NULL);
		
		while(folder && (name = g_dir_read_name(folder))) {
			gchar* cursor_dir = g_strdup_printf("%s/%s/cursors/", fname, name);
			XcursorImage* cursor;
			gint sizes[] = { 12, 16, 24, 32, 36, 48, 0 };
			gint i;
			GtkTreeIter iter;
			
			if(!g_file_test(cursor_dir, G_FILE_TEST_EXISTS)) {
				g_free(cursor_dir);
				continue;
			}
			
			for (i = 0; sizes[i] != 0; i++) {
				cursor = XcursorLibraryLoadImage("left_ptr", name, sizes[i]);
				if (cursor->size != sizes[i]) {
					XcursorImageDestroy (cursor);
					cursor = NULL;
				}
				
				if(cursor) {
					GdkPixbuf* pixbuf = gdk_pixbuf_from_xcursor_image(cursor);

					if(pixbuf) {
						gtk_list_store_append(store, &iter);
						gtk_list_store_set(store, &iter,
								   COLUMN_PIXBUF, pixbuf,
								   COLUMN_TEXT, name,
								   COLUMN_FONT_PATH, name,
								   COLUMN_SIZE, sizes[i],
								   -1);
						g_object_unref(pixbuf);
						
						if(current_theme != NULL && !strcmp(current_theme, name) &&
							current_size == sizes[i]) {
							GtkTreeIter sort_iter;
							gtk_tree_model_sort_convert_child_iter_to_iter(model,
												       &sort_iter,
												       &iter);
							gtk_tree_selection_select_iter(selection,
										       &sort_iter);
						}
					}
				}
			}
			
			g_free(cursor_dir);
		}

		if(folder) {
			g_dir_close(folder);
		}
		g_free(fname);
	}
	
	g_free(current_theme);

	if (xpaths != NULL) {
		g_strfreev (paths);
	}
#else /* !HAVE_XCURSOR */
	gchar* cursor_font;
	static const gchar* builtins[][5] = {
		{
			"gnome/cursor-fonts/cursor-normal.pcf",
		       	N_("Default Cursor"),
		       	N_("Default Cursor - Current"),
		       	N_("The default cursor that ships with X"),
			"mouse-cursor-normal.png"
		}, {
			"gnome/cursor-fonts/cursor-white.pcf",
		      	N_("White Cursor"),
		     	N_("White Cursor - Current"),
		     	N_("The default cursor inverted"),
			"mouse-cursor-white.png"
		}, {
			"gnome/cursor-fonts/cursor-large.pcf",
		      	N_("Large Cursor"),
		     	N_("Large Cursor - Current"),
		     	N_("Large version of normal cursor"),
			"mouse-cursor-normal-large.png"
		}, {
			"gnome/cursor-fonts/cursor-large-white.pcf",
			N_("Large White Cursor - Current"),
			N_("Large White Cursor"),
			N_("Large version of white cursor"),
			"mouse-cursor-white-large.png"
		}
	};
	GtkListStore* store = GTK_LIST_STORE(gtk_tree_model_sort_get_model(model));
	guint i;
	// add the default x font cursors
	cursor_font = read_cursor_font();

	for(i = 0; i < G_N_ELEMENTS(builtins); i++) {
		GtkTreeIter iter;
		gchar* cursor_string;
		gchar* filename;
		GdkPixbuf* pixbuf;

		filename = gnome_program_locate_file (gnome_program_get(), GNOME_FILE_DOMAIN_APP_PIXMAP, builtins[i][4], TRUE, NULL);
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);

		filename = gnome_program_locate_file (gnome_program_get(), GNOME_FILE_DOMAIN_DATADIR, builtins[i][0], FALSE, NULL);
		if(cursor_font && !strcmp(cursor_font, filename)) {
			cursor_string = g_strdup_printf("<b>%s</b>\n%s", ((const gchar*)builtins[i][2]), _((const gchar*)builtins[i][3]));
		} else {
			cursor_string = g_strdup_printf("<b>%s</b>\n%s", _((const gchar*)builtins[i][1]), _((const gchar*)builtins[i][3]));
		}
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COLUMN_PIXBUF, pixbuf,
				   COLUMN_TEXT, cursor_string,
				   COLUMN_FONT_PATH, filename,
				   -1);

		if(cursor_font && !strcmp(cursor_font, filename)) {
			GtkTreeIter sort_iter;
			gtk_tree_model_sort_convert_child_iter_to_iter(model,
						&sort_iter,
						&iter);
			gtk_tree_selection_select_iter(selection, &sort_iter);
		}

		g_free(cursor_string);
		g_free(filename);
	}

	if(0 == gtk_tree_selection_count_selected_rows(selection)) {
		GtkTreeIter child_iter,
			    sort_iter;
		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &child_iter, NULL, 0);
		gtk_tree_model_sort_convert_child_iter_to_iter(model,
					&sort_iter,
					&child_iter);
		gtk_tree_selection_select_iter(selection,
					       &sort_iter);
	}
	
	g_free(cursor_font);
#endif
}

/* Set up the property editors in the dialog. */
static void
setup_dialog (GladeXML *dialog, GConfChangeSet *changeset)
{
	GObject           *peditor;
	GtkWidget         *tree_view;
	GtkTreeSelection  *selection;
	GtkTreeModel      *model;
	GtkCellRenderer   *renderer;
	GtkTreeViewColumn *column;
	GConfValue        *value;
	GnomeProgram      *program;
	gchar             *cursor_font;
	gboolean           found_default;
	gchar             *message;

	GConfClient       *client = gconf_client_get_default ();

	program = gnome_program_get ();
	found_default = FALSE;
	
	/* Buttons page */
	/* Left-handed toggle */
	peditor = gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/mouse/left_handed", WID ("left_handed_toggle"), NULL);
	g_signal_connect (peditor, "value-changed", (GCallback) left_handed_toggle_cb, WID ("orientation_image"));

	/* Make sure the image gets initialized correctly */
	value = gconf_client_get (client, "/desktop/gnome/peripherals/mouse/left_handed", NULL);
	left_handed_toggle_cb (GCONF_PROPERTY_EDITOR (peditor), NULL, value, WID ("orientation_image"));
	gconf_value_free (value);

	/* Double-click time */
	peditor = gconf_peditor_new_numeric_range
		(changeset, DOUBLE_CLICK_KEY, WID ("delay_scale"),
		 "conv-to-widget-cb", double_click_from_gconf,
		 NULL);
	g_signal_connect (G_OBJECT (WID ("delay_scale")), "change_value", (GCallback) delay_value_changed_cb, dialog);
	gtk_widget_set_size_request (WID ("delay_scale"), 150, -1);
	gtk_image_set_from_stock (GTK_IMAGE (WID ("double_click_image")), MOUSE_DBLCLCK_OFF, mouse_capplet_dblclck_icon_get_size ());
	g_object_set_data (G_OBJECT (WID ("double_click_eventbox")), "image", WID ("double_click_image"));
	g_signal_connect (WID ("double_click_eventbox"), "button_press_event",
			  G_CALLBACK (event_box_button_press_event), changeset);
	
	/* set the timeout value  label with correct value of timeout */
	message = g_strdup_printf ("%.1f %s", gtk_range_get_value (GTK_RANGE (WID ("delay_scale"))) / 1000.0, _("seconds"));
	gtk_label_set_label ((GtkLabel*) WID ("delay_label"), message);
	g_free (message); 
	
	/* Cursors page */
	tree_view = WID ("cursor_tree");
	cursor_font = read_cursor_font ();
	
	model = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
	model = (GtkTreeModel *) gtk_tree_model_sort_new_with_model (model);
	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
				 model);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "pixbuf", COLUMN_PIXBUF,
					     NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_attributes (column, renderer,
					     "markup", COLUMN_TEXT,
					     NULL);

	/* Add the cursors */
	populate_tree_model(GTK_TREE_MODEL_SORT(model), selection);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (cursor_changed), NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
	
	gconf_peditor_new_boolean
		(changeset, "/desktop/gnome/peripherals/mouse/locate_pointer", WID ("locate_pointer_toggle"), NULL);

	peditor = gconf_peditor_new_select_menu
		(changeset, "/desktop/gnome/peripherals/mouse/cursor_size", WID ("cursor_size_omenu"),
		 "conv-to-widget-cb", cursor_size_to_widget,
		 "conv-from-widget-cb", cursor_size_from_widget,
		 NULL);

        /* Motion page */
	/* speed */
      	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/mouse/motion_acceleration", WID ("accel_scale"),
		 "conv-to-widget-cb", motion_acceleration_from_gconf,
		 "conv-from-widget-cb", motion_acceleration_to_gconf,
		 NULL);

	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/mouse/motion_threshold", WID ("sensitivity_scale"),
		 "conv-to-widget-cb", threshold_from_gconf,
		 NULL);

	/* DnD threshold */
	gconf_peditor_new_numeric_range
		(changeset, "/desktop/gnome/peripherals/mouse/drag_threshold", WID ("drag_threshold_scale"),
		 "conv-to-widget-cb", drag_threshold_from_gconf,
		 NULL);

	/* listen to cursors changing and set it up initially */
#ifdef HAVE_XCURSOR
	gconf_client_notify_add (client,
				 CURSOR_THEME_KEY,
				 cursor_theme_changed,
				 tree_view, NULL, NULL);
	cursor_theme_changed (client, 0, NULL, tree_view);
#else
        gconf_client_notify_add (client,
				 CURSOR_FONT_KEY,
				 cursor_font_changed,
				 tree_view, NULL, NULL);
	cursor_font_changed (client, 0, NULL, tree_view);
#endif
	g_object_unref (client);
}

/* Construct the dialog */

static GladeXML *
create_dialog (void) 
{
	GladeXML     *dialog;
	GtkSizeGroup *size_group;
	gchar        *text;

	/* register the custom type */
	(void) mouse_capplet_check_button_get_type ();

	dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-mouse-properties.glade", "mouse_properties_dialog", NULL);

	text = g_strdup_printf ("<b>%s</b>", _("Cursor Theme"));
	gtk_label_set_markup (GTK_LABEL (WID ("cursor_category_label")), text);
	g_free (text);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("acceleration_label"));
	gtk_size_group_add_widget (size_group, WID ("sensitivity_label"));
	gtk_size_group_add_widget (size_group, WID ("threshold_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("high_label"));
	gtk_size_group_add_widget (size_group, WID ("fast_label"));
	gtk_size_group_add_widget (size_group, WID ("large_label"));

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget (size_group, WID ("low_label"));
	gtk_size_group_add_widget (size_group, WID ("slow_label"));
	gtk_size_group_add_widget (size_group, WID ("small_label"));

	return dialog;
}

/* Callback issued when a button is clicked on the dialog */

static void
dialog_response_cb (GtkDialog *dialog, gint response_id, GConfChangeSet *changeset) 
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "user-guide.xml",
			      "goscustperiph-5");
	else
		gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	GConfClient    *client;
	GConfChangeSet *changeset;
	GladeXML       *dialog;
	GtkWidget      *dialog_win;

	static gboolean get_legacy;
	static struct poptOption cap_options[] = {
		{ "get-legacy", '\0', POPT_ARG_NONE, &get_legacy, 0,
		  N_("Retrieve and store legacy settings"), NULL },
		{ NULL, '\0', 0, NULL, 0, NULL, NULL }
	};

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("gnome-mouse-properties", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    NULL);

	capplet_init_stock_icons ();

	activate_settings_daemon ();

	client = gconf_client_get_default ();
	gconf_client_add_dir (client, "/desktop/gnome/peripherals/mouse", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
	g_object_unref (client);

	if (get_legacy) {
		get_legacy_settings ();
	} else {
		changeset = NULL;
		dialog = create_dialog ();
		setup_dialog (dialog, changeset);

		dialog_win = WID ("mouse_properties_dialog");
		g_signal_connect (dialog_win, "response",
				  G_CALLBACK (dialog_response_cb), changeset);

		capplet_set_icon (dialog_win, "gnome-dev-mouse-optical");
		gtk_widget_show (dialog_win);

		gtk_main ();
	}

	return 0;
}
