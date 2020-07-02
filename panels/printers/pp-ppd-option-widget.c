/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "pp-ppd-option-widget.h"
#include "pp-utils.h"

static void pp_ppd_option_widget_finalize (GObject *object);

static gboolean construct_widget   (PpPPDOptionWidget *self);
static void     update_widget      (PpPPDOptionWidget *self);
static void     update_widget_real (PpPPDOptionWidget *self);

struct _PpPPDOptionWidget
{
  GtkBox parent_instance;

  GtkWidget *switch_button;
  GtkWidget *combo;
  GtkWidget *image;
  GtkWidget *box;

  ppd_option_t *option;

  gchar *printer_name;
  gchar *option_name;

  cups_dest_t *destination;
  gboolean     destination_set;

  gchar    *ppd_filename;
  gboolean  ppd_filename_set;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (PpPPDOptionWidget, pp_ppd_option_widget, GTK_TYPE_BOX)

/* This list comes from Gtk+ */
static const struct {
  const char *keyword;
  const char *choice;
  const char *translation;
} ppd_choice_translations[] = {
  { "Duplex", "None", N_("One Sided") },
  /* Translators: this is an option of "Two Sided" */
  { "Duplex", "DuplexNoTumble", N_("Long Edge (Standard)") },
  /* Translators: this is an option of "Two Sided" */
  { "Duplex", "DuplexTumble", N_("Short Edge (Flip)") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Auto", N_("Auto Select") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "AutoSelect", N_("Auto Select") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Default", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "None", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "PrinterDefault", N_("Printer Default") },
  /* Translators: this is an option of "Paper Source" */
  { "InputSlot", "Unspecified", N_("Auto Select") },
  /* Translators: this is an option of "Resolution" */
  { "Resolution", "default", N_("Printer Default") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "EmbedFonts", N_("Embed GhostScript fonts only") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "Level1", N_("Convert to PS level 1") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "Level2", N_("Convert to PS level 2") },
  /* Translators: this is an option of "GhostScript" */
  { "PreFilter", "No", N_("No pre-filtering") },
};

static ppd_option_t *
cups_option_copy (ppd_option_t *option)
{
  ppd_option_t *result;
  gint          i;

  result = g_new0 (ppd_option_t, 1);

  *result = *option;

  result->choices = g_new (ppd_choice_t, result->num_choices);
  for (i = 0; i < result->num_choices; i++)
    {
      result->choices[i] = option->choices[i];
      result->choices[i].code = g_strdup (option->choices[i].code);
      result->choices[i].option = result;
    }

  return result;
}

static void
cups_option_free (ppd_option_t *option)
{
  gint i;

  if (option)
    {
      for (i = 0; i < option->num_choices; i++)
        g_free (option->choices[i].code);

      g_free (option->choices);
      g_free (option);
    }
}

static void
pp_ppd_option_widget_class_init (PpPPDOptionWidgetClass *class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (class);

  object_class->finalize = pp_ppd_option_widget_finalize;
}

static void
pp_ppd_option_widget_init (PpPPDOptionWidget *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_HORIZONTAL);
}

static void
pp_ppd_option_widget_finalize (GObject *object)
{
  PpPPDOptionWidget *self = PP_PPD_OPTION_WIDGET (object);

  g_cancellable_cancel (self->cancellable);
  if (self->ppd_filename)
    g_unlink (self->ppd_filename);

  g_clear_pointer (&self->option, cups_option_free);
  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->option_name, g_free);
  if (self->destination)
    {
      cupsFreeDests (1, self->destination);
      self->destination = NULL;
    }
  g_clear_pointer (&self->ppd_filename, g_free);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (pp_ppd_option_widget_parent_class)->finalize (object);
}

static const gchar *
ppd_choice_translate (ppd_choice_t *choice)
{
  const gchar *keyword = choice->option->keyword;
  gint         i;

  for (i = 0; i < G_N_ELEMENTS (ppd_choice_translations); i++)
    {
      if (g_strcmp0 (ppd_choice_translations[i].keyword, keyword) == 0 &&
	  g_strcmp0 (ppd_choice_translations[i].choice, choice->choice) == 0)
	return _(ppd_choice_translations[i].translation);
    }

  return choice->text;
}

