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
#include <libgnomeui/gnome-desktop-thumbnail.h>

#include "cc-background-page.h"
#include "cc-background-item.h"
#include "cc-backgrounds-monitor.h"

#define CC_BACKGROUND_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BACKGROUND_PAGE, CcBackgroundPagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))
#define BACKGROUNDS_DIR "/usr/share/backgrounds"

#define WP_PATH_KEY "/desktop/gnome/background"
#define WP_FILE_KEY WP_PATH_KEY "/picture_filename"
#define WP_OPTIONS_KEY WP_PATH_KEY "/picture_options"
#define WP_SHADING_KEY WP_PATH_KEY "/color_shading_type"
#define WP_PCOLOR_KEY WP_PATH_KEY "/primary_color"
#define WP_SCOLOR_KEY WP_PATH_KEY "/secondary_color"

struct CcBackgroundPagePrivate
{
        CcBackgroundsMonitor *monitor;
        GtkTreeModel *model;
        GtkWidget    *icon_view;
        GtkWidget    *remove_button;
        GtkWidget    *color_menu;
        GtkWidget    *style_menu;
        GtkWidget    *primary_color_picker;
        GtkWidget    *secondary_color_picker;
        GtkWidget    *file_chooser;
        GtkWidget    *file_chooser_preview;

        GnomeDesktopThumbnailFactory *thumb_factory;

        int           frame;
        int           thumb_width;
        int           thumb_height;

        gulong        screen_size_handler;
        gulong        screen_monitors_handler;
};

enum {
        PROP_0,
};

static void     cc_background_page_class_init     (CcBackgroundPageClass *klass);
static void     cc_background_page_init           (CcBackgroundPage      *background_page);
static void     cc_background_page_finalize       (GObject             *object);

G_DEFINE_TYPE (CcBackgroundPage, cc_background_page, CC_TYPE_PAGE)

enum {
        TARGET_URI_LIST,
        TARGET_BGIMAGE
};

static const GtkTargetEntry drop_types[] = {
        { "text/uri-list", 0, TARGET_URI_LIST },
        { "property/bgimage", 0, TARGET_BGIMAGE }
};

static const GtkTargetEntry drag_types[] = {
        {"text/uri-list", GTK_TARGET_OTHER_WIDGET, TARGET_URI_LIST}
};

enum {
        COL_PIXBUF,
        COL_ITEM,
};

enum {
        SHADE_SOLID = 0,
        SHADE_H_GRADIENT,
        SHADE_V_GRADIENT,
};

enum {
        SCALE_TILE = 0,
        SCALE_ZOOM,
        SCALE_CENTER,
        SCALE_SCALE,
        SCALE_STRETCH,
};

static GConfEnumStringPair options_lookup[] = {
        { 0, "wallpaper" },
        { 1, "zoom" },
        { 2, "centered" },
        { 3, "scaled" },
        { 4, "stretched" },
        { 0, NULL }
};

static GConfEnumStringPair shade_lookup[] = {
        { 0, "solid" },
        { 1, "horizontal-gradient" },
        { 2, "vertical-gradient" },
        { 0, NULL }
};

static const char *
option_to_string (int type)
{
        return gconf_enum_to_string (options_lookup, type);
}

static const char *
shading_to_string (int type)
{
        return gconf_enum_to_string (shade_lookup, type);
}

static int
string_to_option (const char *option)
{
        int i = 3;
        gconf_string_to_enum (options_lookup, option, &i);
        return i;
}

static int
string_to_shade (const char *shade_type)
{
        int i = 0;
        gconf_string_to_enum (shade_lookup, shade_type, &i);
        return i;
}

static void
cc_background_page_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_background_page_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
on_style_menu_changed (GtkComboBox      *combobox,
                       CcBackgroundPage *page)
{
        GConfClient *client;
        int          options;

        client = gconf_client_get_default ();
        options = gtk_combo_box_get_active (GTK_COMBO_BOX (page->priv->style_menu));
        if (gconf_client_key_is_writable (client, WP_OPTIONS_KEY, NULL))
                gconf_client_set_string (client,
                                         WP_OPTIONS_KEY,
                                         option_to_string (options),
                                         NULL);
        g_object_unref (client);
}

static void
on_color_menu_changed (GtkWidget        *combobox,
                       CcBackgroundPage *page)
{
        GConfClient *client;
        int          shade_type;

        client = gconf_client_get_default ();
        shade_type = gtk_combo_box_get_active (GTK_COMBO_BOX (page->priv->color_menu));
        if (gconf_client_key_is_writable (client, WP_SHADING_KEY, NULL))
                gconf_client_set_string (client,
                                         WP_SHADING_KEY,
                                         shading_to_string (shade_type),
                                         NULL);
        g_object_unref (client);
}

static void
on_color_changed (GtkWidget        *widget,
                  CcBackgroundPage *page)
{
        GConfClient *client;
        GdkColor     pcolor;
        GdkColor     scolor;
        char        *spcolor;
        char        *sscolor;

        gtk_color_button_get_color (GTK_COLOR_BUTTON (page->priv->primary_color_picker),
                                    &pcolor);
        gtk_color_button_get_color (GTK_COLOR_BUTTON (page->priv->secondary_color_picker),
                                    &scolor);

        spcolor = gdk_color_to_string (&pcolor);
        sscolor = gdk_color_to_string (&scolor);

        client = gconf_client_get_default ();
        gconf_client_set_string (client, WP_PCOLOR_KEY, spcolor, NULL);
        gconf_client_set_string (client, WP_SCOLOR_KEY, sscolor, NULL);
        g_object_unref (client);

        g_free (spcolor);
        g_free (sscolor);
}

static GdkPixbuf *buttons[3];

