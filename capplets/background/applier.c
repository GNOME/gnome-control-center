/* -*- mode: c; style: linux -*- */

/* applier.c
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#include <gnome.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkprivate.h>
#include <gdk-pixbuf/gdk-pixbuf-xlib.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <unistd.h>
#include <bonobo.h>

#include "applier.h"

#define MONITOR_CONTENTS_X 20
#define MONITOR_CONTENTS_Y 10
#define MONITOR_CONTENTS_WIDTH 157
#define MONITOR_CONTENTS_HEIGHT 111

static gboolean gdk_pixbuf_xlib_inited = FALSE;

typedef struct _Renderer Renderer;

enum {
	ARG_0,
};

struct _ApplierPrivate 
{
	GtkWidget          *preview_widget;
	Preferences        *root_prefs;
	Preferences        *preview_prefs;

	GdkPixbuf          *wallpaper_pixbuf;
	gchar              *wallpaper_filename;

	Renderer           *root_renderer;
	Renderer           *preview_renderer;

	gboolean            nautilus_running;
};

struct _Renderer
{
	gboolean            is_root;
	gboolean            is_set;

	GtkWidget          *widget;
	Preferences        *prefs;

	gint                x;         /* Geometry relative to pixmap */
	gint                y;
	gint                width;
	gint                height;

	gint                srcx;      /* Geometry relative to pixbuf */
	gint                srcy;      /* (used when the wallpaper is too big) */

	gint                wx;        /* Geometry of wallpaper as rendered */
	gint                wy;
	gint                wwidth;
	gint                wheight;

	gint                pwidth;    /* Geometry of unscaled wallpaper */
	gint                pheight;

	gint                gwidth;    /* Geometry of gradient-only pixmap */
	gint                gheight;

	guchar             *gradient_data;
	GdkPixbuf          *wallpaper_pixbuf;  /* Alias only */
	GdkPixbuf          *prescaled_pixbuf;  /* For tiled on preview */
	GdkPixbuf          *pixbuf;
	Pixmap              pixmap;
};

static GtkObjectClass *parent_class;

static void applier_init             (Applier *prefs);
static void applier_class_init       (ApplierClass *class);

static void applier_set_arg          (GtkObject *object, 
				      GtkArg *arg, 
				      guint arg_id);
static void applier_get_arg          (GtkObject *object, 
				      GtkArg *arg, 
				      guint arg_id);

static void applier_destroy          (GtkObject *object);

static void run_render_pipeline      (Renderer *renderer, 
				      Preferences *old_prefs,
				      Preferences *new_prefs,
				      GdkPixbuf *wallpaper_pixbuf);
static void draw_disabled_message    (GtkWidget *widget);

static Renderer *renderer_new        (Applier *applier, gboolean is_root);
static void renderer_destroy         (Renderer *renderer);

static void renderer_set_prefs       (Renderer *renderer,
				      Preferences *prefs);
static void renderer_set_wallpaper   (Renderer *renderer, 
				      GdkPixbuf *wallpaper_pixbuf);

static void renderer_render_background (Renderer *renderer);
static void renderer_render_wallpaper  (Renderer *renderer);
static void renderer_create_pixmap     (Renderer *renderer);
static void renderer_render_to_screen  (Renderer *renderer);

static guchar *fill_gradient         (gint w,
				      gint h,
				      GdkColor *c1,
				      GdkColor *c2,
				      orientation_t orientation);
static void get_geometry             (wallpaper_type_t wallpaper_type,
				      GdkPixbuf *pixbuf,
				      int dwidth, int dheight,
				      int vwidth, int vheight,
				      int *xoffset, int *yoffset, 
				      int *rwidth, int *rheight,
				      int *srcx, int *srcy);
static void render_tiled_image       (Pixmap pixmap, GC xgc,
				      GdkPixbuf *pixbuf,
				      gint x, gint y, 
				      gint dwidth, gint dheight);
static void tile_composite           (GdkPixbuf *dest, GdkPixbuf *src,
				      gdouble sx, gdouble sy,
				      gdouble swidth, gdouble sheight, 
				      gdouble dwidth, gdouble dheight,
				      gdouble scalex, gdouble scaley,
				      gint alpha_value);

static gboolean render_gradient_p    (Renderer *renderer,
				      Preferences *prefs);
static gboolean render_small_pixmap_p (Preferences *prefs);

static Pixmap make_root_pixmap       (gint width, gint height);
static void set_root_pixmap          (Pixmap pixmap);

static gboolean is_nautilus_running  (void);

static void output_compat_prefs      (const Preferences *prefs);

guint
applier_get_type (void)
{
	static guint applier_type = 0;

	if (!applier_type) {
		GtkTypeInfo applier_info = {
			"Applier",
			sizeof (Applier),
			sizeof (ApplierClass),
			(GtkClassInitFunc) applier_class_init,
			(GtkObjectInitFunc) applier_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		applier_type = 
			gtk_type_unique (gtk_object_get_type (), 
					 &applier_info);
	}

	return applier_type;
}

static void
applier_init (Applier *applier)
{
	applier->private = g_new0 (ApplierPrivate, 1);
	applier->private->root_prefs = NULL;
	applier->private->preview_prefs = NULL;
	applier->private->root_renderer = NULL;
	applier->private->preview_renderer = NULL;
	applier->private->nautilus_running = is_nautilus_running ();
}

static void
applier_class_init (ApplierClass *class) 
{
	GtkObjectClass *object_class;
	GdkVisual *visual;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = applier_destroy;
	object_class->set_arg = applier_set_arg;
	object_class->get_arg = applier_get_arg;

	parent_class = 
		GTK_OBJECT_CLASS (gtk_type_class (gtk_object_get_type ()));

	if (!gdk_pixbuf_xlib_inited) {
		gdk_pixbuf_xlib_inited = TRUE;

		visual = gdk_window_get_visual (GDK_ROOT_PARENT ());

		gdk_pixbuf_xlib_init_with_depth
			(GDK_DISPLAY (), gdk_screen, visual->depth);
	}
}

static void
applier_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Applier *applier;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_APPLIER (object));

	applier = APPLIER (object);

	switch (arg_id) {
	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
applier_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	Applier *applier;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_APPLIER (object));

	applier = APPLIER (object);

	switch (arg_id) {
	default:
		g_warning ("Bad argument get");
		break;
	}
}

