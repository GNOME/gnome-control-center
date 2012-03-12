/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Debarshi Ray <debarshir@gnome.org>
 */

#ifndef __GOA_PANEL_ADD_ACCOUNT_DIALOG_H__
#define __GOA_PANEL_ADD_ACCOUNT_DIALOG_H__

#include <goa/goa.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG            (goa_panel_add_account_dialog_get_type ())
#define GOA_PANEL_ADD_ACCOUNT_DIALOG(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG, GoaPanelAddAccountDialog))
#define GOA_PANEL_ADD_ACCOUNT_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG, GoaPanelAddAccountDialogClass))
#define GOA_IS_PANEL_ADD_ACCOUNT_DIALOG(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG))
#define GOA_IS_PANEL_ADD_ACCOUNT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG))
#define GOA_PANEL_ADD_ACCOUNT_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GOA_TYPE_PANEL_ADD_ACCOUNT_DIALOG, GoaPanelAddAccountDialogClass))

typedef struct _GoaPanelAddAccountDialog        GoaPanelAddAccountDialog;
typedef struct _GoaPanelAddAccountDialogClass   GoaPanelAddAccountDialogClass;
typedef struct _GoaPanelAddAccountDialogPrivate GoaPanelAddAccountDialogPrivate;

struct _GoaPanelAddAccountDialog
{
  GtkDialog parent_instance;
  GoaPanelAddAccountDialogPrivate *priv;
};

struct _GoaPanelAddAccountDialogClass
{
  GtkDialogClass parent_class;
};

GType                  goa_panel_add_account_dialog_get_type               (void) G_GNUC_CONST;
GtkWidget             *goa_panel_add_account_dialog_new                    (GoaClient *client);
void                   goa_panel_add_account_dialog_add_provider           (GoaPanelAddAccountDialog *add_account,
                                                                            GoaProvider              *provider);
GoaObject             *goa_panel_add_account_dialog_get_account            (GoaPanelAddAccountDialog *add_account,
                                                                            GError                   **error);

G_END_DECLS

#endif /* __GOA_PANEL_ADD_ACCOUNT_DIALOG_H__ */
