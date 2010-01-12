/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 */

#ifndef __CC_BACKGROUNDS_MONITOR_H
#define __CC_BACKGROUNDS_MONITOR_H

#include <glib-object.h>
#include "cc-background-item.h"

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUNDS_MONITOR         (cc_backgrounds_monitor_get_type ())
#define CC_BACKGROUNDS_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_BACKGROUNDS_MONITOR, CcBackgroundsMonitor))
#define CC_BACKGROUNDS_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_BACKGROUNDS_MONITOR, CcBackgroundsMonitorClass))
#define CC_IS_BACKGROUNDS_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_BACKGROUNDS_MONITOR))
#define CC_IS_BACKGROUNDS_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_BACKGROUNDS_MONITOR))
#define CC_BACKGROUNDS_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_BACKGROUNDS_MONITOR, CcBackgroundsMonitorClass))

typedef struct CcBackgroundsMonitorPrivate CcBackgroundsMonitorPrivate;

typedef struct
{
        GObject                      parent;
        CcBackgroundsMonitorPrivate *priv;
} CcBackgroundsMonitor;

typedef struct
{
        GObjectClass   parent_class;

        void (* item_added)           (CcBackgroundsMonitor *monitor,
                                       CcBackgroundItem     *item);
        void (* item_removed)         (CcBackgroundsMonitor *monitor,
                                       CcBackgroundItem     *item);
} CcBackgroundsMonitorClass;

GType                  cc_backgrounds_monitor_get_type    (void);

CcBackgroundsMonitor * cc_backgrounds_monitor_new         (void);

void                   cc_backgrounds_monitor_load        (CcBackgroundsMonitor *monitor);
void                   cc_backgrounds_monitor_save        (CcBackgroundsMonitor *monitor);

GList *                cc_backgrounds_monitor_get_items   (CcBackgroundsMonitor *monitor);
gboolean               cc_backgrounds_monitor_add_item    (CcBackgroundsMonitor *monitor,
                                                           CcBackgroundItem     *item);
gboolean               cc_backgrounds_monitor_remove_item (CcBackgroundsMonitor *monitor,
                                                           CcBackgroundItem     *item);

G_END_DECLS

#endif /* __CC_BACKGROUNDS_MONITOR_H */
