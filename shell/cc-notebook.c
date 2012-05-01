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
	GtkWidget *embed;
        ClutterActor *stage;
        ClutterActor *scroll_actor;
        ClutterActor *bin;
        GList *children; /* GList of ClutterActor */

        GtkWidget *current_page;
};

enum {
        PROP_0,
        PROP_CURRENT_PAGE
};

G_DEFINE_TYPE (CcNotebook, cc_notebook, GTK_TYPE_BOX);

void
cc_notebook_add (CcNotebook *notebook,
		 GtkWidget  *widget)
{
	ClutterConstraint *constraint;
	ClutterActor *child;
	ClutterLayoutManager *layout;

	child = gtk_clutter_actor_new_with_contents (widget);

	/* Make sure that the actor will be window sized */
	constraint = clutter_bind_constraint_new (notebook->priv->bin, CLUTTER_BIND_SIZE, 0.f);
	clutter_actor_add_constraint_with_name (child, "size", constraint);
	clutter_actor_add_child (notebook->priv->bin, child);

	notebook->priv->children = g_list_prepend (notebook->priv->children,
						   child);

	if (notebook->priv->current_page != NULL)
		return;

	cc_notebook_set_current (notebook, widget);
}

static int
_cc_notebook_find_widget (GtkClutterActor *actor,
			  GtkWidget       *widget)
{
	if (gtk_clutter_actor_get_contents (actor) == widget)
		return 0;
	return -1;
}

static ClutterActor *
_cc_notebook_actor_for_widget (CcNotebook *notebook,
			       GtkWidget  *widget)
{
	GList *l;

	l = g_list_find_custom (notebook->priv->children,
				widget,
				(GCompareFunc) _cc_notebook_find_widget);
	if (!l)
		return NULL;
	return l->data;
}

void
cc_notebook_remove (CcNotebook *notebook,
		    GtkWidget  *widget)
{
	ClutterActor *actor;

	actor = _cc_notebook_actor_for_widget (notebook, widget);
	if (actor == NULL)
		return;
	clutter_actor_remove_child (notebook->priv->bin, actor);
	notebook->priv->children = g_list_remove (notebook->priv->children, actor);
}

static void
cc_notebook_set_property (GObject      *object,
			  guint         prop_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
        switch (prop_id) {
        case PROP_CURRENT_PAGE:
                cc_notebook_set_current (CC_NOTEBOOK (object),
					      g_value_get_pointer (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_notebook_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
        switch (prop_id) {
        case PROP_CURRENT_PAGE:
                g_value_set_pointer (value, cc_notebook_get_current (CC_NOTEBOOK (object)));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GtkSizeRequestMode
cc_notebook_get_request_mode (GtkWidget *widget)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->current_page ? notebook->priv->current_page : GTK_WIDGET (notebook);

	return gtk_widget_get_request_mode (target);
}

static void
cc_notebook_get_preferred_height (GtkWidget       *widget,
				  gint            *minimum_height,
				  gint            *natural_height)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->current_page ? notebook->priv->current_page : GTK_WIDGET (notebook);

	gtk_widget_get_preferred_height (target, minimum_height, natural_height);
}

static void
cc_notebook_get_preferred_width_for_height (GtkWidget       *widget,
					    gint             height,
					    gint            *minimum_width,
					    gint            *natural_width)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->current_page ? notebook->priv->current_page : GTK_WIDGET (notebook);

	gtk_widget_get_preferred_width_for_height (target, height, minimum_width, natural_width);
}

static void
cc_notebook_get_preferred_width (GtkWidget       *widget,
				 gint            *minimum_width,
				 gint            *natural_width)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->current_page ? notebook->priv->current_page : GTK_WIDGET (notebook);

	gtk_widget_get_preferred_width (target, minimum_width, natural_width);
}

static void
cc_notebook_get_preferred_height_for_width (GtkWidget       *widget,
					    gint             width,
					    gint            *minimum_height,
					    gint            *natural_height)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->current_page ? notebook->priv->current_page : GTK_WIDGET (notebook);

	gtk_widget_get_preferred_height_for_width (target, width, minimum_height, natural_height);
}

static void
cc_notebook_size_allocate (GtkWidget     *widget,
			   GtkAllocation *allocation)
{
	CcNotebook *notebook;
	GtkWidget *target;

	notebook = CC_NOTEBOOK (widget);

	target = notebook->priv->current_page ? notebook->priv->current_page : GTK_WIDGET (notebook);

	gtk_widget_size_allocate (target, allocation);
}

