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

static gboolean construct_widget   (PpPPDOptionWidget *widget);
static void     update_widget      (PpPPDOptionWidget *widget);
static void     update_widget_real (PpPPDOptionWidget *widget);

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
pp_ppd_option_widget_init (PpPPDOptionWidget *widget)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget),
                                  GTK_ORIENTATION_HORIZONTAL);

  widget->switch_button = NULL;
  widget->combo = NULL;
  widget->image = NULL;
  widget->box = NULL;

  widget->printer_name = NULL;
  widget->option_name = NULL;

  widget->destination = NULL;
  widget->destination_set = FALSE;

  widget->ppd_filename = NULL;
  widget->ppd_filename_set = FALSE;
}

static void
pp_ppd_option_widget_finalize (GObject *object)
{
  PpPPDOptionWidget        *widget = PP_PPD_OPTION_WIDGET (object);

  g_cancellable_cancel (widget->cancellable);
  if (widget->ppd_filename)
    g_unlink (widget->ppd_filename);

  g_clear_pointer (&widget->option, cups_option_free);
  g_clear_pointer (&widget->printer_name, g_free);
  g_clear_pointer (&widget->option_name, g_free);
  if (widget->destination)
    {
      cupsFreeDests (1, widget->destination);
      widget->destination = NULL;
    }
  g_clear_pointer (&widget->ppd_filename, g_free);
  g_clear_object (&widget->cancellable);

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
  PpPPDOptionWidget        *widget = NULL;

  if (option && printer_name)
    {
      widget = g_object_new (PP_TYPE_PPD_OPTION_WIDGET, NULL);

      widget->printer_name = g_strdup (printer_name);
      widget->option = cups_option_copy (option);
      widget->option_name = g_strdup (option->keyword);

      if (construct_widget (widget))
        {
          update_widget_real (widget);
        }
      else
        {
          g_object_ref_sink (widget);
          g_object_unref (widget);
          widget = NULL;
        }
    }

  return (GtkWidget *) widget;
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
  struct ComboSet *set_data = data;
  gboolean         found;
  char            *value;

  gtk_tree_model_get (model, iter, VALUE_COLUMN, &value, -1);
  found = (strcmp (value, set_data->value) == 0);
  g_free (value);

  if (found)
    gtk_combo_box_set_active_iter (set_data->combo, iter);

  return found;
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
  PpPPDOptionWidget        *widget = (PpPPDOptionWidget *) user_data;

  update_widget (user_data);
  g_clear_object (&widget->cancellable);
}

static void
switch_changed_cb (GtkWidget         *switch_button,
                   GParamSpec        *pspec,
                   PpPPDOptionWidget *widget)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);

  if (gtk_switch_get_active (GTK_SWITCH (switch_button)))
    values[0] = g_strdup ("True");
  else
    values[0] = g_strdup ("False");

  if (widget->cancellable)
    {
      g_cancellable_cancel (widget->cancellable);
      g_object_unref (widget->cancellable);
    }

  widget->cancellable = g_cancellable_new ();
  printer_add_option_async (widget->printer_name,
                            widget->option_name,
                            values,
                            FALSE,
                            widget->cancellable,
                            printer_add_option_async_cb,
                            widget);

  g_strfreev (values);
}

static void
combo_changed_cb (GtkWidget         *combo,
                  PpPPDOptionWidget *widget)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = combo_box_get (combo);

  if (widget->cancellable)
    {
      g_cancellable_cancel (widget->cancellable);
      g_object_unref (widget->cancellable);
    }

  widget->cancellable = g_cancellable_new ();
  printer_add_option_async (widget->printer_name,
                            widget->option_name,
                            values,
                            FALSE,
                            widget->cancellable,
                            printer_add_option_async_cb,
                            widget);

  g_strfreev (values);
}

