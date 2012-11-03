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

CC_PANEL_REGISTER (CcKeyboardPanel, cc_keyboard_panel)

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

enum {
  PROP_0,
  PROP_ARGV
};

enum {
  TYPING_PAGE,
  SHORTCUTS_PAGE
};

static void
cc_keyboard_panel_set_page (CcKeyboardPanel *panel,
                            const gchar     *page,
                            const gchar     *section)
{
  GtkWidget *notebook;
  gint page_num;

  if (g_strcmp0 (page, "typing") == 0)
    page_num = TYPING_PAGE;
  else if (g_strcmp0 (page, "shortcuts") == 0)
    page_num = SHORTCUTS_PAGE;
  else {
    g_warning ("Could not switch to non-existent page '%s'", page);
    return;
  }

  notebook = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "keyboard_notebook"));
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);

  if (page_num == SHORTCUTS_PAGE &&
      section != NULL) {
    keyboard_shortcuts_set_section (CC_PANEL (panel), section);
  }
}

static void
cc_keyboard_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CcKeyboardPanel *panel = CC_KEYBOARD_PANEL (object);

  switch (property_id)
    {
    case PROP_ARGV: {
      gchar **args;

      args = g_value_get_boxed (value);

      if (args && args[0]) {
        cc_keyboard_panel_set_page (panel, args[0], args[1]);
      }
      break;
    }

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static GObject *
cc_keyboard_panel_constructor (GType                  gtype,
                               guint                  n_properties,
                               GObjectConstructParam *properties)
{
  GObject *obj;
  CcKeyboardPanel *self;
  CcKeyboardPanelPrivate *priv;
  GtkWidget *widget;

  obj = G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructor (gtype, n_properties, properties);

  self = CC_KEYBOARD_PANEL (obj);
  priv = self->priv;

  keyboard_general_init (CC_PANEL (self), priv->builder);
  keyboard_shortcuts_init (CC_PANEL (self), priv->builder);

  widget = (GtkWidget *) gtk_builder_get_object (priv->builder,
                                                 "keyboard_notebook");

  gtk_widget_reparent (widget, (GtkWidget *) self);

  return obj;
}

static const char *
cc_keyboard_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/keyboard";
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
  CcKeyboardPanel *panel = CC_KEYBOARD_PANEL (object);

  if (panel->priv->builder)
    g_object_unref (panel->priv->builder);

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

static void
cc_keyboard_panel_class_init (CcKeyboardPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcKeyboardPanelPrivate));

  panel_class->get_help_uri = cc_keyboard_panel_get_help_uri;

  object_class->constructor = cc_keyboard_panel_constructor;
  object_class->get_property = cc_keyboard_panel_get_property;
  object_class->set_property = cc_keyboard_panel_set_property;
  object_class->dispose = cc_keyboard_panel_dispose;
  object_class->finalize = cc_keyboard_panel_finalize;

  g_object_class_override_property (object_class, PROP_ARGV, "argv");
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *self)
{
  const gchar *uifile = GNOMECC_UI_DIR "/gnome-keyboard-panel.ui";
  CcKeyboardPanelPrivate *priv;
  GError *error = NULL;

  priv = self->priv = KEYBOARD_PANEL_PRIVATE (self);

  priv->builder = gtk_builder_new ();

  if (gtk_builder_add_from_file (priv->builder, uifile, &error) == 0)
    {
      g_warning ("Could not load UI: %s", error->message);
      g_clear_error (&error);
      g_object_unref (priv->builder);
      priv->builder = NULL;
    }
}

void
cc_keyboard_panel_register (GIOModule *module)
{
  cc_keyboard_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_KEYBOARD_PANEL,
                                  "keyboard", 0);
}

