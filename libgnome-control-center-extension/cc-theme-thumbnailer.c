/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002 Jonathan Blandford
 * Copyright (C) 2010 William Jon McCann
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <metacity-private/util.h>
#include <metacity-private/theme.h>
#include <metacity-private/theme-parser.h>
#include <metacity-private/preview-widget.h>

/* We have to #undef this as metacity #defines these. */
#undef _
#undef N_

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-object.h>

#include "cc-theme-thumbnailer.h"
#include "gtkrc-utils.h"

#define CC_THEME_THUMBNAILER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_THEME_THUMBNAILER, CcThemeThumbnailerPrivate))

#define THUMBNAIL_TYPE_META     "meta"
#define THUMBNAIL_TYPE_GTK      "gtk"
#define THUMBNAIL_TYPE_METACITY "metacity"
#define THUMBNAIL_TYPE_ICON     "icon"

#define META_THUMBNAIL_SIZE       128
#define GTK_THUMBNAIL_SIZE         96
#define METACITY_THUMBNAIL_WIDTH  120
#define METACITY_THUMBNAIL_HEIGHT  60

typedef struct
{
        int         pipe_to_master;
        int         status;
        GByteArray *type;
        GByteArray *control_theme_name;
        GByteArray *gtk_color_scheme;
        GByteArray *wm_theme_name;
        GByteArray *icon_theme_name;
        GByteArray *application_font;
} ThemeThumbnailData;

typedef struct
{
        char                *thumbnail_type;
        gpointer             theme_info;
        CcThemeThumbnailFunc func;
        gpointer             user_data;
        GDestroyNotify       destroy;
} ThemeQueueItem;

struct CcThemeThumbnailerPrivate
{
        GPid                 child_pid;
        int                  pipe_to_factory;
        int                  pipe_from_factory;

        GList               *theme_queue;

        /* async data */
        gboolean             set;
        int                  thumbnail_width;
        int                  thumbnail_height;
        GByteArray          *data;
        char                *theme_name;
        CcThemeThumbnailFunc func;
        gpointer             user_data;
        GDestroyNotify       destroy;
        GIOChannel          *channel;
        guint                watch_id;
};

enum {
        PROP_0,
};

static void     cc_theme_thumbnailer_class_init  (CcThemeThumbnailerClass *klass);
static void     cc_theme_thumbnailer_init        (CcThemeThumbnailer      *theme_thumbnailer);
static void     cc_theme_thumbnailer_finalize    (GObject         *object);

static gpointer theme_thumbnailer_object = NULL;

G_DEFINE_TYPE (CcThemeThumbnailer, cc_theme_thumbnailer, G_TYPE_OBJECT)

/* Protocol */

/* Our protocol is pretty simple.  The parent process will write several strings
 * (separated by a '\000'). They are the widget theme, the wm theme, the icon
 * theme, etc.  Then, it will wait for the child to write back the data.  The
 * parent expects ICON_SIZE_WIDTH * ICON_SIZE_HEIGHT * 4 bytes of information.
 * After that, the child is ready for the next theme to render.
 */

enum {
        READY_FOR_THEME,
        READING_TYPE,
        READING_CONTROL_THEME_NAME,
        READING_GTK_COLOR_SCHEME,
        READING_WM_THEME_NAME,
        READING_ICON_THEME_NAME,
        READING_APPLICATION_FONT,
        WRITING_PIXBUF_DATA
};

GQuark
cc_theme_thumbnailer_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("cc_theme_thumbnailer_error");
        }

        return ret;
}