GtkWidget *
pp_ppd_option_widget_new (ppd_option_t *option,
                          const gchar  *printer_name)
{
  PpPPDOptionWidget *self = NULL;

  if (option && printer_name)
    {
      self = g_object_new (PP_TYPE_PPD_OPTION_WIDGET, NULL);

      self->printer_name = g_strdup (printer_name);
      self->option = cups_option_copy (option);
      self->option_name = g_strdup (option->keyword);

      if (construct_widget (self))
        {
          update_widget_real (self);
        }
      else
        {
          g_object_ref_sink (self);
          g_object_unref (self);
          self = NULL;
        }
    }

  return (GtkWidget *) self;
}

enum {
  NAME_COLUMN,
  VALUE_COLUMN,
  N_COLUMNS
};

static GtkWidget *
combo_box_new (void)
{
  GtkCellRenderer *cell;
  GtkListStore    *store;
  GtkWidget       *combo_box;

  combo_box = gtk_combo_box_new ();

  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
  g_object_unref (store);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
                                  "text", NAME_COLUMN,
                                  NULL);

  return combo_box;
}

static void
combo_box_append (GtkWidget   *combo,
                  const gchar *display_text,
                  const gchar *value)
{
  GtkTreeModel *model;
  GtkListStore *store;
  GtkTreeIter   iter;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  store = GTK_LIST_STORE (model);

  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter,
                      NAME_COLUMN, display_text,
                      VALUE_COLUMN, value,
                      -1);
}

struct ComboSet {
  GtkComboBox *combo;
  const gchar *value;
};

static gboolean
set_cb (GtkTreeModel *model,
        GtkTreePath  *path,
        GtkTreeIter  *iter,
        gpointer      data)
{
  struct ComboSet  *set_data = data;
  g_autofree gchar *value = NULL;

  gtk_tree_model_get (model, iter, VALUE_COLUMN, &value, -1);

  if (strcmp (value, set_data->value) == 0)
    {
      gtk_combo_box_set_active_iter (set_data->combo, iter);
      return TRUE;
    }

  return FALSE;
}

static void
combo_box_set (GtkWidget   *combo,
               const gchar *value)
{
  struct ComboSet  set_data;
  GtkTreeModel    *model;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  set_data.combo = GTK_COMBO_BOX (combo);
  set_data.value = value;
  gtk_tree_model_foreach (model, set_cb, &set_data);
}

static char *
combo_box_get (GtkWidget *combo)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  gchar        *value = NULL;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter))
     gtk_tree_model_get (model, &iter, VALUE_COLUMN, &value, -1);

  return value;
}

static void
printer_add_option_async_cb (gboolean success,
                             gpointer user_data)
{
  PpPPDOptionWidget *self = user_data;

  update_widget (user_data);
  g_clear_object (&self->cancellable);
}

