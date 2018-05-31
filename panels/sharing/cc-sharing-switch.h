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

#define CC_TYPE_SHARING_SWITCH (cc_sharing_switch_get_type ())
G_DECLARE_FINAL_TYPE (CcSharingSwitch, cc_sharing_switch, CC, SHARING_SWITCH, GtkSwitch)

GtkWidget    * cc_sharing_switch_new       (GtkWidget *widget);

G_END_DECLS

#endif /* __CC_SHARING_SWITCH_H__ */
