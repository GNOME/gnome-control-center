/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#include <config.h>

#include "control-center-categories.h"

#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <libnautilus/nautilus-view.h>
#include <gnome.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include <gconf/gconf-client.h>
#include "gnomecc-event-box.h"
#include "gnomecc-rounded-rect.h"

typedef struct _ControlCenter ControlCenter;
typedef void (*ControlCenterStatusCallback) (ControlCenter *cc, const gchar *status, void *data);

struct _ControlCenter {
	GtkWidget *widget; /* widget to embed. */

	GnomeCanvas *canvas;
	GnomeCanvasItem *under_cover;
	double height;
	double width;

	double min_height;
	double max_width;
	ControlCenterInformation *info;
	gboolean firstlayout;
	ControlCenterEntry *selected;
	int last_x;
	int line_count;

	ControlCenterStatusCallback status_cb;
	void *status_data;
	ControlCenterEntry *current_status;
};

typedef struct {
	ControlCenter *cc;
	GnomeCanvasGroup *group;
	GnomeCanvasItem *text;
	GnomeCanvasItem *pixbuf;
	GnomeCanvasItem *highlight_pixbuf;
	GnomeCanvasItem *cover;
	GnomeCanvasItem *selection;
	double height;
	double width;
	double icon_height;
	double icon_width;
	double text_height;
	double text_width;
	guint launching : 1;
	guint selected : 1;
	guint highlighted : 1;
	guint line_start : 1;
} EntryInfo;

typedef struct {
	GnomeCanvasGroup *group;
	GnomeCanvasItem *title;
	GnomeCanvasItem *line;

	int line_count;
} CategoryInfo;

#define PAD 5 /*when scrolling keep a few pixels above or below if possible */

static gboolean use_nautilus = FALSE;
static struct poptOption cap_options[] = {
	{"use-nautilus", '\0', POPT_ARG_NONE, &use_nautilus, 0,
	 N_("Use nautilus if it is running."), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static gboolean
single_click_activates (void)
{
	static gboolean needs_init = TRUE;
	static gboolean use_single_click = FALSE;
	if (needs_init)  {
		GConfClient *client = gconf_client_get_default ();
		char *policy = gconf_client_get_string (client, "/apps/nautilus/preferences/click_policy", NULL);
		g_object_unref (G_OBJECT (client));

		if (policy != NULL) {
			use_single_click = (0 == g_ascii_strcasecmp (policy, "single"));
			g_free (policy);
		}
		needs_init = FALSE;
	}

	return	use_single_click;
}

static guchar
lighten_component (guchar cur_value)
{
	int new_value = cur_value;
	new_value += 24 + (new_value >> 3);
	if (new_value > 255) {
		new_value = 255;
	}
	return (guchar) new_value;
}

static GdkPixbuf *
create_new_pixbuf (GdkPixbuf *src)
{
	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);

