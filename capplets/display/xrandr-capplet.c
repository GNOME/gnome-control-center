/* Monitor Settings. A preference panel for configuring monitors
 *
 * Copyright (C) 2007, 2008  Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#include <config.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <string.h>
#include <stdlib.h>
#include "scrollarea.h"
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-rr.h>
#include <libgnomeui/gnome-rr-config.h>
#include <libgnomeui/gnome-rr-labeler.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>

typedef struct App App;
typedef struct GrabInfo GrabInfo;

struct App
{
    GnomeRRScreen       *screen;
    GnomeRRConfig  *current_configuration;
    GnomeRRLabeler *labeler;
    GnomeOutputInfo         *current_output;

    GtkWidget	   *dialog;
    GtkListStore   *resolution_store;
    GtkWidget	   *resolution_combo;
    GtkWidget	   *refresh_combo;
    GtkWidget	   *rotation_combo;
    GtkWidget	   *panel_checkbox;
    GtkWidget	   *panel_label;
    GtkWidget	   *clone_checkbox;
    GtkWidget	   *show_icon_checkbox;

    GtkWidget      *area;
    gboolean	    ignore_gui_changes;
    GConfClient	   *client;
};

static void rebuild_gui (App *app);
static void on_rate_changed (GtkComboBox *box, gpointer data);

#if 0
static void
show_error (const GError *err)
{
    if (!err)
	return;

    GtkWidget *dialog = gtk_message_dialog_new (
	NULL,
	GTK_DIALOG_DESTROY_WITH_PARENT,
	GTK_MESSAGE_WARNING,
	GTK_BUTTONS_OK, err->message);

    gtk_window_set_title (GTK_WINDOW (dialog), "");

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}
#endif

static gboolean
do_free (gpointer data)
{
    g_free (data);
    return FALSE;
}

static gchar *
idle_free (gchar *s)
{
    g_idle_add (do_free, s);

    return s;
}

static int
compare_outputs (const void *p1, const void *p2)
{
    GnomeOutputInfo *const *o1 = p1;
    GnomeOutputInfo *const *o2 = p2;

    return (**o1).x - (**o2).x;
}

static void
on_screen_changed (GnomeRRScreen *scr,
		   gpointer data)
{
    GnomeRRConfig *current;
    App *app = data;
    int i;
    GnomeOutputInfo *best;

    current = gnome_rr_config_new_current (app->screen);

    if (app->current_configuration)
	gnome_rr_config_free (app->current_configuration);

    app->current_configuration = current;

#if 0
    for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
    {
	GnomeOutputInfo *o = app->current_configuration->outputs[i];

	g_debug ("  output %s %s: %d %d %d %d", o->name, o->on? "on" : "off", o->x, o->y, o->width, o->height);
    }
#endif

#if 0
    g_debug ("sorting");
#endif
    /* Sort outputs according to X coordinate */
    for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
	;

    qsort (app->current_configuration->outputs, i, sizeof (GnomeOutputInfo *),
	   compare_outputs);

    if (app->labeler) {
	gnome_rr_labeler_hide (app->labeler);
	g_object_unref (app->labeler);
    }

    app->labeler = gnome_rr_labeler_new (app->current_configuration);

#if 0
    for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
    {
	GnomeOutputInfo *o = app->current_configuration->outputs[i];

	g_debug ("  output: %d %d %d %d", o->x, o->y, o->width, o->height);
    }
#endif

    /* Select an output */
    best = NULL;
    for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
    {
	GnomeOutputInfo *output = app->current_configuration->outputs[i];

	if (output->connected)
	{
	    char *cur_name =
		app->current_output? app->current_output->name : NULL;

	    if ((cur_name && strcmp (output->name, cur_name) == 0) || !best)
		best = output;
	}
    }

    app->current_output = best;

    rebuild_gui (app);
}

static void
on_viewport_changed (FooScrollArea *scroll_area,
		     GdkRectangle  *old_viewport,
		     GdkRectangle  *new_viewport)
{
    foo_scroll_area_set_size (scroll_area,
			      new_viewport->width,
			      new_viewport->height);

    foo_scroll_area_invalidate (scroll_area);
}

static void
layout_set_font (PangoLayout *layout, const char *font)
{
    PangoFontDescription *desc =
	pango_font_description_from_string (font);

    if (desc)
    {
	pango_layout_set_font_description (layout, desc);

	pango_font_description_free (desc);
    }
}

static void
clear_combo (GtkWidget *widget)
{
    GtkComboBox *box = GTK_COMBO_BOX (widget);
    GtkTreeModel *model = gtk_combo_box_get_model (box);
    GtkListStore *store = GTK_LIST_STORE (model);

    gtk_list_store_clear (store);
}

typedef struct
{
    const char *text;
    gboolean found;
    GtkTreeIter iter;
} ForeachInfo;

static gboolean
foreach (GtkTreeModel *model,
	 GtkTreePath *path,
	 GtkTreeIter *iter,
	 gpointer data)
{
    ForeachInfo *info = data;
    char *text = NULL;

    gtk_tree_model_get (model, iter, 0, &text, -1);

    g_assert (text != NULL);

    if (strcmp (info->text, text) == 0)
    {
	info->found = TRUE;
	info->iter = *iter;
	return TRUE;
    }

    return FALSE;
}

