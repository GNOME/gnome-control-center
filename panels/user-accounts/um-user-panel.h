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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef _UM_USER_PANEL_H
#define _UM_USER_PANEL_H

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define UM_TYPE_USER_PANEL um_user_panel_get_type()

#define UM_USER_PANEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_USER_PANEL, UmUserPanel))
#define UM_USER_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_USER_PANEL, UmUserPanelClass))
#define UM_IS_USER_PANEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_USER_PANEL))
#define UM_IS_USER_PANEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_USER_PANEL))
#define UM_USER_PANEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_USER_PANEL, UmUserPanelClass))

typedef struct _UmUserPanel UmUserPanel;
typedef struct _UmUserPanelClass UmUserPanelClass;
typedef struct _UmUserPanelPrivate UmUserPanelPrivate;

struct _UmUserPanel
{
  CcPanel parent;

  UmUserPanelPrivate *priv;
};

struct _UmUserPanelClass
{
  CcPanelClass parent_class;
};

GType um_user_panel_get_type (void) G_GNUC_CONST;

void  um_user_panel_register (GIOModule *module);

G_END_DECLS

#endif /* _UM_USER_PANEL_H */
