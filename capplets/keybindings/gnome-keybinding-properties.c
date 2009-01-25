/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>
#include <X11/Xatom.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "wm-common.h"
#include "capplet-util.h"
#include "eggcellrendererkeys.h"
#include "activate-settings-daemon.h"

#define GCONF_BINDING_DIR "/desktop/gnome/keybindings"
#define MAX_ELEMENTS_BEFORE_SCROLLING 10
#define MAX_CUSTOM_SHORTCUTS 1000
#define RESPONSE_ADD 0
#define RESPONSE_REMOVE 1

typedef struct {
  char *name;
  /* The gettext package to use to translate the section title */
  char *package;
  /* Name of the window manager the keys would apply to */
  char *wm_name;
  /* an array of KeyListEntry */
  GArray *entries;
} KeyList;

typedef enum {
  COMPARISON_NONE = 0,
  COMPARISON_GT,
  COMPARISON_LT,
  COMPARISON_EQ
} Comparison;

typedef struct
{
  char *name;
  int value;
  char *key;
  char *description_name;
  char *cmd_name;
  Comparison comparison;
} KeyListEntry;

enum
{
  DESCRIPTION_COLUMN,
  KEYENTRY_COLUMN,
  N_COLUMNS
};

typedef struct
{
  char *gconf_key;
  guint keyval;
  guint keycode;
  EggVirtualModifierType mask;
  gboolean editable;
  GtkTreeModel *model;
  char *description;
  char *desc_gconf_key;
  gboolean desc_editable;
  char *command;
  char *cmd_gconf_key;
  gboolean cmd_editable;
  guint gconf_cnxn;
  guint gconf_cnxn_desc;
  guint gconf_cnxn_cmd;
} KeyEntry;

static gboolean block_accels = FALSE;
static GtkWidget *custom_shortcut_dialog = NULL;
static GtkWidget *custom_shortcut_name_entry = NULL;
static GtkWidget *custom_shortcut_command_entry = NULL;

static GladeXML *
create_dialog (void)
{
  return glade_xml_new (GNOMECC_GLADE_DIR "/gnome-keybinding-properties.glade",
                        NULL, NULL);
}

static char*
binding_name (guint                   keyval,
	      guint		      keycode,
              EggVirtualModifierType  mask,
              gboolean                translate)
{
  if (keyval != 0 || keycode != 0)
    return translate ?
	egg_virtual_accelerator_label (keyval, keycode, mask) :
	egg_virtual_accelerator_name (keyval, keycode, mask);
  else
    return g_strdup (translate ? _("Disabled") : "");
}

static gboolean
binding_from_string (const char             *str,
                     guint                  *accelerator_key,
		     guint		    *keycode,
                     EggVirtualModifierType *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);

  if (str == NULL || strcmp (str, "disabled") == 0)
    {
      *accelerator_key = 0;
      *keycode = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  egg_accelerator_parse_virtual (str, accelerator_key, keycode, accelerator_mods);

  if (*accelerator_key == 0)
    return FALSE;
  else
    return TRUE;
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  KeyEntry *key_entry;

  gtk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key_entry,
                      -1);

  if (key_entry == NULL)
    g_object_set (cell,
		  "visible", FALSE,
		  NULL);
  else if (! key_entry->editable)
    g_object_set (cell,
		  "visible", TRUE,
		  "editable", FALSE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_ITALIC,
		  NULL);
  else
    g_object_set (cell,
		  "visible", TRUE,
		  "editable", TRUE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_NORMAL,
		  NULL);
}

static void
description_set_func (GtkTreeViewColumn *tree_column,
                      GtkCellRenderer   *cell,
                      GtkTreeModel      *model,
                      GtkTreeIter       *iter,
                      gpointer           data)
{
  KeyEntry *key_entry;

  gtk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key_entry,
                      -1);

  if (key_entry != NULL)
    g_object_set (cell,
		  "editable", FALSE,
		  "text", key_entry->description != NULL ?
			  key_entry->description : _("<Unknown Action>"),
		  NULL);
  else
    g_object_set (cell,
		  "editable", FALSE, NULL);
}

