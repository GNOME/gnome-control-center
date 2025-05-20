/* cc-locale-row.c
 *
 * Copyright (C) 2020 Canonical Ltd.
 * Copyright 2026 The GNOME Project
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

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-locale-row"

#include "cc-locale-row.h"
#include "cc-common-resources.h"
#include "cc-locale-row-enums.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

/**
 * CcLocaleRow:
 *
 * A [class@Gtk.ListBoxRow] displaying locale information.
 *
 * #CcLocaleRow displays the title and description of a given
 * region format or language, displaying a check mark next
 * to the language/region name when selected by the user.
 *
 * #CcLocaleRow also allows adding additional widgets to the end
 * using [method@CcLocaleRow.add_suffix].
 */

struct _CcLocaleRow {
    GtkListBoxRow parent_instance;

    GtkBox *box;
    GtkImage *check_image;
    GtkLabel *description_label;
    GtkLabel *title_label;

    char *locale_id;
    char *language;
    char *language_local;
    char *country;
    char *country_local;

    gboolean is_extra;
    CcLocaleLayoutType locale_layout_type;
};

G_DEFINE_FINAL_TYPE (CcLocaleRow, cc_locale_row, GTK_TYPE_LIST_BOX_ROW)

enum {
    PROP_0,
    PROP_IS_EXTRA,
    PROP_LOCALE_LAYOUT_TYPE,
    PROP_LOCALE_ID,
    N_PROPS,
};

static GParamSpec *properties[N_PROPS];

static char *
dup_language_label (const char *language_code, const char *modifier, const char *locale_id)
{
    g_autofree char *language = NULL;
    g_autofree char *t_mod = NULL;

    language = gnome_get_language_from_code (language_code, locale_id);

    if (!modifier)
        return g_strdup (language);

    t_mod = gnome_get_translated_modifier (modifier, locale_id);
    return g_strdup_printf ("%s — %s", language, t_mod);
}

static inline void
set_locale_id (CcLocaleRow *self, const char *locale_id)
{
    g_autofree char *language_code = NULL;
    g_autofree char *country_code = NULL;
    g_autofree char *modifier = NULL;
    GtkLabel *language_label;
    GtkLabel *region_label;

    g_assert (locale_id != NULL && *locale_id != '\0');
    self->locale_id = g_strdup (locale_id);

    gnome_parse_locale (self->locale_id, &language_code, &country_code, NULL, &modifier);

    switch (self->locale_layout_type) {
    case CC_LOCALE_LAYOUT_TYPE_REGION:
        region_label = self->title_label;
        language_label = self->description_label;
        break;
    case CC_LOCALE_LAYOUT_TYPE_LANGUAGE:
        region_label = self->description_label;
        language_label = self->title_label;
        break;
    case CC_LOCALE_LAYOUT_TYPE_NONE:
    default:
        g_assert_not_reached ();
    }

    self->language = dup_language_label (language_code, modifier, locale_id);
    self->language_local = dup_language_label (language_code, modifier, NULL);
    gtk_label_set_label (language_label, self->language);

    if (country_code) {
        self->country = gnome_get_country_from_code (country_code, locale_id);
        self->country_local = gnome_get_country_from_code (country_code, NULL);
        gtk_label_set_label (region_label, self->country);
    } else {
        self->country = NULL;
        self->country_local = NULL;
        gtk_label_set_label (region_label, NULL);
    }
}

static inline void
set_locale_layout_type (CcLocaleRow *self, CcLocaleLayoutType locale_layout_type)
{
    self->locale_layout_type = locale_layout_type;
}

static void
cc_locale_row_dispose (GObject *object)
{
    CcLocaleRow *self = CC_LOCALE_ROW (object);

    g_clear_pointer (&self->locale_id, g_free);
    g_clear_pointer (&self->country, g_free);
    g_clear_pointer (&self->country_local, g_free);
    g_clear_pointer (&self->language, g_free);
    g_clear_pointer (&self->language_local, g_free);

    G_OBJECT_CLASS (cc_locale_row_parent_class)->dispose (object);
}