static void
create_button_images (CcBackgroundPage *page)
{
        GtkIconSet *icon_set;
        GdkPixbuf  *pixbuf;
        GdkPixbuf  *pb;
        GdkPixbuf  *pb2;
        int         i, w, h;

        icon_set = gtk_style_lookup_icon_set (page->priv->icon_view->style, "gtk-media-play");
        pb = gtk_icon_set_render_icon (icon_set,
                                       page->priv->icon_view->style,
                                       GTK_TEXT_DIR_RTL,
                                       GTK_STATE_NORMAL,
                                       GTK_ICON_SIZE_MENU,
                                       page->priv->icon_view,
                                       NULL);
        pb2 = gtk_icon_set_render_icon (icon_set,
                                        page->priv->icon_view->style,
                                        GTK_TEXT_DIR_LTR,
                                        GTK_STATE_NORMAL,
                                        GTK_ICON_SIZE_MENU,
                                        page->priv->icon_view,
                                        NULL);
        w = gdk_pixbuf_get_width (pb);
        h = gdk_pixbuf_get_height (pb);

        for (i = 0; i < 3; i++) {
                pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 2 * w, h);
                gdk_pixbuf_fill (pixbuf, 0);
                if (i > 0)
                        gdk_pixbuf_composite (pb, pixbuf, 0, 0, w, h, 0, 0, 1, 1, GDK_INTERP_NEAREST, 255);
                if (i < 2)
                        gdk_pixbuf_composite (pb2, pixbuf, w, 0, w, h, w, 0, 1, 1, GDK_INTERP_NEAREST, 255);

                buttons[i] = pixbuf;
        }

        g_object_unref (pb);
        g_object_unref (pb2);
}

static CcBackgroundItem *
get_selected_item (CcBackgroundPage *page,
                   GtkTreeIter      *piter)
{
        CcBackgroundItem *item;
        GList            *selected;
        GtkTreeIter       iter;
        gboolean          res;

        item = NULL;

        selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (page->priv->icon_view));

        if (selected == NULL) {
                return NULL;
        }

        res = gtk_tree_model_get_iter (page->priv->model,
                                       &iter,
                                       selected->data);
        g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (selected);

        if (res) {
                if (piter != NULL) {
                        *piter = iter;
                }

                gtk_tree_model_get (page->priv->model, &iter, COL_ITEM, &item, -1);
        }

        return item;
}

static void
ui_update_sensitivities (CcBackgroundPage *page)
{
        GConfClient      *client;
        CcBackgroundItem *item;
        char             *filename;

        item = get_selected_item (page, NULL);
        filename = NULL;
        if (item != NULL) {
                g_object_get (item, "filename", &filename, NULL);
                g_object_unref (item);
        }

        client = gconf_client_get_default ();
        if (!gconf_client_key_is_writable (client, WP_OPTIONS_KEY, NULL)
            || (filename != NULL && !strcmp (filename, "(none)")))
                gtk_widget_set_sensitive (page->priv->style_menu, FALSE);
        else
                gtk_widget_set_sensitive (page->priv->style_menu, TRUE);

        if (!gconf_client_key_is_writable (client, WP_SHADING_KEY, NULL))
                gtk_widget_set_sensitive (page->priv->color_menu, FALSE);
        else
                gtk_widget_set_sensitive (page->priv->color_menu, TRUE);

        if (!gconf_client_key_is_writable (client, WP_PCOLOR_KEY, NULL))
                gtk_widget_set_sensitive (page->priv->primary_color_picker, FALSE);
        else
                gtk_widget_set_sensitive (page->priv->primary_color_picker, TRUE);

        if (!gconf_client_key_is_writable (client, WP_SCOLOR_KEY, NULL))
                gtk_widget_set_sensitive (page->priv->secondary_color_picker, FALSE);
        else
                gtk_widget_set_sensitive (page->priv->secondary_color_picker, TRUE);

        if (filename == NULL || strcmp (filename, "(none)") == 0)
                gtk_widget_set_sensitive (page->priv->remove_button, FALSE);
        else
                gtk_widget_set_sensitive (page->priv->remove_button, TRUE);

        g_object_unref (client);
        g_free (filename);
}

static gboolean
find_uri_in_model (GtkTreeModel *model,
                   const char   *uri,
                   GtkTreeIter  *piter)
{
        GtkTreeIter iter;
        gboolean    valid;

        if (uri == NULL) {
                return FALSE;
        }

        for (valid = gtk_tree_model_get_iter_first (model, &iter);
             valid;
             valid = gtk_tree_model_iter_next (model, &iter)) {
                CcBackgroundItem *item;
                char             *filename;

                item = NULL;
                gtk_tree_model_get (model, &iter, COL_ITEM, &item, -1);
                if (item == NULL)
                        continue;

                filename = NULL;
                g_object_get (item, "filename", &filename, NULL);
                g_object_unref (item);

                if (filename != NULL) {
                        int cmp = strcmp (filename, uri);
                        g_free (filename);

                        if (cmp == 0) {
                                if (piter != NULL)
                                        *piter = iter;
                                return TRUE;
                        }
                }
        }

        return FALSE;
}

static void
on_item_changed (CcBackgroundItem *item,
                 CcBackgroundPage *page)
{
        GtkTreeIter iter;
        char       *uri;

        uri = NULL;
        g_object_get (item, "filename", &uri, NULL);
        if (find_uri_in_model (page->priv->model, uri, &iter)) {
                GdkPixbuf *pixbuf;

                g_signal_handlers_block_by_func (item, G_CALLBACK (on_item_changed), page);

                pixbuf = cc_background_item_get_thumbnail (item,
                                                           page->priv->thumb_factory,
                                                           page->priv->thumb_width,
                                                           page->priv->thumb_height);
                if (pixbuf != NULL) {
                        gtk_list_store_set (GTK_LIST_STORE (page->priv->model),
                                            &iter,
                                            COL_PIXBUF, pixbuf,
                                            -1);
                        g_object_unref (pixbuf);
                }

                g_signal_handlers_unblock_by_func (item, G_CALLBACK (on_item_changed), page);
        }

        g_free (uri);
}

static void
load_item (CcBackgroundPage *page,
           CcBackgroundItem *item)
{
        GtkTreeIter  iter;
        GdkPixbuf   *pixbuf;
        gboolean     deleted;

        g_signal_connect (item,
                          "changed",
                          G_CALLBACK (on_item_changed),
                          page);

        g_object_get (item, "is-deleted", &deleted, NULL);
        if (deleted == TRUE)
                return;

        gtk_list_store_append (GTK_LIST_STORE (page->priv->model), &iter);

        pixbuf = cc_background_item_get_thumbnail (item,
                                                   page->priv->thumb_factory,
                                                   page->priv->thumb_width,
                                                   page->priv->thumb_height);

        gtk_list_store_set (GTK_LIST_STORE (page->priv->model),
                            &iter,
                            COL_PIXBUF, pixbuf,
                            COL_ITEM, item,
                            -1);

        if (pixbuf != NULL)
                g_object_unref (pixbuf);

}