static gboolean
construct_widget (PpPPDOptionWidget *widget)
{
  gint                      i;

  /* Don't show options which has only one choice */
  if (widget->option && widget->option->num_choices > 1)
    {
      switch (widget->option->ui)
        {
          case PPD_UI_BOOLEAN:
              widget->switch_button = gtk_switch_new ();
              g_signal_connect (widget->switch_button, "notify::active", G_CALLBACK (switch_changed_cb), widget);
              gtk_box_pack_start (GTK_BOX (widget), widget->switch_button, FALSE, FALSE, 0);
              break;

          case PPD_UI_PICKONE:
              widget->combo = combo_box_new ();

              for (i = 0; i < widget->option->num_choices; i++)
                {
                  combo_box_append (widget->combo,
                                    ppd_choice_translate (&widget->option->choices[i]),
                                    widget->option->choices[i].choice);
                }

              gtk_box_pack_start (GTK_BOX (widget), widget->combo, FALSE, FALSE, 0);
              g_signal_connect (widget->combo, "changed", G_CALLBACK (combo_changed_cb), widget);
              break;

          case PPD_UI_PICKMANY:
              widget->combo = combo_box_new ();

              for (i = 0; i < widget->option->num_choices; i++)
                {
                  combo_box_append (widget->combo,
                                    ppd_choice_translate (&widget->option->choices[i]),
                                    widget->option->choices[i].choice);
                }

              gtk_box_pack_start (GTK_BOX (widget), widget->combo, TRUE, TRUE, 0);
              g_signal_connect (widget->combo, "changed", G_CALLBACK (combo_changed_cb), widget);
              break;

          default:
              break;
        }

      widget->image = gtk_image_new_from_icon_name ("dialog-warning-symbolic", GTK_ICON_SIZE_MENU);
      if (!widget->image)
        widget->image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_MENU);
      gtk_box_pack_start (GTK_BOX (widget), widget->image, FALSE, FALSE, 0);
      gtk_widget_set_no_show_all (GTK_WIDGET (widget->image), TRUE);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static void
update_widget_real (PpPPDOptionWidget *widget)
{
  ppd_option_t             *option = NULL, *iter;
  ppd_file_t               *ppd_file;
  gchar                    *value = NULL;
  gint                      i;

  if (widget->option)
    {
      option = cups_option_copy (widget->option);
      cups_option_free (widget->option);
      widget->option = NULL;
    }
  else if (widget->ppd_filename)
    {
      ppd_file = ppdOpenFile (widget->ppd_filename);
      ppdLocalize (ppd_file);

      if (ppd_file)
        {
          ppdMarkDefaults (ppd_file);

          for (iter = ppdFirstOption(ppd_file); iter; iter = ppdNextOption(ppd_file))
            {
              if (g_str_equal (iter->keyword, widget->option_name))
                {
                  option = cups_option_copy (iter);
                  break;
                }
            }

          ppdClose (ppd_file);
        }

      g_unlink (widget->ppd_filename);
      g_free (widget->ppd_filename);
      widget->ppd_filename = NULL;
    }

  if (option)
    {
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
                g_signal_handlers_block_by_func (widget->switch_button, switch_changed_cb, widget);
                if (g_ascii_strcasecmp (value, "True") == 0)
                  gtk_switch_set_active (GTK_SWITCH (widget->switch_button), TRUE);
                else
                  gtk_switch_set_active (GTK_SWITCH (widget->switch_button), FALSE);
                g_signal_handlers_unblock_by_func (widget->switch_button, switch_changed_cb, widget);
                break;

              case PPD_UI_PICKONE:
                g_signal_handlers_block_by_func (widget->combo, combo_changed_cb, widget);
                combo_box_set (widget->combo, value);
                g_signal_handlers_unblock_by_func (widget->combo, combo_changed_cb, widget);
                break;

              case PPD_UI_PICKMANY:
                g_signal_handlers_block_by_func (widget->combo, combo_changed_cb, widget);
                combo_box_set (widget->combo, value);
                g_signal_handlers_unblock_by_func (widget->combo, combo_changed_cb, widget);
                break;

              default:
                break;
            }

          g_free (value);
        }

      if (option->conflicted)
        gtk_widget_show (widget->image);
      else
        gtk_widget_hide (widget->image);
    }

  cups_option_free (option);
}

static void
get_named_dest_cb (cups_dest_t *dest,
                   gpointer     user_data)
{
  PpPPDOptionWidget        *widget = (PpPPDOptionWidget *) user_data;

  if (widget->destination)
    cupsFreeDests (1, widget->destination);

  widget->destination = dest;
  widget->destination_set = TRUE;

  if (widget->ppd_filename_set)
    {
      update_widget_real (widget);
    }
}

static void
printer_get_ppd_cb (const gchar *ppd_filename,
                    gpointer     user_data)
{
  PpPPDOptionWidget        *widget = (PpPPDOptionWidget *) user_data;

  if (widget->ppd_filename)
    {
      g_unlink (widget->ppd_filename);
      g_free (widget->ppd_filename);
    }

  widget->ppd_filename = g_strdup (ppd_filename);
  widget->ppd_filename_set = TRUE;

  if (widget->destination_set)
    {
      update_widget_real (widget);
    }
}

static void
update_widget (PpPPDOptionWidget *widget)
{
  widget->ppd_filename_set = FALSE;
  widget->destination_set = FALSE;

  get_named_dest_async (widget->printer_name,
                        get_named_dest_cb,
                        widget);

  printer_get_ppd_async (widget->printer_name,
                         NULL,
                         0,
                         printer_get_ppd_cb,
                         widget);
}
