#include <config.h>
#include "da.h"
#include "capplet-widget.h"
#include <libgnomeui/gnome-window-icon.h>
#include <signal.h>

static gboolean   ignore_change = FALSE;

static GtkWidget *install_theme_file_sel;

static GtkWidget *capplet_widget;
static GtkWidget *theme_list;
static GtkWidget *auto_preview;

static GtkWidget *current_theme = NULL;
static GtkWidget *current_global_theme = NULL;
static GtkWidget *initial_theme = NULL;
static GtkWidget *last_theme = NULL;
static GtkWidget *font_sel;
static GtkWidget *font_cbox;
static gboolean initial_preview;
static GtkWidget *fatal_error_dialog;

/* If this is TRUE, then we use the custom font */
static gboolean initial_font_cbox;
static gchar *initial_font;
static void
click_preview(GtkWidget *widget, gpointer data);
static void
click_try(GtkWidget *widget, gpointer data);
static void
click_help(GtkWidget *widget, gpointer data);
static void
click_ok(GtkWidget *widget, gpointer data);
static void
click_revert(GtkWidget *widget, gpointer data);
static void
click_cancel(GtkWidget *widget, gpointer data);
static void
click_entry(GtkWidget *widget, gpointer data);
static void
delete_entry(GtkWidget *widget, gpointer data);

static void
auto_callback (GtkWidget *widget, gpointer data)
{
  if (ignore_change == FALSE) {
	  if (GTK_TOGGLE_BUTTON (auto_preview)->active)
		  click_preview (widget,NULL);
	  capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), TRUE);
  }
  
}
static void
font_callback (GtkWidget *widget, gchar *font, gpointer data)
{
  if (ignore_change == FALSE) {
	capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), TRUE);
	if (GTK_TOGGLE_BUTTON (auto_preview)->active)
	  click_preview (widget,NULL);
  }	
}
static void
use_theme_font_callback (GtkWidget *widget, gpointer data)
{
  if (ignore_change == FALSE) {
	capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), TRUE);
	if (GTK_TOGGLE_BUTTON (auto_preview)->active)
		click_preview (widget,NULL);
	if (!GTK_TOGGLE_BUTTON (font_cbox)->active)
	  gtk_widget_set_sensitive (font_sel, FALSE);
	else
	  gtk_widget_set_sensitive (font_sel, TRUE);
  }
}
static void
browse_dialog_ok (GtkWidget *widget, gpointer data)
{
  GtkWidget *filesel = gtk_widget_get_toplevel (widget);
  gchar *filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));
  gchar *error;

  error = install_theme (filename);
  
  if (!error)
    update_theme_entries (theme_list);
  else
    {
      char *msg = g_strdup_printf (_("Error installing theme:\n'%s'\n%s"),
				   filename, error);
      GtkWidget *msgbox = gnome_message_box_new (msg,
						 GNOME_MESSAGE_BOX_ERROR,
						 GNOME_STOCK_BUTTON_OK,
						 NULL);
      gnome_dialog_run (GNOME_DIALOG (msgbox));
      g_free (msg);
      g_free (error);
    }
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
  gnome_window_icon_set_from_default (GTK_WINDOW (install_theme_file_sel));
  gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (install_theme_file_sel));
  /* BEGIN UGLINESS.  This code is stolen from gnome_dialog_set_parent.
   * We want its functionality, but it takes a GnomeDialog as its argument.
   * So we copy it )-: */

  parent = gtk_widget_get_toplevel (GTK_WIDGET (widget));

  /* We'd like to set a transient_for hint here, but it isn't
   * worth the bother, since our parent window isn't in this process
   */
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

static gint
delete_capplet (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  /* We don't want the toplevel window destroyed until
   * our child exits.
   */
  close(prog_fd);
  return FALSE;
}

