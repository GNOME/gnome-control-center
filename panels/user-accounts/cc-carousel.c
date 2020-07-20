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

#include "cc-carousel.h"

#include <glib-object.h>
#include <gtk/gtk.h>

#define ARROW_SIZE 20

struct _CcCarouselItem {
        GtkRadioButton parent;

        gint page;
};

G_DEFINE_TYPE (CcCarouselItem, cc_carousel_item, GTK_TYPE_RADIO_BUTTON)

GtkWidget *
cc_carousel_item_new (void)
{
        return g_object_new (CC_TYPE_CAROUSEL_ITEM, NULL);
}

static void
cc_carousel_item_class_init (CcCarouselItemClass *klass)
{
}

static void
cc_carousel_item_init (CcCarouselItem *self)
{
        gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (self), FALSE);
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (self)),
                                     "carousel-item");
}

struct _CcCarousel {
        GtkRevealer parent;

        GList *children;
        gint visible_page;
        CcCarouselItem *selected_item;
        GtkWidget *last_box;
        GtkWidget *arrow;
        gint arrow_start_x;

        /* Widgets */
        GtkStack *stack;
        GtkWidget *go_back_button;
        GtkWidget *go_next_button;

        GtkStyleProvider *provider;
};

G_DEFINE_TYPE (CcCarousel, cc_carousel, GTK_TYPE_REVEALER)

enum {
        ITEM_ACTIVATED,
        NUM_SIGNALS
};

static guint signals[NUM_SIGNALS] = { 0, };

#define ITEMS_PER_PAGE 3

static gint
cc_carousel_item_get_x (CcCarouselItem *item,
                        CcCarousel     *carousel)
{
        GtkWidget *widget, *parent;
        gint width;
        gint dest_x = 0;

        parent = GTK_WIDGET (carousel->stack);
        widget = GTK_WIDGET (item);

        width = gtk_widget_get_allocated_width (widget);
        if (!gtk_widget_translate_coordinates (widget,
                                               parent,
                                               width / 2,
                                               0,
                                               &dest_x,
                                               NULL))
                return 0;

        return CLAMP (dest_x - ARROW_SIZE,
                      0,
                      gtk_widget_get_allocated_width (parent));
}

