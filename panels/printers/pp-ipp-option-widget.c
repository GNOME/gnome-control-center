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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#define PP_IPP_OPTION_WIDGET_GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), PP_TYPE_IPP_OPTION_WIDGET, PpIPPOptionWidgetPrivate))

static void pp_ipp_option_widget_finalize (GObject *object);

static gboolean construct_widget   (PpIPPOptionWidget *widget);
static void     update_widget      (PpIPPOptionWidget *widget);
static void     update_widget_real (PpIPPOptionWidget *widget);

struct PpIPPOptionWidgetPrivate
{
  GtkWidget *switch_button;
  GtkWidget *spin_button;
  GtkWidget *combo;
  GtkWidget *box;

  IPPAttribute *option_supported;
  IPPAttribute *option_default;

  gchar *printer_name;
  gchar *option_name;

  GHashTable *ipp_attribute;
};

G_DEFINE_TYPE (PpIPPOptionWidget, pp_ipp_option_widget, GTK_TYPE_HBOX)

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

  g_type_class_add_private (class, sizeof (PpIPPOptionWidgetPrivate));
}

static void
pp_ipp_option_widget_init (PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate *priv;

  priv = widget->priv = PP_IPP_OPTION_WIDGET_GET_PRIVATE (widget);

  priv->switch_button = NULL;
  priv->spin_button = NULL;
  priv->combo = NULL;
  priv->box = NULL;

  priv->printer_name = NULL;
  priv->option_name = NULL;

  priv->option_supported = NULL;
  priv->option_default = NULL;

  priv->ipp_attribute = NULL;
}

static void
pp_ipp_option_widget_finalize (GObject *object)
{
  PpIPPOptionWidget *widget = PP_IPP_OPTION_WIDGET (object);
  PpIPPOptionWidgetPrivate *priv = widget->priv;

  if (priv)
    {
      if (priv->option_name)
        {
          g_free (priv->option_name);
          priv->option_name = NULL;
        }

      if (priv->printer_name)
        {
          g_free (priv->printer_name);
          priv->printer_name = NULL;
        }

      if (priv->option_supported)
        {
          ipp_attribute_free (priv->option_supported);
          priv->option_supported = NULL;
        }

      if (priv->option_default)
        {
          ipp_attribute_free (priv->option_default);
          priv->option_default = NULL;
        }

      if (priv->ipp_attribute)
        {
          g_hash_table_unref (priv->ipp_attribute);
          priv->ipp_attribute = NULL;
        }
    }

  G_OBJECT_CLASS (pp_ipp_option_widget_parent_class)->finalize (object);
}

GtkWidget *
pp_ipp_option_widget_new (IPPAttribute *attr_supported,
                          IPPAttribute *attr_default,
                          const gchar  *option_name,
                          const gchar  *printer)
{
  PpIPPOptionWidgetPrivate *priv;
  PpIPPOptionWidget        *widget = NULL;

  if (attr_supported && option_name && printer)
    {
      widget = g_object_new (PP_TYPE_IPP_OPTION_WIDGET, NULL);

      priv = PP_IPP_OPTION_WIDGET_GET_PRIVATE (widget);

      priv->printer_name = g_strdup (printer);
      priv->option_name = g_strdup (option_name);
      priv->option_supported = ipp_attribute_copy (attr_supported);
      priv->option_default = ipp_attribute_copy (attr_default);

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
  update_widget (user_data);
}

static void
switch_changed_cb (GtkWidget         *switch_button,
                   GParamSpec        *pspec,
                   PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate  *priv = widget->priv;
  gchar                    **values;

  values = g_new0 (gchar *, 2);

  if (gtk_switch_get_active (GTK_SWITCH (switch_button)))
    values[0] = g_strdup ("True");
  else
    values[0] = g_strdup ("False");

  printer_add_option_async (priv->printer_name,
                            priv->option_name,
                            values,
                            TRUE,
                            NULL,
                            printer_add_option_async_cb,
                            widget);

  g_strfreev (values);
}

static void
combo_changed_cb (GtkWidget         *combo,
                  PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate  *priv = widget->priv;
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = combo_box_get (combo);

  printer_add_option_async (priv->printer_name,
                            priv->option_name,
                            values,
                            TRUE,
                            NULL,
                            printer_add_option_async_cb,
                            widget);

  g_strfreev (values);
}

static void
spin_button_changed_cb (GtkWidget         *spin_button,
                        PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate  *priv = widget->priv;
  gchar                    **values;

  values = g_new0 (gchar *, 2);
  values[0] = g_strdup_printf ("%d", gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin_button)));

  printer_add_option_async (priv->printer_name,
                            priv->option_name,
                            values,
                            TRUE,
                            NULL,
                            printer_add_option_async_cb,
                            widget);

  g_strfreev (values);
}