GtkObject *
applier_new (void) 
{
	GtkObject *object;

	object = gtk_object_new (applier_get_type (),
				 NULL);

	return object;
}

static void
applier_destroy (GtkObject *object) 
{
	Applier *applier;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_APPLIER (object));

	applier = APPLIER (object);

	if (applier->private->preview_renderer != NULL)
		renderer_destroy (applier->private->preview_renderer);
	if (applier->private->root_renderer != NULL)
		renderer_destroy (applier->private->root_renderer);

	if (applier->private->root_prefs != NULL)
		gtk_object_unref (GTK_OBJECT (applier->private->root_prefs));
	if (applier->private->preview_prefs != NULL)
		gtk_object_unref (GTK_OBJECT (applier->private->preview_prefs));

	if (applier->private->wallpaper_pixbuf != NULL)
		gdk_pixbuf_unref (applier->private->wallpaper_pixbuf);
	if (applier->private->wallpaper_filename != NULL)
		g_free (applier->private->wallpaper_filename);

	g_free (applier->private);

	parent_class->destroy (object);
}

void
applier_apply_prefs (Applier           *applier, 
		     const Preferences *prefs,
		     gboolean           do_root,
		     gboolean           do_preview)
{
	Preferences *new_prefs;

	g_return_if_fail (applier != NULL);
	g_return_if_fail (IS_APPLIER (applier));

	if (do_root && applier->private->nautilus_running) {
		set_root_pixmap (-1);
	}

	if (!prefs->enabled) {
		if (do_preview)
			draw_disabled_message (applier_get_preview_widget (applier));
		return;
	}

	new_prefs = PREFERENCES (preferences_clone (prefs));

	if (((prefs->wallpaper_filename || 
	      applier->private->wallpaper_filename) &&
	     (!prefs->wallpaper_filename ||
	      !applier->private->wallpaper_filename)) ||
	    (prefs->wallpaper_filename &&
	     applier->private->wallpaper_filename &&
	     strcmp (applier->private->wallpaper_filename, 
		     prefs->wallpaper_filename)))
	{
		if (applier->private->wallpaper_pixbuf != NULL)
			gdk_pixbuf_unref (applier->private->wallpaper_pixbuf);
		if (applier->private->wallpaper_filename != NULL)
			g_free (applier->private->wallpaper_filename);

		applier->private->wallpaper_filename = NULL;
		applier->private->wallpaper_pixbuf = NULL;
	}

	if (prefs->wallpaper_enabled &&
	    prefs->wallpaper_filename && 
	    (applier->private->wallpaper_filename == NULL ||
	     strcmp (applier->private->wallpaper_filename, 
		     prefs->wallpaper_filename)))
	{
		applier->private->wallpaper_filename =
			g_strdup (prefs->wallpaper_filename);
		applier->private->wallpaper_pixbuf = 
			gdk_pixbuf_new_from_file (prefs->wallpaper_filename);

		if (!applier->private->wallpaper_pixbuf) {
			g_warning (_("Could not load pixbuf \"%s\"; disabling wallpaper."),
				   prefs->wallpaper_filename);
			new_prefs->wallpaper_enabled = FALSE;
		}
	}
	else if (applier->private->wallpaper_pixbuf == NULL) {
		new_prefs->wallpaper_enabled = FALSE;
	}

	if (do_preview) {
		if (applier->private->preview_renderer == NULL)
			applier->private->preview_renderer = 
				renderer_new (applier, FALSE);

		run_render_pipeline (applier->private->preview_renderer,
				     applier->private->preview_prefs, 
				     new_prefs,
				     applier->private->wallpaper_pixbuf);

		if (applier->private->preview_prefs != NULL)
			gtk_object_unref (GTK_OBJECT
					  (applier->private->preview_prefs));

		applier->private->preview_prefs = new_prefs;
		gtk_object_ref (GTK_OBJECT (new_prefs));

		if (applier->private->preview_widget != NULL)
			gtk_widget_queue_draw (applier->private->preview_widget);
	}

	if (do_root) {
		nice (20);

		if (applier->private->root_renderer == NULL)
			applier->private->root_renderer = renderer_new (applier, TRUE);

		if (!applier->private->nautilus_running)
			run_render_pipeline (applier->private->root_renderer,
					     applier->private->root_prefs, 
					     new_prefs,
					     applier->private->wallpaper_pixbuf);

		output_compat_prefs (prefs);

		if (applier->private->root_prefs != NULL)
			gtk_object_unref (GTK_OBJECT 
					  (applier->private->root_prefs));
		
		applier->private->root_prefs = new_prefs;
		gtk_object_ref (GTK_OBJECT (new_prefs));
	}

	gtk_object_unref (GTK_OBJECT (new_prefs));
}

