/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <cups/cups.h>
#include <cups/ppd.h>

#include "pp-utils.h"

#define MECHANISM_BUS "org.opensuse.CupsPkHelper.Mechanism"

#define SCP_BUS   "org.fedoraproject.Config.Printing"
#define SCP_PATH  "/org/fedoraproject/Config/Printing"
#define SCP_IFACE "org.fedoraproject.Config.Printing"

gchar *
get_tag_value (const gchar *tag_string, const gchar *tag_name)
{
  gchar **tag_string_splitted = NULL;
  gchar  *tag_value = NULL;
  gint    tag_name_length;
  gint    i;

  if (tag_string && tag_name)
    {
      tag_name_length = strlen (tag_name);
      tag_string_splitted = g_strsplit (tag_string, ";", 0);
      if (tag_string_splitted)
        {
          for (i = 0; i < g_strv_length (tag_string_splitted); i++)
            if (g_ascii_strncasecmp (tag_string_splitted[i], tag_name, tag_name_length) == 0)
              if (strlen (tag_string_splitted[i]) > tag_name_length + 1)
                tag_value = g_strdup (tag_string_splitted[i] + tag_name_length + 1);

          g_strfreev (tag_string_splitted);
        }
    }

  return tag_value;
}


typedef struct
{
  gchar *ppd_name;
  gchar *ppd_device_id;
  gchar *ppd_product;
  gchar *ppd_make_and_model;

  gchar *driver_type;

  gchar *mfg;
  gchar *mdl;
  gint   match_level;
  gint   preference_value;
} PPDItem;


static void
ppd_item_free (PPDItem *item)
{
  if (item)
    {
      g_free (item->ppd_name);
      g_free (item->ppd_device_id);
      g_free (item->ppd_product);
      g_free (item->ppd_make_and_model);
      g_free (item->driver_type);
      g_free (item->mfg);
      g_free (item->mdl);
    }
}

static PPDItem *
ppd_item_copy (PPDItem *item)
{
  PPDItem *result = NULL;

  result = g_new0 (PPDItem, 1);
  if (item && result)
    {
      result->ppd_name = g_strdup (item->ppd_name);
      result->ppd_device_id = g_strdup (item->ppd_device_id);
      result->ppd_product = g_strdup (item->ppd_product);
      result->ppd_make_and_model = g_strdup (item->ppd_make_and_model);
      result->driver_type = g_strdup (item->driver_type);
      result->mfg = g_strdup (item->mfg);
      result->mdl = g_strdup (item->mdl);
      result->match_level = item->match_level;
      result->preference_value = item->preference_value;
    }

  return result;
}


/*
 * Make deep copy of given const string array.
 */
static gchar **
strvdup (const gchar * const x[])
{
  gint i, length = 0;
  gchar **result;

  for (length = 0; x && x[length]; length++);

  length++;

  result = g_new0 (gchar *, length);
  for (i = 0; i < length; i++)
    result[i] = g_strdup (x[i]);

  return result;
}

/*
 * Normalize given string so that it is lowercase, doesn't
 * have trailing or leading whitespaces and digits doesn't
 * neighbour with alphabetic.
 * (see cupshelpers/ppds.py from system-config-printer)
 */
static gchar *
normalize (const gchar *input_string)
{
  gchar *tmp = NULL;
  gchar *res = NULL;
  gchar *result = NULL;
  gint   i, j = 0, k = -1;

  if (input_string)
    {
      tmp = g_strstrip (g_ascii_strdown (input_string, -1));
      if (tmp)
        {
          res = g_new (gchar, 2 * strlen (tmp));

          for (i = 0; i < strlen (tmp); i++)
            {
              if ((g_ascii_isalpha (tmp[i]) && k >= 0 && g_ascii_isdigit (res[k])) ||
                  (g_ascii_isdigit (tmp[i]) && k >= 0 && g_ascii_isalpha (res[k])))
                {
                  res[j] = ' ';
                  k = j++;
                  res[j] = tmp[i];
                  k = j++;
                }
              else
                {
                  if (g_ascii_isspace (tmp[i]) || !g_ascii_isalnum (tmp[i]))
                    {
                      if (!(k >= 0 && res[k] == ' '))
                        {
                          res[j] = ' ';
                          k = j++;
                        }
                    }
                  else
                    {
                      res[j] = tmp[i];
                      k = j++;
                    }
                }
            }

          res[j] = '\0';

          result = g_strdup (res);
          g_free (tmp);
          g_free (res);
        }
    }

  return result;
}


/*
 * Find out type of the given printer driver.
 * (see xml/preferreddrivers.xml from system-config-printer)
 */
