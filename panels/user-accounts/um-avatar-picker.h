/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2013 Red Hat, Inc,
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by:
 *   Jasper St. Pierre <jstpierre@mecheye.net>
 *   Bogdan Ciobanu <bgdn.ciobanu@gmail.com>
 */

#ifndef __UM_AVATAR_PICKER_H__
#define __UM_AVATAR_PICKER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_AVATAR_PICKER            (um_avatar_picker_get_type ())
#define UM_AVATAR_PICKER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_AVATAR_PICKER, UmAvatarPicker))
#define UM_AVATAR_PICKER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  UM_TYPE_AVATAR_PICKER, UmAvatarPickerClass))
#define UM_IS_AVATAR_PICKER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_AVATAR_PICKER))
#define UM_IS_AVATAR_PICKER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  UM_TYPE_AVATAR_PICKER))
#define UM_AVATAR_PICKER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  UM_TYPE_AVATAR_PICKER, UmAvatarPickerClass))

typedef struct _UmAvatarPicker      UmAvatarPicker;
typedef struct _UmAvatarPickerClass UmAvatarPickerClass;

struct _UmAvatarPicker
{
    GtkDialog parent;
};

struct _UmAvatarPickerClass
{
    GtkDialogClass parent_class;
};

G_GNUC_CONST GType um_avatar_picker_get_type (void);

GtkWidget *um_avatar_picker_new (void);
GdkPixbuf *um_avatar_picker_get_avatar (UmAvatarPicker *picker);

G_END_DECLS

#endif /* __UM_AVATAR_PICKER_H__ */