gboolean
applier_render_gradient_p (Applier *applier) 
{
	return render_gradient_p (applier->private->preview_renderer, applier->private->preview_prefs);
}

GtkWidget *
applier_get_preview_widget (Applier *applier) 
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GdkVisual *visual;
	GdkColormap *colormap;
	Pixmap xpixmap, xmask;
	gchar *filename;
	GdkGC *gc;

	if (applier->private->preview_widget != NULL) return applier->private->preview_widget;

	filename = gnome_pixmap_file ("monitor.png");
	visual = gdk_window_get_visual (GDK_ROOT_PARENT ());
	colormap = gdk_window_get_colormap (GDK_ROOT_PARENT ());

	gtk_widget_push_visual (visual);
	gtk_widget_push_colormap (colormap);

	pixbuf = gdk_pixbuf_new_from_file (filename);

	if (pixbuf == NULL) return NULL;

	gdk_pixbuf_xlib_render_pixmap_and_mask (pixbuf, &xpixmap, &xmask, 1);

	if (xpixmap) {
		pixmap = gdk_pixmap_new (GDK_ROOT_PARENT (),
					 gdk_pixbuf_get_width (pixbuf),
					 gdk_pixbuf_get_height (pixbuf),
					 visual->depth);

		gc = gdk_gc_new (GDK_ROOT_PARENT ());

		XCopyArea (GDK_DISPLAY (), xpixmap,
			   GDK_WINDOW_XWINDOW (pixmap), 
			   GDK_GC_XGC (gc), 0, 0,
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf),
			   0, 0);

		XFreePixmap (GDK_DISPLAY (), xpixmap);

		gdk_gc_destroy (gc);
	} else {
		pixmap = NULL;
	}

	if (xmask) {
		mask = gdk_pixmap_new (GDK_ROOT_PARENT (),
				       gdk_pixbuf_get_width (pixbuf),
				       gdk_pixbuf_get_height (pixbuf),
				       1);

		gc = gdk_gc_new (mask);

		XCopyArea (GDK_DISPLAY (), xmask, 
			   GDK_WINDOW_XWINDOW (mask), 
			   GDK_GC_XGC (gc), 0, 0,
			   gdk_pixbuf_get_width (pixbuf),
			   gdk_pixbuf_get_height (pixbuf),
			   0, 0);

		XFreePixmap (GDK_DISPLAY (), xmask);

		gdk_gc_destroy (gc);
	} else {
		mask = NULL;
	}

	applier->private->preview_widget = gtk_pixmap_new (pixmap, mask);
	gtk_widget_show (applier->private->preview_widget);
	gdk_pixbuf_unref (pixbuf);
	g_free (filename);

	gtk_widget_pop_visual ();
	gtk_widget_pop_colormap ();

	return applier->private->preview_widget;
}

static void
draw_disabled_message (GtkWidget *widget)
{
	GdkPixmap  *pixmap;
	GdkColor    color;
	GdkFont    *font;
	GdkGC      *gc;
	gint        x, y, w, h;
	gint        height, width;
	const char *disabled_string = _("Disabled");

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_PIXMAP (widget));

	w = MONITOR_CONTENTS_WIDTH;
	h = MONITOR_CONTENTS_HEIGHT;
	x = MONITOR_CONTENTS_X;
	y = MONITOR_CONTENTS_Y;

	if (!GTK_WIDGET_REALIZED (widget))
		gtk_widget_realize (widget);

	pixmap = GTK_PIXMAP (widget)->pixmap;

	gc = gdk_gc_new (widget->window);

	gdk_color_black (gtk_widget_get_colormap (widget), &color);
	gdk_gc_set_foreground (gc, &color);
	gdk_draw_rectangle (pixmap, gc, TRUE, x, y, w, h);

	font = widget->style->font;
	width = gdk_string_width (font, disabled_string);
	height = gdk_string_height (font, disabled_string);

	gdk_color_white (gtk_widget_get_colormap (widget), &color);
	gdk_gc_set_foreground (gc, &color);
	gdk_draw_string (pixmap, font, gc, 
			 x + (w - width) / 2, y + (h - height) / 2 +
			 height / 2,
			 disabled_string);

	gdk_gc_unref (gc);
	gtk_widget_queue_draw (widget);
}