static void
cc_notebook_configure_event (GtkWidget         *widget,
			     GdkEventConfigure *event)
{
	CcNotebook *notebook;
	ClutterActor *actor;

	GTK_WIDGET_CLASS (cc_notebook_parent_class)->configure_event (widget, event);

	notebook = CC_NOTEBOOK (widget);
	actor = _cc_notebook_actor_for_widget (notebook, notebook->priv->current_page);
	if (actor == NULL)
		return;
	clutter_actor_set_size (actor, event->width, event->height);
}

static void
cc_notebook_class_init (CcNotebookClass *class)
{
        GObjectClass *gobject_class;
        GtkWidgetClass *widget_class;

        gobject_class = (GObjectClass*)class;
        widget_class = (GtkWidgetClass*)class;

        gobject_class->set_property = cc_notebook_set_property;
        gobject_class->get_property = cc_notebook_get_property;

	g_object_class_install_property (gobject_class,
					 PROP_CURRENT_PAGE,
					 g_param_spec_pointer ("current-page",
							      "Current Page",
							      "Current Page",
							      G_PARAM_READWRITE));

	widget_class->get_request_mode = cc_notebook_get_request_mode;
	widget_class->get_preferred_height = cc_notebook_get_preferred_height;
	widget_class->get_preferred_width_for_height = cc_notebook_get_preferred_width_for_height;
	widget_class->get_preferred_width = cc_notebook_get_preferred_width;
	widget_class->get_preferred_height_for_width = cc_notebook_get_preferred_height_for_width;
//	widget_class->size_allocate = cc_notebook_size_allocate;
//	widget_class->configure_event = cc_notebook_configure_event;

        g_type_class_add_private (class, sizeof (CcNotebookPrivate));
}

static void
cc_notebook_init (CcNotebook *notebook)
{
	CcNotebookPrivate *priv;
	ClutterConstraint *constraint;
	ClutterLayoutManager *layout;

	priv = GET_PRIVATE (notebook);
	notebook->priv = priv;

	priv->embed = gtk_clutter_embed_new ();
	gtk_widget_push_composite_child ();
	gtk_box_pack_start (GTK_BOX (notebook), priv->embed, TRUE, TRUE, 0);
	gtk_widget_pop_composite_child ();
	gtk_widget_show (priv->embed);
	priv->stage = gtk_clutter_embed_get_stage (GTK_CLUTTER_EMBED (priv->embed));

	clutter_actor_set_background_color (CLUTTER_ACTOR (priv->stage), CLUTTER_COLOR_Red);

	/* Scroll actor */
	priv->scroll_actor = clutter_scroll_actor_new ();
	clutter_actor_add_child (priv->stage, priv->scroll_actor);
	clutter_scroll_actor_set_scroll_mode (CLUTTER_SCROLL_ACTOR (priv->scroll_actor),
					      CLUTTER_SCROLL_HORIZONTALLY);

	/* Horizontal flow, inside the scroll */
	priv->bin = clutter_actor_new ();
	layout = clutter_flow_layout_new (CLUTTER_FLOW_HORIZONTAL);
	clutter_flow_layout_set_homogeneous (layout, TRUE);
	clutter_actor_set_layout_manager (priv->bin, layout);
	clutter_actor_add_child (priv->scroll_actor, priv->bin);
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
	ClutterActor *actor;

	g_return_if_fail (CC_IS_NOTEBOOK (notebook));
	g_return_if_fail (GTK_IS_WIDGET (widget));

	if (widget == notebook->priv->current_page)
		return;

	actor = _cc_notebook_actor_for_widget (notebook, widget);
	g_return_if_fail (actor != NULL);

	clutter_actor_get_position (actor, &pos.x, &pos.y);

	clutter_actor_save_easing_state (notebook->priv->scroll_actor);
	clutter_actor_set_easing_duration (notebook->priv->scroll_actor, 500);

	clutter_scroll_actor_scroll_to_point (CLUTTER_SCROLL_ACTOR (notebook->priv->scroll_actor), &pos);

	clutter_actor_restore_easing_state (notebook->priv->scroll_actor);

	notebook->priv->current_page = widget;
	g_object_notify (G_OBJECT (notebook), "current-page");

	gtk_widget_queue_resize (GTK_WIDGET (notebook));
}

GtkWidget *
cc_notebook_get_current (CcNotebook *notebook)
{
	g_return_val_if_fail (CC_IS_NOTEBOOK (notebook), NULL);

	return notebook->priv->current_page;
}
