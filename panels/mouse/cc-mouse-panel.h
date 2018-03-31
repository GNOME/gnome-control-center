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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */


#ifndef _CC_MOUSE_PANEL_H
#define _CC_MOUSE_PANEL_H

#include <cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_MOUSE_PANEL cc_mouse_panel_get_type()

#define CC_MOUSE_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_MOUSE_PANEL, CcMousePanel))

#define CC_MOUSE_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_MOUSE_PANEL, CcMousePanelClass))

#define CC_IS_MOUSE_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_MOUSE_PANEL))

#define CC_IS_MOUSE_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_MOUSE_PANEL))

#define CC_MOUSE_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_MOUSE_PANEL, CcMousePanelClass))

typedef struct _CcMousePanel CcMousePanel;
typedef struct _CcMousePanelClass CcMousePanelClass;
typedef struct _CcMousePanelPrivate CcMousePanelPrivate;

struct _CcMousePanel
{
  CcPanel parent;

  CcMousePanelPrivate *priv;
};

struct _CcMousePanelClass
{
  CcPanelClass parent_class;
};

GType cc_mouse_panel_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _CC_MOUSE_PANEL_H */
