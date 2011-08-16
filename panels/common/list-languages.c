#include <glib.h>
#include <glib-object.h>
#include "gdm-languages.h"

int main (int argc, char **argv)
{
	char **langs;
	guint i;

	g_type_init ();

	langs = gdm_get_all_language_names ();
	if (langs == NULL) {
		g_warning ("No languages found");
		return 1;
	}

	for (i = 0; langs[i] != NULL; i++)
		g_print ("%s == %s\n", langs[i], gdm_get_language_from_name (langs[i], NULL));

	g_strfreev (langs);

	return 0;
}