static gchar *
get_driver_type (gchar *ppd_name,
                 gchar *ppd_device_id,
                 gchar *ppd_make_and_model,
                 gchar *ppd_product,
                 gint   match_level)
{
  gchar *tmp = NULL;

  if (match_level == PPD_GENERIC_MATCH)
    {
      if (ppd_name && g_regex_match_simple ("(foomatic(-db-compressed-ppds)?|ijsgutenprint.*):", ppd_name, 0, 0))
        {
          tmp = get_tag_value ("DRV", ppd_device_id);
          if (tmp && g_regex_match_simple (".*,?R1", tmp, 0, 0))
            return g_strdup ("generic-foomatic-recommended");
        }
    }

  if (match_level == PPD_GENERIC_MATCH || match_level == PPD_NO_MATCH)
    {
      if (ppd_name && g_regex_match_simple ("(foomatic(-db-compressed-ppds)?|ijsgutenprint.*):Generic-ESC_P", ppd_name, 0, 0))
        return g_strdup ("generic-escp");

      if (ppd_name && g_regex_match_simple ("drv:///sample.drv/epson(9|24).ppd", ppd_name, 0, 0))
        return g_strdup ("generic-escp");

      if (g_strcmp0 (ppd_make_and_model, "Generic PostScript Printer") == 0)
        return g_strdup ("generic-postscript");

      if (g_strcmp0 (ppd_make_and_model, "Generic PCL 6 Printer") == 0)
        return g_strdup ("generic-pcl6");

      if (g_strcmp0 (ppd_make_and_model, "Generic PCL 5e Printer") == 0)
        return g_strdup ("generic-pcl5e");

      if (g_strcmp0 (ppd_make_and_model, "Generic PCL 5 Printer") == 0)
        return g_strdup ("generic-pcl5");

      if (g_strcmp0 (ppd_make_and_model, "Generic PCL Laser Printer") == 0)
        return g_strdup ("generic-pcl");

      return g_strdup ("generic");
    }


  if (ppd_name && g_regex_match_simple ("drv:///sample.drv/", ppd_name, 0, 0))
    return g_strdup ("cups");

  if (ppd_product && g_regex_match_simple (".*Ghostscript", ppd_product, 0, 0))
    return g_strdup ("ghostscript");

  if (ppd_name && g_regex_match_simple ("gutenprint.*:.*/simple|.*-gutenprint.*\\.sim", ppd_name, 0, 0))
    return g_strdup ("gutenprint-simplified");

  if (ppd_name && g_regex_match_simple ("gutenprint.*:|.*-gutenprint", ppd_name, 0, 0))
    return g_strdup ("gutenprint-expert");

  if (ppd_make_and_model && g_regex_match_simple (".* Foomatic/hpijs", ppd_make_and_model, 0, 0))
    {
      tmp = get_tag_value ("DRV", ppd_device_id);
      if (tmp && g_regex_match_simple (",?R1", tmp, 0, 0))
        return g_strdup ("foomatic-recommended-hpijs");
    }

  if (ppd_make_and_model && g_regex_match_simple (".* Foomatic/hpijs", ppd_make_and_model, 0, 0))
    return g_strdup ("foomatic-hpijs");

  if (ppd_name && g_regex_match_simple ("foomatic(-db-compressed-ppds)?:", ppd_name, 0, 0) &&
      ppd_make_and_model && g_regex_match_simple (".* Postscript", ppd_make_and_model, 0, 0))
    {
      tmp = get_tag_value ("DRV", ppd_device_id);
      if (tmp && g_regex_match_simple (".*,?R1", tmp, 0, 0))
        return g_strdup ("foomatic-recommended-postscript");
    }

  if (ppd_name && g_regex_match_simple ("foomatic(-db-compressed-ppds)?:.*-Postscript", ppd_name, 0, 0))
    return g_strdup ("foomatic-postscript");

  if (ppd_make_and_model && g_regex_match_simple (".* Foomatic/pxlmono", ppd_make_and_model, 0, 0))
    return g_strdup ("foomatic-pxlmono");

  if (ppd_name && g_regex_match_simple ("(foomatic(-db-compressed-ppds)?|ijsgutenprint.*):", ppd_name, 0, 0))
    {
      tmp = get_tag_value ("DRV", ppd_device_id);
      if (tmp && g_regex_match_simple (".*,?R1", tmp, 0, 0))
        return g_strdup ("foomatic-recommended-non-postscript");
    }

  if (ppd_name && g_regex_match_simple ("(foomatic(-db-compressed-ppds)?|ijsgutenprint.*):.*-gutenprint", ppd_name, 0, 0))
    return g_strdup ("foomatic-gutenprint");

  if (ppd_name && g_regex_match_simple ("(foomatic(-db-compressed-ppds)?|ijsgutenprint.*):", ppd_name, 0, 0))
    return g_strdup ("foomatic");

  if (ppd_name && g_regex_match_simple ("drv:///(hp/)?hpcups.drv/|.*-hpcups", ppd_name, 0, 0) &&
      ppd_make_and_model && g_regex_match_simple (".* plugin", ppd_make_and_model, 0, 0))
    return g_strdup ("hpcups-plugin");

  if (ppd_name && g_regex_match_simple ("drv:///(hp/)?hpcups.drv/|.*-hpcups", ppd_name, 0, 0))
    return g_strdup ("hpcups");

  if (ppd_name && g_regex_match_simple ("drv:///(hp/)?hpijs.drv/|.*-hpijs", ppd_name, 0, 0) &&
      ppd_make_and_model && g_regex_match_simple (".* plugin", ppd_make_and_model, 0, 0))
    return g_strdup ("hpijs-plugin");

  if (ppd_name && g_regex_match_simple ("drv:///(hp/)?hpijs.drv/|.*-hpijs", ppd_name, 0, 0))
    return g_strdup ("hpijs");

  if (ppd_name && g_regex_match_simple (".*splix", ppd_name, 0, 0))
    return g_strdup ("splix");

  if (ppd_name && g_regex_match_simple (".*turboprint", ppd_name, 0, 0))
    return g_strdup ("turboprint");

  if (ppd_name && g_regex_match_simple (".*/(Ricoh|Lanier|Gestetner|InfoPrint|Infotech|Savin|NRG)/PS/", ppd_name, 0, 0))
    return g_strdup ("manufacturer-ricoh-postscript");

  if (ppd_name && g_regex_match_simple (".*/(Ricoh|Lanier|Gestetner|InfoPrint|Infotech|Savin|NRG)/PXL/", ppd_name, 0, 0))
    return g_strdup ("manufacturer-ricoh-pxl");

  if (match_level == PPD_EXACT_CMD_MATCH)
    return g_strdup ("manufacturer-cmd");

  return g_strdup ("manufacturer");
}


/*
 * Return preference value. The most preferred driver has the lowest value.
 * If the value is higher or equal to 1000 then try to avoid installation
 * of this driver. If it is higher or equal to 2000 then don't install
 * this driver.
 * (see xml/preferreddrivers.xml from system-config-printer)
 */
static gint
get_driver_preference (PPDItem *item)
{
  gchar   *tmp1 = NULL;
  gchar   *tmp2 = NULL;
  gint     result = 0;

  if (item && item->ppd_make_and_model &&
      g_regex_match_simple ("Brother HL-2030", item->ppd_make_and_model, 0, 0))
    {
      tmp1 = get_tag_value ("MFG", item->ppd_device_id);
      tmp2 = get_tag_value ("MDL", item->ppd_device_id);

      if (tmp1 && g_regex_match_simple ("Brother", tmp1, 0, 0) &&
          tmp2 && g_regex_match_simple ("HL-2030", tmp2, 0, 0))
        {
          if (item->driver_type && g_regex_match_simple ("gutenprint*", item->driver_type, 0, 0))
            return result + 2000;
          else
            return result;
        }
    }
  result++;

  if (item && item->ppd_make_and_model &&
      g_regex_match_simple ("(Ricoh|Lanier|Gestetner|InfoPrint|Infotech|Savin|NRG) ", item->ppd_make_and_model, 0, 0))
    {
      tmp1 = get_tag_value ("MFG", item->ppd_device_id);

      if (tmp1 && g_regex_match_simple ("(Ricoh|Lanier|Gestetner|InfoPrint|Infotech|Savin|NRG)", tmp1, 0, 0) &&
          ((g_strcmp0 (item->driver_type, "manufacturer-ricoh-postscript") == 0) ||
           (g_strcmp0 (item->driver_type, "manufacturer-ricoh-pxl") == 0)))
        return result;
    }
  result++;

  if (item && item->ppd_make_and_model &&
      g_regex_match_simple ("Xerox 6250DP", item->ppd_make_and_model, 0, 0))
    {
      tmp1 = get_tag_value ("MFG", item->ppd_device_id);
      tmp2 = get_tag_value ("MDL", item->ppd_device_id);

      if (tmp1 && g_regex_match_simple ("Xerox", tmp1, 0, 0) &&
          tmp2 && g_regex_match_simple ("6250DP", tmp2, 0, 0))
        {
          if (item->driver_type && g_regex_match_simple ("gutenprint*", item->driver_type, 0, 0))
            return result + 1000;
          else
            return result;
        }
    }
  result++;

  if (item && g_strcmp0 (item->driver_type, "manufacturer-cmd") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-recommended-hpijs") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-recommended-non-postscript") == 0)
    return result;
  result++;

  if (item && item->driver_type &&
      g_regex_match_simple ("manufacturer*", item->driver_type, 0, 0))
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-recommended-postscript") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-postscript") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "hpcups") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "hpijs") == 0)
    return result;
  result++;

  if (item && item->ppd_make_and_model &&
      g_regex_match_simple ("(HP|Hewlett-Packard) ", item->ppd_make_and_model, 0, 0))
    {
      tmp1 = get_tag_value ("MFG", item->ppd_device_id);

      if (tmp1 && g_regex_match_simple ("HP|Hewlett-Packard", tmp1, 0, 0) &&
          g_strcmp0 (item->driver_type, "foomatic-hpijs") == 0)
        return result;
    }
  result++;

  if (item && g_strcmp0 (item->driver_type, "gutenprint-simplified") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "gutenprint-expert") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-hpijs") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-gutenprint") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "cups") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-postscript") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-foomatic-recommended") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-pcl6") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-pcl5c") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-pcl5e") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-pcl5") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-pcl") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "foomatic-pxlmono") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic-escp") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "ghostscript") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "generic") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "hpcups-plugin") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "hpijs-plugin") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "splix") == 0)
    return result;
  result++;

  if (item && g_strcmp0 (item->driver_type, "turboprint") == 0)
    return result;
  result++;

  return result;
}


/*
 * Compare driver types according to preference order.
 * The most preferred driver is the lowest one.
 * (see xml/preferreddrivers.xml from system-config-printer)
 */
