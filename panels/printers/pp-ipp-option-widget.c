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

#include "pp-ipp-option-widget.h"
#include "pp-utils.h"

static void pp_ipp_option_widget_finalize (GObject *object);

static gboolean construct_widget   (PpIPPOptionWidget *self);
static void     update_widget      (PpIPPOptionWidget *self);
static void     update_widget_real (PpIPPOptionWidget *self);

struct _PpIPPOptionWidget
{
  GtkBox parent_instance;

  GtkWidget *switch_button;
  GtkWidget *spin_button;
  GtkWidget *combo;
  GtkWidget *box;

  IPPAttribute *option_supported;
  IPPAttribute *option_default;

  gchar *printer_name;
  gchar *option_name;

  GHashTable *ipp_attribute;

  GCancellable *cancellable;
};

G_DEFINE_TYPE (PpIPPOptionWidget, pp_ipp_option_widget, GTK_TYPE_BOX)

static const struct {
  const char *keyword;
  const char *choice;
  const char *translation;
} ipp_choice_translations[] = {
  /* Translators: this is an option of "Two Sided" */
  { "sides", "one-sided", N_("One Sided") },
  /* Translators: this is an option of "Two Sided" */
  { "sides", "two-sided-long-edge", N_("Long Edge (Standard)") },
  /* Translators: this is an option of "Two Sided" */
  { "sides", "two-sided-short-edge", N_("Short Edge (Flip)") },
  /* Translators: this is an option of "Orientation" */
  { "orientation-requested", "3", N_("Portrait") },
  /* Translators: this is an option of "Orientation" */
  { "orientation-requested", "4", N_("Landscape") },
  /* Translators: this is an option of "Orientation" */
  { "orientation-requested", "5", N_("Reverse landscape") },
  /* Translators: this is an option of "Orientation" */
  { "orientation-requested", "6", N_("Reverse portrait") },
};

static const gchar *
ipp_choice_translate (const gchar *option,
                      const gchar *choice)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (ipp_choice_translations); i++)
    {
      if (g_strcmp0 (ipp_choice_translations[i].keyword, option) == 0 &&
	  g_strcmp0 (ipp_choice_translations[i].choice, choice) == 0)
	return _(ipp_choice_translations[i].translation);
    }

  return choice;
}

static void
pp_ipp_option_widget_class_init (PpIPPOptionWidgetClass *class)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (class);

  object_class->finalize = pp_ipp_option_widget_finalize;
}

static void
pp_ipp_option_widget_init (PpIPPOptionWidget *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self),
                                  GTK_ORIENTATION_HORIZONTAL);

  self->switch_button = NULL;
  self->spin_button = NULL;
  self->combo = NULL;
  self->box = NULL;

  self->printer_name = NULL;
  self->option_name = NULL;

  self->option_supported = NULL;
  self->option_default = NULL;

  self->ipp_attribute = NULL;
}

static void
pp_ipp_option_widget_finalize (GObject *object)
{
  PpIPPOptionWidget *self = PP_IPP_OPTION_WIDGET (object);

  g_cancellable_cancel (self->cancellable);

  g_clear_pointer (&self->option_name, g_free);
  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->option_supported, ipp_attribute_free);
  g_clear_pointer (&self->option_default, ipp_attribute_free);
  g_clear_pointer (&self->ipp_attribute, g_hash_table_unref);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (pp_ipp_option_widget_parent_class)->finalize (object);
}