static void
cc_theme_thumbnailer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue  *value,
                                   GParamSpec    *pspec)
{
        CcThemeThumbnailer *self;

        self = CC_THEME_THUMBNAILER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_theme_thumbnailer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        CcThemeThumbnailer *self;

        self = CC_THEME_THUMBNAILER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
fake_expose_widget (GtkWidget    *widget,
                    GdkPixmap    *pixmap,
                    GdkRectangle *area)
{
        GdkWindow     *tmp_window;
        GdkEventExpose event;

        event.type = GDK_EXPOSE;
        event.window = pixmap;
        event.send_event = FALSE;
        event.area = area ? *area : widget->allocation;
        event.region = NULL;
        event.count = 0;

        tmp_window = widget->window;
        widget->window = pixmap;
        gtk_widget_send_expose (widget, (GdkEvent *) &event);
        widget->window = tmp_window;
}

static void
hbox_foreach (GtkWidget *widget,
              gpointer   data)
{
        if (GTK_WIDGET_VISIBLE (widget)) {
                gtk_widget_realize (widget);
                gtk_widget_map (widget);
                gtk_widget_ensure_style (widget);
                fake_expose_widget (widget, (GdkPixmap *) data, NULL);
        }
}

static void
pixbuf_apply_mask_region (GdkPixbuf *pixbuf,
                          GdkRegion *region)
{
        int nchannels, rowstride, w, h;
        guchar *pixels, *p;

        g_return_if_fail (pixbuf);
        g_return_if_fail (region);

        nchannels = gdk_pixbuf_get_n_channels (pixbuf);
        rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        pixels = gdk_pixbuf_get_pixels (pixbuf);


        /* we need an alpha channel ... */
        if (!gdk_pixbuf_get_has_alpha (pixbuf) || nchannels != 4)
                return;

        for (w = 0; w < gdk_pixbuf_get_width (pixbuf); ++w) {
                for (h = 0; h < gdk_pixbuf_get_height (pixbuf); ++h) {
                        if (!gdk_region_point_in (region, w, h)) {
                                p = pixels + h * rowstride + w * nchannels;
                                if (G_BYTE_ORDER == G_BIG_ENDIAN)
                                        p[0] = 0x0;
                                else
                                        p[3] = 0x0;
                        }
                }
        }
}

static GdkPixbuf *
create_folder_icon (char *icon_theme_name)
{
        GtkIconTheme *icon_theme;
        GdkPixbuf    *folder_icon = NULL;
        GtkIconInfo  *folder_icon_info;
        char         *example_icon_name;
        const char   *icon_names[5];
        int           i;

        icon_theme = gtk_icon_theme_new ();
        gtk_icon_theme_set_custom_theme (icon_theme, icon_theme_name);

        i = 0;
        /* Get the Example icon name in the theme if specified */
        example_icon_name = gtk_icon_theme_get_example_icon_name (icon_theme);
        if (example_icon_name != NULL)
                icon_names[i++] = example_icon_name;
        icon_names[i++] = "x-directory-normal";
        icon_names[i++] = "gnome-fs-directory";
        icon_names[i++] = "folder";
        icon_names[i++] = NULL;

        folder_icon_info = gtk_icon_theme_choose_icon (icon_theme, icon_names, 48, GTK_ICON_LOOKUP_FORCE_SIZE);
        if (folder_icon_info != NULL) {
                folder_icon = gtk_icon_info_load_icon (folder_icon_info, NULL);
                gtk_icon_info_free (folder_icon_info);
        }

        g_object_unref (icon_theme);
        g_free (example_icon_name);

        /* render the icon to the thumbnail */
        if (folder_icon == NULL) {
                GtkWidget *dummy;
                dummy = gtk_label_new ("");

                folder_icon = gtk_widget_render_icon (dummy,
                                                      GTK_STOCK_MISSING_IMAGE,
                                                      GTK_ICON_SIZE_DIALOG,
                                                      NULL);

                gtk_widget_destroy (dummy);
        }

        return folder_icon;
}

static GdkPixbuf *
create_meta_theme_pixbuf (ThemeThumbnailData *data)
{
        GtkWidget *window;
        GtkWidget *preview;
        GtkWidget *vbox;
        GtkWidget *align;
        GtkWidget *box;
        GtkWidget *stock_button;
        GtkWidget *checkbox;
        GtkWidget *radio;

        GtkRequisition requisition;
        GtkAllocation  allocation;
        GdkPixmap     *pixmap;
        GdkVisual     *visual;
        MetaFrameFlags flags;
        MetaTheme     *theme;
        GdkPixbuf     *pixbuf, *icon;
        int            icon_width, icon_height;
        GdkRegion     *region;

        g_object_set (gtk_settings_get_default (),
                      "gtk-theme-name", (char *) data->control_theme_name->data,
                      "gtk-font-name", (char *) data->application_font->data,
                      "gtk-icon-theme-name", (char *) data->icon_theme_name->data,
                      "gtk-color-scheme", (char *) data->gtk_color_scheme->data,
                      NULL);

        theme = meta_theme_load ((char *) data->wm_theme_name->data, NULL);
        if (theme == NULL)
                return NULL;

        /* Represent the icon theme */
        icon = create_folder_icon ((char *) data->icon_theme_name->data);
        icon_width = gdk_pixbuf_get_width (icon);
        icon_height = gdk_pixbuf_get_height (icon);

        /* Create a fake window */
        flags = META_FRAME_ALLOWS_DELETE |
                META_FRAME_ALLOWS_MENU |
                META_FRAME_ALLOWS_MINIMIZE |
                META_FRAME_ALLOWS_MAXIMIZE |
                META_FRAME_ALLOWS_VERTICAL_RESIZE |
                META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
                META_FRAME_HAS_FOCUS |
                META_FRAME_ALLOWS_SHADE |
                META_FRAME_ALLOWS_MOVE;

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        preview = meta_preview_new ();
        gtk_container_add (GTK_CONTAINER (window), preview);
        gtk_widget_realize (window);
        gtk_widget_realize (preview);
        vbox = gtk_vbox_new (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
        gtk_container_add (GTK_CONTAINER (preview), vbox);
        align = gtk_alignment_new (0, 0, 0.0, 0.0);
        gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
        stock_button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
        gtk_container_add (GTK_CONTAINER (align), stock_button);
        box = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
        checkbox = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
        gtk_box_pack_start (GTK_BOX (box), checkbox, FALSE, FALSE, 0);
        radio = gtk_radio_button_new (NULL);
        gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

        gtk_widget_show_all (preview);
        gtk_widget_realize (stock_button);
        gtk_widget_realize (GTK_BIN (stock_button)->child);
        gtk_widget_realize (checkbox);
        gtk_widget_realize (radio);
        gtk_widget_map (stock_button);
        gtk_widget_map (GTK_BIN (stock_button)->child);
        gtk_widget_map (checkbox);
        gtk_widget_map (radio);

        meta_preview_set_frame_flags (META_PREVIEW (preview), flags);
        meta_preview_set_theme (META_PREVIEW (preview), theme);
        meta_preview_set_title (META_PREVIEW (preview), "");

        gtk_window_set_default_size (GTK_WINDOW (window), META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE);

        gtk_widget_size_request (window, &requisition);
        allocation.x = 0;
        allocation.y = 0;
        allocation.width = META_THUMBNAIL_SIZE;
        allocation.height = META_THUMBNAIL_SIZE;
        gtk_widget_size_allocate (window, &allocation);
        gtk_widget_size_request (window, &requisition);

        /* Create a pixmap */
        visual = gtk_widget_get_visual (window);
        pixmap = gdk_pixmap_new (NULL, META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE, visual->depth);
        gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), gtk_widget_get_colormap (window));

        /* Draw the window */
        gtk_widget_ensure_style (window);
        g_assert (window->style);
        g_assert (window->style->font_desc);

        fake_expose_widget (window, pixmap, NULL);
        fake_expose_widget (preview, pixmap, NULL);
        /* we call this again here because the preview sometimes draws into the area
         * of the contents, see http://bugzilla.gnome.org/show_bug.cgi?id=351389 */
        fake_expose_widget (window, pixmap, &vbox->allocation);
        fake_expose_widget (stock_button, pixmap, NULL);
        gtk_container_foreach (GTK_CONTAINER (GTK_BIN (GTK_BIN (stock_button)->child)->child),
                               hbox_foreach,
                               pixmap);
        fake_expose_widget (GTK_BIN (stock_button)->child, pixmap, NULL);
        fake_expose_widget (checkbox, pixmap, NULL);
        fake_expose_widget (radio, pixmap, NULL);

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE);
        gdk_pixbuf_get_from_drawable (pixbuf, pixmap, NULL, 0, 0, 0, 0, META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE);

        /* Add the icon theme to the pixbuf */
        gdk_pixbuf_composite (icon, pixbuf,
                              vbox->allocation.x + vbox->allocation.width - icon_width - 5,
                              vbox->allocation.y + vbox->allocation.height - icon_height - 5,
                              icon_width, icon_height,
                              vbox->allocation.x + vbox->allocation.width - icon_width - 5,
                              vbox->allocation.y + vbox->allocation.height - icon_height - 5,
                              1.0, 1.0, GDK_INTERP_BILINEAR, 255);
        region = meta_preview_get_clip_region (META_PREVIEW (preview),
                                               META_THUMBNAIL_SIZE, META_THUMBNAIL_SIZE);
        pixbuf_apply_mask_region (pixbuf, region);
        gdk_region_destroy (region);

        g_object_unref (icon);
        gtk_widget_destroy (window);
        meta_theme_free (theme);

        return pixbuf;
}