static gint
preference_value_cmp (gconstpointer a,
                      gconstpointer b)
{
  PPDItem *c = (PPDItem *) a;
  PPDItem *d = (PPDItem *) b;

  if (c == NULL && d == NULL)
    return 0;
  else if (c == NULL)
    return -1;
  else if (d == NULL)
    return 1;

  if (c->preference_value < d->preference_value)
    return -1;
  else if (c->preference_value > d->preference_value)
    return 1;
  else
    return 0;
}


/* Compare PPDItems a and b according to normalized model name */
static gint
item_cmp (gconstpointer a,
          gconstpointer b)
{
  PPDItem *c = (PPDItem *) a;
  PPDItem *d = (PPDItem *) b;
  glong   a_number;
  glong   b_number;
  gchar  *a_normalized = NULL;
  gchar  *b_normalized = NULL;
  gchar **av = NULL;
  gchar **bv = NULL;
  gint    a_length = 0;
  gint    b_length = 0;
  gint    min_length;
  gint    result = 0;
  gint    i;

  if (c && d)
    {
      a_normalized = normalize (c->mdl);
      b_normalized = normalize (d->mdl);

      if (a_normalized)
        av = g_strsplit (a_normalized, " ", 0);

      if (b_normalized)
        bv = g_strsplit (b_normalized, " ", 0);

      if (av)
        a_length = g_strv_length (av);

      if (bv)
        b_length = g_strv_length (bv);

      min_length = a_length < b_length ? a_length : b_length;

      for (i = 0; i < min_length; i++)
        {
          if (g_ascii_isdigit (av[i][0]) && g_ascii_isdigit (bv[i][0]))
            {
              a_number = atol (av[i]);
              b_number = atol (bv[i]);
              if (a_number < b_number)
                {
                  result = -1;
                  goto out;
                }
              else if (a_number > b_number)
                {
                  result = 1;
                  goto out;
                }
            }
          else if (g_ascii_isdigit (av[i][0]) && !g_ascii_isdigit (bv[i][0]))
            {
              result = -1;
              goto out;
            }
          else if (!g_ascii_isdigit (av[i][0]) && g_ascii_isdigit (bv[i][0]))
            {
              result = 1;
              goto out;
            }
          else
            {
              if (g_strcmp0 (av[i], bv[i]) != 0)
                {
                  result = g_strcmp0 (av[i], bv[i]);
                  goto out;
                }
            }
        }

      if (a_length < b_length)
        result = -1;
      else if (a_length > b_length)
        result = 1;
    }

out:
  if (av)
    g_strfreev (av);

  if (bv)
    g_strfreev (bv);

  g_free (a_normalized);
  g_free (b_normalized);

  return result;
}


static gint
get_prefix_length (gchar *a, gchar *b)
{
  gint a_length;
  gint b_length;
  gint min_length;
  gint i;

  if (a && b)
    {
      a_length = strlen (a);
      b_length = strlen (b);
      min_length = a_length < b_length ? a_length : b_length;

      for (i = 0; i < min_length; i++)
        {
          if (a[i] != b[i])
            return i;
        }
      return min_length;
    }

  return 0;
}


/*
 * Append best matching ppds from list "ppds" to list "list"
 * according to model name "model". Return the resulting list.
 */
static GList *
append_best_ppds (GList *list,
                  GList *ppds,
                  gchar *model)
{
  PPDItem *item;
  PPDItem *tmp_item;
  PPDItem *best_item = NULL;
  gchar   *mdl_normalized;
  gchar   *mdl;
  gchar   *tmp;
  GList   *local_ppds;
  GList   *actual_item;
  GList   *candidates = NULL;
  GList   *tmp_list = NULL;
  GList   *result = NULL;
  gint     best_prefix_length = -1;
  gint     prefix_length;

  result = list;

  if (model)
    {
      mdl = g_ascii_strdown (model, -1);
      if (g_str_has_suffix (mdl, " series"))
        {
          tmp = g_strndup (mdl, strlen (mdl) - 7);
          g_free (mdl);
          mdl = tmp;
        }

      mdl_normalized = normalize (mdl);

      item = g_new0 (PPDItem, 1);
      item->ppd_device_id = g_strdup_printf ("mdl:%s;", mdl);
      item->mdl = mdl_normalized;

      local_ppds = g_list_copy (ppds);
      local_ppds = g_list_append (local_ppds, item);
      local_ppds = g_list_sort (local_ppds, item_cmp);

      actual_item = g_list_find (local_ppds, item);
      if (actual_item)
        {
          if (actual_item->prev)
            candidates = g_list_append (candidates, actual_item->prev->data);
          if (actual_item->next)
            candidates = g_list_append (candidates, actual_item->next->data);
        }

      for (tmp_list = candidates; tmp_list; tmp_list = tmp_list->next)
        {
          tmp_item = (PPDItem *) tmp_list->data;

          prefix_length = get_prefix_length (tmp_item->mdl, mdl_normalized);
          if (prefix_length > best_prefix_length)
            {
              best_prefix_length = prefix_length;
              best_item = tmp_item;
            }
        }

      if (best_item && best_prefix_length > strlen (mdl_normalized) / 2)
        {
          if (best_prefix_length == strlen (mdl_normalized))
            best_item->match_level = PPD_EXACT_MATCH;
          else
            best_item->match_level = PPD_CLOSE_MATCH;

          result = g_list_append (result, ppd_item_copy (best_item));
        }
      else
        {
          /* TODO the last resort (see _findBestMatchPPDs() in ppds.py) */
        }

      g_list_free (candidates);
      g_list_free (local_ppds);

      g_free (item->ppd_device_id);
      g_free (item);
      g_free (mdl);
      g_free (mdl_normalized);
    }

  return result;
}

/*
 * Return the best matching driver name
 * for device described by given parameters.
 */
