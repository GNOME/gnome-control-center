/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome-control-center/cc-shell.h>

#include "gnome-keyboard-panel.h"

#define MAX_ELEMENTS_BEFORE_SCROLLING 10
#define RESPONSE_ADD 0
#define RESPONSE_REMOVE 1

typedef struct {
  const char *key;
  gboolean found;
} KeyMatchData;

static gboolean
key_match (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  KeyMatchData *match_data = data;
  KeyEntry *element;

  gtk_tree_model_get (model, iter,
		      KEYENTRY_COLUMN, &element,
		      -1);

  if (element && g_strcmp0 (element->gconf_key, match_data->key) == 0)
    {
      match_data->found = TRUE;
      return TRUE;
    }

  return FALSE;
}

static gboolean
key_is_already_shown (GtkTreeModel *model, const KeyListEntry *entry)
{
  KeyMatchData data;

  data.key = entry->name;
  data.found = FALSE;
  gtk_tree_model_foreach (model, key_match, &data);

  return data.found;
}

static gboolean
count_rows_foreach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  gint *rows = data;

  (*rows)++;

  return FALSE;
}

static void
ensure_scrollbar (GtkBuilder *builder, int i)
{
  if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
    {
      GtkRequisition rectangle;
      GObject *actions_swindow = gtk_builder_get_object (builder,
                                                         "actions_swindow");
      GtkWidget *treeview = WID (builder,
                                                     "shortcut_treeview");
      gtk_widget_ensure_style (treeview);
      gtk_widget_size_request (treeview, &rectangle);
      gtk_widget_set_size_request (treeview, -1, rectangle.height);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (actions_swindow),
				      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    }
}
