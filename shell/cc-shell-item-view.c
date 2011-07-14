/*
 * Copyright (c) 2010 Intel, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "cc-shell-item-view.h"
#include "cc-shell-model.h"
#include "cc-shell-marshal.h"

G_DEFINE_TYPE (CcShellItemView, cc_shell_item_view, GTK_TYPE_ICON_VIEW)

#define SHELL_ITEM_VIEW_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_SHELL_ITEM_VIEW, CcShellItemViewPrivate))

struct _CcShellItemViewPrivate
{
  gboolean ignore_release;
};


enum
{
  DESKTOP_ITEM_ACTIVATED,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0,};

static void
cc_shell_item_view_get_property (GObject    *object,
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
cc_shell_item_view_set_property (GObject      *object,
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
cc_shell_item_view_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_shell_item_view_parent_class)->dispose (object);
}

static void
cc_shell_item_view_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_shell_item_view_parent_class)->finalize (object);
}

static gboolean
iconview_button_press_event_cb (GtkWidget       *widget,
                                GdkEventButton  *event,
                                CcShellItemView *cc_view)
{
  /* be sure to ignore double and triple clicks */
  cc_view->priv->ignore_release = (event->type != GDK_BUTTON_PRESS);

  return FALSE;
}

static gboolean
iconview_button_release_event_cb (GtkWidget       *widget,
                                  GdkEventButton  *event,
                                  CcShellItemView *cc_view)
{
  CcShellItemViewPrivate *priv = cc_view->priv;

  if (event->button == 1 && !priv->ignore_release)
    {
      GList *selection;

      selection =
        gtk_icon_view_get_selected_items (GTK_ICON_VIEW (cc_view));

      if (!selection)
        return TRUE;

      gtk_icon_view_item_activated (GTK_ICON_VIEW (cc_view),
                                    (GtkTreePath*) selection->data);

      g_list_free (selection);
    }

  return TRUE;
}

static void
iconview_item_activated_cb (GtkIconView     *icon_view,
                            GtkTreePath     *path,
                            CcShellItemView *cc_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *name, *desktop_file, *id;

  model = gtk_icon_view_get_model (icon_view);

  gtk_icon_view_unselect_all (icon_view);

  /* get the iter and ensure it is valid */
  if (!gtk_tree_model_get_iter (model, &iter, path))
    return;

  gtk_tree_model_get (model, &iter,
                      COL_NAME, &name,
                      COL_DESKTOP_FILE, &desktop_file,
                      COL_ID, &id,
                      -1);

  g_signal_emit (cc_view, signals[DESKTOP_ITEM_ACTIVATED], 0,
                 name, id, desktop_file);

  g_free (desktop_file);
  g_free (name);
  g_free (id);
}

void
cc_shell_item_view_update_cells (CcShellItemView *view)
{
	GList *cells, *l;

	cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (view));
	for (l = cells ; l != NULL; l = l->next)
	{
		GtkCellRenderer *cell = l->data;

		if (GTK_IS_CELL_RENDERER_TEXT (cell)) {
			g_object_set (G_OBJECT (cell),
				      "wrap-mode", PANGO_WRAP_WORD,
				      NULL);
			/* We only have one text cell */
			break;
		}
	}
}

static void
cc_shell_item_view_class_init (CcShellItemViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcShellItemViewPrivate));

  object_class->get_property = cc_shell_item_view_get_property;
  object_class->set_property = cc_shell_item_view_set_property;
  object_class->dispose = cc_shell_item_view_dispose;
  object_class->finalize = cc_shell_item_view_finalize;

  signals[DESKTOP_ITEM_ACTIVATED] = g_signal_new ("desktop-item-activated",
                                                  CC_TYPE_SHELL_ITEM_VIEW,
                                                  G_SIGNAL_RUN_FIRST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  cc_shell_marshal_VOID__STRING_STRING_STRING,
                                                  G_TYPE_NONE,
                                                  3,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING,
                                                  G_TYPE_STRING);
}

static void
cc_shell_item_view_init (CcShellItemView *self)
{
  self->priv = SHELL_ITEM_VIEW_PRIVATE (self);

  g_object_set (self, "margin", 0, NULL);
  g_signal_connect (self, "item-activated",
                    G_CALLBACK (iconview_item_activated_cb), self);
  g_signal_connect (self, "button-press-event",
                    G_CALLBACK (iconview_button_press_event_cb), self);
  g_signal_connect (self, "button-release-event",
                    G_CALLBACK (iconview_button_release_event_cb), self);
}

GtkWidget *
cc_shell_item_view_new (void)
{
  return g_object_new (CC_TYPE_SHELL_ITEM_VIEW, NULL);
}
