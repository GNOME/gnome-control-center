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

#ifndef __UM_ALARM_H__
#define __UM_ALARM_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define UM_TYPE_ALARM             (um_alarm_get_type ())
#define UM_ALARM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_ALARM, UmAlarm))
#define UM_ALARM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_ALARM, UmAlarmClass))
#define UM_IS_ALARM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_ALARM))
#define UM_IS_ALARM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_ALARM))
#define UM_ALARM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), UM_TYPE_ALARM, UmAlarmClass))

typedef struct _UmAlarm        UmAlarm;
typedef struct _UmAlarmClass   UmAlarmClass;
typedef struct _UmAlarmPrivate UmAlarmPrivate;

struct _UmAlarm
{
        GObject parent;

        UmAlarmPrivate *priv;
};

struct _UmAlarmClass
{
        GObjectClass parent_class;

        void     (* fired)       (UmAlarm *alarm);
        void     (* rearmed)     (UmAlarm *alarm);
};

GType         um_alarm_get_type (void);

UmAlarm      *um_alarm_new    (void);
void          um_alarm_set    (UmAlarm      *alarm,
                               GDateTime    *time,
                               GCancellable *cancellable);
G_END_DECLS

#endif /* __UM_ALARM_H__ */