static GdkPixbuf *
create_gtk_theme_pixbuf (ThemeThumbnailData *data)
{
        GtkSettings   *settings;
        GtkWidget     *window, *vbox, *box, *stock_button, *checkbox, *radio;
        GtkRequisition requisition;
        GtkAllocation  allocation;
        GdkVisual     *visual;
        GdkPixmap     *pixmap;
        GdkPixbuf     *pixbuf, *retval;
        int            width, height;

        settings = gtk_settings_get_default ();
        g_object_set (settings,
                      "gtk-theme-name", (char *) data->control_theme_name->data,
                      "gtk-color-scheme", (char *) data->gtk_color_scheme->data,
                      NULL);

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

        vbox = gtk_vbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (window), vbox);
        box = gtk_hbox_new (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (box), 6);
        gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, FALSE, 0);
        stock_button = gtk_button_new_from_stock (GTK_STOCK_OPEN);
        gtk_box_pack_start (GTK_BOX (box), stock_button, FALSE, FALSE, 0);
        checkbox = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox), TRUE);
        gtk_box_pack_start (GTK_BOX (box), checkbox, FALSE, FALSE, 0);
        radio = gtk_radio_button_new_from_widget (NULL);
        gtk_box_pack_start (GTK_BOX (box), radio, FALSE, FALSE, 0);

        gtk_widget_show_all (vbox);
        gtk_widget_realize (stock_button);
        gtk_widget_realize (GTK_BIN (stock_button)->child);
        gtk_widget_realize (checkbox);
        gtk_widget_realize (radio);
        gtk_widget_map (stock_button);
        gtk_widget_map (GTK_BIN (stock_button)->child);
        gtk_widget_map (checkbox);
        gtk_widget_map (radio);

        gtk_widget_size_request (window, &requisition);
        allocation.x = 0;
        allocation.y = 0;
        allocation.width = requisition.width;
        allocation.height = requisition.height;
        gtk_widget_size_allocate (window, &allocation);
        gtk_widget_size_request (window, &requisition);

        /* Draw the window */
        gtk_widget_ensure_style (window);
        g_assert (window->style);
        g_assert (window->style->font_desc);

        gtk_window_get_size (GTK_WINDOW (window), &width, &height);

        visual = gtk_widget_get_visual (window);
        pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);
        gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), gtk_widget_get_colormap (window));

        fake_expose_widget (window, pixmap, NULL);
        fake_expose_widget (stock_button, pixmap, NULL);
        gtk_container_foreach (GTK_CONTAINER (GTK_BIN (GTK_BIN (stock_button)->child)->child),
                               hbox_foreach,
                               pixmap);
        fake_expose_widget (GTK_BIN (stock_button)->child, pixmap, NULL);
        fake_expose_widget (checkbox, pixmap, NULL);
        fake_expose_widget (radio, pixmap, NULL);

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
        gdk_pixbuf_get_from_drawable (pixbuf, pixmap, NULL, 0, 0, 0, 0, width, height);

        retval = gdk_pixbuf_scale_simple (pixbuf,
                                          GTK_THUMBNAIL_SIZE,
                                          (int) GTK_THUMBNAIL_SIZE * (((double) height) / ((double) width)),
                                          GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
        gtk_widget_destroy (window);

        return retval;
}

