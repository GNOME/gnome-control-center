/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * Authors: Joaquim Rocha <jrocha@redhat.com>
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
 */

#include <config.h>
#include <glib/gi18n-lib.h>

#include "gsd-wacom-key-shortcut-button.h"
#include "cc-wacom-button-row.h"

G_DEFINE_TYPE (CcWacomButtonRow, cc_wacom_button_row, GTK_TYPE_LIST_BOX_ROW)

#define ACTION_TYPE_KEY            "action-type"
#define CUSTOM_ACTION_KEY          "custom-action"
#define KEY_CUSTOM_ELEVATOR_ACTION "custom-elevator-action"
#define OLED_LABEL                 "oled-label"

#define WACOM_C(x) g_dpgettext2(NULL, "Wacom action-type", x)

enum {
  ACTION_NAME_COLUMN,
  ACTION_TYPE_COLUMN,
  ACTION_N_COLUMNS
};

struct _CcWacomButtonRowPrivate {
  GsdWacomTabletButton *button;
  GtkDirectionType direction;
  GtkComboBox *action_combo;
  GsdWacomKeyShortcutButton *key_shortcut_button;
};

static GtkWidget *
create_actions_combo (GsdWacomTabletButtonType type)
{
  GtkListStore    *model;
  GtkTreeIter      iter;
  GtkWidget       *combo;
  GtkCellRenderer *renderer;
  gint             i;

  model = gtk_list_store_new (ACTION_N_COLUMNS, G_TYPE_STRING, G_TYPE_INT);

  for (i = 0; i < G_N_ELEMENTS (action_table); i++)
    {
      if ((type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
           type == WACOM_TABLET_BUTTON_TYPE_RING) &&
          action_table[i].action_type != GSD_WACOM_ACTION_TYPE_NONE &&
          action_table[i].action_type != GSD_WACOM_ACTION_TYPE_CUSTOM)
        continue;

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
                          ACTION_NAME_COLUMN, WACOM_C(action_table[i].action_name),
                          ACTION_TYPE_COLUMN, action_table[i].action_type, -1);
    }

  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (model));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), renderer,
                                  "text", ACTION_NAME_COLUMN, NULL);


  return combo;
}

static void
cc_wacom_button_row_update_shortcut (CcWacomButtonRow *row,
                                     GsdWacomActionType action_type)
{
  CcWacomButtonRowPrivate *priv;
  GsdWacomTabletButton    *button;
  GtkDirectionType         dir;
  guint                    keyval;
  GdkModifierType          mask;
  char                    *shortcut;

  if (action_type != GSD_WACOM_ACTION_TYPE_CUSTOM)
    return;

  priv = row->priv;

  button = priv->button;
  dir = priv->direction;

  if (button->type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
      button->type == WACOM_TABLET_BUTTON_TYPE_RING)
    {
      char *str;
      char **strv;

      strv = g_settings_get_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION);

      if (strv != NULL)
        {
          if (dir == GTK_DIR_UP)
            str = strv[0];
          else
            str = strv[1];

          shortcut = g_strdup (str);

          g_strfreev (strv);
        }
      else
        {
          shortcut = NULL;
        }
    }
  else
    shortcut = g_settings_get_string (button->settings, CUSTOM_ACTION_KEY);

  if (shortcut != NULL)
    {
      gtk_accelerator_parse (shortcut, &keyval, &mask);

      g_object_set (priv->key_shortcut_button,
                    "key-value", keyval,
                    "key-mods", mask,
                    NULL);

      g_free (shortcut);
    }
}

static gchar *
get_tablet_dir_button_shortcut (GsdWacomTabletButton *button,
                                GtkDirectionType      dir)
{
  char *str, **strv;
  char *shortcut = NULL;

  strv = g_settings_get_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION);

  if (strv != NULL)
    {
      if (dir == GTK_DIR_UP)
        str = strv[0];
      else
        str = strv[1];

      shortcut = g_strdup (str);

      g_strfreev (strv);
    }

  return shortcut;
}

static void
cc_wacom_button_row_update_action (CcWacomButtonRow *row,
                                   GsdWacomActionType action_type)
{
  CcWacomButtonRowPrivate *priv;
  GtkTreeIter              iter;
  gboolean                 iter_valid;
  GsdWacomActionType       current_action_type, real_action_type;
  GtkTreeModel            *model;

  priv = row->priv;

  model = gtk_combo_box_get_model (priv->action_combo);
  real_action_type = action_type;

  if (priv->button->type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
      priv->button->type == WACOM_TABLET_BUTTON_TYPE_RING)
    {
      char *shortcut;

      shortcut = get_tablet_dir_button_shortcut (priv->button, priv->direction);

      if (shortcut == NULL || g_strcmp0 (shortcut, "") == 0)
        real_action_type = GSD_WACOM_ACTION_TYPE_NONE;

      g_free (shortcut);
    }

  for (iter_valid = gtk_tree_model_get_iter_first (model, &iter); iter_valid;
       iter_valid = gtk_tree_model_iter_next (model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          ACTION_TYPE_COLUMN, &current_action_type,
                          -1);

      if (current_action_type == real_action_type)
        {
          gtk_combo_box_set_active_iter (priv->action_combo, &iter);
          break;
        }
    }
}

