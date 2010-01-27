/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Thomas Wood
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

#include "cc-theme-page.h"

#include "gtkrc-utils.h"
#include <libwindow-settings/gnome-wm-manager.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-desktop-thumbnail.h>

#include "theme-util.h"
#include "gnome-theme-info.h"
#include "gnome-theme-apply.h"
#include "theme-installer.h"

#include "cc-theme-thumbnailer.h"
#include "cc-theme-save-dialog.h"
#include "cc-theme-customize-dialog.h"

#define CC_THEME_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_THEME_PAGE, CcThemePagePrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))
#define CUSTOM_THEME_NAME "__custom__"

#define APPEARANCE_KEY_DIR "/apps/control-center/appearance"
#define MORE_THEMES_URL_KEY APPEARANCE_KEY_DIR "/more_themes_url"
#define KEY_METACITY_GENERAL_DIR "/apps/metacity/general"
#define KEY_GNOME_INTERFACE_DIR  "/desktop/gnome/interface"

enum {
        RESPONSE_APPLY_BG,
        RESPONSE_REVERT_FONT,
        RESPONSE_APPLY_FONT,
        RESPONSE_INSTALL_ENGINE
};

enum {
        TARGET_URI_LIST,
        TARGET_NS_URL
};

static const GtkTargetEntry drop_types[] = {
        {"text/uri-list", 0, TARGET_URI_LIST},
        {"_NETSCAPE_URL", 0, TARGET_NS_URL}
};

struct CcThemePagePrivate
{
        GtkWidget          *icon_view;
        GtkListStore       *store;
        GnomeThemeMetaInfo *custom_info;
        GdkPixbuf          *icon;
        GtkWidget          *save_dialog;
        GtkWidget          *list_vbox;
        GtkWidget          *info_bar;
        GtkWidget          *message_label;
        GtkWidget          *apply_background_button;
        GtkWidget          *revert_font_button;
        GtkWidget          *apply_font_button;
        GtkWidget          *install_button;
        GtkWidget          *delete_button;
        GtkWidget          *save_button;
        GtkWidget          *info_icon;
        GtkWidget          *error_icon;
        GtkWidget          *theme_details;
        char               *revert_application_font;
        char               *revert_documents_font;
        char               *revert_desktop_font;
        char               *revert_windowtitle_font;
        char               *revert_monospace_font;

        GnomeDesktopThumbnailFactory *thumb_factory;
        CcThemeThumbnailer           *thumbnailer;
};

enum {
        PROP_0,
};

static void     cc_theme_page_class_init     (CcThemePageClass *klass);
static void     cc_theme_page_init           (CcThemePage      *theme_page);
static void     cc_theme_page_finalize       (GObject             *object);

G_DEFINE_TYPE (CcThemePage, cc_theme_page, CC_TYPE_PAGE)

static void message_area_update (CcThemePage *page);