static void
load_item_iter (CcBackgroundItem *item,
                CcBackgroundPage *page)
{
        load_item (page, item);
}

static gboolean
reload_item (GtkTreeModel     *model,
             GtkTreePath      *path,
             GtkTreeIter      *iter,
             CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        GdkPixbuf        *pixbuf;

        gtk_tree_model_get (model, iter, COL_ITEM, &item, -1);

        pixbuf = cc_background_item_get_thumbnail (item,
                                                   page->priv->thumb_factory,
                                                   page->priv->thumb_width,
                                                   page->priv->thumb_height);
        g_object_unref (item);

        if (pixbuf) {
                gtk_list_store_set (GTK_LIST_STORE (page->priv->model),
                                    iter,
                                    COL_PIXBUF, pixbuf,
                                    -1);
                g_object_unref (pixbuf);
        }

        return FALSE;
}

static gdouble
get_monitor_aspect_ratio_for_widget (GtkWidget *widget)
{
        gdouble      aspect;
        int          monitor;
        GdkRectangle rect;

        monitor = gdk_screen_get_monitor_at_window (gtk_widget_get_screen (widget),
                                                    gtk_widget_get_window (widget));
        gdk_screen_get_monitor_geometry (gtk_widget_get_screen (widget), monitor, &rect);
        aspect = rect.height / (gdouble)rect.width;

        return aspect;
}

#define LIST_IMAGE_SIZE 108

static void
compute_thumbnail_sizes (CcBackgroundPage *page)
{
        gdouble aspect;

        aspect = get_monitor_aspect_ratio_for_widget (page->priv->icon_view);
        if (aspect > 1) {
                /* portrait */
                page->priv->thumb_width = LIST_IMAGE_SIZE / aspect;
                page->priv->thumb_height = LIST_IMAGE_SIZE;
        } else {
                page->priv->thumb_width = LIST_IMAGE_SIZE;
                page->priv->thumb_height = LIST_IMAGE_SIZE * aspect;
        }
}

static void
reload_wallpapers (CcBackgroundPage *page)
{
        compute_thumbnail_sizes (page);
        gtk_tree_model_foreach (page->priv->model,
                                (GtkTreeModelForeachFunc)reload_item,
                                page);
}

static void
select_item (CcBackgroundPage *page,
             CcBackgroundItem *item,
             gboolean          scroll)
{
        GtkTreeIter  iter;
        char        *uri;

        if (item == NULL)
                return;

        uri = NULL;
        g_object_get (item, "filename", &uri, NULL);
        g_debug ("Selecting item %s", uri);
        if (find_uri_in_model (page->priv->model, uri, &iter)) {
                GtkTreePath *path;

                path = gtk_tree_model_get_path (page->priv->model, &iter);
                gtk_icon_view_select_path (GTK_ICON_VIEW (page->priv->icon_view), path);
                if (scroll)
                        gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (page->priv->icon_view), path, FALSE, 0.5, 0.0);

                gtk_tree_path_free (path);
        }
        g_free (uri);
}

static CcBackgroundItem *
add_item_for_filename (CcBackgroundPage *page,
                       const char       *filename)
{
        CcBackgroundItem *item;
        gboolean          added;

        if (filename == NULL)
                return NULL;

        added = FALSE;

        item = cc_background_item_new (filename);
        if (cc_background_item_load (item)) {
                added = cc_backgrounds_monitor_add_item (page->priv->monitor, item);
        }
        if (!added) {
                g_object_unref (item);
                item = NULL;
        }

        return item;
}

/* FIXME: move to base class? */
static GtkWidget *
get_toplevel_window (CcBackgroundPage *page)
{
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                return NULL;
        }

        return toplevel;
}

static void
enable_busy_cursor (CcBackgroundPage *page,
                    gboolean          enable)
{
        GdkCursor    *cursor;
        GtkWidget    *toplevel;
        GdkWindow    *window;

        toplevel = get_toplevel_window (page);
        if (toplevel == NULL)
                return;

        window = gtk_widget_get_window (toplevel);

        if (enable) {
                cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                                     GDK_WATCH);
        } else {
                cursor = NULL;
        }

        gdk_window_set_cursor (window, cursor);

        if (cursor != NULL)
                gdk_cursor_unref (cursor);
}

static void
add_items_for_filenames (CcBackgroundPage *page,
                         GSList           *filenames)
{
        CcBackgroundItem *item;

        enable_busy_cursor (page, TRUE);

        item = NULL;
        while (filenames != NULL) {
                gchar *uri = filenames->data;

                item = add_item_for_filename (page, uri);
                g_object_unref (item);
                filenames = g_slist_remove (filenames, uri);
                g_free (uri);
        }

        enable_busy_cursor (page, FALSE);

        if (item != NULL) {
                select_item (page, item, TRUE);
        }
}

static void
ui_update_option_menu (CcBackgroundPage *page,
                       int               value)
{
        gtk_combo_box_set_active (GTK_COMBO_BOX (page->priv->style_menu),
                                  value);
}

static void
ui_update_shade_menu (CcBackgroundPage *page,
                      int               value)
{
        gtk_combo_box_set_active (GTK_COMBO_BOX (page->priv->color_menu),
                                  value);

        if (value == SHADE_SOLID)
                gtk_widget_hide (page->priv->secondary_color_picker);
        else
                gtk_widget_show (page->priv->secondary_color_picker);
}

static void
on_item_added (CcBackgroundsMonitor *monitor,
               CcBackgroundItem     *item,
               CcBackgroundPage     *page)
{
        g_debug ("Item added");
        load_item (page, item);
}

static void
on_item_removed (CcBackgroundsMonitor *monitor,
                 CcBackgroundItem     *item,
                 CcBackgroundPage     *page)
{
        GtkTreeIter iter;
        char       *uri;

        uri = NULL;
        g_object_get (item, "filename", &uri, NULL);
        g_debug ("Item removed: %s", uri);
        if (find_uri_in_model (page->priv->model, uri, &iter)) {
                gtk_list_store_remove (GTK_LIST_STORE (page->priv->model), &iter);
        }
        g_free (uri);
}

