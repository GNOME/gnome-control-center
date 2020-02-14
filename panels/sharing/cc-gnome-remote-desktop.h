/*
 * Copyright (C) 2018 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#pragma once

#include <gtk/gtk.h>
#include <libsecret/secret.h>

G_BEGIN_DECLS

const SecretSchema * cc_grd_vnc_password_get_schema (void);
#define CC_GRD_VNC_PASSWORD_SCHEMA cc_grd_vnc_password_get_schema ()

gboolean cc_grd_get_is_auth_method_prompt (GValue   *value,
                                           GVariant *variant,
                                           gpointer  user_data);

GVariant * cc_grd_set_is_auth_method_prompt (const GValue       *value,
                                             const GVariantType *type,
                                             gpointer            user_data);

gboolean cc_grd_get_is_auth_method_password (GValue   *value,
                                             GVariant *variant,
                                             gpointer  user_data);

GVariant * cc_grd_set_is_auth_method_password (const GValue       *value,
                                               const GVariantType *type,
                                               gpointer            user_data);

void cc_grd_on_vnc_password_entry_notify_text (GtkEntry   *entry,
                                               GParamSpec *pspec,
                                               gpointer    user_data);

void cc_grd_update_password_entry (GtkEntry *entry);

G_END_DECLS