PPDName *
get_ppd_name (gchar *device_id,
              gchar *device_make_and_model,
              gchar *device_uri)
{
  GDBusConnection *bus;
  GVariant   *output;
  GVariant   *array;
  GVariant   *tuple;
  PPDName    *result = NULL;
  GError     *error = NULL;
  gchar      *name, *match;
  gint        i, j;
  static const char * const match_levels[] = {
             "exact-cmd",
             "exact",
             "close",
             "generic",
             "none"};

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (bus)
    {
      output = g_dbus_connection_call_sync (bus,
                                            SCP_BUS,
                                            SCP_PATH,
                                            SCP_IFACE,
                                            "GetBestDrivers",
                                            g_variant_new ("(sss)",
                                                           device_id ? device_id : "",
                                                           device_make_and_model ? device_make_and_model : "",
                                                           device_uri ? device_uri : ""),
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            60000,
                                            NULL,
                                            &error);

      if (output && g_variant_n_children (output) >= 1)
        {
          array = g_variant_get_child_value (output, 0);
          if (array)
            for (j = 0; j < G_N_ELEMENTS (match_levels) && result == NULL; j++)
              for (i = 0; i < g_variant_n_children (array) && result == NULL; i++)
                {
                  tuple = g_variant_get_child_value (array, i);
                  if (tuple && g_variant_n_children (tuple) == 2)
                    {
                      name = g_strdup (g_variant_get_string (
                                         g_variant_get_child_value (tuple, 0),
                                         NULL));
                      match = g_strdup (g_variant_get_string (
                                          g_variant_get_child_value (tuple, 1),
                                          NULL));

                      if (g_strcmp0 (match, match_levels[j]) == 0)
                        {
                          result = g_new0 (PPDName, 1);
                          result->ppd_name = g_strdup (name);

                          if (g_strcmp0 (match, "exact-cmd") == 0)
                            result->ppd_match_level = PPD_EXACT_CMD_MATCH;
                          else if (g_strcmp0 (match, "exact") == 0)
                            result->ppd_match_level = PPD_EXACT_MATCH;
                          else if (g_strcmp0 (match, "close") == 0)
                            result->ppd_match_level = PPD_CLOSE_MATCH;
                          else if (g_strcmp0 (match, "generic") == 0)
                            result->ppd_match_level = PPD_GENERIC_MATCH;
                          else if (g_strcmp0 (match, "none") == 0)
                            result->ppd_match_level = PPD_NO_MATCH;
                        }

                      g_free (match);
                      g_free (name);
                    }
                }
        }

      if (output)
        g_variant_unref (output);
      g_object_unref (bus);
    }

  if (bus == NULL ||
      (error &&
       error->domain == G_DBUS_ERROR &&
       (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
        error->code == G_DBUS_ERROR_UNKNOWN_METHOD)))
    {
      ipp_attribute_t *attr = NULL;
      const gchar     *hp_equivalents[] = {"hp", "hewlett packard", NULL};
      const gchar     *kyocera_equivalents[] = {"kyocera", "kyocera mita", NULL};
      const gchar     *toshiba_equivalents[] = {"toshiba", "toshiba tec corp.", NULL};
      const gchar     *lexmark_equivalents[] = {"lexmark", "lexmark international", NULL};
      gboolean         ppd_exact_match_found = FALSE;
      PPDItem         *item;
      http_t          *http = NULL;
      ipp_t           *request = NULL;
      ipp_t           *response = NULL;
      GList           *tmp_list;
      GList           *tmp_list2;
      GList           *mdls = NULL;
      GList           *list = NULL;
      gchar           *mfg_normalized = NULL;
      gchar           *mdl_normalized = NULL;
      gchar           *eq_normalized = NULL;
      gchar           *mfg = NULL;
      gchar           *mdl = NULL;
      gchar           *tmp = NULL;
      gchar           *ppd_device_id;
      gchar           *ppd_make_and_model;
      gchar           *ppd_name;
      gchar           *ppd_product;
      gchar          **equivalents = NULL;
      gint             i;

      g_warning ("You should install system-config-printer which provides \
DBus method \"GetBestDrivers\". Using fallback solution for now.");
      g_error_free (error);

      mfg = get_tag_value (device_id, "mfg");
      if (!mfg)
        mfg = get_tag_value (device_id, "manufacturer");

      mdl = get_tag_value (device_id, "mdl");
      if (!mdl)
        mdl = get_tag_value (device_id, "model");

      mfg_normalized = normalize (mfg);
      mdl_normalized = normalize (mdl);

      if (mfg_normalized && mfg)
        {
          if (g_str_has_prefix (mfg_normalized, "hewlett") ||
              g_str_has_prefix (mfg_normalized, "hp"))
            equivalents = strvdup (hp_equivalents);

          if (g_str_has_prefix (mfg_normalized, "kyocera"))
            equivalents = strvdup (kyocera_equivalents);

          if (g_str_has_prefix (mfg_normalized, "toshiba"))
            equivalents = strvdup (toshiba_equivalents);

          if (g_str_has_prefix (mfg_normalized, "lexmark"))
            equivalents = strvdup (lexmark_equivalents);

          if (equivalents == NULL)
            {
              equivalents = g_new0 (gchar *, 2);
              equivalents[0] = g_strdup (mfg);
            }
        }

      http = httpConnectEncrypt (cupsServer (),
                                 ippPort (),
                                 cupsEncryption ());

      /* Find usable drivers for given device */
      if (http)
        {
          /* Try exact match according to device-id */
          if (device_id)
            {
              request = ippNewRequest (CUPS_GET_PPDS);
              ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_TEXT,
                            "ppd-device-id", NULL, device_id);
              response = cupsDoRequest (http, request, "/");

              if (response &&
                  response->request.status.status_code <= IPP_OK_CONFLICT)
                {
                  for (attr = response->attrs; attr != NULL; attr = attr->next)
                    {
                      while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
                        attr = attr->next;

                      if (attr == NULL)
                        break;

                      ppd_device_id = NULL;
                      ppd_make_and_model = NULL;
                      ppd_name = NULL;
                      ppd_product = NULL;

                      while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
                        {
                          if (g_strcmp0 (attr->name, "ppd-device-id") == 0 &&
                              attr->value_tag == IPP_TAG_TEXT)
                            ppd_device_id = attr->values[0].string.text;
                          else if (g_strcmp0 (attr->name, "ppd-make-and-model") == 0 &&
                                   attr->value_tag == IPP_TAG_TEXT)
                            ppd_make_and_model = attr->values[0].string.text;
                          else if (g_strcmp0 (attr->name, "ppd-name") == 0 &&
                                   attr->value_tag == IPP_TAG_NAME)
                            ppd_name = attr->values[0].string.text;
                          else if (g_strcmp0 (attr->name, "ppd-product") == 0 &&
                                   attr->value_tag == IPP_TAG_TEXT)
                            ppd_product = attr->values[0].string.text;

                          attr = attr->next;
                        }

                      if (ppd_device_id && ppd_name)
                        {
                          item = g_new0 (PPDItem, 1);
                          item->ppd_name = g_strdup (ppd_name);
                          item->ppd_device_id = g_strdup (ppd_device_id);
                          item->ppd_make_and_model = g_strdup (ppd_make_and_model);
                          item->ppd_product = g_strdup (ppd_product);

                          tmp = get_tag_value (ppd_device_id, "mfg");
                          if (!tmp)
                            tmp = get_tag_value (ppd_device_id, "manufacturer");
                          item->mfg = normalize (tmp);
                          g_free (tmp);

                          tmp = get_tag_value (ppd_device_id, "mdl");
                          if (!tmp)
                            tmp = get_tag_value (ppd_device_id, "model");
                          item->mdl = normalize (tmp);
                          g_free (tmp);

                          item->match_level = PPD_EXACT_CMD_MATCH;
                          ppd_exact_match_found = TRUE;
                          list = g_list_append (list, item);
                        }

                      if (attr == NULL)
                        break;
                    }
                }

              if (response)
                ippDelete(response);
            }

          /* Try match according to manufacturer and model fields */
          if (!ppd_exact_match_found && mfg_normalized && mdl_normalized)
            {
              request = ippNewRequest (CUPS_GET_PPDS);
              response = cupsDoRequest (http, request, "/");

              if (response &&
                  response->request.status.status_code <= IPP_OK_CONFLICT)
                {
                  for (i = 0; equivalents && equivalents[i]; i++)
                    {
                      eq_normalized = normalize (equivalents[i]);
                      for (attr = response->attrs; attr != NULL; attr = attr->next)
                        {
                          while (attr != NULL && attr->group_tag != IPP_TAG_PRINTER)
                            attr = attr->next;

                          if (attr == NULL)
                            break;

                          ppd_device_id = NULL;
                          ppd_make_and_model = NULL;
                          ppd_name = NULL;
                          ppd_product = NULL;

                          while (attr != NULL && attr->group_tag == IPP_TAG_PRINTER)
                            {
                              if (g_strcmp0 (attr->name, "ppd-device-id") == 0 &&
                                  attr->value_tag == IPP_TAG_TEXT)
                                ppd_device_id = attr->values[0].string.text;
                              else if (g_strcmp0 (attr->name, "ppd-make-and-model") == 0 &&
                                       attr->value_tag == IPP_TAG_TEXT)
                                ppd_make_and_model = attr->values[0].string.text;
                              else if (g_strcmp0 (attr->name, "ppd-name") == 0 &&
                                       attr->value_tag == IPP_TAG_NAME)
                                ppd_name = attr->values[0].string.text;
                              else if (g_strcmp0 (attr->name, "ppd-product") == 0 &&
                                       attr->value_tag == IPP_TAG_TEXT)
                                ppd_product = attr->values[0].string.text;

                              attr = attr->next;
                            }

                          if (ppd_device_id && ppd_name)
                            {
                              item = g_new0 (PPDItem, 1);
                              item->ppd_name = g_strdup (ppd_name);
                              item->ppd_device_id = g_strdup (ppd_device_id);
                              item->ppd_make_and_model = g_strdup (ppd_make_and_model);
                              item->ppd_product = g_strdup (ppd_product);

                              tmp = get_tag_value (ppd_device_id, "mfg");
                              if (!tmp)
                                tmp = get_tag_value (ppd_device_id, "manufacturer");
                              item->mfg = normalize (tmp);
                              g_free (tmp);

                              tmp = get_tag_value (ppd_device_id, "mdl");
                              if (!tmp)
                                tmp = get_tag_value (ppd_device_id, "model");
                              item->mdl = normalize (tmp);
                              g_free (tmp);

                              if (item->mdl && item->mfg &&
                                  g_ascii_strcasecmp (item->mdl, mdl_normalized) == 0 &&
                                  g_ascii_strcasecmp (item->mfg, eq_normalized) == 0)
                                {
                                  item->match_level = PPD_EXACT_MATCH;
                                  ppd_exact_match_found = TRUE;
                                }

                              if (item->match_level == PPD_EXACT_MATCH)
                                list = g_list_append (list, item);
                              else if (item->mfg &&
                                       g_ascii_strcasecmp (item->mfg, eq_normalized) == 0)
                                mdls = g_list_append (mdls, item);
                              else
                                {
                                  ppd_item_free (item);
                                  g_free (item);
                                }
                            }

                          if (attr == NULL)
                            break;
                        }

                      g_free (eq_normalized);
                    }
                }

              if (response)
                ippDelete(response);
            }

          httpClose (http);
        }

      if (list == NULL)
        list = append_best_ppds (list, mdls, mdl);

      /* Find out driver types for all listed drivers and set their preference values */
      for (tmp_list = list; tmp_list; tmp_list = tmp_list->next)
        {
          item = (PPDItem *) tmp_list->data;
          if (item)
            {
              item->driver_type = get_driver_type (item->ppd_name,
                                                   item->ppd_device_id,
                                                   item->ppd_make_and_model,
                                                   item->ppd_product,
                                                   item->match_level);
              item->preference_value = get_driver_preference (item);
            }
        }

      /* Sort driver list according to preference value */
      list = g_list_sort (list, preference_value_cmp);

      /* Split blacklisted drivers to tmp_list */
      for (tmp_list = list; tmp_list; tmp_list = tmp_list->next)
        {
          item = (PPDItem *) tmp_list->data;
          if (item && item->preference_value >= 2000)
            break;
        }

      /* Free tmp_list */
      if (tmp_list)
        {
          if (tmp_list->prev)
            tmp_list->prev->next = NULL;
          else
            list = NULL;

          tmp_list->prev = NULL;
          for (tmp_list2 = tmp_list; tmp_list2; tmp_list2 = tmp_list2->next)
            {
              item = (PPDItem *) tmp_list2->data;
              ppd_item_free (item);
              g_free (item);
            }

          g_list_free (tmp_list);
        }

      /* Free driver list and set the best one */
      if (list)
        {
          item = (PPDItem *) list->data;
          if (item)
            {
              result = g_new0 (PPDName, 1);
              result->ppd_name = g_strdup (item->ppd_name);
              result->ppd_match_level = item->match_level;
              switch (item->match_level)
                {
                  case PPD_GENERIC_MATCH:
                  case PPD_CLOSE_MATCH:
                    g_warning ("Found PPD does not match given device exactly!");
                    break;
                  default:
                    break;
                }
            }

          for (tmp_list = list; tmp_list; tmp_list = tmp_list->next)
            {
              item = (PPDItem *) tmp_list->data;
              ppd_item_free (item);
              g_free (item);
            }

          g_list_free (list);
        }

      if (mdls)
        {
          for (tmp_list = mdls; tmp_list; tmp_list = tmp_list->next)
            {
              item = (PPDItem *) tmp_list->data;
              ppd_item_free (item);
              g_free (item);
            }
          g_list_free (mdls);
        }

      g_free (mfg);
      g_free (mdl);
      g_free (mfg_normalized);
      g_free (mdl_normalized);
      if (equivalents)
        g_strfreev (equivalents);
    }

  return result;
}

