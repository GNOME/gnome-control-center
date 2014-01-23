/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012 - 2013 Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#ifndef __PP_AUTHENTICATION_DIALOG_H__
#define __PP_AUTHENTICATION_DIALOG_H__

#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_AUTHENTICATION_DIALOG            (pp_authentication_dialog_get_type ())
#define PP_AUTHENTICATION_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), PP_TYPE_AUTHENTICATION_DIALOG, PpAuthenticationDialog))
#define PP_AUTHENTICATION_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PP_TYPE_AUTHENTICATION_DIALOG, PpAuthenticationDialogClass))
#define PP_IS_AUTHENTICATION_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), PP_TYPE_AUTHENTICATION_DIALOG))
#define PP_IS_AUTHENTICATION_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PP_TYPE_AUTHENTICATION_DIALOG))
#define PP_AUTHENTICATION_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PP_TYPE_AUTHENTICATION_DIALOG, PpAuthenticationDialogClass))

typedef struct _PpAuthenticationDialog        PpAuthenticationDialog;
typedef struct _PpAuthenticationDialogClass   PpAuthenticationDialogClass;
typedef struct _PpAuthenticationDialogPrivate PpAuthenticationDialogPrivate;

struct _PpAuthenticationDialog
{
  GObject                        parent_instance;
  PpAuthenticationDialogPrivate *priv;
};

struct _PpAuthenticationDialogClass
{
  GObjectClass parent_class;

  void (*response)      (PpAuthenticationDialog *dialog,
                         gint                    response_id,
                         const gchar            *username,
                         const gchar            *password);
};

GType                   pp_authentication_dialog_get_type (void) G_GNUC_CONST;
PpAuthenticationDialog *pp_authentication_dialog_new      (GtkWindow   *parent,
                                                           const gchar *text,
                                                           const gchar *default_username);

G_END_DECLS

#endif /* __PP_AUTHENTICATION_DIALOG_H__ */
