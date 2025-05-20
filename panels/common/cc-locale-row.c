/* cc-locale-row.c
 *
 * Copyright 2025 Hari Rana <theevilskeleton@riseup.net>
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

#include "cc-locale-row.h"
#include "cc-common-resources.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

/**
 * CcLocaleRow:
 *
 * A [class@Gtk.ListBoxRow] representing locale information.
 */

struct _CcLocaleRow {
  GtkListBoxRow parent_instance;

  GtkImage *check_image;
  GtkLabel *description_label;
  GtkLabel *title_label;

  gchar *locale_id;
  gchar *language;
  gchar *language_local;
  gchar *country;
  gchar *country_local;

  gboolean is_extra;
};

G_DEFINE_TYPE (CcLocaleRow, cc_locale_row, GTK_TYPE_LIST_BOX_ROW)

static gchar *
get_language_label (const gchar *language_code,
                    const gchar *modifier,
                    const gchar *locale_id)
{
  g_autofree gchar *language = NULL;

  language = gnome_get_language_from_code (language_code, locale_id);

  if (modifier == NULL)
    return g_steal_pointer (&language);
  else
    {
      g_autofree gchar *t_mod = gnome_get_translated_modifier (modifier, locale_id);
      return g_strdup_printf ("%s — %s", language, t_mod);
    }
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
cc_locale_row_state_flags_changed (GtkWidget     *widget,
                                   GtkStateFlags  previous_state_flags)
{
  CcLocaleRow *self = CC_LOCALE_ROW (widget);
  gboolean is_selected;

  is_selected = !!(gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_SELECTED);

  gtk_widget_set_visible (GTK_WIDGET (self->check_image),
                          gtk_widget_get_state_flags (widget) & GTK_STATE_FLAG_SELECTED);

  gtk_accessible_update_state (GTK_ACCESSIBLE (self),
                               GTK_ACCESSIBLE_STATE_CHECKED, is_selected,
                               -1);

  GTK_WIDGET_CLASS (cc_locale_row_parent_class)->state_flags_changed (widget, previous_state_flags);
}

static void
cc_locale_row_class_init (CcLocaleRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_locale_row_dispose;

  widget_class->state_flags_changed = cc_locale_row_state_flags_changed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-locale-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, check_image);
  gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, description_label);
  gtk_widget_class_bind_template_child (widget_class, CcLocaleRow, title_label);

  gtk_widget_class_set_accessible_role (widget_class, GTK_ACCESSIBLE_ROLE_RADIO);
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
 *
 * Create a new #CcLocaleRow based on the locale ID.
 *
 * Returns: (transfer full): a newly created #CcLocaleRow
 */
CcLocaleRow *
cc_locale_row_new (const gchar *locale_id)
{
  CcLocaleRow *self;
  g_autofree gchar *language_code = NULL;
  g_autofree gchar *country_code = NULL;
  g_autofree gchar *modifier = NULL;

  self = g_object_new (CC_TYPE_LOCALE_ROW, NULL);
  self->locale_id = g_strdup (locale_id);

  gnome_parse_locale (locale_id, &language_code, &country_code, NULL, &modifier);

  self->language = get_language_label (language_code, modifier, locale_id);
  self->language_local = get_language_label (language_code, modifier, NULL);
  gtk_label_set_label (self->title_label, self->language);

  if (country_code == NULL)
    {
      self->country = NULL;
      self->country_local = NULL;
    }
  else
    {
      self->country = gnome_get_country_from_code (country_code, locale_id);
      self->country_local = gnome_get_country_from_code (country_code, NULL);
      gtk_label_set_label (self->description_label, self->country);
    }

  return self;
}

/**
 * cc_locale_row_get_locale_id:
 * @self: a `CcLocaleRow`
 *
 * Get the locale ID.
 *
 * Returns: (nullable): the locale ID
 */
const gchar *
cc_locale_row_get_locale_id (CcLocaleRow *self)
{
  g_return_val_if_fail (CC_IS_LOCALE_ROW (self), NULL);
  return self->locale_id;
}

/**
 * cc_locale_row_get_language:
 * @self: a `CcLocaleRow`
 *
 * Get the language.
 *
 * Returns: (nullable): the language
 */
const gchar *
cc_locale_row_get_language (CcLocaleRow *self)
{
  g_return_val_if_fail (CC_IS_LOCALE_ROW (self), NULL);
  return self->language;
}

/**
 * cc_locale_row_get_language_local:
 * @self: a `CcLocaleRow`
 *
 * Get the system's language.
 *
 * Returns: (nullable): the system's language
 */
const gchar *
cc_locale_row_get_language_local (CcLocaleRow *self)
{
  g_return_val_if_fail (CC_IS_LOCALE_ROW (self), NULL);
  return self->language_local;
}

/**
 * cc_locale_row_get_country:
 * @self: a `CcLocaleRow`
 *
 * Get the country.
 *
 * Returns: (nullable): the country
 */
const gchar *
cc_locale_row_get_country (CcLocaleRow *self)
{
  g_return_val_if_fail (CC_IS_LOCALE_ROW (self), NULL);
  return self->country;
}

/**
 * cc_locale_row_get_country_local:
 * @self: a `CcLocaleRow`
 *
 * Get the system's country.
 *
 * Returns: (nullable): the system's country
 */
const gchar *
cc_locale_row_get_country_local (CcLocaleRow *self)
{
  g_return_val_if_fail (CC_IS_LOCALE_ROW (self), NULL);
  return self->country_local;
}

/**
 * cc_locale_row_set_is_extra:
 * @self: a `CcLocaleRow`
 * @is_extra: the new value to set
 *
 * Hint the row about whether it should display additional information.
 */
void
cc_locale_row_set_is_extra (CcLocaleRow *self, gboolean is_extra)
{
  g_return_if_fail (CC_IS_LOCALE_ROW (self));
  self->is_extra = is_extra;
}

/**
 * cc_locale_row_get_is_extra:
 * @self: a `CcLocaleRow`
 *
 * Return whether the row is displaying additional information.
 *
 * Returns: whether the row is displaying additional information
 */
gboolean
cc_locale_row_get_is_extra (CcLocaleRow *self)
{
  g_return_val_if_fail (CC_IS_LOCALE_ROW (self), FALSE);
  return self->is_extra;
}
