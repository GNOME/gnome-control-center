/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Thomas Wood
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
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gconf/gconf-client.h>

#include "cc-theme-customize-dialog.h"
#include "gnome-theme-info.h"
#include "theme-util.h"
#include "gtkrc-utils.h"
#include "gconf-property-editor.h"

#include "cc-theme-thumbnailer.h"

#define CC_THEME_CUSTOMIZE_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_THEME_CUSTOMIZE_DIALOG, CcThemeCustomizeDialogPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (builder, s))

static const char *symbolic_names [NUM_SYMBOLIC_COLORS] = {
        "fg_color",
        "bg_color",
        "text_color",
        "base_color",
        "selected_fg_color",
        "selected_bg_color",
        "tooltip_fg_color",
        "tooltip_bg_color"
};

struct CcThemeCustomizeDialogPrivate
{
        GtkWidget *gtk_box;
        GtkWidget *info_bar;
        GtkWidget *message_label;
        GtkWidget *install_button;
        GtkWidget *gtk_tree_view;
        GtkWidget *gtk_delete_button;
        GtkWidget *color_message_box;
        GtkWidget *color_box;
        GtkWidget *color_symbolic_button [NUM_SYMBOLIC_COLORS];
        GtkWidget *color_reset_button;
        GtkWidget *wm_tree_view;
        GtkWidget *wm_delete_button;
        GtkWidget *icon_tree_view;
        GtkWidget *icon_delete_button;
        GtkWidget *icon_message_box;
        GtkWidget *pointer_tree_view;
        GtkWidget *pointer_delete_button;
        GtkWidget *pointer_size_scale;
        GtkWidget *pointer_size_label;
        GtkWidget *pointer_size_small_label;
        GtkWidget *pointer_size_large_label;
        GtkWidget *pointer_message_box;
        GdkPixbuf *gtk_theme_icon;
        GdkPixbuf *window_theme_icon;
        GdkPixbuf *icon_theme_icon;

        CcThemeThumbnailer *thumbnailer;
};

enum {
        PROP_0,
};

static void     cc_theme_customize_dialog_class_init  (CcThemeCustomizeDialogClass *klass);
static void     cc_theme_customize_dialog_init        (CcThemeCustomizeDialog      *theme_customize_dialog);
static void     cc_theme_customize_dialog_finalize    (GObject                   *object);

G_DEFINE_TYPE (CcThemeCustomizeDialog, cc_theme_customize_dialog, GTK_TYPE_DIALOG)

static void update_message_area (CcThemeCustomizeDialog *dialog);

typedef void (* ThumbnailGenFunc) (CcThemeThumbnailer     *thumbnailer,
                                   void                   *theme_info,
                                   CcThemeThumbnailFunc    func,
                                   CcThemeCustomizeDialog *dialog,
                                   GDestroyNotify         *destroy);

typedef struct {
        CcThemeCustomizeDialog *dialog;
        GdkPixbuf              *thumbnail;
} PEditorConvData;