GtkWidget *
make_main(void)
{
  void *sw, *label;
  GtkWidget *box, *hbox, *hbox2, *vbox;
  GtkWidget *frame, *button;
  GtkWidget *button_vbox;
  gboolean default_used;
  
  capplet_widget = capplet_widget_new();
  gtk_container_set_border_width(GTK_CONTAINER(capplet_widget), 5);

  box = gtk_vbox_new(FALSE, GNOME_PAD);
  frame = gtk_frame_new (_("Available Themes"));
  hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), GNOME_PAD_SMALL);
  gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, GNOME_PAD_SMALL);
  gtk_container_add (GTK_CONTAINER (frame), hbox2);

  /* List of available themes
   */
  theme_list = gtk_list_new();
  gtk_list_set_selection_mode(GTK_LIST(theme_list), GTK_SELECTION_BROWSE);
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), theme_list);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC,
				 GTK_POLICY_AUTOMATIC);
  gtk_widget_set_usize (sw, 120, -1);

  gtk_box_pack_start(GTK_BOX(hbox2), sw, TRUE, TRUE, 0);

  /* Buttons to preview, and install themes
   */
  button_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_container_set_border_width (GTK_CONTAINER (button_vbox), GNOME_PAD_SMALL);
  gtk_box_pack_start(GTK_BOX(hbox2), button_vbox, FALSE, FALSE, 0);

  label = gtk_label_new (_("Auto\nPreview"));
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  auto_preview = gtk_check_button_new ();
  initial_preview = gnome_config_get_bool ("/theme-switcher-capplet/settings/auto=TRUE");
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (auto_preview),
			       initial_preview);
  gtk_signal_connect (GTK_OBJECT (auto_preview), "toggled", GTK_SIGNAL_FUNC (auto_callback), NULL);
  gtk_container_add (GTK_CONTAINER (auto_preview), label);
  gtk_box_pack_start (GTK_BOX (button_vbox), auto_preview, FALSE, FALSE, 0);
  button = gtk_button_new_with_label (_("Preview"));
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (click_preview), NULL);
  gtk_box_pack_start (GTK_BOX (button_vbox), button, FALSE, FALSE, 0);
  button = gtk_button_new_with_label (_("Install new\ntheme..."));
  gtk_signal_connect (GTK_OBJECT (button), "clicked", GTK_SIGNAL_FUNC (install_theme_callback), NULL);
  gtk_box_pack_start (GTK_BOX (button_vbox), button, FALSE, FALSE, 0);

  /* Font selector.
   */
  frame = gtk_frame_new (_("User Font"));
  gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);
  font_sel = gnome_font_picker_new ();
  gnome_font_picker_set_mode (GNOME_FONT_PICKER (font_sel),
			      GNOME_FONT_PICKER_MODE_FONT_INFO);
  initial_font = gnome_config_get_string_with_default ("/theme-switcher-capplet/settings/font",&default_used);


  if (initial_font == NULL) {

    GtkStyle *style;
    
    gtk_widget_ensure_style (frame);
    style = gtk_widget_get_style (frame);

    if (style->rc_style == NULL) {
      /* FIXME - should really get this from X somehow   */
      /*         for now we just assume default gtk font */
      initial_font = g_strdup(_("-adobe-helvetica-medium-r-normal--*-120-*-*-*-*-*-*"));
    } else {
      initial_font = style->rc_style->font_name;
    }
  }

  gnome_font_picker_set_font_name (GNOME_FONT_PICKER (font_sel), initial_font);

  gnome_font_picker_fi_set_use_font_in_label (GNOME_FONT_PICKER (font_sel),
					      TRUE,
					      12);
  gnome_font_picker_fi_set_show_size (GNOME_FONT_PICKER (font_sel), FALSE);
  gtk_signal_connect (GTK_OBJECT (font_sel),
		      "font_set",
		      font_callback,
		      NULL);
  vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
  gtk_container_add (GTK_CONTAINER (frame), vbox);
  font_cbox = gtk_check_button_new_with_label (_("Use custom font."));
  initial_font_cbox = gnome_config_get_bool ("/theme-switcher-capplet/settings/use_theme_font=FALSE");
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (font_cbox),
			       initial_font_cbox);
  gtk_signal_connect (GTK_OBJECT (font_cbox),
		      "toggled",
		      GTK_SIGNAL_FUNC (use_theme_font_callback),
		      NULL);
  gtk_box_pack_start (GTK_BOX (vbox), font_cbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), font_sel, FALSE, FALSE, 0);
  if (!GTK_TOGGLE_BUTTON (font_cbox)->active)
    gtk_widget_set_sensitive (font_sel, FALSE);
  else
    gtk_widget_set_sensitive (font_sel, TRUE);

  gtk_widget_show_all (vbox);
  
  
