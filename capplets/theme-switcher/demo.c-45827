#include <config.h>
#include "da.h"
#include <errno.h>

GtkWidget *plug;

void
send_socket()
{
  gchar buffer[256];

  g_snprintf(buffer, sizeof(buffer), "%11lx ", 
	     (gulong)GDK_WINDOW_XWINDOW (preview_socket->window));
  write(prog_fd, buffer, strlen(buffer));
}

void
send_reread()
{
  gchar buffer[256];

  g_snprintf(buffer, sizeof(buffer), "R ");
  write(prog_fd, buffer, strlen(buffer));
}

static void
demo_data_in(gpointer data, gint source, GdkInputCondition condition)
{
  gchar buf[256];
  
  if (condition & GDK_INPUT_EXCEPTION ||
      read(source, buf, 2) == 0)
    gtk_main_quit();		/* Parent exited */
  else {
    if (gtk_rc_reparse_all ())
      gtk_widget_reset_rc_styles(plug);
  }
}

#define NUM 50

static void
demo_main(int argc, char **argv, gint in_fd)
{
  gchar buf[256];
  Window window;
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
  gchar **new_rc_files;
  gint rc_file_count;
  gint new_count;
  gchar *home_dir;
  gint i;

#ifdef ENABLE_NLS
  for (i=0;i<2;i++) {
	titles[i]=_(titles[i]);
	row1[i]=_(row1[i]);
	row2[i]=_(row2[i]);
	row3[i]=_(row3[i]);
	row4[i]=_(row4[i]);
  }
#endif

  if (read(in_fd, buf, 12) <= 0)
    /* Assume this means that our parent exited or was killed */
    exit(0);
  
  buf[12] = 0;
  window = strtol (buf, NULL, 16);

  fcntl(0, F_SETFL, O_NONBLOCK);

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
  new_rc_files[new_count++] = g_strdup (gtkrc_tmp);
  new_rc_files[new_count] = NULL;

  gtk_rc_set_default_files (new_rc_files);
  g_strfreev (new_rc_files);

  gtk_set_locale();
  gtk_init (&argc, &argv);
  
  plug = gtk_plug_new(window);

  table = gtk_table_new (5, 3, FALSE);
  gtk_container_add(GTK_CONTAINER(plug), table);
  
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
  gtk_widget_show(widget);
  gtk_menu_bar_append(GTK_MENU_BAR(menubar), widget);
  gtk_widget_show(menubar);

  menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget), menu);
  widget = gtk_menu_item_new_with_label(_("Item 1"));
  gtk_widget_show(widget);
  gtk_menu_append(GTK_MENU(menu), widget);
  widget = gtk_menu_item_new_with_label(_("Another item"));
  gtk_widget_show(widget);
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
  
  gdk_input_add_full(in_fd, GDK_INPUT_READ | GDK_INPUT_EXCEPTION, demo_data_in, NULL, NULL);
  gtk_widget_show_all (plug);
  
  gtk_main ();
}

gint
do_demo(int argc, char **argv)
{
  gint toProg[2];
  gint pid;
  
  pipe(toProg);
  
  if (!(pid = fork()))
    {
      close(toProg[1]);
      demo_main(argc, argv, toProg[0]);
      exit(0);
    }
  else if (pid > 0)
    {
      close(toProg[0]);
      prog_fd = toProg[1];
      return pid;
    }
  else
    {
      /* baaaaaaaah eeeeek */
      return -1;
    }
}
