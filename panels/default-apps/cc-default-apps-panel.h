/*
 * Copyright (C) 2016 Endless, Inc
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
 * Authors: Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *
 */

#ifndef CC_DEFAULT_APPS_PANEL_H
#define CC_DEFAULT_APPS_PANEL_H

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_DEFAULT_APPS_PANEL (cc_default_apps_panel_get_type())

G_DECLARE_FINAL_TYPE (CcDefaultAppsPanel, cc_default_apps_panel, CC, DEFAULT_APPS_PANEL, CcPanel)

G_END_DECLS

#endif /* CC_DEFAULT_APPS_PANEL_H */