static GdkPixbuf *
create_metacity_theme_pixbuf (ThemeThumbnailData *data)
{
        GtkWidget     *window, *preview, *dummy;
        MetaFrameFlags flags;
        MetaTheme     *theme;
        GtkRequisition requisition;
        GtkAllocation  allocation;
        GdkVisual     *visual;
        GdkPixmap     *pixmap;
        GdkPixbuf     *pixbuf, *retval;
        GdkRegion     *region;

        theme = meta_theme_load ((char *) data->wm_theme_name->data, NULL);
        if (theme == NULL)
                return NULL;

        flags = META_FRAME_ALLOWS_DELETE |
                META_FRAME_ALLOWS_MENU |
                META_FRAME_ALLOWS_MINIMIZE |
                META_FRAME_ALLOWS_MAXIMIZE |
                META_FRAME_ALLOWS_VERTICAL_RESIZE |
                META_FRAME_ALLOWS_HORIZONTAL_RESIZE |
                META_FRAME_HAS_FOCUS |
                META_FRAME_ALLOWS_SHADE |
                META_FRAME_ALLOWS_MOVE;

        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        gtk_window_set_default_size (GTK_WINDOW (window),
                                     (int) METACITY_THUMBNAIL_WIDTH * 1.2,
                                     (int) METACITY_THUMBNAIL_HEIGHT * 1.2);

        preview = meta_preview_new ();
        meta_preview_set_frame_flags (META_PREVIEW (preview), flags);
        meta_preview_set_theme (META_PREVIEW (preview), theme);
        meta_preview_set_title (META_PREVIEW (preview), "");
        gtk_container_add (GTK_CONTAINER (window), preview);

        dummy = gtk_label_new ("");
        gtk_container_add (GTK_CONTAINER (preview), dummy);

        gtk_widget_realize (window);
        gtk_widget_realize (preview);
        gtk_widget_realize (dummy);
        gtk_widget_show_all (preview);
        gtk_widget_map (dummy);

        gtk_widget_size_request (window, &requisition);
        allocation.x = 0;
        allocation.y = 0;
        allocation.width = (int) METACITY_THUMBNAIL_WIDTH * 1.2;
        allocation.height = (int) METACITY_THUMBNAIL_HEIGHT * 1.2;
        gtk_widget_size_allocate (window, &allocation);
        gtk_widget_size_request (window, &requisition);

        /* Draw the window */
        gtk_widget_ensure_style (window);
        g_assert (window->style);
        g_assert (window->style->font_desc);

        /* Create a pixmap */
        visual = gtk_widget_get_visual (window);
        pixmap = gdk_pixmap_new (NULL,
                                 (int) METACITY_THUMBNAIL_WIDTH * 1.2,
                                 (int) METACITY_THUMBNAIL_HEIGHT * 1.2,
                                 visual->depth);
        gdk_drawable_set_colormap (GDK_DRAWABLE (pixmap), gtk_widget_get_colormap (window));

        fake_expose_widget (window, pixmap, NULL);
        fake_expose_widget (preview, pixmap, NULL);
        fake_expose_widget (window, pixmap, &dummy->allocation);

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                 TRUE,
                                 8,
                                 (int) METACITY_THUMBNAIL_WIDTH * 1.2,
                                 (int) METACITY_THUMBNAIL_HEIGHT * 1.2);
        gdk_pixbuf_get_from_drawable (pixbuf,
                                      pixmap,
                                      NULL,
                                      0, 0, 0, 0,
                                      (int) METACITY_THUMBNAIL_WIDTH * 1.2,
                                      (int) METACITY_THUMBNAIL_HEIGHT * 1.2);

        region = meta_preview_get_clip_region (META_PREVIEW (preview),
                                               METACITY_THUMBNAIL_WIDTH * 1.2,
                                               METACITY_THUMBNAIL_HEIGHT * 1.2);
        pixbuf_apply_mask_region (pixbuf, region);
        gdk_region_destroy (region);


        retval = gdk_pixbuf_scale_simple (pixbuf,
                                          METACITY_THUMBNAIL_WIDTH,
                                          METACITY_THUMBNAIL_HEIGHT,
                                          GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);

        gtk_widget_destroy (window);
        meta_theme_free (theme);
        return retval;
}

static GdkPixbuf *
create_icon_theme_pixbuf (ThemeThumbnailData *data)
{
        return create_folder_icon ((char *) data->icon_theme_name->data);
}


