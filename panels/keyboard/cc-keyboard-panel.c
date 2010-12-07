/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-keyboard-panel.h"
#include "keyboard-general.h"
#include "keyboard-shortcuts.h"

G_DEFINE_DYNAMIC_TYPE (CcKeyboardPanel, cc_keyboard_panel, CC_TYPE_PANEL)

#define KEYBOARD_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_KEYBOARD_PANEL, CcKeyboardPanelPrivate))

struct _CcKeyboardPanelPrivate
{
  GtkBuilder *builder;
};


static void
cc_keyboard_panel_get_property (GObject    *object,
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
cc_keyboard_panel_set_property (GObject      *object,
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
cc_keyboard_panel_dispose (GObject *object)
{
  keyboard_general_dispose (CC_PANEL (object));
  keyboard_shortcuts_dispose (CC_PANEL (object));

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->dispose (object);
}

static void
cc_keyboard_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

static GObject *
cc_keyboard_panel_constructor (GType                  gtype,
			       guint                  n_properties,
			       GObjectConstructParam *properties)
{
  GObject *obj;
  CcKeyboardPanel *self;
  CcKeyboardPanelPrivate *priv;
  GError *error = NULL;
  GtkWidget *widget;

  const gchar *uifile = GNOMECC_UI_DIR "/gnome-keyboard-panel.ui";

  obj = G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructor (gtype, n_properties, properties);

  self = CC_KEYBOARD_PANEL (obj);
  priv = self->priv = KEYBOARD_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  if (gtk_builder_add_from_file (priv->builder, uifile, &error) == 0)
    {
      g_warning ("Could not load UI: %s", error->message);
      g_clear_error (&error);
      g_object_unref (priv->builder);
      priv->builder = NULL;
      return obj;
    }

  keyboard_general_init (CC_PANEL (self), priv->builder);
  keyboard_shortcuts_init (CC_PANEL (self), priv->builder);

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
                                                 "keyboard_notebook");

  gtk_widget_reparent (widget, (GtkWidget *) self);

  return obj;
}

static void
cc_keyboard_panel_class_init (CcKeyboardPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcKeyboardPanelPrivate));

  object_class->constructor = cc_keyboard_panel_constructor;
  object_class->get_property = cc_keyboard_panel_get_property;
  object_class->set_property = cc_keyboard_panel_set_property;
  object_class->dispose = cc_keyboard_panel_dispose;
  object_class->finalize = cc_keyboard_panel_finalize;
}

static void
cc_keyboard_panel_class_finalize (CcKeyboardPanelClass *klass)
{
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *self)
{
}

void
cc_keyboard_panel_register (GIOModule *module)
{
  cc_keyboard_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_KEYBOARD_PANEL,
                                  "keyboard", 0);
}

