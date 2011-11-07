/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Sergey Udaltsov <svu@gnome.org>
 *
 */


#ifndef _CC_REGION_PANEL_H
#define _CC_REGION_PANEL_H

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_REGION_PANEL cc_region_panel_get_type()

#define CC_REGION_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_REGION_PANEL, CcRegionPanel))

#define CC_REGION_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_REGION_PANEL, CcRegionPanelClass))

#define CC_IS_REGION_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_REGION_PANEL))

#define CC_IS_REGION_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_REGION_PANEL))

#define CC_REGION_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_REGION_PANEL, CcRegionPanelClass))

typedef struct _CcRegionPanel CcRegionPanel;
typedef struct _CcRegionPanelClass CcRegionPanelClass;
typedef struct _CcRegionPanelPrivate CcRegionPanelPrivate;

struct _CcRegionPanel
{
  CcPanel parent;

  CcRegionPanelPrivate *priv;
};

struct _CcRegionPanelClass
{
  CcPanelClass parent_class;
};

GType cc_region_panel_get_type (void) G_GNUC_CONST;

void  cc_region_panel_register (GIOModule *module);

G_END_DECLS

#endif /* _CC_REGION_PANEL_H */
