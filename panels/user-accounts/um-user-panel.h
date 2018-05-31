/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef _UM_USER_PANEL_H
#define _UM_USER_PANEL_H

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define UM_TYPE_USER_PANEL (cc_user_panel_get_type ())
G_DECLARE_FINAL_TYPE (CcUserPanel, cc_user_panel, UM, USER_PANEL, CcPanel)

G_END_DECLS

#endif /* _UM_USER_PANEL_H */
