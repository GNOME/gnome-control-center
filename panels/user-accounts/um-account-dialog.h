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

#ifndef __UM_ACCOUNT_DIALOG_H__
#define __UM_ACCOUNT_DIALOG_H__

#include <act/act.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_ACCOUNT_DIALOG      (um_account_dialog_get_type ())
#define UM_ACCOUNT_DIALOG(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_ACCOUNT_DIALOG, UmAccountDialog))
#define UM_IS_ACCOUNT_DIALOG(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_ACCOUNT_DIALOG))

typedef struct _UmAccountDialog UmAccountDialog;
typedef struct _UmAccountDialogClass UmAccountDialogClass;

GType            um_account_dialog_get_type (void) G_GNUC_CONST;
UmAccountDialog *um_account_dialog_new      (void);
void             um_account_dialog_show     (UmAccountDialog     *self,
                                             GtkWindow           *parent,
                                             GPermission         *permission,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data);
ActUser *        um_account_dialog_finish   (UmAccountDialog     *self,
                                             GAsyncResult        *result);

G_END_DECLS

#endif
