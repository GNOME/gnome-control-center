#include <config.h>
#include <libbonoboui.h>

static gchar* current_theme = NULL;
static gchar **new_rc_files = NULL;
static gint new_count = 0;

#define GNOME_PAD_SMALL 4

static GtkWidget*
create_form (void)
{
  GtkWidget *widget, *table, *hbox;
  GtkWidget *scrolled_window, *menubar, *menu;
  GSList *group;
  gchar *titles[2] = {N_("One"),N_("Two")};
  /* just 8 short names that will serve as samples for titles in demo */	
  gchar *row1[2] = {N_("Eenie"), N_("Meenie")};
  gchar *row2[2] = {N_("Mynie"), N_("Moe")};
  gchar *row3[2] = {N_("Catcha"), N_("Tiger")};
  gchar *row4[2] = {N_("By Its"), N_("Toe")};
  gchar **rc_files;
  gint rc_file_count;
  gchar *home_dir;
  gint i;

  for (i=0;i<2;i++) {
	titles[i]=_(titles[i]);
	row1[i]=_(row1[i]);
	row2[i]=_(row2[i]);
	row3[i]=_(row3[i]);
	row4[i]=_(row4[i]);
  }

  /* Strip out ~/.gtkrc from the set of initial default files.
   * to suppress reading of the previous rc file.
   */

  rc_files = gtk_rc_get_default_files();
  for (rc_file_count = 0; rc_files[rc_file_count]; rc_file_count++)
    /* Nothing */;

  new_rc_files = g_new (gchar *, rc_file_count + 2);

  home_dir = g_get_home_dir();
  new_count = 0;
  
  for (i = 0; i<rc_file_count; i++)
    {
      if (strncmp (rc_files[i], home_dir, strlen (home_dir)) != 0)
	new_rc_files[new_count++] = g_strdup (rc_files[i]);
    }
  new_rc_files[new_count++] = NULL;
  new_rc_files[new_count] = NULL;

  gtk_rc_set_default_files (new_rc_files);

  table = gtk_table_new (5, 3, FALSE);
  
  widget = gtk_label_new (_("Selected themes from above will be tested by previewing here."));
  gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
  gtk_table_attach (GTK_TABLE (table), widget, 0, 3, 0, 1, 0, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

  /* column one */
  widget = gtk_button_new_with_label (_("Sample Button"));
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
  widget = gtk_check_button_new_with_label (_("Sample Check Button"));
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, 0);
  widget = gtk_entry_new_with_max_length (50);
  gtk_entry_set_text (GTK_ENTRY (widget), _("Sample Text Entry Field"));
  gtk_widget_set_usize (widget, 70, -1);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

  /* column two */

  menubar = gtk_menu_bar_new();
  gtk_table_attach (GTK_TABLE (table), menubar, 1, 2, 2, 3, 0, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

  widget = gtk_menu_item_new_with_label(_("Submenu"));
  gtk_menu_bar_append(GTK_MENU_BAR(menubar), widget);

  menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget), menu);
  widget = gtk_menu_item_new_with_label(_("Item 1"));
  gtk_menu_append(GTK_MENU(menu), widget);
  widget = gtk_menu_item_new_with_label(_("Another item"));
  gtk_menu_append(GTK_MENU(menu), widget);


  widget = gtk_radio_button_new_with_label (NULL, _("Radio Button 1"));
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  widget = gtk_radio_button_new_with_label (group, _("Radio Button 2"));
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  /* column three */
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolled_window), 
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);

  gtk_table_attach (GTK_TABLE (table), scrolled_window, 2, 3, 2, 5, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, 0);

  widget = gtk_clist_new_with_titles (2, titles);
  gtk_clist_set_column_width (GTK_CLIST(widget), 0, 45);
  gtk_clist_set_column_width (GTK_CLIST(widget), 1, 45);
  gtk_clist_append (GTK_CLIST(widget), row1);
  gtk_clist_append (GTK_CLIST(widget), row2);
  gtk_clist_append (GTK_CLIST(widget), row3);
  gtk_clist_append (GTK_CLIST(widget), row4);
  gtk_widget_set_usize (widget, 160, -1);

  gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

  return table;
} 

static void
get_prop_cb (BonoboPropertyBag *bag, BonoboArg *arg, guint arg_id,
	     CORBA_Environment *ev, gpointer data)
{
	BONOBO_ARG_SET_STRING (arg, current_theme);
}

static void
set_prop_cb (BonoboPropertyBag *bag, const BonoboArg *arg, guint arg_id,
	     CORBA_Environment *ev, GtkWidget *form)
{
	if (current_theme)
		g_free (current_theme);
	current_theme = g_strdup (BONOBO_ARG_GET_STRING (arg));
	g_print ("Set to: %s\n", current_theme);
	g_free (new_rc_files[new_count - 1]);
	new_rc_files[new_count - 1] = g_strdup (current_theme);
	gtk_rc_set_default_files (new_rc_files);
	gtk_rc_reparse_all_for_settings (gtk_settings_get_default (), TRUE);
	gtk_widget_reset_rc_styles (form);
}

BonoboObject *
gnome_theme_preview_new (void)
{
	BonoboPropertyBag *pb;
	BonoboControl *control;
	GtkWidget *form;

	form = create_form ();
	gtk_widget_show_all (form);

	control = bonobo_control_new (form);
	pb = bonobo_property_bag_new (get_prop_cb, set_prop_cb, form);
	bonobo_property_bag_add (pb, "theme", 0, BONOBO_ARG_STRING, NULL,
				 "The currently previewed theme",
				 BONOBO_PROPERTY_READABLE |
				 BONOBO_PROPERTY_WRITEABLE);
	bonobo_control_set_properties (control, BONOBO_OBJREF (pb), NULL);
	bonobo_object_unref (BONOBO_OBJECT (pb));

	return BONOBO_OBJECT (control);
}

static BonoboObject *
control_factory (BonoboGenericFactory *factory, const char *id, gpointer data)
{
	BonoboObject *object = NULL;

	g_return_val_if_fail (id != NULL, NULL);

	if (!strcmp (id, "OAFIID:GNOME_Theme_Preview"))
		object = gnome_theme_preview_new ();

	return object;
}

BONOBO_ACTIVATION_FACTORY ("OAFIID:GNOME_Theme_PreviewFactory",
			   "gnome-theme-preview", VERSION,
			   control_factory, NULL);