static void
add_key (GtkWidget *widget,
	 const char *text,
	 int width, int height, int rate,
	 GnomeRRRotation rotation)
{
    ForeachInfo info;
    GtkComboBox *box = GTK_COMBO_BOX (widget);
    GtkTreeModel *model = gtk_combo_box_get_model (box);
    GtkListStore *store = GTK_LIST_STORE (model);
    gboolean retval;

    info.text = text;
    info.found = FALSE;

    gtk_tree_model_foreach (model, foreach, &info);

    if (!info.found)
    {
	GtkTreeIter iter;
	gtk_list_store_append (store, &iter);

	gtk_list_store_set (store, &iter,
			    0, text,
			    1, width,
			    2, height,
			    3, rate,
			    4, width * height,
			    5, rotation,
			    -1);

	retval = TRUE;
    }
    else
    {
	retval = FALSE;
    }
}

static gboolean
combo_select (GtkWidget *widget, const char *text)
{
    GtkComboBox *box = GTK_COMBO_BOX (widget);
    GtkTreeModel *model = gtk_combo_box_get_model (box);
    ForeachInfo info;

    info.text = text;
    info.found = FALSE;

    gtk_tree_model_foreach (model, foreach, &info);

    if (!info.found)
	return FALSE;

    gtk_combo_box_set_active_iter (box, &info.iter);
    return TRUE;
}

static gboolean
has_similar_mode (GnomeRROutput *output, GnomeRRMode *mode)
{
    int i;
    GnomeRRMode **modes = gnome_rr_output_list_modes (output);
    int width = gnome_rr_mode_get_width (mode);
    int height = gnome_rr_mode_get_height (mode);

    for (i = 0; modes[i] != NULL; ++i)
    {
	GnomeRRMode *m = modes[i];

	if (gnome_rr_mode_get_width (m) == width	&&
	    gnome_rr_mode_get_height (m) == height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

static GnomeRRMode **
list_clone_modes (GnomeRRConfig *config, GnomeRRScreen *screen)
{
    int i;
    GPtrArray *result;
    GnomeRRMode **modes;

    for (i = 0; config->outputs[i] != NULL; ++i)
    {
	if (config->outputs[i]->connected)
	{
	    GnomeRROutput *output =
		gnome_rr_screen_get_output_by_name (screen, config->outputs[i]->name);

	    modes = gnome_rr_output_list_modes (output);
	}
    }

    if (!modes)
	return NULL;

    result = g_ptr_array_new ();

    for (i = 0; modes[i] != NULL; ++i)
    {
	gboolean valid = TRUE;
	int j;

	for (j = 0; config->outputs[j] != NULL; ++j)
	{
	    if (config->outputs[j]->connected)
	    {
		GnomeRROutput *output = gnome_rr_screen_get_output_by_name (
		    screen, config->outputs[j]->name);

		if (!has_similar_mode (output, modes[i]))
		{
		    valid = FALSE;
		    break;
		}
	    }
	}

	if (valid)
	    g_ptr_array_add (result, modes[i]);
    }

    g_ptr_array_add (result, NULL);

    return (GnomeRRMode **)g_ptr_array_free (result, FALSE);
}

static GnomeRRMode **
get_current_modes (App *app)
{
    GnomeRROutput *output;

    if (app->current_configuration->clone)
    {
	return list_clone_modes (app->current_configuration, app->screen);
    }
    else
    {
	if (!app->current_output)
	    return NULL;

	output = gnome_rr_screen_get_output_by_name (
	    app->screen, app->current_output->name);

	if (!output)
	    return NULL;

	return gnome_rr_output_list_modes (output);
    }
}

static void
rebuild_rotation_combo (App *app)
{
    typedef struct
    {
	GnomeRRRotation	rotation;
	const char *	name;
    } RotationInfo;
    static const RotationInfo rotations[] = {
	{ GNOME_RR_ROTATION_0, N_("Normal") },
	{ GNOME_RR_ROTATION_90, N_("Left") },
	{ GNOME_RR_ROTATION_270, N_("Right") },
	{ GNOME_RR_ROTATION_180, N_("Upside Down") },
    };
    const char *selection;
    GnomeRRRotation current;
    int i;

    clear_combo (app->rotation_combo);

    gtk_widget_set_sensitive (
	app->rotation_combo, app->current_output && app->current_output->on);

    if (!app->current_output)
	return;

    current = app->current_output->rotation;

    selection = NULL;
    for (i = 0; i < G_N_ELEMENTS (rotations); ++i)
    {
	const RotationInfo *info = &(rotations[i]);

	app->current_output->rotation = info->rotation;

	if (gnome_rr_config_applicable (app->current_configuration, app->screen))
	{
 	    add_key (app->rotation_combo, info->name, 0, 0, 0, info->rotation);

	    if (info->rotation == current)
		selection = info->name;
	}
    }

    app->current_output->rotation = current;

    if (!(selection && combo_select (app->rotation_combo, selection)))
	combo_select (app->rotation_combo, N_("Normal"));
}

#define idle_free_printf(x) idle_free (g_strdup_printf (x))

static void
rebuild_rate_combo (App *app)
{
    GHashTable *rates;
    GnomeRRMode **modes;
    int best;
    int i;

    clear_combo (app->refresh_combo);

    gtk_widget_set_sensitive (
	app->refresh_combo, app->current_output && app->current_output->on);

    if (!(modes = get_current_modes (app)))
	return;

    rates = g_hash_table_new_full (
	g_str_hash, g_str_equal, (GFreeFunc)g_free, NULL);

    best = -1;
    for (i = 0; modes[i] != NULL; ++i)
    {
	GnomeRRMode *mode = modes[i];
	int width, height, rate;

	width = gnome_rr_mode_get_width (mode);
	height = gnome_rr_mode_get_height (mode);
	rate = gnome_rr_mode_get_freq (mode);

	if (width == app->current_output->width		&&
	    height == app->current_output->height)
	{
	    add_key (app->refresh_combo,
		     idle_free (g_strdup_printf (_("%d Hz"), rate)),
		     0, 0, rate, -1);

	    if (rate > best)
		best = rate;
	}
    }

    if (!combo_select (app->refresh_combo, idle_free (g_strdup_printf (_("%d Hz"), app->current_output->rate))))
	combo_select (app->refresh_combo, idle_free (g_strdup_printf (_("%d Hz"), best)));
}

static int
count_active_outputs (App *app)
{
    int i, count = 0;

    for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
    {
	GnomeOutputInfo *output = app->current_configuration->outputs[i];
	if (output->on)
	    count++;
    }

    return count;
}

static int
count_all_outputs (GnomeRRConfig *config)
{
    int i;

    for (i = 0; config->outputs[i] != NULL; i++)
	;

    return i;
}

static void
rebuild_resolution_combo (App *app)
{
    int i;
    GnomeRRMode **modes;
    int best_w, best_h;
    const char *current;

    clear_combo (app->resolution_combo);

    if (!(modes = get_current_modes (app)))
	return;

    best_w = 0;
    best_h = 0;
    for (i = 0; modes[i] != NULL; ++i)
    {
	int width, height;

	width = gnome_rr_mode_get_width (modes[i]);
	height = gnome_rr_mode_get_height (modes[i]);

	add_key (app->resolution_combo,
		 idle_free (g_strdup_printf (_("%d x %d"), width, height)),
		 width, height, 0, -1);

	if (width * height > best_w * best_h)
	{
	    best_w = width;
	    best_h = height;
	}
    }

    if (count_active_outputs (app) > 1 || !app->current_output->on)
	add_key (app->resolution_combo, _("Off"), 0, 0, 0, 0);

    if (!app->current_output->on)
    {
	current = "Off";
    }
    else
    {
	current = idle_free (g_strdup_printf (_("%d x %d"),
					      app->current_output->width,
					      app->current_output->height));
    }


    if (!combo_select (app->resolution_combo, current))
    {
	combo_select (app->resolution_combo,
		      idle_free (
			  g_strdup_printf (_("%d x %d"), best_w, best_h)));
    }
}

static void
rebuild_gui (App *app)
{
    gboolean sensitive;

    /* We would break spectacularly if we recursed, so
     * just assert if that happens
     */
    g_assert (app->ignore_gui_changes == FALSE);

    app->ignore_gui_changes = TRUE;

    sensitive = app->current_output? TRUE : FALSE;

#if 0
    g_debug ("rebuild gui, is on: %d", app->current_output->on);
#endif

    rebuild_resolution_combo (app);
    rebuild_rate_combo (app);
    rebuild_rotation_combo (app);

    gtk_widget_set_sensitive (app->resolution_combo, sensitive);

#if 0
    g_debug ("sensitive: %d, on: %d", sensitive, app->current_output->on);
#endif
    gtk_widget_set_sensitive (app->panel_checkbox, sensitive);

    app->ignore_gui_changes = FALSE;

    if (app->current_configuration && app->current_configuration->clone)
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->clone_checkbox), TRUE);
    else
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->clone_checkbox), FALSE);
}