	return gdk_pixbuf_new (gdk_pixbuf_get_colorspace (src),
			       gdk_pixbuf_get_has_alpha (src),
			       gdk_pixbuf_get_bits_per_sample (src),
			       gdk_pixbuf_get_width (src),
			       gdk_pixbuf_get_height (src));
}

static GdkPixbuf *
create_spotlight_pixbuf (GdkPixbuf* src)
{
	GdkPixbuf *dest;
	int i, j;
	int width, height, has_alpha, src_row_stride, dst_row_stride;
	guchar *target_pixels, *original_pixels;
	guchar *pixsrc, *pixdest;

	g_return_val_if_fail (gdk_pixbuf_get_colorspace (src) == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail ((!gdk_pixbuf_get_has_alpha (src)
			       && gdk_pixbuf_get_n_channels (src) == 3)
			      || (gdk_pixbuf_get_has_alpha (src)
				  && gdk_pixbuf_get_n_channels (src) == 4), NULL);
	g_return_val_if_fail (gdk_pixbuf_get_bits_per_sample (src) == 8, NULL);

	dest = create_new_pixbuf (src);

	has_alpha = gdk_pixbuf_get_has_alpha (src);
	width = gdk_pixbuf_get_width (src);
	height = gdk_pixbuf_get_height (src);
	dst_row_stride = gdk_pixbuf_get_rowstride (dest);
	src_row_stride = gdk_pixbuf_get_rowstride (src);
	target_pixels = gdk_pixbuf_get_pixels (dest);
	original_pixels = gdk_pixbuf_get_pixels (src);

	for (i = 0; i < height; i++) {
		pixdest = target_pixels + i * dst_row_stride;
		pixsrc = original_pixels + i * src_row_stride;
		for (j = 0; j < width; j++) {
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			*pixdest++ = lighten_component (*pixsrc++);
			if (has_alpha) {
				*pixdest++ = *pixsrc++;
			}
		}
	}
	return dest;
}

static void
gnome_canvas_item_move_absolute (GnomeCanvasItem *item, double dx, double dy)
{
	double translate[6];

	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	art_affine_translate (translate, dx, dy);

	gnome_canvas_item_affine_absolute (item, translate);
}

#define ABOVE_LINE_SPACING 4
#define UNDER_LINE_SPACING 0
#define UNDER_TITLE_SPACING 0 /* manually insert 1 blank line of text */
#define LINE_HEIGHT 1
#define BETWEEN_CAT_SPACING 2
#define BORDERS 7
#define LINE_WITHIN FALSE
#define MAX_ITEM_WIDTH	135

static void
relayout_canvas (ControlCenter *cc)
{
	int count, i, j, line, line_i;
	int vert_pos, category_vert_pos, category_horiz_pos;
	gboolean keep_going;
	double max_width, height, max_text_height, max_icon_height;
	PangoRectangle rectangle;
	EntryInfo *ei;
	GArray *breaks = g_array_new (FALSE, FALSE, sizeof (int));

	i = 0;
	g_array_append_val (breaks, i);

	/* Do this in several iterations to keep things straight
	 * 0) walk down each column to decide when to wrap */
start_again :
	count = cc->info->count;
	cc->line_count = 0;
	keep_going = TRUE;
	for (i = 0 ; keep_going; i++) {
		keep_going = FALSE;

		max_width = 0.;
		/* 0.1) Find the maximum width for this column */
		for (line = 0 ; line < breaks->len ; ) {
			line_i = i + g_array_index (breaks, int, line);
			line++;
			if (line < breaks->len && line_i >= g_array_index (breaks, int, line))
				break;

			/* 0.2) check the nth row within a category */
			for (j = 0; j < count; j++) {
				ControlCenterCategory *cat = cc->info->categories[j];
				PangoLayout *layout;
				if (line_i >= cat->count)
					continue;
				ei = cat->entries[line_i]->user_data;
				if (ei == NULL)
					continue;
				keep_going = TRUE;

				/* Try it first with no wrapping */
			       	layout = GNOME_CANVAS_TEXT (ei->text)->layout,
				pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);
				pango_layout_set_width (layout, -1);
				pango_layout_get_pixel_extents (layout, NULL, &rectangle);

				/* If its too big wrap at the max and regen to find the layout */
				if (rectangle.width > MAX_ITEM_WIDTH) {
					pango_layout_set_width (layout, MAX_ITEM_WIDTH * PANGO_SCALE);
					pango_layout_get_pixel_extents (layout, NULL, &rectangle);
					rectangle.width = MAX_ITEM_WIDTH;
				}

				ei->text_height = rectangle.height;
				ei->text_width = rectangle.width;
				if (max_width < ei->text_width)
					max_width = ei->text_width;
				if (max_width < ei->icon_width)
					max_width = ei->icon_width;
			}
		}

		/* 0.3) Now go back and assign the max width */
		for (line = 0 ; line < breaks->len ; ) {
			line_i = i + g_array_index (breaks, int, line);
			line++;
			if (line < breaks->len && line_i >= g_array_index (breaks, int, line))
				break;

			for (j = 0; j < count; j++) {
				ControlCenterCategory *cat = cc->info->categories[j];
				if (line_i >= cat->count)
					continue;
				ei = cat->entries[line_i]->user_data;
				if (ei != NULL) {
					ei->width = max_width;
					pango_layout_set_width (GNOME_CANVAS_TEXT (ei->text)->layout,
						ei->width * PANGO_SCALE);
				}
			}
		}
	}

	/* 1) now walk each row looking for the max text and icon heights */
	vert_pos = BORDERS;
	category_vert_pos = 0;
	category_horiz_pos = BORDERS;
	for (i = 0; i < count; i++) {
		ControlCenterCategory *cat = cc->info->categories[i];
		CategoryInfo *catinfo = cat->user_data;
		if (catinfo == NULL)
			continue;

		/* 1.1) find the bounds */
		max_text_height = max_icon_height = 0.;
		for (j = 0; j < cat->count; j++) {
			ei = cat->entries[j]->user_data;
			if (ei == NULL)
				continue;
			if (ei->pixbuf != NULL && max_icon_height < ei->icon_height)
				max_icon_height = ei->icon_height;
			if (max_text_height < ei->text_height)
				max_text_height = ei->text_height;
		}

		/* 1.2) position things */
		gnome_canvas_item_move_absolute (GNOME_CANVAS_ITEM (catinfo->group),
						 0, vert_pos);

		category_vert_pos = 0;
		category_horiz_pos = BORDERS;
		if (!LINE_WITHIN && catinfo->line) {
			gnome_canvas_item_move_absolute (catinfo->line,
				BORDERS, category_vert_pos + ABOVE_LINE_SPACING);
			category_vert_pos = ABOVE_LINE_SPACING + LINE_HEIGHT + UNDER_LINE_SPACING;
		}

		if (catinfo->title) {
			double text_height;

			g_object_get (catinfo->title,
				      "text_height", &text_height,
				      NULL);

			category_vert_pos += text_height; /* move it down 1 line */
			gnome_canvas_item_move_absolute (catinfo->title, BORDERS, category_vert_pos);
			category_vert_pos += text_height + text_height/2 + UNDER_TITLE_SPACING;
		}

		if (LINE_WITHIN) {
			gnome_canvas_item_move_absolute (catinfo->line,
				BORDERS, category_vert_pos + ABOVE_LINE_SPACING);
			category_vert_pos = ABOVE_LINE_SPACING + LINE_HEIGHT + UNDER_LINE_SPACING;
		}

		category_vert_pos += UNDER_LINE_SPACING;

		catinfo->line_count = 1;
		cc->line_count ++;

		height = max_text_height + max_icon_height;
		for (j = 0; j < cat->count; j++) {
			ei = cat->entries[j]->user_data;
			ei->line_start = (j == 0);

			if (category_horiz_pos + ei->width > cc->max_width - BORDERS && j > 0) {
				category_horiz_pos = BORDERS;
				category_vert_pos += height;
				ei->line_start = TRUE;
				catinfo->line_count ++;
				cc->line_count ++;

				/* If this a new line break start again.
				 * The new layout will never be narrower, but
				 * the content of the extra line may expand */
				for (line = 0 ; line < breaks->len ; line++)
					if (g_array_index (breaks, int, line) == j)
						break;
				if (line >= breaks->len) {
					g_array_append_val (breaks, j);
					goto start_again;
				}
			}

			gnome_canvas_item_move_absolute (GNOME_CANVAS_ITEM (ei->group),
							 category_horiz_pos,
							 category_vert_pos);
			ei->height = height;
			gnome_canvas_item_set (ei->selection,
				"x2", (double) ei->width + 2 * PAD,
				"y2", (double) ei->text_height,
				NULL);
			gnome_canvas_item_set (ei->cover,
				"x2", (double) ei->width,
				"y2", (double) ei->height,
				NULL);
			/* canvas asks layout for its extent, layout gives real
			 * size, not fixed width and drawing gets confused.
			 */
			gnome_canvas_item_set (ei->text,
				"clip_width",  (double) ei->width,
				"clip_height", (double) ei->height,
				NULL);

			gnome_canvas_item_move_absolute (ei->selection, -PAD, max_icon_height);
			if (ei->text) /* text is centered by pango */
				gnome_canvas_item_move_absolute (ei->text,
					0, max_icon_height);

			if (ei->pixbuf) {
				/* manually cc the icon */
				gnome_canvas_item_move_absolute (ei->pixbuf,
					(ei->width - ei->icon_width) / 2, 0);
				gnome_canvas_item_move_absolute (ei->highlight_pixbuf,
					(ei->width - ei->icon_width) / 2, 0);
			}

			if (category_horiz_pos + ei->width + BORDERS > max_width)
				max_width = category_horiz_pos + ei->width + BORDERS;
			category_horiz_pos += ei->width + 16;
		}
		category_vert_pos += height;
		vert_pos += category_vert_pos;
		vert_pos += BETWEEN_CAT_SPACING;
	}

	cc->height = MAX (vert_pos, cc->min_height);
	cc->width = MAX (cc->max_width, max_width);

	for (i = 0; i < count; i++) {
		CategoryInfo *catinfo = cc->info->categories[i]->user_data;
		if (LINE_WITHIN || catinfo->line) {
			g_object_set (catinfo->line,
				      "x2", cc->width - 2 * BORDERS,
				      NULL);
		}
	}
	g_array_free (breaks, TRUE);
}

static gboolean
cb_entry_info_reset (gpointer data)
{
	EntryInfo *ei = data;
	ei->launching = FALSE;
	return FALSE;
}

static void
gnome_canvas_item_show_hide (GnomeCanvasItem *item, gboolean show)
{
	if (show)
		gnome_canvas_item_show (item);
	else
		gnome_canvas_item_hide (item);
}

static void
setup_entry (ControlCenterEntry *entry)
{
	if (entry) {
		EntryInfo *ei = entry->user_data;
		GtkWidget *widget = GTK_WIDGET (ei->cc->canvas);
		GtkStateType state;

		if (ei->pixbuf) {
			gnome_canvas_item_show_hide (ei->highlight_pixbuf, ei->highlighted);
			gnome_canvas_item_show_hide (ei->pixbuf, !ei->highlighted);
		}
		if (!ei->selected)
			state = GTK_STATE_NORMAL;
		else if (gtk_window_has_toplevel_focus (GTK_WINDOW (gtk_widget_get_toplevel (widget))))
			state = GTK_STATE_SELECTED;
		else
			state = GTK_STATE_ACTIVE;
		gnome_canvas_item_show_hide (ei->selection, ei->selected);
		g_object_set (ei->selection,
			"fill_color_gdk", &widget->style->base [state],
			NULL);
		g_object_set (ei->text,
			"fill_color_gdk", &widget->style->text [state],
			NULL);

		if (ei->cc->status_cb) {
			if (ei->selected) {
				ei->cc->status_cb (ei->cc, entry->comment,
						   ei->cc->status_data);
				ei->cc->current_status = entry;
			} else {
				if (entry == ei->cc->current_status)
					ei->cc->status_cb (ei->cc, NULL,
							   ei->cc->status_data);
				ei->cc->current_status = NULL;
			}
		}
	}
}

static int
get_x (ControlCenter *cc, ControlCenterEntry *entry)
{
	int i;
	int x;
	if (entry != NULL) {
		ControlCenterCategory *category = entry->category;
		for (i = 0, x = 0; i < category->count; i++, x++) {
			EntryInfo *ei = category->entries[i]->user_data;
			if (ei->line_start)
				x = 0;
			if (category->entries[i] == entry)
				return x;
		}
	}
	return -1;
}

static int
get_y (ControlCenter *cc, ControlCenterEntry *entry)
{
	int i;
	int line_count = 0;
	if (entry != NULL) {
		ControlCenterCategory *category = entry->category;
		for (i = 0; i < cc->info->count; i++) {
			CategoryInfo *catinfo = cc->info->categories[i]->user_data;

			if (cc->info->categories[i] == category) {
				for (i = 0; i < category->count; i++) {
					EntryInfo *ei = category->entries[i]->user_data;
					if (i > 0 && ei->line_start)
						line_count ++;
					if (category->entries[i] == entry)
						return line_count;
				}
				return -1;
			}

			line_count += catinfo->line_count;
		}
	}
	return -1;
}

static ControlCenterEntry *
get_entry (ControlCenter *cc, int x, int y)
{
	int i;
	for (i = 0; i < cc->info->count; i++) {
		CategoryInfo *catinfo = cc->info->categories[i]->user_data;
		if (y < catinfo->line_count) {
			int j;
			for (j = 0; j < cc->info->categories[i]->count; j++) {
				EntryInfo *ei = cc->info->categories[i]->entries[j]->user_data;
				if (ei->line_start) {
					if (y == 0) {
						g_assert (j + x < cc->info->categories[i]->count);
						return cc->info->categories[i]->entries[j + x];
					} else {
						y --;
					}
				}
			}
			g_assert_not_reached ();
		}
		y -= catinfo->line_count;
	}
	return NULL;
}

static int
get_line_length (ControlCenter *cc, int y)
{
	int i;
	for (i = 0; i < cc->info->count; i++) {
		CategoryInfo *catinfo = cc->info->categories[i]->user_data;
		if (y < catinfo->line_count) {
			int j;
			int last_start = 0;

			for (j = 1; j < cc->info->categories[i]->count; j++) {
				EntryInfo *ei = cc->info->categories[i]->entries[j]->user_data;
				if (ei->line_start) {
					if (y == 0) {
						return j - last_start;
					} else {
						y--;
						last_start = j;
					}
				}
			}
			return j - last_start;
		}

		y -= catinfo->line_count;
	}
	return -1;
}

static void
set_x (ControlCenter *cc)
{
	cc->last_x = get_x (cc, cc->selected);
}

static void
select_entry (ControlCenter *cc, ControlCenterEntry *entry)
{
	EntryInfo *ei = entry->user_data;
	GtkAdjustment *pos;
	double affine[6];
	if (cc->selected == entry)
		return;

	if (cc->selected && cc->selected->user_data)
		((EntryInfo *)cc->selected->user_data)->selected = FALSE;
	setup_entry (cc->selected);

	cc->selected = entry;

	if (cc->selected && cc->selected->user_data)
		((EntryInfo *)cc->selected->user_data)->selected = TRUE;
	setup_entry (cc->selected);

	gnome_canvas_item_i2c_affine (GNOME_CANVAS_ITEM (ei->group), affine);
	pos = gtk_layout_get_vadjustment (GTK_LAYOUT (ei->cover->canvas));

	if (affine[5] < pos->value)
		gtk_adjustment_set_value (pos, MAX (affine[5] - PAD, 0));
	else if ((affine[5] + ei->height) > (pos->value+pos->page_size))
		gtk_adjustment_set_value (pos, MAX (MIN (affine[5] + ei->height + PAD, pos->upper) - pos->page_size, 0));
}

static void
activate_entry (ControlCenterEntry *entry)
{
	EntryInfo *ei = entry->user_data;
	if (!ei->launching) {
		ei->launching = TRUE;
		gtk_timeout_add (1000, cb_entry_info_reset, ei);
		gnome_desktop_item_launch (entry->desktop_entry, NULL, 0, NULL);
	}
}

static gboolean
cover_event (GnomeCanvasItem *item, GdkEvent *event, ControlCenterEntry *entry)
{
	EntryInfo *ei = entry->user_data;
	ControlCenter *cc = ei->cc;
	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		ei->highlighted = TRUE;
		setup_entry (entry); /* highlight even if it is already selected */
		set_x (cc);
		return TRUE;
	case GDK_LEAVE_NOTIFY:
		ei->highlighted = FALSE;
		setup_entry (entry);
		return TRUE;
	case GDK_BUTTON_PRESS:
		if (single_click_activates ()) {
			activate_entry (entry);
		} else {
			select_entry (cc, entry);
			set_x (cc);
		}
		return TRUE;
	case GDK_2BUTTON_PRESS:
		activate_entry (entry);
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean
cb_canvas_event (GnomeCanvasItem *item, GdkEvent *event, ControlCenter *cc)
{
	int x, y;
	int do_set_x = FALSE;

	if (event->type == GDK_BUTTON_PRESS) {
		select_entry (cc, NULL);
		set_x (cc);
		return TRUE;
	}

	if (event->type != GDK_KEY_PRESS)
		return FALSE;

	switch (event->key.keyval) {
	case GDK_KP_Up:
	case GDK_Up:
		if (cc->selected) {
			y = get_y (cc, cc->selected);
			if (y > 0) {
				y--;
				x = cc->last_x;
				if (x == -1)
					x = get_x (cc, cc->selected);
				break;
			}
		} else {
			x = y = 0;
			break;
		}
		return FALSE;
	case GDK_KP_Down:
	case GDK_Down:
		if (cc->selected) {
			y = get_y (cc, cc->selected);
			if (y < cc->line_count - 1) {
				y++;
				x = cc->last_x;
				if (x == -1)
					x = get_x (cc, cc->selected);
				break;
			}
		} else {
			x = y = 0;
			break;
		}
		return FALSE;
	case GDK_KP_Right:
	case GDK_Right:
	case GDK_Tab:
	case GDK_KP_Tab:
		do_set_x = TRUE;
		if (cc->selected) {
			x = get_x (cc, cc->selected);
			y = get_y (cc, cc->selected);

			g_return_val_if_fail (x != -1 && y != -1, FALSE);

			x++;

			if (x >= get_line_length (cc, y)) {
				y++;
				x = 0;
			}
			if (y >= cc->line_count) {
				return FALSE;
			}
		} else {
			x = y = 0;
		}
		break;
	case GDK_KP_Left:
	case GDK_Left:
	case GDK_ISO_Left_Tab:
		do_set_x = TRUE;
		if (cc->selected) {
			x = get_x (cc, cc->selected);
			y = get_y (cc, cc->selected);

			g_return_val_if_fail (x != -1 && y != -1, FALSE);

			x--;

			if (x < 0) {
				if (y == 0)
					return FALSE;
				y--;
				x = get_line_length (cc, y) - 1;
			}
		} else {
			x = y = 0;
		}
		break;
	case GDK_Return:
	case GDK_KP_Enter:
		if (cc->selected) {
			activate_entry (cc->selected);
			set_x (cc);
			return TRUE;
		} else {
			return FALSE;
		}
	case GDK_Escape:
		gtk_main_quit ();
		return TRUE;

	case 'w':
	case 'q':
	case 'W':
	case 'Q':
		if (event->key.state == GDK_CONTROL_MASK) {
			gtk_main_quit ();
		}
		return TRUE;
	default:
		return FALSE;
	}
	if (y < 0)
		y = 0;
	if (y >= cc->line_count)
		y = cc->line_count - 1;
	if (y < 0)
		return FALSE;
	if (x < 0)
		x = 0;
	if (x >= get_line_length (cc, y))
		x = get_line_length (cc, y) - 1;
	select_entry (cc, get_entry (cc, x, y));
	if (do_set_x)
		set_x (cc);
	return TRUE;
}

static void
set_style (ControlCenter *cc, gboolean font_changed)
{
	int i, j;
	GtkWidget *widget = GTK_WIDGET (cc->canvas);

	if (!GTK_WIDGET_REALIZED (widget))
		return;

	for (i = 0; i < cc->info->count; i++) {
		CategoryInfo *catinfo = cc->info->categories[i]->user_data;

		if (LINE_WITHIN || catinfo->line) {
			g_object_set (catinfo->line,
				      "fill_color_gdk", &widget->style->text_aa[GTK_STATE_NORMAL],
				      NULL);
		}
		if (catinfo->title) {
			g_object_set (catinfo->title,
				      "fill_color_gdk", &widget->style->text[GTK_STATE_NORMAL],
				      NULL);

			if (font_changed)
				g_object_set (catinfo->title,
					      "font", NULL,
					      NULL);
		}

		for (j = 0; j < cc->info->categories[i]->count; j++) {
			ControlCenterEntry *entry = cc->info->categories[i]->entries[j];
			EntryInfo *entryinfo = entry->user_data;
			if (font_changed && entryinfo->text)
				g_object_set (entryinfo->text,
					      "font", NULL,
					      NULL);
			setup_entry (entry);
		}
	}
	if (font_changed)
		relayout_canvas (cc);
}

static void
canvas_realize (GtkWidget *canvas, ControlCenter *cc)
{
	set_style (cc, FALSE);
}

static void
canvas_style_set (GtkWidget *canvas, GtkStyle *previous_style, ControlCenter *cc)
{
	if (!GTK_WIDGET_REALIZED (canvas))
		return;

	set_style (cc, previous_style && canvas->style && !pango_font_description_equal (canvas->style->font_desc, previous_style->font_desc));
}

static void
rebuild_canvas (ControlCenter *cc, ControlCenterInformation *info)
{
	int count;
	int i;
	int j;
	int vert_pos = BORDERS;
#if 0
	int preferred_height;
	int preferred_width;
	int preferred_max_width;
#endif

	cc->info = info;
	cc->under_cover = gnome_canvas_item_new (gnome_canvas_root (cc->canvas),
						     gnomecc_event_box_get_type(),
						     NULL);

	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (gnome_canvas_root (cc->canvas)));
	g_signal_connect (gnome_canvas_root (cc->canvas),
		"event",
		G_CALLBACK (cb_canvas_event), cc);

	count = cc->info->count;

	cc->line_count = 0;
	for (i = 0; i < count; i++) {
		CategoryInfo *catinfo;

		if (cc->info->categories[i]->user_data == NULL)
			cc->info->categories[i]->user_data = g_new (CategoryInfo, 1);

		catinfo = cc->info->categories[i]->user_data;
		catinfo->group = NULL;
		catinfo->title = NULL;
		catinfo->line = NULL;

		catinfo->group =
			GNOME_CANVAS_GROUP (gnome_canvas_item_new (gnome_canvas_root (cc->canvas),
								   gnome_canvas_group_get_type (),
								   NULL));
		gnome_canvas_item_move_absolute (GNOME_CANVAS_ITEM (catinfo->group), 0, vert_pos);
		if (LINE_WITHIN || i > 0)
			catinfo->line = gnome_canvas_item_new (catinfo->group,
				gnome_canvas_rect_get_type (),
				"x2", (double) cc->max_width - 2 * BORDERS,
				"y2", (double) LINE_HEIGHT,
				NULL);

		catinfo->title = NULL;
		if (cc->info->categories[i] && (cc->info->count != 1 || cc->info->categories[0]->real_category)) {
			char *label = g_strdup_printf ("<span weight=\"bold\">%s</span>", cc->info->categories[i]->title);
			catinfo->title = gnome_canvas_item_new (catinfo->group,
				gnome_canvas_text_get_type (),
				"text", cc->info->categories[i]->title,
				"markup", label,
				"anchor", GTK_ANCHOR_NW,
				NULL);
			g_free (label);
		}

		catinfo->line_count = 1;
		cc->line_count ++;
		for (j = 0; j < cc->info->categories[i]->count; j++) {
			EntryInfo *ei;

			if (cc->info->categories[i]->entries[j]->user_data == NULL)
				cc->info->categories[i]->entries[j]->user_data = g_new0 (EntryInfo, 1);

			ei	   = cc->info->categories[i]->entries[j]->user_data;
			ei->cc = cc;
			ei->group = GNOME_CANVAS_GROUP (
				gnome_canvas_item_new (catinfo->group,
				gnome_canvas_group_get_type (),
				NULL));
			ei->selection = gnome_canvas_item_new (
				ei->group,
				GNOMECC_TYPE_ROUNDED_RECT,
				NULL);

			if (cc->info->categories[i]->entries[j]->title) {
				ei->text = gnome_canvas_item_new (ei->group,
					gnome_canvas_text_get_type (),
					"anchor", GTK_ANCHOR_NW,
					"justification", GTK_JUSTIFY_CENTER,
					"clip",	  TRUE,
					NULL);
				pango_layout_set_alignment (GNOME_CANVAS_TEXT (ei->text)->layout,
							    PANGO_ALIGN_CENTER);
				pango_layout_set_justify (GNOME_CANVAS_TEXT (ei->text)->layout,
							  FALSE);
				g_object_set (ei->text,
					      "text", cc->info->categories[i]->entries[j]->title,
					      NULL);
			} else
				ei->text = NULL;

			if (cc->info->categories[i]->entries[j]->icon_pixbuf) {
				GdkPixbuf *pixbuf = cc->info->categories[i]->entries[j]->icon_pixbuf;
				GdkPixbuf *highlight_pixbuf =
					create_spotlight_pixbuf (pixbuf);
				ei->icon_height = gdk_pixbuf_get_height (pixbuf);
				ei->icon_width  = gdk_pixbuf_get_width (pixbuf);
				ei->pixbuf = gnome_canvas_item_new (ei->group,
					gnome_canvas_pixbuf_get_type (),
					"pixbuf", pixbuf,
					NULL);
				g_object_unref (pixbuf);
				ei->highlight_pixbuf = gnome_canvas_item_new (ei->group,
					gnome_canvas_pixbuf_get_type (),
					"pixbuf", highlight_pixbuf,
					NULL);
				g_object_unref (highlight_pixbuf);
			} else {
				ei->pixbuf = NULL;
				ei->highlight_pixbuf = NULL;
			}

			ei->cover = gnome_canvas_item_new (ei->group,
							   gnomecc_event_box_get_type(),
							   NULL);
			g_signal_connect (ei->cover, "event",
				G_CALLBACK (cover_event),
				cc->info->categories[i]->entries[j]);

			setup_entry (cc->info->categories[i]->entries[j]);
		}
	}
}

static void
size_allocate(GtkWidget *widget, GtkAllocation *allocation, ControlCenter *cc)
{
	if (allocation->height == 1 || allocation->width == 1)
		return;

	cc->max_width = allocation->width;
	cc->min_height = allocation->height;
	if (cc->firstlayout) {
		rebuild_canvas (cc, cc->info);
		cc->firstlayout = FALSE;
	}

	relayout_canvas (cc);
	gnome_canvas_set_scroll_region (GNOME_CANVAS (cc->canvas), 0, 0, cc->width - 1, cc->height - 1);
	g_object_set (cc->under_cover,
		      "x2", cc->width,
		      "y2", cc->height,
		      NULL);
}

static gboolean
is_nautilus_running (void)
{
	Atom window_id_atom;
	Window nautilus_xid;
	Atom actual_type;
	int actual_format;
	unsigned long nitems, bytes_after;
	unsigned char *data;
	int retval;
	Atom wmclass_atom;
	gboolean running;
	gint error;

	window_id_atom = XInternAtom (GDK_DISPLAY (),
				      "NAUTILUS_DESKTOP_WINDOW_ID", True);

	if (window_id_atom == None) return FALSE;

	retval = XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				     window_id_atom, 0, 1, False, XA_WINDOW,
				     &actual_type, &actual_format, &nitems,
				     &bytes_after, &data);

