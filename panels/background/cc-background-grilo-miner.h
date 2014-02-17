/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 */


#ifndef _CC_BACKGROUND_GRILO_MINER_H
#define _CC_BACKGROUND_GRILO_MINER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_GRILO_MINER cc_background_grilo_miner_get_type()

#define CC_BACKGROUND_GRILO_MINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_BACKGROUND_GRILO_MINER, CcBackgroundGriloMiner))

#define CC_BACKGROUND_GRILO_MINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_BACKGROUND_GRILO_MINER, CcBackgroundGriloMinerClass))

#define CC_IS_BACKGROUND_GRILO_MINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_BACKGROUND_GRILO_MINER))

#define CC_IS_BACKGROUND_GRILO_MINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_BACKGROUND_GRILO_MINER))

#define CC_BACKGROUND_GRILO_MINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_BACKGROUND_GRILO_MINER, CcBackgroundGriloMinerClass))

typedef struct _CcBackgroundGriloMiner CcBackgroundGriloMiner;
typedef struct _CcBackgroundGriloMinerClass CcBackgroundGriloMinerClass;

GType                     cc_background_grilo_miner_get_type       (void) G_GNUC_CONST;

CcBackgroundGriloMiner   *cc_background_grilo_miner_new            (void);

void                      cc_background_grilo_miner_start          (CcBackgroundGriloMiner *self);

G_END_DECLS

#endif /* _CC_BACKGROUND_GRILO_MINER_H */