static void
cc_locale_row_state_flags_changed (GtkWidget *widget, GtkStateFlags previous_state_flags)
{
    CcLocaleRow *self = CC_LOCALE_ROW (widget);
    gboolean is_selected;

    is_selected = !!(gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_SELECTED);

    gtk_widget_set_visible (GTK_WIDGET (self->check_image), is_selected);

    gtk_accessible_update_state (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_STATE_CHECKED, is_selected, -1);

    GTK_WIDGET_CLASS (cc_locale_row_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
cc_locale_row_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    CcLocaleRow *self = CC_LOCALE_ROW (object);

    switch (prop_id) {
    case PROP_IS_EXTRA:
        g_value_set_boolean (value, cc_locale_row_get_is_extra (self));
        break;

    case PROP_LOCALE_ID:
        g_value_set_string (value, cc_locale_row_get_locale_id (self));
        break;

    case PROP_LOCALE_LAYOUT_TYPE:
        g_value_set_enum (value, cc_locale_row_get_locale_layout_type (self));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_locale_row_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    CcLocaleRow *self = CC_LOCALE_ROW (object);

    switch (prop_id) {
    case PROP_IS_EXTRA:
        cc_locale_row_set_is_extra (self, g_value_get_boolean (value));
        break;

    case PROP_LOCALE_ID:
        set_locale_id (self, g_value_get_string (value));
        break;

    case PROP_LOCALE_LAYOUT_TYPE:
        set_locale_layout_type (self, g_value_get_enum (value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_locale_row_class_init (CcLocaleRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = cc_locale_row_dispose;
    object_class->get_property = cc_locale_row_get_property;
    object_class->set_property = cc_locale_row_set_property;

    widget_class->state_flags_changed = cc_locale_row_state_flags_changed;

    /**
     * CcLocaleRow:is-extra:
     *
     * Hint whether the row is identified as an uncommon language.
     */
    properties[PROP_IS_EXTRA] = g_param_spec_boolean (
        "is-extra", NULL, NULL, FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    /**
     * CcLocaleRow:locale-id:
     *
     * The locale ID.
     */
    properties[PROP_LOCALE_ID] = g_param_spec_string (
        "locale-id", NULL, NULL, NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
     * CcLocaleRow:locale-layout-type:
     *
     * The locale layout type.
     *
     * Avoid using [enum@Cc.LocaleLayoutType.NONE] as this will cause an assertion failure.
     */
    properties[PROP_LOCALE_LAYOUT_TYPE] =
        g_param_spec_enum ("locale-layout-type", NULL, NULL, CC_TYPE_LOCALE_LAYOUT_TYPE, CC_LOCALE_LAYOUT_TYPE_NONE,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-locale-row.ui");

    gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, box);
    gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, check_image);
    gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, description_label);
    gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, title_label);
}

static void
cc_locale_row_init (CcLocaleRow *self)
{
    g_resources_register (cc_common_get_resource ());

    gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * cc_locale_row_new:
 * @locale_id: the locale ID
 * @layout_type: a #CcLocaleLayoutType
 *
 * Create a new #CcLocaleRow based on the locale ID. The #CcLocaleLayoutType
 * will adjust the title and description of the row.
 *
 * Returns: (transfer full): a newly created #CcLocaleRow
 */
GtkWidget *
cc_locale_row_new (const char *locale_id, CcLocaleLayoutType locale_layout_type)
{
    return g_object_new (CC_TYPE_LOCALE_ROW, "locale-layout-type", locale_layout_type, "locale-id", locale_id, NULL);
}

CcLocaleLayoutType
cc_locale_row_get_locale_layout_type (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->locale_layout_type;
}

/**
 * cc_locale_row_get_locale_id:
 * @self: a #CcLocaleRow
 *
 * Get the locale ID.
 *
 * Returns: (nullable) (transfer none): the locale ID
 */
const char *
cc_locale_row_get_locale_id (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->locale_id;
}

/**
 * cc_locale_row_get_language:
 * @self: a #CcLocaleRow
 *
 * Get the language.
 *
 * Returns: (nullable) (transfer none): the language
 */
const char *
cc_locale_row_get_language (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->language;
}

/**
 * cc_locale_row_get_language_local:
 * @self: a #CcLocaleRow
 *
 * Get the system's language.
 *
 * Returns: (nullable) (transfer none): the system's language
 */
const char *
cc_locale_row_get_language_local (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->language_local;
}

/**
 * cc_locale_row_get_country:
 * @self: a #CcLocaleRow
 *
 * Get the country.
 *
 * Returns: (nullable) (transfer none): the country
 */
const char *
cc_locale_row_get_country (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->country;
}

/**
 * cc_locale_row_get_country_local:
 * @self: a #CcLocaleRow
 *
 * Get the system's country.
 *
 * Returns: (nullable) (transfer none): the system's country
 */
const char *
cc_locale_row_get_country_local (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->country_local;
}

/**
 * cc_locale_row_get_is_extra:
 * @self: a #CcLocaleRow
 *
 * Return whether this row is identified as an uncommon language.
 *
 * Returns: %TRUE if the row is identified as an uncommon language, %FALSE otherwise
 */
gboolean
cc_locale_row_get_is_extra (CcLocaleRow *self)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    return self->is_extra;
}

/**
 * cc_locale_row_set_is_extra:
 * @self: a #CcLocaleRow
 * @is_extra: the new value to set
 *
 * Set whether this row is identified as an uncommon language.
 */
void
cc_locale_row_set_is_extra (CcLocaleRow *self, gboolean is_extra)
{
    g_assert (CC_IS_LOCALE_ROW (self));

    if (self->is_extra == is_extra)
        return;

    self->is_extra = is_extra;

    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IS_EXTRA]);
}

/**
 * cc_locale_row_add_suffix:
 * @self: a #CcLocaleRow
 * @widget: a widget to append
 *
 * Append @widget to @self.
 */
void
cc_locale_row_add_suffix (CcLocaleRow *self, GtkWidget *widget)
{
    g_assert (CC_IS_LOCALE_ROW (self));
    g_assert (GTK_IS_WIDGET (widget));
    g_assert (gtk_widget_get_parent (widget) == NULL);

    gtk_box_append (self->box, widget);
}
