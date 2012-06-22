/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors: Ray Strode
 */

#ifndef __GSD_ALARM_H__
#define __GSD_ALARM_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GSD_TYPE_ALARM             (gsd_alarm_get_type ())
#define GSD_ALARM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_ALARM, GsdAlarm))
#define GSD_ALARM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_ALARM, GsdAlarmClass))
#define GSD_IS_ALARM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_ALARM))
#define GSD_IS_ALARM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_TYPE_ALARM))
#define GSD_ALARM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GSD_TYPE_ALARM, GsdAlarmClass))

typedef struct _GsdAlarm        GsdAlarm;
typedef struct _GsdAlarmClass   GsdAlarmClass;
typedef struct _GsdAlarmPrivate GsdAlarmPrivate;

struct _GsdAlarm
{
        GObject parent;

        GsdAlarmPrivate *priv;
};

struct _GsdAlarmClass
{
        GObjectClass parent_class;

        void     (* fired)       (GsdAlarm *alarm);
        void     (* rearmed)     (GsdAlarm *alarm);
};

GType         gsd_alarm_get_type (void);

GsdAlarm     *gsd_alarm_new      (void);
void          gsd_alarm_set_time (GsdAlarm     *alarm,
                                  GDateTime    *time,
                                  GCancellable *cancellable);
GDateTime    *gsd_alarm_get_time (GsdAlarm     *alarm);
G_END_DECLS

#endif /* __GSD_ALARM_H__ */
