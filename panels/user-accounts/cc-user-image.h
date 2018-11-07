/*
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2015 Red Hat, Inc.
 */

#pragma once

#include <gtk/gtk.h>
#include <act/act.h>

G_BEGIN_DECLS

#define CC_TYPE_USER_IMAGE (cc_user_image_get_type ())
G_DECLARE_FINAL_TYPE (CcUserImage, cc_user_image, CC, USER_IMAGE, GtkImage)

GtkWidget *cc_user_image_new      (void);
void       cc_user_image_set_user (CcUserImage *image, ActUser *user);

G_END_DECLS