static void
cc_theme_page_set_property (GObject      *object,
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
cc_theme_page_get_property (GObject    *object,
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

static time_t
get_mtime (const char *name)
{
        GnomeThemeMetaInfo *theme;
        time_t              mtime = -1;

        theme = gnome_theme_meta_info_find (name);
        if (theme != NULL) {
                GFile     *file;
                GFileInfo *file_info;

                file = g_file_new_for_path (theme->path);
                file_info = g_file_query_info (file,
                                               G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                               G_FILE_QUERY_INFO_NONE,
                                               NULL, NULL);
                g_object_unref (file);

                if (file_info != NULL) {
                        mtime = g_file_info_get_attribute_uint64 (file_info,
                                                                  G_FILE_ATTRIBUTE_TIME_MODIFIED);
                        g_object_unref (file_info);
                }
        }

        return mtime;
}

static void
thumbnail_update (CcThemePage *page,
                  GdkPixbuf   *pixbuf,
                  char        *theme_name,
                  gboolean     cache)
{
        GtkTreeIter iter;

        /* find item in model and update thumbnail */
        if (!pixbuf)
                return;

        if (theme_find_in_model (GTK_TREE_MODEL (page->priv->store), theme_name, &iter)) {
                time_t mtime;

                gtk_list_store_set (page->priv->store,
                                    &iter,
                                    COL_THUMBNAIL, pixbuf,
                                    -1);

                /* cache thumbnail */
                if (cache && (mtime = get_mtime (theme_name)) != -1) {
                        gchar *path;

                        /* try to share thumbs with nautilus, use themes:/// */
                        path = g_strconcat ("themes:///", theme_name, NULL);

                        gnome_desktop_thumbnail_factory_save_thumbnail (page->priv->thumb_factory,
                                                                        pixbuf,
                                                                        path,
                                                                        mtime);

                        g_free (path);
                }
        }
}

static GdkPixbuf *
get_thumbnail_from_cache (CcThemePage        *page,
                          GnomeThemeMetaInfo *info)
{
        GdkPixbuf *thumb = NULL;
        char      *path;
        char      *thumb_filename;
        time_t     mtime;

        if (info == page->priv->custom_info)
                return NULL;

        mtime = get_mtime (info->name);
        if (mtime == -1)
                return NULL;

        /* try to share thumbs with nautilus, use themes:/// */
        path = g_strconcat ("themes:///", info->name, NULL);
        thumb_filename = gnome_desktop_thumbnail_factory_lookup (page->priv->thumb_factory,
                                                                 path,
                                                                 mtime);
        g_free (path);

        if (thumb_filename != NULL) {
                thumb = gdk_pixbuf_new_from_file (thumb_filename, NULL);
                g_free (thumb_filename);
        }

        return thumb;
}

static void
on_thumbnail_done (GdkPixbuf   *pixbuf,
                   char        *theme_name,
                   CcThemePage *page)
{
        thumbnail_update (page, pixbuf, theme_name, TRUE);
}

static void
thumbnail_generate (CcThemePage        *page,
                    GnomeThemeMetaInfo *info)
{
        GdkPixbuf *thumb;

        thumb = get_thumbnail_from_cache (page, info);

        if (thumb != NULL) {
                thumbnail_update (page, thumb, info->name, FALSE);
                g_object_unref (thumb);
        } else {
                cc_theme_thumbnailer_create_meta_async (page->priv->thumbnailer,
                                                        info,
                                                        (CcThemeThumbnailFunc) on_thumbnail_done,
                                                        page,
                                                        NULL);
        }
}

static void
thumbnail_generate_iter (GnomeThemeMetaInfo *info,
                         CcThemePage        *page)
{
        thumbnail_generate (page, info);
}

static void
on_theme_changed_on_disk (GnomeThemeCommonInfo *theme,
                          GnomeThemeChangeType  change_type,
                          GnomeThemeElement     element_type,
                          CcThemePage          *page)
{
        GnomeThemeMetaInfo *meta;

        if (theme->type != GNOME_THEME_TYPE_METATHEME) {
                return;
        }
        meta = (GnomeThemeMetaInfo *) theme;

        if (change_type == GNOME_THEME_CHANGE_CREATED) {
                gtk_list_store_insert_with_values (page->priv->store,
                                                   NULL,
                                                   0,
                                                   COL_LABEL, meta->readable_name,
                                                   COL_NAME, meta->name,
                                                   COL_THUMBNAIL, page->priv->icon,
                                                   -1);
                thumbnail_generate (page, meta);

        } else if (change_type == GNOME_THEME_CHANGE_DELETED) {
                GtkTreeIter iter;

                if (theme_find_in_model (GTK_TREE_MODEL (page->priv->store), meta->name, &iter))
                        gtk_list_store_remove (page->priv->store, &iter);

        } else if (change_type == GNOME_THEME_CHANGE_CHANGED) {
                thumbnail_generate (page, meta);
        }
}

/* Find out if the lockdown key has been set. Currently returns false on error... */
static gboolean
is_locked_down (GConfClient *client)
{
        return gconf_client_get_bool (client, LOCKDOWN_KEY, NULL);
}

static GnomeThemeMetaInfo *
theme_load_from_gconf (GConfClient *client)
{
        GnomeThemeMetaInfo *theme;
        char               *scheme;

        theme = gnome_theme_meta_info_new ();

        theme->gtk_theme_name = gconf_client_get_string (client,
                                                         GTK_THEME_KEY,
                                                         NULL);
        if (theme->gtk_theme_name == NULL)
                theme->gtk_theme_name = g_strdup ("Clearlooks");

        scheme = gconf_client_get_string (client,
                                          COLOR_SCHEME_KEY,
                                          NULL);
        if (scheme == NULL || !strcmp (scheme, "")) {
                g_free (scheme);
                scheme = gtkrc_get_color_scheme_for_theme (theme->gtk_theme_name);
        }
        theme->gtk_color_scheme = scheme;

        theme->metacity_theme_name = gconf_client_get_string (client,
                                                              METACITY_THEME_KEY,
                                                              NULL);
        if (theme->metacity_theme_name == NULL)
                theme->metacity_theme_name = g_strdup ("Clearlooks");

        theme->icon_theme_name = gconf_client_get_string (client,
                                                          ICON_THEME_KEY,
                                                          NULL);
        if (theme->icon_theme_name == NULL)
                theme->icon_theme_name = g_strdup ("gnome");

        theme->notification_theme_name = gconf_client_get_string (client,
                                                                  NOTIFICATION_THEME_KEY,
                                                                  NULL);

        theme->cursor_theme_name = gconf_client_get_string (client,
                                                            CURSOR_THEME_KEY,
                                                            NULL);
#ifdef HAVE_XCURSOR
        theme->cursor_size = gconf_client_get_int (client,
                                                   CURSOR_SIZE_KEY,
                                                   NULL);
#endif
        if (theme->cursor_theme_name == NULL)
                theme->cursor_theme_name = g_strdup ("default");

        theme->application_font = gconf_client_get_string (client,
                                                           APPLICATION_FONT_KEY,
                                                           NULL);

        return theme;
}

static gboolean
theme_is_equal (const GnomeThemeMetaInfo *a,
                const GnomeThemeMetaInfo *b)
{
        gboolean a_set;
        gboolean b_set;

        if (!(a->gtk_theme_name != NULL && b->gtk_theme_name != NULL)
            || strcmp (a->gtk_theme_name, b->gtk_theme_name) != 0)
                return FALSE;

        if (!(a->icon_theme_name != NULL && b->icon_theme_name != NULL)
            || strcmp (a->icon_theme_name, b->icon_theme_name) != 0)
                return FALSE;

        if (!(a->metacity_theme_name != NULL && b->metacity_theme_name != NULL)
            || strcmp (a->metacity_theme_name, b->metacity_theme_name) != 0)
                return FALSE;

        if (!(a->cursor_theme_name != NULL && b->cursor_theme_name != NULL)
            || strcmp (a->cursor_theme_name, b->cursor_theme_name) != 0)
                return FALSE;

        if (a->cursor_size != b->cursor_size)
                return FALSE;

        a_set = a->gtk_color_scheme && strcmp (a->gtk_color_scheme, "");
        b_set = b->gtk_color_scheme && strcmp (b->gtk_color_scheme, "");
        if ((a_set != b_set)
            || (a_set
                && !gnome_theme_color_scheme_equal (a->gtk_color_scheme, b->gtk_color_scheme)))
                return FALSE;

        return TRUE;
}

static gint
theme_list_sort_func (GnomeThemeMetaInfo *a,
                      GnomeThemeMetaInfo *b)
{
        return strcmp (a->readable_name, b->readable_name);
}

static gint
theme_store_sort_func (GtkTreeModel *model,
                       GtkTreeIter  *a,
                       GtkTreeIter  *b,
                       CcThemePage  *page)
{
        char *a_name;
        char *a_label;
        int   rc;

        gtk_tree_model_get (model, a, COL_NAME, &a_name, COL_LABEL, &a_label, -1);

        if (strcmp (a_name, CUSTOM_THEME_NAME) == 0) {
                rc = -1;
        } else {
                char *b_name;
                char *b_label;

                gtk_tree_model_get (model, b, COL_NAME, &b_name, COL_LABEL, &b_label, -1);

                if (strcmp (b_name, CUSTOM_THEME_NAME) == 0) {
                        rc = 1;
                } else {
                        char *a_case;
                        char *b_case;

                        a_case = g_utf8_casefold (a_label, -1);
                        b_case = g_utf8_casefold (b_label, -1);
                        rc = g_utf8_collate (a_case, b_case);
                        g_free (a_case);
                        g_free (b_case);
                }

                g_free (b_name);
                g_free (b_label);
        }

        g_free (a_name);
        g_free (a_label);

        return rc;
}

static char *
get_selected_theme_name (CcThemePage *page)
{
        char  *name;
        GList *selected;

        name = NULL;
        selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (page->priv->icon_view));

        if (selected != NULL) {
                GtkTreePath  *path;
                GtkTreeModel *model;
                GtkTreeIter   iter;

                path = selected->data;
                model = gtk_icon_view_get_model (GTK_ICON_VIEW (page->priv->icon_view));
                if (gtk_tree_model_get_iter (model, &iter, path)) {
                        gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);
                }

                g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
                g_list_free (selected);
        }

        return name;
}