#if 0  
  readme_display = gtk_xmhtml_new();
  gtk_container_add(GTK_CONTAINER(frame2), readme_display) ;
#endif
  /* Preview of theme
   */
  hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
  frame = gtk_frame_new (_("Preview"));
  gtk_box_pack_start(GTK_BOX(box), frame, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN); 
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  preview_socket = gtk_socket_new();
  gtk_container_add(GTK_CONTAINER(frame), preview_socket);
  update_theme_entries(theme_list);

  gtk_signal_connect (GTK_OBJECT (capplet_widget), "help",
		      GTK_SIGNAL_FUNC (click_help), NULL);
  gtk_signal_connect (GTK_OBJECT (capplet_widget), "try",
		      GTK_SIGNAL_FUNC (click_try), NULL);
  gtk_signal_connect (GTK_OBJECT (capplet_widget), "ok",
		      GTK_SIGNAL_FUNC (click_ok), NULL);
  gtk_signal_connect (GTK_OBJECT (capplet_widget), "revert",
		      GTK_SIGNAL_FUNC (click_revert), NULL);
  gtk_signal_connect (GTK_OBJECT (capplet_widget), "cancel",
		      GTK_SIGNAL_FUNC (click_cancel), NULL);
  gtk_signal_connect (GTK_OBJECT (capplet_widget), "delete_event",
		      GTK_SIGNAL_FUNC (delete_capplet), NULL);
  gtk_container_add (GTK_CONTAINER (capplet_widget), box);

  last_theme = NULL;
  
  return capplet_widget;
}

static void
click_preview(GtkWidget *widget, gpointer data)
{
  gchar *rc;

/*  if (current_theme == last_theme)
    return;*/
  last_theme = current_theme;
  if (!current_theme) {
    return;
  }
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(current_theme), "rc");
  if (GTK_TOGGLE_BUTTON (font_cbox)->active)
    test_theme(rc,
	      gnome_font_picker_get_font_name (GNOME_FONT_PICKER (font_sel)));
  else
    {
      test_theme(rc, NULL);
    }

  send_reread();
}