static void
cc_theme_customize_dialog_set_property (GObject        *object,
                                        guint           prop_id,
                                        const GValue   *value,
                                        GParamSpec     *pspec)
{
        CcThemeCustomizeDialog *self;

        self = CC_THEME_CUSTOMIZE_DIALOG (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_theme_customize_dialog_get_property (GObject        *object,
                                        guint           prop_id,
                                        GValue         *value,
                                        GParamSpec     *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
on_info_bar_response (GtkWidget              *w,
                      int                     response_id,
                      CcThemeCustomizeDialog *dialog)
{
        GtkSettings *settings = gtk_settings_get_default ();
        char        *theme;
        char        *engine_path;

        g_object_get (settings, "gtk-theme-name", &theme, NULL);
        engine_path = gtk_theme_info_missing_engine (theme, FALSE);
        g_free (theme);

        if (engine_path != NULL) {
                theme_install_file (GTK_WINDOW (dialog), engine_path);
                g_free (engine_path);
        }

        update_message_area (dialog);
}

static void
update_message_area (CcThemeCustomizeDialog *dialog)
{
        GtkSettings *settings = gtk_settings_get_default ();
        char        *theme = NULL;
        char        *engine;

        g_object_get (settings, "gtk-theme-name", &theme, NULL);
        engine = gtk_theme_info_missing_engine (theme, TRUE);
        g_free (theme);

        if (dialog->priv->info_bar == NULL) {
                GtkWidget *hbox;
                GtkWidget *icon;

                if (engine == NULL)
                        return;

                dialog->priv->info_bar = gtk_info_bar_new ();

                g_signal_connect (dialog->priv->info_bar,
                                  "response",
                                  (GCallback) on_info_bar_response,
                                  dialog);

                dialog->priv->install_button = gtk_info_bar_add_button (GTK_INFO_BAR (dialog->priv->info_bar),
                                                                _("Install"),
                                                                GTK_RESPONSE_APPLY);

                dialog->priv->message_label = gtk_label_new (NULL);
                gtk_label_set_line_wrap (GTK_LABEL (dialog->priv->message_label), TRUE);
                gtk_misc_set_alignment (GTK_MISC (dialog->priv->message_label), 0.0, 0.5);

                hbox = gtk_info_bar_get_content_area (GTK_INFO_BAR (dialog->priv->info_bar));

                icon = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
                gtk_misc_set_alignment (GTK_MISC (icon), 0.5, 0);
                gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);
                gtk_box_pack_start (GTK_BOX (hbox), dialog->priv->message_label, TRUE, TRUE, 0);
                gtk_widget_show_all (dialog->priv->info_bar);
                gtk_widget_set_no_show_all (dialog->priv->info_bar, TRUE);

                gtk_box_pack_start (GTK_BOX (dialog->priv->gtk_box), dialog->priv->info_bar, FALSE, FALSE, 0);
        }

        if (engine != NULL) {
                char *message;

                message = g_strdup_printf (_("This theme will not look as intended because the required GTK+ theme engine '%s' is not installed."),
                                           engine);
                gtk_label_set_text (GTK_LABEL (dialog->priv->message_label), message);
                g_free (message);
                g_free (engine);

                if (packagekit_available ())
                        gtk_widget_show (dialog->priv->install_button);
                else
                        gtk_widget_hide (dialog->priv->install_button);

                gtk_widget_show (dialog->priv->info_bar);
                gtk_widget_queue_draw (dialog->priv->info_bar);
        } else {
                gtk_widget_hide (dialog->priv->info_bar);
        }
}

static void
update_color_buttons_from_string (CcThemeCustomizeDialog *dialog,
                                  const char             *color_scheme)
{
        GdkColor   colors [NUM_SYMBOLIC_COLORS];
        int        i;

        if (!gnome_theme_color_scheme_parse (color_scheme, colors))
                return;

        /* now set all the buttons to the correct settings */
        for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
                gtk_color_button_set_color (GTK_COLOR_BUTTON (dialog->priv->color_symbolic_button[i]),
                                            &colors[i]);
        }
}

static void
update_color_buttons_from_settings (CcThemeCustomizeDialog *dialog,
                                    GtkSettings            *settings)
{
        char        *scheme;
        char        *setting;
        GConfClient *client;

        client = gconf_client_get_default ();
        scheme = gconf_client_get_string (client, COLOR_SCHEME_KEY, NULL);
        g_object_unref (client);
        g_object_get (settings, "gtk-color-scheme", &setting, NULL);

        if (scheme == NULL || strcmp (scheme, "") == 0)
                gtk_widget_set_sensitive (dialog->priv->color_reset_button, FALSE);

        g_free (scheme);
        update_color_buttons_from_string (dialog, setting);
        g_free (setting);
}

static void
check_color_schemes_enabled (CcThemeCustomizeDialog *dialog,
                             GtkSettings            *settings)
{
        char    *theme = NULL;
        char    *filename;
        GSList  *symbolic_colors = NULL;
        gboolean enable_colors = FALSE;
        int      i;

        g_object_get (settings, "gtk-theme-name", &theme, NULL);
        filename = gtkrc_find_named (theme);
        g_free (theme);

        gtkrc_get_details (filename, NULL, &symbolic_colors);
        g_free (filename);

        for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
                gboolean found;

                found = (g_slist_find_custom (symbolic_colors, symbolic_names[i], (GCompareFunc) strcmp) != NULL);
                gtk_widget_set_sensitive (dialog->priv->color_symbolic_button[i], found);

                enable_colors |= found;
        }

        g_slist_foreach (symbolic_colors, (GFunc) g_free, NULL);
        g_slist_free (symbolic_colors);

        gtk_widget_set_sensitive (dialog->priv->color_box, enable_colors);
        gtk_widget_set_sensitive (dialog->priv->color_reset_button, enable_colors);

        if (enable_colors)
                gtk_widget_hide (dialog->priv->color_message_box);
        else
                gtk_widget_show (dialog->priv->color_message_box);
}

static void
gtk_theme_changed (GConfPropertyEditor    *peditor,
                   const char             *key,
                   const GConfValue       *value,
                   CcThemeCustomizeDialog *dialog)
{
        GnomeThemeInfo *theme = NULL;
        const char     *name;
        GtkSettings    *settings = gtk_settings_get_default ();

        if (value && (name = gconf_value_get_string (value))) {
                char *current;

                theme = gnome_theme_info_find (name);

                /* Manually update GtkSettings to new gtk+ theme.
                 * This will eventually happen anyway, but we need the
                 * info for the color scheme updates already. */
                g_object_get (settings, "gtk-theme-name", &current, NULL);

                if (strcmp (current, name) != 0) {
                        g_object_set (settings, "gtk-theme-name", name, NULL);
                        update_message_area (dialog);
                }

                g_free (current);

                check_color_schemes_enabled (dialog, settings);
                update_color_buttons_from_settings (dialog, settings);
        }

        gtk_widget_set_sensitive (dialog->priv->gtk_delete_button,
                                  theme_is_writable (theme));
}

static void
window_theme_changed (GConfPropertyEditor    *peditor,
                      const char             *key,
                      const GConfValue       *value,
                      CcThemeCustomizeDialog *dialog)
{
        GnomeThemeInfo *theme = NULL;
        const char     *name;

        if (value && (name = gconf_value_get_string (value)))
                theme = gnome_theme_info_find (name);

        gtk_widget_set_sensitive (dialog->priv->wm_delete_button,
                                  theme_is_writable (theme));
}

static void
icon_theme_changed (GConfPropertyEditor    *peditor,
                    const char             *key,
                    const GConfValue       *value,
                    CcThemeCustomizeDialog *dialog)
{
        GnomeThemeIconInfo *theme = NULL;
        const char         *name;

        if (value != NULL && (name = gconf_value_get_string (value)))
                theme = gnome_theme_icon_info_find (name);

        gtk_widget_set_sensitive (dialog->priv->icon_delete_button,
                                  theme_is_writable (theme));
}

#ifdef HAVE_XCURSOR
static void
on_cursor_size_changed (int                     size,
                        CcThemeCustomizeDialog *dialog)
{
        GConfClient *client;

        client = gconf_client_get_default ();
        gconf_client_set_int (client, CURSOR_SIZE_KEY, size, NULL);
        g_object_unref (client);
}

