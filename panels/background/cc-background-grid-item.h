/* cc-background-grid-item.h
 *
 * Copyright (C) 2017 Julian Sparber <julian@sparber.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CC_BACKGROUND_GRID_ITEM_H
#define CC_BACKGROUND_GRID_ITEM_H

#include <gtk/gtk.h>
#include "cc-background-item.h"

G_BEGIN_DECLS

#define CC_TYPE_BACKGROUND_GRID_ITEM (cc_background_grid_item_get_type())

G_DECLARE_FINAL_TYPE (CcBackgroundGridItem, cc_background_grid_item, CC, BACKGROUND_GRID_LIST, GtkFlowBoxChild)

GtkWidget *             cc_background_grid_item_new             (CcBackgroundItem             *);
void                    cc_background_grid_item_set_ref         (GtkWidget                    *,
                                                                 CcBackgroundItem             *);
CcBackgroundItem *      cc_background_grid_item_get_ref         (GtkWidget                    *);

G_END_DECLS

#endif /* CC_BACKGROUND_GRID_ITEM_H */
