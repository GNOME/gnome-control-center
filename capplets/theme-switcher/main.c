#include <gnome.h>
#include <bonobo.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <config.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

static GladeXML *xml;
static gchar** themes = NULL;
static GtkListStore *model;
static gboolean auto_preview;

enum
{
	RESPONSE_APPLY = 1,
	RESPONSE_CLOSE
};

static void
themes_list_add_dir (GArray *arr, const char *dirname)
{
	DIR *dir;
	struct dirent *de;
	const gchar *suffix = "gtk-2.0";

	g_return_if_fail (arr != NULL);
	g_return_if_fail (dirname != NULL);

	dir = opendir (dirname);
	if (!dir)
		return;

	while ((de = readdir (dir)))
	{
		char *tmp;
		
		if (de->d_name[0] == '.')
			continue;

		tmp = g_build_filename (dirname, de->d_name, suffix, NULL);
		if (!g_file_test (tmp, G_FILE_TEST_IS_DIR))
		{
			g_free (tmp);
			continue;
		}
		g_free (tmp);

		tmp = g_build_filename (dirname, de->d_name, NULL);
		g_array_append_val (arr, tmp);
	}

	closedir (dir);
}

static void
themes_list_refresh (void)
{
	GArray *arr;
	gchar *dir;
	int i;
	
	if (themes)
		g_strfreev (themes);

	arr = g_array_new (TRUE, TRUE, sizeof (gchar*));
	
	dir = g_build_filename (g_get_home_dir (), ".themes", NULL);
	themes_list_add_dir (arr, dir);
	g_free (dir);

	dir = gtk_rc_get_theme_dir ();
	themes_list_add_dir (arr, dir);
	g_free (dir);

	themes = (gchar**) arr->data;
	g_array_free (arr, FALSE);

	gtk_list_store_clear (model);
	for (i = 0; themes[i] != NULL; i++)
	{
		GtkTreeIter iter;
		gchar *basename;
		basename = g_path_get_basename (themes[i]);
		gtk_list_store_append (model, &iter);
		gtk_list_store_set (model, &iter, 0, basename, -1);
		g_free (basename);
	}
}

static void
select_foreach_cb (GtkTreeModel *model, GtkTreePath *path,
		   GtkTreeIter *iter, int *index)
{
	int *inds = gtk_tree_path_get_indices (path);
	*index = *inds;
}

static gchar* get_selected_theme (void)
{
	int index = -1;
	gchar *theme;
	GtkTreeView *view = GTK_TREE_VIEW (glade_xml_get_widget (xml, "tree1"));
	
	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (view), (GtkTreeSelectionForeachFunc) select_foreach_cb, &index);
	
	if (index == -1)
		return NULL;

	theme = g_build_filename (themes[index], "gtk-2.0", "gtkrc", NULL);
	return theme;
}


static gchar* get_selected_theme_name (void)
{
	int index = -1;
	GtkTreeView *view = GTK_TREE_VIEW (glade_xml_get_widget (xml, "tree1"));
	
	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (view), (GtkTreeSelectionForeachFunc) select_foreach_cb, &index);
	
	if (index == -1)
		return NULL;

	return g_path_get_basename (themes[index]);
}

static void
apply_cb (void)
{
	gchar *name = get_selected_theme_name ();
	if (name)
	{
		gconf_client_set_string (gconf_client_get_default (), "/desktop/gnome/interface/gtk_theme", name, NULL);
		g_free (name);
	}
}

void
call_apply (GtkWidget *widget, gpointer data)
{
  apply_cb ();
}


void
response_cb (GtkDialog *dialog, gint r, gpointer data)
{
	switch (r)
	{
	case RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	}
}

void
preview_toggled_cb (GtkToggleButton *b, gpointer data)
{
	auto_preview = gtk_toggle_button_get_active (b);
}

static void
select_cb (GtkTreeSelection *sel, GtkButton *b)
{
	GtkWidget *control;
	gchar *theme;

	if (!(auto_preview || b))
		return;

	theme = get_selected_theme ();
	if (!theme)
		return;

	control = glade_xml_get_widget (xml, "control1");
	bonobo_widget_set_property (BONOBO_WIDGET (control),
				    "theme", TC_CORBA_string,
				    theme, NULL);
	g_free (theme);
}

void
preview_cb (GtkButton *b, gpointer data)
{
	select_cb (NULL, b);
}

static void
fsel_ok_cb (GtkButton *b, GtkFileSelection *sel)
{
	const gchar *filename = gtk_file_selection_get_filename (sel);
	gchar *command;
	gchar *todir;
	
	if (!filename)
		return;

	todir = g_build_filename (g_get_home_dir (), ".themes", NULL);
	if (!g_file_test (todir, G_FILE_TEST_IS_DIR))
		mkdir (todir, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	
	command = g_strdup_printf ("gzip -d -c < \"%s\" | tar xf - -C \"%s\"", filename, todir);
	system (command);
	g_free (command);
	g_free (todir);
	gtk_widget_destroy (GTK_WIDGET (sel));
	
	themes_list_refresh ();
}

void
install_cb (GtkButton *b, gpointer data)
{
	GtkFileSelection *sel = GTK_FILE_SELECTION (gtk_file_selection_new (_("Select a theme to install")));
	g_signal_connect (G_OBJECT (sel->ok_button), "clicked", (GCallback) fsel_ok_cb, sel);
	g_signal_connect_swapped (G_OBJECT (sel->cancel_button), "clicked", (GCallback) gtk_widget_destroy, sel);
	gtk_widget_show_all (GTK_WIDGET (sel));
}

static void
setup_list (void)
{
	GtkTreeView *view;
	GtkCellRenderer *cell;
	GtkTreeSelection *sel;
	
	model = gtk_list_store_new (1, G_TYPE_STRING);
	
	view = GTK_TREE_VIEW (glade_xml_get_widget (xml, "tree1"));
	g_object_set (G_OBJECT (view), "model", model, NULL);
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (view, -1, "", cell, "text", 0, NULL);

	themes_list_refresh ();
	
	sel = gtk_tree_view_get_selection (view);
	g_signal_connect (G_OBJECT (sel), "changed", (GCallback) select_cb, NULL);
}

int
main (int argc, char **argv)
{
	gnome_program_init ("gtk-theme-selector", VERSION,
			    LIBGNOMEUI_MODULE, argc, argv, NULL);

	xml = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gtk-theme-selector.glade", NULL, NULL);
	setup_list ();
	
	auto_preview = gconf_client_get_bool (gconf_client_get_default (), "/apps/gtk-theme-selector/auto", NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (glade_xml_get_widget (xml, "check1")), auto_preview);
			
	glade_xml_signal_autoconnect (xml);
	gtk_main ();
 
	gconf_client_set_bool (gconf_client_get_default (), "/apps/gtk-theme-selector/auto", auto_preview, NULL);
	
	return 0;
}

