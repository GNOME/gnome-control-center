#include "da.h"
#include "capplet-widget.h"
#include <signal.h>

static GtkWidget *w;
extern gint pid;
void
die_callback(GtkWidget *widget, gpointer data)
{
  kill (pid,9);
}

GtkWidget *
make_main(void)
{
  GtkWidget *evbox;
  GtkWidget *l1, *l2;
  GtkWidget *sw, *label, *socket;
  GtkWidget *box, *vbox, *hbox;
  GtkWidget *text, *frame, *button;
  
  w = capplet_widget_new();
  gtk_container_border_width(GTK_CONTAINER(w), 0);
  
  l1 = gtk_list_new();
  gtk_list_set_selection_mode(GTK_LIST(l1), GTK_SELECTION_SINGLE);
  
  l2 = gtk_list_new();
  gtk_list_set_selection_mode(GTK_LIST(l2), GTK_SELECTION_SINGLE);

  box = gtk_vbox_new(FALSE, 0);

  hbox = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);
  
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  label = gtk_label_new("Installed System Themes");
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(sw), l1);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, 
				 GTK_POLICY_ALWAYS);
  gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

  label = gtk_label_new("Installed User Themes");
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(sw), l2);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, 
				 GTK_POLICY_ALWAYS);
  gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_ALWAYS, 
				 GTK_POLICY_ALWAYS);
  gtk_widget_set_usize(sw, -1, 160);
  gtk_box_pack_start(GTK_BOX(box), sw, FALSE, FALSE, 0);

  evbox = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(sw), evbox);
  
  socket = gtk_socket_new();
  gtk_container_add(GTK_CONTAINER(evbox), socket);
/*  gtk_widget_set_usize(socket, 640, 480);*/
  
  hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
  
  frame = gtk_frame_new("Theme Information");
  gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, FALSE, 0);
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_set_usize(vbox, 128, 128);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  icon_display = vbox;
  
  evbox = gtk_viewport_new(NULL, NULL);
  gtk_widget_set_usize(evbox, 256, -1);
  gtk_box_pack_start(GTK_BOX(hbox), evbox, TRUE, TRUE, 0);
  
  text = gtk_xmhtml_new();
  gtk_container_add(GTK_CONTAINER(evbox), text);
/*  gtk_box_pack_start(GTK_BOX(hbox), text, TRUE, TRUE, 0);*/
  
  update_theme_entries(l1, l2);
  gtk_signal_connect (GTK_OBJECT (w), "try",
		      GTK_SIGNAL_FUNC (click_preview), NULL);
  gtk_signal_connect (GTK_OBJECT (w), "ok",
		      GTK_SIGNAL_FUNC (click_apply), NULL);
  gtk_container_add (GTK_CONTAINER (w), box);
  gtk_signal_connect (GTK_OBJECT (w), "destroy",
		      (GtkSignalFunc) die_callback, NULL);

  readme_display = text;
  readme_current = NULL;
  icon_current = NULL;
  current_theme = NULL;
  system_list = l1;
  user_list = l2;
  preview_socket = socket;
  
  gtk_widget_realize(socket);
  
  return w;
}

void
click_update(GtkWidget *widget, gpointer data)
{
  update_theme_entries(system_list, user_list);
}

void
click_preview(GtkWidget *widget, gpointer data)
{
  gchar *rc;

  widget = current_theme;
  if (!widget) 
    return;
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  test_theme(rc);
  send_reread();
}

void
click_apply(GtkWidget *widget, gpointer data)
{
  gchar *rc;
  gchar *dir, cmd[10240];

  widget = current_theme;
  if (!widget) 
    return;
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  dir = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "dir");

  /* hack for enlightenment only!!!! */
  /* FIXME: restart what ever windowmanager you have! */
  g_snprintf(cmd, sizeof(cmd), "eesh -e \"restart %s/e\"", dir);
  printf("%s\n", cmd);
  send_reread();
  use_theme(rc);
  gdk_error_warnings = 0;
  signal_apply_theme(widget);
  gdk_flush();
  system(cmd);
  gdk_error_warnings = 1;
}

