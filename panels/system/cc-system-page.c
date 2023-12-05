/*
 * cc-system-page.c
 *
 * Copyright 2023 Red Hat Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors:
 *  Felipe Borges <felipeborges@gnome.org>
 */

#include "cc-system-page.h"

typedef struct
{
  const gchar *summary;
} CcSystemPagePrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (CcSystemPage, cc_system_page, ADW_TYPE_NAVIGATION_PAGE,
                         G_ADD_PRIVATE (CcSystemPage))

enum {
  PROP_0,
  PROP_SUMMARY,
  LAST_PROP,
};

static GParamSpec *props[LAST_PROP];

static void
cc_system_page_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  CcSystemPage *self = CC_SYSTEM_PAGE (object);

  switch (prop_id) {
  case PROP_SUMMARY:
    g_value_set_string (value, cc_system_page_get_summary (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
cc_system_page_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  CcSystemPage *self = CC_SYSTEM_PAGE (object);

  switch (prop_id) {
  case PROP_SUMMARY:
    cc_system_page_set_summary (self, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
cc_system_page_class_init (CcSystemPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_system_page_get_property;
  object_class->set_property = cc_system_page_set_property;

  props[PROP_SUMMARY] =
    g_param_spec_string ("summary", NULL, NULL, "",
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
cc_system_page_init (CcSystemPage *self)
{
}

const gchar *
cc_system_page_get_summary (CcSystemPage *self)
{
  g_return_val_if_fail (CC_IS_SYSTEM_PAGE (self), NULL);

  CcSystemPagePrivate *priv = cc_system_page_get_instance_private (self);

  return priv->summary;
}

void
cc_system_page_set_summary (CcSystemPage *self,
                            const gchar  *summary)
{
  g_return_if_fail (CC_IS_SYSTEM_PAGE (self));

  CcSystemPagePrivate *priv = cc_system_page_get_instance_private (self);

  priv->summary = g_strdup (summary);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_SUMMARY]);
}
