/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __NET_CONNECTION_EDITOR_H
#define __NET_CONNECTION_EDITOR_H

#include <glib-object.h>

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

#define NET_TYPE_CONNECTION_EDITOR         (net_connection_editor_get_type ())
#define NET_CONNECTION_EDITOR(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_CONNECTION_EDITOR, NetConnectionEditor))
#define NET_CONNECTION_EDITOR_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_CONNECTION_EDITOR, NetConnectionEditorClass))
#define NET_IS_CONNECTION_EDITOR(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_CONNECTION_EDITOR))
#define NET_IS_CONNECTION_EDITOR_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_CONNECTION_EDITOR))
#define NET_CONNECTION_EDITOR_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_CONNECTION_EDITOR, NetConnectionEditorClass))

typedef struct _NetConnectionEditor          NetConnectionEditor;
typedef struct _NetConnectionEditorClass     NetConnectionEditorClass;

struct _NetConnectionEditor
{
        GtkDialog parent;

        GtkWidget        *parent_window;
        NMClient         *client;
        NMDevice         *device;

        NMConnection     *connection;
        NMConnection     *orig_connection;
        gboolean          is_new_connection;
        gboolean          is_changed;
        NMAccessPoint    *ap;

        GtkButton        *cancel_button;
        GtkButton        *apply_button;
        GtkNotebook      *notebook;
        GtkFrame         *add_connection_frame;
        GtkNotebook      *toplevel_notebook;

        GSList *initializing_pages;
        GSList *pages;

        guint                    permission_id;
        NMClientPermissionResult can_modify;

        gboolean          title_set;
        gboolean          show_when_initialized;
};

struct _NetConnectionEditorClass
{
        GtkDialogClass parent_class;

        void (*done) (NetConnectionEditor *details, gboolean success);
};

GType                net_connection_editor_get_type (void);
NetConnectionEditor *net_connection_editor_new      (GtkWindow        *parent_window,
                                                     NMConnection     *connection,
                                                     NMDevice         *device,
                                                     NMAccessPoint    *ap,
                                                     NMClient         *client);
void                 net_connection_editor_set_title (NetConnectionEditor  *editor,
                                                      const gchar          *title);
void                 net_connection_editor_run      (NetConnectionEditor   *editor);
void                 net_connection_editor_forget   (NetConnectionEditor   *editor);
void                 net_connection_editor_reset    (NetConnectionEditor   *editor);

G_END_DECLS

#endif /* __NET_CONNECTION_EDITOR_H */