static void
on_cursor_size_scale_value_changed (GtkRange               *range,
                                    CcThemeCustomizeDialog *dialog)
{
        GnomeThemeCursorInfo *theme;
        char                 *name;
        GConfClient          *client;

        client = gconf_client_get_default ();
        name = gconf_client_get_string (client, CURSOR_THEME_KEY, NULL);
        g_object_unref (client);
        if (name == NULL)
                return;

        theme = gnome_theme_cursor_info_find (name);
        g_free (name);

        if (theme != NULL) {
                int size;

                size = g_array_index (theme->sizes, int, (int) gtk_range_get_value (range));
                on_cursor_size_changed (size, dialog);
        }
}
#endif

static void
update_cursor_size_scale (GnomeThemeCursorInfo   *theme,
                          CcThemeCustomizeDialog *dialog)
{
#ifdef HAVE_XCURSOR
        gboolean     sensitive;
        int          size;
        int          gconf_size;
        GConfClient *client;

        sensitive = theme && theme->sizes->len > 1;
        gtk_widget_set_sensitive (dialog->priv->pointer_size_scale, sensitive);
        gtk_widget_set_sensitive (dialog->priv->pointer_size_label, sensitive);
        gtk_widget_set_sensitive (dialog->priv->pointer_size_small_label, sensitive);
        gtk_widget_set_sensitive (dialog->priv->pointer_size_large_label, sensitive);

        client = gconf_client_get_default ();
        gconf_size = gconf_client_get_int (client, CURSOR_SIZE_KEY, NULL);
        g_object_unref (client);

        if (sensitive) {
                GtkAdjustment *adjustment;
                int            i;
                int            index;
                GtkRange      *range = GTK_RANGE (dialog->priv->pointer_size_scale);

                adjustment = gtk_range_get_adjustment (range);
                g_object_set (adjustment,
                              "upper", (gdouble) theme->sizes->len - 1,
                              NULL);


                /* fallback if the gconf value is bigger than all available sizes;
                   use the largest we have */
                index = theme->sizes->len - 1;

                /* set the slider to the cursor size which matches the gconf setting best  */
                for (i = 0; i < theme->sizes->len; i++) {
                        size = g_array_index (theme->sizes, gint, i);

                        if (size == gconf_size) {
                                index = i;
                                break;
                        } else if (size > gconf_size) {
                                if (i == 0) {
                                        index = 0;
                                } else {
                                        gint diff, diff_to_last;

                                        diff = size - gconf_size;
                                        diff_to_last = gconf_size - g_array_index (theme->sizes, gint, i - 1);

                                        index = (diff < diff_to_last) ? i : i - 1;
                                }
                                break;
                        }
                }

                gtk_range_set_value (range, (gdouble) index);

                size = g_array_index (theme->sizes, gint, index);
        } else {
                if (theme && theme->sizes->len > 0)
                        size = g_array_index (theme->sizes, gint, 0);
                else
                        size = 18;
        }

        if (size != gconf_size)
                on_cursor_size_changed (size, dialog);
#endif
}

static void
cursor_theme_changed (GConfPropertyEditor    *peditor,
                      const char             *key,
                      const GConfValue       *value,
                      CcThemeCustomizeDialog *dialog)
{
        GnomeThemeCursorInfo *theme = NULL;
        const char           *name;

        if (value && (name = gconf_value_get_string (value)))
                theme = gnome_theme_cursor_info_find (name);

        update_cursor_size_scale (theme, dialog);

        gtk_widget_set_sensitive (dialog->priv->pointer_delete_button,
                                  theme_is_writable (theme));

}

static void
add_to_treeview (GtkTreeView            *treeview,
                 const char             *theme_name,
                 const char             *theme_label,
                 GdkPixbuf              *theme_thumbnail,
                 CcThemeCustomizeDialog *dialog)
{
        GtkListStore *model;

        model = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (treeview))));

        gtk_list_store_insert_with_values (model,
                                           NULL,
                                           0,
                                           COL_LABEL, theme_label,
                                           COL_NAME, theme_name,
                                           COL_THUMBNAIL, theme_thumbnail,
                                           -1);
}

static void
remove_from_treeview (GtkTreeView            *treeview,
                      const char             *theme_name,
                      CcThemeCustomizeDialog *dialog)
{
        GtkListStore *model;
        GtkTreeIter   iter;

        model = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (treeview))));

        if (theme_find_in_model (GTK_TREE_MODEL (model), theme_name, &iter))
                gtk_list_store_remove (model, &iter);
}

static void
update_in_treeview (GtkTreeView            *treeview,
                    const char             *theme_name,
                    const char             *theme_label,
                    CcThemeCustomizeDialog *dialog)
{
        GtkListStore *model;
        GtkTreeIter   iter;

        model = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (treeview))));

        if (theme_find_in_model (GTK_TREE_MODEL (model), theme_name, &iter)) {
                gtk_list_store_set (model, &iter,
                                    COL_LABEL, theme_label,
                                    COL_NAME, theme_name,
                                    -1);
        }
}

static void
update_thumbnail_in_treeview (GtkTreeView            *treeview,
                              const char             *theme_name,
                              GdkPixbuf              *theme_thumbnail,
                              CcThemeCustomizeDialog *dialog)
{
        GtkListStore *model;
        GtkTreeIter   iter;

        if (theme_thumbnail == NULL)
                return;

        model = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (gtk_tree_view_get_model (treeview))));

        if (theme_find_in_model (GTK_TREE_MODEL (model), theme_name, &iter)) {
                gtk_list_store_set (model,
                                    &iter,
                                    COL_THUMBNAIL, theme_thumbnail,
                                    -1);
        }
}