static void
click_help(GtkWidget *widget, gpointer data)
{
    GnomeHelpMenuEntry help_entry= {"control-center",
    "desktop-intro.html#GCCTHEME"};
    gnome_help_display (NULL, &help_entry);
}
static void
click_try(GtkWidget *widget, gpointer data)
{
  gchar *rc;
  gchar *dir;
  gboolean result;

  if (fatal_error_dialog)
    return;
  
/*  if (current_theme == current_global_theme)
    return;*/
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
  if (GTK_TOGGLE_BUTTON (font_cbox)->active)
    result = use_theme(rc,
		       gnome_font_picker_get_font_name (GNOME_FONT_PICKER (font_sel)));
  else
    result = use_theme(rc, NULL);

  if (result)
    {
      gdk_error_trap_push ();
      signal_apply_theme(widget);
      gdk_flush();
      /* system(cmd); */
      gdk_error_trap_pop ();
    }
  else
    {
      /* Re-sensitize try/revert buttons */
      capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), TRUE);
    }
    
}
static void
click_ok(GtkWidget *widget, gpointer data)
{
  if (fatal_error_dialog)
    {
      gnome_dialog_close (GNOME_DIALOG (fatal_error_dialog));
      return;
    }
  
  click_try (widget, data);
  gnome_config_set_bool ("/theme-switcher-capplet/settings/auto",GTK_TOGGLE_BUTTON (auto_preview)->active);
  gnome_config_set_string ("/theme-switcher-capplet/settings/theme", gtk_object_get_data (GTK_OBJECT (current_theme), "name"));
  gnome_config_set_bool ("/theme-switcher-capplet/settings/use_theme_font",
			 GTK_TOGGLE_BUTTON (font_cbox)->active);
  gnome_config_set_string ("/theme-switcher-capplet/settings/font",
			   gnome_font_picker_get_font_name (GNOME_FONT_PICKER (font_sel)));
  gnome_config_sync ();
}
static void
click_revert(GtkWidget *widget, gpointer data)
{
  gchar *rc;

  if (fatal_error_dialog)
    return;
  
  widget = initial_theme;
  if (!widget)
    /* we hope this doesn't happen, but it could if things
     * are mis-installed -jrb */
    /* Damn, I hate this code... )-: */
    return;
  
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
    
  if ((current_global_theme != initial_theme) ||
	(initial_font_cbox != GTK_TOGGLE_BUTTON (font_cbox)->active) ||
	(GTK_TOGGLE_BUTTON (font_cbox)->active && strcmp (initial_font,
		 gnome_font_picker_get_font_name (GNOME_FONT_PICKER (font_sel)))))
    {
      
      /* The above statement is magic to determine if we want to reset the system theme.
       * It can almost certainly be cleaned up if needed.  Basicly, it sees if anything has
       * or if the theme has been set.. */
      send_reread();
      if (use_theme(rc, initial_font))
	{
	  gdk_error_trap_push();
	  signal_apply_theme(widget);
	  gdk_flush();
	  gdk_error_trap_pop();
	}
      else
	{
	  /* Re-sensitize try/revert buttons */
	  capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), TRUE);
	}
    }
  current_global_theme = widget;
  ignore_change = TRUE;
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (auto_preview),
			       initial_preview);
  
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (font_cbox),
			       initial_font_cbox);
  if (initial_font)
    gnome_font_picker_set_font_name (GNOME_FONT_PICKER (font_sel),
				     initial_font);
  gtk_list_select_child (GTK_LIST (theme_list), initial_theme);
  test_theme(rc, initial_font);
  send_reread();
  if (!GTK_TOGGLE_BUTTON (font_cbox)->active)
    gtk_widget_set_sensitive (font_sel, FALSE);
  else
    gtk_widget_set_sensitive (font_sel, TRUE);
  ignore_change = FALSE;
  current_theme = initial_theme;
}

static void
click_cancel(GtkWidget *widget, gpointer data)
{
  if (fatal_error_dialog)
    {
      gnome_dialog_close (GNOME_DIALOG (fatal_error_dialog));
      return;
    }
  
  click_revert (widget, data);
}
  

static void
click_entry(GtkWidget *widget, gpointer data)
{
  gchar *rc, *name;
  
  name = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "name");
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");

  /* Load in the README file */
#if 0
  if (readme_current)
    {
      g_free(readme_current);
      readme_current = NULL;
    }
  f = fopen(readme, "r");
  if (f)
    {
      GString *new_readme = g_string_new (NULL);
      
      while (fgets(buf, 1024, f))
	g_string_append (new_readme, buf);

      fclose(f);

      gtk_xmhtml_source(GTK_XMHTML(readme_display), new_readme->str);
      g_string_free (new_readme, TRUE);
    }
  else
    gtk_xmhtml_source(GTK_XMHTML(readme_display), "");
#endif
  if (!ignore_change)
    {
      current_theme = widget;

      if (initial_theme)
	capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), TRUE);
      else
	capplet_widget_state_changed(CAPPLET_WIDGET (capplet_widget), FALSE);
      
      if (GTK_TOGGLE_BUTTON (auto_preview)->active)
	click_preview (widget,NULL);
    }
}