	if (data != NULL) {
		nautilus_xid = *(Window *) data;
		XFree (data);
	} else {
		return FALSE;
	}

	if (actual_type != XA_WINDOW) return FALSE;
	if (actual_format != 32) return FALSE;

	wmclass_atom = XInternAtom (GDK_DISPLAY (), "WM_CLASS", False);

	gdk_error_trap_push ();

	retval = XGetWindowProperty (GDK_DISPLAY (), nautilus_xid,
				     wmclass_atom, 0, 24, False, XA_STRING,
				     &actual_type, &actual_format, &nitems,
				     &bytes_after, &data);

	error = gdk_error_trap_pop ();

	if (error == BadWindow) return FALSE;

	if (actual_type == XA_STRING &&
	    nitems == 24 &&
	    bytes_after == 0 &&
	    actual_format == 8 &&
	    data != NULL &&
	    !strcmp (data, "desktop_window") &&
	    !strcmp (data + strlen (data) + 1, "Nautilus"))
		running = TRUE;
	else
		running = FALSE;

	if (data != NULL)
		XFree (data);

	return running;
}

static gboolean
gnome_cc_save_yourself (GnomeClient *client, gint phase, GnomeSaveStyle save_style,
			gboolean shutdown, GnomeInteractStyle interact_style,
			gboolean fast, gchar *argv0)
{
	gchar *argv[3];
	gint argc;

	argv[0] = argv0;
	argv[1] = "--use-shell";
	argc = 2;
	gnome_client_set_clone_command (client, argc, argv);
	gnome_client_set_restart_command (client, argc, argv);

	return TRUE;
}