static void
run_render_pipeline (Renderer *renderer, 
		     Preferences *old_prefs,
		     Preferences *new_prefs,
		     GdkPixbuf *wallpaper_pixbuf)
{
	gboolean bg_formed = FALSE;
	gboolean wp_set = FALSE;
	gboolean opt_old_prefs, opt_new_prefs;

	g_return_if_fail (renderer != NULL);
	g_return_if_fail (new_prefs != NULL);

	renderer_set_prefs (renderer, new_prefs);

	if (old_prefs == NULL ||
	    (render_gradient_p (renderer, old_prefs) !=
	     render_gradient_p (renderer, new_prefs)) ||
	    old_prefs->gradient_enabled != new_prefs->gradient_enabled ||
	    old_prefs->wallpaper_enabled != new_prefs->wallpaper_enabled ||
	    old_prefs->orientation != new_prefs->orientation ||
	    !gdk_color_equal (old_prefs->color1, new_prefs->color1) ||
	    !gdk_color_equal (old_prefs->color2, new_prefs->color2) ||
	    old_prefs->adjust_opacity != new_prefs->adjust_opacity ||
	    old_prefs->opacity != new_prefs->opacity ||
	    ((wallpaper_pixbuf != NULL ||
	      old_prefs->wallpaper_type != new_prefs->wallpaper_type) &&
	     render_gradient_p (renderer, new_prefs)))
	{
		renderer_render_background (renderer);
		bg_formed = TRUE;
	}

	if (wallpaper_pixbuf != renderer->wallpaper_pixbuf) {
		renderer_set_wallpaper (renderer, wallpaper_pixbuf);
		wp_set = TRUE;
	}

	if (old_prefs)
		opt_old_prefs = render_small_pixmap_p (old_prefs);
	else
		opt_old_prefs = FALSE;

	opt_new_prefs = render_small_pixmap_p (new_prefs);

	if (renderer->is_root &&
	    (renderer->pixmap == 0 ||
	     (opt_old_prefs != opt_new_prefs) ||
	     (opt_old_prefs && opt_new_prefs &&
	      (old_prefs->orientation != new_prefs->orientation))))
		renderer_create_pixmap (renderer);

	if (bg_formed || wp_set ||
	    old_prefs->wallpaper_type != new_prefs->wallpaper_type)
	{
		renderer_render_wallpaper (renderer);
	}

	renderer_render_to_screen (renderer);
}

static Renderer *
renderer_new (Applier *applier, gboolean is_root) 
{
	Renderer *renderer;

	renderer = g_new (Renderer, 1);
	renderer->is_root = is_root;

	if (is_root) {
		renderer->x = 0;
		renderer->y = 0;
		renderer->width = gdk_screen_width ();
		renderer->height = gdk_screen_height ();
		renderer->pixmap = 0;
		renderer->is_set = FALSE;
	} else {
		renderer->widget = applier_get_preview_widget (applier);

		if (!GTK_WIDGET_REALIZED (renderer->widget))
			gtk_widget_realize (renderer->widget);

		renderer->x = MONITOR_CONTENTS_X;
		renderer->y = MONITOR_CONTENTS_Y;
		renderer->width = MONITOR_CONTENTS_WIDTH;
		renderer->height = MONITOR_CONTENTS_HEIGHT;
		renderer->pixmap = 
			GDK_WINDOW_XWINDOW (GTK_PIXMAP 
					    (applier->private->preview_widget)->pixmap);
		renderer->is_set = TRUE;
	}

	renderer->gradient_data = NULL;
	renderer->wallpaper_pixbuf = NULL;
	renderer->pixbuf = NULL;
	renderer->prefs = NULL;

	return renderer;
}

static void
renderer_destroy (Renderer *renderer) 
{
	g_return_if_fail (renderer != NULL);

	if (renderer->prefs != NULL)
		gtk_object_unref (GTK_OBJECT (renderer->prefs));

	if (renderer->wallpaper_pixbuf != NULL)
		gdk_pixbuf_unref (renderer->wallpaper_pixbuf);

	if (renderer->pixbuf != NULL)
		gdk_pixbuf_unref (renderer->pixbuf);

	g_free (renderer);
}

static void
renderer_set_prefs (Renderer *renderer, Preferences *prefs) 
{
	g_return_if_fail (renderer != NULL);

	if (renderer->prefs)
		gtk_object_unref (GTK_OBJECT (renderer->prefs));

	renderer->prefs = prefs;

	if (prefs)
		gtk_object_ref (GTK_OBJECT (prefs));
}

static void
renderer_set_wallpaper (Renderer *renderer, GdkPixbuf *wallpaper_pixbuf) 
{
	g_return_if_fail (renderer != NULL);

	if (renderer->wallpaper_pixbuf)
		gdk_pixbuf_unref (renderer->wallpaper_pixbuf);

	renderer->wallpaper_pixbuf = wallpaper_pixbuf;

	if (wallpaper_pixbuf) {
		gdk_pixbuf_ref (wallpaper_pixbuf);
		renderer->pwidth = gdk_pixbuf_get_width (wallpaper_pixbuf);
		renderer->pheight = gdk_pixbuf_get_height (wallpaper_pixbuf);
	} else {
		renderer->pwidth = renderer->pheight = 0;
	}
}

static void
renderer_render_background (Renderer *renderer) 
{
	g_return_if_fail (renderer != NULL);

	if (render_gradient_p (renderer, renderer->prefs)) {
		renderer->gwidth = renderer->width;
		renderer->gheight = renderer->height;

		if (renderer->is_root && !renderer->prefs->wallpaper_enabled) {
			if (renderer->prefs->orientation == ORIENTATION_HORIZ)
				renderer->gheight = 32;
			else
				renderer->gwidth = 32;
		}

		if (renderer->pixbuf != NULL)
			gdk_pixbuf_unref (renderer->pixbuf);

		renderer->gradient_data = 
			fill_gradient (renderer->gwidth, renderer->gheight,
				       renderer->prefs->color1, 
				       renderer->prefs->color2, 
				       renderer->prefs->orientation);

		renderer->pixbuf = 
			gdk_pixbuf_new_from_data (renderer->gradient_data,
						  GDK_COLORSPACE_RGB, 
						  FALSE, 8, 
						  renderer->gwidth, 
						  renderer->gheight, 
						  renderer->gwidth * 3, 
						  (GdkPixbufDestroyNotify) 
						  g_free, 
						  NULL);
	}
}

