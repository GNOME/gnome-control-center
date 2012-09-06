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

#ifndef _CC_EDITABLE_ENTRY_H_
#define _CC_EDITABLE_ENTRY_H_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_EDITABLE_ENTRY  cc_editable_entry_get_type()

#define CC_EDITABLE_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_EDITABLE_ENTRY, CcEditableEntry))
#define CC_EDITABLE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_EDITABLE_ENTRY, CcEditableEntryClass))
#define CC_IS_EDITABLE_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_EDITABLE_ENTRY))
#define CC_IS_EDITABLE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_EDITABLE_ENTRY))
#define CC_EDITABLE_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_EDITABLE_ENTRY, CcEditableEntryClass))

typedef struct _CcEditableEntry CcEditableEntry;
typedef struct _CcEditableEntryClass CcEditableEntryClass;
typedef struct _CcEditableEntryPrivate CcEditableEntryPrivate;

struct _CcEditableEntry
{
  GtkAlignment parent;

  CcEditableEntryPrivate *priv;
};

struct _CcEditableEntryClass
{
  GtkAlignmentClass parent_class;

  void (* editing_done) (CcEditableEntry *entry);
};

GType        cc_editable_entry_get_type       (void) G_GNUC_CONST;
GtkWidget   *cc_editable_entry_new            (void);
void         cc_editable_entry_set_text       (CcEditableEntry *entry,
                                               const gchar     *text);
const gchar *cc_editable_entry_get_text       (CcEditableEntry *entry);
void         cc_editable_entry_set_editable   (CcEditableEntry *entry,
                                               gboolean         editable);
gboolean     cc_editable_entry_get_editable   (CcEditableEntry *entry);
void         cc_editable_entry_set_selectable (CcEditableEntry *entry,
                                               gboolean         selectable);
gboolean     cc_editable_entry_get_selectable (CcEditableEntry *entry);
void         cc_editable_entry_set_weight     (CcEditableEntry *entry,
                                               gint             weight);
gint         cc_editable_entry_get_weight     (CcEditableEntry *entry);
void         cc_editable_entry_set_scale      (CcEditableEntry *entry,
                                               gdouble          scale);
gdouble      cc_editable_entry_get_scale      (CcEditableEntry *entry);

G_END_DECLS

#endif /* _CC_EDITABLE_ENTRY_H_ */
