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

#ifndef _CC_SHELL_SEARCH_RENDERER_H
#define _CC_SHELL_SEARCH_RENDERER_H

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SHELL_TYPE_SEARCH_RENDERER cc_shell_search_renderer_get_type()

#define CC_SHELL_SEARCH_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  SHELL_TYPE_SEARCH_RENDERER, CcShellSearchRenderer))

#define CC_SHELL_SEARCH_RENDERER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  SHELL_TYPE_SEARCH_RENDERER, CcShellSearchRendererClass))

#define SHELL_IS_SEARCH_RENDERER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  SHELL_TYPE_SEARCH_RENDERER))

#define SHELL_IS_SEARCH_RENDERER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  SHELL_TYPE_SEARCH_RENDERER))

#define CC_SHELL_SEARCH_RENDERER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  SHELL_TYPE_SEARCH_RENDERER, CcShellSearchRendererClass))

typedef struct _CcShellSearchRenderer CcShellSearchRenderer;
typedef struct _CcShellSearchRendererClass CcShellSearchRendererClass;
typedef struct _CcShellSearchRendererPrivate CcShellSearchRendererPrivate;

struct _CcShellSearchRenderer
{
  GtkCellRendererText parent;

  CcShellSearchRendererPrivate *priv;
};

struct _CcShellSearchRendererClass
{
  GtkCellRendererTextClass parent_class;
};

GType cc_shell_search_renderer_get_type (void) G_GNUC_CONST;

CcShellSearchRenderer *cc_shell_search_renderer_new (void);

G_END_DECLS

#endif /* _CC_SHELL_SEARCH_RENDERER_H */
