/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2026 Red Hat, Inc
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
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include "cc-removable-media-row.h"

#define PREF_MEDIA_AUTORUN_X_CONTENT_START_APP "autorun-x-content-start-app"
#define PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE "autorun-x-content-ignore"
#define PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "autorun-x-content-open-folder"

#define MEDIA_HANDLING_SCHEMA "org.gnome.desktop.media-handling"

typedef enum {
    MEDIA_ACTION_ASK,
    MEDIA_ACTION_DO_NOTHING,
    MEDIA_ACTION_OPEN_FOLDER,
    MEDIA_ACTION_OTHER_APP,
} MediaAction;

struct _CcRemovableMediaRow {
    AdwComboRow parent_instance;

    GListStore *sections;
    GListStore *apps;
    GtkStringList *actions;

    GSettings *settings;
    GStrv content_types;
    char *heading;

    guint last_selected;
};

G_DEFINE_FINAL_TYPE (CcRemovableMediaRow, cc_removable_media_row, ADW_TYPE_COMBO_ROW)

static void selected_changed_cb (CcRemovableMediaRow *self);

enum {
    PROP_0,
    PROP_CONTENT_TYPES,
    PROP_HEADING,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

static char **
remove_elem_from_str_array (char **v, const char *s)
{
    GPtrArray *array;
    guint idx;

    array = g_ptr_array_new ();

    for (idx = 0; v[idx] != NULL; idx++) {
        if (g_strcmp0 (v[idx], s) == 0) {
            continue;
        }

        g_ptr_array_add (array, v[idx]);
    }

    g_ptr_array_add (array, NULL);

    g_free (v);

    return (char **) g_ptr_array_free (array, FALSE);
}

static char **
add_elem_to_str_array (char **v, const char *s)
{
    GPtrArray *array;
    guint idx;

    array = g_ptr_array_new ();

    for (idx = 0; v[idx] != NULL; idx++) {
        g_ptr_array_add (array, v[idx]);
    }

    g_ptr_array_add (array, g_strdup (s));
    g_ptr_array_add (array, NULL);

    g_free (v);

    return (char **) g_ptr_array_free (array, FALSE);
}

static gboolean
autorun_get_preference (CcRemovableMediaRow *self, const char *key)
{
    g_auto(GStrv) content_types = NULL;
    guint idx;

    content_types = g_settings_get_strv (self->settings, key);

    for (idx = 0; self->content_types[idx] != NULL; idx++) {
        if (!g_strv_contains ((const gchar *const *) content_types, self->content_types[idx])) {
            return FALSE;
        }
    }

    return TRUE;
}

static void
autorun_set_preference (CcRemovableMediaRow *self, const char *key, gboolean enabled)
{
    g_auto(GStrv) content_types = NULL;
    guint idx;

    content_types = g_settings_get_strv (self->settings, key);

    for (idx = 0; self->content_types[idx] != NULL; idx++) {
        content_types = remove_elem_from_str_array (content_types, self->content_types[idx]);
        if (enabled) {
            content_types = add_elem_to_str_array (content_types, self->content_types[idx]);
        }
    }

    g_settings_set_strv (self->settings, key, (const gchar *const *) content_types);
}

static void
autorun_set_preferences (CcRemovableMediaRow *self, gboolean pref_start_app, gboolean pref_ignore,
                         gboolean pref_open_folder)
{
    autorun_set_preference (self, PREF_MEDIA_AUTORUN_X_CONTENT_START_APP, pref_start_app);
    autorun_set_preference (self, PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE, pref_ignore);
    autorun_set_preference (self, PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER, pref_open_folder);
}

static void
set_default_app (CcRemovableMediaRow *self, GAppInfo *info)
{
    guint idx;

    for (idx = 0; self->content_types[idx] != NULL; idx++) {
        g_app_info_set_as_default_for_type (info, self->content_types[idx], NULL);
    }
}

static char *
item_get_label (gpointer item)
{
    if (G_IS_APP_INFO (item))
        return g_strdup (g_app_info_get_display_name (item));

    return g_strdup (gtk_string_object_get_string (item));
}

static void
update_checkmark (CcRemovableMediaRow *self, GtkListItem *list_item)
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
on_selected_notify_cb (CcRemovableMediaRow *self, GParamSpec *pspec, GtkListItem *list_item)
{
    update_checkmark (self, list_item);
}

static void
factory_setup_cb (CcRemovableMediaRow *self, GtkListItem *list_item)
{
    GtkWidget *label;

    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);

    gtk_list_item_set_child (list_item, label);
}

static void
factory_bind_cb (CcRemovableMediaRow *self, GtkListItem *list_item)
{
    gpointer item;
    GtkWidget *label;
    g_autofree char *text = NULL;

    item = gtk_list_item_get_item (list_item);
    if (item == NULL)
        return;

    text = item_get_label (item);
    label = gtk_list_item_get_child (list_item);
    gtk_label_set_label (GTK_LABEL (label), text);
}