void
click_entry(GtkWidget *widget, gpointer data)
{
  gchar *rc, *name, *readme, *icon, *new_readme, buf[1024];
  FILE *f;
  
  
  name = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "name");
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  readme = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "readme");
  icon = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "icon");
  
  capplet_widget_state_changed(CAPPLET_WIDGET (w), FALSE);

  if (readme_current)
    {
      g_free(readme_current);
      readme_current = NULL;
    }
  f = fopen(readme, "r");
  if (f)
    {
      new_readme = NULL;
      while (fgets(buf, 1024, f))
	{
	  if (new_readme)
	    new_readme = g_realloc(new_readme, strlen(buf) + strlen(new_readme) + 1);
	  else
	    {
	      new_readme = g_malloc(strlen(buf) + 1);
	      new_readme[0] = 0;
	    }
	  strcat(new_readme, buf);
	}
      fclose(f);
      if ((new_readme) && (strlen(new_readme) > 0))
	{
	  gtk_xmhtml_source(GTK_XMHTML(readme_display), new_readme);
	}
      readme_current = new_readme;
    }
  if (icon_current)
    gtk_widget_destroy(icon_current);
  icon_current = NULL;
  if (isfile(icon))
    {
      icon_current = gnome_pixmap_new_from_file(icon);
      if (icon_current)
	{
	  gtk_container_add(GTK_CONTAINER(icon_display), icon_current);
	  gtk_widget_show(icon_current);
	}
    }
  current_theme = widget;
}

void
delete_entry(GtkWidget *widget, gpointer data)
{
  gchar *rc, *name, *readme, *icon;

  name = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "name");
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  readme = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "readme");
  icon = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "icon");
  g_free(name);
  g_free(rc);
  g_free(readme);
  g_free(icon);
  
  if (current_theme == widget)
    current_theme = NULL;
}

void
update_theme_entries(GtkWidget *system_list, GtkWidget *user_list)
{
  ThemeEntry *te;
  gint         num;
  GList      *list;
  int         i;
  GtkWidget  *item;
  
  list = NULL;
  gtk_list_clear_items(GTK_LIST(system_list), 0, -1);
  te = list_system_themes(&num);
  for (i = 0; i < num; i++)
    {
      item = gtk_list_item_new_with_label(te[i].name);
      gtk_widget_show(item);
      gtk_object_set_data(GTK_OBJECT(item), "name", g_strdup(te[i].name));
      gtk_object_set_data(GTK_OBJECT(item), "rc", g_strdup(te[i].rc));
      gtk_object_set_data(GTK_OBJECT(item), "dir", g_strdup(te[i].dir));
      gtk_object_set_data(GTK_OBJECT(item), "readme", g_strdup(te[i].readme));
      gtk_object_set_data(GTK_OBJECT(item), "icon", g_strdup(te[i].icon));
      gtk_signal_connect(GTK_OBJECT(item), "select",
			 GTK_SIGNAL_FUNC(click_entry), NULL);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(delete_entry), NULL);
      list = g_list_append(list, item);
    }
  gtk_list_append_items(GTK_LIST(system_list), list);

  list = NULL;
  gtk_list_clear_items(GTK_LIST(user_list), 0, -1);
  te = list_user_themes(&num);
  for (i = 0; i < num; i++)
    {
      item = gtk_list_item_new_with_label(te[i].name);
      gtk_widget_show(item);
      gtk_object_set_data(GTK_OBJECT(item), "name", g_strdup(te[i].name));
      gtk_object_set_data(GTK_OBJECT(item), "rc", g_strdup(te[i].rc));
      gtk_object_set_data(GTK_OBJECT(item), "dir", g_strdup(te[i].dir));
      gtk_object_set_data(GTK_OBJECT(item), "readme", g_strdup(te[i].readme));
      gtk_object_set_data(GTK_OBJECT(item), "icon", g_strdup(te[i].icon));
      gtk_signal_connect(GTK_OBJECT(item), "select",
			 GTK_SIGNAL_FUNC(click_entry), NULL);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(delete_entry), NULL);
      list = g_list_append(list, item);
    }
  gtk_list_append_items(GTK_LIST(user_list), list);
  
  free_theme_list(te, num);
}