GtkWidget *
pp_ipp_option_widget_new (IPPAttribute *attr_supported,
                          IPPAttribute *attr_default,
                          const gchar  *option_name,
                          const gchar  *printer)
{
  PpIPPOptionWidget *self = NULL;

  if (attr_supported && option_name && printer)
    {
      self = g_object_new (PP_TYPE_IPP_OPTION_WIDGET, NULL);

      self->printer_name = g_strdup (printer);
      self->option_name = g_strdup (option_name);
      self->option_supported = ipp_attribute_copy (attr_supported);
      self->option_default = ipp_attribute_copy (attr_default);

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
  PpIPPOptionWidget *self = user_data;

  update_widget (user_data);
  g_clear_object (&self->cancellable);
}

static void
switch_changed_cb (GtkWidget         *switch_button,
                   GParamSpec        *pspec,
                   PpIPPOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);

  if (gtk_switch_get_active (GTK_SWITCH (switch_button)))
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
                            TRUE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static void
combo_changed_cb (GtkWidget         *combo,
                  PpIPPOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = combo_box_get (combo);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_object_unref (self->cancellable);
    }

  self->cancellable = g_cancellable_new ();
  printer_add_option_async (self->printer_name,
                            self->option_name,
                            values,
                            TRUE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static void
spin_button_changed_cb (GtkWidget         *spin_button,
                        PpIPPOptionWidget *self)
{
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = g_strdup_printf ("%d", gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin_button)));

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_object_unref (self->cancellable);
    }

  self->cancellable = g_cancellable_new ();
  printer_add_option_async (self->printer_name,
                            self->option_name,
                            values,
                            TRUE,
                            self->cancellable,
                            printer_add_option_async_cb,
                            self);

  g_strfreev (values);
}

static gboolean
construct_widget (PpIPPOptionWidget *self)
{
  gboolean                  trivial_option = FALSE;
  gboolean                  result = FALSE;
  gint                      i;

  if (self->option_supported)
    {
      switch (self->option_supported->attribute_type)
        {
          case IPP_ATTRIBUTE_TYPE_INTEGER:
            if (self->option_supported->num_of_values <= 1)
              trivial_option = TRUE;
            break;

          case IPP_ATTRIBUTE_TYPE_STRING:
            if (self->option_supported->num_of_values <= 1)
              trivial_option = TRUE;
            break;

          case IPP_ATTRIBUTE_TYPE_RANGE:
            if (self->option_supported->attribute_values[0].lower_range ==
                self->option_supported->attribute_values[0].upper_range)
              trivial_option = TRUE;
            break;
        }

      if (!trivial_option)
        {
          switch (self->option_supported->attribute_type)
            {
              case IPP_ATTRIBUTE_TYPE_BOOLEAN:
                  self->switch_button = gtk_switch_new ();

                  gtk_box_pack_start (GTK_BOX (self), self->switch_button, FALSE, FALSE, 0);
                  g_signal_connect (self->switch_button, "notify::active", G_CALLBACK (switch_changed_cb), self);
                  break;

              case IPP_ATTRIBUTE_TYPE_INTEGER:
                  self->combo = combo_box_new ();

                  for (i = 0; i < self->option_supported->num_of_values; i++)
                    {
                      g_autofree gchar *value = NULL;

                      value = g_strdup_printf ("%d", self->option_supported->attribute_values[i].integer_value);
                      combo_box_append (self->combo,
                                        ipp_choice_translate (self->option_name,
                                                              value),
                                        value);
                    }

                  gtk_box_pack_start (GTK_BOX (self), self->combo, FALSE, FALSE, 0);
                  g_signal_connect (self->combo, "changed", G_CALLBACK (combo_changed_cb), self);
                  break;

              case IPP_ATTRIBUTE_TYPE_STRING:
                  self->combo = combo_box_new ();

                  for (i = 0; i < self->option_supported->num_of_values; i++)
                    combo_box_append (self->combo,
                                      ipp_choice_translate (self->option_name,
                                                            self->option_supported->attribute_values[i].string_value),
                                      self->option_supported->attribute_values[i].string_value);

                  gtk_box_pack_start (GTK_BOX (self), self->combo, FALSE, FALSE, 0);
                  g_signal_connect (self->combo, "changed", G_CALLBACK (combo_changed_cb), self);
                  break;

              case IPP_ATTRIBUTE_TYPE_RANGE:
                  self->spin_button = gtk_spin_button_new_with_range (
                                        self->option_supported->attribute_values[0].lower_range,
                                        self->option_supported->attribute_values[0].upper_range,
                                        1);

                  gtk_box_pack_start (GTK_BOX (self), self->spin_button, FALSE, FALSE, 0);
                  g_signal_connect (self->spin_button, "value-changed", G_CALLBACK (spin_button_changed_cb), self);
                  break;

              default:
                  break;
            }

          result = TRUE;
        }
    }

  return result;
}