static void
handle_bytes (const char         *buffer,
              int                 bytes_read,
              ThemeThumbnailData *theme_thumbnail_data)
{
        const guint8 *ptr;

        ptr = (guint8 *)buffer;

        while (bytes_read > 0) {
                guint8 *nil;

                switch (theme_thumbnail_data->status) {
                case READY_FOR_THEME:
                        theme_thumbnail_data->status = READING_TYPE;
                        /* fall through */
                case READING_TYPE:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (theme_thumbnail_data->type,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (theme_thumbnail_data->type,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                theme_thumbnail_data->status = READING_CONTROL_THEME_NAME;
                        }
                        break;

                case READING_CONTROL_THEME_NAME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (theme_thumbnail_data->control_theme_name,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (theme_thumbnail_data->control_theme_name,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                theme_thumbnail_data->status = READING_GTK_COLOR_SCHEME;
                        }
                        break;

                case READING_GTK_COLOR_SCHEME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (theme_thumbnail_data->gtk_color_scheme,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (theme_thumbnail_data->gtk_color_scheme,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                theme_thumbnail_data->status = READING_WM_THEME_NAME;
                        }
                        break;

                case READING_WM_THEME_NAME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (theme_thumbnail_data->wm_theme_name,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (theme_thumbnail_data->wm_theme_name,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                theme_thumbnail_data->status = READING_ICON_THEME_NAME;
                        }
                        break;

                case READING_ICON_THEME_NAME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (theme_thumbnail_data->icon_theme_name,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (theme_thumbnail_data->icon_theme_name,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                theme_thumbnail_data->status = READING_APPLICATION_FONT;
                        }
                        break;

                case READING_APPLICATION_FONT:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (theme_thumbnail_data->application_font,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (theme_thumbnail_data->application_font,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                theme_thumbnail_data->status = WRITING_PIXBUF_DATA;
                        }
                        break;

                default:
                        g_assert_not_reached ();
                }
        }
}

static gboolean
message_from_master (GIOChannel         *source,
                     GIOCondition        condition,
                     ThemeThumbnailData *data)
{
        char      buffer[1024];
        GIOStatus status;
        gsize     bytes_read;

        status = g_io_channel_read_chars (source,
                                          buffer,
                                          1024,
                                          &bytes_read,
                                          NULL);

        switch (status) {
        case G_IO_STATUS_NORMAL:
                handle_bytes (buffer, bytes_read, data);

                if (data->status == WRITING_PIXBUF_DATA) {
                        GdkPixbuf  *pixbuf = NULL;
                        int         i, rowstride;
                        guchar     *pixels;
                        int         width, height;
                        const char *type;
                        ssize_t     res;

                        type = (const char *) data->type->data;

                        g_debug ("Got message - creating thumb");
                        if (!strcmp (type, THUMBNAIL_TYPE_META))
                                pixbuf = create_meta_theme_pixbuf (data);
                        else if (!strcmp (type, THUMBNAIL_TYPE_GTK))
                                pixbuf = create_gtk_theme_pixbuf (data);
                        else if (!strcmp (type, THUMBNAIL_TYPE_METACITY))
                                pixbuf = create_metacity_theme_pixbuf (data);
                        else if (!strcmp (type, THUMBNAIL_TYPE_ICON))
                                pixbuf = create_icon_theme_pixbuf (data);
                        else
                                g_assert_not_reached ();

                        if (pixbuf == NULL) {
                                width = height = rowstride = 0;
                                pixels = NULL;
                        } else {
                                width = gdk_pixbuf_get_width (pixbuf);
                                height = gdk_pixbuf_get_height (pixbuf);
                                rowstride = gdk_pixbuf_get_rowstride (pixbuf);
                                pixels = gdk_pixbuf_get_pixels (pixbuf);
                        }

                        /* Write the pixbuf's size */
                        res = write (data->pipe_to_master, &width, sizeof (width));
                        res = write (data->pipe_to_master, &height, sizeof (height));

                        for (i = 0; i < height; i++) {
                                res = write (data->pipe_to_master,
                                             pixels + rowstride * i,
                                             width * gdk_pixbuf_get_n_channels (pixbuf));
                        }

                        if (pixbuf != NULL)
                                g_object_unref (pixbuf);

                        g_byte_array_set_size (data->type, 0);
                        g_byte_array_set_size (data->control_theme_name, 0);
                        g_byte_array_set_size (data->gtk_color_scheme, 0);
                        g_byte_array_set_size (data->wm_theme_name, 0);
                        g_byte_array_set_size (data->icon_theme_name, 0);
                        g_byte_array_set_size (data->application_font, 0);
                        data->status = READY_FOR_THEME;
                }
                return TRUE;

        case G_IO_STATUS_AGAIN:
                return TRUE;

        case G_IO_STATUS_EOF:
        case G_IO_STATUS_ERROR:
                _exit (0);

        default:
                g_assert_not_reached ();
        }

        return TRUE;
}

static void
generate_next_in_queue (CcThemeThumbnailer *thumbnailer)
{
        ThemeQueueItem *item;

        if (thumbnailer->priv->theme_queue == NULL)
                return;

        item = thumbnailer->priv->theme_queue->data;
        thumbnailer->priv->theme_queue = g_list_delete_link (thumbnailer->priv->theme_queue,
                                                             g_list_first (thumbnailer->priv->theme_queue));

        if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_META) == 0)
                cc_theme_thumbnailer_create_meta_async (thumbnailer,
                                                        (GnomeThemeMetaInfo *) item->theme_info,
                                                        item->func,
                                                        item->user_data,
                                                        item->destroy);
        else if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_GTK) == 0)
                cc_theme_thumbnailer_create_gtk_async (thumbnailer,
                                                       (GnomeThemeInfo *) item->theme_info,
                                                       item->func,
                                                       item->user_data,
                                                       item->destroy);
        else if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_METACITY) == 0)
                cc_theme_thumbnailer_create_metacity_async (thumbnailer,
                                                            (GnomeThemeInfo *) item->theme_info,
                                                            item->func,
                                                            item->user_data,
                                                            item->destroy);
        else if (strcmp (item->thumbnail_type, THUMBNAIL_TYPE_ICON) == 0)
                cc_theme_thumbnailer_create_icon_async (thumbnailer,
                                                        (GnomeThemeIconInfo *) item->theme_info,
                                                        item->func,
                                                        item->user_data,
                                                        item->destroy);

        g_free (item);
}