static gboolean
keybinding_key_changed_foreach (GtkTreeModel *model,
				GtkTreePath  *path,
				GtkTreeIter  *iter,
				gpointer      user_data)
{
  KeyEntry *key_entry;
  KeyEntry *tmp_key_entry;

  key_entry = (KeyEntry *)user_data;
  gtk_tree_model_get (key_entry->model, iter,
		      KEYENTRY_COLUMN, &tmp_key_entry,
		      -1);

  if (key_entry == tmp_key_entry)
    {
      gtk_tree_model_row_changed (key_entry->model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
keybinding_key_changed (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *) user_data;
  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;

  binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
  key_entry->editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
keybinding_description_changed (GConfClient *client,
				guint        cnxn_id,
				GConfEntry  *entry,
				gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *) user_data;
  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;

  g_free (key_entry->description);
  key_entry->description = key_value ? g_strdup (key_value) : NULL;
  key_entry->desc_editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
keybinding_command_changed (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *) user_data;
  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;

  g_free (key_entry->command);
  key_entry->command = key_value ? g_strdup (key_value) : NULL;
  key_entry->cmd_editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static int
keyentry_sort_func (GtkTreeModel *model,
                    GtkTreeIter  *a,
                    GtkTreeIter  *b,
                    gpointer      user_data)
{
  KeyEntry *key_entry_a;
  KeyEntry *key_entry_b;
  int retval;

  key_entry_a = NULL;
  gtk_tree_model_get (model, a,
                      KEYENTRY_COLUMN, &key_entry_a,
                      -1);

  key_entry_b = NULL;
  gtk_tree_model_get (model, b,
                      KEYENTRY_COLUMN, &key_entry_b,
                      -1);

  if (key_entry_a && key_entry_b)
    {
      if ((key_entry_a->keyval || key_entry_a->keycode) &&
          (key_entry_b->keyval || key_entry_b->keycode))
        {
          gchar *name_a, *name_b;

          name_a = binding_name (key_entry_a->keyval,
                                 key_entry_a->keycode,
                                 key_entry_a->mask,
                                 TRUE);

          name_b = binding_name (key_entry_b->keyval,
                                 key_entry_b->keycode,
                                 key_entry_b->mask,
                                 TRUE);

          retval = g_utf8_collate (name_a, name_b);

          g_free (name_a);
          g_free (name_b);
        }
      else if (key_entry_a->keyval || key_entry_a->keycode)
        retval = -1;
      else if (key_entry_b->keyval || key_entry_b->keycode)
        retval = 1;
      else
        retval = 0;
    }
  else if (key_entry_a)
    retval = -1;
  else if (key_entry_b)
    retval = 1;
  else
    retval = 0;

  return retval;
}

static void
clear_old_model (GladeXML *dialog)
{
  GtkWidget *tree_view;
  GtkTreeModel *model;

  tree_view = WID ("shortcut_treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  if (model == NULL)
    {
      /* create a new model */
      model = (GtkTreeModel *) gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

      gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model),
                                       KEYENTRY_COLUMN,
                                       keyentry_sort_func,
                                       NULL, NULL);

      gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), model);

      g_object_unref (model);
    }
  else
    {
      /* clear the existing model */
      GConfClient *client;
      gboolean valid;
      GtkTreeIter iter;
      KeyEntry *key_entry;

      client = gconf_client_get_default ();
      /* we need the schema name below;
       * cached values do not have that set, though */
      gconf_client_clear_cache (client);

      for (valid = gtk_tree_model_get_iter_first (model, &iter);
           valid;
           valid = gtk_tree_model_iter_next (model, &iter))
        {
          gtk_tree_model_get (model, &iter,
                              KEYENTRY_COLUMN, &key_entry,
                              -1);

          if (key_entry != NULL)
            {
              gconf_client_remove_dir (client, key_entry->gconf_key, NULL);
              gconf_client_notify_remove (client, key_entry->gconf_cnxn);
              if (key_entry->gconf_cnxn_desc != 0)
                gconf_client_notify_remove (client, key_entry->gconf_cnxn_desc);
              if (key_entry->gconf_cnxn_cmd != 0)
                gconf_client_notify_remove (client, key_entry->gconf_cnxn_cmd);
              g_free (key_entry->gconf_key);
              g_free (key_entry->description);
              g_free (key_entry->desc_gconf_key);
              g_free (key_entry->command);
              g_free (key_entry->cmd_gconf_key);
              g_free (key_entry);
            }
        }

      gtk_tree_store_clear (GTK_TREE_STORE (model));
      g_object_unref (client);
    }

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (WID ("actions_swindow")),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_usize (WID ("actions_swindow"), -1, -1);
}

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
should_show_key (const KeyListEntry *entry)
{
  int value;
  GConfClient *client;

  if (entry->comparison == COMPARISON_NONE)
    return TRUE;

  g_return_val_if_fail (entry->key != NULL, FALSE);

  client = gconf_client_get_default();
  value = gconf_client_get_int (client, entry->key, NULL);
  g_object_unref (client);

  switch (entry->comparison) {
  case COMPARISON_NONE:
    /* For compiler warnings */
    g_assert_not_reached ();
    return FALSE;
  case COMPARISON_GT:
    if (value > entry->value)
      return TRUE;
    break;
  case COMPARISON_LT:
    if (value < entry->value)
      return TRUE;
    break;
  case COMPARISON_EQ:
    if (value == entry->value)
      return TRUE;
    break;
  }

  return FALSE;
}

static gboolean
count_rows_foreach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  gint *rows = data;

  (*rows)++;

  return FALSE;
}

static void
ensure_scrollbar (GladeXML *dialog, int i)
{
  if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
    {
      GtkRequisition rectangle;
      gtk_widget_ensure_style (WID ("shortcut_treeview"));
      gtk_widget_size_request (WID ("shortcut_treeview"), &rectangle);
      gtk_widget_set_size_request (WID ("shortcut_treeview"), -1, rectangle.height);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (WID ("actions_swindow")),
				      GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    }
}

static void
find_section (GtkTreeModel *model,
              GtkTreeIter  *iter,
	      const char   *title)
{
  gint i, j;
  gboolean found;

  i = gtk_tree_model_iter_n_children (model, NULL);
  found = FALSE;
  gtk_tree_model_get_iter_first (model, iter);
  for (j = 0; j < i; j++)
    {
      char *description = NULL;

      gtk_tree_model_iter_next (model, iter);
      gtk_tree_model_get (model, iter,
			  DESCRIPTION_COLUMN, &description,
			  -1);
      if (g_strcmp0 (description, title) == 0)
        {
	  found = TRUE;
     	  break;
        }
    }
  if (!found)
    {
      gtk_tree_store_append (GTK_TREE_STORE (model), iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			  DESCRIPTION_COLUMN, title,
			  -1);
    }
}