static void
gtk_theme_thumbnail_cb (GdkPixbuf              *pixbuf,
                        char                   *theme_name,
                        CcThemeCustomizeDialog *dialog)
{
        update_thumbnail_in_treeview (GTK_TREE_VIEW (dialog->priv->gtk_tree_view),
                                      theme_name,
                                      pixbuf,
                                      dialog);
}

static void
metacity_theme_thumbnail_cb (GdkPixbuf              *pixbuf,
                             char                   *theme_name,
                             CcThemeCustomizeDialog *dialog)
{
        update_thumbnail_in_treeview (GTK_TREE_VIEW (dialog->priv->wm_tree_view),
                                      theme_name,
                                      pixbuf,
                                      dialog);
}

static void
icon_theme_thumbnail_cb (GdkPixbuf              *pixbuf,
                         char                   *theme_name,
                         CcThemeCustomizeDialog *dialog)
{
        update_thumbnail_in_treeview (GTK_TREE_VIEW (dialog->priv->icon_tree_view),
                                      theme_name,
                                      pixbuf,
                                      dialog);
}

static void
create_thumbnail (CcThemeCustomizeDialog *dialog,
                  const char             *name,
                  GdkPixbuf              *default_thumb)
{
        if (default_thumb == dialog->priv->icon_theme_icon) {
                GnomeThemeIconInfo *info;

                info = gnome_theme_icon_info_find (name);
                if (info != NULL) {
                        cc_theme_thumbnailer_create_icon_async (dialog->priv->thumbnailer,
                                                                info,
                                                                (CcThemeThumbnailFunc) icon_theme_thumbnail_cb,
                                                                dialog,
                                                                NULL);
                }
        } else if (default_thumb == dialog->priv->gtk_theme_icon) {
                GnomeThemeInfo *info;

                info = gnome_theme_info_find (name);
                if (info != NULL && info->has_gtk) {
                        cc_theme_thumbnailer_create_gtk_async (dialog->priv->thumbnailer,
                                                               info,
                                                               (CcThemeThumbnailFunc) gtk_theme_thumbnail_cb,
                                                               dialog,
                                                               NULL);
                }
        } else if (default_thumb == dialog->priv->window_theme_icon) {
                GnomeThemeInfo *info;

                info = gnome_theme_info_find (name);
                if (info != NULL && info->has_metacity) {
                        cc_theme_thumbnailer_create_metacity_async (dialog->priv->thumbnailer,
                                                                    info,
                                                                    (CcThemeThumbnailFunc) metacity_theme_thumbnail_cb,
                                                                    dialog,
                                                                    NULL);
                }
        }
}

static void
on_changed_on_disk (GnomeThemeCommonInfo   *theme,
                    GnomeThemeChangeType    change_type,
                    GnomeThemeElement       element_type,
                    CcThemeCustomizeDialog *dialog)
{
        if (theme->type == GNOME_THEME_TYPE_REGULAR) {
                GnomeThemeInfo *info = (GnomeThemeInfo *) theme;

                if (change_type == GNOME_THEME_CHANGE_DELETED) {
                        if (element_type & GNOME_THEME_GTK_2)
                                remove_from_treeview (GTK_TREE_VIEW (dialog->priv->gtk_tree_view),
                                                      info->name,
                                                      dialog);
                        if (element_type & GNOME_THEME_METACITY)
                                remove_from_treeview (GTK_TREE_VIEW (dialog->priv->wm_tree_view),
                                                      info->name,
                                                      dialog);

                } else {
                        if (element_type & GNOME_THEME_GTK_2) {
                                if (change_type == GNOME_THEME_CHANGE_CREATED)
                                        add_to_treeview (GTK_TREE_VIEW (dialog->priv->gtk_tree_view),
                                                         info->name,
                                                         info->name,
                                                         dialog->priv->gtk_theme_icon,
                                                         dialog);
                                else if (change_type == GNOME_THEME_CHANGE_CHANGED)
                                        update_in_treeview (GTK_TREE_VIEW (dialog->priv->gtk_tree_view),
                                                            info->name,
                                                            info->name,
                                                            dialog);

                                cc_theme_thumbnailer_create_gtk_async (dialog->priv->thumbnailer,
                                                                       info,
                                                                       (CcThemeThumbnailFunc) gtk_theme_thumbnail_cb,
                                                                       dialog,
                                                                       NULL);
                        }

                        if (element_type & GNOME_THEME_METACITY) {
                                if (change_type == GNOME_THEME_CHANGE_CREATED)
                                        add_to_treeview (GTK_TREE_VIEW (dialog->priv->wm_tree_view),
                                                         info->name,
                                                         info->name,
                                                         dialog->priv->window_theme_icon,
                                                         dialog);
                                else if (change_type == GNOME_THEME_CHANGE_CHANGED)
                                        update_in_treeview (GTK_TREE_VIEW (dialog->priv->wm_tree_view),
                                                            info->name,
                                                            info->name,
                                                            dialog);

                                cc_theme_thumbnailer_create_metacity_async (dialog->priv->thumbnailer,
                                                                            info,
                                                                            (CcThemeThumbnailFunc) metacity_theme_thumbnail_cb,
                                                                            dialog,
                                                                            NULL);
                        }
                }

        } else if (theme->type == GNOME_THEME_TYPE_ICON) {
                GnomeThemeIconInfo *info = (GnomeThemeIconInfo *) theme;

                if (change_type == GNOME_THEME_CHANGE_DELETED) {
                        remove_from_treeview (GTK_TREE_VIEW (dialog->priv->icon_tree_view),
                                              info->name,
                                              dialog);
                } else {
                        if (change_type == GNOME_THEME_CHANGE_CREATED)
                                add_to_treeview (GTK_TREE_VIEW (dialog->priv->icon_tree_view),
                                                 info->name,
                                                 info->readable_name,
                                                 dialog->priv->icon_theme_icon,
                                                 dialog);
                        else if (change_type == GNOME_THEME_CHANGE_CHANGED)
                                update_in_treeview (GTK_TREE_VIEW (dialog->priv->icon_tree_view),
                                                    info->name,
                                                    info->readable_name,
                                                    dialog);

                        cc_theme_thumbnailer_create_icon_async (dialog->priv->thumbnailer,
                                                                info,
                                                                (CcThemeThumbnailFunc) icon_theme_thumbnail_cb,
                                                                dialog,
                                                                NULL);
                }

        } else if (theme->type == GNOME_THEME_TYPE_CURSOR) {
                GnomeThemeCursorInfo *info = (GnomeThemeCursorInfo *) theme;

                if (change_type == GNOME_THEME_CHANGE_DELETED) {
                        remove_from_treeview (GTK_TREE_VIEW (dialog->priv->pointer_tree_view),
                                              info->name,
                                              dialog);
                } else {
                        if (change_type == GNOME_THEME_CHANGE_CREATED)
                                add_to_treeview (GTK_TREE_VIEW (dialog->priv->pointer_tree_view),
                                                 info->name,
                                                 info->readable_name,
                                                 info->thumbnail,
                                                 dialog);
                        else if (change_type == GNOME_THEME_CHANGE_CHANGED)
                                update_in_treeview (GTK_TREE_VIEW (dialog->priv->pointer_tree_view),
                                                    info->name,
                                                    info->readable_name,
                                                    dialog);
                }
        }
}

