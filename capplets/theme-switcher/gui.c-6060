#include <config.h>
#include "da.h"
#include "capplet-widget.h"
#include <signal.h>

static GtkWidget *w;
GtkWidget *install_theme_file_sel;
extern gint pid;


void
die_callback(GtkWidget *widget, gpointer data)
{
  kill (pid,9);
}
void
auto_callback (GtkWidget *widget, gpointer data)
{
  if (GTK_TOGGLE_BUTTON (auto_preview)->active)
    click_preview (widget,NULL);
  
}
static void
browse_dialog_ok (GtkWidget *widget, gpointer data)
{
  GtkWidget *filesel = gtk_widget_get_toplevel (widget);
  install_theme (gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel)));
  gtk_widget_set_sensitive (GTK_WIDGET (data), TRUE);
  gtk_widget_destroy (filesel);
}
static void
browse_dialog_close (GtkWidget *widget, gpointer data)
{
  gtk_widget_set_sensitive (GTK_WIDGET (data), TRUE);
  gtk_widget_destroy (gtk_widget_get_toplevel (widget));
}
static void
browse_dialog_kill (GtkWidget *widget, gpointer data)
{
  gtk_widget_set_sensitive (GTK_WIDGET (data), TRUE);
}

static void
install_theme_callback (GtkWidget *widget, gpointer data)
{
  GtkWidget *parent;
  gtk_widget_set_sensitive (widget, FALSE);

  install_theme_file_sel = gtk_file_selection_new (_("Select a theme to install"));
  gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (install_theme_file_sel));
  /* BEGIN UGLINESS.  This code is stolen from gnome_dialog_set_parent.
   * We want its functionality, but it takes a GnomeDialog as its argument.
   * So we copy it )-: */
  parent = gtk_widget_get_toplevel (GTK_WIDGET (widget));
  gtk_window_set_transient_for (GTK_WINDOW(install_theme_file_sel), GTK_WINDOW (parent));

  if ( gnome_preferences_get_dialog_centered() ) {
	  /* User wants us to center over parent */

	  gint x, y, w, h, dialog_x, dialog_y;

	  if (GTK_WIDGET_VISIBLE(parent)) {
		  /* Throw out other positioning */
		  gtk_window_set_position(GTK_WINDOW(install_theme_file_sel),
					  GTK_WIN_POS_NONE);

		  gdk_window_get_origin (GTK_WIDGET(parent)->window, &x, &y);
		  gdk_window_get_size   (GTK_WIDGET(parent)->window, &w, &h);

		/* The problem here is we don't know how big the dialog is.
		   So "centered" isn't really true. We'll go with 
		   "kind of more or less on top" */
		  dialog_x = x + w/4;
		  dialog_y = y + h/4;
		
		  gtk_widget_set_uposition(GTK_WIDGET(install_theme_file_sel),
					   dialog_x, dialog_y);
	  }
  }
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (install_theme_file_sel)
				  ->ok_button), "clicked",
		      (GtkSignalFunc) browse_dialog_ok,
		      widget);
  gtk_signal_connect (GTK_OBJECT (GTK_FILE_SELECTION (install_theme_file_sel)->cancel_button),
		      "clicked",
		      GTK_SIGNAL_FUNC(browse_dialog_close),
		      widget);
  gtk_signal_connect (GTK_OBJECT (install_theme_file_sel), "destroy",
		      GTK_SIGNAL_FUNC(browse_dialog_kill),
		      widget);

  if (gtk_grab_get_current ())
	  gtk_grab_add (install_theme_file_sel);

  gtk_widget_show (install_theme_file_sel);

}
GtkWidget *
make_main(void)
{
  GtkWidget *evbox;
  GtkWidget *l2;
  GtkWidget *sw, *label, *socket;
  GtkWidget *box, *vbox, *hbox;
  GtkWidget *text, *frame, *button;
  GtkWidget *hbxo;
  
  w = capplet_widget_new();
  gtk_container_set_border_width(GTK_CONTAINER(w), 5);

  box = gtk_vbox_new(FALSE, GNOME_PAD);
  hbox = gtk_hbox_new(TRUE, GNOME_PAD);
  frame = gtk_frame_new (_("Available Themes"));
  vbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  l2 = gtk_list_new();
  gtk_list_set_selection_mode(GTK_LIST(l2), GTK_SELECTION_SINGLE);
  hbxo = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_container_set_border_width (GTK_CONTAINER (hbxo), GNOME_PAD_SMALL);
  label = gtk_label_new (_("Auto\nPreview"));
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  auto_preview = gtk_check_button_new ();
  
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (auto_preview), gnome_config_get_bool ("/theme-switcher-capplet/settings/auto=TRUE"));
  gtk_signal_connect (GTK_OBJECT (auto_preview), "toggled", GTK_SIGNAL_FUNC (auto_callback), NULL);
  gtk_container_add (GTK_CONTAINER (auto_preview), label);
  gtk_box_pack_start (GTK_BOX (hbxo), auto_preview, FALSE, FALSE, 0);
  button = gtk_button_new_with_label (_("Preview"));
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (click_preview), NULL);
  gtk_box_pack_start (GTK_BOX (hbxo), button, FALSE, FALSE, 0);
  button = gtk_button_new_with_label (_("Install new\ntheme..."));
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (install_theme_callback), NULL);
  
  /* FIXME: this needs ot actually do something. */
  gtk_box_pack_start (GTK_BOX (hbxo), button, FALSE, FALSE, 0);
  
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), l2);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_set_usize (sw, 120, -1);
  
  gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbxo, FALSE, FALSE, 0);

  frame = gtk_frame_new (_("Theme Information"));
  evbox = gtk_viewport_new(NULL, NULL);
  gtk_container_add (GTK_CONTAINER (frame), evbox);
  gtk_container_set_border_width (GTK_CONTAINER (evbox), GNOME_PAD_SMALL);
  //gtk_widget_set_usize(evbox, 150, -1);
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  
  text = gtk_xmhtml_new();
  gtk_container_add(GTK_CONTAINER(evbox), text);

  hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox);
  /* in a gratuituous reuse of variable names... */
  sw = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (sw), GTK_SHADOW_IN); 
  evbox = gtk_event_box_new();
  gtk_box_pack_start(GTK_BOX(hbox), sw, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(sw), evbox);
  socket = gtk_socket_new();
  gtk_container_add(GTK_CONTAINER(evbox), socket);
  update_theme_entries(l2);
  gtk_signal_connect (GTK_OBJECT (w), "help",
		      GTK_SIGNAL_FUNC (click_help), NULL);
  gtk_signal_connect (GTK_OBJECT (w), "try",
		      GTK_SIGNAL_FUNC (click_try), NULL);
  gtk_signal_connect (GTK_OBJECT (w), "ok",
		      GTK_SIGNAL_FUNC (click_ok), NULL);
  gtk_signal_connect (GTK_OBJECT (w), "revert",
		      GTK_SIGNAL_FUNC (click_revert), NULL);
  gtk_signal_connect (GTK_OBJECT (w), "cancel",
		      GTK_SIGNAL_FUNC (click_revert), NULL);
  gtk_container_add (GTK_CONTAINER (w), box);
  gtk_signal_connect (GTK_OBJECT (w), "destroy",
		      (GtkSignalFunc) die_callback, NULL);

  readme_display = text;
  readme_current = NULL;
  current_theme = NULL;
  last_theme = NULL;
  system_list = l2;
  preview_socket = socket;
  
  gtk_widget_realize(socket);
  
  return w;
}

