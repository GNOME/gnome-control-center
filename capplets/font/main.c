/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <stdarg.h>

#ifdef HAVE_XFT2
#include <gdk/gdkx.h>
#include <X11/Xft/Xft.h>
#endif /* HAVE_XFT2 */

#include "theme-common.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"

#ifdef HAVE_XFT2
static void cb_show_details (GtkWidget *button,
			     GtkWindow *parent);
#endif /* HAVE_XFT2 */

#define GTK_FONT_KEY           "/desktop/gnome/interface/font_name"
#define DESKTOP_FONT_KEY       "/apps/nautilus/preferences/desktop_font"
#define WINDOW_TITLE_FONT_KEY  "/desktop/gnome/applications/window_manager/titlebar_font"

#define METACITY_DIR "/apps/metacity/general"
#define WINDOW_TITLE_FONT_KEY METACITY_DIR "/titlebar_font"
#define WINDOW_TITLE_USES_SYSTEM_KEY METACITY_DIR "/titlebar_uses_system_font"
#define MONOSPACE_FONT_KEY "/desktop/gnome/interface/monospace_font_name"

#ifdef HAVE_XFT2
#define FONT_RENDER_DIR "/desktop/gnome/font_rendering"
#define FONT_ANTIALIASING_KEY FONT_RENDER_DIR "/antialiasing"
#define FONT_HINTING_KEY      FONT_RENDER_DIR "/hinting"
#define FONT_RGBA_ORDER_KEY   FONT_RENDER_DIR "/rgba_order"
#define FONT_DPI_KEY          FONT_RENDER_DIR "/dpi"

static gboolean in_change = FALSE;
#endif /* HAVE_XFT2 */

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GLADEDIR "/font-properties.glade", "font_dialog", NULL);

  return dialog;
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"wgoscustdesk.xml",
			"goscustdesk-38");
	else
		gtk_main_quit ();
}

#ifdef HAVE_XFT2

/*
 * Code for displaying previews of font rendering with various Xft options
 */

static void
sample_size_request (GtkWidget      *darea,
		     GtkRequisition *requisition)
{
	GdkPixbuf *pixbuf = g_object_get_data (G_OBJECT (darea), "sample-pixbuf");
	
	requisition->width = gdk_pixbuf_get_width (pixbuf) + 2;
	requisition->height = gdk_pixbuf_get_height (pixbuf) + 2;
}

static void
sample_expose (GtkWidget      *darea,
	       GdkEventExpose  expose)
{
	GdkPixbuf *pixbuf = g_object_get_data (G_OBJECT (darea), "sample-pixbuf");
	int width = gdk_pixbuf_get_width (pixbuf);
	int height = gdk_pixbuf_get_height (pixbuf);
	
	int x = (darea->allocation.width - width) / 2;
	int y = (darea->allocation.height - height) / 2;
	
	gdk_draw_rectangle (darea->window, darea->style->white_gc, TRUE,
			    0, 0,
			    darea->allocation.width, darea->allocation.height);
	gdk_draw_rectangle (darea->window, darea->style->black_gc, FALSE,
			    0, 0,
			    darea->allocation.width - 1, darea->allocation.height - 1);
	
	gdk_pixbuf_render_to_drawable (pixbuf, darea->window, NULL,
				       0, 0, x, y, width, height,
				       GDK_RGB_DITHER_NORMAL, 0, 0);
}

typedef enum {
	ANTIALIAS_NONE,
	ANTIALIAS_GRAYSCALE,
	ANTIALIAS_RGBA
} Antialiasing;

static GConfEnumStringPair antialias_enums[] = {
	{ ANTIALIAS_NONE,      "none" },
	{ ANTIALIAS_GRAYSCALE, "grayscale" },
	{ ANTIALIAS_RGBA,      "rgba" },
	{ -1,                  NULL }
};

typedef enum {
	HINT_NONE,
	HINT_SLIGHT,
	HINT_MEDIUM,
	HINT_FULL
} Hinting;

static GConfEnumStringPair hint_enums[] = {
	{ HINT_NONE,   "none" },
	{ HINT_SLIGHT, "slight" },
	{ HINT_MEDIUM, "medium" },
	{ HINT_FULL,   "full" },
	{ -1,          NULL }
};

