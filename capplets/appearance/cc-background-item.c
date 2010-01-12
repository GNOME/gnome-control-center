/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include <gconf/gconf-client.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-bg.h>

#include "cc-background-item.h"

#define CC_BACKGROUND_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BACKGROUND_ITEM, CcBackgroundItemPrivate))

struct CcBackgroundItemPrivate
{
        /* properties */
        char            *name;
        char            *filename;
        char            *description;
        char            *placement;
        char            *shading;
        char            *primary_color;
        char            *secondary_color;
        gboolean         is_deleted;

        /* internal */
        GnomeBG         *bg;
        char            *mime_type;
        int              width;
        int              height;
};

enum {
        PROP_0,
        PROP_NAME,
        PROP_FILENAME,
        PROP_DESCRIPTION,
        PROP_PLACEMENT,
        PROP_SHADING,
        PROP_PRIMARY_COLOR,
        PROP_SECONDARY_COLOR,
        PROP_IS_DELETED,
};

enum {
        CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     cc_background_item_class_init     (CcBackgroundItemClass *klass);
static void     cc_background_item_init           (CcBackgroundItem      *background_item);
static void     cc_background_item_finalize       (GObject               *object);

G_DEFINE_TYPE (CcBackgroundItem, cc_background_item, G_TYPE_OBJECT)

static GConfEnumStringPair placement_lookup[] = {
        { GNOME_BG_PLACEMENT_CENTERED, "centered" },
        { GNOME_BG_PLACEMENT_FILL_SCREEN, "stretched" },
        { GNOME_BG_PLACEMENT_SCALED, "scaled" },
        { GNOME_BG_PLACEMENT_ZOOMED, "zoom" },
        { GNOME_BG_PLACEMENT_TILED, "wallpaper" },
        { 0, NULL }
};

static GConfEnumStringPair shading_lookup[] = {
        { GNOME_BG_COLOR_SOLID, "solid" },
        { GNOME_BG_COLOR_H_GRADIENT, "horizontal-gradient" },
        { GNOME_BG_COLOR_V_GRADIENT, "vertical-gradient" },
        { 0, NULL }
};

static const char *
placement_to_string (GnomeBGPlacement type)
{
        return gconf_enum_to_string (placement_lookup, type);
}

static const char *
shading_to_string (GnomeBGColorType type)
{
        return gconf_enum_to_string (shading_lookup, type);
}

static GnomeBGPlacement
string_to_placement (const char *option)
{
        int i = GNOME_BG_PLACEMENT_SCALED;
        gconf_string_to_enum (placement_lookup, option, &i);
        return i;
}

static GnomeBGColorType
string_to_shading (const char *shading)
{
        int i = GNOME_BG_COLOR_SOLID;
        gconf_string_to_enum (shading_lookup, shading, &i);
        return i;
}

static GdkPixbuf *
add_slideshow_frame (GdkPixbuf *pixbuf)
{
        GdkPixbuf *sheet;
        GdkPixbuf *sheet2;
        GdkPixbuf *tmp;
        int        w;
        int        h;

        w = gdk_pixbuf_get_width (pixbuf);
        h = gdk_pixbuf_get_height (pixbuf);

        sheet = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, w, h);
        gdk_pixbuf_fill (sheet, 0x00000000);
        sheet2 = gdk_pixbuf_new_subpixbuf (sheet, 1, 1, w - 2, h - 2);
        gdk_pixbuf_fill (sheet2, 0xffffffff);
        g_object_unref (sheet2);

        tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w + 6, h + 6);

        gdk_pixbuf_fill (tmp, 0x00000000);
        gdk_pixbuf_composite (sheet, tmp, 6, 6, w, h, 6.0, 6.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
        gdk_pixbuf_composite (sheet, tmp, 3, 3, w, h, 3.0, 3.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
        gdk_pixbuf_composite (pixbuf, tmp, 0, 0, w, h, 0.0, 0.0, 1.0, 1.0, GDK_INTERP_NEAREST, 255);

        g_object_unref (sheet);

        return tmp;
}

static void
set_bg_properties (CcBackgroundItem *item)
{
        int      shading;
        int      placement;
        GdkColor pcolor = { 0, 0, 0, 0 };
        GdkColor scolor = { 0, 0, 0, 0 };

        if (item->priv->filename)
                gnome_bg_set_filename (item->priv->bg, item->priv->filename);

        gdk_color_parse (item->priv->primary_color, &pcolor);
        gdk_color_parse (item->priv->secondary_color, &scolor);
        placement = string_to_placement (item->priv->placement);
        shading = string_to_shading (item->priv->shading);

        gnome_bg_set_color (item->priv->bg, shading, &pcolor, &scolor);
        gnome_bg_set_placement (item->priv->bg, placement);
}


gboolean
cc_background_item_changes_with_time (CcBackgroundItem *item)
{
        gboolean changes;

        changes = FALSE;
        if (item->priv->bg != NULL) {
                changes = gnome_bg_changes_with_time (item->priv->bg);
        }
        return changes;
}

static void
update_description (CcBackgroundItem *item)
{
        g_free (item->priv->description);
        item->priv->description = NULL;

        if (strcmp (item->priv->filename, "(none)") == 0) {
                item->priv->description = g_strdup (item->priv->name);
        } else {
                const char *description;
                char       *size;
                char       *dirname;

                dirname = g_path_get_dirname (item->priv->filename);

                description = NULL;
                size = NULL;

                if (item->priv->mime_type != NULL) {
                        if (strcmp (item->priv->mime_type, "application/xml") == 0) {
                                if (gnome_bg_changes_with_time (item->priv->bg))
                                        description = _("Slide Show");
                                else if (item->priv->width > 0 && item->priv->height > 0)
                                        description = _("Image");
                        } else {
                                description = g_content_type_get_description (item->priv->mime_type);
                        }
                }

                if (gnome_bg_has_multiple_sizes (item->priv->bg)) {
                        size = g_strdup (_("multiple sizes"));
                } else if (item->priv->width > 0 && item->priv->height > 0) {
                        /* translators: x pixel(s) by y pixel(s) */
                        size = g_strdup_printf ("%d %s by %d %s",
                                                item->priv->width,
                                                ngettext ("pixel", "pixels", item->priv->width),
                                                item->priv->height,
                                                ngettext ("pixel", "pixels", item->priv->height));
                }

                if (description != NULL && size != NULL) {
                        /* translators: <b>wallpaper name</b>
                         * mime type, size
                         * Folder: /path/to/file
                         */
                        item->priv->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                                                             "%s, %s\n"
                                                                             "Folder: %s"),
                                                                           item->priv->name,
                                                                           description,
                                                                           size,
                                                                           dirname);
                } else {
                        /* translators: <b>wallpaper name</b>
                         * Image missing
                         * Folder: /path/to/file
                         */
                        item->priv->description = g_markup_printf_escaped (_("<b>%s</b>\n"
                                                                             "%s\n"
                                                                             "Folder: %s"),
                                                                           item->priv->name,
                                                                           _("Image missing"),
                                                                           dirname);
                }

                g_free (size);
                g_free (dirname);
        }
}

GdkPixbuf *
cc_background_item_get_frame_thumbnail (CcBackgroundItem             *item,
                                        GnomeDesktopThumbnailFactory *thumbs,
                                        int                           width,
                                        int                           height,
                                        int                           frame)
{
        GdkPixbuf *pixbuf = NULL;

        set_bg_properties (item);

        if (frame != -1)
                pixbuf = gnome_bg_create_frame_thumbnail (item->priv->bg,
                                                          thumbs,
                                                          gdk_screen_get_default (),
                                                          width,
                                                          height,
                                                          frame);
        else
                pixbuf = gnome_bg_create_thumbnail (item->priv->bg,
                                                    thumbs,
                                                    gdk_screen_get_default(),
                                                    width,
                                                    height);

        if (pixbuf != NULL
            && gnome_bg_changes_with_time (item->priv->bg)) {
                GdkPixbuf *tmp;

                tmp = add_slideshow_frame (pixbuf);
                g_object_unref (pixbuf);
                pixbuf = tmp;
        }

        gnome_bg_get_image_size (item->priv->bg,
                                 thumbs,
                                 width,
                                 height,
                                 &item->priv->width,
                                 &item->priv->height);

        update_description (item);

        return pixbuf;
}


