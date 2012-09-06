/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
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
 * Written by: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef _UM_EDITABLE_COMBO_H
#define _UM_EDITABLE_COMBO_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UM_TYPE_EDITABLE_COMBO  um_editable_combo_get_type()

#define UM_EDITABLE_COMBO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_EDITABLE_COMBO, UmEditableCombo))
#define UM_EDITABLE_COMBO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_EDITABLE_COMBO, UmEditableComboClass))
#define UM_IS_EDITABLE_COMBO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_EDITABLE_COMBO))
#define UM_IS_EDITABLE_COMBO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), UM_TYPE_EDITABLE_COMBO))
#define UM_EDITABLE_COMBO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), UM_TYPE_EDITABLE_COMBO, UmEditableComboClass))

typedef struct _UmEditableCombo UmEditableCombo;
typedef struct _UmEditableComboClass UmEditableComboClass;
typedef struct _UmEditableComboPrivate UmEditableComboPrivate;

struct _UmEditableCombo
{
  GtkAlignment parent;

  UmEditableComboPrivate *priv;
};

struct _UmEditableComboClass
{
  GtkAlignmentClass parent_class;

  void (* editing_done) (UmEditableCombo *combo);
  void (* activate)     (UmEditableCombo *combo);
};

GType         um_editable_combo_get_type        (void) G_GNUC_CONST;
GtkWidget    *um_editable_combo_new             (void);
void          um_editable_combo_set_editable    (UmEditableCombo *combo,
                                                 gboolean         editable);
gboolean      um_editable_combo_get_editable    (UmEditableCombo *combo);
void          um_editable_combo_set_model       (UmEditableCombo *combo,
                                                 GtkTreeModel    *model);
GtkTreeModel *um_editable_combo_get_model       (UmEditableCombo *combo);
void          um_editable_combo_set_text_column (UmEditableCombo *combo,
                                                 gint             column);
gint          um_editable_combo_get_text_column (UmEditableCombo *combo);
gint          um_editable_combo_get_active      (UmEditableCombo *combo);
void          um_editable_combo_set_active      (UmEditableCombo *combo,
                                                 gint             active);
gboolean      um_editable_combo_get_active_iter (UmEditableCombo *combo,
                                                 GtkTreeIter     *iter);
void          um_editable_combo_set_active_iter (UmEditableCombo *combo,
                                                 GtkTreeIter     *iter);

G_END_DECLS

#endif /* _UM_EDITABLE_COMBO_H_ */