static gboolean
message_from_child (GIOChannel         *source,
                    GIOCondition        condition,
                    CcThemeThumbnailer *thumbnailer)
{
        char      buffer[1024];
        GIOStatus status;
        gsize     bytes_read;

        if (thumbnailer->priv->set == FALSE)
                return TRUE;

        if (condition == G_IO_HUP)
                return FALSE;

        status = g_io_channel_read_chars (source,
                                          buffer,
                                          1024,
                                          &bytes_read,
                                          NULL);
        switch (status) {
        case G_IO_STATUS_NORMAL:
                g_byte_array_append (thumbnailer->priv->data, (guchar *) buffer, bytes_read);

                if (thumbnailer->priv->thumbnail_width == -1
                    && thumbnailer->priv->data->len >= 2 * sizeof (int)) {
                        thumbnailer->priv->thumbnail_width = *((int *) thumbnailer->priv->data->data);
                        thumbnailer->priv->thumbnail_height = *(((int *) thumbnailer->priv->data->data) + 1);
                        g_byte_array_remove_range (thumbnailer->priv->data, 0, 2 * sizeof (int));
                }

                if (thumbnailer->priv->thumbnail_width >= 0
                    && thumbnailer->priv->data->len == thumbnailer->priv->thumbnail_width * thumbnailer->priv->thumbnail_height * 4) {
                        GdkPixbuf *pixbuf = NULL;

                        if (thumbnailer->priv->thumbnail_width > 0) {
                                char *pixels;
                                int i, rowstride;

                                pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                                                         TRUE,
                                                         8,
                                                         thumbnailer->priv->thumbnail_width,
                                                         thumbnailer->priv->thumbnail_height);
                                pixels = (char *) gdk_pixbuf_get_pixels (pixbuf);
                                rowstride = gdk_pixbuf_get_rowstride (pixbuf);

                                for (i = 0; i < thumbnailer->priv->thumbnail_height; ++i)
                                        memcpy (pixels + rowstride * i,
                                                thumbnailer->priv->data->data + 4 * thumbnailer->priv->thumbnail_width * i,
                                                thumbnailer->priv->thumbnail_width * 4);
                        }

                        /* callback function needs to ref the pixbuf if it wants to keep it */
                        (* thumbnailer->priv->func) (pixbuf,
                                                     thumbnailer->priv->theme_name,
                                                     thumbnailer->priv->user_data);

                        if (thumbnailer->priv->destroy)
                                (* thumbnailer->priv->destroy) (thumbnailer->priv->user_data);

                        if (pixbuf)
                                g_object_unref (pixbuf);

                        /* Clean up async_data */
                        g_free (thumbnailer->priv->theme_name);
                        g_source_remove (thumbnailer->priv->watch_id);
                        g_io_channel_unref (thumbnailer->priv->channel);

                        /* reset async_data */
                        thumbnailer->priv->thumbnail_width = -1;
                        thumbnailer->priv->thumbnail_height = -1;
                        thumbnailer->priv->theme_name = NULL;
                        thumbnailer->priv->channel = NULL;
                        thumbnailer->priv->func = NULL;
                        thumbnailer->priv->user_data = NULL;
                        thumbnailer->priv->destroy = NULL;
                        thumbnailer->priv->set = FALSE;
                        g_byte_array_set_size (thumbnailer->priv->data, 0);

                        generate_next_in_queue (thumbnailer);
                }
                return TRUE;

        case G_IO_STATUS_AGAIN:
                return TRUE;

        case G_IO_STATUS_EOF:
        case G_IO_STATUS_ERROR:
                return FALSE;

        default:
                g_assert_not_reached ();
        }

        return TRUE;
}

