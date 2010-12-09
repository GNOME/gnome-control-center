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
#define MAX_CUSTOM_SHORTCUTS 1000
#define RESPONSE_ADD 0
#define RESPONSE_REMOVE 1

static gboolean block_accels = FALSE;
static GHashTable *keyb_sections = NULL;

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
clear_old_model (GtkBuilder *builder)
{
  GtkWidget *tree_view;
  GtkWidget *actions_swindow;
  GtkTreeModel *model;

  tree_view = WID (builder, "shortcut_treeview");
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

  actions_swindow = WID (builder, "actions_swindow");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (actions_swindow),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_size_request (actions_swindow, -1, -1);
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

static void
find_section (GtkTreeModel *model,
              GtkTreeIter  *iter,
	      const char   *title)
{
  gboolean success;

  success = gtk_tree_model_get_iter_first (model, iter);
  while (success)
    {
      char *description = NULL;

      gtk_tree_model_get (model, iter,
			  DESCRIPTION_COLUMN, &description,
			  -1);

      if (g_strcmp0 (description, title) == 0)
        return;
      success = gtk_tree_model_iter_next (model, iter);
    }

    gtk_tree_store_append (GTK_TREE_STORE (model), iter, NULL);
    gtk_tree_store_set (GTK_TREE_STORE (model), iter,
                        DESCRIPTION_COLUMN, title,
                        -1);
}

static void
append_keys_to_tree (GtkBuilder         *builder,
		     const gchar        *title,
		     const KeyListEntry *keys_list)
{
  GtkTreeIter parent_iter, iter;
  GtkTreeModel *model;
  gint i, j;

  /* Try to find a section parent iter, if it already exists */
  find_section (model, &iter, title);
  parent_iter = iter;

  i = 0;
  gtk_tree_model_foreach (model, count_rows_foreach, &i);

  /* If the header we just added is the MAX_ELEMENTS_BEFORE_SCROLLING th,
   * then we need to scroll now */
  ensure_scrollbar (builder, i - 1);

===============================================================

  /* Don't show an empty section */
  if (gtk_tree_model_iter_n_children (model, &parent_iter) == 0)
    gtk_tree_store_remove (GTK_TREE_STORE (model), &parent_iter);

  if (i == 0)
      gtk_widget_hide (WID (builder, "shortcuts_vbox"));
  else
      gtk_widget_show (WID (builder, "shortcuts_vbox"));
}

static void
key_entry_controlling_key_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   gpointer     user_data)
{
  reload_key_entries (user_data);
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
  GDK_KEY_Home,
  GDK_KEY_Left,
  GDK_KEY_Up,
  GDK_KEY_Right,
  GDK_KEY_Down,
  GDK_KEY_Page_Up,
  GDK_KEY_Page_Down,
  GDK_KEY_End,
  GDK_KEY_Tab,

  /* Return */
  GDK_KEY_KP_Enter,
  GDK_KEY_Return,

  GDK_KEY_space,
  GDK_KEY_Mode_switch
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
      if ((tmp_key.keyval >= GDK_KEY_a && tmp_key.keyval <= GDK_KEY_z)
	   || (tmp_key.keyval >= GDK_KEY_A && tmp_key.keyval <= GDK_KEY_Z)
	   || (tmp_key.keyval >= GDK_KEY_0 && tmp_key.keyval <= GDK_KEY_9)
	   || (tmp_key.keyval >= GDK_KEY_kana_fullstop && tmp_key.keyval <= GDK_KEY_semivoicedsound)
	   || (tmp_key.keyval >= GDK_KEY_Arabic_comma && tmp_key.keyval <= GDK_KEY_Arabic_sukun)
	   || (tmp_key.keyval >= GDK_KEY_Serbian_dje && tmp_key.keyval <= GDK_KEY_Cyrillic_HARDSIGN)
	   || (tmp_key.keyval >= GDK_KEY_Greek_ALPHAaccent && tmp_key.keyval <= GDK_KEY_Greek_omega)
	   || (tmp_key.keyval >= GDK_KEY_hebrew_doublelowline && tmp_key.keyval <= GDK_KEY_hebrew_taf)
	   || (tmp_key.keyval >= GDK_KEY_Thai_kokai && tmp_key.keyval <= GDK_KEY_Thai_lekkao)
	   || (tmp_key.keyval >= GDK_KEY_Hangul && tmp_key.keyval <= GDK_KEY_Hangul_Special)
	   || (tmp_key.keyval >= GDK_KEY_Hangul_Kiyeog && tmp_key.keyval <= GDK_KEY_Hangul_J_YeorinHieuh)
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

  gtk_entry_set_text (GTK_ENTRY (custom_shortcut_name_entry), key->description ? key->description : "");
  gtk_widget_set_sensitive (custom_shortcut_name_entry, key->desc_editable);
  gtk_widget_grab_focus (custom_shortcut_name_entry);
  gtk_entry_set_text (GTK_ENTRY (custom_shortcut_command_entry), key->command ? key->command : "");
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
  /* suggest sync now so the unset directory actually gets dropped;
   * if we don't do this we may end up with 'zombie' shortcuts when
   * restarting the app */
  gconf_client_suggest_sync (client, NULL);
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
    {
      remove_custom_shortcut (model, iter);
    }
  else
    {
      GConfClient *client;

      gtk_tree_store_set (GTK_TREE_STORE (model), iter,
			  KEYENTRY_COLUMN, key, -1);
      client = gconf_client_get_default ();
      if (key->description != NULL)
        gconf_client_set_string (client, key->desc_gconf_key, key->description, NULL);
      else
        gconf_client_unset (client, key->desc_gconf_key, NULL);
      gconf_client_set_string (client, key->cmd_gconf_key, key->command, NULL);
      g_object_unref (client);
    }
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

static void
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

  if (key == NULL)
    {
      /* This is a section heading - expand or collapse */
      if (gtk_tree_view_row_expanded (treeview, path))
	gtk_tree_view_collapse_row (treeview, path);
      else
	gtk_tree_view_expand_row (treeview, path, FALSE);
      return;
    }

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

#if 0
static void
cb_dialog_response (GtkWidget *widget, gint response_id, gpointer data)
{
  GtkBuilder *builder = data;
  GtkTreeView *treeview;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));
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
      clear_old_model (builder);
      gtk_main_quit ();
    }
}
#endif

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
add_button_clicked (GtkWidget  *button,
                    GtkBuilder *builder)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));
  model = gtk_tree_view_get_model (treeview);

  add_custom_shortcut (treeview, model);
}

static void
remove_button_clicked (GtkWidget  *button,
                       GtkBuilder *builder)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));
  model = gtk_tree_view_get_model (treeview);

  selection = gtk_tree_view_get_selection (treeview);
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      remove_custom_shortcut (model, &iter);
    }
}

static void
on_window_manager_change (const char *wm_name, GtkBuilder *builder)
{
  reload_key_entries (builder);
}

void
gnome_keybinding_properties_init (CcPanel *panel, GtkBuilder *builder)
{
  wm_common_register_window_manager_change ((GFunc) on_window_manager_change,
                                            builder);

}

void
gnome_keybinding_properties_dispose (CcPanel *panel)
{
  if (maybe_block_accels_id != 0)
    {
      CcShell *shell;
      GtkWidget *toplevel;

      shell = cc_panel_get_shell (CC_PANEL (panel));
      toplevel = cc_shell_get_toplevel (shell);

      g_signal_handler_disconnect (toplevel, maybe_block_accels_id);
      maybe_block_accels_id = 0;

      if (keyb_sections != NULL)
        g_hash_table_destroy (keyb_sections);
    }
}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