static void
update_ui_from_gconf (CcBackgroundPage *page)
{
        GConfClient      *client;
        char             *uri;
        char             *path;
        CcBackgroundItem *item;
        GtkTreeIter       iter;
        gboolean          already_added;

        client = gconf_client_get_default ();

        uri = gconf_client_get_string (client,
                                       WP_FILE_KEY,
                                       NULL);

        if (uri != NULL && *uri == '\0') {
                g_free (uri);
                uri = NULL;
        }
        if (uri == NULL)
                uri = g_strdup ("(none)");

        path = NULL;
        if (g_utf8_validate (uri, -1, NULL)
            && g_file_test (uri, G_FILE_TEST_EXISTS))
                path = g_strdup (uri);
        else
                path = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
        g_free (uri);

        /* now update or add item */
        already_added = FALSE;
        item = NULL;
        if (find_uri_in_model (page->priv->model, path, &iter)) {
                gtk_tree_model_get (page->priv->model, &iter, COL_ITEM, &item, -1);
                already_added = TRUE;
        } else {
                item = cc_background_item_new (path);
        }
        g_free (path);

        if (item != NULL) {
                char             *placement;
                char             *shading;
                char             *primary_color;
                char             *secondary_color;

                placement = gconf_client_get_string (client,
                                                     WP_OPTIONS_KEY,
                                                     NULL);
                shading = gconf_client_get_string (client,
                                                   WP_SHADING_KEY,
                                                   NULL);
                primary_color = gconf_client_get_string (client,
                                                         WP_PCOLOR_KEY,
                                                         NULL);
                secondary_color = gconf_client_get_string (client,
                                                           WP_SCOLOR_KEY,
                                                           NULL);
                if (placement == NULL)
                        placement = g_strdup ("none");

                g_object_unref (client);


                g_object_set (item,
                              "primary-color", primary_color,
                              "secondary-color", secondary_color,
                              "placement", placement,
                              "shading", shading,
                              NULL);

                if (cc_background_item_load (item)) {
                        if (!already_added)
                                cc_backgrounds_monitor_add_item (page->priv->monitor, item);
                }
                select_item (page, item, TRUE);
                g_object_unref (item);
        }
}

static gboolean
load_stuffs (CcBackgroundPage *page)
{
        GList *items;

        compute_thumbnail_sizes (page);

        g_debug ("Beginning to load");
        cc_backgrounds_monitor_load (page->priv->monitor);
        g_debug ("Finished loading");
        items = cc_backgrounds_monitor_get_items (page->priv->monitor);
        g_list_foreach (items, (GFunc)load_item_iter, page);
        g_list_free (items);

        g_signal_connect (page->priv->monitor,
                          "item-added",
                          G_CALLBACK (on_item_added),
                          page);
        g_signal_connect (page->priv->monitor,
                          "item-removed",
                          G_CALLBACK (on_item_removed),
                          page);

        update_ui_from_gconf (page);

        return FALSE;
}

static void
on_icon_view_realize (GtkWidget        *widget,
                      CcBackgroundPage *page);

static void
on_icon_view_realize (GtkWidget        *widget,
                      CcBackgroundPage *page)
{
        g_idle_add ((GSourceFunc)load_stuffs, page);
        /* only run once */
        g_signal_handlers_disconnect_by_func (widget, on_icon_view_realize, page);
}

static void
next_frame (CcBackgroundPage *page,
            GtkCellRenderer  *cr,
            int               direction)
{
        CcBackgroundItem *item;
        GtkTreeIter       iter;
        GdkPixbuf        *pixbuf;
        GdkPixbuf        *pb;
        int               frame;

        pixbuf = NULL;

        frame = page->priv->frame + direction;
        item = get_selected_item (page, &iter);
        if (item == NULL)
                return;

        if (frame >= 0)
                pixbuf = cc_background_item_get_frame_thumbnail (item,
                                                                 page->priv->thumb_factory,
                                                                 page->priv->thumb_width,
                                                                 page->priv->thumb_height,
                                                                 frame);
        if (pixbuf) {
                gtk_list_store_set (GTK_LIST_STORE (page->priv->model),
                                    &iter,
                                    COL_PIXBUF, pixbuf,
                                    -1);
                g_object_unref (pixbuf);
                page->priv->frame = frame;
        }

        pb = buttons[1];
        if (direction < 0) {
                if (frame == 0)
                        pb = buttons[0];
        } else {
                pixbuf = cc_background_item_get_frame_thumbnail (item,
                                                                 page->priv->thumb_factory,
                                                                 page->priv->thumb_width,
                                                                 page->priv->thumb_height,
                                                                 frame + 1);
                if (pixbuf)
                        g_object_unref (pixbuf);
                else
                        pb = buttons[2];
        }
        g_object_set (cr, "pixbuf", pb, NULL);
        g_object_unref (item);
}

static gboolean
on_icon_view_button_press (GtkWidget        *widget,
                           GdkEventButton   *event,
                           CcBackgroundPage *page)
{
        GtkCellRenderer *cell;

        if (event->type != GDK_BUTTON_PRESS)
                return FALSE;

        if (gtk_icon_view_get_item_at_pos (GTK_ICON_VIEW (widget),
                                           event->x,
                                           event->y,
                                           NULL,
                                           &cell)) {
                if (g_object_get_data (G_OBJECT (cell), "buttons")) {
                        int              w;
                        int              h;
                        GtkCellRenderer *cell2 = NULL;

                        gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &w, &h);
                        if (gtk_icon_view_get_item_at_pos (GTK_ICON_VIEW (widget),
                                                           event->x + w,
                                                           event->y,
                                                           NULL, &cell2)
                            && cell == cell2) {
                                next_frame (page, cell, -1);
                        } else {
                                next_frame (page, cell, 1);
                        }
                        return TRUE;
                }
        }

        return FALSE;
}