static void
send_thumbnail_request (CcThemeThumbnailer *thumbnailer,
                        char               *thumbnail_type,
                        char               *gtk_theme_name,
                        char               *gtk_color_scheme,
                        char               *metacity_theme_name,
                        char               *icon_theme_name,
                        char               *application_font)
{
        ssize_t res;

        res = write (thumbnailer->priv->pipe_to_factory,
                     thumbnail_type,
                     strlen (thumbnail_type) + 1);

        if (gtk_theme_name != NULL)
                res = write (thumbnailer->priv->pipe_to_factory,
                             gtk_theme_name,
                             strlen (gtk_theme_name) + 1);
        else
                res = write (thumbnailer->priv->pipe_to_factory,
                             "",
                             1);

        if (gtk_color_scheme != NULL)
                res = write (thumbnailer->priv->pipe_to_factory,
                             gtk_color_scheme,
                             strlen (gtk_color_scheme) + 1);
        else
                res = write (thumbnailer->priv->pipe_to_factory,
                             "",
                             1);

        if (metacity_theme_name != NULL)
                res = write (thumbnailer->priv->pipe_to_factory,
                             metacity_theme_name,
                             strlen (metacity_theme_name) + 1);
        else
                res = write (thumbnailer->priv->pipe_to_factory,
                             "",
                             1);

        if (icon_theme_name != NULL)
                res = write (thumbnailer->priv->pipe_to_factory,
                             icon_theme_name,
                             strlen (icon_theme_name) + 1);
        else
                res = write (thumbnailer->priv->pipe_to_factory,
                             "",
                             1);

        if (application_font != NULL)
                res = write (thumbnailer->priv->pipe_to_factory,
                             application_font,
                             strlen (application_font) + 1);
        else
                res = write (thumbnailer->priv->pipe_to_factory,
                             "Sans 10",
                             strlen ("Sans 10") + 1);

        /* FIXME: check the return values */
}

static void
generate_thumbnail_async (CcThemeThumbnailer  *thumbnailer,
                          gpointer             theme_info,
                          char                *theme_name,
                          char                *thumbnail_type,
                          char                *gtk_theme_name,
                          char                *gtk_color_scheme,
                          char                *metacity_theme_name,
                          char                *icon_theme_name,
                          char                *application_font,
                          CcThemeThumbnailFunc func,
                          gpointer             user_data,
                          GDestroyNotify       destroy)
{
        g_debug ("In generator for %s", theme_name);
        if (thumbnailer->priv->set) {
                ThemeQueueItem *item;

                item = g_new0 (ThemeQueueItem, 1);
                item->thumbnail_type = thumbnail_type;
                item->theme_info = theme_info;
                item->func = func;
                item->user_data = user_data;
                item->destroy = destroy;

                thumbnailer->priv->theme_queue = g_list_append (thumbnailer->priv->theme_queue, item);
                g_debug ("\tAdded to queue");
                return;
        }

        if (!thumbnailer->priv->pipe_to_factory
            || !thumbnailer->priv->pipe_from_factory) {
                g_debug ("\tPipes not set up");

                (* func) (NULL, theme_name, user_data);

                if (destroy)
                        (* destroy) (user_data);

                return;
        }

        if (thumbnailer->priv->channel == NULL) {
                thumbnailer->priv->channel = g_io_channel_unix_new (thumbnailer->priv->pipe_from_factory);
                g_io_channel_set_flags (thumbnailer->priv->channel,
                                        g_io_channel_get_flags (thumbnailer->priv->channel)
                                        | G_IO_FLAG_NONBLOCK,
                                        NULL);
                g_io_channel_set_encoding (thumbnailer->priv->channel, NULL, NULL);
                thumbnailer->priv->watch_id = g_io_add_watch (thumbnailer->priv->channel,
                                                              G_IO_IN | G_IO_HUP,
                                                              (GIOFunc) message_from_child,
                                                              thumbnailer);
        }

        thumbnailer->priv->set = TRUE;
        thumbnailer->priv->thumbnail_width = -1;
        thumbnailer->priv->thumbnail_height = -1;
        thumbnailer->priv->theme_name = g_strdup (theme_name);
        thumbnailer->priv->func = func;
        thumbnailer->priv->user_data = user_data;
        thumbnailer->priv->destroy = destroy;

        g_debug ("\tSending thumbnail request");
        send_thumbnail_request (thumbnailer,
                                thumbnail_type,
                                gtk_theme_name,
                                gtk_color_scheme,
                                metacity_theme_name,
                                icon_theme_name,
                                application_font);
}

void
cc_theme_thumbnailer_create_meta_async (CcThemeThumbnailer  *thumbnailer,
                                        GnomeThemeMetaInfo  *theme_info,
                                        CcThemeThumbnailFunc func,
                                        gpointer             user_data,
                                        GDestroyNotify       destroy)
{
        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_META,
                                  theme_info->gtk_theme_name,
                                  theme_info->gtk_color_scheme,
                                  theme_info->metacity_theme_name,
                                  theme_info->icon_theme_name,
                                  theme_info->application_font,
                                  func,
                                  user_data,
                                  destroy);
}

void
cc_theme_thumbnailer_create_gtk_async (CcThemeThumbnailer   *thumbnailer,
                                       GnomeThemeInfo       *theme_info,
                                       CcThemeThumbnailFunc  func,
                                       gpointer              user_data,
                                       GDestroyNotify        destroy)
{
        char *scheme;

        scheme = gtkrc_get_color_scheme_for_theme (theme_info->name);

        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_GTK,
                                  theme_info->name,
                                  scheme,
                                  NULL,
                                  NULL,
                                  NULL,
                                  func,
                                  user_data,
                                  destroy);
        g_free (scheme);
}