void
click_update(GtkWidget *widget, gpointer data)
{
  update_theme_entries(system_list);
}

void
click_preview(GtkWidget *widget, gpointer data)
{
  gchar *rc;

  if (current_theme == last_theme)
    return;
  last_theme = current_theme;
  if (!current_theme) 
    return;
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(current_theme), "rc");
  test_theme(rc);
  send_reread();
}

void
click_help(GtkWidget *widget, gpointer data)
{
  gchar *tmp;

  tmp = gnome_help_file_find_file ("users-guide", "gccdesktop.html#GCCTHEME");
  if (tmp) {
    gnome_help_goto(0, tmp);
    g_free(tmp);
  }

}
void
click_try(GtkWidget *widget, gpointer data)
{
  gchar *rc;
  gchar *dir, cmd[10240];

  if (current_theme == current_global_theme)
    return;
  widget = current_theme;
  if (!widget) 
    return;
  current_global_theme = current_theme;
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  dir = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "dir");

  /* hack for enlightenment only!!!! */
  /* FIXME: restart what ever windowmanager you have! */
  /*g_snprintf(cmd, sizeof(cmd), "eesh -e \"restart %s/e\"", dir);*/
  /* printf("%s\n", cmd); */
  send_reread();
  use_theme(rc);
  gdk_error_warnings = 0;
  signal_apply_theme(widget);
  gdk_flush();
  /* system(cmd); */
  gdk_error_warnings = 1;
}
void
click_ok(GtkWidget *widget, gpointer data)
{
  click_try (widget, data);
  gnome_config_set_bool ("/theme-switcher-capplet/settings/auto",GTK_TOGGLE_BUTTON (auto_preview)->active);
  gnome_config_set_string ("/theme-switcher-capplet/settings/theme", gtk_object_get_data (GTK_OBJECT (current_theme), "name"));
  gnome_config_sync ();
}
void
click_revert(GtkWidget *widget, gpointer data)
{
  gchar *rc;
  gchar *dir, cmd[10240];

  if ((current_global_theme == initial_theme) || (!current_global_theme))
    return;
  widget = initial_theme;
  if (!widget) 
    return;

  current_global_theme = widget;
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  dir = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "dir");

  /* hack for enlightenment only!!!! */
  /* FIXME: restart what ever windowmanager you have! */
  /*  g_snprintf(cmd, sizeof(cmd), "eesh -e \"restart %s/e\"", dir);*/
  /* printf("%s\n", cmd); */
  send_reread();
  use_theme(rc);
  gdk_error_warnings = 0;
  signal_apply_theme(widget);
  gdk_flush();
  /* system(cmd); */
  gdk_error_warnings = 1;
  gtk_list_select_child (GTK_LIST (system_list), initial_theme);
}
void
click_entry(GtkWidget *widget, gpointer data)
{
  gchar *rc, *name, *readme, *new_readme, buf[1024];
  FILE *f;
  
  
  name = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "name");
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  readme = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "readme");
  
  /* boy is this hackish! */
  if (widget != initial_theme)
    gtk_list_item_deselect (GTK_LIST_ITEM (initial_theme));

  if (initial_theme)
    capplet_widget_state_changed(CAPPLET_WIDGET (w), TRUE);
  else
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

  current_theme = widget;
  if (GTK_TOGGLE_BUTTON (auto_preview)->active)
    click_preview (widget,NULL);
    
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
static gint sort_alpha(const void *a, const void *b)
{
  GtkBin *A, *B;

  A = GTK_BIN (a);
  B = GTK_BIN (b);
  
  return strcmp((char *)GTK_LABEL (A->child)->label, (char *)GTK_LABEL (B->child)->label);
}

