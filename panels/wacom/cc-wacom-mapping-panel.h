/*
 * Copyright © 2012 Wacom.
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
 * Authors: Jason Gerecke <killertofu@gmail.com>
 *
 */

#ifndef CC_WACOM_MAPPING_PANEL_H_
#define CC_WACOM_MAPPING_PANEL_H_

#include <gtk/gtk.h>
#include "cc-wacom-device.h"

G_BEGIN_DECLS

#define CC_TYPE_WACOM_MAPPING_PANEL cc_wacom_mapping_panel_get_type()

#define CC_WACOM_MAPPING_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_WACOM_MAPPING_PANEL, CcWacomMappingPanel))

#define CC_WACOM_MAPPING_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_WACOM_MAPPING_PANEL, CcWacomMappignPanelClass))

#define CC_IS_WACOM_MAPPING_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_WACOM_MAPPING_PANEL))

#define CC_IS_WACOM_MAPPING_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_WACOM_MAPPING_PANEL))

#define CC_WACOM_MAPPING_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_WACOM_MAPPING_PANEL, CcWacomMappingPanelClass))

typedef struct _CcWacomMappingPanel CcWacomMappingPanel;
typedef struct _CcWacomMappingPanelClass CcWacomMappingPanelClass;
typedef struct _CcWacomMappingPanelPrivate CcWacomMappingPanelPrivate;

struct _CcWacomMappingPanel
{
  GtkBox parent;

  CcWacomMappingPanelPrivate *priv;
};

struct _CcWacomMappingPanelClass
{
  GtkBoxClass parent_class;
};

GType cc_wacom_mapping_panel_get_type (void) G_GNUC_CONST;

GtkWidget * cc_wacom_mapping_panel_new (void);


void cc_wacom_mapping_panel_set_device (CcWacomMappingPanel *self,
                                        CcWacomDevice       *device);

G_END_DECLS

#endif /* CC_WACOM_MAPPING_PANEL_H_ */
