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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-keyboard-panel.h"
#include "cc-keyboard-resources.h"

#include "keyboard-shortcuts.h"

struct _CcKeyboardPanel
{
  CcPanel             parent;

  GtkBuilder         *builder;
};

CC_PANEL_REGISTER (CcKeyboardPanel, cc_keyboard_panel)

enum {
  PROP_0,
  PROP_PARAMETERS
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

  notebook = GTK_WIDGET (gtk_builder_get_object (panel->builder, "keyboard_notebook"));
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
    case PROP_PARAMETERS: {
      GVariant *parameters, *v;
      const gchar *page, *section;

      parameters = g_value_get_variant (value);
      if (!parameters)
        break;
      page = section = NULL;
      switch (g_variant_n_children (parameters))
        {
          case 2:
            g_variant_get_child (parameters, 1, "v", &v);
            section = g_variant_get_string (v, NULL);
            g_variant_unref (v);
            /* fall-through */
          case 1:
            g_variant_get_child (parameters, 0, "v", &v);
            page = g_variant_get_string (v, NULL);
            g_variant_unref (v);
            cc_keyboard_panel_set_page (panel, page, section);
            /* fall-through */
          case 0:
            break;
          default:
            g_warning ("Unexpected parameters found, ignore request");
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
  GtkWidget *widget;

  obj = G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructor (gtype, n_properties, properties);

  self = CC_KEYBOARD_PANEL (obj);

  keyboard_shortcuts_init (CC_PANEL (self), self->builder);

  widget = (GtkWidget *) gtk_builder_get_object (self->builder, "shortcuts_page");

  gtk_container_add (GTK_CONTAINER (self), widget);

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
  keyboard_shortcuts_dispose (CC_PANEL (object));

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->dispose (object);
}

static void
cc_keyboard_panel_finalize (GObject *object)
{
  CcKeyboardPanel *panel = CC_KEYBOARD_PANEL (object);

  if (panel->builder)
    g_object_unref (panel->builder);

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

static void
cc_keyboard_panel_class_init (CcKeyboardPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_keyboard_panel_get_help_uri;

  object_class->constructor = cc_keyboard_panel_constructor;
  object_class->set_property = cc_keyboard_panel_set_property;
  object_class->dispose = cc_keyboard_panel_dispose;
  object_class->finalize = cc_keyboard_panel_finalize;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *self)
{
  GError *error = NULL;

  g_resources_register (cc_keyboard_get_resource ());

  self->builder = gtk_builder_new ();

  if (gtk_builder_add_from_resource (self->builder,
                                     "/org/gnome/control-center/keyboard/gnome-keyboard-panel.ui",
                                     &error) == 0)
    {
      g_warning ("Could not load UI: %s", error->message);
      g_clear_error (&error);
      g_object_unref (self->builder);
      self->builder = NULL;
    }
}
