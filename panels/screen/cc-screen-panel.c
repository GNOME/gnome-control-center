/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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

#include "cc-screen-panel.h"

#define WID(b, w) (GtkWidget *) gtk_builder_get_object (b, w)

G_DEFINE_DYNAMIC_TYPE (CcScreenPanel, cc_screen_panel, CC_TYPE_PANEL)

#define SCREEN_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_SCREEN_PANEL, CcScreenPanelPrivate))

struct _CcScreenPanelPrivate
{
  GSettings *lock_settings;
};


static void
cc_screen_panel_get_property (GObject    *object,
                              guint       property_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_screen_panel_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
cc_screen_panel_dispose (GObject *object)
{
  CcScreenPanelPrivate *priv = CC_SCREEN_PANEL (object)->priv;

  if (priv->lock_settings)
    {
      g_object_unref (priv->lock_settings);
      priv->lock_settings = NULL;
    }

  G_OBJECT_CLASS (cc_screen_panel_parent_class)->dispose (object);
}

static void
cc_screen_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_screen_panel_parent_class)->finalize (object);
}

static void
on_lock_settings_changed (GSettings     *settings,
                          const char    *key,
                          CcScreenPanel *panel)
{
}

static void
cc_screen_panel_class_init (CcScreenPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcScreenPanelPrivate));

  object_class->get_property = cc_screen_panel_get_property;
  object_class->set_property = cc_screen_panel_set_property;
  object_class->dispose = cc_screen_panel_dispose;
  object_class->finalize = cc_screen_panel_finalize;
}

static void
cc_screen_panel_class_finalize (CcScreenPanelClass *klass)
{
}

static void
cc_screen_panel_init (CcScreenPanel *self)
{
  GError     *error;
  GtkBuilder *builder;
  GtkWidget  *widget;

  self->priv = SCREEN_PANEL_PRIVATE (self);

  builder = gtk_builder_new ();

  error = NULL;
  gtk_builder_add_from_file (builder,
                             GNOMECC_UI_DIR "/screen.ui",
                             &error);

  if (error != NULL)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);

      g_object_unref (builder);

      return;
    }

  self->priv->lock_settings = g_settings_new ("org.gnome.desktop.interface");
  g_signal_connect (self->priv->lock_settings,
                    "changed",
                    G_CALLBACK (on_lock_settings_changed),
                    self);

  widget = WID (builder, "screen_vbox");

  gtk_widget_reparent (widget, (GtkWidget *) self);
}

void
cc_screen_panel_register (GIOModule *module)
{
  cc_screen_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_SCREEN_PANEL,
                                  "screen", 0);
}

