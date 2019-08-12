/*
 * Copyright © 2019 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "cc-default-app-row.h"

struct _CcDefaultAppRow
{
  GtkListBoxRow        parent_instance;

  GtkAppChooserButton *app_chooser;
  GtkBox              *box;
  GtkLabel            *title_label;

  gchar               *content_type;
  gchar               *extra_type_filter;
};

G_DEFINE_TYPE (CcDefaultAppRow, cc_default_app_row, GTK_TYPE_LIST_BOX_ROW)

static void
default_app_changed (CcDefaultAppRow *self)
{
  g_autoptr(GAppInfo) info = NULL;
  g_autoptr(GError) error = NULL;
  int i;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (self->app_chooser));

  if (g_app_info_set_as_default_for_type (info, self->content_type, &error) == FALSE)
    {
      g_warning ("Failed to set '%s' as the default application for '%s': %s",
                 g_app_info_get_name (info), self->content_type, error->message);
    }
  else
    {
      g_debug ("Set '%s' as the default handler for '%s'",
               g_app_info_get_name (info), self->content_type);
    }

  if (self->extra_type_filter)
    {
      g_auto(GStrv) entries = NULL;
      const char *const *mime_extra_type_filter;
      g_autoptr(GPtrArray) patterns = NULL;

      entries = g_strsplit (self->extra_type_filter, ";", -1);
      patterns = g_ptr_array_new_with_free_func ((GDestroyNotify) g_pattern_spec_free);
      for (i = 0; entries[i] != NULL; i++)
        {
          GPatternSpec *pattern = g_pattern_spec_new (entries[i]);
          g_ptr_array_add (patterns, pattern);
        }

      mime_extra_type_filter = g_app_info_get_supported_types (info);
      for (i = 0; mime_extra_type_filter && mime_extra_type_filter[i]; i++)
        {
          int j;
          gboolean matched = FALSE;
          g_autoptr(GError) local_error = NULL;

          for (j = 0; j < patterns->len; j++)
            {
              GPatternSpec *pattern = g_ptr_array_index (patterns, j);
              if (g_pattern_match_string (pattern, mime_extra_type_filter[i]))
                matched = TRUE;
            }
          if (!matched)
            continue;

          if (g_app_info_set_as_default_for_type (info, mime_extra_type_filter[i], &local_error) == FALSE)
            {
              g_warning ("Failed to set '%s' as the default application for secondary "
                         "content type '%s': %s",
                         g_app_info_get_name (info), mime_extra_type_filter[i], local_error->message);
            }
          else
            {
              g_debug ("Set '%s' as the default handler for '%s'",
              g_app_info_get_name (info), mime_extra_type_filter[i]);
            }
        }
    }
}

static void
cc_default_app_row_dispose (GObject *object)
{
  CcDefaultAppRow *self = CC_DEFAULT_APP_ROW (object);

  g_clear_pointer (&self->content_type, g_free);
  g_clear_pointer (&self->extra_type_filter, g_free);

  G_OBJECT_CLASS (cc_default_app_row_parent_class)->dispose (object);
}

void
cc_default_app_row_class_init (CcDefaultAppRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_default_app_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info/cc-default-app-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppRow, box);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppRow, title_label);
}

void
cc_default_app_row_init (CcDefaultAppRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcDefaultAppRow *
cc_default_app_row_new (const gchar *content_type, const gchar *extra_type_filter, const gchar *label, GtkSizeGroup *size_group)
{
  CcDefaultAppRow *self;
  g_autoptr(GList) cells = NULL;
  GList *cell;

  self = g_object_new (CC_TYPE_DEFAULT_APP_ROW, NULL);
  self->content_type = g_strdup (content_type);
  self->extra_type_filter = g_strdup (extra_type_filter);

  gtk_label_set_label (self->title_label, label);

  self->app_chooser = GTK_APP_CHOOSER_BUTTON (gtk_app_chooser_button_new (content_type));
  gtk_widget_show (GTK_WIDGET (self->app_chooser));
  gtk_app_chooser_button_set_show_default_item (self->app_chooser, TRUE);
  gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (self->app_chooser));
  gtk_size_group_add_widget (size_group, GTK_WIDGET (self->app_chooser));

  cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (self->app_chooser));
  for (cell = cells; cell; cell = cell->next)
    if (GTK_IS_CELL_RENDERER_TEXT (cell->data))
      g_object_set (G_OBJECT (cell->data), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  g_signal_connect_object (self->app_chooser, "changed", G_CALLBACK (default_app_changed), self, G_CONNECT_SWAPPED);

  return self;
}