static gboolean
construct_widget (PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate *priv = widget->priv;
  gboolean                  trivial_option = FALSE;
  gboolean                  result = FALSE;
  gchar                    *value;
  gint                      i;

  if (priv->option_supported)
    {
      switch (priv->option_supported->attribute_type)
        {
          case IPP_ATTRIBUTE_TYPE_INTEGER:
            if (priv->option_supported->num_of_values <= 1)
              trivial_option = TRUE;
            break;

          case IPP_ATTRIBUTE_TYPE_STRING:
            if (priv->option_supported->num_of_values <= 1)
              trivial_option = TRUE;
            break;

          case IPP_ATTRIBUTE_TYPE_RANGE:
            if (priv->option_supported->attribute_values[0].lower_range ==
                priv->option_supported->attribute_values[0].upper_range)
              trivial_option = TRUE;
            break;
        }

      if (!trivial_option)
        {
          switch (priv->option_supported->attribute_type)
            {
              case IPP_ATTRIBUTE_TYPE_BOOLEAN:
                  priv->switch_button = gtk_switch_new ();

                  gtk_box_pack_start (GTK_BOX (widget), priv->switch_button, FALSE, FALSE, 0);
                  g_signal_connect (priv->switch_button, "notify::active", G_CALLBACK (switch_changed_cb), widget);
                  break;

              case IPP_ATTRIBUTE_TYPE_INTEGER:
                  priv->combo = combo_box_new ();

                  for (i = 0; i < priv->option_supported->num_of_values; i++)
                    {
                      value = g_strdup_printf ("%d", priv->option_supported->attribute_values[i].integer_value);
                      combo_box_append (priv->combo,
                                        ipp_choice_translate (priv->option_name,
                                                              value),
                                        value);
                      g_free (value);
                    }

                  gtk_box_pack_start (GTK_BOX (widget), priv->combo, FALSE, FALSE, 0);
                  g_signal_connect (priv->combo, "changed", G_CALLBACK (combo_changed_cb), widget);
                  break;

              case IPP_ATTRIBUTE_TYPE_STRING:
                  priv->combo = combo_box_new ();

                  for (i = 0; i < priv->option_supported->num_of_values; i++)
                    combo_box_append (priv->combo,
                                      ipp_choice_translate (priv->option_name,
                                                            priv->option_supported->attribute_values[i].string_value),
                                      priv->option_supported->attribute_values[i].string_value);

                  gtk_box_pack_start (GTK_BOX (widget), priv->combo, FALSE, FALSE, 0);
                  g_signal_connect (priv->combo, "changed", G_CALLBACK (combo_changed_cb), widget);
                  break;

              case IPP_ATTRIBUTE_TYPE_RANGE:
                  priv->spin_button = gtk_spin_button_new_with_range (
                                        priv->option_supported->attribute_values[0].lower_range,
                                        priv->option_supported->attribute_values[0].upper_range,
                                        1);

                  gtk_box_pack_start (GTK_BOX (widget), priv->spin_button, FALSE, FALSE, 0);
                  g_signal_connect (priv->spin_button, "value-changed", G_CALLBACK (spin_button_changed_cb), widget);
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
update_widget_real (PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate *priv = widget->priv;
  IPPAttribute             *attr = NULL;
  gchar                    *value;
  gchar                    *attr_name;

  if (priv->option_default)
    {
      attr = ipp_attribute_copy (priv->option_default);

      ipp_attribute_free (priv->option_default);
      priv->option_default = NULL;
    }
  else if (priv->ipp_attribute)
    {
      attr_name = g_strdup_printf ("%s-default", priv->option_name);
      attr = ipp_attribute_copy (g_hash_table_lookup (priv->ipp_attribute, attr_name));

      g_free (attr_name);
      g_hash_table_unref (priv->ipp_attribute);
      priv->ipp_attribute = NULL;
    }

  switch (priv->option_supported->attribute_type)
    {
      case IPP_ATTRIBUTE_TYPE_BOOLEAN:
        g_signal_handlers_block_by_func (priv->switch_button, switch_changed_cb, widget);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_BOOLEAN)
          {
            gtk_switch_set_active (GTK_SWITCH (priv->switch_button),
                                   attr->attribute_values[0].boolean_value);
          }

        g_signal_handlers_unblock_by_func (priv->switch_button, switch_changed_cb, widget);
        break;

      case IPP_ATTRIBUTE_TYPE_INTEGER:
        g_signal_handlers_block_by_func (priv->combo, combo_changed_cb, widget);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_INTEGER)
          {
            value = g_strdup_printf ("%d", attr->attribute_values[0].integer_value);
            combo_box_set (priv->combo, value);
            g_free (value);
          }
        else
          {
            value = g_strdup_printf ("%d", priv->option_supported->attribute_values[0].integer_value);
            combo_box_set (priv->combo, value);
            g_free (value);
          }

        g_signal_handlers_unblock_by_func (priv->combo, combo_changed_cb, widget);
        break;

      case IPP_ATTRIBUTE_TYPE_STRING:
        g_signal_handlers_block_by_func (priv->combo, combo_changed_cb, widget);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_STRING)
          {
            combo_box_set (priv->combo, attr->attribute_values[0].string_value);
          }
        else
          {
            combo_box_set (priv->combo, priv->option_supported->attribute_values[0].string_value);
          }

        g_signal_handlers_unblock_by_func (priv->combo, combo_changed_cb, widget);
        break;

      case IPP_ATTRIBUTE_TYPE_RANGE:
        g_signal_handlers_block_by_func (priv->spin_button, spin_button_changed_cb, widget);

        if (attr && attr->num_of_values > 0 &&
            attr->attribute_type == IPP_ATTRIBUTE_TYPE_INTEGER)
          {
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->spin_button),
                                       attr->attribute_values[0].integer_value);
          }
        else
          {
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->spin_button),
                                       priv->option_supported->attribute_values[0].lower_range);
          }

        g_signal_handlers_unblock_by_func (priv->spin_button, spin_button_changed_cb, widget);
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
  PpIPPOptionWidget        *widget = (PpIPPOptionWidget *) user_data;
  PpIPPOptionWidgetPrivate *priv = widget->priv;

  if (priv->ipp_attribute)
    g_hash_table_unref (priv->ipp_attribute);

  priv->ipp_attribute = table;

  update_widget_real (widget);
}

static void
update_widget (PpIPPOptionWidget *widget)
{
  PpIPPOptionWidgetPrivate  *priv = widget->priv;
  gchar                    **attributes_names;

  attributes_names = g_new0 (gchar *, 2);
  attributes_names[0] = g_strdup_printf ("%s-default", priv->option_name);

  get_ipp_attributes_async (priv->printer_name,
                            attributes_names,
                            get_ipp_attributes_cb,
                            widget);

  g_strfreev (attributes_names);
}