static void
gnome_cc_die (void)
{
	gtk_main_quit ();
}

#if 0
static void
gnome_cc_about (void)
{
	static GtkWidget *about;
	const char *authors[] = { "Christopher James Lahey <clahey@ximian.com>", NULL };
	const char *documenters[] = { NULL };
	if (about) {
		gdk_window_raise (about->window);
	} else {
		gtk_widget_show (about = gnome_about_new (_("Gnome Control Center"),
						  VERSION,
						  "Copyright 2002, Ximian, Inc.",
						  NULL,
						  authors,
						  documenters,
						  NULL,
						  NULL));
		g_object_add_weak_pointer (G_OBJECT (about), (void **) &about);
	}
}
#endif

static void
canvas_draw_background (GnomeCanvas *canvas, GdkDrawable *drawable,
			int x, int y, int width, int height, ControlCenter *user_data)
{
	/* By default, we use the style background. */
	gdk_gc_set_foreground (canvas->pixmap_gc,
			       &GTK_WIDGET (canvas)->style->base[GTK_STATE_NORMAL]);
	gdk_draw_rectangle (drawable,
			    canvas->pixmap_gc,
			    TRUE,
			    0, 0,
			    width, height);

	g_signal_stop_emission_by_name (canvas, "draw_background");
}

