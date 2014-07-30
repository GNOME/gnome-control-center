#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "pp-utils.h"

int
main (int argc, char **argv)
{
  guint   i, j;
  char   *contents;
  char  **lines;
  char   *locale;

  /* Running in some locales will
   * break the tests as "ü" will be transliterated to
   * "ue" in de_DE, and 'u"' in the C locale.
   *
   * Work around that by forcing en_US with UTF-8 in
   * our tests
   * https://bugzilla.gnome.org/show_bug.cgi?id=650342 */

  locale = setlocale (LC_ALL, "en_US.UTF-8");
  if (locale == NULL)
    {
      g_debug("Missing en_US.UTF-8 locale, ignoring test.");
      return 0;
    }

  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  if (g_file_get_contents (argv[1], &contents, NULL, NULL) == FALSE)
    {
      g_warning ("Failed to load '%s'", argv[1]);
      return 1;
    }

  lines = g_strsplit (contents, "\n", -1);
  if (lines == NULL)
    {
      g_warning ("Test file is empty");
      return 1;
    }

  for (i = 0; lines[i] != NULL; i++)
    {
      char **items;

      if (*lines[i] == '#')
        continue;

      if (*lines[i] == '\0')
        break;

      items = g_strsplit (lines[i], "\t", -1);
      if (g_strv_length (items) == 6)
        {
          PpPrintDevice  *device, *tmp;
          gchar         **already_present_printers;
          gchar          *canonicalized_name;
          GList          *devices = NULL;

          already_present_printers = g_strsplit (items[0], " ", -1);

          for (j = 0; already_present_printers[j] != NULL; j++)
            {
              tmp = g_new0 (PpPrintDevice, 1);
              tmp->device_original_name = g_strdup (already_present_printers[j]);

              devices = g_list_append (devices, tmp);
            }

          device = g_new0 (PpPrintDevice, 1);
          device->device_id = g_strdup (items[1]);
          device->device_make_and_model = g_strdup (items[2]);
          device->device_original_name = g_strdup (items[3]);
          device->device_info = g_strdup (items[4]);

          canonicalized_name =
            canonicalize_device_name (devices, NULL, NULL, 0, device);

          if (g_strcmp0 (canonicalized_name, items[5]) != 0)
            {
              g_error ("Result for ('%s', '%s', '%s', '%s') doesn't match '%s' (got: '%s')",
                        items[1], items[2], items[3], items[4], items[5], canonicalized_name);
            }
          else
            {
              g_debug ("Result for ('%s', '%s', '%s', '%s') matches '%s'",
                       items[1], items[2], items[3], items[4], canonicalized_name);
            }

          g_free (canonicalized_name);
          pp_print_device_free (device);
          g_list_free_full (devices, (GDestroyNotify) pp_print_device_free);
          g_strfreev (already_present_printers);
        }
      else
        {
          g_warning ("Line number %u has not correct number of items!", i);
        }

      g_strfreev (items);
    }

  g_strfreev (lines);
  g_free (contents);

  return 0;
}
