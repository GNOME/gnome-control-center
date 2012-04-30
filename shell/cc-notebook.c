/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *      Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-notebook.h"

#define GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CC_TYPE_NOTEBOOK, CcNotebookPrivate))

struct _CcNotebookPrivate
{
        ClutterActor *stage;
        ClutterActor *scroll_actor;
        ClutterActor *bin;
        GList *children; /* GList of ClutterActor */
};

G_DEFINE_TYPE (CcNotebook, cc_notebook, GTK_CLUTTER_TYPE_EMBED);

static void
cc_notebook_add (GtkContainer *container,
		 GtkWidget    *widget)
{
	CcNotebook *notebook;
	ClutterActor *child;

	notebook = CC_NOTEBOOK (container);
	child = gtk_clutter_actor_new_with_contents (widget);
	clutter_actor_add_child (notebook->priv->bin, child);

	notebook->priv->children = g_list_prepend (notebook->priv->children,
						   child);
}

static int
_cc_notebook_find_widget (GtkClutterActor *actor,
			  GtkWidget       *widget)
{
	if (gtk_clutter_actor_get_contents (actor) == widget)
		return 0;
	return -1;
}

static void
cc_notebook_remove (GtkContainer *container,
		    GtkWidget    *widget)
{
	CcNotebook *notebook;
	GList *l;

	notebook = CC_NOTEBOOK (container);

	l = g_list_find_custom (notebook->priv->children,
				widget,
				(GCompareFunc) _cc_notebook_find_widget);
	if (!l)
		return;
	clutter_actor_remove_child (notebook->priv->bin, l->data);
	notebook->priv->children = g_list_remove (notebook->priv->children, l->data);
}

static GType
cc_notebook_child_type (GtkContainer *container)
{
	return GTK_TYPE_WIDGET;
}

static void
cc_notebook_forall (GtkContainer *container,
		    gboolean      include_internals,
		    GtkCallback   callback,
		    gpointer      callback_data)
{
#if 0
	CcNotebook *notebook;
	GList *children;
	ClutterActor *child;

	notebook = CC_NOTEBOOK (container);

	children = notebook->priv->children;
	while (children) {
		child = children->data;
		children = children->next;

		(* callback) (gtk_clutter_actor_get_contents (GTK_CLUTTER_ACTOR (child)), callback_data);
	}
#endif
}

static void
cc_notebook_class_init (CcNotebookClass *class)
{
        GObjectClass *gobject_class;
        GtkWidgetClass *widget_class;
        GtkContainerClass *container_class;

        gobject_class = (GObjectClass*)class;
        widget_class = (GtkWidgetClass*)class;
        container_class = (GtkContainerClass*)class;

        container_class->add = cc_notebook_add;
        container_class->remove = cc_notebook_remove;
        container_class->child_type = cc_notebook_child_type;
        container_class->forall = cc_notebook_forall;

        g_type_class_add_private (class, sizeof (CcNotebookPrivate));
}

static void
cc_notebook_init (CcNotebook *notebook)
{
	CcNotebookPrivate *priv;
	ClutterConstraint *constraint;

	priv = GET_PRIVATE (notebook);
	notebook->priv = priv;

	priv->stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (notebook));

	priv->scroll_actor = clutter_scroll_actor_new ();
	clutter_actor_add_child (priv->stage, priv->scroll_actor);
	clutter_scroll_actor_set_scroll_mode (CLUTTER_SCROLL_ACTOR (priv->scroll_actor),
					      CLUTTER_SCROLL_HORIZONTALLY);
//	constraint = clutter_bind_constraint_new (priv->stage, CLUTTER_BIND_SIZE, 0.0);
//	clutter_actor_add_constraint_with_name (priv->scroll_actor, "size", constraint);

	priv->bin = clutter_actor_new ();
	clutter_actor_set_layout_manager (priv->bin, clutter_box_layout_new ());
	clutter_actor_add_child (priv->scroll_actor, priv->bin);
//	constraint = clutter_bind_constraint_new (priv->scroll_actor, CLUTTER_BIND_SIZE, 0.0);
//	clutter_actor_add_constraint_with_name (priv->bin, "size", constraint);
}

GtkWidget *
cc_notebook_new (void)
{
        return (GtkWidget*) g_object_new (CC_TYPE_NOTEBOOK, NULL);
}

void
cc_notebook_set_current (CcNotebook *notebook,
			 GtkWidget  *widget)
{
	ClutterPoint pos;
	GList *l;

	g_return_if_fail (CC_IS_NOTEBOOK (notebook));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	l = g_list_find_custom (notebook->priv->children,
				widget,
				(GCompareFunc) _cc_notebook_find_widget);
	g_return_if_fail (l != NULL);

	g_message ("setting %p as the current widget", l->data);

	clutter_actor_get_position (l->data, &pos.x, &pos.y);

	clutter_actor_save_easing_state (notebook->priv->scroll_actor);
	clutter_actor_set_easing_duration (notebook->priv->scroll_actor, 500);
	clutter_actor_set_easing_mode (notebook->priv->scroll_actor, CLUTTER_EASE_OUT_BOUNCE);

	clutter_scroll_actor_scroll_to_point (CLUTTER_SCROLL_ACTOR (notebook->priv->scroll_actor), &pos);

	clutter_actor_restore_easing_state (notebook->priv->scroll_actor);
}

GtkWidget *
cc_notebook_get_current (CcNotebook *notebook)
{
	g_return_val_if_fail (CC_IS_NOTEBOOK (notebook), NULL);

	return NULL;
}