static gboolean
get_mode (GtkWidget *widget, int *width, int *height, int *freq, GnomeRRRotation *rot)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkComboBox *box = GTK_COMBO_BOX (widget);
    int dummy;

    if (!gtk_combo_box_get_active_iter (box, &iter))
	return FALSE;

    if (!width)
	width = &dummy;

    if (!height)
	height = &dummy;

    if (!freq)
	freq = &dummy;

    if (!rot)
	rot = (GnomeRRRotation *)&dummy;

    model = gtk_combo_box_get_model (box);
    gtk_tree_model_get (model, &iter,
			1, width,
			2, height,
			3, freq,
			5, rot,
			-1);

    return TRUE;

}

static void
on_rotation_changed (GtkComboBox *box, gpointer data)
{
    App *app = data;
    GnomeRRRotation rotation;

    if (!app->current_output)
	return;

    if (get_mode (app->rotation_combo, NULL, NULL, NULL, &rotation))
	app->current_output->rotation = rotation;

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
on_rate_changed (GtkComboBox *box, gpointer data)
{
    App *app = data;
    int rate;

    if (!app->current_output)
	return;

    if (get_mode (app->refresh_combo, NULL, NULL, &rate, NULL))
	app->current_output->rate = rate;

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
on_resolution_changed (GtkComboBox *box, gpointer data)
{
    App *app = data;
    int width;
    int height;

    if (!app->current_output)
	return;

    if (get_mode (app->resolution_combo, &width, &height, NULL, NULL))
    {
	app->current_output->width = width;
	app->current_output->height = height;

	if (width == 0 || height == 0)
	    app->current_output->on = FALSE;
	else
	    app->current_output->on = TRUE;
    }

#if 0
    if (app->current_configuration)
    {
	x = 0;
	for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
	{
	    GnomeOutputInfo *output = app->current_configuration->outputs[i];

	    if (output->connected)
	    {
		output->x = x;

		x += output->width;
	    }
	}
    }
#endif

    rebuild_rate_combo (app);
    rebuild_rotation_combo (app);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));
}

static void
on_clone_changed (GtkWidget *box, gpointer data)
{
    App *app = data;

    app->current_configuration->clone =
	gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->clone_checkbox));

    if (app->current_configuration->clone)
    {
	int i;

	for (i = 0; app->current_configuration->outputs[i]; ++i)
	{
	    if (app->current_configuration->outputs[i]->connected)
	    {
		app->current_output = app->current_configuration->outputs[i];
		break;
	    }
	}
    }

    rebuild_gui (app);
}

static void
get_geometry (GnomeOutputInfo *output, int *w, int *h)
{
    if (output->on)
    {
	*h = output->height;
	*w = output->width;
    }
    else
    {
	*h = output->pref_height;
	*w = output->pref_width;
    }
}

