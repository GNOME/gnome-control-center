/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2016 (c) Red Hat, Inc,
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
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include "um-carousel.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#define ARROW_SIZE 20

struct _UmCarouselItem {
        GtkRadioButton parent;

        gint page;
};

G_DEFINE_TYPE (UmCarouselItem, um_carousel_item, GTK_TYPE_RADIO_BUTTON)

GtkWidget *
um_carousel_item_new (void)
{
        return g_object_new (UM_TYPE_CAROUSEL_ITEM, NULL);
}

static void
um_carousel_item_class_init (UmCarouselItemClass *klass)
{
}

static void
um_carousel_item_init (UmCarouselItem *self)
{
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (self), FALSE);
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)),
                                     "carousel-item");
}

struct _UmCarousel {
        GtkRevealer parent;

        GList *children;
        gint visible_page;
        UmCarouselItem *selected_item;
        GtkWidget *last_box;
        GtkWidget *arrow;

        /* Widgets */
        GtkStack *stack;
        GtkWidget *go_back_button;
        GtkWidget *go_next_button;

        GtkStyleProvider *provider;
};

G_DEFINE_TYPE (UmCarousel, um_carousel, GTK_TYPE_REVEALER)

enum {
        ITEM_ACTIVATED,
        NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

#define ITEMS_PER_PAGE 3

static gint
um_carousel_item_get_x (UmCarouselItem *item,
                        UmCarousel     *carousel)
{
        GtkWidget *widget, *parent;
        gint width;
        gint dest_x;

        parent = GTK_WIDGET (carousel->stack);
        widget = GTK_WIDGET (item);

        width = gtk_widget_get_allocated_width (widget);
        gtk_widget_translate_coordinates (widget,
                                          parent,
                                          width / 2,
                                          0,
                                          &dest_x,
                                          NULL);

        return CLAMP (dest_x - ARROW_SIZE,
                      0,
                      gtk_widget_get_allocated_width (parent));
}

static void
um_carousel_move_arrow (UmCarousel *self)
{
        GtkStyleContext *context;
        gchar *css;
        gint end_x;

        if (!self->selected_item)
                return;

        end_x = um_carousel_item_get_x (self->selected_item, self);

        context = gtk_widget_get_style_context (self->arrow);
        if (self->provider)
                gtk_style_context_remove_provider (context, self->provider);
        g_clear_object (&self->provider);

        css = g_strdup_printf ("* {\n"
                               "  margin-left: %dpx;\n"
                               "}\n", end_x);

        self->provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
        gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (self->provider), css, -1, NULL);
        gtk_style_context_add_provider (context, self->provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        g_free (css);
}

static gint
get_last_page_number (UmCarousel *self)
{
        if (g_list_length (self->children) == 0)
                return 0;

        return ((g_list_length (self->children) - 1) / ITEMS_PER_PAGE);
}

static void
update_buttons_visibility (UmCarousel *self)
{
        gtk_widget_set_visible (self->go_back_button, (self->visible_page > 0));
        gtk_widget_set_visible (self->go_next_button, (self->visible_page < get_last_page_number (self)));
}

/**
 * um_carousel_find_item:
 * @carousel: an UmCarousel instance
 * @data: user data passed to the comparation function
 * @func: the function to call for each element.
 *      It should return 0 when the desired element is found
 *
 * Finds an UmCarousel item using the supplied function to find the
 * desired element.
 * Ideally useful for matching a model object and its correspondent
 * widget.
 *
 * Returns: the found UmCarouselItem, or %NULL if it is not found
 */
UmCarouselItem *
um_carousel_find_item (UmCarousel    *self,
                       gconstpointer  data,
                       GCompareFunc   func)
{
        GList *list;

        list = self->children;
        while (list != NULL)
        {
                if (!func (list->data, data))
                        return list->data;
                list = list->next;
        }

        return NULL;
}

static void
on_item_toggled (UmCarouselItem *item,
                 GdkEvent       *event,
                 gpointer        user_data)
{
        UmCarousel *self = UM_CAROUSEL (user_data);

        self->selected_item = item;

        g_signal_emit (user_data, signals[ITEM_ACTIVATED], 0, item);

        um_carousel_move_arrow (self);
}

void
um_carousel_select_item (UmCarousel     *self,
                         UmCarouselItem *item)
{
        gchar *page_name;

        on_item_toggled (item, NULL, self);

        self->visible_page = item->page;
        page_name = g_strdup_printf ("%d", self->visible_page);
        gtk_stack_set_visible_child_name (self->stack, page_name);

        g_free (page_name);

        update_buttons_visibility (self);
}

static void
um_carousel_select_item_at_index (UmCarousel *self,
                                  gint        index)
{
        GList *l = NULL;

        l = g_list_nth (self->children, index);
        um_carousel_select_item (self, l->data);
}

static void
um_carousel_goto_previous_page (GtkWidget *button,
                                gpointer   user_data)
{
        UmCarousel *self = UM_CAROUSEL (user_data);

        self->visible_page--;
        if (self->visible_page < 0)
                self->visible_page = 0;

        /* Select first item of the page */
        um_carousel_select_item_at_index (self, self->visible_page * ITEMS_PER_PAGE);
}

static void
um_carousel_goto_next_page (GtkWidget *button,
                            gpointer   user_data)
{
        UmCarousel *self = UM_CAROUSEL (user_data);
        gint last_page;

        last_page = get_last_page_number (self);

        self->visible_page++;
        if (self->visible_page > last_page)
                self->visible_page = last_page;

        /* Select first item of the page */
        um_carousel_select_item_at_index (self, self->visible_page * ITEMS_PER_PAGE);
}

static void
um_carousel_add (GtkContainer *container,
                 GtkWidget    *widget)
{
        UmCarousel *self = UM_CAROUSEL (container);
        gboolean last_box_is_full;

        if (!UM_IS_CAROUSEL_ITEM (widget)) {
                GTK_CONTAINER_CLASS (um_carousel_parent_class)->add (container, widget);
                return;
        }

        gtk_style_context_add_class (gtk_widget_get_style_context (widget), "menu");
        gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);

