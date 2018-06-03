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

struct _CcWacomNavButton
{
	GtkBox       parent_instance;

	GtkNotebook *notebook;
	GtkWidget   *label;
	GtkWidget   *prev;
	GtkWidget   *next;
	guint        page_added_id;
	guint        page_removed_id;
	guint        page_switched_id;
	gboolean     ignore_first_page;
};

G_DEFINE_TYPE (CcWacomNavButton, cc_wacom_nav_button, GTK_TYPE_BOX)

enum {
	PROP_0,
	PROP_NOTEBOOK,
	PROP_IGNORE_FIRST
};

static void
cc_wacom_nav_button_update (CcWacomNavButton *nav)
{
	int num_pages;
	int current_page;
	char *text;

	if (nav->notebook == NULL) {
		gtk_widget_hide (GTK_WIDGET (nav));
		return;
	}

	num_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (nav->notebook));
	if (num_pages == 0)
		return;
	if (nav->ignore_first_page && num_pages == 1)
		return;

	if (nav->ignore_first_page)
		num_pages--;

	g_assert (num_pages >= 1);

	gtk_revealer_set_reveal_child (GTK_REVEALER (gtk_widget_get_parent (GTK_WIDGET (nav))),
				       num_pages > 1);

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (nav->notebook));
	if (current_page < 0)
		return;
	if (nav->ignore_first_page)
		current_page--;
	gtk_widget_set_sensitive (nav->prev, current_page == 0 ? FALSE : TRUE);
	gtk_widget_set_sensitive (nav->next, current_page + 1 == num_pages ? FALSE : TRUE);

	text = g_strdup_printf (_("%d of %d"),
				current_page + 1,
				num_pages);
	gtk_label_set_text (GTK_LABEL (nav->label), text);
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

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (nav->notebook));
	current_page++;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (nav->notebook), current_page);
}

static void
prev_clicked (GtkButton        *button,
	      CcWacomNavButton *nav)
{
	int current_page;

	current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (nav->notebook));
	current_page--;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (nav->notebook), current_page--);
}

static void
cc_wacom_nav_button_set_property (GObject      *object,
				  guint         property_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
	CcWacomNavButton *nav = CC_WACOM_NAV_BUTTON (object);

	switch (property_id) {
	case PROP_NOTEBOOK:
		if (nav->notebook) {
			g_signal_handler_disconnect (nav->notebook, nav->page_added_id);
			g_signal_handler_disconnect (nav->notebook, nav->page_removed_id);
			g_signal_handler_disconnect (nav->notebook, nav->page_switched_id);
		}
		g_clear_object (&nav->notebook);
		nav->notebook = g_value_dup_object (value);
		nav->page_added_id = g_signal_connect (G_OBJECT (nav->notebook), "page-added",
						       G_CALLBACK (pages_changed), nav);
		nav->page_removed_id = g_signal_connect (G_OBJECT (nav->notebook), "page-removed",
							 G_CALLBACK (pages_changed), nav);
		nav->page_switched_id = g_signal_connect (G_OBJECT (nav->notebook), "notify::page",
							  G_CALLBACK (page_switched), nav);
		cc_wacom_nav_button_update (nav);
		break;
	case PROP_IGNORE_FIRST:
		nav->ignore_first_page = g_value_get_boolean (value);
		cc_wacom_nav_button_update (nav);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
cc_wacom_nav_button_dispose (GObject *object)
{
	CcWacomNavButton *self = CC_WACOM_NAV_BUTTON (object);

	if (self->notebook) {
		g_signal_handler_disconnect (self->notebook, self->page_added_id);
		self->page_added_id = 0;
		g_signal_handler_disconnect (self->notebook, self->page_removed_id);
		self->page_removed_id = 0;
		g_signal_handler_disconnect (self->notebook, self->page_switched_id);
		self->page_switched_id = 0;
		g_clear_object (&self->notebook);
	}

	G_OBJECT_CLASS (cc_wacom_nav_button_parent_class)->dispose (object);
}

static void
cc_wacom_nav_button_class_init (CcWacomNavButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
	GtkStyleContext *context;
	GtkWidget *image, *box;

	/* Label */
	self->label = gtk_label_new (NULL);
	gtk_style_context_add_class (gtk_widget_get_style_context (self->label), "dim-label");
	gtk_box_pack_start (GTK_BOX (self), self->label,
			    FALSE, FALSE, 8);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	context = gtk_widget_get_style_context (GTK_WIDGET (box));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);
	gtk_box_pack_start (GTK_BOX (self), box,
			    FALSE, FALSE, 0);

	/* Prev button */
	self->prev = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (self->prev), image);
	g_signal_connect (G_OBJECT (self->prev), "clicked",
			  G_CALLBACK (prev_clicked), self);
	gtk_widget_set_valign (self->prev, GTK_ALIGN_CENTER);

	/* Next button */
	self->next = gtk_button_new ();
	image = gtk_image_new_from_icon_name ("go-next-symbolic", GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (self->next), image);
	g_signal_connect (G_OBJECT (self->next), "clicked",
			  G_CALLBACK (next_clicked), self);
	gtk_widget_set_valign (self->next, GTK_ALIGN_CENTER);

	gtk_box_pack_start (GTK_BOX (box), self->prev,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), self->next,
			    FALSE, FALSE, 0);

	gtk_widget_show (self->label);
	gtk_widget_show_all (box);
}

GtkWidget *
cc_wacom_nav_button_new (void)
{
	return GTK_WIDGET (g_object_new (CC_TYPE_WACOM_NAV_BUTTON, NULL));
}
