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

#include "cc-panel.h"
#include "cc-page.h"

#define CC_PANEL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_PANEL, CcPanelPrivate))

struct CcPanelPrivate
{
        char            *id;
        char            *display_name;
        char            *category;
        char            *current_location;

        gboolean         is_active;
        CcPage          *current_page;
};

enum {
        PROP_0,
        PROP_ID,
        PROP_DISPLAY_NAME,
        PROP_CATEGORY,
        PROP_CURRENT_LOCATION,
        PROP_CURRENT_PAGE,
};

enum {
        ACTIVE_CHANGED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     cc_panel_class_init    (CcPanelClass *klass);
static void     cc_panel_init          (CcPanel      *panel);
static void     cc_panel_finalize      (GObject       *object);

G_DEFINE_ABSTRACT_TYPE (CcPanel, cc_panel, GTK_TYPE_ALIGNMENT)

static void
_cc_panel_set_id (CcPanel    *panel,
                  const char *id)
{
        g_free (panel->priv->id);
        panel->priv->id = g_strdup (id);
}

static void
_cc_panel_set_display_name (CcPanel    *panel,
                            const char *name)
{
        g_free (panel->priv->display_name);
        panel->priv->display_name = g_strdup (name);
}

static void
_cc_panel_set_current_page (CcPanel    *panel,
                            CcPage     *page)
{
        CcPage *old;

        if (page == panel->priv->current_page)
                return;

        old = panel->priv->current_page;
        panel->priv->current_page = page;
        if (old != NULL) {
                cc_page_set_active (old, FALSE);
        }
        if (panel->priv->current_page != NULL) {
                g_object_ref (panel->priv->current_page);
                cc_page_set_active (panel->priv->current_page,
                                    panel->priv->is_active);
        }
        if (old != NULL) {
                g_object_unref (old);
        }

        g_object_notify (G_OBJECT (panel), "current-page");
}

static void
cc_panel_set_property (GObject      *object,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
        CcPanel *self;

        self = CC_PANEL (object);

        switch (prop_id) {
        case PROP_ID:
                _cc_panel_set_id (self, g_value_get_string (value));
                break;
        case PROP_DISPLAY_NAME:
                _cc_panel_set_display_name (self, g_value_get_string (value));
                break;
        case PROP_CURRENT_PAGE:
                _cc_panel_set_current_page (self, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_panel_get_property (GObject    *object,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
        CcPanel *self;

        self = CC_PANEL (object);

        switch (prop_id) {
        case PROP_ID:
                g_value_set_string (value, self->priv->id);
                break;
        case PROP_DISPLAY_NAME:
                g_value_set_string (value, self->priv->display_name);
                break;
        case PROP_CURRENT_PAGE:
                g_value_set_object (value, self->priv->current_page);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
cc_panel_real_active_changed (CcPanel *panel,
                              gboolean is_active)
{
        if (panel->priv->is_active == is_active)
                return;

        panel->priv->is_active = is_active;
        if (panel->priv->current_page != NULL) {
                cc_page_set_active (panel->priv->current_page, is_active);
        }
        g_debug ("Panel %s is %s",
                 panel->priv->id,
                 panel->priv->is_active ? "active" : "inactive");
}

void
cc_panel_set_active (CcPanel *panel,
                     gboolean is_active)
{
        g_return_if_fail (CC_IS_PANEL (panel));

        g_object_ref (panel);
        gtk_widget_queue_resize (GTK_WIDGET (panel));
        if (panel->priv->is_active != is_active) {
                g_signal_emit (panel, signals [ACTIVE_CHANGED], 0, is_active);
        }
        g_object_unref (panel);
}

gboolean
cc_panel_is_active (CcPanel *panel)
{
        g_return_val_if_fail (CC_IS_PANEL (panel), FALSE);
        return panel->priv->is_active;
}

static GObject *
cc_panel_constructor (GType                  type,
                      guint                  n_construct_properties,
                      GObjectConstructParam *construct_properties)
{
        CcPanel      *panel;

        panel = CC_PANEL (G_OBJECT_CLASS (cc_panel_parent_class)->constructor (type,
                                                                               n_construct_properties,
                                                                               construct_properties));

        return G_OBJECT (panel);
}

static void
cc_panel_class_init (CcPanelClass *klass)
{
        GObjectClass    *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = cc_panel_get_property;
        object_class->set_property = cc_panel_set_property;
        object_class->constructor = cc_panel_constructor;
        object_class->finalize = cc_panel_finalize;

        klass->active_changed = cc_panel_real_active_changed;

        g_type_class_add_private (klass, sizeof (CcPanelPrivate));

        signals [ACTIVE_CHANGED]
                = g_signal_new ("active-changed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_FIRST,
                                G_STRUCT_OFFSET (CcPanelClass, active_changed),
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__BOOLEAN,
                                G_TYPE_NONE,
                                1, G_TYPE_BOOLEAN);

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
        g_object_class_install_property (object_class,
                                         PROP_CURRENT_PAGE,
                                         g_param_spec_object ("current-page",
                                                              "",
                                                              "",
                                                              CC_TYPE_PAGE,
                                                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void
cc_panel_init (CcPanel *panel)
{

        panel->priv = CC_PANEL_GET_PRIVATE (panel);
}

static void
cc_panel_finalize (GObject *object)
{
        CcPanel *panel;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_PANEL (object));

        panel = CC_PANEL (object);

        g_return_if_fail (panel->priv != NULL);

        g_free (panel->priv->id);
        g_free (panel->priv->display_name);

        G_OBJECT_CLASS (cc_panel_parent_class)->finalize (object);
}