#define SPACE 15
#define MARGIN  15

static GList *
list_connected_outputs (App *app, int *total_w, int *total_h)
{
    int i, dummy;
    GList *result = NULL;

    if (!total_w)
	total_w = &dummy;
    if (!total_h)
	total_h = &dummy;

    *total_w = 0;
    *total_h = 0;
    for (i = 0; app->current_configuration->outputs[i] != NULL; ++i)
    {
	GnomeOutputInfo *output = app->current_configuration->outputs[i];

	if (output->connected)
	{
	    int w, h;

	    result = g_list_prepend (result, output);

	    get_geometry (output, &w, &h);

	    *total_w += w;
	    *total_h += h;
	}
    }

    return g_list_reverse (result);
}

static int
get_n_connected (App *app)
{
    GList *connected_outputs = list_connected_outputs (app, NULL, NULL);
    int n = g_list_length (connected_outputs);

    g_list_free (connected_outputs);

    return n;
}

static double
compute_scale (App *app)
{
    int available_w, available_h;
    int total_w, total_h;
    int n_monitors;
    GdkRectangle viewport;
    GList *connected_outputs;

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (app->area), &viewport);

    connected_outputs = list_connected_outputs (app, &total_w, &total_h);

    n_monitors = g_list_length (connected_outputs);

    g_list_free (connected_outputs);

    available_w = viewport.width - 2 * MARGIN - (n_monitors - 1) * SPACE;
    available_h = viewport.height - 2 * MARGIN - (n_monitors - 1) * SPACE;

    return MIN ((double)available_w / total_w, (double)available_h / total_h);
}

typedef struct Edge
{
    GnomeOutputInfo *output;
    int x1, y1;
    int x2, y2;
} Edge;

typedef struct Snap
{
    Edge *snapper;		/* Edge that should be snapped */
    Edge *snappee;
    int dy, dx;
} Snap;

static void
add_edge (GnomeOutputInfo *output, int x1, int y1, int x2, int y2, GArray *edges)
{
    Edge e;

    e.x1 = x1;
    e.x2 = x2;
    e.y1 = y1;
    e.y2 = y2;
    e.output = output;

    g_array_append_val (edges, e);
}

static void
list_edges_for_output (GnomeOutputInfo *output, GArray *edges)
{
    int x, y, w, h;

    x = output->x;
    y = output->y;
    get_geometry (output, &w, &h);

    /* Top, Bottom, Left, Right */
    add_edge (output, x, y, x + w, y, edges);
    add_edge (output, x, y + h, x + w, y + h, edges);
    add_edge (output, x, y, x, y + h, edges);
    add_edge (output, x + w, y, x + w, y + h, edges);
}

static void
list_edges (GnomeRRConfig *config, GArray *edges)
{
    int i;

    for (i = 0; config->outputs[i]; ++i)
    {
	GnomeOutputInfo *output = config->outputs[i];

	if (output->connected)
	    list_edges_for_output (output, edges);
    }
}

static gboolean
overlap (int s1, int e1, int s2, int e2)
{
    return (!(e1 < s2 || s1 >= e2));
}

static gboolean
horizontal_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->y1 != snapper->y2 || snappee->y1 != snappee->y2)
	return FALSE;

    return overlap (snapper->x1, snapper->x2, snappee->x1, snappee->x2);
}

static gboolean
vertical_overlap (Edge *snapper, Edge *snappee)
{
    if (snapper->x1 != snapper->x2 || snappee->x1 != snappee->x2)
	return FALSE;

    return overlap (snapper->y1, snapper->y2, snappee->y1, snappee->y2);
}

static void
add_snap (GArray *snaps, Snap snap)
{
    if (ABS (snap.dx) <= 200 || ABS (snap.dy) <= 200)
	g_array_append_val (snaps, snap);
}

static void
add_edge_snaps (Edge *snapper, Edge *snappee, GArray *snaps)
{
    Snap snap;

    snap.snapper = snapper;
    snap.snappee = snappee;

    if (horizontal_overlap (snapper, snappee))
    {
	snap.dx = 0;
	snap.dy = snappee->y1 - snapper->y1;

	add_snap (snaps, snap);
    }
    else if (vertical_overlap (snapper, snappee))
    {
	snap.dy = 0;
	snap.dx = snappee->x1 - snapper->x1;

	add_snap (snaps, snap);
    }

    /* Corner snaps */
    /* 1->1 */
    snap.dx = snappee->x1 - snapper->x1;
    snap.dy = snappee->y1 - snapper->y1;

    add_snap (snaps, snap);

    /* 1->2 */
    snap.dx = snappee->x2 - snapper->x1;
    snap.dy = snappee->y2 - snapper->y1;

    add_snap (snaps, snap);

    /* 2->2 */
    snap.dx = snappee->x2 - snapper->x2;
    snap.dy = snappee->y2 - snapper->y2;

    add_snap (snaps, snap);

    /* 2->1 */
    snap.dx = snappee->x1 - snapper->x2;
    snap.dy = snappee->y1 - snapper->y2;

    add_snap (snaps, snap);
}

static void
list_snaps (GnomeOutputInfo *output, GArray *edges, GArray *snaps)
{
    int i;

    for (i = 0; i < edges->len; ++i)
    {
	Edge *output_edge = &(g_array_index (edges, Edge, i));

	if (output_edge->output == output)
	{
	    int j;

	    for (j = 0; j < edges->len; ++j)
	    {
		Edge *edge = &(g_array_index (edges, Edge, j));

		if (edge->output != output)
		    add_edge_snaps (output_edge, edge, snaps);
	    }
	}
    }
}