static void
append_keys_to_tree (GladeXML           *dialog,
		     const gchar        *title,
		     const KeyListEntry *keys_list)
{
  GConfClient *client;
  GtkTreeIter parent_iter, iter;
  GtkTreeModel *model;
  gint i, j;
  gint rows_before;

  client = gconf_client_get_default ();
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("shortcut_treeview")));

  /* Try to find a section parent iter, if it already exists */
  find_section (model, &iter, title);
  parent_iter = iter;

  i = 0;
  gtk_tree_model_foreach (model, count_rows_foreach, &i);

  /* If the header we just added is the MAX_ELEMENTS_BEFORE_SCROLLING th,
   * then we need to scroll now */
  ensure_scrollbar (dialog, i - 1);

  rows_before = i;
  for (j = 0; keys_list[j].name != NULL; j++)
    {
      GConfEntry *entry;
      KeyEntry *key_entry;
      const gchar *key_string;
      gchar *key_value;
      gchar *description;
      gchar *command;

      if (!should_show_key (&keys_list[j]))
	continue;

      if (key_is_already_shown (model, &keys_list[j]))
	continue;

      key_string = keys_list[j].name;

      entry = gconf_client_get_entry (client,
                                      key_string,
				      NULL,
				      TRUE,
				      NULL);
      if (entry == NULL)
	{
	  /* We don't actually want to popup a dialog - just skip this one */
	  continue;
	}

      if (keys_list[j].description_name != NULL)
        {
          description = gconf_client_get_string (client, keys_list[j].description_name, NULL);
        }
      else
        {
          description = NULL;

          if (gconf_entry_get_schema_name (entry))
            {
              GConfSchema *schema;

              schema = gconf_client_get_schema (client,
                                                gconf_entry_get_schema_name (entry),
                                                NULL);
              if (schema != NULL)
                {
                  description = g_strdup (gconf_schema_get_short_desc (schema));
                  gconf_schema_free (schema);
                }
            }
        }

      if (keys_list[j].cmd_name != NULL)
        {
          command = gconf_client_get_string (client, keys_list[j].cmd_name, NULL);
        }
      else
        {
          command = NULL;
        }

      key_entry = g_new0 (KeyEntry, 1);
      key_entry->gconf_key = g_strdup (key_string);
      key_entry->editable = gconf_entry_get_is_writable (entry);
      key_entry->model = model;
      key_entry->description = description;
      key_entry->command = command;
      if (keys_list[j].description_name != NULL)
        {
          key_entry->desc_gconf_key =  g_strdup (keys_list[j].description_name);
          key_entry->desc_editable = gconf_client_key_is_writable (client, key_entry->desc_gconf_key, NULL);
          key_entry->gconf_cnxn_desc = gconf_client_notify_add (client,
								key_entry->desc_gconf_key,
								(GConfClientNotifyFunc) &keybinding_description_changed,
								key_entry, NULL, NULL);
        }
      if (keys_list[j].cmd_name != NULL)
        {
          key_entry->cmd_gconf_key =  g_strdup (keys_list[j].cmd_name);
          key_entry->cmd_editable = gconf_client_key_is_writable (client, key_entry->cmd_gconf_key, NULL);
          key_entry->gconf_cnxn_cmd = gconf_client_notify_add (client,
							       key_entry->cmd_gconf_key,
								(GConfClientNotifyFunc) &keybinding_command_changed,
								key_entry, NULL, NULL);
        }

      gconf_client_add_dir (client, key_string, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
      key_entry->gconf_cnxn = gconf_client_notify_add (client,
						       key_string,
						       (GConfClientNotifyFunc) &keybinding_key_changed,
						       key_entry, NULL, NULL);

      key_value = gconf_client_get_string (client, key_string, NULL);
      binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
      g_free (key_value);

      gconf_entry_free (entry);
      ensure_scrollbar (dialog, i);

      ++i;
      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent_iter);
      /* we use the DESCRIPTION_COLUMN only for the section headers */
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			  KEYENTRY_COLUMN, key_entry,
			  -1);
      gtk_tree_view_expand_all (GTK_TREE_VIEW (WID ("shortcut_treeview")));
    }

  g_object_unref (client);

  /* Don't show an empty section */
  if (i == rows_before)
    gtk_tree_store_remove (GTK_TREE_STORE (model), &parent_iter);

  if (i == 0)
      gtk_widget_hide (WID ("shortcuts_vbox"));
  else
      gtk_widget_show (WID ("shortcuts_vbox"));
}

static void
parse_start_tag (GMarkupParseContext *ctx,
		 const gchar         *element_name,
		 const gchar        **attr_names,
		 const gchar        **attr_values,
		 gpointer             user_data,
		 GError             **error)
{
  KeyList *keylist = (KeyList *) user_data;
  KeyListEntry key;
  const char *name, *gconf_key;
  int value;
  Comparison comparison;

  name = NULL;

  /* The top-level element, names the section in the tree */
  if (g_str_equal (element_name, "KeyListEntries"))
    {
      const char *wm_name = NULL;
      const char *package = NULL;

      while (*attr_names && *attr_values)
        {
	  if (g_str_equal (*attr_names, "name"))
	    {
	      if (**attr_values)
		name = *attr_values;
	    } else if (g_str_equal (*attr_names, "wm_name")) {
	      if (**attr_values)
		wm_name = *attr_values;
	    } else if (g_str_equal (*attr_names, "package")) {
	      if (**attr_values)
		package = *attr_values;
	    }
	  ++attr_names;
	  ++attr_values;
	}

      if (name)
        {
	  if (keylist->name)
	    g_warning ("Duplicate section name");
	  g_free (keylist->name);
	  keylist->name = g_strdup (name);
	}
      if (wm_name)
        {
	  if (keylist->wm_name)
	    g_warning ("Duplicate window manager name");
	  g_free (keylist->wm_name);
	  keylist->wm_name = g_strdup (wm_name);
	}
      if (package)
        {
	  if (keylist->package)
	    g_warning ("Duplicate gettext package name");
	  g_free (keylist->package);
	  keylist->package = g_strdup (package);
	}
      return;
    }

  if (!g_str_equal (element_name, "KeyListEntry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  value = 0;
  comparison = COMPARISON_NONE;
  gconf_key = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "name"))
        {
	  /* skip if empty */
	  if (**attr_values)
	    name = *attr_values;
	} else if (g_str_equal (*attr_names, "value")) {
	  if (**attr_values) {
	    value = (int) g_ascii_strtoull (*attr_values, NULL, 0);
	  }
	} else if (g_str_equal (*attr_names, "key")) {
	  if (**attr_values) {
	    gconf_key = *attr_values;
	  }
	} else if (g_str_equal (*attr_names, "comparison")) {
	  if (**attr_values) {
	    if (g_str_equal (*attr_values, "gt")) {
	      comparison = COMPARISON_GT;
	    } else if (g_str_equal (*attr_values, "lt")) {
	      comparison = COMPARISON_LT;
	    } else if (g_str_equal (*attr_values, "eq")) {
	      comparison = COMPARISON_EQ;
	    }
	  }
	}

      ++attr_names;
      ++attr_values;
    }

  if (name == NULL)
    return;

  key.name = g_strdup (name);
  key.description_name = NULL;
  key.value = value;
  if (gconf_key)
    key.key = g_strdup (gconf_key);
  else
    key.key = NULL;
  key.comparison = comparison;
  key.cmd_name = NULL;
  g_array_append_val (keylist->entries, key);
}

