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

#include "keyboard-shortcuts.h"
#include <adwaita.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define CC_TYPE_APPLICATION_SHORTCUT_DIALOG (cc_application_shortcut_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcApplicationShortcutDialog, cc_application_shortcut_dialog, CC, APPLICATION_SHORTCUT_DIALOG, AdwDialog)

CcApplicationShortcutDialog *cc_application_shortcut_dialog_new (const char *app_id);

G_END_DECLS