static const GnomeThemeMetaInfo *
get_selected_theme (CcThemePage *page)
{
        GnomeThemeMetaInfo *theme = NULL;
        char               *name;

        name = get_selected_theme_name (page);

        if (name != NULL) {
                if (strcmp (name, page->priv->custom_info->name) == 0) {
                        theme = page->priv->custom_info;
                } else {
                        theme = gnome_theme_meta_info_find (name);
                }

                g_free (name);
        }

        return theme;
}


static void
on_info_bar_response (GtkWidget   *w,
                      int          response_id,
                      CcThemePage *page)
{
        const GnomeThemeMetaInfo *theme;
        char                     *tmpfont;
        char                     *engine_path;
        GConfClient              *client;

        theme = get_selected_theme (page);
        if (theme == NULL)
                return;

        client = gconf_client_get_default ();

        switch (response_id) {
        case RESPONSE_APPLY_BG:
                gconf_client_set_string (client,
                                         BACKGROUND_KEY,
                                         theme->background_image,
                                         NULL);
                break;

        case RESPONSE_REVERT_FONT:
                if (page->priv->revert_application_font != NULL) {
                        gconf_client_set_string (client,
                                                 APPLICATION_FONT_KEY,
                                                 page->priv->revert_application_font,
                                                 NULL);
                        g_free (page->priv->revert_application_font);
                        page->priv->revert_application_font = NULL;
                }

                if (page->priv->revert_documents_font != NULL) {
                        gconf_client_set_string (client,
                                                 DOCUMENTS_FONT_KEY,
                                                 page->priv->revert_documents_font,
                                                 NULL);
                        g_free (page->priv->revert_documents_font);
                        page->priv->revert_documents_font = NULL;
                }

                if (page->priv->revert_desktop_font != NULL) {
                        gconf_client_set_string (client,
                                                 DESKTOP_FONT_KEY,
                                                 page->priv->revert_desktop_font,
                                                 NULL);
                        g_free (page->priv->revert_desktop_font);
                        page->priv->revert_desktop_font = NULL;
                }

                if (page->priv->revert_windowtitle_font != NULL) {
                        gconf_client_set_string (client,
                                                 WINDOWTITLE_FONT_KEY,
                                                 page->priv->revert_windowtitle_font,
                                                 NULL);
                        g_free (page->priv->revert_windowtitle_font);
                        page->priv->revert_windowtitle_font = NULL;
                }

                if (page->priv->revert_monospace_font != NULL) {
                        gconf_client_set_string (client,
                                                 MONOSPACE_FONT_KEY,
                                                 page->priv->revert_monospace_font,
                                                 NULL);
                        g_free (page->priv->revert_monospace_font);
                        page->priv->revert_monospace_font = NULL;
                }
                break;

        case RESPONSE_APPLY_FONT:
                if (theme->application_font) {
                        tmpfont = gconf_client_get_string (client,
                                                           APPLICATION_FONT_KEY,
                                                           NULL);
                        if (tmpfont != NULL) {
                                g_free (page->priv->revert_application_font);

                                if (strcmp (theme->application_font, tmpfont) == 0) {
                                        g_free (tmpfont);
                                        page->priv->revert_application_font = NULL;
                                } else
                                        page->priv->revert_application_font = tmpfont;
                        }
                        gconf_client_set_string (client,
                                                 APPLICATION_FONT_KEY,
                                                 theme->application_font,
                                                 NULL);
                }

                if (theme->documents_font) {
                        tmpfont = gconf_client_get_string (client,
                                                           DOCUMENTS_FONT_KEY,
                                                           NULL);
                        if (tmpfont != NULL) {
                                g_free (page->priv->revert_documents_font);

                                if (strcmp (theme->documents_font, tmpfont) == 0) {
                                        g_free (tmpfont);
                                        page->priv->revert_documents_font = NULL;
                                } else
                                        page->priv->revert_documents_font = tmpfont;
                        }
                        gconf_client_set_string (client,
                                                 DOCUMENTS_FONT_KEY,
                                                 theme->documents_font,
                                                 NULL);
                }

                if (theme->desktop_font) {
                        tmpfont = gconf_client_get_string (client,
                                                           DESKTOP_FONT_KEY,
                                                           NULL);
                        if (tmpfont != NULL) {
                                g_free (page->priv->revert_desktop_font);

                                if (strcmp (theme->desktop_font, tmpfont) == 0) {
                                        g_free (tmpfont);
                                        page->priv->revert_desktop_font = NULL;
                                } else
                                        page->priv->revert_desktop_font = tmpfont;
                        }
                        gconf_client_set_string (client,
                                                 DESKTOP_FONT_KEY,
                                                 theme->desktop_font,
                                                 NULL);
                }

                if (theme->windowtitle_font) {
                        tmpfont = gconf_client_get_string (client,
                                                           WINDOWTITLE_FONT_KEY,
                                                           NULL);
                        if (tmpfont != NULL) {
                                g_free (page->priv->revert_windowtitle_font);

                                if (strcmp (theme->windowtitle_font, tmpfont) == 0) {
                                        g_free (tmpfont);
                                        page->priv->revert_windowtitle_font = NULL;
                                } else
                                        page->priv->revert_windowtitle_font = tmpfont;
                        }
                        gconf_client_set_string (client,
                                                 WINDOWTITLE_FONT_KEY,
                                                 theme->windowtitle_font,
                                                 NULL);
                }

                if (theme->monospace_font) {
                        tmpfont = gconf_client_get_string (client,
                                                           MONOSPACE_FONT_KEY,
                                                           NULL);
                        if (tmpfont != NULL) {
                                g_free (page->priv->revert_monospace_font);

                                if (strcmp (theme->monospace_font, tmpfont) == 0) {
                                        g_free (tmpfont);
                                        page->priv->revert_monospace_font = NULL;
                                } else
                                        page->priv->revert_monospace_font = tmpfont;
                        }
                        gconf_client_set_string (client,
                                                 MONOSPACE_FONT_KEY,
                                                 theme->monospace_font, NULL);
                }
                break;

        case RESPONSE_INSTALL_ENGINE:
                engine_path = gtk_theme_info_missing_engine (theme->gtk_theme_name, FALSE);
                if (engine_path != NULL) {
                        theme_install_file (GTK_WINDOW (gtk_widget_get_toplevel (page->priv->install_button)),
                                            engine_path);
                        g_free (engine_path);
                }

                message_area_update (page);

                break;
        }

        g_object_unref (client);
}

