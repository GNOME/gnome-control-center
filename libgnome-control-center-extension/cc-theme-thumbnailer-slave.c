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
#include <dirent.h>
#include <errno.h>
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

#include "cc-theme-thumbnailer-slave.h"
#include "gtkrc-utils.h"

#define CC_THEME_THUMBNAILER_SLAVE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_THEME_THUMBNAILER_SLAVE, CcThemeThumbnailerSlavePrivate))

#define THUMBNAIL_TYPE_META     "meta"
#define THUMBNAIL_TYPE_GTK      "gtk"
#define THUMBNAIL_TYPE_METACITY "metacity"
#define THUMBNAIL_TYPE_ICON     "icon"

#define META_THUMBNAIL_SIZE       128
#define GTK_THUMBNAIL_SIZE         96
#define METACITY_THUMBNAIL_WIDTH  120
#define METACITY_THUMBNAIL_HEIGHT  60

struct CcThemeThumbnailerSlavePrivate
{
        int                  status;
        GByteArray          *type;
        GByteArray          *control_theme_name;
        GByteArray          *gtk_color_scheme;
        GByteArray          *wm_theme_name;
        GByteArray          *icon_theme_name;
        GByteArray          *application_font;

        GIOChannel          *channel;
        guint                watch_id;
};

enum {
        PROP_0,
};

static void     cc_theme_thumbnailer_slave_class_init  (CcThemeThumbnailerSlaveClass *klass);
static void     cc_theme_thumbnailer_slave_init        (CcThemeThumbnailerSlave      *theme_thumbnailer_slave);
static void     cc_theme_thumbnailer_slave_finalize    (GObject         *object);

static gpointer theme_thumbnailer_slave_object = NULL;

G_DEFINE_TYPE (CcThemeThumbnailerSlave, cc_theme_thumbnailer_slave, G_TYPE_OBJECT)

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
cc_theme_thumbnailer_slave_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("cc_theme_thumbnailer_slave_error");
        }

        return ret;
}

