/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>

#include "cc-page.h"

#define CC_PAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_PAGE, CcPagePrivate))

struct CcPagePrivate
{
        char            *id;
        char            *display_name;
};

enum {
        PROP_0,
        PROP_ID,
        PROP_DISPLAY_NAME,
};

static void     cc_page_class_init    (CcPageClass *klass);
static void     cc_page_init          (CcPage      *page);
static void     cc_page_finalize      (GObject     *object);

G_DEFINE_ABSTRACT_TYPE (CcPage, cc_page, GTK_TYPE_ALIGNMENT)

static void
_cc_page_set_id (CcPage     *page,
                 const char *id)
{
        g_free (page->priv->id);
        page->priv->id = g_strdup (id);
}

static void
_cc_page_set_display_name (CcPage     *page,
                           const char *name)
{
        g_free (page->priv->display_name);
        page->priv->display_name = g_strdup (name);
}

static void
cc_page_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
        CcPage *self;

        self = CC_PAGE (object);

        switch (prop_id) {
        case PROP_ID:
                _cc_page_set_id (self, g_value_get_string (value));
                break;
        case PROP_DISPLAY_NAME:
                _cc_page_set_display_name (self, g_value_get_string (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_page_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
        CcPage *self;

        self = CC_PAGE (object);

        switch (prop_id) {
        case PROP_ID:
                g_value_set_string (value, self->priv->id);
                break;
        case PROP_DISPLAY_NAME:
                g_value_set_string (value, self->priv->display_name);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
cc_page_constructor (GType                  type,
                     guint                  n_construct_properties,
                     GObjectConstructParam *construct_properties)
{
        CcPage      *page;

        page = CC_PAGE (G_OBJECT_CLASS (cc_page_parent_class)->constructor (type,
                                                                            n_construct_properties,
                                                                            construct_properties));

        return G_OBJECT (page);
}

static void
cc_page_class_init (CcPageClass *klass)
{
        GObjectClass    *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_page_get_property;
        object_class->set_property = cc_page_set_property;
        object_class->constructor = cc_page_constructor;
        object_class->finalize = cc_page_finalize;

        g_type_class_add_private (klass, sizeof (CcPagePrivate));

        g_object_class_install_property (object_class,
                                         PROP_ID,
                                         g_param_spec_string ("id",
                                                              "id",
                                                              "id",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (object_class,
                                         PROP_DISPLAY_NAME,
                                         g_param_spec_string ("display-name",
                                                              "display name",
                                                              "display name",
                                                              NULL,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
cc_page_init (CcPage *page)
{

        page->priv = CC_PAGE_GET_PRIVATE (page);
}

static void
cc_page_finalize (GObject *object)
{
        CcPage *page;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_PAGE (object));

        page = CC_PAGE (object);

        g_return_if_fail (page->priv != NULL);

        g_free (page->priv->id);
        g_free (page->priv->display_name);

        G_OBJECT_CLASS (cc_page_parent_class)->finalize (object);
}