static void
switch_changed_cb (PpPPDOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);

  if (gtk_switch_get_active (GTK_SWITCH (self->switch_button)))
    values[0] = g_strdup ("True");
  else
    values[0] = g_strdup ("False");

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_object_unref (self->cancellable);
    }

  self->cancellable = g_cancellable_new ();
  printer_add_option_async (self->printer_name,
                            self->option_name,
                            values,
                            FALSE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static void
combo_changed_cb (PpPPDOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = combo_box_get (self->combo);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_object_unref (self->cancellable);
    }

  self->cancellable = g_cancellable_new ();
  printer_add_option_async (self->printer_name,
                            self->option_name,
                            values,
                            FALSE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static gboolean
construct_widget (PpPPDOptionWidget *self)
{
  gint                      i;

  /* Don't show options which has only one choice */
  if (self->option && self->option->num_choices > 1)
    {
      switch (self->option->ui)
        {
          case PPD_UI_BOOLEAN:
              self->switch_button = gtk_switch_new ();
              g_signal_connect_object (self->switch_button, "notify::active", G_CALLBACK (switch_changed_cb), self, G_CONNECT_SWAPPED);
              gtk_box_pack_start (GTK_BOX (self), self->switch_button, FALSE, FALSE, 0);
              break;

          case PPD_UI_PICKONE:
              self->combo = combo_box_new ();

              for (i = 0; i < self->option->num_choices; i++)
                {
                  combo_box_append (self->combo,
                                    ppd_choice_translate (&self->option->choices[i]),
                                    self->option->choices[i].choice);
                }

              gtk_box_pack_start (GTK_BOX (self), self->combo, FALSE, FALSE, 0);
              g_signal_connect_object (self->combo, "changed", G_CALLBACK (combo_changed_cb), self, G_CONNECT_SWAPPED);
              break;

          case PPD_UI_PICKMANY:
              self->combo = combo_box_new ();

              for (i = 0; i < self->option->num_choices; i++)
                {
                  combo_box_append (self->combo,
                                    ppd_choice_translate (&self->option->choices[i]),
                                    self->option->choices[i].choice);
                }

              gtk_box_pack_start (GTK_BOX (self), self->combo, TRUE, TRUE, 0);
              g_signal_connect_object (self->combo, "changed", G_CALLBACK (combo_changed_cb), self, G_CONNECT_SWAPPED);
              break;

          default:
              break;
        }

      self->image = gtk_image_new_from_icon_name ("dialog-warning-symbolic", GTK_ICON_SIZE_MENU);
      if (!self->image)
        self->image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_MENU);
      gtk_box_pack_start (GTK_BOX (self), self->image, FALSE, FALSE, 0);
      gtk_widget_set_no_show_all (GTK_WIDGET (self->image), TRUE);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
update_widget_real (PpPPDOptionWidget *self)
{
  ppd_option_t             *option = NULL, *iter;
  ppd_file_t               *ppd_file;
  gint                      i;

  if (self->option)
    {
      option = cups_option_copy (self->option);
      cups_option_free (self->option);
      self->option = NULL;
    }
  else if (self->ppd_filename)
    {
      ppd_file = ppdOpenFile (self->ppd_filename);
      ppdLocalize (ppd_file);

      if (ppd_file)
        {
          ppdMarkDefaults (ppd_file);

          for (iter = ppdFirstOption(ppd_file); iter; iter = ppdNextOption(ppd_file))
            {
              if (g_str_equal (iter->keyword, self->option_name))
                {
                  option = cups_option_copy (iter);
                  break;
                }
            }

          ppdClose (ppd_file);
        }

      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
      self->ppd_filename = NULL;
    }

  if (option)
    {
      g_autofree gchar *value = NULL;

      for (i = 0; i < option->num_choices; i++)
        if (option->choices[i].marked)
          value = g_strdup (option->choices[i].choice);

      if (value == NULL)
        value = g_strdup (option->defchoice);

      if (value)
        {
          switch (option->ui)
            {
              case PPD_UI_BOOLEAN:
                g_signal_handlers_block_by_func (self->switch_button, switch_changed_cb, self);
                if (g_ascii_strcasecmp (value, "True") == 0)
                  gtk_switch_set_active (GTK_SWITCH (self->switch_button), TRUE);
                else
                  gtk_switch_set_active (GTK_SWITCH (self->switch_button), FALSE);
                g_signal_handlers_unblock_by_func (self->switch_button, switch_changed_cb, self);
                break;

              case PPD_UI_PICKONE:
                g_signal_handlers_block_by_func (self->combo, combo_changed_cb, self);
                combo_box_set (self->combo, value);
                g_signal_handlers_unblock_by_func (self->combo, combo_changed_cb, self);
                break;

              case PPD_UI_PICKMANY:
                g_signal_handlers_block_by_func (self->combo, combo_changed_cb, self);
                combo_box_set (self->combo, value);
                g_signal_handlers_unblock_by_func (self->combo, combo_changed_cb, self);
                break;

              default:
                break;
            }
        }

      if (option->conflicted)
        gtk_widget_show (self->image);
      else
        gtk_widget_hide (self->image);
    }

  cups_option_free (option);
}

static void
get_named_dest_cb (cups_dest_t *dest,
                   gpointer     user_data)
{
  PpPPDOptionWidget *self = user_data;

  if (self->destination)
    cupsFreeDests (1, self->destination);

  self->destination = dest;
  self->destination_set = TRUE;

  if (self->ppd_filename_set)
    {
      update_widget_real (self);
    }
}

static void
printer_get_ppd_cb (const gchar *ppd_filename,
                    gpointer     user_data)
{
  PpPPDOptionWidget *self = user_data;

  if (self->ppd_filename)
    {
      g_unlink (self->ppd_filename);
      g_free (self->ppd_filename);
    }

  self->ppd_filename = g_strdup (ppd_filename);
  self->ppd_filename_set = TRUE;

  if (self->destination_set)
    {
      update_widget_real (self);
    }
}

static void
update_widget (PpPPDOptionWidget *self)
{
  self->ppd_filename_set = FALSE;
  self->destination_set = FALSE;

  get_named_dest_async (self->printer_name,
                        get_named_dest_cb,
                        self);

  printer_get_ppd_async (self->printer_name,
                         NULL,
                         0,
                         printer_get_ppd_cb,
                         self);
}