static void
update_gconf_from_item (CcBackgroundPage *page,
                        CcBackgroundItem *item)
{
        GConfClient    *client;
        GConfChangeSet *cs;
        char           *pcolor;
        char           *scolor;
        char           *filename;
        char           *shade;
        char           *scale;

        if (item == NULL) {
                return;
        }

        cs = gconf_change_set_new ();

        g_object_get (item,
                      "filename", &filename,
                      "shading", &shade,
                      "placement", &scale,
                      "primary-color", &pcolor,
                      "secondary-color", &scolor,
                      NULL);

        if (strcmp (filename, "(none)") == 0) {
                gconf_change_set_set_string (cs, WP_OPTIONS_KEY, "none");
                gconf_change_set_set_string (cs, WP_FILE_KEY, "");
        } else {
                char *uri;

                if (g_utf8_validate (filename, -1, NULL))
                        uri = g_strdup (filename);
                else
                        uri = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);

                if (uri == NULL) {
                        g_warning ("Failed to convert filename to UTF-8: %s", filename);
                } else {
                        gconf_change_set_set_string (cs, WP_FILE_KEY, uri);
                        g_free (uri);
                }

                if (scale == NULL) {
                        scale = g_strdup ("scaled");
                }
                gconf_change_set_set_string (cs,
                                             WP_OPTIONS_KEY,
                                             scale);
        }

        if (shade == NULL) {
                shade = g_strdup ("solid");
        }
        gconf_change_set_set_string (cs,
                                     WP_SHADING_KEY,
                                     shade);
        if (pcolor == NULL) {
                pcolor = g_strdup ("#000000000000");
        }
        if (scolor == NULL) {
                scolor = g_strdup ("#000000000000");
        }
        gconf_change_set_set_string (cs, WP_PCOLOR_KEY, pcolor);
        gconf_change_set_set_string (cs, WP_SCOLOR_KEY, scolor);

        g_free (filename);
        g_free (shade);
        g_free (scale);
        g_free (pcolor);
        g_free (scolor);

        client = gconf_client_get_default ();
        gconf_client_commit_change_set (client, cs, TRUE, NULL);
        g_object_unref (client);

        gconf_change_set_unref (cs);
}

static void
on_icon_view_selection_changed (GtkIconView      *view,
                                CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        GtkCellRenderer  *cr;
        GList            *cells;
        GList            *l;

        /* update the frame buttons */
        page->priv->frame = -1;
        cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (view));
        for (l = cells; l; l = l->next) {
                cr = l->data;
                if (g_object_get_data (G_OBJECT (cr), "buttons"))
                        g_object_set (cr,
                                      "pixbuf", buttons[0],
                                      NULL);
        }
        g_list_free (cells);

        item = get_selected_item (page, NULL);
        g_debug ("Selection changed %p", item);
        if (item != NULL) {
                update_gconf_from_item (page, item);
                g_object_unref (item);
        }
}

static void
buttons_cell_data_func (GtkCellLayout    *layout,
                        GtkCellRenderer  *cell,
                        GtkTreeModel     *model,
                        GtkTreeIter      *iter,
                        CcBackgroundPage *page)
{
        GtkTreePath      *path;
        CcBackgroundItem *item;
        gboolean          visible;

        path = gtk_tree_model_get_path (model, iter);

        visible = FALSE;
        if (gtk_icon_view_path_is_selected (GTK_ICON_VIEW (layout), path)) {
                item = get_selected_item (page, NULL);
                if (item != NULL) {
                        visible = cc_background_item_changes_with_time (item);
                        g_object_unref (item);
                }
        }

        g_object_set (G_OBJECT (cell),
                      "visible", visible,
                      NULL);

        gtk_tree_path_free (path);
}

static void
update_file_chooser_preview (GtkFileChooser   *chooser,
                             CcBackgroundPage *page)
{
        char       *uri;
        GdkPixbuf  *pixbuf;
        const char *mime_type;
        GFile      *file;
        GFileInfo  *file_info;

        gtk_file_chooser_set_preview_widget_active (chooser, TRUE);

        uri = gtk_file_chooser_get_preview_uri (chooser);
        if (uri == NULL) {
                return;

        }

        file = g_file_new_for_uri (uri);
        file_info = g_file_query_info (file,
                                       G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                       G_FILE_QUERY_INFO_NONE,
                                       NULL, NULL);
        g_object_unref (file);

        mime_type = NULL;
        if (file_info != NULL) {
                mime_type = g_file_info_get_content_type (file_info);
                g_object_unref (file_info);
        }

        pixbuf = NULL;
        if (mime_type != NULL) {
                pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (page->priv->thumb_factory,
                                                                             uri,
                                                                             mime_type);
        }

        if (pixbuf != NULL) {
                gtk_image_set_from_pixbuf (GTK_IMAGE (page->priv->file_chooser_preview), pixbuf);
                g_object_unref (pixbuf);
        } else {
                gtk_image_set_from_stock (GTK_IMAGE (page->priv->file_chooser_preview),
                                          "gtk-dialog-question",
                                          GTK_ICON_SIZE_DIALOG);
        }
}

static void
create_filechooser (CcBackgroundPage *page)
{
        const char    *start_dir;
        const char    *pictures;
        GtkFileFilter *filter;
        GtkWidget     *toplevel;
        GtkWindow     *window;

        window = NULL;
        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (GTK_WIDGET_TOPLEVEL (toplevel)) {
                window = GTK_WINDOW (toplevel);
        }

        page->priv->file_chooser = gtk_file_chooser_dialog_new (_("Add Wallpaper"),
                                                                window,
                                                                GTK_FILE_CHOOSER_ACTION_OPEN,
                                                                GTK_STOCK_CANCEL,
                                                                GTK_RESPONSE_CANCEL,
                                                                GTK_STOCK_OPEN,
                                                                GTK_RESPONSE_OK,
                                                                NULL);

        gtk_dialog_set_default_response (GTK_DIALOG (page->priv->file_chooser), GTK_RESPONSE_OK);
        gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (page->priv->file_chooser), TRUE);
        gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (page->priv->file_chooser), FALSE);

        start_dir = g_get_home_dir ();
        if (g_file_test (BACKGROUNDS_DIR, G_FILE_TEST_IS_DIR)) {
                gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (page->priv->file_chooser),
                                                      BACKGROUNDS_DIR,
                                                      NULL);
                start_dir = BACKGROUNDS_DIR;
        }

        pictures = g_get_user_special_dir (G_USER_DIRECTORY_PICTURES);
        if (pictures != NULL
            && g_file_test (pictures, G_FILE_TEST_IS_DIR)) {
                gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (page->priv->file_chooser),
                                                      pictures,
                                                      NULL);
                start_dir = pictures;
        }

        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (page->priv->file_chooser),
                                             start_dir);

        filter = gtk_file_filter_new ();
        gtk_file_filter_add_pixbuf_formats (filter);
        gtk_file_filter_set_name (filter, _("Images"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (page->priv->file_chooser),
                                     filter);

        filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (filter, _("All files"));
        gtk_file_filter_add_pattern (filter, "*");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (page->priv->file_chooser), filter);

        page->priv->file_chooser_preview = gtk_image_new ();
        gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (page->priv->file_chooser),
                                             page->priv->file_chooser_preview);
        gtk_widget_set_size_request (page->priv->file_chooser_preview, 128, -1);

        gtk_widget_show (page->priv->file_chooser_preview);

        g_signal_connect (page->priv->file_chooser,
                          "update-preview",
                          (GCallback) update_file_chooser_preview,
                          page);
}