static void
append_keys_to_tree_from_file (GladeXML   *dialog,
			       const char *filename,
			       const char *wm_name)
{
  GMarkupParseContext *ctx;
  GMarkupParser parser = { parse_start_tag, NULL, NULL, NULL, NULL };
  KeyList *keylist;
  KeyListEntry key, *keys;
  GError *err = NULL;
  char *buf;
  const char *title;
  gsize buf_len;
  guint i;

  if (!g_file_get_contents (filename, &buf, &buf_len, &err))
    return;

  keylist = g_new0 (KeyList, 1);
  keylist->entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));
  ctx = g_markup_parse_context_new (&parser, 0, keylist, NULL);

  if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
    {
      g_warning ("Failed to parse '%s': '%s'", filename, err->message);
      g_error_free (err);
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      for (i = 0; i < keylist->entries->len; i++)
	g_free (((KeyListEntry *) &(keylist->entries->data[i]))->name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      keylist = NULL;
    }
  g_markup_parse_context_free (ctx);
  g_free (buf);

  if (keylist == NULL)
    return;

  /* If there's no keys to add, or the settings apply to a window manager
   * that's not the one we're running */
  if (keylist->entries->len == 0
      || (keylist->wm_name != NULL && !g_str_equal (wm_name, keylist->wm_name))
      || keylist->name == NULL)
    {
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return;
    }

  /* Empty KeyListEntry to end the array */
  key.name = NULL;
  key.description_name = NULL;
  key.key = NULL;
  key.value = 0;
  key.comparison = COMPARISON_NONE;
  g_array_append_val (keylist->entries, key);

  keys = (KeyListEntry *) g_array_free (keylist->entries, FALSE);
  if (keylist->package)
    {
      bind_textdomain_codeset (keylist->package, "UTF-8");
      title = dgettext (keylist->package, keylist->name);
    } else {
      title = _(keylist->name);
    }

  append_keys_to_tree (dialog, title, keys);

  g_free (keylist->name);
  g_free (keylist->package);
  for (i = 0; keys[i].name != NULL; i++)
    g_free (keys[i].name);
  g_free (keylist);
}

static void
append_keys_to_tree_from_gconf (GladeXML *dialog, const gchar *gconf_path)
{
  GConfClient *client;
  GSList *custom_list, *l;
  GArray *entries;
  KeyListEntry key;

  /* load custom shortcuts from GConf */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  key.key = NULL;
  key.value = 0;
  key.comparison = COMPARISON_NONE;

  client = gconf_client_get_default ();
  custom_list = gconf_client_all_dirs (client, gconf_path, NULL);

  for (l = custom_list; l != NULL; l = l->next)
    {
      key.name = g_strconcat (l->data, "/binding", NULL);
      key.cmd_name = g_strconcat (l->data, "/action", NULL);
      key.description_name = g_strconcat (l->data, "/name", NULL);
      g_array_append_val (entries, key);

      g_free (l->data);
    }

  g_slist_free (custom_list);
  g_object_unref (client);

  if (entries->len > 0)
    {
      KeyListEntry *keys;
      int i;

      /* Empty KeyListEntry to end the array */
      key.name = NULL;
      key.description_name = NULL;
      g_array_append_val (entries, key);

      keys = (KeyListEntry *) entries->data;
      append_keys_to_tree (dialog, _("Custom Shortcuts"), keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
          g_free (keys[i].description_name);
        }
    }

  g_array_free (entries, TRUE);
}

static void
reload_key_entries (gpointer wm_name, GladeXML *dialog)
{
  GDir *dir;
  const char *name;
  GList *list, *l;

  clear_old_model (dialog);

  dir = g_dir_open (GNOMECC_KEYBINDINGS_DIR, 0, NULL);
  if (!dir)
      return;

  list = NULL;
  for (name = g_dir_read_name (dir) ; name ; name = g_dir_read_name (dir))
    {
      if (g_str_has_suffix (name, ".xml"))
        {
	  list = g_list_insert_sorted (list, g_strdup (name),
				       (GCompareFunc) g_ascii_strcasecmp);
	}
    }
  g_dir_close (dir);

  for (l = list; l != NULL; l = l->next)
    {
        gchar *path;

	path = g_build_filename (GNOMECC_KEYBINDINGS_DIR, l->data, NULL);
        append_keys_to_tree_from_file (dialog, path, wm_name);
	g_free (l->data);
	g_free (path);
    }
  g_list_free (list);

  /* Load custom shortcuts _after_ system-provided ones,
   * since some of the custom shortcuts may also be listed
   * in a file. Loading the custom shortcuts last makes
   * such keys not show up in the custom section.
   */
  append_keys_to_tree_from_gconf (dialog, GCONF_BINDING_DIR);
}

static void
key_entry_controlling_key_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   gpointer     user_data)
{
  gchar *wm_name = wm_common_get_current_window_manager();
  reload_key_entries (wm_name, user_data);
  g_free (wm_name);
}

