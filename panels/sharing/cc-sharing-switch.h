/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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
 */

#ifndef __CC_SHARING_SWITCH_H__
#define __CC_SHARING_SWITCH_H__

#include <gtk/gtkswitch.h>

G_BEGIN_DECLS

#define CC_TYPE_SHARING_SWITCH             (cc_sharing_switch_get_type ())
#define CC_SHARING_SWITCH(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_SHARING_SWITCH, CcSharingSwitch))
#define CC_SHARING_SWITCH_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_SHARING_SWITCH, CcSharingSwitchClass))
#define CC_IS_SHARING_SWITCH(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_SHARING_SWITCH))
#define CC_IS_SHARING_SWITCH_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_SHARING_SWITCH))
#define CC_SHARING_SWITCH_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_SHARING_SWITCH, CcSharingSwitchClass))

typedef struct _CcSharingSwitch        CcSharingSwitch;
typedef struct _CcSharingSwitchPrivate CcSharingSwitchPrivate;
typedef struct _CcSharingSwitchClass   CcSharingSwitchClass;

struct _CcSharingSwitch
{
  GtkSwitch parent_instance;

  CcSharingSwitchPrivate *priv;
};

struct _CcSharingSwitchClass
{
  GtkSwitchClass parent_class;
};

GType          cc_sharing_switch_get_type  (void) G_GNUC_CONST;
GtkWidget    * cc_sharing_switch_new       (GtkWidget *widget);

G_END_DECLS

#endif /* __CC_SHARING_SWITCH_H__ */
