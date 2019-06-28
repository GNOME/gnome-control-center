/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2019  Red Hat, Inc,
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
 * Written by: Kalev Lember <klember@redhat.com>
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SUBSCRIPTION_REGISTER_DIALOG (cc_subscription_register_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcSubscriptionRegisterDialog, cc_subscription_register_dialog, CC, SUBSCRIPTION_REGISTER_DIALOG, GtkDialog)

CcSubscriptionRegisterDialog *cc_subscription_register_dialog_new (GDBusProxy *subscription_proxy);

G_END_DECLS