#if 0
static void
print_edge (Edge *edge)
{
    g_debug ("(%d %d %d %d)", edge->x1, edge->y1, edge->x2, edge->y2);
}
#endif

static gboolean
corner_on_edge (int x, int y, Edge *e)
{
    if (x == e->x1 && x == e->x2 && y >= e->y1 && y <= e->y2)
	return TRUE;

    if (y == e->y1 && y == e->y2 && x >= e->x1 && x <= e->x2)
	return TRUE;

    return FALSE;
}

static gboolean
edges_align (Edge *e1, Edge *e2)
{
    if (corner_on_edge (e1->x1, e1->y1, e2))
	return TRUE;

    if (corner_on_edge (e2->x1, e2->y1, e1))
	return TRUE;

    return FALSE;
}

static gboolean
output_is_aligned (GnomeOutputInfo *output, GArray *edges)
{
    gboolean result = FALSE;
    int i;

    for (i = 0; i < edges->len; ++i)
    {
	Edge *output_edge = &(g_array_index (edges, Edge, i));

	if (output_edge->output == output)
	{
	    int j;

	    for (j = 0; j < edges->len; ++j)
	    {
		Edge *edge = &(g_array_index (edges, Edge, j));

		/* We are aligned if an output edge matches
		 * an edge of another output
		 */
		if (edge->output != output_edge->output)
		{
		    if (edges_align (output_edge, edge))
		    {
			result = TRUE;
			goto done;
		    }
		}
	    }
	}
    }
done:

    return result;
}

static void
get_output_rect (GnomeOutputInfo *output, GdkRectangle *rect)
{
    int w, h;

    get_geometry (output, &w, &h);

    rect->width = w;
    rect->height = h;
    rect->x = output->x;
    rect->y = output->y;
}

static gboolean
output_overlaps (GnomeOutputInfo *output, GnomeRRConfig *config)
{
    int i;
    GdkRectangle output_rect;

    get_output_rect (output, &output_rect);

    for (i = 0; config->outputs[i]; ++i)
    {
	GnomeOutputInfo *other = config->outputs[i];

	if (other != output && other->connected)
	{
	    GdkRectangle other_rect;

	    get_output_rect (other, &other_rect);
	    if (gdk_rectangle_intersect (&output_rect, &other_rect, NULL))
		return TRUE;
	}
    }

    return FALSE;
}

static gboolean
gnome_rr_config_is_aligned (GnomeRRConfig *config, GArray *edges)
{
    int i;
    gboolean result = TRUE;

    for (i = 0; config->outputs[i]; ++i)
    {
	GnomeOutputInfo *output = config->outputs[i];

	if (output->connected)
	{
	    if (!output_is_aligned (output, edges))
		return FALSE;

	    if (output_overlaps (output, config))
		return FALSE;
	}
    }

    return result;
}

struct GrabInfo
{
    int grab_x;
    int grab_y;
    int output_x;
    int output_y;
};

static gboolean
is_corner_snap (const Snap *s)
{
    return s->dx != 0 && s->dy != 0;
}

static int
compare_snaps (gconstpointer v1, gconstpointer v2)
{
    const Snap *s1 = v1;
    const Snap *s2 = v2;
    int sv1 = MAX (ABS (s1->dx), ABS (s1->dy));
    int sv2 = MAX (ABS (s2->dx), ABS (s2->dy));
    int d;

    d = sv1 - sv2;

    /* This snapping algorithm is good enough for rock'n'roll, but
     * this is probably a better:
     *
     *    First do a horizontal/vertical snap, then
     *    with the new coordinates from that snap,
     *    do a corner snap.
     *
     * Right now, it's confusing that corner snapping
     * depends on the distance in an axis that you can't actually see.
     *
     */
    if (d == 0)
    {
	if (is_corner_snap (s1) && !is_corner_snap (s2))
	    return -1;
	else if (is_corner_snap (s2) && !is_corner_snap (s1))
	    return 1;
	else
	    return 0;
    }
    else
    {
	return d;
    }
}

static void
on_output_event (FooScrollArea *area,
		 FooScrollAreaEvent *event,
		 gpointer data)
{
    GnomeOutputInfo *output = data;
    App *app = g_object_get_data (G_OBJECT (area), "app");

    if (event->type == FOO_BUTTON_PRESS)
    {
	GrabInfo *info;

	app->current_output = output;

	rebuild_gui (app);

	if (!app->current_configuration->clone && get_n_connected (app) > 1)
	{
	    foo_scroll_area_begin_grab (area, on_output_event, data);

	    info = g_new0 (GrabInfo, 1);
	    info->grab_x = event->x;
	    info->grab_y = event->y;
	    info->output_x = output->x;
	    info->output_y = output->y;

	    output->user_data = info;
	}

	foo_scroll_area_invalidate (area);
    }
    else
    {
	if (foo_scroll_area_is_grabbed (area))
	{
	    GrabInfo *info = output->user_data;
	    double scale = compute_scale (app);
	    int old_x, old_y;
	    int new_x, new_y;
	    int i;
	    GArray *edges, *snaps, *new_edges;

	    old_x = output->x;
	    old_y = output->y;
	    new_x = info->output_x + (event->x - info->grab_x) / scale;
	    new_y = info->output_y + (event->y - info->grab_y) / scale;

	    output->x = new_x;
	    output->y = new_y;

	    edges = g_array_new (TRUE, TRUE, sizeof (Edge));
	    snaps = g_array_new (TRUE, TRUE, sizeof (Snap));
	    new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

	    list_edges (app->current_configuration, edges);
	    list_snaps (output, edges, snaps);

	    g_array_sort (snaps, compare_snaps);

	    output->x = info->output_x;
	    output->y = info->output_y;

	    for (i = 0; i < snaps->len; ++i)
	    {
		Snap *snap = &(g_array_index (snaps, Snap, i));
		GArray *new_edges = g_array_new (TRUE, TRUE, sizeof (Edge));

		output->x = new_x + snap->dx;
		output->y = new_y + snap->dy;

		g_array_set_size (new_edges, 0);
		list_edges (app->current_configuration, new_edges);

		if (gnome_rr_config_is_aligned (app->current_configuration, new_edges))
		{
		    g_array_free (new_edges, TRUE);
		    break;
		}
		else
		{
		    output->x = info->output_x;
		    output->y = info->output_y;
		}
	    }

	    g_array_free (new_edges, TRUE);
	    g_array_free (snaps, TRUE);
	    g_array_free (edges, TRUE);

	    if (event->type == FOO_BUTTON_RELEASE)
	    {
		foo_scroll_area_end_grab (area);

		g_free (output->user_data);
		output->user_data = NULL;

#if 0
		g_debug ("new position: %d %d %d %d", output->x, output->y, output->width, output->height);
#endif
	    }

	    foo_scroll_area_invalidate (area);
	}
    }
}