static gchar *
find_string_in_model (GtkTreeModel *model,
                      const char   *value,
                      int           column)
{
        GtkTreeIter iter;
        gboolean    valid;
        char       *path = NULL;
        char       *test;

        if (value == NULL)
                return NULL;

        for (valid = gtk_tree_model_get_iter_first (model, &iter);
             valid;
             valid = gtk_tree_model_iter_next (model, &iter)) {
                gtk_tree_model_get (model, &iter, column, &test, -1);

                if (test) {
                        int cmp = strcmp (test, value);
                        g_free (test);

                        if (!cmp) {
                                path = gtk_tree_model_get_string_from_iter (model, &iter);
                                break;
                        }
                }
        }

        return path;
}

static GConfValue *
conv_to_widget_cb (GConfPropertyEditor *peditor,
                   const GConfValue    *value)
{
        GtkTreeModel *store;
        GtkTreeView  *list;
        const char   *curr_value;
        GConfValue   *new_value;
        char         *path;

        /* find value in model */
        curr_value = gconf_value_get_string (value);
        list = GTK_TREE_VIEW (gconf_property_editor_get_ui_control (peditor));
        store = gtk_tree_view_get_model (list);

        path = find_string_in_model (store, curr_value, COL_NAME);

        /* Add a temporary item if we can't find a match
         * TODO: delete this item if it is no longer selected?
         */
        if (path == NULL) {
                GtkListStore    *list_store;
                GtkTreeIter      iter;
                GtkTreeIter      sort_iter;
                PEditorConvData *conv;

                list_store = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (store)));

                g_object_get (peditor, "data", &conv, NULL);
                gtk_list_store_insert_with_values (list_store,
                                                   &iter,
                                                   0,
                                                   COL_LABEL, curr_value,
                                                   COL_NAME, curr_value,
                                                   COL_THUMBNAIL, conv->thumbnail,
                                                   -1);
                /* convert the tree store iter for use with the sort model */
                gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (store),
                                                                &sort_iter,
                                                                &iter);
                path = gtk_tree_model_get_string_from_iter (store, &sort_iter);

                create_thumbnail (conv->dialog, curr_value, conv->thumbnail);
        }

        new_value = gconf_value_new (GCONF_VALUE_STRING);
        gconf_value_set_string (new_value, path);
        g_free (path);

        return new_value;
}

static GConfValue *
conv_from_widget_cb (GConfPropertyEditor *peditor,
                     const GConfValue    *value)
{
        GConfValue       *new_value = NULL;
        GtkTreeIter       iter;
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeView      *list;

        list = GTK_TREE_VIEW (gconf_property_editor_get_ui_control (peditor));
        selection = gtk_tree_view_get_selection (list);

        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                char *list_value;

                gtk_tree_model_get (model, &iter, COL_NAME, &list_value, -1);

                if (list_value) {
                        new_value = gconf_value_new (GCONF_VALUE_STRING);
                        gconf_value_set_string (new_value, list_value);
                        g_free (list_value);
                }
        }

        return new_value;
}

static int
cursor_theme_sort_func (GtkTreeModel *model,
                        GtkTreeIter  *a,
                        GtkTreeIter  *b,
                        gpointer      user_data)
{
        char       *a_label = NULL;
        char       *b_label = NULL;
        const char *default_label;
        int         result;

        gtk_tree_model_get (model, a, COL_LABEL, &a_label, -1);
        gtk_tree_model_get (model, b, COL_LABEL, &b_label, -1);

        default_label = _("Default Pointer");

        if (strcmp (a_label, default_label) == 0)
                result = -1;
        else if (strcmp (b_label, default_label) == 0)
                result = 1;
        else
                result = strcmp (a_label, b_label);

        g_free (a_label);
        g_free (b_label);

        return result;
}

