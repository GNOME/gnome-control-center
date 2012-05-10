/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/XKBlib.h>
#include <X11/extensions/XKBrules.h>

#include <gdk/gdkx.h>

#include "xkb-rules-db.h"

#ifndef DFLT_XKB_CONFIG_ROOT
#define DFLT_XKB_CONFIG_ROOT "/usr/share/X11/xkb"
#endif
#ifndef DFLT_XKB_RULES_FILE
#define DFLT_XKB_RULES_FILE "base"
#endif
#ifndef DFLT_XKB_LAYOUT
#define DFLT_XKB_LAYOUT "us"
#endif
#ifndef DFLT_XKB_MODEL
#define DFLT_XKB_MODEL "pc105"
#endif

typedef struct _Layout Layout;
struct _Layout
{
  gchar *id;
  gchar *xkb_name;
  gchar *short_id;
  gboolean is_variant;
  const Layout *main_layout;
};

static GHashTable *layouts_by_short_id = NULL;
static GHashTable *layouts_by_iso639 = NULL;
static GHashTable *layouts_table = NULL;
static Layout *current_parser_layout = NULL;
static Layout *current_parser_variant = NULL;
static gchar **current_parser_text = NULL;
static gchar *current_parser_iso639Id = NULL;

static void
free_layout (gpointer data)
{
  Layout *layout = data;

  g_free (layout->id);
  g_free (layout->xkb_name);
  g_free (layout->short_id);
  g_free (layout);
}

static void
get_xkb_values (gchar            **rules,
                XkbRF_VarDefsRec  *var_defs)
{
  Display *display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
  *rules = NULL;

  /* Get it from the X property or fallback on defaults */
  if (!XkbRF_GetNamesProp (display, rules, var_defs) || !*rules) {
    *rules = strdup (DFLT_XKB_RULES_FILE);
    var_defs->model = strdup (DFLT_XKB_MODEL);
    var_defs->layout = strdup (DFLT_XKB_LAYOUT);
    var_defs->variant = NULL;
    var_defs->options = NULL;
  }
}

static void
free_xkb_var_defs (XkbRF_VarDefsRec *p)
{
  if (p->model)
    free (p->model);
  if (p->layout)
    free (p->layout);
  if (p->variant)
    free (p->variant);
  if (p->options)
    free (p->options);
  free (p);
}

static gchar *
get_rules_file_path (void)
{
  XkbRF_VarDefsRec *xkb_var_defs;
  gchar *rules_file;
  gchar *rules_path;

  xkb_var_defs = calloc (1, sizeof (XkbRF_VarDefsRec));
  get_xkb_values (&rules_file, xkb_var_defs);

  if (rules_file[0] == '/')
    rules_path = g_strdup_printf ("%s.xml", rules_file);
  else
    rules_path = g_strdup_printf ("%s/rules/%s.xml",
                                  DFLT_XKB_CONFIG_ROOT,
                                  rules_file);

  free_xkb_var_defs (xkb_var_defs);
  free (rules_file);

  return rules_path;
}

static void
parse_start_element (GMarkupParseContext  *context,
                     const gchar          *element_name,
                     const gchar         **attribute_names,
                     const gchar         **attribute_values,
                     gpointer              data,
                     GError              **error)
{
  if (current_parser_text)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Expected character data but got element '%s'", element_name);
      return;
    }

  if (strcmp (element_name, "name") == 0)
    {
      if (current_parser_variant)
        current_parser_text = &current_parser_variant->xkb_name;
      else if (current_parser_layout)
        current_parser_text = &current_parser_layout->xkb_name;
    }
  else if (strcmp (element_name, "description") == 0)
    {
      if (current_parser_variant)
        current_parser_text = &current_parser_variant->id;
      else if (current_parser_layout)
        current_parser_text = &current_parser_layout->id;
    }
  else if (strcmp (element_name, "shortDescription") == 0)
    {
      if (current_parser_variant)
        current_parser_text = &current_parser_variant->short_id;
      else if (current_parser_layout)
        current_parser_text = &current_parser_layout->short_id;
    }
  else if (strcmp (element_name, "iso639Id") == 0)
    {
      current_parser_text = &current_parser_iso639Id;
    }
  else if (strcmp (element_name, "layout") == 0)
    {
      if (current_parser_layout)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'layout' elements can't be nested");
          return;
        }

      current_parser_layout = g_new0 (Layout, 1);
    }
  else if (strcmp (element_name, "variant") == 0)
    {
      if (current_parser_variant)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements can't be nested");
          return;
        }

      if (!current_parser_layout)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements must be inside 'layout' elements");
          return;
        }

      current_parser_variant = g_new0 (Layout, 1);
      current_parser_variant->is_variant = TRUE;
      current_parser_variant->main_layout = current_parser_layout;
    }
}