static void
list_factory_setup_cb (CcRemovableMediaRow *self, GtkListItem *list_item)
{
    GtkWidget *box;
    GtkWidget *icon_image;
    GtkWidget *label;
    GtkWidget *checkmark_image;

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);

    icon_image = gtk_image_new ();
    gtk_image_set_icon_size (GTK_IMAGE (icon_image), GTK_ICON_SIZE_LARGE);
    gtk_widget_set_valign (icon_image, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class (icon_image, "icon-dropshadow");

    label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);

    checkmark_image = gtk_image_new_from_icon_name ("object-select-symbolic");
    gtk_widget_set_valign (checkmark_image, GTK_ALIGN_CENTER);
    gtk_widget_set_opacity (checkmark_image, 0.0);

    gtk_box_append (GTK_BOX (box), icon_image);
    gtk_box_append (GTK_BOX (box), label);
    gtk_box_append (GTK_BOX (box), checkmark_image);

    gtk_list_item_set_child (list_item, box);
}

static void
list_factory_bind_cb (CcRemovableMediaRow *self, GtkListItem *list_item)
{
    gpointer item;
    GtkWidget *box;
    GtkWidget *icon_image;
    GtkWidget *label;
    GIcon *app_icon = NULL;
    g_autofree char *text = NULL;

    item = gtk_list_item_get_item (list_item);
    if (item == NULL)
        return;

    box = gtk_list_item_get_child (list_item);
    icon_image = gtk_widget_get_first_child (box);
    label = gtk_widget_get_next_sibling (icon_image);

    text = item_get_label (item);
    gtk_label_set_label (GTK_LABEL (label), text);

    if (G_IS_APP_INFO (item))
        app_icon = g_app_info_get_icon (item);

    if (app_icon != NULL)
        gtk_image_set_from_gicon (GTK_IMAGE (icon_image), app_icon);
    else
        gtk_image_clear (GTK_IMAGE (icon_image));

    gtk_widget_set_visible (icon_image, G_IS_APP_INFO (item));

    update_checkmark (self, list_item);

    g_signal_connect (self, "notify::selected", G_CALLBACK (on_selected_notify_cb), list_item);
}

static void
list_factory_unbind_cb (CcRemovableMediaRow *self, GtkListItem *list_item)
{
    g_signal_handlers_disconnect_by_func (self, on_selected_notify_cb, list_item);
}

static void
on_app_chooser_dialog_response (GtkDialog *dialog, int response, CcRemovableMediaRow *self)
{
    g_autoptr(GAppInfo) info = NULL;

    if (response == GTK_RESPONSE_OK)
        info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (dialog));

    gtk_window_destroy (GTK_WINDOW (dialog));

    g_signal_handlers_block_by_func (self, selected_changed_cb, self);

    if (info == NULL) {
        adw_combo_row_set_selected (ADW_COMBO_ROW (self), self->last_selected);
        g_signal_handlers_unblock_by_func (self, selected_changed_cb, self);

        return;
    }

    g_list_store_splice (self->apps, 0, g_list_model_get_n_items (G_LIST_MODEL (self->apps)), (gpointer *) &info, 1);
    adw_combo_row_set_selected (ADW_COMBO_ROW (self), 0);

    g_signal_handlers_unblock_by_func (self, selected_changed_cb, self);

    self->last_selected = 0;

    autorun_set_preferences (self, TRUE, FALSE, FALSE);
    set_default_app (self, info);
}

static void
present_app_chooser_dialog (CcRemovableMediaRow *self)
{
    GtkWidget *dialog;
    GtkRoot *root;

    root = gtk_widget_get_root (GTK_WIDGET (self));

    dialog = gtk_app_chooser_dialog_new_for_content_type (
        GTK_WINDOW (root), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, self->content_types[0]);

    if (self->heading != NULL)
        gtk_app_chooser_dialog_set_heading (GTK_APP_CHOOSER_DIALOG (dialog), self->heading);

    g_signal_connect (dialog, "response", G_CALLBACK (on_app_chooser_dialog_response), self);

    gtk_window_present (GTK_WINDOW (dialog));
}

