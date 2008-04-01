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

#include "wm-common.h"
#include "capplet-util.h"
#include "eggcellrendererkeys.h"
#include "activate-settings-daemon.h"

#define MAX_ELEMENTS_BEFORE_SCROLLING 10

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
  guint gconf_cnxn;
  char *description;
} KeyEntry;

static gboolean block_accels = FALSE;

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GNOMECC_GLADE_DIR "/gnome-keybinding-properties.glade", "gnome-keybinding-dialog", NULL);
  if (!dialog)
    dialog = glade_xml_new ("gnome-keybinding-properties.glade", "gnome-keybinding-dialog", NULL);

  return dialog;
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
    return translate ? g_strdup (_("Disabled")) : g_strdup ("disabled");
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
    g_object_set (G_OBJECT (cell),
		  "visible", FALSE,
		  NULL);
  else if (! key_entry->editable)
    g_object_set (G_OBJECT (cell),
		  "visible", TRUE,
		  "editable", FALSE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_ITALIC,
		  NULL);
  else
    g_object_set (G_OBJECT (cell),
		  "visible", TRUE,
		  "editable", TRUE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_NORMAL,
		  NULL);
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

  key_entry = (KeyEntry *)user_data;
  key_value = gconf_value_get_string (entry->value);

  binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
  key_entry->editable = gconf_entry_get_is_writable (entry);

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
              gconf_client_notify_remove (client, key_entry->gconf_cnxn);
              g_free (key_entry->gconf_key);
              g_free (key_entry->description);
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
append_keys_to_tree (GladeXML           *dialog,
		     const gchar        *title,
		     const KeyListEntry *keys_list)
{
  GConfClient *client;
  GtkTreeIter parent_iter, iter;
  GtkTreeModel *model;
  gboolean found;
  gint i, j;

  client = gconf_client_get_default ();
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("shortcut_treeview")));

  /* Try to find a section parent iter, if it already exists */
  i = gtk_tree_model_iter_n_children (model, NULL);
  found = FALSE;
  gtk_tree_model_get_iter_first (model, &iter);
  for (j = 0; j < i; j++)
    {
      char *description = NULL;

      gtk_tree_model_iter_next (model, &iter);
      gtk_tree_model_get (model, &iter,
			  DESCRIPTION_COLUMN, &description,
			  -1);
      if (description != NULL && g_str_equal (description, title))
        {
	  found = TRUE;
	  break;
        }
    }

  if (found)
    {
      parent_iter = iter;
    } else {
      gtk_tree_store_append (GTK_TREE_STORE (model), &parent_iter, NULL);
      gtk_tree_store_set (GTK_TREE_STORE (model), &parent_iter,
			  DESCRIPTION_COLUMN, title,
			  -1);
    }


  i = 0;
  gtk_tree_model_foreach (model, count_rows_foreach, &i);

  /* If the header we just added is the MAX_ELEMENTS_BEFORE_SCROLLING th,
   * then we need to scroll now */
  ensure_scrollbar (dialog, i - 1);

  for (j = 0; keys_list[j].name != NULL; j++)
    {
      GConfEntry *entry;
      GConfSchema *schema = NULL;
      KeyEntry *key_entry;
      GError *error = NULL;
      const gchar *key_string;
      gchar *key_value;

      if (!should_show_key (&keys_list[j]))
	continue;

      key_string = keys_list[j].name;

      entry = gconf_client_get_entry (client,
                                      key_string,
				      NULL,
				      TRUE,
				      &error);
      if (error || entry == NULL)
	{
	  /* We don't actually want to popup a dialog - just skip this one */
	  if (error)
	    g_error_free (error);
	  continue;
	}

      if (gconf_entry_get_schema_name (entry))
	schema = gconf_client_get_schema (client, gconf_entry_get_schema_name (entry), &error);

      if (error || schema == NULL)
	{
	  /* We don't actually want to popup a dialog - just skip this one */
	  if (error)
	    g_error_free (error);
	  continue;
	}

      key_value = gconf_client_get_string (client, key_string, &error);

      key_entry = g_new0 (KeyEntry, 1);
      key_entry->gconf_key = g_strdup (key_string);
      key_entry->editable = gconf_entry_get_is_writable (entry);
      key_entry->model = model;
      gconf_client_add_dir (client, key_string, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
      key_entry->gconf_cnxn = gconf_client_notify_add (client,
						       key_string,
						       (GConfClientNotifyFunc) &keybinding_key_changed,
						       key_entry, NULL, NULL);
      binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
      g_free (key_value);
      key_entry->description = g_strdup (gconf_schema_get_short_desc (schema));

      ensure_scrollbar (dialog, i);

      i++;
      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent_iter);
      if (gconf_schema_get_short_desc (schema))
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    DESCRIPTION_COLUMN,
                            key_entry->description,
			    KEYENTRY_COLUMN, key_entry,
			    -1);
      else
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    DESCRIPTION_COLUMN, _("<Unknown Action>"),
			    KEYENTRY_COLUMN, key_entry,
			    -1);
      gtk_tree_view_expand_all (GTK_TREE_VIEW (WID ("shortcut_treeview")));
      gconf_entry_free (entry);
      gconf_schema_free (schema);
    }

  g_object_unref (client);

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
  key.value = value;
  if (gconf_key)
    key.key = g_strdup (gconf_key);
  else
    key.key = NULL;
  key.comparison = comparison;
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
				  "Please try with a key such as Control, Alt or Shift at the same time.\n"),
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

      name = binding_name (keyval, keycode, mask, TRUE);

      dialog =
        gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                GTK_MESSAGE_WARNING,
                                GTK_BUTTONS_CANCEL,
                                _("The shortcut \"%s\" is already used for:\n \"%s\"\n"),
                                name,
                                tmp_key.description ?
                                tmp_key.description : tmp_key.gconf_key);
      g_free (name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      /* set it back to its previous value. */
      egg_cell_renderer_keys_set_accelerator (EGG_CELL_RENDERER_KEYS (cell),
					      key_entry->keyval, key_entry->keycode, key_entry->mask);
      return;
    }

  str = binding_name (keyval, keycode, mask, FALSE);

  client = gconf_client_get_default();
  gconf_client_set_string (client,
                           key_entry->gconf_key,
                           str,
                           &err);
  g_free (str);
  g_object_unref (G_OBJECT (client));

  if (err != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       _("Error setting new accelerator in configuration database: %s\n"),
				       err->message);
      gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);
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
			   "disabled",
			   &err);
  g_object_unref (G_OBJECT (client));

  if (err != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       _("Error unsetting accelerator in configuration database: %s\n"),
				       err->message);
      gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}


