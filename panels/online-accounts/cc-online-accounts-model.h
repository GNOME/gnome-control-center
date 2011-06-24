/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#ifndef __GOA_PANEL_ACCOUNTS_MODEL_H__
#define __GOA_PANEL_ACCOUNTS_MODEL_H__

#include <goa/goa.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

struct _GoaPanelAccountsModel;
typedef struct _GoaPanelAccountsModel GoaPanelAccountsModel;

#define GOA_TYPE_PANEL_ACCOUNTS_MODEL  (goa_panel_accounts_model_get_type ())
#define GOA_PANEL_ACCOUNTS_MODEL(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), GOA_TYPE_PANEL_ACCOUNTS_MODEL, GoaPanelAccountsModel))
#define GOA_IS_PANEL_ACCOUNTS_MODEL(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GOA_TYPE_PANEL_ACCOUNTS_MODEL))

enum
{
  GOA_PANEL_ACCOUNTS_MODEL_COLUMN_SORT_KEY,
  GOA_PANEL_ACCOUNTS_MODEL_COLUMN_OBJECT,
  GOA_PANEL_ACCOUNTS_MODEL_COLUMN_ATTENTION_NEEDED,
  GOA_PANEL_ACCOUNTS_MODEL_COLUMN_MARKUP,
  GOA_PANEL_ACCOUNTS_MODEL_COLUMN_ICON,
  GOA_PANEL_ACCOUNTS_MODEL_N_COLUMNS
};

GType                   goa_panel_accounts_model_get_type            (void) G_GNUC_CONST;
GoaPanelAccountsModel  *goa_panel_accounts_model_new                 (GoaClient      *client);
GoaClient              *goa_panel_accounts_model_get_client          (GoaPanelAccountsModel *model);
gboolean                goa_panel_accounts_model_get_iter_for_object (GoaPanelAccountsModel  *model,
                                                                      GoaObject              *object,
                                                                      GtkTreeIter            *iter);


G_END_DECLS

#endif /* __GOA_PANEL_ACCOUNTS_MODEL_H__ */