static void
maybe_replace (GHashTable *table,
               gchar      *key,
               Layout     *new_layout)
{
  Layout *layout;
  gboolean exists;
  gboolean replace = TRUE;

  exists = g_hash_table_lookup_extended (table, key, NULL, (gpointer *)&layout);
  if (exists)
    replace = strlen (new_layout->id) < strlen (layout->id);
  if (replace)
    g_hash_table_replace (table, key, new_layout);
}

static void
parse_end_element (GMarkupParseContext  *context,
                   const gchar          *element_name,
                   gpointer              data,
                   GError              **error)
{
  if (strcmp (element_name, "layout") == 0)
    {
      if (!current_parser_layout->id || !current_parser_layout->xkb_name)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'layout' elements must enclose 'description' and 'name' elements");
          return;
        }

      if (current_parser_layout->short_id)
        maybe_replace (layouts_by_short_id,
                       current_parser_layout->short_id, current_parser_layout);

      g_hash_table_replace (layouts_table,
                            current_parser_layout->id,
                            current_parser_layout);
      current_parser_layout = NULL;
    }
  else if (strcmp (element_name, "variant") == 0)
    {
      if (!current_parser_variant->id || !current_parser_variant->xkb_name)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'variant' elements must enclose 'description' and 'name' elements");
          return;
        }

      if (current_parser_variant->short_id)
        maybe_replace (layouts_by_short_id,
                       current_parser_variant->short_id, current_parser_variant);

      g_hash_table_replace (layouts_table,
                            current_parser_variant->id,
                            current_parser_variant);
      current_parser_variant = NULL;
    }
  else if (strcmp (element_name, "iso639Id") == 0)
    {
      if (!current_parser_iso639Id)
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "'iso639Id' elements must enclose text");
          return;
        }

      if (current_parser_layout)
        maybe_replace (layouts_by_iso639,
                       current_parser_iso639Id, current_parser_layout);
      else if (current_parser_variant)
        maybe_replace (layouts_by_iso639,
                       current_parser_iso639Id, current_parser_variant);
      else
        g_free (current_parser_iso639Id);

      current_parser_iso639Id = NULL;
    }
}

static void
parse_text (GMarkupParseContext  *context,
            const gchar          *text,
            gsize                 text_len,
            gpointer              data,
            GError              **error)
{
  if (current_parser_text)
    {
      *current_parser_text = g_strndup (text, text_len);
      current_parser_text = NULL;
    }
}

static void
parse_error (GMarkupParseContext *context,
             GError              *error,
             gpointer             data)
{
  free_layout (current_parser_layout);
  free_layout (current_parser_variant);
  g_free (current_parser_iso639Id);
}

static const GMarkupParser markup_parser = {
  parse_start_element,
  parse_end_element,
  parse_text,
  NULL,
  parse_error
};

static void
parse_rules_file (void)
{
  gchar *buffer;
  gsize length;
  GMarkupParseContext *context;
  GError *error = NULL;
  gchar *full_path = get_rules_file_path ();

  g_file_get_contents (full_path, &buffer, &length, &error);
  g_free (full_path);
  if (error)
    {
      g_warning ("Failed to read XKB rules file: %s", error->message);
      g_error_free (error);
      return;
    }

  layouts_by_short_id = g_hash_table_new (g_str_hash, g_str_equal);
  layouts_by_iso639 = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  /* This is the "master" table so it assumes memory "ownership". */
  layouts_table = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, free_layout);

  context = g_markup_parse_context_new (&markup_parser, 0, NULL, NULL);
  g_markup_parse_context_parse (context, buffer, length, &error);
  g_markup_parse_context_free (context);
  g_free (buffer);
  if (error)
    {
      g_warning ("Failed to parse XKB rules file: %s", error->message);
      g_error_free (error);
      g_hash_table_destroy (layouts_by_short_id);
      g_hash_table_destroy (layouts_by_iso639);
      g_hash_table_destroy (layouts_table);
      layouts_table = NULL;
      return;
    }
}

static gboolean
ensure_rules_are_parsed (void)
{
  if (!layouts_table)
    parse_rules_file ();

  return !!layouts_table;
}

static void
add_name_to_list (gpointer key,
                  gpointer value,
                  gpointer data)
{
  GSList **list = data;

  *list = g_slist_prepend (*list, key);
}