static void
message_area_update (CcThemePage *page)
{
        const GnomeThemeMetaInfo *theme;
        gboolean                  show_apply_background = FALSE;
        gboolean                  show_apply_font = FALSE;
        gboolean                  show_revert_font = FALSE;
        gboolean                  show_error;
        const char               *message;
        char                     *font;
        GError                   *error = NULL;
        GConfClient              *client;

        theme = get_selected_theme (page);

        if (theme == NULL) {
                if (page->priv->info_bar != NULL)
                        gtk_widget_hide (page->priv->info_bar);
                return;
        }

        show_error = !gnome_theme_meta_info_validate (theme, &error);

        client = gconf_client_get_default ();
        if (!show_error) {
                if (theme->background_image != NULL) {
                        char *background;

                        background = gconf_client_get_string (client, BACKGROUND_KEY, NULL);
                        show_apply_background =
                                (!background || strcmp (theme->background_image, background) != 0);
                        g_free (background);
                }

                if (theme->application_font) {
                        font = gconf_client_get_string (client, APPLICATION_FONT_KEY, NULL);
                        show_apply_font = (!font || strcmp (theme->application_font, font) != 0);
                        g_free (font);
                }

                if (!show_apply_font && theme->documents_font) {
                        font = gconf_client_get_string (client, DOCUMENTS_FONT_KEY, NULL);
                        show_apply_font = (!font || strcmp (theme->application_font, font) != 0);
                        g_free (font);
                }

                if (!show_apply_font && theme->desktop_font) {
                        font = gconf_client_get_string (client, DESKTOP_FONT_KEY, NULL);
                        show_apply_font = (!font || strcmp (theme->application_font, font) != 0);
                        g_free (font);
                }

                if (!show_apply_font && theme->windowtitle_font) {
                        font = gconf_client_get_string (client, WINDOWTITLE_FONT_KEY, NULL);
                        show_apply_font = (!font || strcmp (theme->application_font, font) != 0);
                        g_free (font);
                }

                if (!show_apply_font && theme->monospace_font) {
                        font = gconf_client_get_string (client, MONOSPACE_FONT_KEY, NULL);
                        show_apply_font = (!font || strcmp (theme->application_font, font) != 0);
                        g_free (font);
                }

                show_revert_font = (page->priv->revert_application_font != NULL
                                    || page->priv->revert_documents_font != NULL
                                    || page->priv->revert_desktop_font != NULL
                                    || page->priv->revert_windowtitle_font != NULL
                                    || page->priv->revert_monospace_font != NULL);
        }
        g_object_unref (client);

        if (page->priv->info_bar == NULL) {
                GtkWidget *hbox;

                if (!show_apply_background
                    && !show_revert_font
                    && !show_apply_font
                    && !show_error)
                        return;

                page->priv->info_bar = gtk_info_bar_new ();
                gtk_widget_set_no_show_all (page->priv->info_bar, TRUE);

                g_signal_connect (page->priv->info_bar,
                                  "response",
                                  (GCallback) on_info_bar_response,
                                  page);

                page->priv->apply_background_button = gtk_info_bar_add_button (GTK_INFO_BAR (page->priv->info_bar),
                                                                               _("Apply Background"),
                                                                               RESPONSE_APPLY_BG);
                page->priv->apply_font_button = gtk_info_bar_add_button (GTK_INFO_BAR (page->priv->info_bar),
                                                                         _("Apply Font"),
                                                                         RESPONSE_APPLY_FONT);
                page->priv->revert_font_button = gtk_info_bar_add_button (GTK_INFO_BAR (page->priv->info_bar),
                                                                          _("Revert Font"),
                                                                          RESPONSE_REVERT_FONT);
                page->priv->install_button = gtk_info_bar_add_button (GTK_INFO_BAR (page->priv->info_bar),
                                                                      _("Install"),
                                                                      RESPONSE_INSTALL_ENGINE);

                page->priv->message_label = gtk_label_new (NULL);
                gtk_widget_show (page->priv->message_label);
                gtk_label_set_line_wrap (GTK_LABEL (page->priv->message_label), TRUE);
                gtk_misc_set_alignment (GTK_MISC (page->priv->message_label), 0.0, 0.5);

                hbox = gtk_info_bar_get_content_area (GTK_INFO_BAR (page->priv->info_bar));
                page->priv->info_icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_INFO,
                                                                  GTK_ICON_SIZE_DIALOG);
                page->priv->error_icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
                                                                   GTK_ICON_SIZE_DIALOG);
                gtk_misc_set_alignment (GTK_MISC (page->priv->info_icon), 0.5, 0);
                gtk_misc_set_alignment (GTK_MISC (page->priv->error_icon), 0.5, 0);
                gtk_box_pack_start (GTK_BOX (hbox), page->priv->info_icon, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (hbox), page->priv->error_icon, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (hbox), page->priv->message_label, TRUE, TRUE, 0);

                gtk_box_pack_start (GTK_BOX (page->priv->list_vbox), page->priv->info_bar, FALSE, FALSE, 0);
        }

        if (show_error)
                message = error->message;
        else if (show_apply_background
                 && show_apply_font
                 && show_revert_font)
                message = _("The current theme suggests a background and a font. Also, the last applied font suggestion can be reverted.");
        else if (show_apply_background
                 && show_revert_font)
                message = _("The current theme suggests a background. Also, the last applied font suggestion can be reverted.");
        else if (show_apply_background
                 && show_apply_font)
                message = _("The current theme suggests a background and a font.");
        else if (show_apply_font
                 && show_revert_font)
                message = _("The current theme suggests a font. Also, the last applied font suggestion can be reverted.");
        else if (show_apply_background)
                message = _("The current theme suggests a background.");
        else if (show_revert_font)
                message = _("The last applied font suggestion can be reverted.");
        else if (show_apply_font)
                message = _("The current theme suggests a font.");
        else
                message = NULL;

        if (show_apply_background)
                gtk_widget_show (page->priv->apply_background_button);
        else
                gtk_widget_hide (page->priv->apply_background_button);

        if (show_apply_font)
                gtk_widget_show (page->priv->apply_font_button);
        else
                gtk_widget_hide (page->priv->apply_font_button);

        if (show_revert_font)
                gtk_widget_show (page->priv->revert_font_button);
        else
                gtk_widget_hide (page->priv->revert_font_button);

        if (show_error
            && g_error_matches (error,
                                GNOME_THEME_ERROR,
                                GNOME_THEME_ERROR_GTK_ENGINE_NOT_AVAILABLE)
            && packagekit_available ())
                gtk_widget_show (page->priv->install_button);
        else
                gtk_widget_hide (page->priv->install_button);

        if (show_error
            || show_apply_background
            || show_apply_font
            || show_revert_font) {
                gtk_widget_show (page->priv->info_bar);
                gtk_widget_queue_draw (page->priv->info_bar);

                if (show_error) {
                        gtk_widget_show (page->priv->error_icon);
                        gtk_widget_hide (page->priv->info_icon);
                } else {
                        gtk_widget_show (page->priv->info_icon);
                        gtk_widget_hide (page->priv->error_icon);
                }
        } else {
                gtk_widget_hide (page->priv->info_bar);
        }

        gtk_label_set_text (GTK_LABEL (page->priv->message_label), message);
        g_clear_error (&error);
}