static gboolean
cb_check_for_uniqueness (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 KeyEntry *new_key)
{
  KeyEntry *element;

  gtk_tree_model_get (new_key->model, iter,
		      KEYENTRY_COLUMN, &element,
		      -1);

  /* no conflict for : blanks, different modifiers, or ourselves */
  if (element == NULL || new_key->mask != element->mask ||
      !strcmp (new_key->gconf_key, element->gconf_key))
    return FALSE;

  if (new_key->keyval != 0) {
      if (new_key->keyval != element->keyval)
	  return FALSE;
  } else if (element->keyval != 0 || new_key->keycode != element->keycode)
    return FALSE;

  new_key->editable = FALSE;
  new_key->gconf_key = element->gconf_key;
  new_key->description = element->description;
  new_key->desc_gconf_key = element->desc_gconf_key;
  new_key->desc_editable = element->desc_editable;
  return TRUE;
}

static const guint forbidden_keyvals[] = {
  /* Navigation keys */
  GDK_Home,
  GDK_Left,
  GDK_Up,
  GDK_Right,
  GDK_Down,
  GDK_Page_Up,
  GDK_Page_Down,
  GDK_End,
  GDK_Tab,

  /* Return */
  GDK_KP_Enter,
  GDK_Return,

  GDK_space,
  GDK_Mode_switch
};

static gboolean
keyval_is_forbidden (guint keyval)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(forbidden_keyvals); i++) {
    if (keyval == forbidden_keyvals[i])
      return TRUE;
  }

  return FALSE;
}

static void
show_error (GtkWindow *parent,
	    GError *err)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
				   GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				   GTK_MESSAGE_WARNING,
				   GTK_BUTTONS_OK,
				   _("Error saving the new shortcut"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s", err->message);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void
accel_edited_callback (GtkCellRendererText   *cell,
                       const char            *path_string,
                       guint                  keyval,
                       EggVirtualModifierType mask,
		       guint		      keycode,
                       gpointer               data)
{
  GConfClient *client;
  GtkTreeView *view = (GtkTreeView *)data;
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  KeyEntry *key_entry, tmp_key;
  GError *err = NULL;
  char *str;

  block_accels = FALSE;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
		      KEYENTRY_COLUMN, &key_entry,
		      -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  mask &= ~EGG_VIRTUAL_LOCK_MASK;

  tmp_key.model  = model;
  tmp_key.keyval = keyval;
  tmp_key.keycode = keycode;
  tmp_key.mask   = mask;
  tmp_key.gconf_key = key_entry->gconf_key;
  tmp_key.description = NULL;
  tmp_key.editable = TRUE; /* kludge to stuff in a return flag */

  if (keyval != 0 || keycode != 0) /* any number of keys can be disabled */
    gtk_tree_model_foreach (model,
      (GtkTreeModelForeachFunc) cb_check_for_uniqueness,
      &tmp_key);

  /* Check for unmodified keys */
  if (tmp_key.mask == 0 && tmp_key.keycode != 0)
    {
      if ((tmp_key.keyval >= GDK_a && tmp_key.keyval <= GDK_z)
	   || (tmp_key.keyval >= GDK_A && tmp_key.keyval <= GDK_Z)
	   || (tmp_key.keyval >= GDK_0 && tmp_key.keyval <= GDK_9)
	   || (tmp_key.keyval >= GDK_kana_fullstop && tmp_key.keyval <= GDK_semivoicedsound)
	   || (tmp_key.keyval >= GDK_Arabic_comma && tmp_key.keyval <= GDK_Arabic_sukun)
	   || (tmp_key.keyval >= GDK_Serbian_dje && tmp_key.keyval <= GDK_Cyrillic_HARDSIGN)
	   || (tmp_key.keyval >= GDK_Greek_ALPHAaccent && tmp_key.keyval <= GDK_Greek_omega)
	   || (tmp_key.keyval >= GDK_hebrew_doublelowline && tmp_key.keyval <= GDK_hebrew_taf)
	   || (tmp_key.keyval >= GDK_Thai_kokai && tmp_key.keyval <= GDK_Thai_lekkao)
	   || (tmp_key.keyval >= GDK_Hangul && tmp_key.keyval <= GDK_Hangul_Special)
	   || (tmp_key.keyval >= GDK_Hangul_Kiyeog && tmp_key.keyval <= GDK_Hangul_J_YeorinHieuh)
	   || keyval_is_forbidden (tmp_key.keyval)) {
        GtkWidget *dialog;
	char *name;

	name = binding_name (keyval, keycode, mask, TRUE);

	dialog =
	  gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
			  	  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				  GTK_MESSAGE_WARNING,
				  GTK_BUTTONS_CANCEL,
				  _("The shortcut \"%s\" cannot be used because it will become impossible to type using this key.\n"
				  "Please try with a key such as Control, Alt or Shift at the same time."),
				  name);

	g_free (name);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* set it back to its previous value. */
	egg_cell_renderer_keys_set_accelerator
	  (EGG_CELL_RENDERER_KEYS (cell),
	   key_entry->keyval, key_entry->keycode, key_entry->mask);
	return;
      }
    }

  /* flag to see if the new accelerator was in use by something */
  if (!tmp_key.editable)
    {
      GtkWidget *dialog;
      char *name;
      int response;

      name = binding_name (keyval, keycode, mask, TRUE);

      dialog =
	gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				GTK_MESSAGE_WARNING,
				GTK_BUTTONS_CANCEL,
				_("The shortcut \"%s\" is already used for\n\"%s\""),
				name, tmp_key.description ?
				tmp_key.description : tmp_key.gconf_key);
      g_free (name);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
	  _("If you reassign the shortcut to \"%s\", the \"%s\" shortcut "
	    "will be disabled."),
	  key_entry->description ?
	  key_entry->description : key_entry->gconf_key,
	  tmp_key.description ?
	  tmp_key.description : tmp_key.gconf_key);

      gtk_dialog_add_button (GTK_DIALOG (dialog),
                             _("_Reassign"),
			     GTK_RESPONSE_ACCEPT);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog),
				       GTK_RESPONSE_ACCEPT);

      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_ACCEPT)
	{
	  GConfClient *client;

	  client = gconf_client_get_default ();

          gconf_client_set_string (client,
				   tmp_key.gconf_key,
				   "", &err);

	  if (err != NULL)
	    {
	       show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
			   err);
	       g_error_free (err);
	       g_object_unref (client);
	       return;
	    }

	  str = binding_name (keyval, keycode, mask, FALSE);
	  gconf_client_set_string (client,
				   key_entry->gconf_key,
				   str, &err);

	  if (err != NULL)
	    {
	       show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
			   err);
	       g_error_free (err);

	       /* reset the previous shortcut */
	       gconf_client_set_string (client,
					tmp_key.gconf_key,
					str, NULL);
	    }

	  g_free (str);
	  g_object_unref (client);
	}
      else
	{
	  /* set it back to its previous value. */
	  egg_cell_renderer_keys_set_accelerator (EGG_CELL_RENDERER_KEYS (cell),
						  key_entry->keyval,
						  key_entry->keycode,
						  key_entry->mask);
	}

      return;
    }

  str = binding_name (keyval, keycode, mask, FALSE);

  client = gconf_client_get_default ();
  gconf_client_set_string (client,
                           key_entry->gconf_key,
                           str,
                           &err);
  g_free (str);
  g_object_unref (client);

  if (err != NULL)
    {
      show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))), err);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}