        self->children = g_list_append (self->children, widget);
        UM_CAROUSEL_ITEM (widget)->page = get_last_page_number (self);
        if (self->selected_item != NULL)
                gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (self->selected_item));
        g_signal_connect (widget, "button-press-event", G_CALLBACK (on_item_toggled), self);

        last_box_is_full = ((g_list_length (self->children) - 1) % ITEMS_PER_PAGE == 0);
        if (last_box_is_full) {
                gchar *page;

                page = g_strdup_printf ("%d", UM_CAROUSEL_ITEM (widget)->page);
                self->last_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_set_valign (self->last_box, GTK_ALIGN_CENTER);
                gtk_stack_add_named (self->stack, self->last_box, page);
        }

        gtk_box_pack_start (GTK_BOX (self->last_box), widget, TRUE, FALSE, 10);
        gtk_widget_show_all (self->last_box);

        update_buttons_visibility (self);

        /* If there's only one child, select it. */
        if (self->children->next == NULL)
                um_carousel_select_item_at_index (self, 0);
}

void
um_carousel_purge_items (UmCarousel *self)
{
        gtk_container_forall (GTK_CONTAINER (self->stack),
                              (GtkCallback) gtk_widget_destroy,
                              NULL);

        g_list_free (self->children);
        self->children = NULL;
        self->visible_page = 0;
        self->selected_item = NULL;
}

UmCarousel *
um_carousel_new (void)
{
        return g_object_new (UM_TYPE_CAROUSEL, NULL);
}

static void
um_carousel_class_init (UmCarouselClass *klass)
{
        GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
        GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

        gtk_widget_class_set_template_from_resource (wclass,
                                                     "/org/gnome/control-center/user-accounts/carousel.ui");

        gtk_widget_class_bind_template_child (wclass, UmCarousel, stack);
        gtk_widget_class_bind_template_child (wclass, UmCarousel, go_back_button);
        gtk_widget_class_bind_template_child (wclass, UmCarousel, go_next_button);
        gtk_widget_class_bind_template_child (wclass, UmCarousel, arrow);

        gtk_widget_class_bind_template_callback (wclass, um_carousel_goto_previous_page);
        gtk_widget_class_bind_template_callback (wclass, um_carousel_goto_next_page);

        container_class->add = um_carousel_add;

        signals[ITEM_ACTIVATED] = g_signal_new ("item-activated",
                                                UM_TYPE_CAROUSEL,
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__OBJECT,
                                                G_TYPE_NONE, 1,
                                                UM_TYPE_CAROUSEL_ITEM);
}

static void
um_carousel_init (UmCarousel *self)
{
        GtkStyleProvider *provider;

        gtk_widget_init_template (GTK_WIDGET (self));

        provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
        gtk_css_provider_load_from_resource (GTK_CSS_PROVIDER (provider),
                                             "/org/gnome/control-center/user-accounts/carousel.css");

        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   provider,
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        g_object_unref (provider);

        g_signal_connect_swapped (self->stack, "size-allocate", G_CALLBACK (um_carousel_move_arrow), self);
}
