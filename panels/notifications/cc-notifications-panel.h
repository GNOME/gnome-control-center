/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _CC_NOTIFICATIONS_PANEL_H_
#define _CC_NOTIFICATIONS_PANEL_H_

#include <gio/gio.h>
#include <cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_NOTIFICATIONS_PANEL  (cc_notifications_panel_get_type ())
#define CC_NOTIFICATIONS_PANEL(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_NOTIFICATIONS_PANEL, CcNotificationsPanel))
#define GC_IS_NOTIFICATIONS_PANEL(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_NOTIFICATIONS_PANEL))

typedef struct _CcNotificationsPanel CcNotificationsPanel;
typedef struct _CcNotificationsPanelClass CcNotificationsPanelClass;

GType cc_notifications_panel_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CC_EDIT_DIALOG_H_ */
