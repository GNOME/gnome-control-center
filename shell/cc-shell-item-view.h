/*
 * Copyright (c) 2010 Intel, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#ifndef _CC_SHELL_ITEM_VIEW_H
#define _CC_SHELL_ITEM_VIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SHELL_ITEM_VIEW cc_shell_item_view_get_type()

#define CC_SHELL_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SHELL_ITEM_VIEW, CcShellItemView))

#define CC_SHELL_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_SHELL_ITEM_VIEW, CcShellItemViewClass))

#define CC_IS_SHELL_ITEM_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_SHELL_ITEM_VIEW))

#define CC_IS_SHELL_ITEM_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_SHELL_ITEM_VIEW))

#define CC_SHELL_ITEM_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_SHELL_ITEM_VIEW, CcShellItemViewClass))

typedef struct _CcShellItemView CcShellItemView;
typedef struct _CcShellItemViewClass CcShellItemViewClass;
typedef struct _CcShellItemViewPrivate CcShellItemViewPrivate;

struct _CcShellItemView
{
  GtkIconView parent;

  CcShellItemViewPrivate *priv;
};

struct _CcShellItemViewClass
{
  GtkIconViewClass parent_class;
};

GType cc_shell_item_view_get_type (void) G_GNUC_CONST;

GtkWidget *cc_shell_item_view_new (void);

void cc_shell_item_view_update_cells (CcShellItemView *view);

G_END_DECLS

#endif /* _CC_SHELL_ITEM_VIEW_H */