static void
on_theme_selection_changed (GtkWidget   *icon_view,
                            CcThemePage *page)
{
        GList              *selection;
        GnomeThemeMetaInfo *theme = NULL;
        gboolean            is_custom = FALSE;

        selection = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (icon_view));

        if (selection != NULL) {
                GtkTreeModel *model;
                GtkTreeIter   iter;
                char         *name;

                model = gtk_icon_view_get_model (GTK_ICON_VIEW (icon_view));
                gtk_tree_model_get_iter (model, &iter, selection->data);
                gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

                is_custom = (strcmp (name, CUSTOM_THEME_NAME) == 0);

                if (is_custom) {
                        theme = page->priv->custom_info;
                } else {
                        theme = gnome_theme_meta_info_find (name);
                }

                if (theme) {
                        gnome_meta_theme_set (theme);
                        message_area_update (page);
                }

                g_free (name);
                g_list_foreach (selection, (GFunc) gtk_tree_path_free, NULL);
                g_list_free (selection);
        }

        gtk_widget_set_sensitive (page->priv->delete_button,
                                  theme_is_writable (theme));
        gtk_widget_set_sensitive (page->priv->save_button,
                                  is_custom);
}

static void
select_iter (CcThemePage *page,
             GtkTreeIter *iter)
{
        GtkTreePath *path;
        GtkIconView *icon_view;

        icon_view = GTK_ICON_VIEW (page->priv->icon_view);
        path = gtk_tree_model_get_path (gtk_icon_view_get_model (icon_view), iter);
        gtk_icon_view_select_path (icon_view, path);
        gtk_icon_view_scroll_to_path (icon_view, path, FALSE, 0.5, 0.0);
        gtk_tree_path_free (path);
}

static void
select_name (CcThemePage *page,
             const char  *theme)
{
        GtkTreeIter   iter;
        GtkTreeModel *model;

        model = gtk_icon_view_get_model (GTK_ICON_VIEW (page->priv->icon_view));

        if (theme_find_in_model (model, theme, &iter))
                select_iter (page, &iter);
}

static void
load_model (CcThemePage *page)
{
        GList              *theme_list;
        GList              *l;
        GnomeThemeMetaInfo *meta_theme;

        /* set up theme list */
        theme_list = gnome_theme_meta_info_find_all ();
        gnome_theme_info_register_theme_change ((ThemeChangedCallback) on_theme_changed_on_disk,
                                                page);
        for (l = theme_list; l; l = l->next) {
                GnomeThemeMetaInfo *info = l->data;

                gtk_list_store_insert_with_values (page->priv->store,
                                                   NULL,
                                                   0,
                                                   COL_LABEL, info->readable_name,
                                                   COL_NAME, info->name,
                                                   COL_THUMBNAIL, page->priv->icon,
                                                   -1);

                if (meta_theme == NULL
                    && theme_is_equal (page->priv->custom_info, info))
                        meta_theme = info;
        }

        if (meta_theme == NULL) {
                /* add custom theme */
                meta_theme = page->priv->custom_info;

                gtk_list_store_insert_with_values (page->priv->store,
                                                   NULL,
                                                   0,
                                                   COL_LABEL, meta_theme->readable_name,
                                                   COL_NAME, meta_theme->name,
                                                   COL_THUMBNAIL, page->priv->icon,
                                                   -1);

                thumbnail_generate (page, meta_theme);
        }

        select_name (page, meta_theme->name);

        theme_list = g_list_sort (theme_list, (GCompareFunc) theme_list_sort_func);

        g_list_foreach (theme_list, (GFunc) thumbnail_generate_iter, page);
        g_list_free (theme_list);
}


static void
on_theme_customize_clicked (GtkWidget   *button,
                            CcThemePage *page)
{
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        if (page->priv->theme_details == NULL) {
                page->priv->theme_details = cc_theme_customize_dialog_new ();
        }

        gtk_window_set_transient_for (GTK_WINDOW (page->priv->theme_details),
                                      GTK_WINDOW (toplevel));
        gtk_widget_show_all (page->priv->theme_details);
}

