/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2022 Red Hat, Inc
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
 * Author: Felipe Borges <feborges@redhat.com>
 */

#pragma once

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_LOCK_PANEL (cc_remote_desktop_panel_get_type ())
G_DECLARE_FINAL_TYPE (CcRemoteDesktopPanel, cc_remote_desktop_panel, CC, REMOTE_DESKTOP_PANEL, CcPanel)

G_END_DECLS