static void
renderer_render_wallpaper (Renderer *renderer) 
{
	gint root_width, root_height;
	gdouble scalex, scaley;

	g_return_if_fail (renderer != NULL);
	g_return_if_fail (!render_gradient_p (renderer, renderer->prefs) ||
			  renderer->pixbuf != NULL);
	g_return_if_fail (!renderer->prefs->wallpaper_enabled ||
			  renderer->wallpaper_pixbuf != NULL);

	if (renderer->prefs->wallpaper_enabled) {
		gdk_window_get_size (GDK_ROOT_PARENT (),
				     &root_width, &root_height);

		get_geometry (renderer->prefs->wallpaper_type,
			      renderer->wallpaper_pixbuf,
			      renderer->width, renderer->height,
			      root_width, root_height,
			      &renderer->wx, &renderer->wy,
			      &renderer->wwidth, &renderer->wheight,
			      &renderer->srcx, &renderer->srcy);

		if (renderer->prefs->wallpaper_type == WPTYPE_TILED &&
		    renderer->wwidth != renderer->pwidth &&
		    renderer->wheight != renderer->pheight)
			renderer->prescaled_pixbuf =
				gdk_pixbuf_scale_simple
				(renderer->wallpaper_pixbuf,
				 renderer->wwidth,
				 renderer->wheight,
				 GDK_INTERP_BILINEAR);
		else
			renderer->prescaled_pixbuf = NULL;

		if (renderer->prefs->adjust_opacity) {
			guint alpha_value;
			guint32 colorv;

			alpha_value = 2.56 * renderer->prefs->opacity;
			alpha_value = alpha_value * alpha_value / 256;
			alpha_value = CLAMP (alpha_value, 0, 255);

			if (render_gradient_p (renderer, renderer->prefs)) {
				scalex = (gdouble) renderer->wwidth / 
					(gdouble) renderer->pwidth;
				scaley = (gdouble) renderer->wheight / 
					(gdouble) renderer->pheight;

				if (renderer->prefs->wallpaper_type !=
				    WPTYPE_TILED) 
					gdk_pixbuf_composite
						(renderer->wallpaper_pixbuf,
						 renderer->pixbuf,
						 renderer->wx, renderer->wy,
						 renderer->wwidth, 
						 renderer->wheight,
						 renderer->wx, renderer->wy,
						 scalex, scaley,
						 GDK_INTERP_BILINEAR,
						 alpha_value);
				else if (renderer->wwidth != 
					 renderer->pwidth &&
					 renderer->wheight != 
					 renderer->pheight)
					tile_composite
						(renderer->prescaled_pixbuf,
						 renderer->pixbuf,
						 renderer->wx, renderer->wy,
						 renderer->wwidth, 
						 renderer->wheight,
						 renderer->width,
						 renderer->height,
						 1.0, 1.0,
						 alpha_value);
				else
					tile_composite
						(renderer->wallpaper_pixbuf,
						 renderer->pixbuf,
						 renderer->wx, renderer->wy,
						 renderer->wwidth, 
						 renderer->wheight,
						 renderer->width,
						 renderer->height,
						 scalex, scaley,
						 alpha_value);
			} else {
				GdkColor *color;

				color = renderer->prefs->color1;
				colorv = ((color->red & 0xff00) << 8) |
					(color->green & 0xff00) |
					((color->blue & 0xff00) >> 8);

				if (renderer->pixbuf != NULL)
					gdk_pixbuf_unref (renderer->pixbuf);

				renderer->pixbuf =
					gdk_pixbuf_composite_color_simple 
					(renderer->wallpaper_pixbuf,
					 renderer->wwidth, renderer->wheight,
					 GDK_INTERP_BILINEAR,
					 alpha_value, 65536,
					 colorv, colorv);
			}
		}
		else if (renderer->wwidth != renderer->pwidth ||
			 renderer->wheight != renderer->pheight)
		{
			if (render_gradient_p (renderer, renderer->prefs)) {
				scalex = (gdouble) renderer->wwidth / 
					(gdouble) renderer->pwidth;
				scaley = (gdouble) renderer->wheight / 
					(gdouble) renderer->pheight;
				gdk_pixbuf_scale 
					(renderer->wallpaper_pixbuf,
					 renderer->pixbuf,
					 renderer->wx, renderer->wy,
					 MIN (renderer->wwidth, renderer->width), 
					 MIN (renderer->wheight, renderer->height),
					 renderer->wx - renderer->srcx,
					 renderer->wy - renderer->srcy,
					 scalex, scaley,
					 GDK_INTERP_BILINEAR);
			} else {
				if (renderer->pixbuf != NULL)
					gdk_pixbuf_unref (renderer->pixbuf);

				if (renderer->prescaled_pixbuf !=
				    renderer->wallpaper_pixbuf)
				{
					renderer->pixbuf =
						gdk_pixbuf_scale_simple
						(renderer->wallpaper_pixbuf,
						 renderer->wwidth,
						 renderer->wheight,
						 GDK_INTERP_BILINEAR);
				} else {
					renderer->pixbuf =
						renderer->prescaled_pixbuf;
					gdk_pixbuf_ref (renderer->pixbuf);
				}
			}
		} else {
			if (render_gradient_p (renderer, renderer->prefs)) {
				gdk_pixbuf_copy_area
					(renderer->wallpaper_pixbuf,
					 renderer->srcx, renderer->srcy, 
					 MIN (renderer->width, renderer->pwidth),
					 MIN (renderer->height, renderer->pheight),
					 renderer->pixbuf,
					 renderer->wx, renderer->wy);
			} else {
				if (renderer->pixbuf != NULL)
					gdk_pixbuf_unref (renderer->pixbuf);

				renderer->pixbuf = renderer->wallpaper_pixbuf;

				gdk_pixbuf_ref (renderer->pixbuf);
			}
		}

		if (renderer->prescaled_pixbuf != NULL)
			gdk_pixbuf_unref (renderer->prescaled_pixbuf);
	}
}