GdkPixbuf *
cc_background_item_get_thumbnail (CcBackgroundItem             *item,
                                  GnomeDesktopThumbnailFactory *thumbs,
                                  int                           width,
                                  int                           height)
{
        return cc_background_item_get_frame_thumbnail (item, thumbs, width, height, -1);
}

static void
update_info (CcBackgroundItem *item)
{
        GFile     *file;
        GFileInfo *info;

        file = g_file_new_for_commandline_arg (item->priv->filename);

        info = g_file_query_info (file,
                                  G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                  G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                  G_FILE_QUERY_INFO_NONE,
                                  NULL,
                                  NULL);
        g_object_unref (file);

        g_free (item->priv->mime_type);
        item->priv->mime_type = NULL;

        if (info == NULL
            || g_file_info_get_content_type (info) == NULL) {
                if (strcmp (item->priv->filename, "(none)") == 0) {
                        item->priv->mime_type = g_strdup ("image/x-no-data");
                        g_free (item->priv->name);
                        item->priv->name = g_strdup (_("No Desktop Background"));
                        //item->priv->size = 0;
                }
        } else {
                if (item->priv->name == NULL) {
                        const char *name;

                        g_free (item->priv->name);

                        name = g_file_info_get_name (info);
                        if (g_utf8_validate (name, -1, NULL))
                                item->priv->name = g_strdup (name);
                        else
                                item->priv->name = g_filename_to_utf8 (name,
                                                                       -1,
                                                                       NULL,
                                                                       NULL,
                                                                       NULL);
                }

                item->priv->mime_type = g_strdup (g_file_info_get_content_type (info));

#if 0
                item->priv->size = g_file_info_get_size (info);
                item->priv->mtime = g_file_info_get_attribute_uint64 (info,
                                                                      G_FILE_ATTRIBUTE_TIME_MODIFIED);
#endif
        }

        if (info != NULL)
                g_object_unref (info);

}

static void
on_bg_changed (GnomeBG          *bg,
               CcBackgroundItem *item)
{
        g_signal_emit (item, signals[CHANGED], 0);
}

gboolean
cc_background_item_load (CcBackgroundItem *item)
{
        gboolean ret;

        g_return_val_if_fail (item != NULL, FALSE);

        update_info (item);

        ret = FALSE;
        if (item->priv->mime_type != NULL
            && (g_str_has_prefix (item->priv->mime_type, "image/")
                || strcmp (item->priv->mime_type, "application/xml") == 0)) {
                ret = TRUE;

                set_bg_properties (item);
        } else {
                /* FIXME: return error message? */
                /* unknown mime type */
        }

        update_description (item);

        return TRUE;
}

static void
_set_name (CcBackgroundItem *item,
           const char       *value)
{
        g_free (item->priv->name);
        item->priv->name = g_strdup (value);
}

static void
_set_filename (CcBackgroundItem *item,
               const char       *value)
{
        g_free (item->priv->filename);
        item->priv->filename = g_strdup (value);
}

static void
_set_description (CcBackgroundItem *item,
                  const char       *value)
{
        g_free (item->priv->description);
        item->priv->description = g_strdup (value);
}

static void
_set_placement (CcBackgroundItem *item,
                const char       *value)
{
        g_free (item->priv->placement);
        item->priv->placement = g_strdup (value);
}

static void
_set_shading (CcBackgroundItem *item,
              const char       *value)
{
        g_free (item->priv->shading);
        item->priv->shading = g_strdup (value);
}

static void
_set_primary_color (CcBackgroundItem *item,
                    const char       *value)
{
        g_free (item->priv->primary_color);
        item->priv->primary_color = g_strdup (value);
}