static void
selected_changed_cb (CcRemovableMediaRow *self)
{
    guint n_apps;
    guint selected;

    selected = adw_combo_row_get_selected (ADW_COMBO_ROW (self));
    if (selected == GTK_INVALID_LIST_POSITION)
        return;

    n_apps = g_list_model_get_n_items (G_LIST_MODEL (self->apps));

    if (selected < n_apps) {
        GAppInfo *info = G_APP_INFO (adw_combo_row_get_selected_item (ADW_COMBO_ROW (self)));

        self->last_selected = selected;
        autorun_set_preferences (self, TRUE, FALSE, FALSE);
        set_default_app (self, info);

        return;
    }

    switch (selected - n_apps) {
    case MEDIA_ACTION_ASK:
        self->last_selected = selected;
        autorun_set_preferences (self, FALSE, FALSE, FALSE);
        break;
    case MEDIA_ACTION_DO_NOTHING:
        self->last_selected = selected;
        autorun_set_preferences (self, FALSE, TRUE, FALSE);
        break;
    case MEDIA_ACTION_OPEN_FOLDER:
        self->last_selected = selected;
        autorun_set_preferences (self, FALSE, FALSE, TRUE);
        break;
    case MEDIA_ACTION_OTHER_APP:
        present_app_chooser_dialog (self);
        break;
    default:
        g_assert_not_reached ();
    }
}

static void
cc_removable_media_row_constructed (GObject *object)
{
    CcRemovableMediaRow *self = CC_REMOVABLE_MEDIA_ROW (object);
    g_autoptr(GAppInfo) default_app = NULL;
    gboolean pref_start_app;
    gboolean pref_ignore;
    gboolean pref_open_folder;
    guint n_apps;

    G_OBJECT_CLASS (cc_removable_media_row_parent_class)->constructed (object);

    g_assert (self->content_types != NULL && self->content_types[0] != NULL);

    g_signal_handlers_block_by_func (self, selected_changed_cb, self);

    default_app = g_app_info_get_default_for_type (self->content_types[0], FALSE);
    if (G_IS_APP_INFO (default_app))
        g_list_store_append (self->apps, default_app);

    g_list_store_append (self->sections, self->apps);
    g_list_store_append (self->sections, self->actions);

    pref_start_app = autorun_get_preference (self, PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
    pref_ignore = autorun_get_preference (self, PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
    pref_open_folder = autorun_get_preference (self, PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);

    n_apps = g_list_model_get_n_items (G_LIST_MODEL (self->apps));

    if (pref_ignore)
        self->last_selected = n_apps + MEDIA_ACTION_DO_NOTHING;
    else if (pref_open_folder)
        self->last_selected = n_apps + MEDIA_ACTION_OPEN_FOLDER;
    else if (pref_start_app && n_apps > 0)
        self->last_selected = 0;
    else
        self->last_selected = n_apps + MEDIA_ACTION_ASK;

    adw_combo_row_set_selected (ADW_COMBO_ROW (self), self->last_selected);

    g_signal_handlers_unblock_by_func (self, selected_changed_cb, self);
}

static void
cc_removable_media_row_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    CcRemovableMediaRow *self = CC_REMOVABLE_MEDIA_ROW (object);

    switch (prop_id) {
    case PROP_CONTENT_TYPES:
        g_value_set_boxed (value, self->content_types);
        break;
    case PROP_HEADING:
        g_value_set_string (value, self->heading);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_removable_media_row_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    CcRemovableMediaRow *self = CC_REMOVABLE_MEDIA_ROW (object);

    switch (prop_id) {
    case PROP_CONTENT_TYPES:
        self->content_types = g_value_dup_boxed (value);
        break;
    case PROP_HEADING:
        self->heading = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_removable_media_row_finalize (GObject *object)
{
    CcRemovableMediaRow *self = CC_REMOVABLE_MEDIA_ROW (object);

    g_clear_object (&self->settings);
    g_clear_pointer (&self->content_types, g_strfreev);
    g_clear_pointer (&self->heading, g_free);

    G_OBJECT_CLASS (cc_removable_media_row_parent_class)->finalize (object);
}

static void
cc_removable_media_row_class_init (CcRemovableMediaRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->constructed = cc_removable_media_row_constructed;
    object_class->get_property = cc_removable_media_row_get_property;
    object_class->set_property = cc_removable_media_row_set_property;
    object_class->finalize = cc_removable_media_row_finalize;

    properties[PROP_CONTENT_TYPES] = g_param_spec_boxed (
        "content-types", NULL, NULL, G_TYPE_STRV, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
    properties[PROP_HEADING] = g_param_spec_string (
        "heading", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/control-center/applications/cc-removable-media-row.ui");

    gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaRow, sections);
    gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaRow, apps);
    gtk_widget_class_bind_template_child (widget_class, CcRemovableMediaRow, actions);

    gtk_widget_class_bind_template_callback (widget_class, factory_setup_cb);
    gtk_widget_class_bind_template_callback (widget_class, factory_bind_cb);
    gtk_widget_class_bind_template_callback (widget_class, list_factory_setup_cb);
    gtk_widget_class_bind_template_callback (widget_class, list_factory_bind_cb);
    gtk_widget_class_bind_template_callback (widget_class, list_factory_unbind_cb);
    gtk_widget_class_bind_template_callback (widget_class, selected_changed_cb);
}

static void
cc_removable_media_row_init (CcRemovableMediaRow *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    self->settings = g_settings_new (MEDIA_HANDLING_SCHEMA);
}