static void
on_add_button_clicked (GtkWidget        *widget,
                       CcBackgroundPage *page)
{
        GSList *files;

        if (page->priv->file_chooser == NULL)
                create_filechooser (page);

        switch (gtk_dialog_run (GTK_DIALOG (page->priv->file_chooser))) {
        case GTK_RESPONSE_OK:
                files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (page->priv->file_chooser));
                add_items_for_filenames (page, files);
        case GTK_RESPONSE_CANCEL:
        default:
                gtk_widget_hide (page->priv->file_chooser);
                break;
        }
}

static void
on_remove_button_clicked (GtkWidget        *widget,
                          CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        GtkTreeIter       iter;
        GtkTreePath      *path;

        item = get_selected_item (page, &iter);
        if (item == NULL) {
                return;
        }

        if (cc_backgrounds_monitor_remove_item (page->priv->monitor, item)) {
                path = gtk_tree_model_get_path (page->priv->model, &iter);
        } else {
                path = gtk_tree_path_new_first ();
        }

        gtk_icon_view_select_path (GTK_ICON_VIEW (page->priv->icon_view), path);
        gtk_icon_view_set_cursor (GTK_ICON_VIEW (page->priv->icon_view), path, NULL, FALSE);
        gtk_tree_path_free (path);

        g_object_unref (item);
}

static void
wp_drag_received (GtkWidget        *widget,
                  GdkDragContext   *context,
                  int               x,
                  int               y,
                  GtkSelectionData *selection_data,
                  guint             info,
                  guint             time,
                  CcBackgroundPage *page)
{
        if (info == TARGET_URI_LIST
            || info == TARGET_BGIMAGE) {
                GSList *realuris = NULL;
                gchar **uris;

                uris = g_uri_list_extract_uris ((char *) selection_data->data);
                if (uris != NULL) {
                        char **uri;

                        enable_busy_cursor (page, TRUE);

                        for (uri = uris; *uri; ++uri) {
                                GFile *f;

                                f = g_file_new_for_uri (*uri);
                                realuris = g_slist_append (realuris, g_file_get_path (f));
                                g_object_unref (f);
                        }

                        add_items_for_filenames (page, realuris);

                        enable_busy_cursor (page, FALSE);

                        g_strfreev (uris);
                }
        }
}

static void
wp_drag_get_data (GtkWidget        *widget,
                  GdkDragContext   *context,
                  GtkSelectionData *selection_data,
                  guint             type,
                  guint             time,
                  CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        char             *uris[2];
        char             *filename;

        if (type != TARGET_URI_LIST) {
                return;
        }

        item = get_selected_item (page, NULL);
        if (item == NULL) {
                return;
        }
        g_object_get (item,
                      "filename", &filename,
                      NULL);
        uris[0] = g_filename_to_uri (filename, NULL, NULL);
        uris[1] = NULL;

        gtk_selection_data_set_uris (selection_data, uris);

        g_free (uris[0]);
        g_free (filename);
        g_object_unref (item);
}

static gboolean
on_query_tooltip (GtkWidget        *widget,
                  int               x,
                  int               y,
                  gboolean          keyboard_mode,
                  GtkTooltip       *tooltip,
                  CcBackgroundPage *page)
{
        GtkTreeIter       iter;
        CcBackgroundItem *item;

        if (gtk_icon_view_get_tooltip_context (GTK_ICON_VIEW (page->priv->icon_view),
                                               &x, &y,
                                               keyboard_mode,
                                               NULL,
                                               NULL,
                                               &iter)) {
                gtk_tree_model_get (page->priv->model, &iter, COL_ITEM, &item, -1);
                if (item != NULL) {
                        char *description;
                        g_object_get (item, "description", &description, NULL);
                        gtk_tooltip_set_markup (tooltip, description);
                        g_free (description);
                        g_object_unref (item);
                }

                return TRUE;
        }

        return FALSE;
}

static gint
sort_model_iter (GtkTreeModel     *model,
                 GtkTreeIter      *a,
                 GtkTreeIter      *b,
                 CcBackgroundPage *page)

{
        CcBackgroundItem *itema;
        CcBackgroundItem *itemb;
        char             *filenamea;
        char             *filenameb;
        char             *descriptiona;
        char             *descriptionb;
        int               retval;

        gtk_tree_model_get (model, a, COL_ITEM, &itema, -1);
        gtk_tree_model_get (model, b, COL_ITEM, &itemb, -1);

        g_object_get (itema,
                      "filename", &filenamea,
                      "description", &descriptiona,
                      NULL);
        g_object_get (itemb,
                      "filename", &filenameb,
                      "description", &descriptionb,
                      NULL);
        if (strcmp (filenamea, "(none)") == 0) {
                retval =  -1;
        } else if (strcmp (filenameb, "(none)") == 0) {
                retval =  1;
        } else if (descriptiona != NULL && descriptionb != NULL) {
                retval = g_utf8_collate (descriptiona, descriptionb);
        } else {
                retval = 0;
        }
        g_free (filenamea);
        g_free (filenameb);
        g_free (descriptiona);
        g_free (descriptionb);
        if (itema != NULL)
                g_object_unref (itema);
        if (itemb != NULL)
                g_object_unref (itemb);

        return retval;
}

static void
screen_monitors_changed (GdkScreen        *screen,
                         CcBackgroundPage *page)
{
        reload_wallpapers (page);
}

