#include "da.h"

GtkWidget *plug;

gint pid;
void
send_socket()
{
  gchar buffer[256];
  
  g_snprintf(buffer, sizeof(buffer), "%11x ", 
	     GDK_WINDOW_XWINDOW (preview_socket->window));
  write(prog_fd, buffer, strlen(buffer));
}

void
send_reread()
{
  gchar buffer[256];
  
  g_snprintf(buffer, sizeof(buffer), "R ");
  write(prog_fd, buffer, strlen(buffer));
  fsync(prog_fd);
}

void
demo_data_in(gpointer data, gint source, GdkInputCondition condition)
{
  gchar buf[256];
  
  read(0, buf, 2);
  if (gtk_rc_reparse_all ())
    gtk_widget_reset_rc_styles(plug);
}

#define NUM 50

void demo_main(int argc, char **argv)
{
  gchar buf[256];
  XID window;
  GtkWidget *widget, *table, *hbox;
  GSList *group;
  gchar *titles[2] = {"One","Two"};
  gchar *row1[2] = {"Eenie", "Meenie"};
  gchar *row2[2] = {"Mynie", "Moe"};
  gchar *row3[2] = {"Catcha", "Tiger"};
  gchar *row4[2] = {"By Its", "Toe"};

  read(0, buf, 12);
  buf[12] = 0;
  sscanf(buf, "%x", &window);

  fcntl(0, F_SETFL, O_NONBLOCK);
  
  gtk_init (&argc, &argv);
  gtk_rc_parse(gtkrc_tmp);  
  
  plug = gtk_plug_new(window);

  table = gtk_table_new (4, 3, FALSE);
  gtk_container_add(GTK_CONTAINER(plug), table);
  
  widget = gtk_label_new ("Selected themes from above will be tested by previewing here.");
  gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);

  /* column one */
  gtk_table_attach (GTK_TABLE (table), widget, 0, 3, 0, 1, 0, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
  widget = gtk_button_new_with_label ("Sample Button");
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);
  widget = gtk_check_button_new_with_label ("Sample Check Button");
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, 0);
  widget = gtk_entry_new_with_max_length (50);
  gtk_entry_set_text (GTK_ENTRY (widget), "Sample Text Entry Field");
  gtk_widget_set_usize (widget, 70, -1);
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, TRUE, TRUE, 0);
  gtk_table_attach (GTK_TABLE (table), hbox, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, GNOME_PAD_SMALL);

  /* column two */
  widget = gtk_radio_button_new_with_label (NULL, "Radio Button 1");
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, GNOME_PAD_SMALL);

  widget = gtk_radio_button_new_with_label (group, "Radio Button 2");
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  widget = gtk_radio_button_new_with_label (group, "Radio Button 3");
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
  group = gtk_radio_button_group (GTK_RADIO_BUTTON (widget));
  gtk_table_attach (GTK_TABLE (table), hbox, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 0, 0);

  /* column three */
  widget = gtk_clist_new_with_titles (2, titles);
  //  gtk_clist_set_border(GTK_CLIST(widget), GTK_SHADOW_OUT);
  gtk_clist_set_policy(GTK_CLIST(widget), GTK_POLICY_ALWAYS, GTK_POLICY_AUTOMATIC);
  gtk_clist_set_column_width (GTK_CLIST(widget), 0, 45);
  gtk_clist_set_column_width (GTK_CLIST(widget), 1, 45);
  gtk_clist_append (GTK_CLIST(widget), row1);
  gtk_clist_append (GTK_CLIST(widget), row2);
  gtk_clist_append (GTK_CLIST(widget), row3);
  gtk_clist_append (GTK_CLIST(widget), row4);
  gtk_widget_set_usize (widget, 160, -1);
  gtk_table_attach (GTK_TABLE (table), widget, 2, 3, 1, 4, GTK_EXPAND | GTK_FILL, 0, GNOME_PAD_SMALL, 0);

  gdk_input_add_full(0, GDK_INPUT_READ, demo_data_in, NULL, NULL);
  gtk_widget_show_all (plug);
  
  gtk_main ();
}

void
do_demo(int argc, char **argv)
{
  gint toProg[2];
  
  pipe(toProg);
  
  if (!(pid = fork()))
    {
      close(toProg[1]);
      dup2(toProg[0], 0);   /* Make stdin the in pipe */
      close(toProg[0]);
      demo_main(argc, argv);
    }
  else if (pid > 0)
    {
      close(toProg[0]);
      prog_fd = toProg[1];
    }
  else
    {
      /* baaaaaaaah eeeeek */
    }
  
}