static void
on_theme_save_clicked (GtkWidget   *button,
                       CcThemePage *page)
{
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        if (page->priv->save_dialog == NULL) {
                page->priv->save_dialog = cc_theme_save_dialog_new ();
        }

        cc_theme_save_dialog_set_theme_info (CC_THEME_SAVE_DIALOG (page->priv->save_dialog),
                                             page->priv->custom_info);

        gtk_window_set_transient_for (GTK_WINDOW (page->priv->save_dialog),
                                      GTK_WINDOW (toplevel));
        gtk_widget_show_all (page->priv->save_dialog);
}

static void
on_theme_install_clicked (GtkWidget   *button,
                          CcThemePage *page)
{
        GtkWidget *toplevel;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        gnome_theme_installer_run (GTK_WINDOW (toplevel), NULL);
}

static void
on_theme_delete_clicked (GtkWidget   *button,
                         CcThemePage *page)

{
        GList        *selected;
        GtkTreePath  *path;
        GtkTreeModel *model;
        GtkTreeIter   iter;
        char         *name = NULL;

        selected = gtk_icon_view_get_selected_items (GTK_ICON_VIEW (page->priv->icon_view));

        if (selected == NULL) {
                return;
        }

        path = selected->data;
        model = gtk_icon_view_get_model (GTK_ICON_VIEW (page->priv->icon_view));

        if (gtk_tree_model_get_iter (model, &iter, path))
                gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

        if (name != NULL
            && strcmp (name, page->priv->custom_info->name) != 0
            && theme_delete (name, THEME_TYPE_META)) {
                /* remove theme from the model, too */
                GtkTreeIter child;

                if (gtk_tree_model_iter_next (model, &iter)
                    || theme_model_iter_last (model, &iter)) {
                        select_iter (page, &iter);
                }

                gtk_tree_model_get_iter (model, &iter, path);
                gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
                                                                &child,
                                                                &iter);
                gtk_list_store_remove (page->priv->store, &child);
        }

        g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
        g_list_free (selected);
        g_free (name);
}

static void
on_theme_drag_data_received (GtkWidget        *widget,
                             GdkDragContext   *context,
                             int               x,
                             int               y,
                             GtkSelectionData *selection_data,
                             guint             info,
                             guint             time,
                             CcThemePage      *page)
{
        char     **uris;
        GtkWidget *toplevel;

        if (!(info == TARGET_URI_LIST || info == TARGET_NS_URL))
                return;

        toplevel = gtk_widget_get_toplevel (GTK_WIDGET (page));
        if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
                toplevel = NULL;
        }

        uris = g_uri_list_extract_uris ((char *) selection_data->data);

        if (uris != NULL && uris[0] != NULL) {
                GFile *f;
                f = g_file_new_for_uri (uris[0]);
                gnome_theme_install (f, GTK_WINDOW (toplevel));
                g_object_unref (f);
        }

        g_strfreev (uris);
}

static char *
get_default_string_from_key (GConfClient *client,
                             const char  *key)
{
        GConfValue *value;
        char       *str = NULL;

        value = gconf_client_get_default_from_schema (client, key, NULL);

        if (value) {
                if (value->type == GCONF_VALUE_STRING)
                        str = gconf_value_to_string (value);
                gconf_value_free (value);
        }

        return str;
}

static void
set_custom_from_theme (CcThemePage              *page,
                       const GnomeThemeMetaInfo *info)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        GtkTreePath  *path;
        GConfClient  *client;

        if (info == page->priv->custom_info)
                return;

        client = gconf_client_get_default ();

        /* if info is not NULL, we'll copy those theme settings over */
        if (info != NULL) {
                g_free (page->priv->custom_info->gtk_theme_name);
                g_free (page->priv->custom_info->icon_theme_name);
                g_free (page->priv->custom_info->metacity_theme_name);
                g_free (page->priv->custom_info->gtk_color_scheme);
                g_free (page->priv->custom_info->cursor_theme_name);
                g_free (page->priv->custom_info->application_font);
                page->priv->custom_info->gtk_color_scheme = NULL;
                page->priv->custom_info->application_font = NULL;

                /* these settings are guaranteed to be non-NULL */
                page->priv->custom_info->gtk_theme_name = g_strdup (info->gtk_theme_name);
                page->priv->custom_info->icon_theme_name = g_strdup (info->icon_theme_name);
                page->priv->custom_info->metacity_theme_name = g_strdup (info->metacity_theme_name);
                page->priv->custom_info->cursor_theme_name = g_strdup (info->cursor_theme_name);
                page->priv->custom_info->cursor_size = info->cursor_size;

                /* these can be NULL */
                if (info->gtk_color_scheme)
                        page->priv->custom_info->gtk_color_scheme = g_strdup (info->gtk_color_scheme);
                else
                        page->priv->custom_info->gtk_color_scheme = get_default_string_from_key (client, COLOR_SCHEME_KEY);

                if (info->application_font)
                        page->priv->custom_info->application_font = g_strdup (info->application_font);
                else
                        page->priv->custom_info->application_font = get_default_string_from_key (client, APPLICATION_FONT_KEY);
        }

        /* select the custom theme */
        model = gtk_icon_view_get_model (GTK_ICON_VIEW (page->priv->icon_view));
        if (!theme_find_in_model (model, page->priv->custom_info->name, &iter)) {
                GtkTreeIter child;

                gtk_list_store_insert_with_values (page->priv->store,
                                                   &child,
                                                   0,
                                                   COL_LABEL, page->priv->custom_info->readable_name,
                                                   COL_NAME, page->priv->custom_info->name,
                                                   COL_THUMBNAIL, page->priv->icon,
                                                   -1);
                gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (model),
                                                                &iter,
                                                                &child);
        }

        path = gtk_tree_model_get_path (model, &iter);
        gtk_icon_view_select_path (GTK_ICON_VIEW (page->priv->icon_view), path);
        gtk_icon_view_scroll_to_path (GTK_ICON_VIEW (page->priv->icon_view), path, FALSE, 0.5, 0.0);
        gtk_tree_path_free (path);

        /* update the theme thumbnail */
        thumbnail_generate (page, page->priv->custom_info);

        g_object_unref (client);
}