static void
renderer_create_pixmap (Renderer *renderer) 
{
	gint width, height;

	g_return_if_fail (renderer != NULL);
	g_return_if_fail (renderer->prefs != NULL);

	if (renderer->is_root) {
		if (renderer->prefs->gradient_enabled &&
		    !renderer->prefs->wallpaper_enabled) 
		{
			width = renderer->gwidth;
			height = renderer->gheight;
		} else {
			width = renderer->width;
			height = renderer->height;
		}

		renderer->pixmap = make_root_pixmap (width, height);
		renderer->is_set = FALSE;
	}
}

static void
renderer_render_to_screen (Renderer *renderer) 
{
	GdkGC *gc;
	GC xgc;

	g_return_if_fail (renderer != NULL);
	g_return_if_fail ((!renderer->prefs->gradient_enabled &&
			   !renderer->prefs->wallpaper_enabled) ||
			  renderer->pixbuf != NULL);
	g_return_if_fail (renderer->pixmap != 0);

	gc = gdk_gc_new (GDK_ROOT_PARENT ());
	xgc = GDK_GC_XGC (gc);

	if (render_gradient_p (renderer, renderer->prefs)) {
		gdk_pixbuf_xlib_render_to_drawable
			(renderer->pixbuf,
			 renderer->pixmap, xgc,
			 0, 0, renderer->x, renderer->y,
			 renderer->gwidth, renderer->gheight,
			 GDK_RGB_DITHER_NORMAL, 0, 0);
	}
	else if (renderer->prefs->wallpaper_enabled &&
		 renderer->prefs->wallpaper_type == WPTYPE_TILED)
	{
		render_tiled_image (renderer->pixmap, xgc,
				    renderer->pixbuf,
				    renderer->x, renderer->y,
				    renderer->width, renderer->height);
	} 
	else if (renderer->prefs->wallpaper_enabled) {
		if (renderer->wx != renderer->x ||
		    renderer->wy != renderer->y ||
		    renderer->wwidth != renderer->width ||
		    renderer->wheight != renderer->height)
		{
			/* FIXME: Potential flickering problems? */
			gdk_color_alloc (gdk_window_get_colormap
					 (GDK_ROOT_PARENT()), 
					 renderer->prefs->color1);
			gdk_gc_set_foreground (gc, renderer->prefs->color1);
			XFillRectangle (GDK_DISPLAY (), renderer->pixmap, xgc, 
					renderer->x, renderer->y, 
					renderer->width, renderer->height);
		}

		gdk_pixbuf_xlib_render_to_drawable
			(renderer->pixbuf,
			 renderer->pixmap, xgc,
			 renderer->srcx, renderer->srcy, 
			 renderer->x + MAX (renderer->wx, 0), 
			 renderer->y + MAX (renderer->wy, 0),
			 MIN (renderer->width, renderer->wwidth), 
			 MIN (renderer->height, renderer->wheight),
			 GDK_RGB_DITHER_NORMAL, 0, 0);
	} else {
		if (renderer->is_root) {
			gdk_color_alloc (gdk_window_get_colormap
					 (GDK_ROOT_PARENT()), 
					 renderer->prefs->color1);
			gdk_window_set_background (GDK_ROOT_PARENT (), 
						   renderer->prefs->color1);
			gdk_window_clear (GDK_ROOT_PARENT ());
		} else {
			gdk_color_alloc (gdk_window_get_colormap
					 (renderer->widget->window), 
					 renderer->prefs->color1);
			gdk_gc_set_foreground (gc, renderer->prefs->color1);
			XFillRectangle (GDK_DISPLAY (), renderer->pixmap, xgc, 
					renderer->x, renderer->y, 
					renderer->width, renderer->height);
		}
	}

	if (renderer->is_root && !renderer->is_set &&
	    (renderer->prefs->wallpaper_enabled || 
	     renderer->prefs->gradient_enabled))
		set_root_pixmap (renderer->pixmap);
	else if (renderer->is_root && !renderer->is_set)
		set_root_pixmap (None);

	gdk_gc_destroy (gc);
}

static guchar *
fill_gradient (gint w, gint h, GdkColor *c1, GdkColor *c2,
	       orientation_t orientation)
{
	gint i, j;
	gint dr, dg, db;
	gint gs1, w3;
	gint vc = (orientation == ORIENTATION_HORIZ || (c1 == c2));
	guchar *b, *row, *d, *buffer;

	buffer = g_new (guchar, w * h * 3);
	d = buffer;

#define R1 c1->red
#define G1 c1->green
#define B1 c1->blue
#define R2 c2->red
#define G2 c2->green
#define B2 c2->blue

	dr = R2 - R1;
	dg = G2 - G1;
	db = B2 - B1;

	gs1 = (orientation == ORIENTATION_VERT) ? h-1 : w-1;
	w3 = w*3;

	row = g_new (guchar, w3);

	if (vc) {
		b = row;
		for (j = 0; j < w; j++) {
			*b++ = (R1 + (j * dr) / gs1) >> 8;
			*b++ = (G1 + (j * dg) / gs1) >> 8;
			*b++ = (B1 + (j * db) / gs1) >> 8;
		}
	}

	for (i = 0; i < h; i++) {
		if (!vc) {
			guchar cr, cg, cb;

			cr = (R1 + (i * dr) / gs1) >> 8;
			cg = (G1 + (i * dg) / gs1) >> 8;
			cb = (B1 + (i * db) / gs1) >> 8;
			b = row;
			for (j = 0; j < w; j++) {
				*b++ = cr;
				*b++ = cg;
				*b++ = cb;
			}
		}
		memcpy (d, row, w3);
		d += w3;
	}

#undef R1
#undef G1
#undef B1
#undef R2
#undef G2
#undef B2

	g_free (row);

	return buffer;
}

