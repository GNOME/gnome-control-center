/*
 * cc-security-login-page.h
 *
 * Copyright 2026 Red Hat
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
#include <act/act.h>

G_BEGIN_DECLS

#define CC_TYPE_SECURITY_LOGIN_PAGE (cc_security_login_page_get_type ())
G_DECLARE_FINAL_TYPE (CcSecurityLoginPage, cc_security_login_page, CC, SECURITY_LOGIN_PAGE, AdwNavigationPage)

void cc_security_login_page_set_user (CcSecurityLoginPage *self,
                                      ActUser             *user);

G_END_DECLS