typedef enum {
	RGBA_RGB,
	RGBA_BGR,
	RGBA_VRGB,
	RGBA_VBGR
} RgbaOrder;

static GConfEnumStringPair rgba_order_enums[] = {
	{ RGBA_RGB,  "rgb" },
	{ RGBA_BGR,  "bgr" },
	{ RGBA_VRGB, "vrgb" },
	{ RGBA_VBGR, "vbgr" },
	{ -1,         NULL }
};

static XftFont *
open_pattern (FcPattern   *pattern,
	      Antialiasing antialiasing,
	      Hinting      hinting)
{
#ifdef FC_HINT_STYLE
	static const int hintstyles[] = { FC_HINT_NONE, FC_HINT_SLIGHT, FC_HINT_MEDIUM, FC_HINT_FULL };
#endif /* FC_HINT_STYLE */
	
	FcPattern *res_pattern;
	FcResult result;
	XftFont *font;
	
	Display *xdisplay = gdk_x11_get_default_xdisplay ();
	int screen = gdk_x11_get_default_screen ();
	
	res_pattern = XftFontMatch (xdisplay, screen, pattern, &result);
	if (res_pattern == NULL)
		return NULL;
	
	FcPatternDel (res_pattern, FC_HINTING);
	FcPatternAddBool (res_pattern, FC_HINTING, hinting != HINT_NONE);

#ifdef FC_HINT_STYLE	
	FcPatternDel (res_pattern, FC_HINT_STYLE);
	FcPatternAddInteger (res_pattern, FC_HINT_STYLE, hintstyles[hinting]);
#endif /* FC_HINT_STYLE */ 	

	FcPatternDel (res_pattern, FC_ANTIALIAS);
	FcPatternAddBool (res_pattern, FC_ANTIALIAS, antialiasing != ANTIALIAS_NONE);

	FcPatternDel (res_pattern, FC_RGBA);
	FcPatternAddInteger (res_pattern, FC_RGBA,
			     antialiasing == ANTIALIAS_RGBA ? FC_RGBA_RGB : FC_RGBA_NONE);

	FcPatternDel (res_pattern, FC_DPI);
	FcPatternAddInteger (res_pattern, FC_DPI, 96);

	font = XftFontOpenPattern (xdisplay, res_pattern);
	if (!font)
		FcPatternDestroy (res_pattern);

	return font;
}

