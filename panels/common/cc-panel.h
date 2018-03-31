/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2010 Intel, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: William Jon McCann <jmccann@redhat.com>
 *          Thomas Wood <thomas.wood@intel.com>
 */


#ifndef __CC_PANEL_H
#define __CC_PANEL_H

#include <glib-object.h>
#include <gtk/gtk.h>


G_BEGIN_DECLS

#define CC_TYPE_PANEL         (cc_panel_get_type ())
#define CC_PANEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_PANEL, CcPanel))
#define CC_PANEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_PANEL, CcPanelClass))
#define CC_IS_PANEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_PANEL))
#define CC_IS_PANEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_PANEL))
#define CC_PANEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_PANEL, CcPanelClass))

/*•
 * Utility macro used to register panels
 *
 * use: CC_PANEL_REGISTER (PluginName, plugin_name)
 */
#define CC_PANEL_REGISTER(PluginName, plugin_name)                      \
  G_DEFINE_TYPE (PluginName, plugin_name, CC_TYPE_PANEL)

typedef struct CcPanelPrivate CcPanelPrivate;

typedef struct _CcPanel       CcPanel;
typedef struct _CcPanelClass  CcPanelClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CcPanel, g_object_unref)

/* cc-shell.h requires CcPanel, so make sure it is defined first */
#include "cc-shell.h"

/**
 * CcPanel:
 *
 * The contents of this struct are private and should not be accessed directly.
 */
struct _CcPanel
{
  /*< private >*/
  GtkBin          parent;
  CcPanelPrivate *priv;
};
/**
 * CcPanelClass:
 *
 * The contents of this struct are private and should not be accessed directly.
 */
struct _CcPanelClass
{
  /*< private >*/
  GtkBinClass parent_class;

  GPermission * (* get_permission) (CcPanel *panel);
  const char  * (* get_help_uri)   (CcPanel *panel);

  GtkWidget *   (* get_title_widget) (CcPanel *panel);
};

GType        cc_panel_get_type         (void);

CcShell*     cc_panel_get_shell        (CcPanel     *panel);

GPermission *cc_panel_get_permission   (CcPanel     *panel);

const char  *cc_panel_get_help_uri     (CcPanel     *panel);

GtkWidget   *cc_panel_get_title_widget (CcPanel     *panel);

G_END_DECLS

#endif /* __CC_PANEL_H */
