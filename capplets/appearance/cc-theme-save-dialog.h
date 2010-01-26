/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 William Jon McCann
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
 */

#ifndef __CC_THEME_SAVE_DIALOG_H
#define __CC_THEME_SAVE_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gnome-theme-info.h"

G_BEGIN_DECLS

#define CC_TYPE_THEME_SAVE_DIALOG         (cc_theme_save_dialog_get_type ())
#define CC_THEME_SAVE_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_THEME_SAVE_DIALOG, CcThemeSaveDialog))
#define CC_THEME_SAVE_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_THEME_SAVE_DIALOG, CcThemeSaveDialogClass))
#define CC_IS_THEME_SAVE_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_THEME_SAVE_DIALOG))
#define CC_IS_THEME_SAVE_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_THEME_SAVE_DIALOG))
#define CC_THEME_SAVE_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_THEME_SAVE_DIALOG, CcThemeSaveDialogClass))

typedef struct CcThemeSaveDialogPrivate CcThemeSaveDialogPrivate;

typedef struct
{
        GtkDialog                 parent;
        CcThemeSaveDialogPrivate *priv;
} CcThemeSaveDialog;

typedef struct
{
        GtkDialogClass   parent_class;
} CcThemeSaveDialogClass;

#define CC_THEME_SAVE_DIALOG_ERROR (cc_theme_save_dialog_error_quark ())

GType                  cc_theme_save_dialog_get_type           (void);
GQuark                 cc_theme_save_dialog_error_quark        (void);

GtkWidget            * cc_theme_save_dialog_new                (void);
void                   cc_theme_save_dialog_set_theme_info     (CcThemeSaveDialog  *dialog,
                                                                GnomeThemeMetaInfo *info);

G_END_DECLS

#endif /* __CC_THEME_SAVE_DIALOG_H */