static void
setup_page (CcBackgroundPage *page)
{
        GtkBuilder      *builder;
        GtkWidget       *widget;
        GError          *error;
        GtkCellRenderer *cr;

        builder = gtk_builder_new ();

        error = NULL;
        gtk_builder_add_from_file (builder,
                                   GNOMECC_UI_DIR
                                   "/appearance.ui",
                                   &error);
        if (error != NULL) {
                g_error (_("Could not load user interface file: %s"),
                         error->message);
                g_error_free (error);
                return;
        }

        page->priv->model = GTK_TREE_MODEL (gtk_list_store_new (2,
                                                                GDK_TYPE_PIXBUF,
                                                                G_TYPE_OBJECT));

        page->priv->icon_view = WID ("wp_view");
        gtk_icon_view_set_model (GTK_ICON_VIEW (page->priv->icon_view),
                                 GTK_TREE_MODEL (page->priv->model));

        g_signal_connect_after (page->priv->icon_view,
                                "realize",
                                (GCallback) on_icon_view_realize,
                                page);

        gtk_cell_layout_clear (GTK_CELL_LAYOUT (page->priv->icon_view));

        cr = gtk_cell_renderer_pixbuf_new ();
        g_object_set (cr, "xpad", 5, "ypad", 5, NULL);

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (page->priv->icon_view), cr, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (page->priv->icon_view),
                                        cr,
                                        "pixbuf", 0,
                                        NULL);

        cr = gtk_cell_renderer_pixbuf_new ();
        create_button_images (page);
        g_object_set (cr,
                      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                      "pixbuf", buttons[0],
                      NULL);
        g_object_set_data (G_OBJECT (cr), "buttons", GINT_TO_POINTER (TRUE));

        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (page->priv->icon_view), cr, FALSE);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (page->priv->icon_view),
                                            cr,
                                            (GtkCellLayoutDataFunc) buttons_cell_data_func,
                                            page,
                                            NULL);

        gtk_widget_set_has_tooltip (page->priv->icon_view, TRUE);
        g_signal_connect (page->priv->icon_view,
                          "selection-changed",
                          (GCallback) on_icon_view_selection_changed,
                          page);
        g_signal_connect (page->priv->icon_view,
                          "button-press-event",
                          G_CALLBACK (on_icon_view_button_press),
                          page);
        g_signal_connect (page->priv->icon_view,
                          "query-tooltip",
                          (GCallback) on_query_tooltip,
                          page);

        page->priv->frame = -1;

        gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (page->priv->model),
                                         1,
                                         (GtkTreeIterCompareFunc) sort_model_iter,
                                         page,
                                         NULL);

        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (page->priv->model),
                                              1,
                                              GTK_SORT_ASCENDING);

        gtk_drag_dest_set (page->priv->icon_view,
                           GTK_DEST_DEFAULT_ALL,
                           drop_types,
                           G_N_ELEMENTS (drop_types),
                           GDK_ACTION_COPY | GDK_ACTION_MOVE);
        g_signal_connect (page->priv->icon_view,
                          "drag_data_received",
                          (GCallback) wp_drag_received,
                          page);

        gtk_drag_source_set (page->priv->icon_view,
                             GDK_BUTTON1_MASK,
                             drag_types,
                             G_N_ELEMENTS (drag_types),
                             GDK_ACTION_COPY);
        g_signal_connect (page->priv->icon_view,
                          "drag-data-get",
                          (GCallback) wp_drag_get_data,
                          page);

        page->priv->style_menu = WID ("wp_style_menu");
        g_signal_connect (page->priv->style_menu,
                          "changed",
                          (GCallback) on_style_menu_changed,
                          page);

        page->priv->color_menu = WID ("wp_color_menu");
        g_signal_connect (page->priv->color_menu,
                          "changed",
                          (GCallback) on_color_menu_changed,
                          page);

        page->priv->secondary_color_picker = WID ("wp_scpicker");
        g_signal_connect (page->priv->secondary_color_picker,
                          "color-set",
                          (GCallback) on_color_changed,
                          page);

        page->priv->primary_color_picker = WID ("wp_pcpicker");
        g_signal_connect (page->priv->primary_color_picker,
                          "color-set",
                          (GCallback) on_color_changed,
                          page);

        widget = WID ("wp_add_button");
        g_signal_connect (widget,
                          "clicked",
                          (GCallback) on_add_button_clicked,
                          page);

        page->priv->remove_button = WID ("wp_rem_button");
        g_signal_connect (page->priv->remove_button,
                          "clicked",
                          (GCallback) on_remove_button_clicked,
                          page);

        /* FIXME: only register after initial load? */
        page->priv->screen_monitors_handler = g_signal_connect (gtk_widget_get_screen (page->priv->icon_view),
                                                                "monitors-changed",
                                                                G_CALLBACK (screen_monitors_changed),
                                                                page);
        page->priv->screen_size_handler = g_signal_connect (gtk_widget_get_screen (page->priv->icon_view),
                                                            "size-changed",
                                                            G_CALLBACK (screen_monitors_changed),
                                                            page);


        ui_update_sensitivities (page);

        /* create the file selector later to save time on startup */

        widget = WID ("background_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);
}

static GObject *
cc_background_page_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
        CcBackgroundPage      *background_page;

        background_page = CC_BACKGROUND_PAGE (G_OBJECT_CLASS (cc_background_page_parent_class)->constructor (type,
                                                                                                                n_construct_properties,
                                                                                                                construct_properties));

        g_object_set (background_page,
                      "display-name", _("Background"),
                      "id", "background",
                      NULL);

        setup_page (background_page);

        return G_OBJECT (background_page);
}

static void
cc_background_page_class_init (CcBackgroundPageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_background_page_get_property;
        object_class->set_property = cc_background_page_set_property;
        object_class->constructor = cc_background_page_constructor;
        object_class->finalize = cc_background_page_finalize;

        g_type_class_add_private (klass, sizeof (CcBackgroundPagePrivate));
}

