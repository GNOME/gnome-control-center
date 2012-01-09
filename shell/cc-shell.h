/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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

#ifndef _CC_SHELL_H
#define _CC_SHELL_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_SHELL cc_shell_get_type()

#define CC_SHELL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SHELL, CcShell))

#define CC_SHELL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_SHELL, CcShellClass))

#define CC_IS_SHELL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_SHELL))

#define CC_IS_SHELL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_SHELL))

#define CC_SHELL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_SHELL, CcShellClass))


#define CC_SHELL_PANEL_EXTENSION_POINT "control-center-1"

typedef struct _CcShell CcShell;
typedef struct _CcShellClass CcShellClass;
typedef struct _CcShellPrivate CcShellPrivate;

/* cc-panel.h requires CcShell, so make sure they are defined first */
#include "cc-panel.h"

/**
 * CcShell:
 *
 * The contents of this struct are private should not be accessed directly.
 */
struct _CcShell
{
  /*< private >*/
  GObject parent;

  CcShellPrivate *priv;
};

/**
 * CcShellClass:
 * @set_active_panel_from_id: virtual function to set the active panel from an
 *                            id string
 *
 */
struct _CcShellClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* vfuncs */
  gboolean    (*set_active_panel_from_id) (CcShell      *shell,
                                           const gchar  *id,
                                           const gchar **argv,
                                           GError      **error);
  GtkWidget * (*get_toplevel)             (CcShell      *shell);
  void        (*embed_widget_in_header)   (CcShell      *shell,
                                           GtkWidget    *widget);
};

GType           cc_shell_get_type                 (void) G_GNUC_CONST;

CcPanel*        cc_shell_get_active_panel         (CcShell      *shell);
void            cc_shell_set_active_panel         (CcShell      *shell,
                                                   CcPanel      *panel);
gboolean        cc_shell_set_active_panel_from_id (CcShell      *shell,
                                                   const gchar  *id,
                                                   const gchar **argv,
                                                   GError      **error);
GtkWidget *     cc_shell_get_toplevel             (CcShell      *shell);

void            cc_shell_embed_widget_in_header   (CcShell      *shell,
                                                   GtkWidget    *widget);

G_END_DECLS

#endif /* _CC_SHELL_H */
