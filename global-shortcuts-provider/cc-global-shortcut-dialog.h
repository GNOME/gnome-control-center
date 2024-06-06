/* cc-global-shortcut-dialog.h
 *
 * Copyright (C) 2020 System76, Inc.
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
 * Author: Ian Douglas Scott <idscott@system76.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "cc-keyboard-manager.h"
#include <adwaita.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CC_TYPE_GLOBAL_SHORTCUT_DIALOG (cc_global_shortcut_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcGlobalShortcutDialog,
                      cc_global_shortcut_dialog,
                      CC, GLOBAL_SHORTCUT_DIALOG,
                      AdwWindow)

CcGlobalShortcutDialog *cc_global_shortcut_dialog_new (const char *app_id,
                                                       const char *parent_window,
                                                       GVariant   *shortcuts);

void cc_global_shortcut_dialog_present (CcGlobalShortcutDialog *dialog);

G_END_DECLS
