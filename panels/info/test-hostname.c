#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "hostname-helper.h"

struct {
	char *input;
	char *output_display;
	char *output_real;
} tests[] = {
	{ "Lennart's PC", "Lennarts-PC", "lennarts-pc" },
	{ "Müllers Computer", "Mullers-Computer", "mullers-computer" },
	{ "Voran!", "Voran", "voran" },
	{ "Es war einmal ein Männlein", "Es-war-einmal-ein-Mannlein", "es-war-einmal-ein-mannlein" },
	{ "Jawoll. Ist doch wahr!", "Jawoll-Ist-doch-wahr", "jawoll-ist-doch-wahr" },
	{ "レナート", "localhost", "localhost" },
	{ "!!!", "localhost", "localhost" },
	{ "...zack!!! zack!...", "zack-zack", "zack-zack" },
	{ "Bãstien's computer... Foo-bar", "Bastiens-computer-Foo-bar", "bastiens-computer-foo-bar" },
	{ "", "localhost", "localhost" },
};

int main (int argc, char **argv)
{
	char *result;
	guint i;

	setlocale (LC_ALL, "");
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* FIXME:
	 * - Tests don't work in non-UTF-8 locales
	 * - They also fail in de_DE.UTF-8 because of "ü" -> "ue" conversions */

	for (i = 0; i < G_N_ELEMENTS (tests); i++) {
		result = pretty_hostname_to_static (tests[i].input, FALSE);
		if (g_strcmp0 (result, tests[i].output_real) != 0)
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 tests[i].input, tests[i].output_real, result);
		else
			g_debug ("Result for '%s' matches '%s'",
				 tests[i].input, result);
		g_free (result);

		result = pretty_hostname_to_static (tests[i].input, TRUE);
		if (g_strcmp0 (result, tests[i].output_display) != 0)
			g_error ("Result for '%s' doesn't match '%s' (got: '%s')",
				 tests[i].input, tests[i].output_display, result);
		else
			g_debug ("Result for '%s' matches '%s'",
				 tests[i].input, result);
		g_free (result);
	}

	return 0;
}
