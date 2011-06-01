#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "hostname-helper.h"

int main (int argc, char **argv)
{
	char *result;
	guint i;
	char *contents;
	char **lines;
	char *locale;

	/* Running in some locales will
	 * break the tests as "ü" will be transliterated to
	 * "ue" in de_DE, and 'u"' in the C locale.
	 *
	 * Work around that by forcing en_US with UTF-8 in
	 * our tests
	 * https://bugzilla.gnome.org/show_bug.cgi?id=650342 */
	locale = setlocale (LC_ALL, "en_US.UTF-8");
	if (locale == NULL) {
		g_debug("Missing en_US.UTF-8 locale, ignoring test.");
		return 0;
	}
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	if (g_file_get_contents (argv[1], &contents, NULL, NULL) == FALSE) {
		g_warning ("Failed to load '%s'", argv[1]);
		return 1;
	}
	lines = g_strsplit (contents, "\n", -1);
	if (lines == NULL) {
		g_warning ("Test file is empty");
		return 1;
	}

	for (i = 0; lines[i] != NULL; i++) {
		char *utf8;
		char **items;

		if (*lines[i] == '#')
			continue;
		if (*lines[i] == '\0')
			break;

		items = g_strsplit (lines[i], "\t", -1);
		utf8 = g_locale_from_utf8 (items[0], -1, NULL, NULL, NULL);
		result = pretty_hostname_to_static (items[0], FALSE);
		if (g_strcmp0 (result, items[2]) != 0)
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 utf8, items[2], result);
		else
			g_debug ("Result for '%s' matches '%s'",
				 utf8, result);
		g_free (result);
		g_free (utf8);

		result = pretty_hostname_to_static (items[0], TRUE);
		utf8 = g_locale_from_utf8 (items[0], -1, NULL, NULL, NULL);
		if (g_strcmp0 (result, items[1]) != 0)
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 utf8, items[1], result);
		else
			g_debug ("Result for '%s' matches '%s'",
				 utf8, result);
		g_free (result);
		g_free (utf8);

		g_strfreev (items);
	}

	g_strfreev (lines);

	return 0;
}