static void
theme_details_changed (CcThemePage *page)
{
        GnomeThemeMetaInfo       *gconf_theme;
        const GnomeThemeMetaInfo *selected;
        gboolean                  done = FALSE;
        GConfClient              *client;

        client = gconf_client_get_default ();

        /* load new state from gconf */
        gconf_theme = theme_load_from_gconf (client);

        /* check if it's our currently selected theme */
        selected = get_selected_theme (page);

        if (!selected || !(done = theme_is_equal (selected, gconf_theme))) {
                /* look for a matching metatheme */
                GList *theme_list;
                GList *l;

                theme_list = gnome_theme_meta_info_find_all ();

                for (l = theme_list; l; l = l->next) {
                        GnomeThemeMetaInfo *info = l->data;

                        if (theme_is_equal (gconf_theme, info)) {
                                select_name (page, info->name);
                                done = TRUE;
                                break;
                        }
                }
                g_list_free (theme_list);
        }

        if (!done) {
                /* didn't find a match, set or update custom */
                set_custom_from_theme (page, gconf_theme);
        }

        gnome_theme_meta_info_free (gconf_theme);
        g_object_unref (client);
}

static void
on_theme_setting_changed (GObject     *settings,
                          GParamSpec  *pspec,
                          CcThemePage *page)
{
        theme_details_changed (page);
}

static void
on_theme_gconf_changed (GConfClient *client,
                        guint        conn_id,
                        GConfEntry  *entry,
                        CcThemePage *page)
{
        theme_details_changed (page);
}

static void
on_background_or_font_changed (GConfEngine *conf,
                               guint        cnxn_id,
                               GConfEntry  *entry,
                               CcThemePage *page)
{
        message_area_update (page);
}


static void
setup_page (CcThemePage *page)
{
        GtkBuilder         *builder;
        GtkWidget          *widget;
        GError             *error;
        GtkWidget          *w;
        GtkTreeModel       *sort_model;
        GtkCellRenderer    *renderer;
        GtkSettings        *settings;
        char               *url;
        GConfClient        *client;

        client = gconf_client_get_default ();

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


        /* initialize some stuff */
        gnome_theme_init ();
        gnome_wm_manager_init ();

        page->priv->custom_info = gnome_theme_meta_info_new ();
        page->priv->icon = gdk_pixbuf_new_from_file (GNOMECC_PIXMAP_DIR "/theme-thumbnailing.png", NULL);
        page->priv->store = gtk_list_store_new (NUM_COLS,
                                                GDK_TYPE_PIXBUF,
                                                G_TYPE_STRING,
                                                G_TYPE_STRING);


        page->priv->icon_view = WID ("theme_list");
        page->priv->list_vbox = WID ("theme_list_vbox");

        page->priv->custom_info = theme_load_from_gconf (client);
        page->priv->custom_info->name = g_strdup (CUSTOM_THEME_NAME);
        page->priv->custom_info->readable_name = g_strdup_printf ("<i>%s</i>", _("Custom"));


        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer,
                      "xpad", 5,
                      "ypad", 5,
                      "xalign", 0.5,
                      "yalign", 1.0,
                      NULL);
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (page->priv->icon_view),
                                  renderer,
                                  FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (page->priv->icon_view),
                                        renderer,
                                        "pixbuf",
                                        COL_THUMBNAIL,
                                        NULL);

        renderer = gtk_cell_renderer_text_new ();
        g_object_set (renderer,
                      "alignment", PANGO_ALIGN_CENTER,
                      "wrap-mode", PANGO_WRAP_WORD_CHAR,
                      "wrap-width", gtk_icon_view_get_item_width (GTK_ICON_VIEW (page->priv->icon_view)),
                      "width", gtk_icon_view_get_item_width (GTK_ICON_VIEW (page->priv->icon_view)),
                      "xalign", 0.0,
                      "yalign", 0.0,
                      NULL);
        gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (page->priv->icon_view), renderer, FALSE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (page->priv->icon_view),
                                        renderer,
                                        "markup", COL_LABEL,
                                        NULL);

        sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (page->priv->store));
        gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model),
                                         COL_LABEL,
                                         (GtkTreeIterCompareFunc) theme_store_sort_func,
                                         page,
                                         NULL);
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                              COL_LABEL,
                                              GTK_SORT_ASCENDING);
        gtk_icon_view_set_model (GTK_ICON_VIEW (page->priv->icon_view),
                                 GTK_TREE_MODEL (sort_model));

        g_signal_connect (page->priv->icon_view,
                          "selection-changed",
                          (GCallback) on_theme_selection_changed,
                          page);

        w = WID ("theme_install");
        gtk_button_set_image (GTK_BUTTON (w),
                              gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON));
        g_signal_connect (w,
                          "clicked",
                          (GCallback) on_theme_install_clicked,
                          page);

        page->priv->save_button = WID ("theme_save");
        gtk_button_set_image (GTK_BUTTON (page->priv->save_button),
                              gtk_image_new_from_stock (GTK_STOCK_SAVE_AS,
                                                        GTK_ICON_SIZE_BUTTON));
        g_signal_connect (page->priv->save_button,
                          "clicked",
                          (GCallback) on_theme_save_clicked,
                          page);

        w = WID ("theme_custom");
        gtk_button_set_image (GTK_BUTTON (w),
                              gtk_image_new_from_stock (GTK_STOCK_EDIT,
                                                        GTK_ICON_SIZE_BUTTON));
        g_signal_connect (w,
                          "clicked",
                          (GCallback) on_theme_customize_clicked,
                          page);

        page->priv->delete_button = WID ("theme_delete");
        g_signal_connect (page->priv->delete_button,
                          "clicked",
                          (GCallback) on_theme_delete_clicked,
                          page);

        w = WID ("theme_vbox");
        gtk_drag_dest_set (w,
                           GTK_DEST_DEFAULT_ALL,
                           drop_types,
                           G_N_ELEMENTS (drop_types),
                           GDK_ACTION_COPY
                           | GDK_ACTION_LINK
                           | GDK_ACTION_MOVE);
        g_signal_connect (w,
                          "drag-data-received",
                          (GCallback) on_theme_drag_data_received,
                          page);
        if (is_locked_down (client))
                gtk_widget_set_sensitive (w, FALSE);

        w = WID ("more_themes_linkbutton");
        url = gconf_client_get_string (client,
                                       MORE_THEMES_URL_KEY,
                                       NULL);
        if (url != NULL && url[0] != '\0') {
                gtk_link_button_set_uri (GTK_LINK_BUTTON (w), url);
                gtk_widget_show (w);
        } else {
                gtk_widget_hide (w);
        }
        g_free (url);

        /* listen to gconf changes, too */
        gconf_client_add_dir (client,
                              KEY_METACITY_GENERAL_DIR,
                              GCONF_CLIENT_PRELOAD_NONE,
                              NULL);
        gconf_client_add_dir (client,
                              KEY_GNOME_INTERFACE_DIR,
                              GCONF_CLIENT_PRELOAD_NONE,
                              NULL);
        gconf_client_notify_add (client,
                                 METACITY_THEME_KEY,
                                 (GConfClientNotifyFunc) on_theme_gconf_changed,
                                 page,
                                 NULL,
                                 NULL);
        gconf_client_notify_add (client,
                                 CURSOR_THEME_KEY,
                                 (GConfClientNotifyFunc) on_theme_gconf_changed,
                                 page,
                                 NULL,
                                 NULL);