typedef struct
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
} IdleData;

static gboolean
real_start_editing_cb (IdleData *idle_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (idle_data->tree_view));
  gtk_tree_view_set_cursor (idle_data->tree_view,
			    idle_data->path,
			    gtk_tree_view_get_column (idle_data->tree_view, 1),
			    TRUE);

  gtk_tree_path_free (idle_data->path);
  g_free (idle_data);
  return FALSE;
}

static gboolean
start_editing_kb_cb (GtkTreeView *treeview,
			  GtkTreePath *path,
			  GtkTreeViewColumn *column,
			  gpointer user_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (treeview));
  gtk_tree_view_set_cursor (treeview,
			    path,
			    gtk_tree_view_get_column (treeview, 1),
			    TRUE);

  return FALSE;
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
		  GdkEventButton *event,
		  gpointer        user_data)
{
  GtkTreePath *path;

  if (event->window != gtk_tree_view_get_bin_window (tree_view))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view,
				     (gint) event->x,
				     (gint) event->y,
				     &path, NULL,
				     NULL, NULL))
    {
      IdleData *idle_data;

      if (gtk_tree_path_get_depth (path) == 1)
	{
	  gtk_tree_path_free (path);
	  return FALSE;
	}

      idle_data = g_new (IdleData, 1);
      idle_data->tree_view = tree_view;
      idle_data->path = path;
      g_signal_stop_emission_by_name (G_OBJECT (tree_view), "button_press_event");
      g_idle_add ((GSourceFunc) real_start_editing_cb, idle_data);
      block_accels = TRUE;
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
	if (response_id == GTK_RESPONSE_HELP)
          {
            capplet_help (GTK_WINDOW (widget),
                          "user-guide.xml",
                          "goscustdesk-39");
          }
	else
          {
            GladeXML *dialog = data;

            clear_old_model (dialog);
            gtk_main_quit ();
          }
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  gchar *wm_name;

  client = gconf_client_get_default ();

  g_signal_connect (GTK_TREE_VIEW (WID ("shortcut_treeview")),
		    "button_press_event",
		    G_CALLBACK (start_editing_cb), NULL);
  g_signal_connect (GTK_TREE_VIEW (WID ("shortcut_treeview")),
		    "row-activated",
		    G_CALLBACK (start_editing_kb_cb), NULL);

  column = gtk_tree_view_column_new_with_attributes (_("Action"),
						     gtk_cell_renderer_text_new (),
						     "text", DESCRIPTION_COLUMN,
						     NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("shortcut_treeview")), column);
  gtk_tree_view_column_set_sort_column_id (column, DESCRIPTION_COLUMN);

  renderer = (GtkCellRenderer *) g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
					       "editable", TRUE,
					       "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
					       NULL);

  g_signal_connect (G_OBJECT (renderer),
		    "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    WID ("shortcut_treeview"));

  g_signal_connect (G_OBJECT (renderer),
		    "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    WID ("shortcut_treeview"));

  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("shortcut_treeview")), column);
  gtk_tree_view_column_set_sort_column_id (column, KEYENTRY_COLUMN);

  gconf_client_add_dir (client, "/apps/gnome_keybinding_properties", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (client,
			   "/apps/metacity/general/num_workspaces",
			   (GConfClientNotifyFunc) key_entry_controlling_key_changed,
			   dialog, NULL, NULL);
  g_object_unref (client);

  /* set up the dialog */
  wm_name = wm_common_get_current_window_manager();
  reload_key_entries (wm_name, dialog);
  g_free (wm_name);

  widget = WID ("gnome-keybinding-dialog");
  capplet_set_icon (widget, "gnome-settings-keybindings");
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "key_press_event", G_CALLBACK (maybe_block_accels), NULL);
  g_signal_connect (G_OBJECT (widget), "response", G_CALLBACK (cb_dialog_response), dialog);
}

int
main (int argc, char *argv[])
{
  GnomeProgram *program;
  GladeXML *dialog;

  g_thread_init (NULL);
  gtk_init (&argc, &argv);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  program = gnome_program_init ("gnome-keybinding-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  activate_settings_daemon ();

  dialog = create_dialog ();
  wm_common_register_window_manager_change ((GFunc) reload_key_entries, dialog);
  setup_dialog (dialog);

  gtk_main ();

  g_object_unref (dialog);
  g_object_unref (program);
  return 0;
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