/**
 * xkb_rules_db_get_all_layout_names:
 *
 * Returns a list of all layout names we know about.
 *
 * Return value: (transfer container): the list of layout names. The
 * caller takes ownership of the #GSList but not of the strings
 * themselves, those are internally allocated and must not be
 * modified.
 */
GSList *
xkb_rules_db_get_all_layout_names (void)
{
  GSList *layout_names = NULL;

  if (!ensure_rules_are_parsed ())
    return NULL;

  g_hash_table_foreach (layouts_table, add_name_to_list, &layout_names);

  return layout_names;
}

/**
 * xkb_rules_db_get_layout_info:
 * @name: layout's name about which to retrieve the info
 * @short_name: (out) (allow-none) (transfer none): location to store
 * the layout's short name, or %NULL
 * @xkb_layout: (out) (allow-none) (transfer none): location to store
 * the layout's XKB name, or %NULL
 * @xkb_variant: (out) (allow-none) (transfer none): location to store
 * the layout's XKB variant, or %NULL
 *
 * Retrieves information about a layout. Some layouts don't provide a
 * short name (2 or 3 letters) or don't specify a XKB variant, in
 * those cases @short_name or @xkb_variant are empty strings, i.e. "".
 *
 * If the given layout doesn't exist the return value is %FALSE and
 * all the (out) parameters are set to %NULL.
 *
 * Return value: %TRUE if the layout exists or %FALSE otherwise.
 */
gboolean
xkb_rules_db_get_layout_info (const gchar  *name,
                              const gchar **short_name,
                              const gchar **xkb_layout,
                              const gchar **xkb_variant)
{
  const Layout *layout;

  if (short_name)
    *short_name = NULL;
  if (xkb_layout)
    *xkb_layout = NULL;
  if (xkb_variant)
    *xkb_variant = NULL;

  if (!ensure_rules_are_parsed ())
    return FALSE;

  if (!g_hash_table_lookup_extended (layouts_table, name, NULL, (gpointer *)&layout))
    return FALSE;

  if (!layout->is_variant)
    {
      if (short_name)
        *short_name = layout->short_id ? layout->short_id : "";
      if (xkb_layout)
        *xkb_layout = layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = "";
    }
  else
    {
      if (short_name)
        *short_name = layout->short_id ? layout->short_id :
          layout->main_layout->short_id ? layout->main_layout->short_id : "";
      if (xkb_layout)
        *xkb_layout = layout->main_layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = layout->xkb_name;
    }

  return TRUE;
}

/**
 * xkb_rules_db_get_layout_info_for_language:
 * @language: an ISO 639 code
 * @name: (out) (allow-none) (transfer none): location to store the
 * layout's name, or %NULL
 * @short_name: (out) (allow-none) (transfer none): location to store
 * the layout's short name, or %NULL
 * @xkb_layout: (out) (allow-none) (transfer none): location to store
 * the layout's XKB name, or %NULL
 * @xkb_variant: (out) (allow-none) (transfer none): location to store
 * the layout's XKB variant, or %NULL
 *
 * Retrieves the layout that better fits @language. It also fetches
 * information about that layout like xkb_rules_db_get_layout_info().
 *
 * If the a layout can't be found the return value is %FALSE and all
 * the (out) parameters are set to %NULL.
 *
 * Return value: %TRUE if a layout exists or %FALSE otherwise.
 */
gboolean
xkb_rules_db_get_layout_info_for_language (const gchar  *language,
                                           const gchar **name,
                                           const gchar **short_name,
                                           const gchar **xkb_layout,
                                           const gchar **xkb_variant)
{
  const Layout *layout;

  if (name)
    *name = NULL;
  if (short_name)
    *short_name = NULL;
  if (xkb_layout)
    *xkb_layout = NULL;
  if (xkb_variant)
    *xkb_variant = NULL;

  if (!ensure_rules_are_parsed ())
    return FALSE;

  if (!g_hash_table_lookup_extended (layouts_by_iso639, language, NULL, (gpointer *)&layout))
    if (!g_hash_table_lookup_extended (layouts_by_short_id, language, NULL, (gpointer *)&layout))
      return FALSE;

  if (name)
    *name = layout->id;

  if (!layout->is_variant)
    {
      if (short_name)
        *short_name = layout->short_id ? layout->short_id : "";
      if (xkb_layout)
        *xkb_layout = layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = "";
    }
  else
    {
      if (short_name)
        *short_name = layout->short_id ? layout->short_id :
          layout->main_layout->short_id ? layout->main_layout->short_id : "";
      if (xkb_layout)
        *xkb_layout = layout->main_layout->xkb_name;
      if (xkb_variant)
        *xkb_variant = layout->xkb_name;
    }

  return TRUE;
}