static ControlCenter *
create_control_center ()
{
	ControlCenter *cc;
	GtkWidget *scroll_window;

	cc = g_new (ControlCenter, 1);

	cc->canvas = GNOME_CANVAS (gnome_canvas_new ());

	cc->max_width = 300;
	cc->min_height = 0;
	cc->info = NULL;
	cc->selected = NULL;
	cc->last_x = -1;
	cc->status_cb = NULL;
	cc->status_data = NULL;
	cc->current_status = NULL;
	cc->firstlayout = FALSE;

	g_signal_connect (cc->canvas, "size_allocate",
			  G_CALLBACK (size_allocate), cc);
	g_signal_connect (cc->canvas, "realize",
			  G_CALLBACK (canvas_realize), cc);
	g_signal_connect (cc->canvas, "style_set",
			  G_CALLBACK (canvas_style_set), cc);
	g_signal_connect (cc->canvas, "draw_background",
			  G_CALLBACK (canvas_draw_background), cc);
	gtk_widget_show_all (GTK_WIDGET (cc->canvas));

	scroll_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll_window), GTK_WIDGET (cc->canvas));

	cc->widget = scroll_window;
	return cc;
}

static void
control_center_set_info (ControlCenter *cc, ControlCenterInformation *info)
{
	cc->info = info;
	cc->firstlayout = TRUE;
}

