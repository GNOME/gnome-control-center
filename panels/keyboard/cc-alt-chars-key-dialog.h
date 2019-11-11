/* cc-alt-chars-key-dialog.h
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_ALT_CHARS_KEY_DIALOG (cc_alt_chars_key_dialog_get_type())
G_DECLARE_FINAL_TYPE (CcAltCharsKeyDialog, cc_alt_chars_key_dialog, CC, ALT_CHARS_KEY_DIALOG, GtkDialog)

CcAltCharsKeyDialog *cc_alt_chars_key_dialog_new (GSettings *input_settings);

G_END_DECLS
