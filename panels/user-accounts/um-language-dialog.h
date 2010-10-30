/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __UM_LANGUAGE_DIALOG_H__
#define __UM_LANGUAGE_DIALOG_H__

#include <gtk/gtk.h>
#include "um-user.h"

G_BEGIN_DECLS

typedef struct _UmLanguageDialog UmLanguageDialog;

UmLanguageDialog *um_language_dialog_new      (void);
void              um_language_dialog_free     (UmLanguageDialog *dialog);
void              um_language_dialog_set_user (UmLanguageDialog *dialog,
                                               UmUser            *user);
void              um_language_dialog_show     (UmLanguageDialog *dialog,
                                               GtkWindow        *parent);
void              um_add_user_languages       (GtkTreeModel     *model);
gchar            *um_get_current_language     (void);
gboolean          um_get_iter_for_language    (GtkTreeModel     *model,
                                               const gchar      *lang,
                                               GtkTreeIter      *iter);

GtkWidget        *um_language_chooser_new          (void);
gchar            *um_language_chooser_get_language (GtkWidget *chooser);

G_END_DECLS

#endif
