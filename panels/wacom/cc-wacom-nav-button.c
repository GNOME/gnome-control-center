/*
 * Copyright © 2011 Red Hat, Inc.
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
 * Authors: Bastien Nocera <hadess@hadess.net>
 *
 */

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "cc-wacom-nav-button.h"

G_DEFINE_TYPE (CcWacomNavButton, cc_wacom_nav_button, GTK_TYPE_BOX)

#define WACOM_NAV_BUTTON_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_WACOM_NAV_BUTTON, CcWacomNavButtonPrivate))

struct _CcWacomNavButtonPrivate
{
	GtkNotebook *notebook;
	GtkWidget   *label;
	GtkWidget   *prev;
	GtkWidget   *next;
	guint        page_added_id;
	guint        page_removed_id;
	guint        page_switched_id;
	gboolean     ignore_first_page;
};

enum {
	PROP_0,
	PROP_NOTEBOOK,
	PROP_IGNORE_FIRST
};

static void
cc_wacom_nav_button_update (CcWacomNavButton *nav)
{
	CcWacomNavButtonPrivate *priv = nav->priv;
	int num_pages;
	int current_page;
	char *text;

	if (priv->notebook == NULL) {
		gtk_widget_hide (GTK_WIDGET (nav));
		return;
	}

	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (priv->notebook));
	if (num_pages == 0)
		return;
	if (priv->ignore_first_page && num_pages == 1)
		return;

	if (priv->ignore_first_page)
		num_pages--;

	g_assert (num_pages >= 1);

	gtk_revealer_set_reveal_child (GTK_REVEALER (gtk_widget_get_parent (nav)),
				       num_pages > 1);

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook));
	if (current_page < 0)
		return;
	if (priv->ignore_first_page)
		current_page--;
	gtk_widget_set_sensitive (priv->prev, current_page == 0 ? FALSE : TRUE);
	gtk_widget_set_sensitive (priv->next, current_page + 1 == num_pages ? FALSE : TRUE);

	text = g_strdup_printf (_("%d of %d"),
				current_page + 1,
				num_pages);
	gtk_label_set_text (GTK_LABEL (priv->label), text);
}

static void
pages_changed (GtkNotebook      *notebook,
	       GtkWidget        *child,
	       guint             page_num,
	       CcWacomNavButton *nav)
{
	cc_wacom_nav_button_update (nav);
}

static void
page_switched (GtkNotebook      *notebook,
	       GParamSpec       *pspec,
	       CcWacomNavButton *nav)
{
	cc_wacom_nav_button_update (nav);
}

static void
next_clicked (GtkButton        *button,
	      CcWacomNavButton *nav)
{
	int current_page;

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (nav->priv->notebook));
	current_page++;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (nav->priv->notebook), current_page);
}

static void
prev_clicked (GtkButton        *button,
	      CcWacomNavButton *nav)
{
	int current_page;

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (nav->priv->notebook));
	current_page--;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (nav->priv->notebook), current_page--);
}

static void
cc_wacom_nav_button_set_property (GObject      *object,
				  guint         property_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	CcWacomNavButton *nav = CC_WACOM_NAV_BUTTON (object);
	CcWacomNavButtonPrivate *priv = nav->priv;

	switch (property_id) {
	case PROP_NOTEBOOK:
		if (priv->notebook) {
			g_signal_handler_disconnect (priv->notebook, priv->page_added_id);
			g_signal_handler_disconnect (priv->notebook, priv->page_removed_id);
			g_signal_handler_disconnect (priv->notebook, priv->page_switched_id);
			g_object_unref (priv->notebook);
		}
		priv->notebook = g_value_dup_object (value);
		priv->page_added_id = g_signal_connect (G_OBJECT (priv->notebook), "page-added",
							G_CALLBACK (pages_changed), nav);
		priv->page_removed_id = g_signal_connect (G_OBJECT (priv->notebook), "page-removed",
							  G_CALLBACK (pages_changed), nav);
		priv->page_switched_id = g_signal_connect (G_OBJECT (priv->notebook), "notify::page",
							   G_CALLBACK (page_switched), nav);
		cc_wacom_nav_button_update (nav);
		break;
	case PROP_IGNORE_FIRST:
		priv->ignore_first_page = g_value_get_boolean (value);
		cc_wacom_nav_button_update (nav);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_nav_button_dispose (GObject *object)
{
	CcWacomNavButtonPrivate *priv = CC_WACOM_NAV_BUTTON (object)->priv;

	if (priv->notebook) {
		g_signal_handler_disconnect (priv->notebook, priv->page_added_id);
		priv->page_added_id = 0;
		g_signal_handler_disconnect (priv->notebook, priv->page_removed_id);
		priv->page_removed_id = 0;
		g_signal_handler_disconnect (priv->notebook, priv->page_switched_id);
		priv->page_switched_id = 0;
		g_object_unref (priv->notebook);
		priv->notebook = NULL;
	}

	G_OBJECT_CLASS (cc_wacom_nav_button_parent_class)->dispose (object);
}

static void
cc_wacom_nav_button_class_init (CcWacomNavButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (CcWacomNavButtonPrivate));

	object_class->set_property = cc_wacom_nav_button_set_property;
	object_class->dispose = cc_wacom_nav_button_dispose;

	g_object_class_install_property (object_class, PROP_NOTEBOOK,
					 g_param_spec_object ("notebook", "notebook", "notebook",
							      GTK_TYPE_NOTEBOOK,
							      G_PARAM_WRITABLE));
	g_object_class_install_property (object_class, PROP_IGNORE_FIRST,
					 g_param_spec_boolean ("ignore-first", "ignore-first", "ignore-first",
							       FALSE,
							       G_PARAM_WRITABLE));
}

static void
cc_wacom_nav_button_init (CcWacomNavButton *self)
{
	CcWacomNavButtonPrivate *priv;
	GtkStyleContext *context;
	GtkWidget *image, *box;

	priv = self->priv = WACOM_NAV_BUTTON_PRIVATE (self);

	/* Label */
	priv->label = gtk_label_new (NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (priv->label), "dim-label");
	gtk_box_pack_start (GTK_BOX (self), priv->label,
			    FALSE, FALSE, 8);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	context = gtk_widget_get_style_context (GTK_WIDGET (box));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);
	gtk_box_pack_start (GTK_BOX (self), box,
			    FALSE, FALSE, 0);

	/* Prev button */
	priv->prev = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (priv->prev), image);
	g_signal_connect (G_OBJECT (priv->prev), "clicked",
			  G_CALLBACK (prev_clicked), self);
	gtk_widget_set_valign (priv->prev, GTK_ALIGN_CENTER);

	/* Next button */
	priv->next = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (priv->next), image);
	g_signal_connect (G_OBJECT (priv->next), "clicked",
			  G_CALLBACK (next_clicked), self);
	gtk_widget_set_valign (priv->next, GTK_ALIGN_CENTER);

	gtk_box_pack_start (GTK_BOX (box), priv->prev,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), priv->next,
			    FALSE, FALSE, 0);

	gtk_widget_show (priv->label);
	gtk_widget_show_all (box);
}

GtkWidget *
cc_wacom_nav_button_new (void)
{
	return GTK_WIDGET (g_object_new (CC_TYPE_WACOM_NAV_BUTTON, NULL));
}
