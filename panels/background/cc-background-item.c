/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include <libgnome-desktop/gnome-bg.h>
#include <gdesktop-enums.h>

#include "cc-background-item.h"
#include "gdesktop-enums-types.h"

struct _CcBackgroundItem
{
        GObject          parent_instance;

        /* properties */
        char            *name;
        char            *uri;
        char            *size;
        GDesktopBackgroundStyle placement;
        GDesktopBackgroundShading shading;
        char            *primary_color;
        char            *secondary_color;
        char            *source_url; /* Used by the Flickr source */
        char            *source_xml; /* Used by the Wallpapers source */
        gboolean         is_deleted;
        gboolean         needs_download;
        CcBackgroundItemFlags flags;
        guint64          modified;

        /* internal */
        GdkPixbuf       *slideshow_emblem;
        GnomeBG         *bg;
        char            *mime_type;
        int              width;
        int              height;
};

enum {
        PROP_0,
        PROP_NAME,
        PROP_URI,
        PROP_PLACEMENT,
        PROP_SHADING,
        PROP_PRIMARY_COLOR,
        PROP_SECONDARY_COLOR,
        PROP_IS_DELETED,
        PROP_SOURCE_URL,
        PROP_SOURCE_XML,
        PROP_FLAGS,
        PROP_SIZE,
        PROP_NEEDS_DOWNLOAD,
        PROP_MODIFIED
};

static void     cc_background_item_class_init     (CcBackgroundItemClass *klass);
static void     cc_background_item_init           (CcBackgroundItem      *background_item);
static void     cc_background_item_finalize       (GObject               *object);

G_DEFINE_TYPE (CcBackgroundItem, cc_background_item, G_TYPE_OBJECT)

static GdkPixbuf *slideshow_emblem = NULL;

static GdkPixbuf *
get_emblemed_pixbuf (CcBackgroundItem *item, GdkPixbuf *pixbuf, gint scale_factor)
{
        GdkPixbuf *retval;
        GIcon *icon = NULL;
        GtkIconInfo *icon_info = NULL;
        int eh;
        int ew;
        int h;
        int w;
        int x;
        int y;

        retval = g_object_ref (pixbuf);

        if (item->slideshow_emblem == NULL) {
                if (slideshow_emblem == NULL) {
                        GError *error = NULL;
                        GtkIconTheme *theme;

                        icon = g_themed_icon_new ("slideshow-emblem");
                        theme = gtk_icon_theme_get_default ();
                        icon_info = gtk_icon_theme_lookup_by_gicon_for_scale (theme,
                                                                              icon,
                                                                              16,
                                                                              scale_factor,
                                                                              GTK_ICON_LOOKUP_FORCE_SIZE |
                                                                              GTK_ICON_LOOKUP_USE_BUILTIN);
                        if (icon_info == NULL) {
                                g_warning ("Your icon theme is missing the slideshow-emblem icon, "
                                           "please file a bug against it");
                                goto out;
                        }

                        slideshow_emblem = gtk_icon_info_load_icon (icon_info, &error);
                        if (slideshow_emblem == NULL) {
                                g_warning ("Failed to load slideshow emblem: %s", error->message);
                                g_error_free (error);
                                goto out;
                        }

                        g_object_add_weak_pointer (G_OBJECT (slideshow_emblem), (gpointer *) (&slideshow_emblem));
                        item->slideshow_emblem = slideshow_emblem;
                } else {
                        item->slideshow_emblem = g_object_ref (slideshow_emblem);
                }
        }

        eh = gdk_pixbuf_get_height (slideshow_emblem);
        ew = gdk_pixbuf_get_width (slideshow_emblem);
        h = gdk_pixbuf_get_height (pixbuf);
        w = gdk_pixbuf_get_width (pixbuf);
        x = w - ew;
        y = h - eh;

        gdk_pixbuf_composite (slideshow_emblem, pixbuf, x, y, ew, eh, x, y, 1.0, 1.0, GDK_INTERP_BILINEAR, 255);

 out:
        g_clear_object (&icon_info);
        g_clear_object (&icon);
        return retval;
}