static void
cc_wacom_button_row_update (CcWacomButtonRow *row)
{
  CcWacomButtonRowPrivate *priv;
  GsdWacomTabletButton *button;
  GsdWacomActionType current_action_type;

  priv = row->priv;

  button = priv->button;

  current_action_type = g_settings_get_enum (button->settings, ACTION_TYPE_KEY);

  cc_wacom_button_row_update_shortcut (row, current_action_type);

  cc_wacom_button_row_update_action (row, current_action_type);

  gtk_widget_set_sensitive (GTK_WIDGET (row->priv->key_shortcut_button),
                            current_action_type == GSD_WACOM_ACTION_TYPE_CUSTOM);
}

static void
assign_custom_key_to_dir_button (CcWacomButtonRow *row,
                                 gchar            *custom_key)
{
  GsdWacomTabletButton *button;
  GtkDirectionType            dir;
  char *strs[3];
  char **strv;

  button = row->priv->button;
  dir = row->priv->direction;

  strs[2] = NULL;
  strs[0] = strs[1] = "";
  strv = g_settings_get_strv (button->settings, KEY_CUSTOM_ELEVATOR_ACTION);

  if (strv != NULL)
    {
      if (g_strv_length (strv) >= 1)
        strs[0] = strv[0];
      if (g_strv_length (strv) >= 2)
        strs[1] = strv[1];
    }

  if (dir == GTK_DIR_UP)
    strs[0] = custom_key;
  else
    strs[1] = custom_key;

  g_settings_set_strv (button->settings,
                       KEY_CUSTOM_ELEVATOR_ACTION,
                       (const gchar * const*) strs);

  g_clear_pointer (&strv, g_strfreev);
}

static void
change_button_action_type (CcWacomButtonRow   *row,
                           GsdWacomActionType  type)
{
  GsdWacomTabletButton *button;
  GsdWacomActionType    current_type;

  button = row->priv->button;

  if (button == NULL)
    return;

  current_type = g_settings_get_enum (button->settings, ACTION_TYPE_KEY);

  if (button->type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
      button->type == WACOM_TABLET_BUTTON_TYPE_RING)
    {
      if (type == GSD_WACOM_ACTION_TYPE_NONE)
        assign_custom_key_to_dir_button (row, "");
      else if (type == GSD_WACOM_ACTION_TYPE_CUSTOM)
        {
          guint           keyval;
          GdkModifierType mask;
          char            *custom_key;

          g_object_get (row->priv->key_shortcut_button,
                        "key-value", &keyval,
                        "key-mods", &mask,
                        NULL);

          mask &= ~GDK_LOCK_MASK;

          custom_key = gtk_accelerator_name (keyval, mask);

          assign_custom_key_to_dir_button (row, custom_key);
          g_settings_set_enum (button->settings, ACTION_TYPE_KEY, type);

          g_free (custom_key);
        }
    }
  else if (current_type != type)
    {
      g_settings_set_enum (button->settings, ACTION_TYPE_KEY, type);
    }

  gtk_widget_set_sensitive (GTK_WIDGET (row->priv->key_shortcut_button),
                            type == GSD_WACOM_ACTION_TYPE_CUSTOM);
}

static void
on_key_shortcut_edited (GsdWacomKeyShortcutButton *shortcut_button,
                        CcWacomButtonRow    *row)
{
  GsdWacomTabletButton *button;
  char *custom_key;
  guint keyval;
  GdkModifierType mask;

  button = row->priv->button;

  if (button == NULL)
    return;

  change_button_action_type (row, GSD_WACOM_ACTION_TYPE_CUSTOM);

  g_object_get (row->priv->key_shortcut_button,
                "key-value", &keyval,
                "key-mods", &mask,
                NULL);

  mask &= ~GDK_LOCK_MASK;

  custom_key = gtk_accelerator_name (keyval, mask);

  if (button->type == WACOM_TABLET_BUTTON_TYPE_STRIP ||
      button->type == WACOM_TABLET_BUTTON_TYPE_RING)
    {
      assign_custom_key_to_dir_button (row, custom_key);
    }
  else
    g_settings_set_string (button->settings, CUSTOM_ACTION_KEY, custom_key);

  g_free (custom_key);
}

