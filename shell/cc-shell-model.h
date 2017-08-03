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


#ifndef _CC_SHELL_MODEL_H
#define _CC_SHELL_MODEL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SHELL_MODEL cc_shell_model_get_type()

#define CC_SHELL_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SHELL_MODEL, CcShellModel))

#define CC_SHELL_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_SHELL_MODEL, CcShellModelClass))

#define CC_IS_SHELL_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_SHELL_MODEL))

#define CC_IS_SHELL_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_SHELL_MODEL))

#define CC_SHELL_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_SHELL_MODEL, CcShellModelClass))

typedef struct _CcShellModel CcShellModel;
typedef struct _CcShellModelClass CcShellModelClass;
typedef struct _CcShellModelPrivate CcShellModelPrivate;

typedef enum {
#ifdef CC_ENABLE_ALT_CATEGORIES
  CC_CATEGORY_PERSONAL,
  CC_CATEGORY_SYSTEM,
  CC_CATEGORY_HARDWARE,
#else
  CC_CATEGORY_CONNECTIVITY,
  CC_CATEGORY_PERSONALIZATION,
  CC_CATEGORY_ACCOUNT,
  CC_CATEGORY_HARDWARE,
  CC_CATEGORY_DEVICES,
  CC_CATEGORY_DETAILS,
#endif

  CC_CATEGORY_HIDDEN,
  CC_CATEGORY_LAST
} CcPanelCategory;

enum
{
  COL_NAME,
  COL_CASEFOLDED_NAME,
  COL_APP,
  COL_ID,
  COL_CATEGORY,
  COL_DESCRIPTION,
  COL_CASEFOLDED_DESCRIPTION,
  COL_GICON,
  COL_KEYWORDS,

  N_COLS
};

struct _CcShellModel
{
  GtkListStore parent;

  CcShellModelPrivate *priv;
};

struct _CcShellModelClass
{
  GtkListStoreClass parent_class;
};

GType cc_shell_model_get_type (void) G_GNUC_CONST;

CcShellModel *cc_shell_model_new (void);

void cc_shell_model_add_item (CcShellModel   *model,
                              CcPanelCategory category,
                              GAppInfo       *appinfo,
                              const char     *id);

gboolean cc_shell_model_iter_matches_search (CcShellModel *model,
                                             GtkTreeIter  *iter,
                                             const char   *term);

void cc_shell_model_set_sort_terms (CcShellModel  *model,
                                    gchar        **terms);

G_END_DECLS

#endif /* _CC_SHELL_MODEL_H */
