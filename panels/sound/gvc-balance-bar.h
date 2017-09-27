/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
 */

#ifndef __GVC_BALANCE_BAR_H
#define __GVC_BALANCE_BAR_H

#include <glib-object.h>

#include "gvc-channel-map.h"

G_BEGIN_DECLS

#define GVC_TYPE_BALANCE_BAR (gvc_balance_bar_get_type ())
G_DECLARE_FINAL_TYPE (GvcBalanceBar, gvc_balance_bar, GVC, BALANCE_BAR, GtkBox)

typedef enum {
        BALANCE_TYPE_RL,
        BALANCE_TYPE_FR,
        BALANCE_TYPE_LFE,
} GvcBalanceType;

#define NUM_BALANCE_TYPES BALANCE_TYPE_LFE + 1

GtkWidget *         gvc_balance_bar_new                 (const GvcChannelMap *map,
                                                         GvcBalanceType btype);

void                gvc_balance_bar_set_size_group      (GvcBalanceBar *bar,
                                                         GtkSizeGroup  *group,
                                                         gboolean       symmetric);

G_END_DECLS

#endif /* __GVC_BALANCE_BAR_H */