static void
set_bg_properties (CcBackgroundItem *item)
{
        GdkColor pcolor = { 0, 0, 0, 0 };
        GdkColor scolor = { 0, 0, 0, 0 };

        if (item->uri) {
		GFile *file;
		char *filename;

		file = g_file_new_for_commandline_arg (item->uri);
		filename = g_file_get_path (file);
		g_object_unref (file);

		gnome_bg_set_filename (item->bg, filename);
		g_free (filename);
	}

        if (item->primary_color != NULL) {
                gdk_color_parse (item->primary_color, &pcolor);
        }
        if (item->secondary_color != NULL) {
                gdk_color_parse (item->secondary_color, &scolor);
        }

        gnome_bg_set_color (item->bg, item->shading, &pcolor, &scolor);
        gnome_bg_set_placement (item->bg, item->placement);
}


gboolean
cc_background_item_changes_with_time (CcBackgroundItem *item)
{
        gboolean changes;

	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), FALSE);

        changes = FALSE;
        if (item->bg != NULL) {
                changes = gnome_bg_changes_with_time (item->bg);
        }
        return changes;
}

static void
update_size (CcBackgroundItem *item)
{
	g_clear_pointer (&item->size, g_free);

	if (item->uri == NULL) {
		item->size = g_strdup ("");
	} else {
		if (gnome_bg_has_multiple_sizes (item->bg) || gnome_bg_changes_with_time (item->bg)) {
			item->size = g_strdup (_("multiple sizes"));
		} else {
			/* translators: 100 × 100px
			 * Note that this is not an "x", but U+00D7 MULTIPLICATION SIGN */
			item->size = g_strdup_printf (_("%d × %d"),
						      item->width,
						      item->height);
		}
	}
}

static GdkPixbuf *
render_at_size (GnomeBG *bg,
                gint width,
                gint height)
{
        GdkPixbuf *pixbuf;

        pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
        gnome_bg_draw (bg, pixbuf, gdk_screen_get_default (), FALSE);

        return pixbuf;
}

GdkPixbuf *
cc_background_item_get_frame_thumbnail (CcBackgroundItem             *item,
                                        GnomeDesktopThumbnailFactory *thumbs,
                                        int                           width,
                                        int                           height,
                                        int                           scale_factor,
                                        int                           frame,
                                        gboolean                      force_size)
{
        GdkPixbuf *pixbuf = NULL;
        GdkPixbuf *retval = NULL;

	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);
	g_return_val_if_fail (width > 0 && height > 0, NULL);

        set_bg_properties (item);

        if (force_size) {
                /* FIXME: this doesn't play nice with slideshow stepping at all,
                 * because it will always render the current slideshow frame, which
                 * might not be what we want.
                 * We're lacking an API to draw a high-res GnomeBG manually choosing
                 * the slideshow frame though, so we can't do much better than this
                 * for now.
                 */
                pixbuf = render_at_size (item->bg, width, height);
        } else {
                if (frame >= 0) {
                        pixbuf = gnome_bg_create_frame_thumbnail (item->bg,
                                                                  thumbs,
                                                                  gdk_screen_get_default (),
                                                                  width,
                                                                  height,
                                                                  frame);
                } else {
                        pixbuf = gnome_bg_create_thumbnail (item->bg,
                                                            thumbs,
                                                            gdk_screen_get_default (),
                                                            width,
                                                            height);
                }
        }

        if (pixbuf != NULL
            && frame != -2
            && gnome_bg_changes_with_time (item->bg)) {
                retval = get_emblemed_pixbuf (item, pixbuf, scale_factor);
                g_object_unref (pixbuf);
        } else {
                retval = pixbuf;
	}

        gnome_bg_get_image_size (item->bg,
                                 thumbs,
                                 width,
                                 height,
                                 &item->width,
                                 &item->height);

        update_size (item);

        return retval;
}


GdkPixbuf *
cc_background_item_get_thumbnail (CcBackgroundItem             *item,
                                  GnomeDesktopThumbnailFactory *thumbs,
                                  int                           width,
                                  int                           height,
                                  int                           scale_factor)
{
        return cc_background_item_get_frame_thumbnail (item, thumbs, width, height, scale_factor, -1, FALSE);
}