static void
setup_font_sample (GtkWidget   *darea,
		   Antialiasing antialiasing,
		   Hinting      hinting)
{
	const char *string1 = "abcfgop AO ";
	const char *string2 = "abcfgop";

	XftColor black = { 0, {      0,      0,       0, 0xffff } };
	XftColor white = { 0, { 0xffff, 0xffff,  0xffff, 0xffff } };
	
	Display *xdisplay = gdk_x11_get_default_xdisplay ();
	
	GdkColormap *colormap = gdk_rgb_get_colormap ();
	Colormap xcolormap = GDK_COLORMAP_XCOLORMAP (colormap);

	GdkVisual *visual = gdk_colormap_get_visual (colormap);
	Visual *xvisual = GDK_VISUAL_XVISUAL (visual);

	FcPattern *pattern;
	XftFont *font1, *font2;
	XGlyphInfo extents1 = { 0 };
	XGlyphInfo extents2 = { 0 };
	GdkPixmap *pixmap;
	XftDraw *draw;
	GdkPixbuf *tmp_pixbuf, *pixbuf;

	int width, height;
	int ascent, descent;

	pattern = FcPatternBuild (NULL,
				  FC_FAMILY, FcTypeString, "Serif",
				  FC_SLANT, FcTypeInteger, FC_SLANT_ROMAN,
				  FC_SIZE, FcTypeDouble, 18.,
				  NULL);
	font1 = open_pattern (pattern, antialiasing, hinting);
	FcPatternDestroy (pattern);

	pattern = FcPatternBuild (NULL,
				  FC_FAMILY, FcTypeString, "Serif",
				  FC_SLANT, FcTypeInteger, FC_SLANT_ITALIC,
				  FC_SIZE, FcTypeDouble, 20.,
				  NULL);
	font2 = open_pattern (pattern, antialiasing, hinting);
	FcPatternDestroy (pattern);

	if (font1)
		XftTextExtentsUtf8 (xdisplay, font1, (char *)string1, strlen (string1), &extents1);
	if (font2)
		XftTextExtentsUtf8 (xdisplay, font2, (char *)string2, strlen (string2), &extents2);

	ascent = 0;
	if (font1)
		ascent = MAX (ascent, font1->ascent);
	if (font2)
		ascent = MAX (ascent, font2->ascent);

	descent = 0;
	if (font1)
		descent = MAX (descent, font1->descent);
	if (font2)
		descent = MAX (descent, font2->descent);
	
	width = extents1.xOff + extents2.xOff + 4;
	
	height = ascent + descent + 2;

	pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);
	
	draw = XftDrawCreate (xdisplay, GDK_DRAWABLE_XID (pixmap), xvisual, xcolormap);

	XftDrawRect (draw, &white, 0, 0, width, height);
	if (font1)
		XftDrawStringUtf8 (draw, &black, font1,
				   2, 2 + ascent,
				   (char *)string1, strlen (string1));
	if (font2)
		XftDrawStringUtf8 (draw, &black, font2,
				   2 + extents1.xOff, 2 + ascent,
				   (char *)string2, strlen (string2));
	
	XftDrawDestroy (draw);

	if (font1)
		XftFontClose (xdisplay, font1);
	if (font2)
		XftFontClose (xdisplay, font2);

	tmp_pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap, 0, 0, 0, 0, width, height);
	pixbuf = gdk_pixbuf_scale_simple (tmp_pixbuf, 1 * width, 1 * height, GDK_INTERP_TILES);

	g_object_unref (pixmap);
	g_object_unref (tmp_pixbuf);

	g_object_set_data_full (G_OBJECT (darea), "sample-pixbuf",
				pixbuf, (GDestroyNotify)g_object_unref);

	g_signal_connect (darea, "size_request", G_CALLBACK (sample_size_request), NULL);
	g_signal_connect (darea, "expose_event", G_CALLBACK (sample_expose), NULL);
}

/*
 * Code implementing a group of radio buttons with different Xft option combinations.
 * If one of the buttons is matched by the GConf key, we pick it. Otherwise we
 * show the group as inconsistent.
 */
static void
font_render_get_gconf (Antialiasing *antialiasing,
		       Hinting      *hinting)
{
	GConfClient *client = gconf_client_get_default ();
	char *antialias_str = gconf_client_get_string (client, FONT_ANTIALIASING_KEY, NULL);
	char *hint_str = gconf_client_get_string (client, FONT_HINTING_KEY, NULL);
	int val;

	val = ANTIALIAS_GRAYSCALE;
	if (antialias_str)
		gconf_string_to_enum (antialias_enums, antialias_str, &val);
	*antialiasing = val;

	val = HINT_FULL;
	if (hint_str)
		gconf_string_to_enum (hint_enums, hint_str, &val);
	*hinting = val;

	g_object_unref (client);
}

typedef struct {
	Antialiasing antialiasing;
	Hinting hinting;
	GtkWidget *radio;
} FontPair;

static GSList *font_pairs = NULL;

static void
font_render_load (void)
{
	Antialiasing antialiasing;
	Hinting hinting;
	gboolean inconsistent = TRUE;
	GSList *tmp_list;
	
	font_render_get_gconf (&antialiasing, &hinting);

	in_change = TRUE;
	
	for (tmp_list = font_pairs; tmp_list; tmp_list = tmp_list->next) {
		FontPair *pair = tmp_list->data;

		if (antialiasing == pair->antialiasing && hinting == pair->hinting) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pair->radio), TRUE);
			inconsistent = FALSE;
		}
	}

	for (tmp_list = font_pairs; tmp_list; tmp_list = tmp_list->next) {
		FontPair *pair = tmp_list->data;

		gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (pair->radio), inconsistent);
	}
	
	in_change = FALSE;
}

static void
font_render_changed (GConfClient *client,
                     guint        cnxn_id,
                     GConfEntry  *entry,
                     gpointer     user_data)
{
	font_render_load ();
}