static void
on_key_shortcut_cleared (GsdWacomKeyShortcutButton *key_shortcut_button,
                         CcWacomButtonRow    *row)
{
  change_button_action_type (row, GSD_WACOM_ACTION_TYPE_NONE);
  cc_wacom_button_row_update_action (row, GSD_WACOM_ACTION_TYPE_NONE);
}

static void
on_row_action_combo_box_changed (GtkComboBox      *combo,
                                 CcWacomButtonRow *row)
{
  GsdWacomActionType type;
  GtkTreeModel *model;
  GtkListBox *list_box;
  GtkTreeIter iter;

  if (!gtk_combo_box_get_active_iter (combo, &iter))
    return;

  /* Select the row where we changed the combo box (if not yet selected) */
  list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (row)));
  if (list_box && gtk_list_box_get_selected_row (list_box) != GTK_LIST_BOX_ROW (row))
    gtk_list_box_select_row (list_box, GTK_LIST_BOX_ROW (row));

  model = gtk_combo_box_get_model (combo);
  gtk_tree_model_get (model, &iter, ACTION_TYPE_COLUMN, &type, -1);

  change_button_action_type (row, type);
}

static gboolean
on_key_shortcut_button_press_event (GsdWacomKeyShortcutButton  *button,
                                    GdkEventButton       *event,
                                    GtkListBoxRow        *row)
{
  GtkListBox *list_box;

  /* Select the row where we pressed the button (if not yet selected) */
  list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (row)));
  if (list_box && gtk_list_box_get_selected_row (list_box) != row)
    gtk_list_box_select_row (list_box, row);

  return FALSE;
}

static void
cc_wacom_button_row_class_init (CcWacomButtonRowClass *button_row_class)
{
  g_type_class_add_private (button_row_class, sizeof (CcWacomButtonRowPrivate));
}

static void
cc_wacom_button_row_init (CcWacomButtonRow *button_row)
{
  button_row->priv = G_TYPE_INSTANCE_GET_PRIVATE (button_row,
                                                  CC_WACOM_TYPE_BUTTON_ROW,
                                                  CcWacomButtonRowPrivate);
}

GtkWidget *
cc_wacom_button_row_new (GsdWacomTabletButton *button,
                         GtkDirectionType      dir)
{
  GtkWidget               *row;
  GtkWidget               *grid, *combo, *label, *shortcut_button;
  CcWacomButtonRowPrivate *priv;
  char *dir_name = NULL;

  row = g_object_new (CC_WACOM_TYPE_BUTTON_ROW, NULL);
  priv = CC_WACOM_BUTTON_ROW (row)->priv;

  priv->button = button;
  priv->direction = dir;

  grid = gtk_grid_new ();
  gtk_widget_show (grid);
  gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
  gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

  if (dir == GTK_DIR_UP || dir == GTK_DIR_DOWN)
    {
      if (button->type == WACOM_TABLET_BUTTON_TYPE_RING)
        {
          dir_name = g_strdup_printf ("%s (%s)",
                                      button->name,
                                      dir == GTK_DIR_UP ? "↺" : "↻");
        } else {
        dir_name = g_strdup_printf ("%s (%s)",
                                    button->name,
                                    dir == GTK_DIR_UP ? C_("Wacom tablet button", "Up") :
                                    C_("Wacom tablet button", "Down"));
      }
    }

  label = gtk_label_new (dir_name ? dir_name : button->name);
  g_object_set (label, "halign", GTK_ALIGN_START, NULL);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  combo = create_actions_combo (button->type);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);
  gtk_widget_show (combo);
  priv->action_combo = GTK_COMBO_BOX (combo);
  g_signal_connect (combo, "changed",
                    G_CALLBACK (on_row_action_combo_box_changed), row);

  shortcut_button = gsd_wacom_key_shortcut_button_new ();
  g_object_set (shortcut_button, "mode", GSD_WACOM_KEY_SHORTCUT_BUTTON_MODE_ALL, NULL);
  gtk_grid_attach (GTK_GRID (grid), shortcut_button, 2, 0, 1, 1);
  gtk_widget_show (shortcut_button);
  priv->key_shortcut_button = GSD_WACOM_KEY_SHORTCUT_BUTTON (shortcut_button);
  g_signal_connect (shortcut_button, "key-shortcut-cleared",
                    G_CALLBACK (on_key_shortcut_cleared),
                    row);
  g_signal_connect (shortcut_button, "key-shortcut-edited",
                    G_CALLBACK (on_key_shortcut_edited),
                    row);
  g_signal_connect (shortcut_button, "button-press-event",
                    G_CALLBACK (on_key_shortcut_button_press_event),
                    row);

  gtk_container_add (GTK_CONTAINER (row), grid);

  cc_wacom_button_row_update (CC_WACOM_BUTTON_ROW (row));

  g_free (dir_name);

  return row;
}