#if 0
static void
on_canvas_event (FooScrollArea *area,
		 FooScrollAreaEvent *event,
		 gpointer data)
{
    App *app = g_object_get_data (G_OBJECT (area), "app");

    if (event->type == FOO_BUTTON_PRESS)
    {
	app->current_output = NULL;

	rebuild_gui (app);

	foo_scroll_area_invalidate (area);
    }
}
#endif

static PangoLayout *
get_display_name (App *app,
		  GnomeOutputInfo *output)
{
    const char *text;

    if (app->current_configuration->clone)
	text = _("Mirror Screens");
    else
	text = output->display_name;

    return gtk_widget_create_pango_layout (
	GTK_WIDGET (app->area), text);
}

#define BACKGROUND_FILL_RGBA	0.72, 0.78, 0.87, 1.0
#define BACKGROUND_STROKE_RGBA	0.44, 0.59, 0.76, 1.0

static void
paint_background (FooScrollArea *area,
		  cairo_t       *cr)
{
    GdkRectangle viewport;

    foo_scroll_area_get_viewport (area, &viewport);

    cairo_set_source_rgba (cr, BACKGROUND_FILL_RGBA);

    cairo_rectangle (cr,
		     viewport.x, viewport.y,
		     viewport.width, viewport.height);

    cairo_fill_preserve (cr);

#if 0
    foo_scroll_area_add_input_from_fill (area, cr, on_canvas_event, NULL);
#endif

    cairo_set_source_rgba (cr, BACKGROUND_STROKE_RGBA);

    cairo_stroke (cr);
}

static void
paint_output (App *app, cairo_t *cr, int i)
{
    int w, h;
    double scale = compute_scale (app);
    double x, y;
    int total_w, total_h;
    GList *connected_outputs = list_connected_outputs (app, &total_w, &total_h);
    GnomeOutputInfo *output = g_list_nth (connected_outputs, i)->data;
    PangoLayout *layout = get_display_name (app, output);
    PangoRectangle extent;
    GdkRectangle viewport;
    double angle;
    GdkColor output_color;
    double r, g, b;

    cairo_save (cr);

    foo_scroll_area_get_viewport (FOO_SCROLL_AREA (app->area), &viewport);

    get_geometry (output, &w, &h);

#if 0
    g_debug ("%s (%p) geometry %d %d %d", output->name, output,
	     w, h, output->rate);
#endif

    viewport.height -= 2 * MARGIN;
    viewport.width -= 2 * MARGIN;

    x = output->x * scale + MARGIN + (viewport.width - total_w * scale) / 2.0;
    y = output->y * scale + MARGIN + (viewport.height - total_h * scale) / 2.0;

#if 0
    g_debug ("scaled: %f %f", x, y);

    g_debug ("scale: %f", scale);

    g_debug ("%f %f %f %f", x, y, w * scale + 0.5, h * scale + 0.5);
#endif

    cairo_save (cr);

    cairo_translate (cr,
		     x + (w * scale + 0.5) / 2,
		     y + (h * scale + 0.5) / 2);

    if (output->rotation & GNOME_RR_ROTATION_0)
    {
	angle = 0;
    }
    else if (output->rotation & GNOME_RR_ROTATION_90)
    {
	angle = G_PI / 2;
    }
    else if (output->rotation & GNOME_RR_ROTATION_180)
    {
	angle = G_PI;
    }
    else if (output->rotation & GNOME_RR_ROTATION_270)
    {
	angle = 1.5 * G_PI;
    }
    else
    {
	angle = 0;
    }

    cairo_rotate (cr, angle);

    if (output->rotation & GNOME_RR_REFLECT_X)
	cairo_scale (cr, -1, 1);

    if (output->rotation & GNOME_RR_REFLECT_Y)
	cairo_scale (cr, 1, -1);

    cairo_translate (cr,
		     - x - (w * scale + 0.5) / 2,
		     - y - (h * scale + 0.5) / 2);


    cairo_rectangle (cr, x, y, w * scale + 0.5, h * scale + 0.5);
    cairo_clip_preserve (cr);

    gnome_rr_labeler_get_color_for_output (app->labeler, output, &output_color);
    r = output_color.red / 65535.0;
    g = output_color.green / 65535.0;
    b = output_color.blue / 65535.0;

    if (!output->on)
    {
	/* If the output is turned off, just darken the selected color */
	r *= 0.2;
	g *= 0.2;
	b *= 0.2;
    }

    cairo_set_source_rgba (cr, r, g, b, 1.0);

    foo_scroll_area_add_input_from_fill (FOO_SCROLL_AREA (app->area),
					 cr, on_output_event, output);
    cairo_fill (cr);

    if (output == app->current_output)
    {
	cairo_rectangle (cr, x + 2, y + 2, w * scale + 0.5 - 4, h * scale + 0.5 - 4);

	cairo_set_line_width (cr, 4);
	cairo_set_source_rgba (cr, 0.33, 0.43, 0.57, 1.0);
	cairo_stroke (cr);
    }

    cairo_rectangle (cr, x + 0.5, y + 0.5, w * scale + 0.5 - 1, h * scale + 0.5 - 1);

    cairo_set_line_width (cr, 1);
    cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

    cairo_stroke (cr);
    cairo_set_line_width (cr, 2);

    layout_set_font (layout, "Sans Bold 12");

    pango_layout_get_pixel_extents (layout, NULL, &extent);

    extent.x = x + ((w * scale + 0.5) - extent.width) / 2;
    extent.y = y + ((h * scale + 0.5) - extent.height) / 2;

    cairo_move_to (cr, extent.x, extent.y);

    if (output->on)
	cairo_set_source_rgb (cr, 0.2, 0.2, 0.8);
    else
	cairo_set_source_rgb (cr, 1.0, 1.0, 1.0);

    pango_cairo_show_layout (cr, layout);

    cairo_restore (cr);

    g_object_unref (layout);
}

