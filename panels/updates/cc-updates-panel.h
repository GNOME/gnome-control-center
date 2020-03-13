/* cc-updates-panel.h
 *
 * Copyright © 2018 Endless, Inc
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
 * Authors: Georges Basile Stavracas Neto <georges@endlessm.com>
 */

#pragma once

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_UPDATES_PANEL (cc_updates_panel_get_type())

G_DECLARE_FINAL_TYPE (CcUpdatesPanel, cc_updates_panel, CC, UPDATES_PANEL, CcPanel)

CcUpdatesPanel *cc_updates_panel_new (void);

G_END_DECLS