static void
font_radio_toggled (GtkToggleButton *toggle_button,
		    FontPair        *pair)
{
	if (!in_change) {
		GConfClient *client = gconf_client_get_default ();

		gconf_client_set_string (client, FONT_ANTIALIASING_KEY,
					 gconf_enum_to_string (antialias_enums, pair->antialiasing),
					 NULL);
		gconf_client_set_string (client, FONT_HINTING_KEY, 
					 gconf_enum_to_string (hint_enums, pair->hinting),
					 NULL);

		g_object_unref (client);
	}

	/* Restore back to the previous state until we get notification
	 */
	font_render_load ();
}

static void
setup_font_pair (GtkWidget    *radio,
		 GtkWidget    *darea,
		 Antialiasing  antialiasing,
		 Hinting       hinting)
{
	FontPair *pair = g_new (FontPair, 1);

	pair->antialiasing = antialiasing;
	pair->hinting = hinting;
	pair->radio = radio;

	setup_font_sample (darea, antialiasing, hinting);
	font_pairs = g_slist_prepend (font_pairs, pair);

	g_signal_connect (radio, "toggled",
			  G_CALLBACK (font_radio_toggled), pair);
}
#endif /* HAVE_XFT2 */

static void
metacity_titlebar_load_sensitivity (GConfClient *client,
				    GladeXML    *dialog)
{
	gtk_widget_set_sensitive (WID ("window_title_font"),
				  !gconf_client_get_bool (client,
							  WINDOW_TITLE_USES_SYSTEM_KEY,
							  NULL));
}

