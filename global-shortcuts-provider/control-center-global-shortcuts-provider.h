/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#pragma once

#include <gtk/gtk.h>

#include "cc-global-shortcuts-provider.h"
#include <shell/cc-shell-model.h>

G_BEGIN_DECLS

#define CC_TYPE_GLOBAL_SHORTCUTS_PROVIDER_APP cc_global_shortcuts_provider_app_get_type ()
G_DECLARE_FINAL_TYPE (CcGlobalShortcutsProviderApp,
                      cc_global_shortcuts_provider_app,
                      CC, GLOBAL_SHORTCUTS_PROVIDER_APP,
                      AdwApplication)

CcGlobalShortcutsProviderApp *cc_global_shortcuts_provider_app_get (void);

G_END_DECLS