void
cc_theme_thumbnailer_create_metacity_async (CcThemeThumbnailer  *thumbnailer,
                                            GnomeThemeInfo      *theme_info,
                                            CcThemeThumbnailFunc func,
                                            gpointer             user_data,
                                            GDestroyNotify       destroy)
{
        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_METACITY,
                                  NULL,
                                  NULL,
                                  theme_info->name,
                                  NULL,
                                  NULL,
                                  func,
                                  user_data,
                                  destroy);
}

void
cc_theme_thumbnailer_create_icon_async (CcThemeThumbnailer  *thumbnailer,
                                        GnomeThemeIconInfo  *theme_info,
                                        CcThemeThumbnailFunc func,
                                        gpointer             user_data,
                                        GDestroyNotify       destroy)
{
        generate_thumbnail_async (thumbnailer,
                                  theme_info,
                                  theme_info->name,
                                  THUMBNAIL_TYPE_ICON,
                                  NULL,
                                  NULL,
                                  NULL,
                                  theme_info->name,
                                  NULL,
                                  func,
                                  user_data,
                                  destroy);
}

static void
create_server (CcThemeThumbnailer *thumbnailer)
{
        GPid child_pid;
        int  pipe_to_factory_fd[2];
        int  pipe_from_factory_fd[2];

        pipe (pipe_to_factory_fd);
        pipe (pipe_from_factory_fd);

        /* Apple's CoreFoundation classes must not be used from forked
         * processes. Since freetype (and thus GTK) uses them, we simply
         * disable the thumbnailer on MacOS for now. That means no thumbs
         * until the thumbnailing process is rewritten, but at least we won't
         * make apps crash. */
#ifndef __APPLE__
        child_pid = fork ();
        if (child_pid == 0) {
                ThemeThumbnailData data;
                GIOChannel        *channel;

                /* Child */
                gtk_init (NULL, NULL);

                close (pipe_to_factory_fd[1]);
                pipe_to_factory_fd[1] = 0;
                close (pipe_from_factory_fd[0]);
                pipe_from_factory_fd[0] = 0;

                data.status = READY_FOR_THEME;
                data.type = g_byte_array_new ();
                data.control_theme_name = g_byte_array_new ();
                data.gtk_color_scheme = g_byte_array_new ();
                data.wm_theme_name = g_byte_array_new ();
                data.icon_theme_name = g_byte_array_new ();
                data.application_font = g_byte_array_new ();
                data.pipe_to_master = pipe_from_factory_fd[1];

                channel = g_io_channel_unix_new (pipe_to_factory_fd[0]);
                g_io_channel_set_flags (channel,
                                        g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK,
                                        NULL);
                g_io_channel_set_encoding (channel, NULL, NULL);
                g_io_add_watch (channel,
                                G_IO_IN | G_IO_HUP,
                                (GIOFunc) message_from_master,
                                &data);
                g_io_channel_unref (channel);

                gtk_main ();
                _exit (0);
        }

        g_assert (child_pid > 0);

        /* Parent */
        close (pipe_to_factory_fd[0]);
        close (pipe_from_factory_fd[1]);
        thumbnailer->priv->child_pid = child_pid;
        thumbnailer->priv->pipe_to_factory = pipe_to_factory_fd[1];
        thumbnailer->priv->pipe_from_factory = pipe_from_factory_fd[0];

#endif /* __APPLE__ */
}

static GObject *
cc_theme_thumbnailer_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
        CcThemeThumbnailer *thumbnailer;

        thumbnailer = CC_THEME_THUMBNAILER (G_OBJECT_CLASS (cc_theme_thumbnailer_parent_class)->constructor (type,
                                                                                                                   n_construct_properties,
                                                                                                                   construct_properties));

        create_server (thumbnailer);

        return G_OBJECT (thumbnailer);
}

static void
cc_theme_thumbnailer_class_init (CcThemeThumbnailerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_theme_thumbnailer_get_property;
        object_class->set_property = cc_theme_thumbnailer_set_property;
        object_class->constructor = cc_theme_thumbnailer_constructor;
        object_class->finalize = cc_theme_thumbnailer_finalize;

        g_type_class_add_private (klass, sizeof (CcThemeThumbnailerPrivate));
}

static void
cc_theme_thumbnailer_init (CcThemeThumbnailer *thumbnailer)
{

        thumbnailer->priv = CC_THEME_THUMBNAILER_GET_PRIVATE (thumbnailer);

        thumbnailer->priv->set = FALSE;
        thumbnailer->priv->data = g_byte_array_new ();
}

static void
cc_theme_thumbnailer_finalize (GObject *object)
{
        CcThemeThumbnailer *theme_thumbnailer;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_THEME_THUMBNAILER (object));

        theme_thumbnailer = CC_THEME_THUMBNAILER (object);

        g_return_if_fail (theme_thumbnailer->priv != NULL);

        G_OBJECT_CLASS (cc_theme_thumbnailer_parent_class)->finalize (object);
}

CcThemeThumbnailer *
cc_theme_thumbnailer_new (void)
{
        if (theme_thumbnailer_object != NULL) {
                g_object_ref (theme_thumbnailer_object);
        } else {
                theme_thumbnailer_object = g_object_new (CC_TYPE_THEME_THUMBNAILER, NULL);
                g_object_add_weak_pointer (theme_thumbnailer_object,
                                           (gpointer *) &theme_thumbnailer_object);
        }

        return CC_THEME_THUMBNAILER (theme_thumbnailer_object);
}