static void
get_geometry (wallpaper_type_t wallpaper_type, GdkPixbuf *pixbuf,
	      int dwidth, int dheight,
	      int vwidth, int vheight,
	      int *xoffset, int *yoffset, 
	      int *rwidth, int *rheight,
	      int *srcx, int *srcy) 
{
	gdouble asp, factor;
	gint st = 0;

	switch (wallpaper_type) {
	case WPTYPE_TILED:
		*xoffset = *yoffset = 0;
		/* No break here */

	case WPTYPE_CENTERED:
		if (dwidth != vwidth)
			factor = (gdouble) dwidth / (gdouble) vwidth;
		else
			factor = 1.0;

		/* wallpaper_type could be WPTYPE_TILED too */
		if (vwidth < gdk_pixbuf_get_width (pixbuf) &&
		    wallpaper_type == WPTYPE_CENTERED)
		{
			*srcx = (gdk_pixbuf_get_width (pixbuf) - vwidth) * factor / 2;
			*rwidth = (gdouble) gdk_pixbuf_get_width (pixbuf) * factor;
		}
		else
		{
			*srcx = 0;
			*rwidth = (gdouble) gdk_pixbuf_get_width (pixbuf) * factor;
		}

		if (dheight != vheight)
			factor = (gdouble) dheight / (gdouble) vheight;
		else
			factor = 1.0;

		/* wallpaper_type could be WPTYPE_TILED too */
		if (vheight < gdk_pixbuf_get_height (pixbuf) &&
		    wallpaper_type == WPTYPE_CENTERED)
		{
			*srcy = (gdk_pixbuf_get_height (pixbuf) - vheight) * factor / 2;
			*rheight = gdk_pixbuf_get_height (pixbuf) * factor;
		}
		else
		{
			*srcy = 0;
			*rheight = gdk_pixbuf_get_height (pixbuf) * factor;
		}

		/* wallpaper_type could be WPTYPE_TILED too */
		if (wallpaper_type == WPTYPE_CENTERED) {
			*xoffset = MAX ((dwidth - *rwidth) >> 1, 0);
			*yoffset = MAX ((dheight - *rheight) >> 1, 0);
		}

		break;

	case WPTYPE_SCALED_ASPECT:
		asp = (gdouble) gdk_pixbuf_get_width (pixbuf) / vwidth;

		if (asp < (gdouble) gdk_pixbuf_get_height (pixbuf) / vheight) {
			asp = (gdouble) 
				gdk_pixbuf_get_height (pixbuf) / vheight;
			st = 1;
		}

		if (st) {
			*rwidth = gdk_pixbuf_get_width (pixbuf) / asp / vwidth * dwidth;
			*rheight = dheight;
			*xoffset = (dwidth - *rwidth) >> 1;
			*yoffset = 0;
		} else {
			*rheight = gdk_pixbuf_get_height (pixbuf) / asp / vheight * dheight;
			*rwidth = dwidth;
			*xoffset = 0;
			*yoffset = (dheight - *rheight) >> 1;
		}

		*srcx = *srcy = 0;

		break;

	case WPTYPE_SCALED:
		*rwidth = dwidth;
		*rheight = dheight;
		*xoffset = *yoffset = 0;
		*srcx = *srcy = 0;
		break;

	default:
		g_error ("Bad wallpaper type");
		break;
	}
}

static void
tile_composite (GdkPixbuf *dest, GdkPixbuf *src,
		gdouble sx, gdouble sy,
		gdouble swidth, gdouble sheight, 
		gdouble dwidth, gdouble dheight,
		gdouble scalex, gdouble scaley,
		gint alpha_value) 
{
	gdouble cx, cy;

	for (cy = sy; cy < dheight; cy += sheight)
		for (cx = sx; cx < dwidth; cx += swidth)
			gdk_pixbuf_composite
				(dest, src, cx, cy,
				 MIN (swidth, dwidth - cx), 
				 MIN (sheight, dheight - cy),
				 cx, cy, scalex, scaley,
				 GDK_INTERP_BILINEAR,
				 alpha_value);
}

static void
render_tiled_image (Pixmap pixmap, GC xgc, GdkPixbuf *pixbuf,
		    gint x, gint y, gint dwidth, gint dheight)
{
	gint xoff, yoff;
	gint pwidth, pheight;

	pwidth = gdk_pixbuf_get_width (pixbuf);
	pheight = gdk_pixbuf_get_height (pixbuf);

	for (yoff = 0; yoff < dheight; yoff += pheight)
		for (xoff = 0; xoff < dwidth; xoff += pwidth)
			gdk_pixbuf_xlib_render_to_drawable
				(pixbuf, pixmap, xgc,
				 0, 0, xoff + x, yoff + y, 
				 MIN (pwidth, dwidth - xoff), 
				 MIN (pheight, dheight - yoff),
				 GDK_RGB_DITHER_NORMAL, 0, 0);
}