static void
on_area_paint (FooScrollArea *area,
	       cairo_t	     *cr,
	       GdkRectangle  *extent,
	       GdkRegion     *region,
	       gpointer	      data)
{
    App *app = data;
    double scale;
    GList *connected_outputs = NULL;
    GList *list;

    paint_background (area, cr);

    if (!app->current_configuration)
	return;

    scale = compute_scale (app);
    connected_outputs = list_connected_outputs (app, NULL, NULL);

#if 0
    g_debug ("scale: %f", scale);
#endif

    for (list = connected_outputs; list != NULL; list = list->next)
    {
	paint_output (app, cr, g_list_position (connected_outputs, list));

	if (app->current_configuration->clone)
	    break;
    }
}

static void
make_text_combo (GtkWidget *widget, int sort_column)
{
    GtkComboBox *box = GTK_COMBO_BOX (widget);
    GtkListStore *store = gtk_list_store_new (
	6,
	G_TYPE_STRING,		/* Text */
	G_TYPE_INT,		/* Width */
	G_TYPE_INT,		/* Height */
	G_TYPE_INT,		/* Frequency */
	G_TYPE_INT,		/* Width * Height */
	G_TYPE_INT);		/* Rotation */

    GtkCellRenderer *cell;

    gtk_cell_layout_clear (GTK_CELL_LAYOUT (widget));

    gtk_combo_box_set_model (box, GTK_TREE_MODEL (store));

    cell = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (box), cell, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (box), cell,
				    "text", 0,
				    NULL);

    if (sort_column != -1)
    {
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      sort_column,
					      GTK_SORT_DESCENDING);
    }
}

static Atom
gnome_randr_atom (void)
{
    static Atom atom = None;

    if (!atom)
    {
	atom = XInternAtom (gdk_x11_get_default_xdisplay(),
			    "_GNOME_RANDR_ATOM", FALSE);
    }

    return atom;
}

static void
compute_virtual_size_for_configuration (GnomeRRConfig *config, int *ret_width, int *ret_height)
{
    int i;
    int width, height;

    width = height = 0;

    for (i = 0; config->outputs[i] != NULL; i++)
    {
	GnomeOutputInfo *output;

	output = config->outputs[i];

	if (output->on)
	{
	    width = MAX (width, output->x + output->width);
	    height = MAX (height, output->y + output->height);
	}
    }

    *ret_width = width;
    *ret_height = height;
}

static void
check_required_virtual_size (App *app)
{
    int req_width, req_height;
    int min_width, max_width;
    int min_height, max_height;

    compute_virtual_size_for_configuration (app->current_configuration, &req_width, &req_height);

    gnome_rr_screen_get_ranges (app->screen, &min_width, &max_width, &min_height, &max_height);

#if 0
    g_debug ("X Server supports:");
    g_debug ("min_width = %d, max_width = %d", min_width, max_width);
    g_debug ("min_height = %d, max_height = %d", min_height, max_height);

    g_debug ("Requesting size of %dx%d", req_width, req_height);
#endif

    if (!(min_width <= req_width && req_width <= max_width
	  && min_height <= req_height && req_height <= max_height))
    {
	/* FIXME: present a useful dialog, maybe even before the user tries to Apply */
#if 0
	g_debug ("Your X server needs a larger Virtual size!");
#endif
    }
}

static void
apply (App *app)
{
    GError *err = NULL;

    gnome_rr_config_sanitize (app->current_configuration);

    check_required_virtual_size (app);

    foo_scroll_area_invalidate (FOO_SCROLL_AREA (app->area));

    if (gnome_rr_config_save (app->current_configuration, &err))
    {
	XEvent message;

	message.xclient.type = ClientMessage;
	message.xclient.message_type = gnome_randr_atom();
	message.xclient.format = 8;

#if 0
	g_debug ("Sending client message");
#endif

	XSendEvent (gdk_x11_get_default_xdisplay(),
		    gdk_x11_get_default_root_xwindow(),
		    FALSE,
		    StructureNotifyMask, &message);
    }
}