static void
cc_theme_thumbnailer_slave_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue  *value,
                                         GParamSpec    *pspec)
{
        CcThemeThumbnailerSlave *self;

        self = CC_THEME_THUMBNAILER_SLAVE (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_theme_thumbnailer_slave_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
        CcThemeThumbnailerSlave *self;

        self = CC_THEME_THUMBNAILER_SLAVE (object);

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
        int     nchannels, rowstride, w, h;
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
create_meta_theme_pixbuf (CcThemeThumbnailerSlave *slave)
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
                      "gtk-theme-name", (char *) slave->priv->control_theme_name->data,
                      "gtk-font-name", (char *) slave->priv->application_font->data,
                      "gtk-icon-theme-name", (char *) slave->priv->icon_theme_name->data,
                      "gtk-color-scheme", (char *) slave->priv->gtk_color_scheme->data,
                      NULL);

        theme = meta_theme_load ((char *) slave->priv->wm_theme_name->data, NULL);
        if (theme == NULL)
                return NULL;

        /* Represent the icon theme */
        icon = create_folder_icon ((char *) slave->priv->icon_theme_name->data);
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
create_gtk_theme_pixbuf (CcThemeThumbnailerSlave *slave)
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
                      "gtk-theme-name", (char *) slave->priv->control_theme_name->data,
                      "gtk-color-scheme", (char *) slave->priv->gtk_color_scheme->data,
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
create_metacity_theme_pixbuf (CcThemeThumbnailerSlave *slave)
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

        theme = meta_theme_load ((char *) slave->priv->wm_theme_name->data, NULL);
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
create_icon_theme_pixbuf (CcThemeThumbnailerSlave *slave)
{
        return create_folder_icon ((char *) slave->priv->icon_theme_name->data);
}


static void
handle_bytes (CcThemeThumbnailerSlave *slave,
              const char              *buffer,
              int                      bytes_read)
{
        const guint8 *ptr;

        ptr = (guint8 *)buffer;

        while (bytes_read > 0) {
                guint8 *nil;

                switch (slave->priv->status) {
                case READY_FOR_THEME:
                        slave->priv->status = READING_TYPE;
                        /* fall through */
                case READING_TYPE:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (slave->priv->type,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (slave->priv->type,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                slave->priv->status = READING_CONTROL_THEME_NAME;
                        }
                        break;

                case READING_CONTROL_THEME_NAME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (slave->priv->control_theme_name,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (slave->priv->control_theme_name,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                slave->priv->status = READING_GTK_COLOR_SCHEME;
                        }
                        break;

                case READING_GTK_COLOR_SCHEME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (slave->priv->gtk_color_scheme,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (slave->priv->gtk_color_scheme,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                slave->priv->status = READING_WM_THEME_NAME;
                        }
                        break;

                case READING_WM_THEME_NAME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (slave->priv->wm_theme_name,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (slave->priv->wm_theme_name,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                slave->priv->status = READING_ICON_THEME_NAME;
                        }
                        break;

                case READING_ICON_THEME_NAME:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (slave->priv->icon_theme_name,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (slave->priv->icon_theme_name,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                slave->priv->status = READING_APPLICATION_FONT;
                        }
                        break;

                case READING_APPLICATION_FONT:
                        nil = memchr (ptr, '\000', bytes_read);
                        if (nil == NULL) {
                                g_byte_array_append (slave->priv->application_font,
                                                     ptr,
                                                     bytes_read);
                                bytes_read = 0;
                        } else {
                                g_byte_array_append (slave->priv->application_font,
                                                     ptr,
                                                     nil - ptr + 1);
                                bytes_read -= (nil - ptr + 1);
                                ptr = nil + 1;
                                slave->priv->status = WRITING_PIXBUF_DATA;
                        }
                        break;

                default:
                        g_assert_not_reached ();
                }
        }
}

static gboolean
message_from_master (GIOChannel              *source,
                     GIOCondition             condition,
                     CcThemeThumbnailerSlave *slave)
{
        gboolean finished = FALSE;

        if (condition & G_IO_IN) {
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
                        handle_bytes (slave, buffer, bytes_read);

                        if (slave->priv->status == WRITING_PIXBUF_DATA) {
                                GdkPixbuf  *pixbuf = NULL;
                                int         i, rowstride;
                                guchar     *pixels;
                                int         width, height;
                                const char *type;
                                ssize_t     res;

                                type = (const char *) slave->priv->type->data;

                                if (!strcmp (type, THUMBNAIL_TYPE_META))
                                        pixbuf = create_meta_theme_pixbuf (slave);
                                else if (!strcmp (type, THUMBNAIL_TYPE_GTK))
                                        pixbuf = create_gtk_theme_pixbuf (slave);
                                else if (!strcmp (type, THUMBNAIL_TYPE_METACITY))
                                        pixbuf = create_metacity_theme_pixbuf (slave);
                                else if (!strcmp (type, THUMBNAIL_TYPE_ICON))
                                        pixbuf = create_icon_theme_pixbuf (slave);
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
                                res = write (STDOUT_FILENO, &width, sizeof (width));
                                res = write (STDOUT_FILENO, &height, sizeof (height));

                                for (i = 0; i < height; i++) {
                                        res = write (STDOUT_FILENO,
                                                     pixels + rowstride * i,
                                                     width * gdk_pixbuf_get_n_channels (pixbuf));
                                }

                                if (pixbuf != NULL)
                                        g_object_unref (pixbuf);

                                g_byte_array_set_size (slave->priv->type, 0);
                                g_byte_array_set_size (slave->priv->control_theme_name, 0);
                                g_byte_array_set_size (slave->priv->gtk_color_scheme, 0);
                                g_byte_array_set_size (slave->priv->wm_theme_name, 0);
                                g_byte_array_set_size (slave->priv->icon_theme_name, 0);
                                g_byte_array_set_size (slave->priv->application_font, 0);
                                slave->priv->status = READY_FOR_THEME;
                        }
                        break;

                case G_IO_STATUS_AGAIN:
                        break;

                case G_IO_STATUS_EOF:
                case G_IO_STATUS_ERROR:
                        finished = TRUE;
                        break;

                default:
                        g_assert_not_reached ();
                }
        } else if (condition & G_IO_HUP) {
                finished = TRUE;
        }

        if (finished) {
                return FALSE;
        }

        return TRUE;
}

static void
start_slave (CcThemeThumbnailerSlave *slave)
{
        slave->priv->status = READY_FOR_THEME;
        slave->priv->type = g_byte_array_new ();
        slave->priv->control_theme_name = g_byte_array_new ();
        slave->priv->gtk_color_scheme = g_byte_array_new ();
        slave->priv->wm_theme_name = g_byte_array_new ();
        slave->priv->icon_theme_name = g_byte_array_new ();
        slave->priv->application_font = g_byte_array_new ();

        slave->priv->channel = g_io_channel_unix_new (STDIN_FILENO);
        g_io_channel_set_flags (slave->priv->channel,
                                g_io_channel_get_flags (slave->priv->channel) | G_IO_FLAG_NONBLOCK,
                                NULL);

        g_io_channel_set_encoding (slave->priv->channel, NULL, NULL);
        g_io_add_watch (slave->priv->channel,
                        G_IO_IN | G_IO_HUP,
                        (GIOFunc) message_from_master,
                        slave);
}

static GObject *
cc_theme_thumbnailer_slave_constructor (GType                  type,
                                        guint                  n_construct_properties,
                                        GObjectConstructParam *construct_properties)
{
        CcThemeThumbnailerSlave *slave;

        slave = CC_THEME_THUMBNAILER_SLAVE (G_OBJECT_CLASS (cc_theme_thumbnailer_slave_parent_class)->constructor (type,
                                                                                                                               n_construct_properties,
                                                                                                                               construct_properties));

        start_slave (slave);

        return G_OBJECT (slave);
}

static void
cc_theme_thumbnailer_slave_class_init (CcThemeThumbnailerSlaveClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_theme_thumbnailer_slave_get_property;
        object_class->set_property = cc_theme_thumbnailer_slave_set_property;
        object_class->constructor = cc_theme_thumbnailer_slave_constructor;
        object_class->finalize = cc_theme_thumbnailer_slave_finalize;

        g_type_class_add_private (klass, sizeof (CcThemeThumbnailerSlavePrivate));
}

static void
cc_theme_thumbnailer_slave_init (CcThemeThumbnailerSlave *thumbnailer_slave)
{

        thumbnailer_slave->priv = CC_THEME_THUMBNAILER_SLAVE_GET_PRIVATE (thumbnailer_slave);
}

static void
cc_theme_thumbnailer_slave_finalize (GObject *object)
{
        CcThemeThumbnailerSlave *theme_thumbnailer_slave;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_THEME_THUMBNAILER_SLAVE (object));

        theme_thumbnailer_slave = CC_THEME_THUMBNAILER_SLAVE (object);

        g_return_if_fail (theme_thumbnailer_slave->priv != NULL);

        G_OBJECT_CLASS (cc_theme_thumbnailer_slave_parent_class)->finalize (object);
}

CcThemeThumbnailerSlave *
cc_theme_thumbnailer_slave_new (void)
{
        if (theme_thumbnailer_slave_object != NULL) {
                g_object_ref (theme_thumbnailer_slave_object);
        } else {
                theme_thumbnailer_slave_object = g_object_new (CC_TYPE_THEME_THUMBNAILER_SLAVE, NULL);
                g_object_add_weak_pointer (theme_thumbnailer_slave_object,
                                           (gpointer *) &theme_thumbnailer_slave_object);
        }

        return CC_THEME_THUMBNAILER_SLAVE (theme_thumbnailer_slave_object);
}