void
update_theme_entries(GtkWidget *disp_list)
{
  ThemeEntry *te;
  gint         num;
  GList      *list;
  int         i;
  GtkWidget  *item;
  gchar *d_theme = gnome_config_get_string ("/theme-switcher-capplet/settings/theme=Default");
  
  list = NULL;
  gtk_list_clear_items(GTK_LIST(disp_list), 0, -1);
  te = list_system_themes(&num);
  for (i = 0; i < num; i++)
    {
      item = gtk_list_item_new_with_label(te[i].name);
      gtk_widget_show(item);
      if (strcmp (d_theme, te[i].name) == 0) {
	initial_theme = item;
      }
      gtk_object_set_data(GTK_OBJECT(item), "name", g_strdup(te[i].name));
      gtk_object_set_data(GTK_OBJECT(item), "rc", g_strdup(te[i].rc));
      gtk_object_set_data(GTK_OBJECT(item), "dir", g_strdup(te[i].dir));
      gtk_object_set_data(GTK_OBJECT(item), "readme", g_strdup(te[i].readme));
      gtk_object_set_data(GTK_OBJECT(item), "icon", g_strdup(te[i].icon));
      gtk_signal_connect(GTK_OBJECT(item), "select",
			 GTK_SIGNAL_FUNC(click_entry), NULL);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(delete_entry), NULL);
      list = g_list_insert_sorted(list, item, sort_alpha);
    }

  te = list_user_themes(&num);
  for (i = 0; i < num; i++)
    {
      item = gtk_list_item_new_with_label(te[i].name);
      gtk_widget_show(item);
      if (strcmp (d_theme, te[i].name) == 0) {
	g_print ("woo hoo -- gotta match %s\n",te[i].name);
	initial_theme = item;
      }
      gtk_object_set_data(GTK_OBJECT(item), "name", g_strdup(te[i].name));
      gtk_object_set_data(GTK_OBJECT(item), "rc", g_strdup(te[i].rc));
      gtk_object_set_data(GTK_OBJECT(item), "dir", g_strdup(te[i].dir));
      gtk_object_set_data(GTK_OBJECT(item), "readme", g_strdup(te[i].readme));
      gtk_object_set_data(GTK_OBJECT(item), "icon", g_strdup(te[i].icon));
      gtk_signal_connect(GTK_OBJECT(item), "select",
			 GTK_SIGNAL_FUNC(click_entry), NULL);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(delete_entry), NULL);
      list = g_list_insert_sorted(list, item, sort_alpha);
    }

  gtk_list_select_child (GTK_LIST (system_list), initial_theme);
  gtk_list_append_items(GTK_LIST(disp_list), list);
  
  free_theme_list(te, num);
  g_free (d_theme);
}
