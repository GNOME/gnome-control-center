/* -*- mode: c; style: linux -*- */

/* selection-dialog.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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

#ifndef __SELECTION_DIALOG_H
#define __SELECTION_DIALOG_H

#include <gnome.h>
#include <libxml/tree.h>

#include "prefs-widget.h"

#define SELECTION_DIALOG(obj)          GTK_CHECK_CAST (obj, selection_dialog_get_type (), SelectionDialog)
#define SELECTION_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, selection_dialog_get_type (), SelectionDialogClass)
#define IS_SELECTION_DIALOG(obj)       GTK_CHECK_TYPE (obj, selection_dialog_get_type ())

typedef struct _SelectionDialog SelectionDialog;
typedef struct _SelectionDialogClass SelectionDialogClass;

struct _SelectionDialog 
{
	GnomeDialog gnome_dialog;

	GtkList *program_list;

	GtkListItem *selected_program_item;
	char *selected_name;
};

struct _SelectionDialogClass 
{
	GnomeDialogClass gnome_dialog_class;

	void (*ok_clicked) (SelectionDialog *, Screensaver *);
};

guint selection_dialog_get_type (void);

GtkWidget *selection_dialog_new (PrefsWidget *prefs_widget);

#endif /* __SELECTION_DIALOG_H */
