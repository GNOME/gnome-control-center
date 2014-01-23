/*
 * Copyright © 2013 Red Hat, Inc.
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
 * Author: Joaquim Rocha <jrocha@redhat.com>
 */

#ifndef __CC_TARGET_ACTOR_H__
#define __CC_TARGET_ACTOR_H__

#include <glib.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define CC_TARGET_ACTOR_TYPE            (cc_target_actor_get_type ())
#define CC_TARGET_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TARGET_ACTOR_TYPE, CcTargetActor))
#define CC_IS_TARGET_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TARGET_ACTOR_TYPE))
#define CC_TARGET_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TARGET_ACTOR_TYPE, CcTargetActorClass))
#define CC_IS_TARGET_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TARGET_ACTOR_TYPE))
#define CC_TARGET_ACTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TARGET_ACTOR_TYPE, CcTargetActorClass))

typedef struct _CcTargetActor        CcTargetActor;
typedef struct _CcTargetActorClass   CcTargetActorClass;
typedef struct _CcTargetActorPrivate CcTargetActorPrivate;

struct _CcTargetActor
{
  ClutterActor parent_instance;

  /*< private >*/
  CcTargetActorPrivate *priv;
};

struct _CcTargetActorClass
{
  ClutterActorClass parent_class;
};

ClutterActor * cc_target_actor_new         (void);
void           cc_target_actor_move_center (CcTargetActor *target,
                                            gdouble        x,
                                            gdouble        y);

GType          cc_target_actor_get_type    (void);

#endif /* __CC_TARGET_ACTOR_H__ */
