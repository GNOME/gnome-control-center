/*
 * Copyright (C) 2013 Intel, Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#ifndef __CC_SHARING_PANEL_H__
#define __CC_SHARING_PANEL_H__

#include <cc-shell.h>

G_BEGIN_DECLS

#define CC_TYPE_SHARING_PANEL cc_sharing_panel_get_type()

#define CC_SHARING_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SHARING_PANEL, CcSharingPanel))

#define CC_SHARING_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_SHARING_PANEL, CcSharingPanelClass))

#define CC_SHARING_IS_PANEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_SHARING_PANEL))

#define CC_SHARING_IS_PANEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_SHARING_PANEL))

#define CC_SHARING_PANEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_SHARING_PANEL, CcSharingPanelClass))

typedef struct _CcSharingPanel CcSharingPanel;
typedef struct _CcSharingPanelClass CcSharingPanelClass;
typedef struct _CcSharingPanelPrivate CcSharingPanelPrivate;

struct _CcSharingPanel
{
  CcPanel parent;

  CcSharingPanelPrivate *priv;
};

struct _CcSharingPanelClass
{
  CcPanelClass parent_class;
};

GType cc_sharing_panel_get_type (void) G_GNUC_CONST;

CcSharingPanel *cc_sharing_panel_new (void);

G_END_DECLS

#endif /* __CC_SHARING_PANEL_H__ */