static void
prepare_list (CcThemeCustomizeDialog *dialog,
              GtkWidget              *list,
              ThemeType               type,
              GCallback               callback)
{
        GtkListStore      *store;
        GList             *l;
        GList             *themes = NULL;
        GtkCellRenderer   *renderer;
        GtkTreeViewColumn *column;
        GtkTreeModel      *sort_model;
        GdkPixbuf         *thumbnail;
        const char        *key;
        GObject           *peditor;
        GConfClient       *client;
        GConfValue        *value;
        ThumbnailGenFunc   generator;
        CcThemeThumbnailFunc thumb_cb;
        PEditorConvData   *conv_data;

        switch (type) {
        case THEME_TYPE_GTK:
                themes = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
                thumbnail = dialog->priv->gtk_theme_icon;
                key = GTK_THEME_KEY;
                generator = (ThumbnailGenFunc) cc_theme_thumbnailer_create_gtk_async;
                thumb_cb = (CcThemeThumbnailFunc) gtk_theme_thumbnail_cb;
                break;

        case THEME_TYPE_WINDOW:
                themes = gnome_theme_info_find_by_type (GNOME_THEME_METACITY);
                thumbnail = dialog->priv->window_theme_icon;
                key = METACITY_THEME_KEY;
                generator = (ThumbnailGenFunc) cc_theme_thumbnailer_create_metacity_async;
                thumb_cb = (CcThemeThumbnailFunc) metacity_theme_thumbnail_cb;
                break;

        case THEME_TYPE_ICON:
                themes = gnome_theme_icon_info_find_all ();
                thumbnail = dialog->priv->icon_theme_icon;
                key = ICON_THEME_KEY;
                generator = (ThumbnailGenFunc) cc_theme_thumbnailer_create_icon_async;
                thumb_cb = (CcThemeThumbnailFunc) icon_theme_thumbnail_cb;
                break;

        case THEME_TYPE_CURSOR:
                themes = gnome_theme_cursor_info_find_all ();
                thumbnail = NULL;
                key = CURSOR_THEME_KEY;
                generator = NULL;
                thumb_cb = NULL;
                break;

        default:
                /* we don't deal with any other type of themes here */
                return;
        }

        store = gtk_list_store_new (NUM_COLS,
                                    GDK_TYPE_PIXBUF,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);

        for (l = themes; l; l = g_list_next (l)) {
                GnomeThemeCommonInfo *theme = (GnomeThemeCommonInfo *) l->data;
                GtkTreeIter           i;

                if (type == THEME_TYPE_CURSOR) {
                        thumbnail = ((GnomeThemeCursorInfo *) theme)->thumbnail;
                } else {
                        generator (dialog->priv->thumbnailer, theme, thumb_cb, dialog, NULL);
                }

                gtk_list_store_insert_with_values (store, &i, 0,
                                                   COL_LABEL, theme->readable_name,
                                                   COL_NAME, theme->name,
                                                   COL_THUMBNAIL, thumbnail,
                                                   -1);

                if (type == THEME_TYPE_CURSOR && thumbnail) {
                        g_object_unref (thumbnail);
                        thumbnail = NULL;
                }
        }
        g_list_free (themes);

        sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                              COL_LABEL,
                                              GTK_SORT_ASCENDING);

        if (type == THEME_TYPE_CURSOR) {
                gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model),
                                                 COL_LABEL,
                                                 (GtkTreeIterCompareFunc) cursor_theme_sort_func,
                                                 NULL,
                                                 NULL);
        }

        gtk_tree_view_set_model (GTK_TREE_VIEW (list), GTK_TREE_MODEL (sort_model));

        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (renderer, "xpad", 3, "ypad", 3, NULL);

        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COL_THUMBNAIL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);

        renderer = gtk_cell_renderer_text_new ();

        column = gtk_tree_view_column_new ();
        gtk_tree_view_column_pack_start (column, renderer, FALSE);
        gtk_tree_view_column_add_attribute (column, renderer, "text", COL_LABEL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);

        conv_data = g_new (PEditorConvData, 1);
        conv_data->dialog = dialog;
        conv_data->thumbnail = thumbnail;
        peditor = gconf_peditor_new_tree_view (NULL,
                                               key,
                                               list,
                                               "conv-to-widget-cb", conv_to_widget_cb,
                                               "conv-from-widget-cb", conv_from_widget_cb,
                                               "data", conv_data,
                                               "data-free-cb", g_free,
                                               NULL);
        g_signal_connect (peditor,
                          "value-changed",
                          callback,
                          dialog);

        /* init the delete buttons */
        client = gconf_client_get_default ();
        value = gconf_client_get (client, key, NULL);

        (*((void (*) (GConfPropertyEditor *, const char *, const GConfValue *, gpointer)) callback))
                (GCONF_PROPERTY_EDITOR (peditor), key, value, dialog);

        if (value)
                gconf_value_free (value);
        g_object_unref (client);
}

static void
on_color_scheme_changed (GObject                *settings,
                         GParamSpec             *pspec,
                         CcThemeCustomizeDialog *dialog)
{
        update_color_buttons_from_settings (dialog, GTK_SETTINGS (settings));
}