static void
update_widget_real (PpIPPOptionWidget *self)
{
  IPPAttribute             *attr = NULL;

  if (self->option_default)
    {
      attr = ipp_attribute_copy (self->option_default);

      ipp_attribute_free (self->option_default);
      self->option_default = NULL;
    }
  else if (self->ipp_attribute)
    {
      g_autofree gchar *attr_name = g_strdup_printf ("%s-default", self->option_name);
      attr = ipp_attribute_copy (g_hash_table_lookup (self->ipp_attribute, attr_name));

      g_hash_table_unref (self->ipp_attribute);
      self->ipp_attribute = NULL;
    }

  switch (self->option_supported->attribute_type)
    {
      case IPP_ATTRIBUTE_TYPE_BOOLEAN:
        g_signal_handlers_block_by_func (self->switch_button, switch_changed_cb, self);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_BOOLEAN)
          {
            gtk_switch_set_active (GTK_SWITCH (self->switch_button),
                                   attr->attribute_values[0].boolean_value);
          }

        g_signal_handlers_unblock_by_func (self->switch_button, switch_changed_cb, self);
        break;

      case IPP_ATTRIBUTE_TYPE_INTEGER:
        g_signal_handlers_block_by_func (self->combo, combo_changed_cb, self);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_INTEGER)
          {
            g_autofree gchar *value = g_strdup_printf ("%d", attr->attribute_values[0].integer_value);
            combo_box_set (self->combo, value);
          }
        else
          {
            g_autofree gchar *value = g_strdup_printf ("%d", self->option_supported->attribute_values[0].integer_value);
            combo_box_set (self->combo, value);
          }

        g_signal_handlers_unblock_by_func (self->combo, combo_changed_cb, self);
        break;

      case IPP_ATTRIBUTE_TYPE_STRING:
        g_signal_handlers_block_by_func (self->combo, combo_changed_cb, self);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_STRING)
          {
            combo_box_set (self->combo, attr->attribute_values[0].string_value);
          }
        else
          {
            combo_box_set (self->combo, self->option_supported->attribute_values[0].string_value);
          }

        g_signal_handlers_unblock_by_func (self->combo, combo_changed_cb, self);
        break;

      case IPP_ATTRIBUTE_TYPE_RANGE:
        g_signal_handlers_block_by_func (self->spin_button, spin_button_changed_cb, self);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_INTEGER)
          {
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->spin_button),
                                       attr->attribute_values[0].integer_value);
          }
        else
          {
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->spin_button),
                                       self->option_supported->attribute_values[0].lower_range);
          }

        g_signal_handlers_unblock_by_func (self->spin_button, spin_button_changed_cb, self);
        break;

      default:
        break;
    }

  ipp_attribute_free (attr);
}

static void
get_ipp_attributes_cb (GHashTable *table,
                       gpointer    user_data)
{
  PpIPPOptionWidget *self = user_data;

  if (self->ipp_attribute)
    g_hash_table_unref (self->ipp_attribute);

  self->ipp_attribute = table;

  update_widget_real (self);
}

static void
update_widget (PpIPPOptionWidget *self)
{
  gchar                    **attributes_names;

  attributes_names = g_new0 (gchar *, 2);
  attributes_names[0] = g_strdup_printf ("%s-default", self->option_name);

  get_ipp_attributes_async (self->printer_name,
                            attributes_names,
                            get_ipp_attributes_cb,
                            self);

  g_strfreev (attributes_names);
}