static void
metacity_changed (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
	if (strcmp (entry->key, WINDOW_TITLE_USES_SYSTEM_KEY) == 0)
		metacity_titlebar_load_sensitivity (client, user_data);
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *widget;
  GObject *peditor;

  client = gconf_client_get_default ();

  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (client, METACITY_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
#ifdef HAVE_XFT2  
  gconf_client_add_dir (client, FONT_RENDER_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
#endif  /* HAVE_XFT2 */

  peditor = gconf_peditor_new_font (NULL, GTK_FONT_KEY,
		  		    WID ("application_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  peditor = gconf_peditor_new_font (NULL, DESKTOP_FONT_KEY,
		  		    WID ("desktop_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  peditor = gconf_peditor_new_font (NULL, WINDOW_TITLE_FONT_KEY,
		  		    WID ("window_title_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  peditor = gconf_peditor_new_font (NULL, MONOSPACE_FONT_KEY,
		  		    WID ("monospace_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  gconf_client_notify_add (client, METACITY_DIR,
			   metacity_changed,
			   dialog, NULL, NULL);

  metacity_titlebar_load_sensitivity (client, dialog);

  widget = WID ("font_dialog");
  capplet_set_icon (widget, "font-capplet.png");

#ifdef HAVE_XFT2  
  setup_font_pair (WID ("monochrome_radio"),    WID ("monochrome_sample"),    ANTIALIAS_NONE,      HINT_FULL);
  setup_font_pair (WID ("best_shapes_radio"),   WID ("best_shapes_sample"),   ANTIALIAS_GRAYSCALE, HINT_MEDIUM);
  setup_font_pair (WID ("best_contrast_radio"), WID ("best_contrast_sample"), ANTIALIAS_GRAYSCALE, HINT_FULL);
  setup_font_pair (WID ("subpixel_radio"),      WID ("subpixel_sample"),      ANTIALIAS_RGBA,      HINT_FULL);
  
  font_render_load ();
  
  gconf_client_notify_add (client, FONT_RENDER_DIR,
			   font_render_changed,
			   NULL, NULL, NULL);

  g_signal_connect (WID ("details_button"), 
 		    "clicked",
 		    G_CALLBACK (cb_show_details), widget);
#else /* !HAVE_XFT2 */
  gtk_widget_hide (WID ("font_render_frame"));
#endif /* HAVE_XFT2 */
  
  g_signal_connect (G_OBJECT (widget),
    "response",
    G_CALLBACK (cb_dialog_response), NULL);
 
  gtk_widget_show (widget);

  g_object_unref (client);
}

#ifdef HAVE_XFT2 
/*
 * EnumGroup - a group of radio buttons tied to a string enumeration
 *             value. We add this here because the gconf peditor
 *             equivalent of this is both painful to use (you have
 *             to supply functions to convert from enums to indices)
 *             and conceptually broken (the order of radio buttons
 *             in a group when using Glade is not predictable.
 */
typedef struct
{
	GConfClient *client;
	GSList *items;
	const gchar *gconf_key;
	GConfEnumStringPair *enums;
	int default_value;
} EnumGroup;

typedef struct
{
	EnumGroup *group;
	GtkWidget *widget;
	int value;
} EnumItem;

static void
enum_group_load (EnumGroup *group)
{
	char *str = gconf_client_get_string (group->client, group->gconf_key, NULL);
	int val = group->default_value;
	GSList *tmp_list;

	if (str)
		gconf_string_to_enum (group->enums, str, &val);

	in_change = TRUE;
	
	for (tmp_list = group->items; tmp_list; tmp_list = tmp_list->next) {
		EnumItem *item = tmp_list->data;

		if (val == item->value)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (item->widget), TRUE);
	}

	

	in_change = FALSE;
}

static void
enum_group_changed (GConfClient *client,
		    guint        cnxn_id,
		    GConfEntry  *entry,
		    gpointer     user_data)
{
	enum_group_load (user_data);
}

static void
enum_item_toggled (GtkToggleButton *toggle_button,
		   EnumItem      *item)
{
	EnumGroup *group = item->group;
	
	if (!in_change) {
		gconf_client_set_string (group->client, group->gconf_key,
					 gconf_enum_to_string (group->enums, item->value),
					 NULL);
	}

	/* Restore back to the previous state until we get notification
	 */
	enum_group_load (group);
}

static EnumGroup *
enum_group_create (const gchar         *gconf_key,
		   GConfEnumStringPair *enums,
		   int                  default_value,
		   GtkWidget           *first_widget,
		   ...)
{
	EnumGroup *group;
	GtkWidget *widget;
	va_list args;

	group = g_new (EnumGroup, 1);

	group->client = gconf_client_get_default ();
	group->gconf_key = g_strdup (gconf_key);
	group->enums = enums;
	group->default_value = default_value;
	group->items = NULL;
	
	va_start (args, first_widget);

	widget = first_widget;	
	while (widget)
	{
		EnumItem *item;

		item = g_new (EnumItem, 1);
		item->group = group;
		item->widget = widget;
		item->value = va_arg (args, int);

		g_signal_connect (item->widget, "toggled",
				  G_CALLBACK (enum_item_toggled), item);
		
		group->items = g_slist_prepend (group->items, item);

		widget = va_arg (args, GtkWidget *);
	}

	va_end (args);

	enum_group_load (group);

	gconf_client_notify_add (group->client, gconf_key,
				 enum_group_changed,
				 group, NULL, NULL);

 	return group;
}

/*
 * The font rendering details dialog
 */
static void
dpi_load (GConfClient   *client,
	  GtkSpinButton *spinner)
{
	gdouble dpi = gconf_client_get_float (client, FONT_DPI_KEY, NULL);

	in_change = TRUE;
	gtk_spin_button_set_value (spinner, dpi);
	in_change = FALSE;
}

static void
dpi_changed (GConfClient *client,
	     guint        cnxn_id,
	     GConfEntry  *entry,
	     gpointer     user_data)
{
	dpi_load (client, user_data);
}

static void
dpi_value_changed (GtkSpinButton *spinner,
		   GConfClient   *client)
{
	/* Like any time when using a spin button with GConf, there is
	 * a race condition here. When we change, we send the new
	 * value to GCOnf, then restore to the old value until
	 * we get a response to emulate the proper model/view behavior.
	 *
	 * If the user changes the value faster than responses are
	 * received from GConf, this may cause mild strange effects.
	 */
	gdouble new_dpi = gtk_spin_button_get_value (spinner);

	gconf_client_set_float (client, FONT_DPI_KEY, new_dpi, NULL);
	
	dpi_load (client, spinner);
}

static void
cb_details_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			      "wgoscustdesk.xml",
			      "goscustdesk-38");
	else
		gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
cb_show_details (GtkWidget *button,
		 GtkWindow *parent)
{
	static GtkWidget *details_dialog = NULL;

	if (!details_dialog) {
		GConfClient *client = gconf_client_get_default ();
		GladeXML *dialog = glade_xml_new (GLADEDIR "/font-properties.glade", "render_details", NULL);
		GtkWidget *dpi_spinner;

		details_dialog = WID ("render_details");

		gtk_window_set_transient_for (GTK_WINDOW (details_dialog), parent);

		dpi_spinner = WID ("dpi_spinner");
		dpi_load (client, GTK_SPIN_BUTTON (dpi_spinner));
		g_signal_connect (dpi_spinner, "value_changed",
				  G_CALLBACK (dpi_value_changed), client);

		gconf_client_notify_add (client, FONT_DPI_KEY,
					 dpi_changed,
					 dpi_spinner, NULL, NULL);
		
		setup_font_sample (WID ("antialias_none_sample"),      ANTIALIAS_NONE,      HINT_FULL);
		setup_font_sample (WID ("antialias_grayscale_sample"), ANTIALIAS_GRAYSCALE, HINT_FULL);
		setup_font_sample (WID ("antialias_subpixel_sample"),  ANTIALIAS_RGBA,      HINT_FULL);

		enum_group_create (FONT_ANTIALIASING_KEY, antialias_enums, ANTIALIAS_GRAYSCALE,
				     WID ("antialias_none_radio"), ANTIALIAS_NONE,
				     WID ("antialias_grayscale_radio"), ANTIALIAS_GRAYSCALE,
				     WID ("antialias_subpixel_radio"), ANTIALIAS_RGBA,
				     NULL);

		setup_font_sample (WID ("hint_none_sample"),    ANTIALIAS_GRAYSCALE, HINT_NONE);
		setup_font_sample (WID ("hint_slight_sample"),  ANTIALIAS_GRAYSCALE, HINT_SLIGHT);
		setup_font_sample (WID ("hint_medium_sample"),  ANTIALIAS_GRAYSCALE, HINT_MEDIUM);
		setup_font_sample (WID ("hint_full_sample"),    ANTIALIAS_GRAYSCALE, HINT_FULL);
		
		enum_group_create (FONT_HINTING_KEY, hint_enums, HINT_FULL,
				   WID ("hint_none_radio"),   HINT_NONE,
				   WID ("hint_slight_radio"), HINT_SLIGHT,
				   WID ("hint_medium_radio"), HINT_MEDIUM,
				   WID ("hint_full_radio"),   HINT_FULL,
				   NULL);

		gtk_image_set_from_file (GTK_IMAGE (WID ("subpixel_rgb_image")),
					 PIXMAPDIR "/subpixel-rgb.png");
		gtk_image_set_from_file (GTK_IMAGE (WID ("subpixel_bgr_image")),
					 PIXMAPDIR "/subpixel-bgr.png");
		gtk_image_set_from_file (GTK_IMAGE (WID ("subpixel_vrgb_image")),
					 PIXMAPDIR "/subpixel-vrgb.png");
		gtk_image_set_from_file (GTK_IMAGE (WID ("subpixel_vbgr_image")),
					 PIXMAPDIR "/subpixel-vbgr.png");

		enum_group_create (FONT_RGBA_ORDER_KEY, rgba_order_enums, RGBA_RGB,
				   WID ("subpixel_rgb_radio"),  RGBA_RGB,
				   WID ("subpixel_bgr_radio"),  RGBA_BGR,
				   WID ("subpixel_vrgb_radio"), RGBA_VRGB,
				   WID ("subpixel_vbgr_radio"), RGBA_VBGR,
				   NULL);
		
		g_signal_connect (G_OBJECT (details_dialog),
				  "response",
				  G_CALLBACK (cb_details_response), NULL);
		g_signal_connect (G_OBJECT (details_dialog),
				  "delete_event",
				  G_CALLBACK (gtk_true), NULL);

		g_object_unref (client);
	}

	gtk_window_present (GTK_WINDOW (details_dialog));
}
#endif /* HAVE_XFT2 */
  
int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-font-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  activate_settings_daemon ();

  dialog = create_dialog ();
  setup_dialog (dialog);

  gtk_main ();

  return 0;
}
