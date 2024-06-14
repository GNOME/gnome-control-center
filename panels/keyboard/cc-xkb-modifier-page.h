/* cc-xkb-modifier-page.h
 *
 * Copyright 2019 Bastien Nocera <hadess@hadess.net>
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

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct
{
  gchar *label;
  gchar *xkb_option;
} CcXkbOption;

typedef struct
{
  gchar *prefix;
  gchar *title;
  gchar *description;
  CcXkbOption *options;
  gchar *default_option;
} CcXkbModifier;

#define CC_TYPE_XKB_MODIFIER_PAGE (cc_xkb_modifier_page_get_type())
G_DECLARE_FINAL_TYPE (CcXkbModifierPage, cc_xkb_modifier_page, CC, XKB_MODIFIER_PAGE, AdwNavigationPage)

CcXkbModifierPage *cc_xkb_modifier_page_new (GSettings *input_settings, const CcXkbModifier*);

gboolean xcb_modifier_transform_binding_to_label (GValue*, GVariant*, gpointer);

G_END_DECLS
