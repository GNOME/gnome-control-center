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
  GtkWidget *w[NUM], *hb, *vb[5], *widg, *ww, *www, *l;
  int j, i;
  
  read(0, buf, 12);
  buf[12] = 0;
  sscanf(buf, "%x", &window);

  fcntl(0, F_SETFL, O_NONBLOCK);
  
  gtk_init (&argc, &argv);
  gtk_rc_parse(gtkrc_tmp);  
  
  plug = gtk_plug_new(window);
  gtk_widget_show(plug);

  hb = gtk_hbox_new(TRUE, 4);
  gtk_container_add(GTK_CONTAINER(plug), hb);
  gtk_widget_show(hb);
  
  for(i=0; i<5;i++)
    {
      vb[i] = gtk_vbox_new(TRUE, 4);
      gtk_box_pack_start(GTK_BOX(hb), vb[i], TRUE, TRUE, 0);
      gtk_widget_show(vb[i]);
    }
  
  j= 0;
  w[j++] = widg = 
    gtk_button_new_with_label("A Button");
  gtk_box_pack_start(GTK_BOX(vb[0]), widg , TRUE, TRUE, 0);
  w[j++] = widg = 
    gtk_check_button_new_with_label("A Check Button");
  gtk_box_pack_start(GTK_BOX(vb[0]), widg , TRUE, TRUE, 0);
  w[j++] = widg = 
    gtk_radio_button_new_with_label(NULL, "A Radio Button");
  gtk_box_pack_start(GTK_BOX(vb[0]), widg , TRUE, TRUE, 0);
  ww = gtk_menu_new();
  gtk_widget_show(ww);
  www = gtk_menu_item_new_with_label("Menu 1");
  gtk_widget_show(www);
  gtk_menu_append(GTK_MENU(ww), www);
  www = gtk_menu_item_new_with_label("Menu 2");
  gtk_widget_show(www);
  gtk_menu_append(GTK_MENU(ww), www);
  www = gtk_menu_item_new_with_label("Menu 3");
  gtk_widget_show(www);
  gtk_menu_append(GTK_MENU(ww), www);
  www = gtk_menu_item_new_with_label("Menu 4");
  gtk_widget_show(www);
  gtk_menu_append(GTK_MENU(ww), www);
  www = gtk_menu_item_new_with_label("Menu 5");
  gtk_widget_show(www);
  gtk_menu_append(GTK_MENU(ww), www);
  w[j++] = widg = 
    gtk_option_menu_new();
  gtk_box_pack_start(GTK_BOX(vb[0]), widg , TRUE, TRUE, 0);
  gtk_option_menu_set_menu(GTK_OPTION_MENU(widg), ww);
  w[j++] = widg = 
    gtk_gamma_curve_new();
  gtk_box_pack_start(GTK_BOX(vb[1]), widg , TRUE, TRUE, 0);
  
  for (i=0; i< j; i++)
    gtk_widget_show(w[i]);

  gdk_input_add_full(0, GDK_INPUT_READ, demo_data_in, NULL, NULL);
  
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