/* Return TRUE if the gradient should be rendered, false otherwise */

static gboolean
render_gradient_p (Renderer *renderer, Preferences *prefs) 
{
	return prefs->gradient_enabled &&
		!(prefs->wallpaper_enabled &&
		  !prefs->adjust_opacity &&
		  ((prefs->wallpaper_type == WPTYPE_TILED ||
		    prefs->wallpaper_type == WPTYPE_SCALED) ||
		   (renderer->pwidth == renderer->width &&
		    renderer->pheight == renderer->height)));
}

/* Return TRUE if we can optimize the rendering by using a small thin pixmap */

static gboolean
render_small_pixmap_p (Preferences *prefs) 
{
	return prefs->gradient_enabled && !prefs->wallpaper_enabled;
}

/* Create a persistant pixmap. We create a separate display
 * and set the closedown mode on it to RetainPermanent
 */
static Pixmap
make_root_pixmap (gint width, gint height)
{
	Display *display;
	Pixmap result;
	
	display = XOpenDisplay (gdk_display_name);
	XSetCloseDownMode (display, RetainPermanent);

	result = XCreatePixmap (display,
				DefaultRootWindow (display),
				width, height,
				xlib_rgb_get_depth ());

	XCloseDisplay (display);

	return result;
}

/* Set the root pixmap, and properties pointing to it. We
 * do this atomically with XGrabServer to make sure that
 * we won't leak the pixmap if somebody else it setting
 * it at the same time. (This assumes that they follow the
 * same conventions we do)
 */

static void 
set_root_pixmap (Pixmap pixmap) 
{
	GdkAtom type;
	gulong nitems, bytes_after;
	gint format;
	guchar *data_esetroot;

	XGrabServer (GDK_DISPLAY ());

	XGetWindowProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
			    gdk_atom_intern ("ESETROOT_PMAP_ID", FALSE),
			    0L, 1L, False, XA_PIXMAP,
			    &type, &format, &nitems, &bytes_after,
			    &data_esetroot);

	if (type == XA_PIXMAP) {
		if (format == 32 && nitems == 1) {
			Pixmap old_pixmap;

			old_pixmap = *((Pixmap *) data_esetroot);

			if (pixmap != -1 && old_pixmap != pixmap)
				XKillClient(GDK_DISPLAY (), old_pixmap);
			else if (pixmap == -1)
				pixmap = old_pixmap;
		}

		XFree (data_esetroot);
	}

	if (pixmap != None && pixmap != -1) {
		XChangeProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				 gdk_atom_intern ("ESETROOT_PMAP_ID", FALSE),
				 XA_PIXMAP, 32, PropModeReplace,
				 (guchar *) &pixmap, 1);
		XChangeProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				 gdk_atom_intern ("_XROOTPMAP_ID", FALSE),
				 XA_PIXMAP, 32, PropModeReplace,
				 (guchar *) &pixmap, 1);

		XSetWindowBackgroundPixmap (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
					    pixmap);
	} else if (pixmap == None) {
		XDeleteProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				 gdk_atom_intern ("ESETROOT_PMAP_ID", FALSE));
		XDeleteProperty (GDK_DISPLAY (), GDK_ROOT_WINDOW (),
				 gdk_atom_intern ("_XROOTPMAP_ID", FALSE));
	}

	XClearWindow (GDK_DISPLAY (), GDK_ROOT_WINDOW ());
	XUngrabServer (GDK_DISPLAY ());
	XFlush(GDK_DISPLAY ());
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


static void
output_compat_prefs (const Preferences *prefs)
{
	static const gint wallpaper_types[] = { 0, 1, 3, 2 };
	gchar *color;

	gnome_config_pop_prefix ();
	gnome_config_set_bool ("/Background/Default/Enabled", prefs->enabled);
	gnome_config_set_string ("/Background/Default/wallpaper",
				 (prefs->wallpaper_filename) ? prefs->wallpaper_filename : "none");
	gnome_config_set_int ("/Background/Default/wallpaperAlign", wallpaper_types[prefs->wallpaper_type]);

	color = g_strdup_printf ("#%02x%02x%02x",
		prefs->color1->red >> 8,
		prefs->color1->green >> 8,
		prefs->color1->blue >> 8);
	gnome_config_set_string ("/Background/Default/color1", color);
	g_free (color);

	color = g_strdup_printf ("#%02x%02x%02x",
		prefs->color2->red >> 8,
		prefs->color2->green >> 8,
		prefs->color2->blue >> 8);
	gnome_config_set_string ("/Background/Default/color2", color);
	g_free (color);

	gnome_config_set_string ("/Background/Default/simple",
		       		 (prefs->gradient_enabled) ? "gradient" : "solid");
	gnome_config_set_string ("/Background/Default/gradient",
				   (prefs->orientation == ORIENTATION_VERT) ? "vertical" : "horizontal");
	
	gnome_config_set_bool ("/Background/Default/adjustOpacity", prefs->adjust_opacity);
	gnome_config_set_int ("/Background/Default/opacity", prefs->opacity);

	gnome_config_sync ();
}

GdkPixbuf *
applier_get_wallpaper_pixbuf (Applier *applier)
{
	g_return_val_if_fail (applier != NULL, NULL);

	return applier->private->wallpaper_pixbuf;
}