static void
control_center_set_status_cb (ControlCenter               *cc,
			      ControlCenterStatusCallback  status_cb,
			      void                        *status_data)
{
	cc->status_cb = status_cb;
	cc->status_data = status_data;
}

static void
change_status (ControlCenter *cc, const gchar *status, void *data)
{
	GnomeAppBar *bar = data;

	if (!status)
		status = "";

	gnome_appbar_set_status (bar, status);
}

static void
cb_focus_changed (ControlCenter *cc)
{
	if (cc->selected)
		setup_entry (cc->selected);
}

static GtkWindow *
create_window (const gchar *appname,
	       const gchar *uri)
{
	GtkWidget *window;
	GnomeClient *client;
	GtkWidget *appbar;
	ControlCenter *cc;
	ControlCenterInformation *info;

	client = gnome_master_client ();
	g_signal_connect (G_OBJECT (client),
		"save_yourself",
		G_CALLBACK (gnome_cc_save_yourself), (void *) appname);
	g_signal_connect (G_OBJECT (client),
		"die",
		G_CALLBACK (gnome_cc_die), NULL);

	info = control_center_get_categories (uri);
	window = gnome_app_new ("gnomecc", info->title);
	gnome_window_icon_set_from_file (GTK_WINDOW (window),
					 PIXMAP_DIR "/control-center.png");
	gtk_window_set_default_size (GTK_WINDOW (window), 760, 530);

	appbar = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_USER);
	gnome_app_set_statusbar (GNOME_APP (window), appbar);

	cc = create_control_center ();
	control_center_set_info (cc, info);
	control_center_set_status_cb (cc, change_status, appbar);

	gnome_app_set_contents (GNOME_APP (window), cc->widget);

	gtk_widget_show_all (window);

	g_object_weak_ref (G_OBJECT (window), (GWeakNotify) gnome_cc_die, NULL);
	g_signal_connect_swapped (G_OBJECT (window),
		"notify::has-toplevel-focus",
		G_CALLBACK (cb_focus_changed), cc);

	return GTK_WINDOW (window);
}