char *
get_dest_attr (const char *dest_name,
               const char *attr)
{
  cups_dest_t *dests;
  int          num_dests;
  cups_dest_t *dest;
  const char  *value;
  char        *ret;

  if (dest_name == NULL)
          return NULL;

  ret = NULL;

  num_dests = cupsGetDests (&dests);
  if (num_dests < 1) {
          g_debug ("Unable to get printer destinations");
          return NULL;
  }

  dest = cupsGetDest (dest_name, NULL, num_dests, dests);
  if (dest == NULL) {
          g_debug ("Unable to find a printer named '%s'", dest_name);
          goto out;
  }

  value = cupsGetOption (attr, dest->num_options, dest->options);
  if (value == NULL) {
          g_debug ("Unable to get %s for '%s'", attr, dest_name);
          goto out;
  }
  ret = g_strdup (value);
out:
  cupsFreeDests (num_dests, dests);

  return ret;
}

ipp_t *
execute_maintenance_command (const char *printer_name,
                             const char *command,
                             const char *title)
{
  http_t *http;
  GError *error = NULL;
  ipp_t  *request = NULL;
  ipp_t  *response = NULL;
  gchar  *file_name = NULL;
  char   *uri;
  int     fd = -1;

  http = httpConnectEncrypt (cupsServer (),
                             ippPort (),
                             cupsEncryption ());

  if (!http)
    return NULL;

  request = ippNewRequest (IPP_PRINT_JOB);

  uri = g_strdup_printf ("ipp://localhost/printers/%s", printer_name);

  ippAddString (request,
                IPP_TAG_OPERATION,
                IPP_TAG_URI,
                "printer-uri",
                NULL,
                uri);

  g_free (uri);

  ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME, "job-name",
                NULL, title);

  ippAddString (request, IPP_TAG_JOB, IPP_TAG_MIMETYPE, "document-format",
                NULL, "application/vnd.cups-command");

  fd = g_file_open_tmp ("ccXXXXXX", &file_name, &error);

  if (fd != -1)
    {
      FILE *file;

      file = fdopen (fd, "w");
      fprintf (file, "#CUPS-COMMAND\n");
      fprintf (file, "%s\n", command);
      fclose (file);

      response = cupsDoFileRequest (http, request, "/", file_name);
      g_unlink (file_name);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_free (file_name);
  httpClose (http);

  return response;
}

int
ccGetAllowedUsers (gchar ***allowed_users, const char *printer_name)
{
  const char * const   attrs[1] = { "requesting-user-name-allowed" };
  http_t              *http;
  ipp_t               *request = NULL;
  gchar              **users = NULL;
  ipp_t               *response;
  char                *uri;
  int                  num_allowed_users = 0;

  http = httpConnectEncrypt (cupsServer (),
                             ippPort (),
                             cupsEncryption ());

  if (!http)
    {
      *allowed_users = NULL;
      return 0;
    }

  request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);

  uri = g_strdup_printf ("ipp://localhost/printers/%s", printer_name);

  ippAddString (request,
                IPP_TAG_OPERATION,
                IPP_TAG_URI,
                "printer-uri",
                NULL,
                uri);

  g_free (uri);

  ippAddStrings (request,
                 IPP_TAG_OPERATION,
                 IPP_TAG_KEYWORD,
                 "requested-attributes",
                 1,
                 NULL,
                 attrs);

  response = cupsDoRequest (http, request, "/");
  if (response)
    {
      ipp_attribute_t *attr = NULL;
      ipp_attribute_t *allowed = NULL;

      for (attr = response->attrs; attr != NULL; attr = attr->next)
        {
          if (attr->group_tag == IPP_TAG_PRINTER &&
              attr->value_tag == IPP_TAG_NAME &&
              !g_strcmp0 (attr->name, "requesting-user-name-allowed"))
            allowed = attr;
        }

      if (allowed && allowed->num_values > 0)
        {
          int i;

          num_allowed_users = allowed->num_values;
          users = g_new (gchar*, num_allowed_users);

          for (i = 0; i < num_allowed_users; i ++)
            users[i] = g_strdup (allowed->values[i].string.text);
        }
      ippDelete(response);
    }
  httpClose (http);

  *allowed_users = users;
  return num_allowed_users;
}

