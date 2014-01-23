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

#ifndef __UM_PASSWORD_DIALOG_H__
#define __UM_PASSWORD_DIALOG_H__

#include <gtk/gtk.h>
#include <act/act.h>

G_BEGIN_DECLS

typedef struct _UmPasswordDialog UmPasswordDialog;

UmPasswordDialog *um_password_dialog_new      (void);
void              um_password_dialog_free     (UmPasswordDialog *dialog);
void              um_password_dialog_set_user (UmPasswordDialog *dialog,
                                               ActUser          *user);
void              um_password_dialog_show     (UmPasswordDialog *dialog,
                                               GtkWindow        *parent);

G_END_DECLS

#endif