static void
accel_cleared_callback (GtkCellRendererText *cell,
			const char          *path_string,
			gpointer             data)
{
  GConfClient *client;
  GtkTreeView *view = (GtkTreeView *) data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  KeyEntry *key_entry;
  GtkTreeIter iter;
  GError *err = NULL;
  GtkTreeModel *model;

  block_accels = FALSE;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
		      KEYENTRY_COLUMN, &key_entry,
		      -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  /* Unset the key */
  client = gconf_client_get_default();
  gconf_client_set_string (client,
			   key_entry->gconf_key,
			   "",
			   &err);
  g_object_unref (client);

  if (err != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       _("Error unsetting accelerator in configuration database: %s"),
				       err->message);
      gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}

static void
description_edited_callback (GtkCellRendererText *renderer,
                             gchar               *path_string,
                             gchar               *new_text,
                             gpointer             user_data)
{
  GConfClient *client;
  GtkTreeView *view = GTK_TREE_VIEW (user_data);
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  KeyEntry *key_entry;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter,
		      KEYENTRY_COLUMN, &key_entry,
		      -1);

  /* sanity check */
  if (key_entry == NULL || key_entry->desc_gconf_key == NULL)
    return;

  client = gconf_client_get_default ();
  if (!gconf_client_set_string (client, key_entry->desc_gconf_key, new_text, NULL))
    key_entry->desc_editable = FALSE;

  g_object_unref (client);
}


typedef struct
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GtkTreeViewColumn *column;
} IdleData;

static gboolean
real_start_editing_cb (IdleData *idle_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (idle_data->tree_view));
  gtk_tree_view_set_cursor (idle_data->tree_view,
			    idle_data->path,
			    idle_data->column,
			    TRUE);
  gtk_tree_path_free (idle_data->path);
  g_free (idle_data);
  return FALSE;
}

static gboolean
edit_custom_shortcut (KeyEntry *key)
{
  gint result;
  const gchar *text;
  gboolean ret;

  gtk_entry_set_text (GTK_ENTRY (custom_shortcut_name_entry), key->description);
  gtk_widget_set_sensitive (custom_shortcut_name_entry, key->desc_editable);
  gtk_entry_set_text (GTK_ENTRY (custom_shortcut_command_entry), key->command);
  gtk_widget_set_sensitive (custom_shortcut_command_entry, key->cmd_editable);

  gtk_window_present (GTK_WINDOW (custom_shortcut_dialog));
  result = gtk_dialog_run (GTK_DIALOG (custom_shortcut_dialog));
  switch (result)
    {
    case GTK_RESPONSE_OK:
      text = gtk_entry_get_text (GTK_ENTRY (custom_shortcut_name_entry));
      g_free (key->description);
      key->description = g_strdup (text);
      text = gtk_entry_get_text (GTK_ENTRY (custom_shortcut_command_entry));
      g_free (key->command);
      key->command = g_strdup (text);
      ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
    }

  gtk_widget_hide (custom_shortcut_dialog);

  return ret;
}

static gboolean
remove_custom_shortcut (GtkTreeModel *model, GtkTreeIter *iter)
{
  GtkTreeIter parent;
  GConfClient *client;
  gchar *base;
  KeyEntry *key;

  gtk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key,
                      -1);

  /* not a custom shortcut */
  if (key->command == NULL)
    return FALSE;

  client = gconf_client_get_default ();

  gconf_client_notify_remove (client, key->gconf_cnxn);
  if (key->gconf_cnxn_desc != 0)
    gconf_client_notify_remove (client, key->gconf_cnxn_desc);
  if (key->gconf_cnxn_cmd != 0)
    gconf_client_notify_remove (client, key->gconf_cnxn_cmd);

  base = g_path_get_dirname (key->gconf_key);
  gconf_client_recursive_unset (client, base, 0, NULL);
  g_free (base);
  g_object_unref (client);

  g_free (key->gconf_key);
  g_free (key->description);
  g_free (key->desc_gconf_key);
  g_free (key->command);
  g_free (key->cmd_gconf_key);
  g_free (key);

  gtk_tree_model_iter_parent (model, &parent, iter);
  gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
  if (!gtk_tree_model_iter_has_child (model, &parent))
    gtk_tree_store_remove (GTK_TREE_STORE (model), &parent);

  return TRUE;
}

