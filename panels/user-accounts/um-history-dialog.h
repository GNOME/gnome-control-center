/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Written by: Ondrej Holy <oholy@redhat.com>
 */

#ifndef __UM_HISTORY_DIALOG_H__
#define __UM_HISTORY_DIALOG_H__

#include <gtk/gtk.h>
#include <act/act-user.h>

G_BEGIN_DECLS

typedef struct _UmHistoryDialog UmHistoryDialog;

UmHistoryDialog *um_history_dialog_new      (void);
void             um_history_dialog_free     (UmHistoryDialog *dialog);
void             um_history_dialog_set_user (UmHistoryDialog *dialog,
                                             ActUser         *user);
void             um_history_dialog_show     (UmHistoryDialog *dialog,
                                             GtkWindow       *parent);

G_END_DECLS

#endif