static void
change_status_view (ControlCenter *cc, const gchar *status, void *data)
{
	NautilusView *view = data;

	if (!status)
		status = "";

	nautilus_view_report_status (view, status);
}

static void
cb_load_location (NautilusView  *view,
		  char const    *location,
		  ControlCenter *cc)
{
	ControlCenterInformation *info;

	nautilus_view_report_load_underway (view);

	info = control_center_get_categories (location);
	control_center_set_info (cc, info);
	control_center_set_status_cb (cc, change_status_view, view);

	gtk_widget_show_all (cc->widget);

	nautilus_view_report_load_complete (view);
}

#define GNOMECC_VIEW_OAFIID "OAFIID:GNOME_ControlCenter_View"
#define GNOMECC_FACTORY_OAFIID "OAFIID:GNOME_ControlCenter_Factory"

static BonoboObject *
factory_create_cb (BonoboGenericFactory *factory,
		   gchar const          *iid,
		   gpointer              closure)
{
	ControlCenter *cc;
	NautilusView *view;

	if (strcmp (iid, GNOMECC_VIEW_OAFIID) != 0) {
		return NULL;
	}

	cc = create_control_center ();
	view = nautilus_view_new (cc->widget);

	g_signal_connect (view,
			  "load_location",
			  G_CALLBACK (cb_load_location),
			  cc);

	return BONOBO_OBJECT (view);
}