static void
change_current_uri (CcBackgroundPage *page,
                    const char       *uri)
{
        CcBackgroundItem *item;
        GtkTreeIter       iter;

        item = NULL;
        g_debug ("Looking for %s", uri);
        if (find_uri_in_model (page->priv->model, uri, &iter)) {
                gtk_tree_model_get (page->priv->model, &iter, COL_ITEM, &item, -1);
        } else {
                item = add_item_for_filename (page, uri);
                if (item != NULL)
                        g_object_ref (item);
        }
        if (item != NULL) {
                select_item (page, item, TRUE);
                g_object_unref (item);
        }
}

static void
on_gconf_file_changed (GConfClient      *client,
                       guint             id,
                       GConfEntry       *entry,
                       CcBackgroundPage *page)
{
        const char *uri;
        char       *path;

        uri = gconf_value_get_string (entry->value);

        path = NULL;
        if (uri == NULL || *uri == '\0') {
                path = g_strdup ("(none)");
        } else {
                if (g_utf8_validate (uri, -1, NULL)
                    && g_file_test (uri, G_FILE_TEST_EXISTS))
                        path = g_strdup (uri);
                else
                        path = g_filename_from_utf8 (uri, -1, NULL, NULL, NULL);
        }

        change_current_uri (page, path);

        g_free (path);
}

static void
on_gconf_options_changed (GConfClient      *client,
                          guint             id,
                          GConfEntry       *entry,
                          CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        const char       *placement;

        placement = gconf_value_get_string (entry->value);

        /* "none" means we don't use a background image */
        if (placement == NULL
            || strcmp (placement, "none") == 0) {
                change_current_uri (page, "(none)");
                return;
        }

        ui_update_option_menu (page, string_to_option (placement));

        item = get_selected_item (page, NULL);
        if (item != NULL) {
                g_object_set (item, "placement", placement, NULL);
                cc_background_item_load (item);
                g_object_unref (item);
        }
}

static void
on_gconf_shading_changed (GConfClient      *client,
                          guint             id,
                          GConfEntry       *entry,
                          CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        const char       *shading;

        shading = gconf_value_get_string (entry->value);
        ui_update_shade_menu (page, string_to_shade (shading));

        item = get_selected_item (page, NULL);
        if (item != NULL) {
                g_object_set (item, "shading", shading, NULL);
                cc_background_item_load (item);
                g_object_unref (item);
        }
}

static void
on_gconf_color1_changed (GConfClient      *client,
                         guint             id,
                         GConfEntry       *entry,
                         CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        GdkColor          color = { 0, 0, 0, 0 };
        const char       *colorhex;

        colorhex = gconf_value_get_string (entry->value);

        if (colorhex != NULL) {
                gdk_color_parse (colorhex, &color);
        }
        gtk_color_button_set_color (GTK_COLOR_BUTTON (page->priv->primary_color_picker),
                                    &color);

        item = get_selected_item (page, NULL);
        if (item != NULL) {
                g_object_set (item, "primary-color", colorhex, NULL);
                cc_background_item_load (item);
                g_object_unref (item);
        }
}

static void
on_gconf_color2_changed (GConfClient      *client,
                         guint             id,
                         GConfEntry       *entry,
                         CcBackgroundPage *page)
{
        CcBackgroundItem *item;
        GdkColor          color = { 0, 0, 0, 0 };
        const char       *colorhex;

        ui_update_sensitivities (page);

        colorhex = gconf_value_get_string (entry->value);

        if (colorhex != NULL) {
                gdk_color_parse (colorhex, &color);
        }
        gtk_color_button_set_color (GTK_COLOR_BUTTON (page->priv->secondary_color_picker),
                                    &color);

        item = get_selected_item (page, NULL);
        if (item != NULL) {
                g_object_set (item, "secondary-color", colorhex, NULL);
                cc_background_item_load (item);
                g_object_unref (item);
        }
}

static void
cc_background_page_init (CcBackgroundPage *page)
{
        GConfClient *client;

        page->priv = CC_BACKGROUND_PAGE_GET_PRIVATE (page);

        page->priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);

        page->priv->monitor = cc_backgrounds_monitor_new ();

        client = gconf_client_get_default ();
        gconf_client_add_dir (client,
                              WP_PATH_KEY,
                              GCONF_CLIENT_PRELOAD_ONELEVEL,
                              NULL);

        gconf_client_notify_add (client,
                                 WP_FILE_KEY,
                                 (GConfClientNotifyFunc) on_gconf_file_changed,
                                 page, NULL, NULL);
        gconf_client_notify_add (client,
                                 WP_OPTIONS_KEY,
                                 (GConfClientNotifyFunc) on_gconf_options_changed,
                                 page, NULL, NULL);
        gconf_client_notify_add (client,
                                 WP_SHADING_KEY,
                                 (GConfClientNotifyFunc) on_gconf_shading_changed,
                                 page, NULL, NULL);
        gconf_client_notify_add (client,
                                 WP_PCOLOR_KEY,
                                 (GConfClientNotifyFunc) on_gconf_color1_changed,
                                 page, NULL, NULL);
        gconf_client_notify_add (client,
                                 WP_SCOLOR_KEY,
                                 (GConfClientNotifyFunc) on_gconf_color2_changed,
                                 page, NULL, NULL);

        g_object_unref (client);

}

static void
cc_background_page_finalize (GObject *object)
{
        CcBackgroundPage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_BACKGROUND_PAGE (object));

        page = CC_BACKGROUND_PAGE (object);

        g_return_if_fail (page->priv != NULL);

        if (page->priv->screen_monitors_handler > 0) {
                g_signal_handler_disconnect (gtk_widget_get_screen (page->priv->icon_view),
                                             page->priv->screen_monitors_handler);
                page->priv->screen_monitors_handler = 0;
        }
        if (page->priv->screen_size_handler > 0) {
                g_signal_handler_disconnect (gtk_widget_get_screen (page->priv->icon_view),
                                             page->priv->screen_size_handler);
                page->priv->screen_size_handler = 0;
        }

        if (page->priv->file_chooser != NULL) {
                g_object_ref_sink (page->priv->file_chooser);
                g_object_unref (page->priv->file_chooser);
        }

        if (page->priv->monitor != NULL) {
                cc_backgrounds_monitor_save (page->priv->monitor);
                g_object_unref (page->priv->monitor);
        }

        G_OBJECT_CLASS (cc_background_page_parent_class)->finalize (object);
}

CcPage *
cc_background_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_BACKGROUND_PAGE, NULL);

        return CC_PAGE (object);
}
