/*
 * cc-enterprise-login-dialog.h
 *
 * Copyright 2023 Red Hat Inc
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Author(s):
 *  Felipe Borges <felipeborges@gnome.org>
 */

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_ENTERPRISE_LOGIN_DIALOG (cc_enterprise_login_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcEnterpriseLoginDialog, cc_enterprise_login_dialog, CC, ENTERPRISE_LOGIN_DIALOG, AdwDialog)

CcEnterpriseLoginDialog *cc_enterprise_login_dialog_new      (void);

G_END_DECLS