int
main (int argc, char *argv[])
{
	GnomeProgram *ccprogram;

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

	ccprogram = gnome_program_init ("gnome-control-center",
			    VERSION, LIBGNOMEUI_MODULE,
			    argc, argv,
			    GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
			    GNOME_PARAM_POPT_TABLE, cap_options,
			    NULL);

	if (use_nautilus && is_nautilus_running ())
		execlp ("nautilus", "nautilus", "preferences:///", NULL);

	if (bonobo_activation_iid_get ()) {
		/* Look for an existing factory */
		CORBA_Object existing_factory = bonobo_activation_activate_from_id (
			GNOMECC_FACTORY_OAFIID, Bonobo_ACTIVATION_FLAG_EXISTING_ONLY,
			NULL, NULL);

		if (existing_factory == CORBA_OBJECT_NIL) {
			/* Not started, start now */
			gchar *registration_id = bonobo_activation_make_registration_id (
				GNOMECC_FACTORY_OAFIID, DisplayString (gdk_display));
			BonoboGenericFactory *factory = bonobo_generic_factory_new (
				registration_id, factory_create_cb, NULL);
			g_free (registration_id);

			bonobo_running_context_auto_exit_unref (
				BONOBO_OBJECT (factory));

			bonobo_main ();
		}
	} else {
		const gchar **args;
		poptContext ctx;
		GValue context = { 0 };

		g_object_get_property (G_OBJECT (ccprogram),
				       GNOME_PARAM_POPT_CONTEXT,
				       g_value_init (&context, G_TYPE_POINTER));
		ctx = g_value_get_pointer (&context);

		/* Create a standalone window */
		args = poptGetArgs (ctx);
		if (args != NULL) {
			create_window (argv[0], args[0]);
		} else {
			create_window (argv[0], "preferences:///");
		}
		poptFreeContext (ctx);

		gtk_main ();
	}

#if 0
	if (gnome_unique_window_create ("gnome-control-center", create_unique_window, argv[0]))
		gtk_main ();
#endif

	return 0;
}