gchar *
get_ppd_attribute (const gchar *ppd_file_name,
                   const gchar *attribute_name)
{
  ppd_file_t *ppd_file = NULL;
  ppd_attr_t *ppd_attr = NULL;
  gchar *result = NULL;

  if (ppd_file_name)
    {
      ppd_file = ppdOpenFile (ppd_file_name);

      if (ppd_file)
        {
          ppd_attr = ppdFindAttr (ppd_file, attribute_name, NULL);
          if (ppd_attr != NULL)
            result = g_strdup (ppd_attr->value);
          ppdClose (ppd_file);
        }
    }

  return result;
}

/* Cancels subscription of given id */
void
cancel_cups_subscription (gint id)
{
  http_t *http;
  ipp_t  *request;

  if (id >= 0 &&
      ((http = httpConnectEncrypt (cupsServer (), ippPort (),
                                  cupsEncryption ())) != NULL)) {
    request = ippNewRequest (IPP_CANCEL_SUBSCRIPTION);
    ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                 "printer-uri", NULL, "/");
    ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                 "requesting-user-name", NULL, cupsUser ());
    ippAddInteger (request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                  "notify-subscription-id", id);
    ippDelete (cupsDoRequest (http, request, "/"));
    httpClose (http);
  }
}

/* Returns id of renewed subscription or new id */
gint
renew_cups_subscription (gint id,
                         const char * const *events,
                         gint num_events,
                         gint lease_duration)
{
  ipp_attribute_t              *attr = NULL;
  http_t                       *http;
  ipp_t                        *request;
  ipp_t                        *response = NULL;
  gint                          result = -1;

  if ((http = httpConnectEncrypt (cupsServer (), ippPort (),
                                  cupsEncryption ())) == NULL) {
    g_debug ("Connection to CUPS server \'%s\' failed.", cupsServer ());
  }
  else {
    if (id >= 0) {
      request = ippNewRequest (IPP_RENEW_SUBSCRIPTION);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                   "printer-uri", NULL, "/");
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                   "requesting-user-name", NULL, cupsUser ());
      ippAddInteger (request, IPP_TAG_OPERATION, IPP_TAG_INTEGER,
                    "notify-subscription-id", id);
      ippAddInteger (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                    "notify-lease-duration", lease_duration);
      response = cupsDoRequest (http, request, "/");
      if (response != NULL &&
          response->request.status.status_code <= IPP_OK_CONFLICT) {
        if ((attr = ippFindAttribute (response, "notify-lease-duration",
                                      IPP_TAG_INTEGER)) == NULL)
          g_debug ("No notify-lease-duration in response!\n");
        else
          if (attr->values[0].integer == lease_duration)
            result = id;
      }
    }

    if (result < 0) {
      request = ippNewRequest (IPP_CREATE_PRINTER_SUBSCRIPTION);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, "/");
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_NAME,
                    "requesting-user-name", NULL, cupsUser ());
      ippAddStrings (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD,
                     "notify-events", num_events, NULL, events);
      ippAddString (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_KEYWORD,
                    "notify-pull-method", NULL, "ippget");
      ippAddString (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_URI,
                    "notify-recipient-uri", NULL, "dbus://");
      ippAddInteger (request, IPP_TAG_SUBSCRIPTION, IPP_TAG_INTEGER,
                     "notify-lease-duration", lease_duration);
      response = cupsDoRequest (http, request, "/");

      if (response != NULL &&
          response->request.status.status_code <= IPP_OK_CONFLICT) {
        if ((attr = ippFindAttribute (response, "notify-subscription-id",
                                      IPP_TAG_INTEGER)) == NULL)
          g_debug ("No notify-subscription-id in response!\n");
        else
          result = attr->values[0].integer;
      }
    }

    if (response)
      ippDelete (response);

    httpClose (http);
  }

  return result;
}

/*  Set default destination in ~/.cups/lpoptions.
 *  Unset default destination if "dest" is NULL.
 */
void
set_local_default_printer (const gchar *printer_name)
{
  cups_dest_t *dests = NULL;
  int          num_dests = 0;
  int          i;

  num_dests = cupsGetDests (&dests);

  for (i = 0; i < num_dests; i ++)
    {
      if (printer_name && g_strcmp0 (dests[i].name, printer_name) == 0)
        dests[i].is_default = 1;
      else
        dests[i].is_default = 0;
    }

  cupsSetDests (num_dests, dests);
}

/*
 * This function does something which should be provided by CUPS...
 * It returns FALSE if the renaming fails.
 */
