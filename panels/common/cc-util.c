/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-util.h"

/* Combining diacritical mark?
 *  Basic range: [0x0300,0x036F]
 *  Supplement:  [0x1DC0,0x1DFF]
 *  For Symbols: [0x20D0,0x20FF]
 *  Half marks:  [0xFE20,0xFE2F]
 */
#define IS_CDM_UCS4(c) (((c) >= 0x0300 && (c) <= 0x036F)  || \
                        ((c) >= 0x1DC0 && (c) <= 0x1DFF)  || \
                        ((c) >= 0x20D0 && (c) <= 0x20FF)  || \
                        ((c) >= 0xFE20 && (c) <= 0xFE2F))

/* Copied from tracker/src/libtracker-fts/tracker-parser-glib.c under the GPL
 * And then from gnome-shell/src/shell-util.c
 *
 * Originally written by Aleksander Morgado <aleksander@gnu.org>
 */
char *
cc_util_normalize_casefold_and_unaccent (const char *str)
{
  char *normalized, *tmp;
  int i = 0, j = 0, ilen;

  if (str == NULL)
    return NULL;

  normalized = g_utf8_normalize (str, -1, G_NORMALIZE_NFKD);
  tmp = g_utf8_casefold (normalized, -1);
  g_free (normalized);

  ilen = strlen (tmp);

  while (i < ilen)
    {
      gunichar unichar;
      gchar *next_utf8;
      gint utf8_len;

      /* Get next character of the word as UCS4 */
      unichar = g_utf8_get_char_validated (&tmp[i], -1);

      /* Invalid UTF-8 character or end of original string. */
      if (unichar == (gunichar) -1 ||
          unichar == (gunichar) -2)
        {
          break;
        }

      /* Find next UTF-8 character */
      next_utf8 = g_utf8_next_char (&tmp[i]);
      utf8_len = next_utf8 - &tmp[i];

      if (IS_CDM_UCS4 ((guint32) unichar))
        {
          /* If the given unichar is a combining diacritical mark,
           * just update the original index, not the output one */
          i += utf8_len;
          continue;
        }

      /* If already found a previous combining
       * diacritical mark, indexes are different so
       * need to copy characters. As output and input
       * buffers may overlap, need to use memmove
       * instead of memcpy */
      if (i != j)
        {
          memmove (&tmp[j], &tmp[i], utf8_len);
        }

      /* Update both indexes */
      i += utf8_len;
      j += utf8_len;
    }

  /* Force proper string end */
  tmp[j] = '\0';

  return tmp;
}

typedef struct {
  const gchar *value;
  const gchar *description;
} CcInputSwitcherOptions;

static CcInputSwitcherOptions cc_input_switcher_options[] = {
  { "grp:lshift_toggle", N_("Left Shift") },
  { "grp:lalt_toggle", N_("Left Alt") },
  { "grp:lctrl_toggle", N_("Left Ctrl") },
  { "grp:rshift_toggle", N_("Right Shift") },
  { "grp:toggle", N_("Right Alt") },
  { "grp:rctrl_toggle", N_("Right Ctrl") },
  { "grp:lalt_lshift_toggle", N_("Left Alt+Shift") },
  { "grp:lctrl_lshift_toggle", N_("Left Ctrl+Shift") },
  { "grp:rctrl_rshift_toggle", N_("Right Ctrl+Shift") },
  { "grp:alt_shift_toggle", N_("Alt+Shift") },
  { "grp:ctrl_shift_toggle", N_("Ctrl+Shift") },
  { "grp:ctrl_alt_toggle", N_("Alt+Ctrl") },
  { "grp:caps_toggle", N_("Caps") },
  { "grp:shift_caps_toggle", N_("Shift+Caps") },
  { "grp:alt_caps_toggle", N_("Alt+Caps") },
  { NULL, NULL }
};

const gchar *
cc_util_xkb_info_description_for_option (void        *info,
                                         const gchar *group_id,
                                         const gchar *id)
{
  CcInputSwitcherOptions *option;

  if (!g_str_equal (group_id, "grp"))
    return gnome_xkb_info_description_for_option (info, group_id, id);

  for (option = &cc_input_switcher_options[0]; option->value != NULL; option++)
    if (g_str_equal (id, option->value))
      return _(option->description);

  return gnome_xkb_info_description_for_option (info, group_id, id);
}