static void
cc_carousel_move_arrow (CcCarousel *self)
{
        GtkStyleContext *context;
        gchar *css;
        gint end_x;
        GtkSettings *settings;
        gboolean animations;

        if (!self->selected_item)
                return;

        end_x = cc_carousel_item_get_x (self->selected_item, self);

        context = gtk_widget_get_style_context (self->arrow);
        if (self->provider)
                gtk_style_context_remove_provider (context, self->provider);
        g_clear_object (&self->provider);

        settings = gtk_widget_get_settings (GTK_WIDGET (self));
        g_object_get (settings, "gtk-enable-animations", &animations, NULL);

        /* Animate the arrow movement if animations are enabled. Otherwise,
         * jump the arrow to the right location instantly. */
        if (animations)
        {
                css = g_strdup_printf ("@keyframes arrow_keyframes-%d {\n"
                                       "  from { margin-left: %dpx; }\n"
                                       "  to { margin-left: %dpx; }\n"
                                       "}\n"
                                       "* {\n"
                                       "  animation-name: arrow_keyframes-%d;\n"
                                       "}\n",
                                       end_x, self->arrow_start_x, end_x, end_x);
        }
        else
        {
                css = g_strdup_printf ("* { margin-left: %dpx }", end_x);
        }

        self->provider = GTK_STYLE_PROVIDER (gtk_css_provider_new ());
        gtk_css_provider_load_from_data (GTK_CSS_PROVIDER (self->provider), css, -1, NULL);
        gtk_style_context_add_provider (context, self->provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        g_free (css);
}

static gint
get_last_page_number (CcCarousel *self)
{
        if (g_list_length (self->children) == 0)
                return 0;

        return ((g_list_length (self->children) - 1) / ITEMS_PER_PAGE);
}

static void
update_buttons_visibility (CcCarousel *self)
{
        gtk_widget_set_visible (self->go_back_button, (self->visible_page > 0));
        gtk_widget_set_visible (self->go_next_button, (self->visible_page < get_last_page_number (self)));
}

/**
 * cc_carousel_find_item:
 * @carousel: an CcCarousel instance
 * @data: user data passed to the comparison function
 * @func: the function to call for each element.
 *      It should return 0 when the desired element is found
 *
 * Finds an CcCarousel item using the supplied function to find the
 * desired element.
 * Ideally useful for matching a model object and its correspondent
 * widget.
 *
 * Returns: the found CcCarouselItem, or %NULL if it is not found
 */
CcCarouselItem *
cc_carousel_find_item (CcCarousel    *self,
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
on_item_toggled (CcCarousel     *self,
                 GdkEvent       *event,
                 CcCarouselItem *item)
{
        cc_carousel_select_item (self, item);
}

void
cc_carousel_select_item (CcCarousel     *self,
                         CcCarouselItem *item)
{
        gboolean page_changed = TRUE;
        GList *children;

        /* Select first user if none is specified */
        if (item == NULL)
        {
                if (self->children != NULL)
                        item = self->children->data;
                else
                        return;
        }

        if (self->selected_item != NULL)
        {
                page_changed = (self->selected_item->page != item->page);
                self->arrow_start_x = cc_carousel_item_get_x (self->selected_item, self);
        }

        self->selected_item = item;
        self->visible_page = item->page;
        g_signal_emit (self, signals[ITEM_ACTIVATED], 0, item);

        if (!page_changed)
        {
                cc_carousel_move_arrow (self);
                return;
        }

        children = gtk_container_get_children (GTK_CONTAINER (self->stack));
        gtk_stack_set_visible_child (self->stack, GTK_WIDGET (g_list_nth_data (children, self->visible_page)));

        update_buttons_visibility (self);

        /* cc_carousel_move_arrow is called from on_transition_running */
}

static void
cc_carousel_select_item_at_index (CcCarousel *self,
                                  gint        index)
{
        GList *l = NULL;

        l = g_list_nth (self->children, index);
        cc_carousel_select_item (self, l->data);
}

static void
cc_carousel_goto_previous_page (GtkWidget *button,
                                gpointer   user_data)
{
        CcCarousel *self = CC_CAROUSEL (user_data);

        self->visible_page--;
        if (self->visible_page < 0)
                self->visible_page = 0;

        /* Select first item of the page */
        cc_carousel_select_item_at_index (self, self->visible_page * ITEMS_PER_PAGE);
}

static void
cc_carousel_goto_next_page (GtkWidget *button,
                            gpointer   user_data)
{
        CcCarousel *self = CC_CAROUSEL (user_data);
        gint last_page;

        last_page = get_last_page_number (self);

        self->visible_page++;
        if (self->visible_page > last_page)
                self->visible_page = last_page;

        /* Select first item of the page */
        cc_carousel_select_item_at_index (self, self->visible_page * ITEMS_PER_PAGE);
}

static void
cc_carousel_add (GtkContainer *container,
                 GtkWidget    *widget)
{
        CcCarousel *self = CC_CAROUSEL (container);
        gboolean last_box_is_full;

        if (!CC_IS_CAROUSEL_ITEM (widget)) {
                GTK_CONTAINER_CLASS (cc_carousel_parent_class)->add (container, widget);
                return;
        }

        gtk_style_context_add_class (gtk_widget_get_style_context (widget), "menu");
        gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);

        self->children = g_list_append (self->children, widget);
        CC_CAROUSEL_ITEM (widget)->page = get_last_page_number (self);
        if (self->selected_item != NULL)
                gtk_radio_button_join_group (GTK_RADIO_BUTTON (widget), GTK_RADIO_BUTTON (self->selected_item));
        g_signal_connect_object (widget, "button-press-event", G_CALLBACK (on_item_toggled), self, G_CONNECT_SWAPPED);

        last_box_is_full = ((g_list_length (self->children) - 1) % ITEMS_PER_PAGE == 0);
        if (last_box_is_full) {
                self->last_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_show (self->last_box);
                gtk_widget_set_valign (self->last_box, GTK_ALIGN_CENTER);
                gtk_container_add (GTK_CONTAINER (self->stack), self->last_box);
        }

        gtk_widget_show_all (widget);
        gtk_box_pack_start (GTK_BOX (self->last_box), widget, TRUE, FALSE, 10);

        update_buttons_visibility (self);
}

void
cc_carousel_purge_items (CcCarousel *self)
{
        gtk_container_forall (GTK_CONTAINER (self->stack),
                              (GtkCallback) gtk_widget_destroy,
                              NULL);

        g_list_free (self->children);
        self->children = NULL;
        self->visible_page = 0;
        self->selected_item = NULL;
}

CcCarousel *
cc_carousel_new (void)
{
        return g_object_new (CC_TYPE_CAROUSEL, NULL);
}

static void
cc_carousel_dispose (GObject *object)
{
        CcCarousel *self = CC_CAROUSEL (object);

        g_clear_object (&self->provider);
        if (self->children != NULL) {
                g_list_free (self->children);
                self->children = NULL;
        }

        G_OBJECT_CLASS (cc_carousel_parent_class)->dispose (object);
}

static void
cc_carousel_class_init (CcCarouselClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
        GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);

        gtk_widget_class_set_template_from_resource (wclass,
                                                     "/org/gnome/control-center/user-accounts/cc-carousel.ui");

        gtk_widget_class_bind_template_child (wclass, CcCarousel, stack);
        gtk_widget_class_bind_template_child (wclass, CcCarousel, go_back_button);
        gtk_widget_class_bind_template_child (wclass, CcCarousel, go_next_button);
        gtk_widget_class_bind_template_child (wclass, CcCarousel, arrow);

        gtk_widget_class_bind_template_callback (wclass, cc_carousel_goto_previous_page);
        gtk_widget_class_bind_template_callback (wclass, cc_carousel_goto_next_page);

        object_class->dispose = cc_carousel_dispose;

        container_class->add = cc_carousel_add;

        signals[ITEM_ACTIVATED] = g_signal_new ("item-activated",
                                                CC_TYPE_CAROUSEL,
                                                G_SIGNAL_RUN_LAST,
                                                0,
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__OBJECT,
                                                G_TYPE_NONE, 1,
                                                CC_TYPE_CAROUSEL_ITEM);
}

static void
on_size_allocate (CcCarousel *self)
{
       if (self->selected_item == NULL)
               return;

       if (gtk_stack_get_transition_running (self->stack))
               return;

       self->arrow_start_x = cc_carousel_item_get_x (self->selected_item, self);
       cc_carousel_move_arrow (self);
}

static void
on_transition_running (CcCarousel *self)
{
        if (!gtk_stack_get_transition_running (self->stack))
                cc_carousel_move_arrow (self);
}

static void
cc_carousel_init (CcCarousel *self)
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

        g_signal_connect_object (self->stack, "size-allocate", G_CALLBACK (on_size_allocate), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->stack, "notify::transition-running", G_CALLBACK (on_transition_running), self, G_CONNECT_SWAPPED);
}

guint
cc_carousel_get_item_count (CcCarousel *self)
{
        return g_list_length (self->children);
}