static void
update_custom_shortcut (GtkTreeModel *model, GtkTreeIter *iter)
{
  KeyEntry *key;

  gtk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key,
                      -1);

  edit_custom_shortcut (key);
  if (key->command == NULL || key->command[0] == '\0')
    remove_custom_shortcut (model, iter);
  else
    gtk_tree_store_set (GTK_TREE_STORE (model), iter,
	                KEYENTRY_COLUMN, key, -1);
}

static gchar *
find_free_gconf_key (GError **error)
{
  GConfClient *client;

  gchar *dir;
  int i;

  client = gconf_client_get_default ();

  for (i = 0; i < MAX_CUSTOM_SHORTCUTS; i++)
    {
      dir = g_strdup_printf ("%s/custom%d", GCONF_BINDING_DIR, i);
      if (!gconf_client_dir_exists (client, dir, NULL))
        break;
      g_free (dir);
    }

  if (i == MAX_CUSTOM_SHORTCUTS)
    {
      dir = NULL;
      g_set_error_literal (error,
                           g_quark_from_string ("Keyboard Shortcuts"),
                           0,
                           _("Too many custom shortcuts"));
    }

  g_object_unref (client);

  return dir;
}

static void
add_custom_shortcut (GtkTreeView  *tree_view,
                     GtkTreeModel *model)
{
  KeyEntry *key_entry;
  GtkTreeIter iter;
  GtkTreeIter parent_iter;
  GtkTreePath *path;
  gchar *dir;
  GConfClient *client;
  GError *error;

  error = NULL;
  dir = find_free_gconf_key (&error);
  if (dir == NULL)
    {
      show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tree_view))), error);

      g_error_free (error);
      return;
    }

  key_entry = g_new0 (KeyEntry, 1);
  key_entry->gconf_key = g_strconcat (dir, "/binding", NULL);
  key_entry->editable = TRUE;
  key_entry->model = model;
  key_entry->desc_gconf_key = g_strconcat (dir, "/name", NULL);
  key_entry->description = g_strdup ("");
  key_entry->desc_editable = TRUE;
  key_entry->cmd_gconf_key = g_strconcat (dir, "/action", NULL);
  key_entry->command = g_strdup ("");
  key_entry->cmd_editable = TRUE;
  g_free (dir);

  if (edit_custom_shortcut (key_entry) &&
      key_entry->command && key_entry->command[0])
    {
      find_section (model, &iter, _("Custom Shortcuts"));
      parent_iter = iter;
      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent_iter);
      gtk_tree_store_set (GTK_TREE_STORE (model), &iter, KEYENTRY_COLUMN, key_entry, -1);

      /* store in gconf */
      client = gconf_client_get_default ();
      gconf_client_set_string (client, key_entry->gconf_key, "", NULL);
      gconf_client_set_string (client, key_entry->desc_gconf_key, key_entry->description, NULL);
      gconf_client_set_string (client, key_entry->cmd_gconf_key, key_entry->command, NULL);

      /* add gconf watches */
      key_entry->gconf_cnxn_desc = gconf_client_notify_add (client,
                                                            key_entry->desc_gconf_key,
					    		    (GConfClientNotifyFunc) &keybinding_description_changed,
																            key_entry, NULL, NULL);
      key_entry->gconf_cnxn_cmd = gconf_client_notify_add (client,
	                                                   key_entry->cmd_gconf_key,
						           (GConfClientNotifyFunc) &keybinding_command_changed,
						           key_entry, NULL, NULL);
      key_entry->gconf_cnxn = gconf_client_notify_add (client,
	                                               key_entry->gconf_key,
						       (GConfClientNotifyFunc) &keybinding_key_changed,
					               key_entry, NULL, NULL);


      g_object_unref (client);

      /* make the new shortcut visible */
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_expand_to_path (tree_view, path);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0, 0);
      gtk_tree_path_free (path);
    }
  else
    {
      g_free (key_entry->gconf_key);
      g_free (key_entry->description);
      g_free (key_entry->desc_gconf_key);
      g_free (key_entry->command);
      g_free (key_entry->cmd_gconf_key);
      g_free (key_entry);
    }
}

static gboolean
start_editing_kb_cb (GtkTreeView *treeview,
			  GtkTreePath *path,
			  GtkTreeViewColumn *column,
			  gpointer user_data)
{
      GtkTreeModel *model;
      GtkTreeIter iter;
      KeyEntry *key;

      model = gtk_tree_view_get_model (treeview);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          KEYENTRY_COLUMN, &key,
                         -1);

      /* if only the accel can be edited on the selected row
       * always select the accel column */
      if (key->desc_editable &&
          column == gtk_tree_view_get_column (treeview, 0))
        {
          gtk_widget_grab_focus (GTK_WIDGET (treeview));
          gtk_tree_view_set_cursor (treeview, path,
                                    gtk_tree_view_get_column (treeview, 0),
                                    FALSE);
          update_custom_shortcut (model, &iter);
        }
      else
        {
          gtk_widget_grab_focus (GTK_WIDGET (treeview));
          gtk_tree_view_set_cursor (treeview,
                                    path,
                                    gtk_tree_view_get_column (treeview, 1),
                                    TRUE);
        }

  return FALSE;
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
		  GdkEventButton *event,
		  gpointer        user_data)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;

  if (event->window != gtk_tree_view_get_bin_window (tree_view))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view,
				     (gint) event->x,
				     (gint) event->y,
				     &path, &column,
				     NULL, NULL))
    {
      IdleData *idle_data;
      GtkTreeModel *model;
      GtkTreeIter iter;
      KeyEntry *key;

      if (gtk_tree_path_get_depth (path) == 1)
	{
	  gtk_tree_path_free (path);
	  return FALSE;
	}

      model = gtk_tree_view_get_model (tree_view);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          KEYENTRY_COLUMN, &key,
                         -1);

      /* if only the accel can be edited on the selected row
       * always select the accel column */
      if (key->desc_editable &&
          column == gtk_tree_view_get_column (tree_view, 0))
        {
          gtk_widget_grab_focus (GTK_WIDGET (tree_view));
          gtk_tree_view_set_cursor (tree_view, path,
                                    gtk_tree_view_get_column (tree_view, 0),
                                    FALSE);
          update_custom_shortcut (model, &iter);
        }
      else
        {
          idle_data = g_new (IdleData, 1);
          idle_data->tree_view = tree_view;
          idle_data->path = path;
          idle_data->column = key->desc_editable ? column :
                              gtk_tree_view_get_column (tree_view, 1);
          g_idle_add ((GSourceFunc) real_start_editing_cb, idle_data);
          block_accels = TRUE;
        }
      g_signal_stop_emission_by_name (tree_view, "button_press_event");
    }
  return TRUE;
}