static void
on_color_button_clicked (GtkWidget              *colorbutton,
                         CcThemeCustomizeDialog *dialog)
{
        GdkColor     color;
        GString     *scheme;
        char        *colstr;
        char        *old_scheme = NULL;
        int          i;

        scheme = g_string_new (NULL);
        for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
                gtk_color_button_get_color (GTK_COLOR_BUTTON (dialog->priv->color_symbolic_button[i]),
                                            &color);

                colstr = gdk_color_to_string (&color);
                g_string_append_printf (scheme, "%s:%s\n", symbolic_names[i], colstr);
                g_free (colstr);
        }

        /* remove the last newline */
        g_string_truncate (scheme, scheme->len - 1);

        /* verify that the scheme really has changed */
        g_object_get (gtk_settings_get_default (),
                      "gtk-color-scheme", &old_scheme,
                      NULL);

        if (!gnome_theme_color_scheme_equal (old_scheme, scheme->str)) {
                GConfClient *client;

                client = gconf_client_get_default ();
                gconf_client_set_string (client, COLOR_SCHEME_KEY, scheme->str, NULL);
                g_object_unref (client);

                gtk_widget_set_sensitive (dialog->priv->color_reset_button, TRUE);
        }

        g_free (old_scheme);
        g_string_free (scheme, TRUE);
}

static void
on_color_scheme_defaults_button_clicked (GtkWidget              *button,
                                         CcThemeCustomizeDialog *dialog)
{
        GConfClient *client;

        client = gconf_client_get_default ();
        gconf_client_unset (client, COLOR_SCHEME_KEY, NULL);
        g_object_unref (client);
        gtk_widget_set_sensitive (dialog->priv->color_reset_button, FALSE);
}

static void
generic_theme_delete (CcThemeCustomizeDialog *dialog,
                      GtkTreeView            *treeview,
                      ThemeType               type)
{
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;

        selection = gtk_tree_view_get_selection (treeview);
        if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
                char *name;

                gtk_tree_model_get (model, &iter, COL_NAME, &name, -1);

                if (name != NULL && theme_delete (name, type)) {
                        /* remove theme from the model, too */
                        GtkTreeIter child;
                        GtkTreePath *path;

                        path = gtk_tree_model_get_path (model, &iter);
                        gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (model),
                                                                        &child,
                                                                        &iter);
                        gtk_list_store_remove (GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model))),
                                               &child);

                        if (gtk_tree_model_get_iter (model, &iter, path)
                            || theme_model_iter_last (model, &iter)) {
                                gtk_tree_path_free (path);
                                path = gtk_tree_model_get_path (model, &iter);
                                gtk_tree_selection_select_path (selection, path);
                                gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0, 0);
                        }
                        gtk_tree_path_free (path);
                }
                g_free (name);
        }
}

static void
on_gtk_delete_clicked (GtkWidget              *button,
                       CcThemeCustomizeDialog *dialog)
{
        generic_theme_delete (dialog,
                              GTK_TREE_VIEW (dialog->priv->gtk_tree_view),
                              THEME_TYPE_GTK);
}

static void
on_wm_delete_clicked (GtkWidget              *button,
                      CcThemeCustomizeDialog *dialog)
{
        generic_theme_delete (dialog,
                              GTK_TREE_VIEW (dialog->priv->wm_tree_view),
                              THEME_TYPE_WINDOW);
}

static void
on_icon_delete_clicked (GtkWidget *button,
                        CcThemeCustomizeDialog *dialog)
{
        generic_theme_delete (dialog,
                              GTK_TREE_VIEW (dialog->priv->icon_tree_view),
                              THEME_TYPE_ICON);
}

static void
on_pointer_delete_clicked (GtkWidget              *button,
                           CcThemeCustomizeDialog *dialog)
{
        generic_theme_delete (dialog,
                              GTK_TREE_VIEW (dialog->priv->pointer_tree_view),
                              THEME_TYPE_CURSOR);
}

static void
setup_dialog (CcThemeCustomizeDialog *dialog)
{
        GtkBuilder        *builder;
        GtkWidget         *widget;
        GtkWidget         *box;
        GError            *error;
        GtkSettings       *settings;
        int                i;

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

        dialog->priv->gtk_theme_icon = gdk_pixbuf_new_from_file (GNOMECC_PIXMAP_DIR "/gtk-theme-thumbnailing.png", NULL);
        dialog->priv->window_theme_icon = gdk_pixbuf_new_from_file (GNOMECC_PIXMAP_DIR "/window-theme-thumbnailing.png", NULL);
        dialog->priv->icon_theme_icon = gdk_pixbuf_new_from_file (GNOMECC_PIXMAP_DIR "/icon-theme-thumbnailing.png", NULL);

        dialog->priv->pointer_size_label = WID ("cursor_size_label");
        dialog->priv->pointer_size_small_label = WID ("cursor_size_small_label");
        dialog->priv->pointer_size_large_label = WID ("cursor_size_large_label");

        dialog->priv->gtk_box = WID ("gtk_themes_vbox");
        dialog->priv->gtk_tree_view = WID ("gtk_themes_list");
        dialog->priv->wm_tree_view = WID ("window_themes_list");
        dialog->priv->color_box = WID ("color_scheme_table");
        dialog->priv->icon_tree_view = WID ("icon_themes_list");
        dialog->priv->pointer_tree_view = WID ("cursor_themes_list");

        for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
                dialog->priv->color_symbolic_button[i] = WID (symbolic_names[i]);
        }
        dialog->priv->color_reset_button = WID ("color_scheme_defaults_button");
        dialog->priv->color_message_box = WID ("color_scheme_message_hbox");
        dialog->priv->pointer_message_box = WID ("cursor_message_hbox");
        dialog->priv->pointer_size_scale = WID ("cursor_size_scale");
        dialog->priv->gtk_delete_button = WID ("gtk_themes_delete");
        dialog->priv->wm_delete_button = WID ("window_themes_delete");
        dialog->priv->icon_delete_button = WID ("icon_themes_delete");
        dialog->priv->pointer_delete_button = WID ("cursor_themes_delete");

        prepare_list (dialog,
                      dialog->priv->gtk_tree_view,
                      THEME_TYPE_GTK,
                      (GCallback) gtk_theme_changed);
        prepare_list (dialog,
                      dialog->priv->wm_tree_view,
                      THEME_TYPE_WINDOW,
                      (GCallback) window_theme_changed);
        prepare_list (dialog,
                      dialog->priv->icon_tree_view,
                      THEME_TYPE_ICON,
                      (GCallback) icon_theme_changed);
        prepare_list (dialog,
                      dialog->priv->pointer_tree_view,
                      THEME_TYPE_CURSOR,
                      (GCallback) cursor_theme_changed);

        gtk_widget_set_no_show_all (dialog->priv->color_message_box, TRUE);

        settings = gtk_settings_get_default ();
        g_signal_connect (settings,
                          "notify::gtk-color-scheme",
                          (GCallback) on_color_scheme_changed,
                          dialog);