static void
update_info (CcBackgroundItem *item,
	     GFileInfo        *_info)
{
        GFile     *file;
        GFileInfo *info;

	if (_info == NULL) {
		file = g_file_new_for_uri (item->uri);

		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_NAME ","
					  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
					  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
					  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
					  G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE,
					  NULL,
					  NULL);
		g_object_unref (file);
	} else {
		info = g_object_ref (_info);
	}

	g_clear_pointer (&item->mime_type, g_free);

        if (info == NULL
            || g_file_info_get_content_type (info) == NULL) {
                if (item->uri == NULL) {
                        item->mime_type = g_strdup ("image/x-no-data");
                        g_free (item->name);
                        item->name = g_strdup (_("No Desktop Background"));
                }
        } else {
                if (item->name == NULL)
                        item->name = g_strdup (g_file_info_get_display_name (info));

                item->mime_type = g_strdup (g_file_info_get_content_type (info));
                if (item->modified == 0)
                  item->modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        }

        if (info != NULL)
                g_object_unref (info);
}

gboolean
cc_background_item_load (CcBackgroundItem *item,
			 GFileInfo        *info)
{
        g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), FALSE);

        if (item->uri == NULL)
		return TRUE;

        update_info (item, info);

        if (item->mime_type != NULL
            && (g_str_has_prefix (item->mime_type, "image/")
                || strcmp (item->mime_type, "application/xml") == 0)) {
                set_bg_properties (item);
        } else {
		return FALSE;
        }

	/* FIXME we should handle XML files as well */
        if (item->mime_type != NULL &&
            g_str_has_prefix (item->mime_type, "image/")) {
		char *filename;

		filename = g_filename_from_uri (item->uri, NULL, NULL);
		gdk_pixbuf_get_file_info (filename,
					  &item->width,
					  &item->height);
		g_free (filename);
		update_size (item);
	}

        return TRUE;
}

static void
_set_name (CcBackgroundItem *item,
           const char       *value)
{
        g_free (item->name);
        item->name = g_strdup (value);
}

const char *
cc_background_item_get_name (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->name;
}

static void
_set_uri (CcBackgroundItem *item,
	  const char       *value)
{
        g_free (item->uri);
        if (value && *value == '\0') {
		item->uri = NULL;
	} else {
		if (value && strstr (value, "://") == NULL)
			g_warning ("URI '%s' is invalid", value);
		item->uri = g_strdup (value);
	}
}

const char *
cc_background_item_get_uri (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->uri;
}

static void
_set_placement (CcBackgroundItem        *item,
                GDesktopBackgroundStyle  value)
{
        item->placement = value;
}

static void
_set_shading (CcBackgroundItem          *item,
              GDesktopBackgroundShading  value)
{
        item->shading = value;
}

static void
_set_primary_color (CcBackgroundItem *item,
                    const char       *value)
{
        g_free (item->primary_color);
        item->primary_color = g_strdup (value);
}

const char *
cc_background_item_get_pcolor (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->primary_color;
}

static void
_set_secondary_color (CcBackgroundItem *item,
                      const char       *value)
{
        g_free (item->secondary_color);
        item->secondary_color = g_strdup (value);
}

const char *
cc_background_item_get_scolor (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->secondary_color;
}

GDesktopBackgroundStyle
cc_background_item_get_placement (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), G_DESKTOP_BACKGROUND_STYLE_SCALED);

	return item->placement;
}

GDesktopBackgroundShading
cc_background_item_get_shading (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), G_DESKTOP_BACKGROUND_SHADING_SOLID);

	return item->shading;
}

static void
_set_is_deleted (CcBackgroundItem *item,
                 gboolean          value)
{
        item->is_deleted = value;
}

static void
_set_source_url (CcBackgroundItem *item,
                 const char       *value)
{
        g_free (item->source_url);
        item->source_url = g_strdup (value);
}

const char *
cc_background_item_get_source_url (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->source_url;
}

static void
_set_source_xml (CcBackgroundItem *item,
                 const char       *value)
{
        g_free (item->source_xml);
        item->source_xml = g_strdup (value);
}

const char *
cc_background_item_get_source_xml (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->source_xml;
}

static void
_set_flags (CcBackgroundItem      *item,
            CcBackgroundItemFlags  value)
{
	item->flags = value;
}

CcBackgroundItemFlags
cc_background_item_get_flags (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), 0);

	return item->flags;
}

const char *
cc_background_item_get_size (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), NULL);

	return item->size;
}