gboolean
printer_rename (const gchar *old_name,
                const gchar *new_name)
{
  ipp_attribute_t  *attr = NULL;
  cups_ptype_t      printer_type = 0;
  cups_dest_t      *dests = NULL;
  cups_dest_t      *dest = NULL;
  cups_job_t       *jobs = NULL;
  GDBusConnection  *bus;
  const char       *printer_location = NULL;
  const char       *ppd_filename = NULL;
  const char       *printer_info = NULL;
  const char       *printer_uri = NULL;
  const char       *device_uri = NULL;
  const char       *job_sheets = NULL;
  gboolean          result = FALSE;
  gboolean          accepting = TRUE;
  gboolean          printer_paused = FALSE;
  gboolean          default_printer = FALSE;
  gboolean          printer_shared = FALSE;
  GError           *error = NULL;
  http_t           *http;
  gchar           **sheets = NULL;
  gchar           **users_allowed = NULL;
  gchar           **users_denied = NULL;
  gchar           **member_names = NULL;
  gchar            *start_sheet = NULL;
  gchar            *end_sheet = NULL;
  gchar            *error_policy = NULL;
  gchar            *op_policy = NULL;
  ipp_t            *request;
  ipp_t            *response;
  gint              i;
  int               num_dests = 0;
  int               num_jobs = 0;
  static const char * const requested_attrs[] = {
    "printer-error-policy",
    "printer-op-policy",
    "requesting-user-name-allowed",
    "requesting-user-name-denied",
    "member-names"};

  if (old_name == NULL ||
      old_name[0] == '\0' ||
      new_name == NULL ||
      new_name[0] == '\0' ||
      g_strcmp0 (old_name, new_name) == 0)
    return FALSE;

  num_dests = cupsGetDests (&dests);

  dest = cupsGetDest (new_name, NULL, num_dests, dests);
  if (dest)
    {
      cupsFreeDests (num_dests, dests);
      return FALSE;
    }

  num_jobs = cupsGetJobs (&jobs, old_name, 0, CUPS_WHICHJOBS_ACTIVE);
  cupsFreeJobs (num_jobs, jobs);
  if (num_jobs > 1)
    {
      g_warning ("There are queued jobs on printer %s!", old_name);
      cupsFreeDests (num_dests, dests);
      return FALSE;
    }

  /*
   * Gather some informations about the original printer
   */
  dest = cupsGetDest (old_name, NULL, num_dests, dests);
  if (dest)
    {
      for (i = 0; i < dest->num_options; i++)
        {
          if (g_strcmp0 (dest->options[i].name, "printer-is-accepting-jobs") == 0)
            accepting = g_strcmp0 (dest->options[i].value, "true") == 0;
          else if (g_strcmp0 (dest->options[i].name, "printer-is-shared") == 0)
            printer_shared = g_strcmp0 (dest->options[i].value, "true") == 0;
          else if (g_strcmp0 (dest->options[i].name, "device-uri") == 0)
            device_uri = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-uri-supported") == 0)
            printer_uri = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-info") == 0)
            printer_info = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-location") == 0)
            printer_location = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-state") == 0)
            printer_paused = g_strcmp0 (dest->options[i].value, "5") == 0;
          else if (g_strcmp0 (dest->options[i].name, "job-sheets") == 0)
            job_sheets = dest->options[i].value;
          else if (g_strcmp0 (dest->options[i].name, "printer-type") == 0)
            printer_type = atoi (dest->options[i].value);
        }
      default_printer = dest->is_default;
    }
  cupsFreeDests (num_dests, dests);

  if (accepting)
    {
      printer_set_accepting_jobs (old_name, FALSE, NULL);

      num_jobs = cupsGetJobs (&jobs, old_name, 0, CUPS_WHICHJOBS_ACTIVE);
      cupsFreeJobs (num_jobs, jobs);
      if (num_jobs > 1)
        {
          printer_set_accepting_jobs (old_name, accepting, NULL);
          g_warning ("There are queued jobs on printer %s!", old_name);
          return FALSE;
        }
    }


  /*
   * Gather additional informations about the original printer
   */
  if ((http = httpConnectEncrypt (cupsServer (), ippPort (),
                                  cupsEncryption ())) != NULL)
    {
      request = ippNewRequest (IPP_GET_PRINTER_ATTRIBUTES);
      ippAddString (request, IPP_TAG_OPERATION, IPP_TAG_URI,
                    "printer-uri", NULL, printer_uri);
      ippAddStrings (request, IPP_TAG_OPERATION, IPP_TAG_KEYWORD,
                     "requested-attributes", G_N_ELEMENTS (requested_attrs), NULL, requested_attrs);
      response = cupsDoRequest (http, request, "/");

      if (response)
        {
          if (response->request.status.status_code <= IPP_OK_CONFLICT)
            {
              attr = ippFindAttribute (response, "printer-error-policy", IPP_TAG_NAME);
              if (attr)
                error_policy = g_strdup (attr->values[0].string.text);

              attr = ippFindAttribute (response, "printer-op-policy", IPP_TAG_NAME);
              if (attr)
                op_policy = g_strdup (attr->values[0].string.text);

              attr = ippFindAttribute (response, "requesting-user-name-allowed", IPP_TAG_NAME);
              if (attr && attr->num_values > 0)
                {
                  users_allowed = g_new0 (gchar *, attr->num_values + 1);
                  for (i = 0; i < attr->num_values; i++)
                    users_allowed[i] = g_strdup (attr->values[i].string.text);
                }

              attr = ippFindAttribute (response, "requesting-user-name-denied", IPP_TAG_NAME);
              if (attr && attr->num_values > 0)
                {
                  users_denied = g_new0 (gchar *, attr->num_values + 1);
                  for (i = 0; i < attr->num_values; i++)
                    users_denied[i] = g_strdup (attr->values[i].string.text);
                }

              attr = ippFindAttribute (response, "member-names", IPP_TAG_NAME);
              if (attr && attr->num_values > 0)
                {
                  member_names = g_new0 (gchar *, attr->num_values + 1);
                  for (i = 0; i < attr->num_values; i++)
                    member_names[i] = g_strdup (attr->values[i].string.text);
                }
            }
          ippDelete (response);
        }
      httpClose (http);
    }

  if (job_sheets)
    {
      sheets = g_strsplit (job_sheets, ",", 0);
      if (g_strv_length (sheets) > 1)
        {
          start_sheet = sheets[0];
          end_sheet = sheets[1];
        }
    }

  ppd_filename = cupsGetPPD (old_name);

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
   }
  else
    {
      if (printer_type & CUPS_PRINTER_CLASS)
        {
          if (member_names)
            for (i = 0; i < g_strv_length (member_names); i++)
              class_add_printer (new_name, member_names[i]);
        }
      else
        {
          GVariant *output;

          output = g_dbus_connection_call_sync (bus,
                                                MECHANISM_BUS,
                                                "/",
                                                MECHANISM_BUS,
                                                "PrinterAddWithPpdFile",
                                                g_variant_new ("(sssss)",
                                                               new_name,
                                                               device_uri ? device_uri : "",
                                                               ppd_filename ? ppd_filename : "",
                                                               printer_info ? printer_info : "",
                                                               printer_location ? printer_location : ""),
                                                G_VARIANT_TYPE ("(s)"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                NULL,
                                                &error);
          g_object_unref (bus);

          if (output)
            {
              const gchar *ret_error;

              g_variant_get (output, "(&s)", &ret_error);
              if (ret_error[0] != '\0')
                g_warning ("%s", ret_error);

              g_variant_unref (output);
            }
          else
            {
              g_warning ("%s", error->message);
              g_error_free (error);
            }
        }
    }

  if (ppd_filename)
    g_unlink (ppd_filename);

  num_dests = cupsGetDests (&dests);
  dest = cupsGetDest (new_name, NULL, num_dests, dests);
  if (dest)
    {
      printer_set_accepting_jobs (new_name, accepting, NULL);
      printer_set_enabled (new_name, !printer_paused);
      printer_set_shared (new_name, printer_shared);
      printer_set_job_sheets (new_name, start_sheet, end_sheet);
      printer_set_policy (new_name, op_policy, FALSE);
      printer_set_policy (new_name, error_policy, TRUE);
      printer_set_users (new_name, users_allowed, TRUE);
      printer_set_users (new_name, users_denied, FALSE);
      if (default_printer)
        printer_set_default (new_name);

      printer_delete (old_name);

      result = TRUE;
    }
  else
    printer_set_accepting_jobs (old_name, accepting, NULL);

  cupsFreeDests (num_dests, dests);
  g_free (op_policy);
  g_free (error_policy);
  if (sheets)
    g_strfreev (sheets);
  if (users_allowed)
    g_strfreev (users_allowed);
  if (users_denied)
    g_strfreev (users_denied);

  return result;
}

gboolean
printer_set_location (const gchar *printer_name,
                      const gchar *location)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name || !location)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetLocation",
                                        g_variant_new ("(ss)", printer_name, location),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_set_accepting_jobs (const gchar *printer_name,
                            gboolean     accepting_jobs,
                            const gchar *reason)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetAcceptJobs",
                                        g_variant_new ("(sbs)",
                                                       printer_name,
                                                       accepting_jobs,
                                                       reason ? reason : ""),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;
      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_set_enabled (const gchar *printer_name,
                     gboolean     enabled)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetEnabled",
                                        g_variant_new ("(sb)", printer_name, enabled),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_delete (const gchar *printer_name)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterDelete",
                                        g_variant_new ("(s)", printer_name),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_set_default (const gchar *printer_name)
{
  GDBusConnection *bus;
  const char *cups_server;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name)
    return TRUE;

  cups_server = cupsServer ();
  if (g_ascii_strncasecmp (cups_server, "localhost", 9) == 0 ||
      g_ascii_strncasecmp (cups_server, "127.0.0.1", 9) == 0 ||
      g_ascii_strncasecmp (cups_server, "::1", 3) == 0 ||
      cups_server[0] == '/')
    {
      /* Clean .cups/lpoptions before setting
       * default printer on local CUPS server.
       */
      set_local_default_printer (NULL);

      bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
      if (!bus)
        {
          g_warning ("Failed to get system bus: %s", error->message);
          g_error_free (error);
        }
      else
        {
          output = g_dbus_connection_call_sync (bus,
                                                MECHANISM_BUS,
                                                "/",
                                                MECHANISM_BUS,
                                                "PrinterSetDefault",
                                                g_variant_new ("(s)", printer_name),
                                                G_VARIANT_TYPE ("(s)"),
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                NULL,
                                                &error);
          g_object_unref (bus);

          if (output)
            {
              const gchar *ret_error;

              g_variant_get (output, "(&s)", &ret_error);
              if (ret_error[0] != '\0')
                g_warning ("%s", ret_error);
              else
                result = TRUE;

              g_variant_unref (output);
            }
          else
            {
              g_warning ("%s", error->message);
              g_error_free (error);
            }
        }
    }
  else
    /* Store default printer to .cups/lpoptions
     * if we are connected to a remote CUPS server.
     */
    {
      set_local_default_printer (printer_name);
    }

  return result;
}

