/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright 2022 Christopher Davis <christopherdavis@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <glib/gi18n.h>

#include "cc-default-apps-row.h"

struct _CcDefaultAppsRow {
    AdwComboRow parent_instance;

    char *content_type;
    char *filters;

    GListStore *model;
};

G_DEFINE_FINAL_TYPE (CcDefaultAppsRow, cc_default_apps_row, ADW_TYPE_COMBO_ROW)

enum {
    PROP_0,
    PROP_CONTENT_TYPE,
    PROP_FILTERS,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static void
cc_default_apps_row_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    CcDefaultAppsRow *self = CC_DEFAULT_APPS_ROW (object);

    switch (prop_id) {
    case PROP_CONTENT_TYPE:
        g_value_set_string (value, self->content_type);
        break;
    case PROP_FILTERS:
        g_value_set_string (value, self->filters);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_default_apps_row_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    CcDefaultAppsRow *self = CC_DEFAULT_APPS_ROW (object);

    switch (prop_id) {
    case PROP_CONTENT_TYPE:
        self->content_type = g_value_dup_string (value);
        break;
    case PROP_FILTERS:
        self->filters = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_default_apps_row_finalize (GObject *object)
{
    CcDefaultAppsRow *self = CC_DEFAULT_APPS_ROW (object);

    g_clear_pointer (&self->content_type, g_free);
    g_clear_pointer (&self->filters, g_free);

    G_OBJECT_CLASS (cc_default_apps_row_parent_class)->finalize (object);
}

static void
update_checkmark (CcDefaultAppsRow *self, GtkListItem *list_item)
{
    GtkWidget *box;
    GtkWidget *checkmark_image;

    box = gtk_list_item_get_child (list_item);
    if (box == NULL)
        return;

    checkmark_image = gtk_widget_get_last_child (box);

    gtk_widget_set_opacity (
        checkmark_image,
        (gtk_list_item_get_position (list_item) == adw_combo_row_get_selected (ADW_COMBO_ROW (self))) ? 1.0 : 0.0);
}

static void
on_combo_row_selected_changed (AdwComboRow *combo_row, GParamSpec *pspec, GtkListItem *list_item)
{
    update_checkmark (CC_DEFAULT_APPS_ROW (combo_row), list_item);
}

static void
cc_default_apps_row_setup_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
    GtkWidget *box;
    GtkWidget *icon_image;
    GtkWidget *label;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

    icon_image = gtk_image_new ();
    gtk_image_set_icon_size (GTK_IMAGE (icon_image), GTK_ICON_SIZE_LARGE);
    gtk_widget_set_valign (icon_image, GTK_ALIGN_CENTER);

    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

    gtk_box_append (GTK_BOX (box), icon_image);
    gtk_box_append (GTK_BOX (box), label);

    gtk_list_item_set_child (list_item, box);
}

static void
cc_default_apps_row_setup_list_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
    GtkWidget *box;
    GtkWidget *checkmark_image;

    cc_default_apps_row_setup_cb (factory, list_item, user_data);

    box = gtk_list_item_get_child (list_item);

    checkmark_image = gtk_image_new_from_icon_name ("object-select-symbolic");
    gtk_widget_set_valign (checkmark_image, GTK_ALIGN_CENTER);

    gtk_widget_set_opacity (checkmark_image, 0.0);
    gtk_box_append (GTK_BOX (box), checkmark_image);
}

static void
cc_default_apps_row_bind_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
    GAppInfo *info;
    GIcon *app_icon;
    GtkWidget *box;
    GtkWidget *icon_image;
    GtkWidget *label;

    info = gtk_list_item_get_item (list_item);
    if (!G_IS_APP_INFO (info))
        return;

    box = gtk_list_item_get_child (list_item);
    icon_image = gtk_widget_get_first_child (box);
    label = gtk_widget_get_next_sibling (icon_image);

    gtk_widget_set_tooltip_text (box, g_app_info_get_id (info));

    gtk_label_set_label (GTK_LABEL (label), g_app_info_get_display_name (info));

    app_icon = g_app_info_get_icon (info);

    if (app_icon != NULL)
        gtk_image_set_from_gicon (GTK_IMAGE (icon_image), app_icon);
    else
        gtk_image_clear (GTK_IMAGE (icon_image));
}

static void
cc_default_apps_row_bind_list_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
    cc_default_apps_row_bind_cb (factory, list_item, user_data);

    update_checkmark (CC_DEFAULT_APPS_ROW (user_data), list_item);

    g_signal_connect (user_data, "notify::selected", G_CALLBACK (on_combo_row_selected_changed), list_item);
}

static void
cc_default_apps_row_unbind_list_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
    g_signal_handlers_disconnect_by_func (user_data, on_combo_row_selected_changed, list_item);
}

