/* -*- mode: c; style: linux -*- */

/* config-manager-dialog.h
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef __CONFIG_MANAGER_DIALOG_H
#define __CONFIG_MANAGER_DIALOG_H

#include <gnome.h>

BEGIN_GNOME_DECLS

#define CONFIG_MANAGER_DIALOG(obj)          GTK_CHECK_CAST (obj, config_manager_dialog_get_type (), ConfigManagerDialog)
#define CONFIG_MANAGER_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, config_manager_dialog_get_type (), ConfigManagerDialogClass)
#define IS_CONFIG_MANAGER_DIALOG(obj)       GTK_CHECK_TYPE (obj, config_manager_dialog_get_type ())

typedef struct _ConfigManagerDialog ConfigManagerDialog;
typedef struct _ConfigManagerDialogClass ConfigManagerDialogClass;
typedef struct _ConfigManagerDialogPrivate ConfigManagerDialogPrivate;

typedef enum _CMDialogType CMDialogType;

struct _ConfigManagerDialog 
{
	GnomeDialog parent;

	ConfigManagerDialogPrivate *p;
};

struct _ConfigManagerDialogClass 
{
	GnomeDialogClass gnome_dialog_class;
};

enum _CMDialogType {
	CM_DIALOG_USER_ONLY, CM_DIALOG_GLOBAL_ONLY, CM_DIALOG_BOTH
};

guint config_manager_dialog_get_type         (void);

GtkWidget *config_manager_dialog_new         (CMDialogType type);

END_GNOME_DECLS

#endif /* __CONFIG_MANAGER_DIALOG_H */