gboolean
printer_set_shared (const gchar *printer_name,
                    gboolean     shared)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetShared",
                                        g_variant_new ("(sb)", printer_name, shared),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_set_job_sheets (const gchar *printer_name,
                        const gchar *start_sheet,
                        const gchar *end_sheet)
{
  GDBusConnection *bus;
  GVariant   *output;
  GError     *error = NULL;
  gboolean    result = FALSE;

  if (!printer_name || !start_sheet || !end_sheet)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterSetJobSheets",
                                        g_variant_new ("(sss)", printer_name, start_sheet, end_sheet),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_set_policy (const gchar *printer_name,
                    const gchar *policy,
                    gboolean     error_policy)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean   result = FALSE;
  GError     *error = NULL;

  if (!printer_name || !policy)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  if (error_policy)
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetErrorPolicy",
                                          g_variant_new ("(ss)", printer_name, policy),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
  else
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetOpPolicy",
                                          g_variant_new ("(ss)", printer_name, policy),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_set_users (const gchar  *printer_name,
                   gchar       **users,
                   gboolean      allowed)
{
  GDBusConnection *bus;
  GVariantBuilder array_builder;
  gint        i;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!printer_name || !users)
    return TRUE;
  
  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
  for (i = 0; users[i]; i++)
    g_variant_builder_add (&array_builder, "s", users[i]);

  if (allowed)
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetUsersAllowed",
                                          g_variant_new ("(sas)", printer_name, &array_builder),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
  else
    output = g_dbus_connection_call_sync (bus,
                                          MECHANISM_BUS,
                                          "/",
                                          MECHANISM_BUS,
                                          "PrinterSetUsersDenied",
                                          g_variant_new ("(sas)", printer_name, &array_builder),
                                          G_VARIANT_TYPE ("(s)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
class_add_printer (const gchar *class_name,
                   const gchar *printer_name)
{
  GDBusConnection *bus;
  GVariant   *output;
  gboolean    result = FALSE;
  GError     *error = NULL;

  if (!class_name || !printer_name)
    return TRUE;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return TRUE;
   }

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "ClassAddPrinter",
                                        g_variant_new ("(ss)", class_name, printer_name),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);
      else
        result = TRUE;

      g_variant_unref (output);
    }
  else
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  return result;
}

gboolean
printer_is_local (cups_ptype_t  printer_type,
                  const gchar  *device_uri)
{
  gboolean result = TRUE;
  char     scheme[HTTP_MAX_URI];
  char     username[HTTP_MAX_URI];
  char     hostname[HTTP_MAX_URI];
  char     resource[HTTP_MAX_URI];
  int      port;

  if (printer_type &
      (CUPS_PRINTER_DISCOVERED |
       CUPS_PRINTER_REMOTE |
       CUPS_PRINTER_IMPLICIT))
    result = FALSE;

  if (device_uri == NULL || !result)
    return result;

  httpSeparateURI (HTTP_URI_CODING_ALL, device_uri,
		   scheme, sizeof (scheme), 
		   username, sizeof (username),
		   hostname, sizeof (hostname),
		   &port,
		   resource, sizeof (resource));

  if (g_str_equal (scheme, "ipp") ||
      g_str_equal (scheme, "smb") ||
      g_str_equal (scheme, "socket") ||
      g_str_equal (scheme, "lpd"))
    result = FALSE;

  return result;
}

gchar*
printer_get_hostname (cups_ptype_t  printer_type,
                      const gchar  *device_uri,
                      const gchar  *printer_uri)
{
  gboolean  local = TRUE;
  gchar    *result = NULL;
  char      scheme[HTTP_MAX_URI];
  char      username[HTTP_MAX_URI];
  char      hostname[HTTP_MAX_URI];
  char      resource[HTTP_MAX_URI];
  int       port;

  if (device_uri == NULL)
    return result;

  if (printer_type & (CUPS_PRINTER_DISCOVERED |
                      CUPS_PRINTER_REMOTE |
                      CUPS_PRINTER_IMPLICIT))
    {
      if (printer_uri)
        {
          httpSeparateURI (HTTP_URI_CODING_ALL, printer_uri,
                           scheme, sizeof (scheme),
                           username, sizeof (username),
                           hostname, sizeof (hostname),
                           &port,
                           resource, sizeof (resource));

          if (hostname[0] != '\0')
            result = g_strdup (hostname);
        }

      local = FALSE;
    }

  if (result == NULL && device_uri)
    {
      httpSeparateURI (HTTP_URI_CODING_ALL, device_uri,
                       scheme, sizeof (scheme),
                       username, sizeof (username),
                       hostname, sizeof (hostname),
                       &port,
                       resource, sizeof (resource));

      if (g_str_equal (scheme, "ipp") ||
          g_str_equal (scheme, "smb") ||
          g_str_equal (scheme, "socket") ||
          g_str_equal (scheme, "lpd"))
        {
          if (hostname[0] != '\0')
            result = g_strdup (hostname);

          local = FALSE;
        }
    }

  if (local)
    result = g_strdup ("localhost");

  return result;
}

/* Returns default media size for current locale */
static const gchar *
get_paper_size_from_locale ()
{
  if (g_str_equal (gtk_paper_size_get_default (), GTK_PAPER_NAME_LETTER))
    return "na-letter";
  else
    return "iso-a4";
}

/* Set default media size according to the locale */
void
printer_set_default_media_size (const gchar *printer_name)
{
  GVariantBuilder  array_builder;
  GDBusConnection *bus;
  GVariant        *output;
  GError          *error = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!bus)
   {
     g_warning ("Failed to get system bus: %s", error->message);
     g_error_free (error);
     return;
   }

  g_variant_builder_init (&array_builder, G_VARIANT_TYPE ("as"));
  g_variant_builder_add (&array_builder, "s", get_paper_size_from_locale ());

  output = g_dbus_connection_call_sync (bus,
                                        MECHANISM_BUS,
                                        "/",
                                        MECHANISM_BUS,
                                        "PrinterAddOption",
                                        g_variant_new ("(ssas)",
                                                       printer_name,
                                                       "media",
                                                       &array_builder),
                                        G_VARIANT_TYPE ("(s)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);

  g_object_unref (bus);

  if (output)
    {
      const gchar *ret_error;

      g_variant_get (output, "(&s)", &ret_error);
      if (ret_error[0] != '\0')
        g_warning ("%s", ret_error);

      g_variant_unref (output);
    }
  else
    {
      if (!(error->domain == G_DBUS_ERROR &&
            (error->code == G_DBUS_ERROR_SERVICE_UNKNOWN ||
             error->code == G_DBUS_ERROR_UNKNOWN_METHOD)))
        g_warning ("%s", error->message);
      g_error_free (error);
    }
}