static void
_set_needs_download (CcBackgroundItem *item,
		     gboolean          value)
{
	item->needs_download = value;
}

gboolean
cc_background_item_get_needs_download (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), 0);

	return item->needs_download;
}

static void
_set_modified (CcBackgroundItem *item,
               guint64           value)
{
        item->modified = value;
}

guint64
cc_background_item_get_modified (CcBackgroundItem *item)
{
	g_return_val_if_fail (CC_IS_BACKGROUND_ITEM (item), 0);

	return item->modified;
}

static void
cc_background_item_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        CcBackgroundItem *self;

        self = CC_BACKGROUND_ITEM (object);

        switch (prop_id) {
        case PROP_NAME:
                _set_name (self, g_value_get_string (value));
                break;
        case PROP_URI:
                _set_uri (self, g_value_get_string (value));
                break;
        case PROP_PLACEMENT:
                _set_placement (self, g_value_get_enum (value));
                break;
        case PROP_SHADING:
                _set_shading (self, g_value_get_enum (value));
                break;
        case PROP_PRIMARY_COLOR:
                _set_primary_color (self, g_value_get_string (value));
                break;
        case PROP_SECONDARY_COLOR:
                _set_secondary_color (self, g_value_get_string (value));
                break;
        case PROP_IS_DELETED:
                _set_is_deleted (self, g_value_get_boolean (value));
                break;
	case PROP_SOURCE_URL:
		_set_source_url (self, g_value_get_string (value));
		break;
	case PROP_SOURCE_XML:
		_set_source_xml (self, g_value_get_string (value));
		break;
	case PROP_FLAGS:
		_set_flags (self, g_value_get_flags (value));
		break;
	case PROP_NEEDS_DOWNLOAD:
		_set_needs_download (self, g_value_get_boolean (value));
		break;
	case PROP_MODIFIED:
		_set_modified (self, g_value_get_uint64 (value));
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_background_item_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        CcBackgroundItem *self;

        self = CC_BACKGROUND_ITEM (object);

        switch (prop_id) {
        case PROP_NAME:
                g_value_set_string (value, self->name);
                break;
	case PROP_URI:
                g_value_set_string (value, self->uri);
                break;
        case PROP_PLACEMENT:
                g_value_set_enum (value, self->placement);
                break;
        case PROP_SHADING:
                g_value_set_enum (value, self->shading);
                break;
        case PROP_PRIMARY_COLOR:
                g_value_set_string (value, self->primary_color);
                break;
        case PROP_SECONDARY_COLOR:
                g_value_set_string (value, self->secondary_color);
                break;
        case PROP_IS_DELETED:
                g_value_set_boolean (value, self->is_deleted);
                break;
	case PROP_SOURCE_URL:
		g_value_set_string (value, self->source_url);
		break;
	case PROP_SOURCE_XML:
		g_value_set_string (value, self->source_xml);
		break;
	case PROP_FLAGS:
		g_value_set_flags (value, self->flags);
		break;
	case PROP_SIZE:
		g_value_set_string (value, self->size);
		break;
	case PROP_NEEDS_DOWNLOAD:
		g_value_set_boolean (value, self->needs_download);
		break;
	case PROP_MODIFIED:
		g_value_set_uint64 (value, self->modified);
		break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
cc_background_item_constructor (GType                  type,
                                guint                  n_construct_properties,
                                GObjectConstructParam *construct_properties)
{
        CcBackgroundItem      *background_item;

        background_item = CC_BACKGROUND_ITEM (G_OBJECT_CLASS (cc_background_item_parent_class)->constructor (type,
                                                                                                                         n_construct_properties,
                                                                                                                         construct_properties));

        return G_OBJECT (background_item);
}

static void
cc_background_item_class_init (CcBackgroundItemClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_background_item_get_property;
        object_class->set_property = cc_background_item_set_property;
        object_class->constructor = cc_background_item_constructor;
        object_class->finalize = cc_background_item_finalize;

        g_object_class_install_property (object_class,
                                         PROP_NAME,
                                         g_param_spec_string ("name",
                                                              "name",
                                                              "name",
                                                              NULL,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_URI,
                                         g_param_spec_string ("uri",
                                                              "uri",
                                                              "uri",
                                                              NULL,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_PLACEMENT,
					 g_param_spec_enum ("placement",
							    "placement",
							    "placement",
							    G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE,
							    G_DESKTOP_BACKGROUND_STYLE_SCALED,
							    G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SHADING,
                                         g_param_spec_enum ("shading",
							    "shading",
							    "shading",
							    G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING,
							    G_DESKTOP_BACKGROUND_SHADING_SOLID,
							    G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_PRIMARY_COLOR,
                                         g_param_spec_string ("primary-color",
                                                              "primary-color",
                                                              "primary-color",
                                                              "#000000000000",
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_SECONDARY_COLOR,
                                         g_param_spec_string ("secondary-color",
                                                              "secondary-color",
                                                              "secondary-color",
                                                              "#000000000000",
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_IS_DELETED,
                                         g_param_spec_boolean ("is-deleted",
                                                               NULL,
                                                               NULL,
                                                               FALSE,
                                                               G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SOURCE_URL,
                                         g_param_spec_string ("source-url",
                                                              "source-url",
                                                              "source-url",
                                                              NULL,
                                                              G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SOURCE_XML,
                                         g_param_spec_string ("source-xml",
                                                              "source-xml",
                                                              "source-xml",
                                                              NULL,
                                                              G_PARAM_READWRITE));

	g_object_class_install_property (object_class,
					 PROP_FLAGS,
					 g_param_spec_flags ("flags",
							     "flags",
							     "flags",
							     G_DESKTOP_TYPE_BACKGROUND_ITEM_FLAGS,
							     0,
							     G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_SIZE,
                                         g_param_spec_string ("size",
                                                              "size",
                                                              "size",
                                                              NULL,
                                                              G_PARAM_READABLE));

        g_object_class_install_property (object_class,
                                         PROP_NEEDS_DOWNLOAD,
                                         g_param_spec_boolean ("needs-download",
                                                               NULL,
                                                               NULL,
                                                               TRUE,
                                                               G_PARAM_READWRITE));

        g_object_class_install_property (object_class,
                                         PROP_MODIFIED,
                                         g_param_spec_uint64 ("modified",
                                                              "modified",
                                                              NULL,
                                                              0,
                                                              G_MAXUINT64,
                                                              0,
                                                              G_PARAM_READWRITE));
}

static void
cc_background_item_init (CcBackgroundItem *item)
{
        item->bg = gnome_bg_new ();

        item->shading = G_DESKTOP_BACKGROUND_SHADING_SOLID;
        item->placement = G_DESKTOP_BACKGROUND_STYLE_SCALED;
        item->primary_color = g_strdup ("#000000000000");
        item->secondary_color = g_strdup ("#000000000000");
        item->needs_download = TRUE;
        item->flags = 0;
        item->modified = 0;
}

static void
cc_background_item_finalize (GObject *object)
{
        CcBackgroundItem *item;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_BACKGROUND_ITEM (object));

        item = CC_BACKGROUND_ITEM (object);

        g_return_if_fail (item != NULL);

        g_free (item->name);
        g_free (item->uri);
        g_free (item->primary_color);
        g_free (item->secondary_color);
        g_free (item->mime_type);
        g_free (item->size);
        g_free (item->source_url);
        g_free (item->source_xml);

        if (item->bg != NULL)
                g_object_unref (item->bg);

        g_clear_object (&item->slideshow_emblem);

        G_OBJECT_CLASS (cc_background_item_parent_class)->finalize (object);
}

CcBackgroundItem *
cc_background_item_new (const char *uri)
{
        GObject *object;

        object = g_object_new (CC_TYPE_BACKGROUND_ITEM,
                               "uri", uri,
                               NULL);

        return CC_BACKGROUND_ITEM (object);
}

CcBackgroundItem *
cc_background_item_copy (CcBackgroundItem *item)
{
	CcBackgroundItem *ret;

	ret = cc_background_item_new (item->uri);
	ret->name = g_strdup (item->name);
	ret->size = g_strdup (item->size);
	ret->placement = item->placement;
	ret->shading = item->shading;
	ret->primary_color = g_strdup (item->primary_color);
	ret->secondary_color = g_strdup (item->secondary_color);
	ret->source_url = g_strdup (item->source_url);
	ret->source_xml = g_strdup (item->source_xml);
	ret->is_deleted = item->is_deleted;
	ret->needs_download = item->needs_download;
	ret->flags = item->flags;

	return ret;
}

static const char *
flags_to_str (CcBackgroundItemFlags flag)
{
	GFlagsClass *fclass;
	GFlagsValue *value;

	fclass = G_FLAGS_CLASS (g_type_class_peek (G_DESKTOP_TYPE_BACKGROUND_ITEM_FLAGS));
	value = g_flags_get_first_value (fclass, flag);

	g_assert (value);

	return value->value_nick;
}

static const char *
enum_to_str (GType type,
	     int   v)
{
	GEnumClass *eclass;
	GEnumValue *value;

	eclass = G_ENUM_CLASS (g_type_class_peek (type));
	value = g_enum_get_value (eclass, v);

	g_assert (value);

	return value->value_nick;
}

void
cc_background_item_dump (CcBackgroundItem *item)
{
	GString *flags;
	int i;

	g_return_if_fail (CC_IS_BACKGROUND_ITEM (item));

	g_debug ("name:\t\t\t%s", item->name);
	g_debug ("URI:\t\t\t%s", item->uri ? item->uri : "NULL");
	if (item->size)
		g_debug ("size:\t\t\t'%s'", item->size);
	flags = g_string_new (NULL);
	for (i = 0; i < 5; i++) {
		if (item->flags & (1 << i)) {
			g_string_append (flags, flags_to_str (1 << i));
			g_string_append_c (flags, ' ');
		}
	}
	if (flags->len == 0)
		g_string_append (flags, "-none-");
	g_debug ("flags:\t\t\t%s", flags->str);
	g_string_free (flags, TRUE);
	if (item->primary_color)
		g_debug ("pcolor:\t\t\t%s", item->primary_color);
	if (item->secondary_color)
		g_debug ("scolor:\t\t\t%s", item->secondary_color);
	g_debug ("placement:\t\t%s", enum_to_str (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE, item->placement));
	g_debug ("shading:\t\t%s", enum_to_str (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING, item->shading));
	if (item->source_url)
		g_debug ("source URL:\t\t%s", item->source_url);
	if (item->source_xml)
		g_debug ("source XML:\t\t%s", item->source_xml);
	g_debug ("deleted:\t\t%s", item->is_deleted ? "yes" : "no");
	if (item->mime_type)
		g_debug ("mime-type:\t\t%s", item->mime_type);
	g_debug ("dimensions:\t\t%d x %d", item->width, item->height);
        g_debug ("modified: %"G_GUINT64_FORMAT, item->modified);
	g_debug (" ");
}

static gboolean
files_equal (const char *a,
	     const char *b)
{
	GFile *file1, *file2;
	gboolean retval;

	if (a == NULL &&
	    b == NULL)
		return TRUE;

	if (a == NULL ||
	    b == NULL)
		return FALSE;

	file1 = g_file_new_for_commandline_arg (a);
	file2 = g_file_new_for_commandline_arg (b);
	if (g_file_equal (file1, file2) == FALSE)
		retval = FALSE;
	else
		retval = TRUE;
	g_object_unref (file1);
	g_object_unref (file2);

	return retval;
}

static gboolean
colors_equal (const char *a,
	      const char *b)
{
	GdkColor color1, color2;

	gdk_color_parse (a, &color1);
	gdk_color_parse (b, &color2);

	return gdk_color_equal (&color1, &color2);
}

gboolean
cc_background_item_compare (CcBackgroundItem *saved,
			    CcBackgroundItem *configured)
{
	CcBackgroundItemFlags flags;

	flags = saved->flags;
	if (flags == 0)
		return FALSE;

	if (flags & CC_BACKGROUND_ITEM_HAS_URI) {
		if (files_equal (saved->uri, configured->uri) == FALSE)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_SHADING) {
		if (saved->shading != configured->shading)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT) {
		if (saved->placement != configured->placement)
			return FALSE;
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_PCOLOR) {
		if (colors_equal (saved->primary_color,
				  configured->primary_color) == FALSE) {
			return FALSE;
		}
	}
	if (flags & CC_BACKGROUND_ITEM_HAS_SCOLOR) {
		if (colors_equal (saved->secondary_color,
				  configured->secondary_color) == FALSE) {
			return FALSE;
		}
	}

	return TRUE;
}