static void
_set_secondary_color (CcBackgroundItem *item,
                      const char       *value)
{
        g_free (item->priv->secondary_color);
        item->priv->secondary_color = g_strdup (value);
}

static void
_set_is_deleted (CcBackgroundItem *item,
                 gboolean          value)
{
        item->priv->is_deleted = value;
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
        case PROP_FILENAME:
                _set_filename (self, g_value_get_string (value));
                break;
        case PROP_DESCRIPTION:
                _set_description (self, g_value_get_string (value));
                break;
        case PROP_PLACEMENT:
                _set_placement (self, g_value_get_string (value));
                break;
        case PROP_SHADING:
                _set_shading (self, g_value_get_string (value));
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
                g_value_set_string (value, self->priv->name);
                break;
        case PROP_FILENAME:
                g_value_set_string (value, self->priv->filename);
                break;
        case PROP_DESCRIPTION:
                g_value_set_string (value, self->priv->description);
                break;
        case PROP_PLACEMENT:
                g_value_set_string (value, self->priv->placement);
                break;
        case PROP_SHADING:
                g_value_set_string (value, self->priv->shading);
                break;
        case PROP_PRIMARY_COLOR:
                g_value_set_string (value, self->priv->primary_color);
                break;
        case PROP_SECONDARY_COLOR:
                g_value_set_string (value, self->priv->secondary_color);
                break;
        case PROP_IS_DELETED:
                g_value_set_boolean (value, self->priv->is_deleted);
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

        signals [CHANGED]
                = g_signal_new ("changed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE,
                                0);

        g_object_class_install_property (object_class,
                                         PROP_NAME,
                                         g_param_spec_string ("name",
                                                              "name",
                                                              "name",
                                                              NULL,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_FILENAME,
                                         g_param_spec_string ("filename",
                                                              "filename",
                                                              "filename",
                                                              NULL,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_DESCRIPTION,
                                         g_param_spec_string ("description",
                                                              "description",
                                                              "description",
                                                              NULL,
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_PLACEMENT,
                                         g_param_spec_string ("placement",
                                                              "placement",
                                                              "placement",
                                                              "scaled",
                                                              G_PARAM_READWRITE));
        g_object_class_install_property (object_class,
                                         PROP_SHADING,
                                         g_param_spec_string ("shading",
                                                              "shading",
                                                              "shading",
                                                              "solid",
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

        g_type_class_add_private (klass, sizeof (CcBackgroundItemPrivate));
}

static void
cc_background_item_init (CcBackgroundItem *item)
{
        item->priv = CC_BACKGROUND_ITEM_GET_PRIVATE (item);

        item->priv->bg = gnome_bg_new ();

        g_signal_connect (item->priv->bg,
                          "changed",
                          G_CALLBACK (on_bg_changed),
                          item);

        item->priv->shading = g_strdup ("solid");
        item->priv->placement = g_strdup ("scaled");
        item->priv->primary_color = g_strdup ("#000000000000");
        item->priv->secondary_color = g_strdup ("#000000000000");
}

static void
cc_background_item_finalize (GObject *object)
{
        CcBackgroundItem *item;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_BACKGROUND_ITEM (object));

        item = CC_BACKGROUND_ITEM (object);

        g_return_if_fail (item->priv != NULL);

        g_free (item->priv->name);
        g_free (item->priv->filename);
        g_free (item->priv->description);
        g_free (item->priv->primary_color);
        g_free (item->priv->secondary_color);
        g_free (item->priv->mime_type);

        if (item->priv->bg != NULL)
                g_object_unref (item->priv->bg);

        G_OBJECT_CLASS (cc_background_item_parent_class)->finalize (object);
}

CcBackgroundItem *
cc_background_item_new (const char *filename)
{
        GObject *object;

        object = g_object_new (CC_TYPE_BACKGROUND_ITEM,
                               "filename", filename,
                               NULL);

        return CC_BACKGROUND_ITEM (object);
}
