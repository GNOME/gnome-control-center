#include <config.h>
#include <locale.h>

#include "tz.h"

static void
test_timezone_gfx (gconstpointer data)
{
	const char *pixmap_dir = data;
	g_autoptr(TzDB) db = NULL;
	GPtrArray *locs;
	guint i;

	db = tz_load_db ();
	locs = tz_get_locations (db);
	for (i = 0; i < locs->len ; i++) {
		TzLocation *loc = locs->pdata[i];
		TzInfo *info;
		g_autofree gchar *filename = NULL;
		g_autofree gchar *path = NULL;
		gdouble selected_offset;
                char buf[16];

		info = tz_info_from_location (loc);
		selected_offset = tz_location_get_utc_offset (loc)
			/ (60.0*60.0) + ((info->daylight) ? -1.0 : 0.0);

		filename = g_strdup_printf ("timezone_%s.png",
                                            g_ascii_formatd (buf, sizeof (buf),
                                                             "%g", selected_offset));
		path = g_build_filename (pixmap_dir, filename, NULL);

		if (g_file_test (path, G_FILE_TEST_IS_REGULAR) == FALSE) {
			g_message ("File '%s' missing for zone '%s'", filename, loc->zone);
			g_test_fail ();
		}
	}
}

int main (int argc, char **argv)
{
	char *pixmap_dir;

        setlocale (LC_ALL, "");
	g_test_init (&argc, &argv, NULL);

	g_setenv ("G_DEBUG", "fatal_warnings", FALSE);

	if (argc == 2) {
		pixmap_dir = g_strdup (argv[1]);
	} else if (argc == 1) {
		pixmap_dir = g_strdup (SRCDIR "/data/");
	} else {
		g_message ("Usage: %s [PIXMAP DIRECTORY]", argv[0]);
		return 1;
	}

	g_test_add_data_func ("/datetime/timezone-gfx", pixmap_dir, test_timezone_gfx);

	return g_test_run ();
}
