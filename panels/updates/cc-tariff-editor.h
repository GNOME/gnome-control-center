/* cc-tariff-editor.c
 *
 * Copyright © 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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
 */

#pragma once

#include <glib.h>
#include <gtk/gtk.h>
#include <libmogwai-tariff/tariff.h>

G_BEGIN_DECLS

typedef enum
{
  CC_TARIFF_EDITOR_ERROR_NOT_SUPPORTED,
  CC_TARIFF_EDITOR_ERROR_WRONG_FORMAT,
} CcTariffEditorError;

#define CC_TARIFF_EDITOR_ERROR (cc_tariff_editor_error_quark ())
#define CC_TYPE_TARIFF_EDITOR  (cc_tariff_editor_get_type())

G_DECLARE_FINAL_TYPE (CcTariffEditor, cc_tariff_editor, CC, TARIFF_EDITOR, GtkGrid)

GQuark    cc_tariff_editor_error_quark           (void);

GVariant* cc_tariff_editor_get_tariff_as_variant (CcTariffEditor  *self);

void      cc_tariff_editor_load_tariff           (CcTariffEditor  *self,
                                                  MwtTariff       *tariff,
                                                  GError         **error);

G_END_DECLS