#ifdef HAVE_XCURSOR
        g_signal_connect (dialog->priv->pointer_size_scale,
                          "value-changed",
                          (GCallback) on_cursor_size_scale_value_changed,
                          dialog);
#else
        widget = WID ("cursor_size_hbox");
        gtk_widget_set_no_show_all (widget, TRUE);
        gtk_widget_hide (widget);
        gtk_widget_show (dialog->priv->pointer_message_box);
        gtk_box_set_spacing (GTK_BOX (WID ("cursor_vbox")), 12);
#endif

        /* connect signals */
        /* color buttons */
        for (i = 0; i < NUM_SYMBOLIC_COLORS; ++i) {
                g_signal_connect (dialog->priv->color_symbolic_button[i],
                                  "color-set",
                                  (GCallback) on_color_button_clicked,
                                  dialog);
        }

        /* revert button */
        g_signal_connect (dialog->priv->color_reset_button,
                          "clicked",
                          (GCallback) on_color_scheme_defaults_button_clicked,
                          dialog);

        /* delete buttons */
        g_signal_connect (dialog->priv->gtk_delete_button,
                          "clicked",
                          (GCallback) on_gtk_delete_clicked,
                          dialog);
        g_signal_connect (dialog->priv->wm_delete_button,
                          "clicked",
                          (GCallback) on_wm_delete_clicked,
                          dialog);
        g_signal_connect (dialog->priv->icon_delete_button,
                          "clicked",
                          (GCallback) on_icon_delete_clicked,
                          dialog);
        g_signal_connect (dialog->priv->pointer_delete_button,
                          "clicked",
                          (GCallback) on_pointer_delete_clicked,
                          dialog);

        update_message_area (dialog);
        gnome_theme_info_register_theme_change ((ThemeChangedCallback) on_changed_on_disk, dialog);

        gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

        widget = WID ("theme_details_notebook");
        box = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_widget_reparent (widget, box);
        gtk_widget_show (widget);

        g_object_unref (builder);
}

static GObject *
cc_theme_customize_dialog_constructor (GType                  type,
                                       guint                  n_construct_properties,
                                       GObjectConstructParam *construct_properties)
{
        CcThemeCustomizeDialog *dialog;

        dialog = CC_THEME_CUSTOMIZE_DIALOG (G_OBJECT_CLASS (cc_theme_customize_dialog_parent_class)->constructor (type,
                                                                                                                  n_construct_properties,
                                                                                                                  construct_properties));

        setup_dialog (dialog);

        return G_OBJECT (dialog);
}

static void
cc_theme_customize_dialog_class_init (CcThemeCustomizeDialogClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_theme_customize_dialog_get_property;
        object_class->set_property = cc_theme_customize_dialog_set_property;
        object_class->constructor = cc_theme_customize_dialog_constructor;
        object_class->finalize = cc_theme_customize_dialog_finalize;

        g_type_class_add_private (klass, sizeof (CcThemeCustomizeDialogPrivate));
}

static void
cc_theme_customize_dialog_init (CcThemeCustomizeDialog *dialog)
{
        dialog->priv = CC_THEME_CUSTOMIZE_DIALOG_GET_PRIVATE (dialog);

        dialog->priv->thumbnailer = cc_theme_thumbnailer_new ();
}

static void
cc_theme_customize_dialog_finalize (GObject *object)
{
        CcThemeCustomizeDialog *dialog;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_THEME_CUSTOMIZE_DIALOG (object));

        dialog = CC_THEME_CUSTOMIZE_DIALOG (object);

        g_return_if_fail (dialog->priv != NULL);

        if (dialog->priv->gtk_theme_icon != NULL)
                g_object_unref (dialog->priv->gtk_theme_icon);
        if (dialog->priv->window_theme_icon != NULL)
                g_object_unref (dialog->priv->window_theme_icon);
        if (dialog->priv->icon_theme_icon != NULL)
                g_object_unref (dialog->priv->icon_theme_icon);
        if (dialog->priv->thumbnailer != NULL)
                g_object_unref (dialog->priv->thumbnailer);

        G_OBJECT_CLASS (cc_theme_customize_dialog_parent_class)->finalize (object);
}

GtkWidget *
cc_theme_customize_dialog_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_THEME_CUSTOMIZE_DIALOG,
                               "title", _("Customize Theme"),
                               "has-separator", FALSE,
                               NULL);

        return GTK_WIDGET (object);
}