static void
cc_default_apps_row_constructed (GObject *object)
{
    CcDefaultAppsRow *self;
    g_autoptr(GAppInfo) default_app = NULL;
    g_autolist(GAppInfo) recommended_apps = NULL;
    GList *l;
    g_autoptr(GtkListItemFactory) factory = NULL;
    g_autoptr(GtkListItemFactory) list_factory = NULL;

    G_OBJECT_CLASS (cc_default_apps_row_parent_class)->constructed (object);

    self = CC_DEFAULT_APPS_ROW (object);
    default_app = g_app_info_get_default_for_type (self->content_type, FALSE);
    recommended_apps = g_app_info_get_recommended_for_type (self->content_type);
    self->model = g_list_store_new (G_TYPE_APP_INFO);

    /* Add the default separately because it may not be in the list of recommended apps */
    if (G_IS_APP_INFO (default_app))
        g_list_store_append (self->model, default_app);

    for (l = recommended_apps; l != NULL; l = l->next) {
        GAppInfo *app = l->data;

        if (!G_IS_APP_INFO (app) || (default_app != NULL && g_app_info_equal (app, default_app)))
            continue;

        g_list_store_append (self->model, app);
    }

    if (g_list_model_get_n_items (G_LIST_MODEL (self->model)) == 0) {
        GtkWidget *no_apps_label;

        no_apps_label = gtk_label_new (_("No Apps Available"));
        gtk_widget_add_css_class (no_apps_label, "dim-label");
        adw_action_row_add_suffix (ADW_ACTION_ROW (self), no_apps_label);
    }

    factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (factory, "setup", G_CALLBACK (cc_default_apps_row_setup_cb), NULL);
    g_signal_connect (factory, "bind", G_CALLBACK (cc_default_apps_row_bind_cb), NULL);

    list_factory = gtk_signal_list_item_factory_new ();
    g_signal_connect (list_factory, "setup", G_CALLBACK (cc_default_apps_row_setup_list_cb), NULL);
    g_signal_connect (list_factory, "bind", G_CALLBACK (cc_default_apps_row_bind_list_cb), self);
    g_signal_connect (list_factory, "unbind", G_CALLBACK (cc_default_apps_row_unbind_list_cb), self);

    adw_combo_row_set_factory (ADW_COMBO_ROW (self), factory);
    adw_combo_row_set_list_factory (ADW_COMBO_ROW (self), list_factory);

    adw_combo_row_set_model (ADW_COMBO_ROW (self), G_LIST_MODEL (self->model));
}

static void
cc_default_apps_row_class_init (CcDefaultAppsRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = cc_default_apps_row_get_property;
    object_class->set_property = cc_default_apps_row_set_property;
    object_class->constructed = cc_default_apps_row_constructed;
    object_class->finalize = cc_default_apps_row_finalize;

    properties[PROP_CONTENT_TYPE] = g_param_spec_string (
        "content-type", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_FILTERS] = g_param_spec_string (
        "filters", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
cc_default_apps_row_init (CcDefaultAppsRow *self)
{
}

void
cc_default_apps_row_update_default_app (CcDefaultAppsRow *self)
{
    GAppInfo *info;
    g_autoptr(GError) error = NULL;
    int i;

    info = G_APP_INFO (adw_combo_row_get_selected_item (ADW_COMBO_ROW (self)));

    if (!info)
        return;

    if (g_app_info_set_as_default_for_type (info, self->content_type, &error) == FALSE) {
        g_warning ("Failed to set '%s' as the default app for '%s': %s", g_app_info_get_name (info), self->content_type,
                   error->message);
    } else {
        g_debug ("Set '%s' as the default handler for '%s'", g_app_info_get_name (info), self->content_type);
    }

    if (self->filters) {
        g_auto(GStrv) entries = NULL;
        const char *const *mime_types;
        g_autoptr(GPtrArray) patterns = NULL;

        entries = g_strsplit (self->filters, ";", -1);
        patterns = g_ptr_array_new_with_free_func ((GDestroyNotify) g_pattern_spec_free);
        for (i = 0; entries[i] != NULL; i++) {
            GPatternSpec *pattern = g_pattern_spec_new (entries[i]);
            g_ptr_array_add (patterns, pattern);
        }

        mime_types = g_app_info_get_supported_types (info);
        for (i = 0; mime_types && mime_types[i]; i++) {
            int j;
            gboolean matched = FALSE;
            g_autoptr(GError) local_error = NULL;

            for (j = 0; j < patterns->len; j++) {
                GPatternSpec *pattern = g_ptr_array_index (patterns, j);
                if (g_pattern_spec_match_string (pattern, mime_types[i]))
                    matched = TRUE;
            }
            if (!matched)
                continue;

            if (g_app_info_set_as_default_for_type (info, mime_types[i], &local_error) == FALSE) {
                g_warning ("Failed to set '%s' as the default app for secondary "
                           "content type '%s': %s",
                           g_app_info_get_name (info), mime_types[i], local_error->message);
            } else {
                g_debug ("Set '%s' as the default handler for '%s'", g_app_info_get_name (info), mime_types[i]);
            }
        }
    }
}
