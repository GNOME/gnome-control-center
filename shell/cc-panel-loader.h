/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
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

#include <glib.h>
#include <glib-object.h>
#include <shell/cc-panel.h>
#include <shell/cc-shell-model.h>

G_BEGIN_DECLS

void     cc_panel_loader_fill_model     (CcShellModel  *model);
GList   *cc_panel_loader_get_panels     (void);
CcPanel *cc_panel_loader_load_by_name   (CcShell       *shell,
                                         const char    *name,
                                         GVariant      *parameters);

G_END_DECLS