/* Returns whether the graphics driver doesn't advertise RANDR 1.2 features, and just 1.0 */
static gboolean
driver_is_randr_10 (GnomeRRConfig *config)
{
    /* In the Xorg code, see xserver/randr/rrinfo.c:RRScanOldConfig().  It gets
     * called when the graphics driver doesn't support RANDR 1.2 yet, just 1.0.
     * In that case, the X server's base code (which supports RANDR 1.2) will
     * simulate having a single output called "default".  For drivers that *do*
     * support RANDR 1.2, the separate outputs will be named differently, we
     * hope.
     *
     * This heuristic is courtesy of Dirk Mueller <dmueller@suse.de>
     *
     * FIXME: however, we don't even check for XRRQueryVersion() returning 1.2, neither
     * here nor in gnome-desktop/libgnomedesktop*.c.  Do we need to check for that,
     * or is gnome_rr_screen_new()'s return value sufficient?
     */

    return (count_all_outputs (config) == 1 && strcmp (config->outputs[0]->name, "default") == 0);
}

static void
on_detect_displays (GtkWidget *widget, gpointer data)
{
    App *app = data;

    gnome_rr_screen_refresh (app->screen);
}

#define SHOW_ICON_KEY "/apps/gnome_settings_daemon/xrandr/show_notification_icon"


static void
on_show_icon_toggled (GtkWidget *widget, gpointer data)
{
    GtkToggleButton *tb = GTK_TOGGLE_BUTTON (widget);
    App *app = data;

    gconf_client_set_bool (app->client, SHOW_ICON_KEY,
			   gtk_toggle_button_get_active (tb), NULL);
}

static void
run_application (App *app)
{
#ifndef GLADEDIR
#define GLADEDIR "."
#endif
#define GLADE_FILE GLADEDIR "/display-capplet.glade"
    GladeXML *xml;
    GtkWidget *align;

    xml = glade_xml_new (GLADE_FILE, NULL, NULL);
    if (!xml)
    {
	g_warning ("Could not open " GLADE_FILE);
	return;
    }

    app->screen = gnome_rr_screen_new (gdk_screen_get_default (),
				       on_screen_changed, app);
    if (!app->screen)
    {
	g_error ("Could not get screen info");
	g_object_unref (xml);
	return;
    }

    app->client = gconf_client_get_default ();

    app->dialog = glade_xml_get_widget (xml, "dialog");

    gtk_window_set_default_icon_name ("gnome-display-properties");
    gtk_window_set_icon_name (GTK_WINDOW (app->dialog),
			      "gnome-display-properties");

    app->resolution_combo = glade_xml_get_widget (xml, "resolution_combo");
    g_signal_connect (app->resolution_combo, "changed",
		      G_CALLBACK (on_resolution_changed), app);

    app->refresh_combo = glade_xml_get_widget (xml, "refresh_combo");
    g_signal_connect (app->refresh_combo, "changed",
		      G_CALLBACK (on_rate_changed), app);

    app->rotation_combo = glade_xml_get_widget (xml, "rotation_combo");
    g_signal_connect (app->rotation_combo, "changed",
		      G_CALLBACK (on_rotation_changed), app);

    app->clone_checkbox = glade_xml_get_widget (xml, "clone_checkbox");
    g_signal_connect (app->clone_checkbox, "toggled",
		      G_CALLBACK (on_clone_changed), app);

    g_signal_connect (glade_xml_get_widget (xml, "detect_displays_button"),
		      "clicked", G_CALLBACK (on_detect_displays), app);

    app->show_icon_checkbox = glade_xml_get_widget (xml, "show_notification_icon");

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->show_icon_checkbox),
				  gconf_client_get_bool (app->client, SHOW_ICON_KEY, NULL));

    g_signal_connect (app->show_icon_checkbox, "toggled", G_CALLBACK (on_show_icon_toggled), app);

    app->panel_checkbox = glade_xml_get_widget (xml, "panel_checkbox");
    app->panel_label = glade_xml_get_widget (xml, "panel_label");

    make_text_combo (app->resolution_combo, 4);
    make_text_combo (app->refresh_combo, 3);
    make_text_combo (app->rotation_combo, -1);

    g_assert (app->panel_checkbox);

    /* Scroll Area */
    app->area = (GtkWidget *)foo_scroll_area_new ();

    g_object_set_data (G_OBJECT (app->area), "app", app);

    /* FIXME: this should be computed dynamically */
    foo_scroll_area_set_min_size (FOO_SCROLL_AREA (app->area), -1, 200);
    gtk_widget_show (app->area);
    g_signal_connect (app->area, "paint",
		      G_CALLBACK (on_area_paint), app);
    g_signal_connect (app->area, "viewport_changed",
		      G_CALLBACK (on_viewport_changed), app);

    align = glade_xml_get_widget (xml, "align");

    gtk_container_add (GTK_CONTAINER (align), app->area);

    on_screen_changed (app->screen, app);
    rebuild_gui (app);

    gtk_widget_hide (app->panel_checkbox);
    gtk_widget_hide (app->panel_label);
    g_object_unref (xml);

restart:
    switch (gtk_dialog_run (GTK_DIALOG (app->dialog)))
    {
    default:
	/* Fall Through */
    case GTK_RESPONSE_DELETE_EVENT:
    case GTK_RESPONSE_CLOSE:
#if 0
	g_debug ("Close");
#endif
	break;

    case GTK_RESPONSE_HELP:
#if 0
	g_debug ("Help");
#endif
	goto restart;
	break;

    case GTK_RESPONSE_APPLY:
	apply (app);
	goto restart;
	break;
    }

    gtk_widget_destroy (app->dialog);
    gnome_rr_screen_destroy (app->screen);
    g_object_unref (app->client);
}

int
main (int argc, char **argv)
{
    App *app;

    bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);

    gtk_init (&argc, &argv);

    app = g_new0 (App, 1);

    run_application (app);

    g_free (app);

    return 0;
}