#ifdef HAVE_XCURSOR
        gconf_client_notify_add (client,
                                 CURSOR_SIZE_KEY,
                                 (GConfClientNotifyFunc) on_theme_gconf_changed,
                                 page,
                                 NULL,
                                 NULL);
#endif
        gconf_client_notify_add (client,
                                 BACKGROUND_KEY,
                                 (GConfClientNotifyFunc) on_background_or_font_changed,
                                 page,
                                 NULL,
                                 NULL);
        gconf_client_notify_add (client,
                                 APPLICATION_FONT_KEY,
                                 (GConfClientNotifyFunc) on_background_or_font_changed,
                                 page,
                                 NULL,
                                 NULL);
        gconf_client_notify_add (client,
                                 DOCUMENTS_FONT_KEY,
                                 (GConfClientNotifyFunc) on_background_or_font_changed,
                                 page,
                                 NULL,
                                 NULL);
        gconf_client_notify_add (client,
                                 DESKTOP_FONT_KEY,
                                 (GConfClientNotifyFunc) on_background_or_font_changed,
                                 page,
                                 NULL,
                                 NULL);
        gconf_client_notify_add (client,
                                 WINDOWTITLE_FONT_KEY,
                                 (GConfClientNotifyFunc) on_background_or_font_changed,
                                 page,
                                 NULL,
                                 NULL);
        gconf_client_notify_add (client,
                                 MONOSPACE_FONT_KEY,
                                 (GConfClientNotifyFunc) on_background_or_font_changed,
                                 page,
                                 NULL,
                                 NULL);

        settings = gtk_settings_get_default ();
        g_signal_connect (settings,
                          "notify::gtk-color-scheme",
                          (GCallback) on_theme_setting_changed,
                          page);
        g_signal_connect (settings,
                          "notify::gtk-theme-name",
                          (GCallback) on_theme_setting_changed,
                          page);
        g_signal_connect (settings,
                          "notify::gtk-icon-theme-name",
                          (GCallback) on_theme_setting_changed,
                          page);


        widget = WID ("theme_vbox");
        gtk_widget_reparent (widget, GTK_WIDGET (page));
        gtk_widget_show (widget);

        g_object_unref (client);
        g_object_unref (builder);
}

static GObject *
cc_theme_page_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_properties)
{
        CcThemePage      *theme_page;

        theme_page = CC_THEME_PAGE (G_OBJECT_CLASS (cc_theme_page_parent_class)->constructor (type,
                                                                                              n_construct_properties,
                                                                                              construct_properties));

        g_object_set (theme_page,
                      "display-name", _("Theme"),
                      "id", "theme",
                      NULL);

        setup_page (theme_page);

        return G_OBJECT (theme_page);
}

static void
start_working (CcThemePage *page)
{
        static gboolean once = FALSE;

        if (!once) {
                load_model (page);
                once = TRUE;
        }
}

static void
stop_working (CcThemePage *page)
{

}

static void
cc_theme_page_active_changed (CcPage  *base_page,
                              gboolean is_active)
{
        CcThemePage *page = CC_THEME_PAGE (base_page);

        if (is_active)
                start_working (page);
        else
                stop_working (page);

        CC_PAGE_CLASS (cc_theme_page_parent_class)->active_changed (base_page, is_active);

}

static void
cc_theme_page_class_init (CcThemePageClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);
        CcPageClass   *page_class = CC_PAGE_CLASS (klass);

        object_class->get_property = cc_theme_page_get_property;
        object_class->set_property = cc_theme_page_set_property;
        object_class->constructor = cc_theme_page_constructor;
        object_class->finalize = cc_theme_page_finalize;

        page_class->active_changed = cc_theme_page_active_changed;

        g_type_class_add_private (klass, sizeof (CcThemePagePrivate));
}

static void
cc_theme_page_init (CcThemePage *page)
{
        page->priv = CC_THEME_PAGE_GET_PRIVATE (page);

        page->priv->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_NORMAL);
        page->priv->thumbnailer = cc_theme_thumbnailer_new ();
}

static void
cc_theme_page_finalize (GObject *object)
{
        CcThemePage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_THEME_PAGE (object));

        page = CC_THEME_PAGE (object);

        g_return_if_fail (page->priv != NULL);

        gnome_theme_meta_info_free (page->priv->custom_info);

        if (page->priv->icon != NULL)
                g_object_unref (page->priv->icon);
        if (page->priv->save_dialog != NULL)
                gtk_widget_destroy (page->priv->save_dialog);

        g_free (page->priv->revert_application_font);
        g_free (page->priv->revert_documents_font);
        g_free (page->priv->revert_desktop_font);
        g_free (page->priv->revert_windowtitle_font);
        g_free (page->priv->revert_monospace_font);

        if (page->priv->thumb_factory != NULL) {
                g_object_unref (page->priv->thumb_factory);
        }
        if (page->priv->thumbnailer != NULL) {
                g_object_unref (page->priv->thumbnailer);
        }

        G_OBJECT_CLASS (cc_theme_page_parent_class)->finalize (object);
}

CcPage *
cc_theme_page_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_THEME_PAGE, NULL);

        return CC_PAGE (object);
}