/* this handler is used to keep accels from activating while the user
 * is assigning a new shortcut so that he won't accidentally trigger one
 * of the widgets */
static gboolean
maybe_block_accels (GtkWidget *widget,
                    GdkEventKey *event,
                    gpointer user_data)
{
  if (block_accels)
  {
    return gtk_window_propagate_key_event (GTK_WINDOW (widget), event);
  }
  return FALSE;
}

static void
cb_dialog_response (GtkWidget *widget, gint response_id, gpointer data)
{
  GladeXML *dialog = data;
  GtkTreeView *treeview;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  treeview = GTK_TREE_VIEW (WID ("shortcut_treeview"));
  model = gtk_tree_view_get_model (treeview);

  if (response_id == GTK_RESPONSE_HELP)
    {
      capplet_help (GTK_WINDOW (widget),
                    "goscustdesk-39");
    }
  else if (response_id == RESPONSE_ADD)
    {
      add_custom_shortcut (treeview, model);
    }
  else if (response_id == RESPONSE_REMOVE)
    {
      selection = gtk_tree_view_get_selection (treeview);
      if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        {
          remove_custom_shortcut (model, &iter);
        }
    }
  else
    {
      clear_old_model (dialog);
      gtk_main_quit ();
    }
}

static void
selection_changed (GtkTreeSelection *selection, gpointer data)
{
  GtkWidget *button = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  KeyEntry *key;
  gboolean can_remove;

  can_remove = FALSE;
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, KEYENTRY_COLUMN, &key, -1);
      if (key && key->command != NULL && key->editable)
	can_remove = TRUE;
    }

  gtk_widget_set_sensitive (button, can_remove);
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  GtkTreeView *treeview = GTK_TREE_VIEW (WID ("shortcut_treeview"));
  gchar *wm_name;
  GtkTreeSelection *selection;
  GSList *allowed_keys;

  client = gconf_client_get_default ();

  g_signal_connect (treeview, "button_press_event",
		    G_CALLBACK (start_editing_cb), dialog);
  g_signal_connect (treeview, "row-activated",
		    G_CALLBACK (start_editing_kb_cb), NULL);

  renderer = gtk_cell_renderer_text_new ();

  g_signal_connect (renderer, "edited",
                    G_CALLBACK (description_edited_callback),
                    treeview);

  column = gtk_tree_view_column_new_with_attributes (_("Action"),
						     renderer,
						     "text", DESCRIPTION_COLUMN,
						     NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, description_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (treeview, column);
  gtk_tree_view_column_set_sort_column_id (column, DESCRIPTION_COLUMN);

  renderer = (GtkCellRenderer *) g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
					       "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
					       NULL);

  g_signal_connect (renderer, "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    treeview);

  g_signal_connect (renderer, "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    treeview);

  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (treeview, column);
  gtk_tree_view_column_set_sort_column_id (column, KEYENTRY_COLUMN);

  gconf_client_add_dir (client, GCONF_BINDING_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (client,
			   "/apps/metacity/general/num_workspaces",
			   (GConfClientNotifyFunc) key_entry_controlling_key_changed,
			   dialog, NULL, NULL);

  /* set up the dialog */
  wm_name = wm_common_get_current_window_manager();
  reload_key_entries (wm_name, dialog);
  g_free (wm_name);

  widget = WID ("gnome-keybinding-dialog");
  capplet_set_icon (widget, "preferences-desktop-keyboard-shortcuts");
  gtk_widget_show (widget);

  g_signal_connect (widget, "key_press_event", G_CALLBACK (maybe_block_accels), NULL);
  g_signal_connect (widget, "response", G_CALLBACK (cb_dialog_response), dialog);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (selection_changed), WID ("remove-button"));

  allowed_keys = gconf_client_get_list (client,
                                        GCONF_BINDING_DIR "/allowed_keys",
                                        GCONF_VALUE_STRING,
                                        NULL);
  if (allowed_keys != NULL)
    {
      g_slist_foreach (allowed_keys, (GFunc)g_free, NULL);
      g_slist_free (allowed_keys);
      gtk_widget_set_sensitive (WID ("add-button"), FALSE);
    }

  g_object_unref (client);

  /* setup the custom shortcut dialog */
  custom_shortcut_dialog = WID ("custom-shortcut-dialog");
  custom_shortcut_name_entry = WID ("custom-shortcut-name-entry");
  custom_shortcut_command_entry = WID ("custom-shortcut-command-entry");
  gtk_window_set_transient_for (GTK_WINDOW (custom_shortcut_dialog),
                                GTK_WINDOW (widget));
}

int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gtk_init (&argc, &argv);

  activate_settings_daemon ();

  dialog = create_dialog ();
  wm_common_register_window_manager_change ((GFunc) reload_key_entries, dialog);
  setup_dialog (dialog);

  gtk_main ();

  g_object_unref (dialog);
  return 0;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