static void
delete_entry(GtkWidget *widget, gpointer data)
{
  gchar *rc, *name, *icon, *dir;

  name = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "name");
  rc = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "rc");
  dir = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "dir");
  icon = (gchar *)gtk_object_get_data(GTK_OBJECT(widget), "icon");
  g_free(name);
  g_free(rc);
  g_free(dir);
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
  gchar *current_name = NULL;

  if (current_theme)
    current_name = g_strdup(gtk_object_get_data(GTK_OBJECT(current_theme), "name"));
  else
    current_name = d_theme;

  current_theme = NULL;
  initial_theme = NULL;
  
  list = NULL;
  gtk_list_clear_items(GTK_LIST(disp_list), 0, -1);
  te = list_system_themes(&num);
  for (i = 0; i < num; i++)
    {
      item = gtk_list_item_new_with_label(te[i].name);
      gtk_widget_show(item);
      if (strcmp (d_theme, te[i].name) == 0)
	{
	  current_global_theme = item;
	  initial_theme = item;
	}
      if (current_name && (strcmp (current_name, te[i].name) == 0))
	{
	  current_theme = item;
	}
      
      gtk_object_set_data(GTK_OBJECT(item), "name", g_strdup(te[i].name));
      gtk_object_set_data(GTK_OBJECT(item), "rc", g_strdup(te[i].rc));
      gtk_object_set_data(GTK_OBJECT(item), "dir", g_strdup(te[i].dir));
      gtk_object_set_data(GTK_OBJECT(item), "icon", g_strdup(te[i].icon));
      gtk_signal_connect(GTK_OBJECT(item), "select",
			 GTK_SIGNAL_FUNC(click_entry), NULL);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(delete_entry), NULL);
      list = g_list_insert_sorted(list, item, sort_alpha);
    }
  free_theme_list(te, num);

  te = list_user_themes(&num);
  for (i = 0; i < num; i++)
    {
      item = gtk_list_item_new_with_label(te[i].name);
      gtk_widget_show(item);
      if (strcmp (d_theme, te[i].name) == 0)
	initial_theme = item;
      if (current_name && (strcmp (current_name, te[i].name) == 0))
	current_theme = item;

      gtk_object_set_data(GTK_OBJECT(item), "name", g_strdup(te[i].name));
      gtk_object_set_data(GTK_OBJECT(item), "rc", g_strdup(te[i].rc));
      gtk_object_set_data(GTK_OBJECT(item), "dir", g_strdup(te[i].dir));
      gtk_object_set_data(GTK_OBJECT(item), "icon", g_strdup(te[i].icon));
      gtk_signal_connect(GTK_OBJECT(item), "select",
			 GTK_SIGNAL_FUNC(click_entry), NULL);
      gtk_signal_connect(GTK_OBJECT(item), "destroy",
			 GTK_SIGNAL_FUNC(delete_entry), NULL);
      list = g_list_insert_sorted(list, item, sort_alpha);
    }
  free_theme_list(te, num);

  /* Suppress an update here, because the BROWSE mode will
   * cause a false initial selection
   */
  ignore_change = TRUE;
  gtk_list_append_items(GTK_LIST(disp_list), list);
  ignore_change = FALSE;

  if (!current_theme)
    current_theme = initial_theme;

  /* Suppress an update only if the current theme didn't change or
   * this was the first time around
   */
  if (current_theme)
    {
      if (current_name &&
	  strcmp (gtk_object_get_data(GTK_OBJECT(current_theme), "name"),
		  current_name) != 0)
	{
	  gtk_list_select_child (GTK_LIST (disp_list), current_theme);
	}
      else
	{
	  ignore_change = TRUE;
	  gtk_list_select_child (GTK_LIST (disp_list), current_theme);
	  ignore_change = FALSE;
	}
    }

  if (current_name != d_theme) {
    g_free (current_name);
    g_free (d_theme);
  } else
    g_free (d_theme);
  if (current_theme == NULL)
    ;
}

void
show_error (const char *string, gboolean fatal)
{
  GtkWidget *dialog;
  GtkWidget *hack_widget;

  dialog = gnome_message_box_new (string, GNOME_MESSAGE_BOX_ERROR,
				  GNOME_STOCK_BUTTON_OK, NULL);
  
  /* Kids, don't try this at home */
  /* this is pretty evil... <-:  */
  hack_widget = GNOME_DIALOG (dialog)->vbox;
  hack_widget = ((GtkBoxChild *) GTK_BOX (hack_widget)->children->data)->widget;
  hack_widget = ((GtkBoxChild *) GTK_BOX (hack_widget)->children->next->data)->widget;
  
  gtk_label_set_line_wrap (GTK_LABEL (hack_widget), TRUE);

  if (fatal)
    {
      fatal_error_dialog = dialog;
      
      gtk_widget_set_sensitive (capplet_widget, FALSE);
      gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
      delete_capplet (NULL, NULL, NULL);
    }
  else
    gtk_widget_show (dialog);
}
