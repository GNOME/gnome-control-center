
#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "da.h"
#include <signal.h>

GtkWidget *preview_control;

static gboolean   ignore_change = FALSE;

static GtkWidget *install_theme_file_sel;

static GtkWidget *capplet_widget;
static GtkWidget *theme_list;
static GtkWidget *auto_preview;

static ThemeEntry *current_theme = NULL;
static ThemeEntry *current_global_theme = NULL;
static ThemeEntry *initial_theme = NULL;
static ThemeEntry *last_theme = NULL;
static GtkWidget *font_sel;
static GtkWidget *font_cbox;
static gboolean initial_preview;
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
click_entry(GtkWidget *clist, gint row, gint col, GdkEvent *event,
	    gpointer data);
static void
state_changed (gboolean state);
static void
response_cb (GtkDialog *dialog, GtkResponseType response, gpointer data);

void
send_reread(void)
{
	if (!(current_theme && current_theme->rc))
		return;

	bonobo_widget_set_property (BONOBO_WIDGET (preview_control),
				    "theme", TC_CORBA_string,
				    gtkrc_tmp, NULL);
}

static void
auto_callback (GtkWidget *widget, gpointer data)
{
  if (ignore_change == FALSE) {
	  if (GTK_TOGGLE_BUTTON (auto_preview)->active)
		  click_preview (widget,NULL);
	  state_changed (TRUE);
  }
  
}
static void
font_callback (GtkWidget *widget, gchar *font, gpointer data)
{
  if (ignore_change == FALSE) {
	state_changed (TRUE);
	if (GTK_TOGGLE_BUTTON (auto_preview)->active)
	  click_preview (widget,NULL);
  }	
}
static void
use_theme_font_callback (GtkWidget *widget, gpointer data)
{
  if (ignore_change == FALSE) {
	state_changed (TRUE);
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
  gtk_file_selection_hide_fileop_buttons (GTK_FILE_SELECTION (install_theme_file_sel));
  /* BEGIN UGLINESS.  This code is stolen from gnome_dialog_set_parent.
   * We want its functionality, but it takes a GnomeDialog as its argument.
   * So we copy it )-: */

  parent = gtk_widget_get_toplevel (GTK_WIDGET (widget));

  /* We'd like to set a transient_for hint here, but it isn't
   * worth the bother, since our parent window isn't in this process
   */
#if 0
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
#endif
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
  void *sw, *label;
  GtkWidget *box, *hbox, *hbox2, *vbox;
  GtkWidget *frame, *button;
  GtkWidget *button_vbox;
  gboolean default_used;
  BonoboUIContainer *container;
  
  capplet_widget = gtk_dialog_new_with_buttons (_("Gtk+ Theme Selector"),
					 NULL, -1,
					 GTK_STOCK_HELP, GTK_RESPONSE_HELP,
					 GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
					 GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					 NULL);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (capplet_widget),
		  		     GTK_RESPONSE_APPLY, FALSE);

  box = gtk_vbox_new(FALSE, GNOME_PAD);
  hbox = gtk_hbox_new(TRUE, GNOME_PAD);
  frame = gtk_frame_new (_("Available Themes"));
  hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox2), GNOME_PAD_SMALL);
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), hbox, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (frame), hbox2);

  /* List of available themes
   */
  theme_list = gtk_clist_new (1);
  gtk_signal_connect (GTK_OBJECT (theme_list), "select_row", click_entry,
		      NULL);
  gtk_clist_set_selection_mode(GTK_CLIST(theme_list), GTK_SELECTION_BROWSE);
  sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add (GTK_CONTAINER(sw), theme_list);
  /* Mysterious allocation bug keeps shrinking hscrollbar during browse */
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER,
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
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0);
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
      initial_font = pango_font_description_to_string (style->rc_style->font_desc);
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
  container = bonobo_ui_container_new ();
  preview_control = bonobo_widget_new_control ("OAFIID:GNOME_Theme_Preview",
		  			       BONOBO_OBJREF (container));
  bonobo_object_unref (BONOBO_OBJECT (container));

  gtk_container_add(GTK_CONTAINER(frame), preview_control);
  update_theme_entries(theme_list);

  g_signal_connect (G_OBJECT (capplet_widget), "response", response_cb, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (capplet_widget)->vbox), box,
		      TRUE, TRUE, 0);

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
  rc = current_theme->rc;
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
  gnome_help_display_with_doc_id (gnome_program_get (), "users-guide", "gccdesktop.html", "#GCCTHEME", NULL);
}
static void
click_try(GtkWidget *widget, gpointer data)
{
  gchar *rc;
  gchar *dir;

/*  if (current_theme == current_global_theme)
    return;*/
  if (!current_theme)
	  return;

  current_global_theme = current_theme;
  rc = current_theme->rc;
  dir = current_theme->dir;

  /* hack for enlightenment only!!!! */
  /* FIXME: restart what ever windowmanager you have! */
  /*g_snprintf(cmd, sizeof(cmd), "eesh -e \"restart %s/e\"", dir);*/
  /* printf("%s\n", cmd); */
  send_reread();
  if (GTK_TOGGLE_BUTTON (font_cbox)->active)
    {
      use_theme(rc,
		gnome_font_picker_get_font_name (GNOME_FONT_PICKER (font_sel)));
    }
  else
    {
      use_theme(rc, NULL);
    }
  gdk_error_trap_push ();
  signal_apply_theme(capplet_widget);
  gdk_flush();
  /* system(cmd); */
  gdk_error_trap_pop ();
}
static void
click_ok(GtkWidget *widget, gpointer data)
{
  click_try (widget, data);
  gnome_config_set_bool ("/theme-switcher-capplet/settings/auto",GTK_TOGGLE_BUTTON (auto_preview)->active);
  gnome_config_set_string ("/theme-switcher-capplet/settings/theme", current_theme->name);
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

  if (!initial_theme)
    /* we hope this doesn't happen, but it could if things
     * are mis-installed -jrb */
    /* Damn, I hate this code... )-: */
    return;
  
  rc = initial_theme->rc;
    
  if ((current_global_theme != initial_theme) ||
	(initial_font_cbox != GTK_TOGGLE_BUTTON (font_cbox)->active) ||
	(GTK_TOGGLE_BUTTON (font_cbox)->active && strcmp (initial_font,
		 gnome_font_picker_get_font_name (GNOME_FONT_PICKER (font_sel)))))
    {
      
      /* This if statement is magic to determine if we want to reset the system theme.
       * It can almost certainly be cleaned up if needed.  Basicly, it sees if anything has
       * or if the theme has been set.. */
      send_reread();
      use_theme(rc, initial_font);
      gdk_error_trap_push ();
      signal_apply_theme(widget);
      gdk_flush();
      gdk_error_trap_pop ();
    }
  current_global_theme = initial_theme;
  ignore_change = TRUE;
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (auto_preview),
			       initial_preview);
  
  gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (font_cbox),
			       initial_font_cbox);
  if (initial_font)
    gnome_font_picker_set_font_name (GNOME_FONT_PICKER (font_sel),
				     initial_font);
  gtk_clist_select_row (GTK_CLIST (theme_list), initial_theme->row, -1);
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
click_entry(GtkWidget *clist, gint row, gint col, GdkEvent *event,
	    gpointer data)
{
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
      current_theme = gtk_clist_get_row_data (GTK_CLIST (clist), row);

      if (initial_theme)
	state_changed (TRUE);
      else
	state_changed (FALSE);
      
      if (GTK_TOGGLE_BUTTON (auto_preview)->active)
	click_preview (NULL,NULL);
    }
}

static void
item_destroy_notify (gpointer data)
{
  ThemeEntry *item = data;

  g_free(item->name);
  g_free(item->rc);
  g_free(item->dir);
  g_free(item->icon);
  
  if (current_theme == item)
    current_theme = NULL;

  g_free (item);
}
static gint sort_alpha(const void *a, const void *b)
{
  const ThemeEntry *A, *B;

  A = a;
  B = b;
  
  return g_strcasecmp(A->name, B->name);
}

static void
add_theme_list (GtkWidget *disp_list, GList *themes, gchar *d_theme, gchar *current_name)
{
  ThemeEntry *item;
  GList *l;

  for (l = themes; l != NULL; l = l->next)
    {
      gchar *text[1] = { NULL };
     
      item = l->data;
      text[0] = item->name;
      item->row = gtk_clist_append (GTK_CLIST(disp_list), text);

      gtk_clist_set_row_data_full (GTK_CLIST(disp_list), item->row, item,
		      		   item_destroy_notify);

      if (strcmp (d_theme, item->name) == 0)
	{
	  current_global_theme = item;
	  initial_theme = item;
	}
      if (current_name && (strcmp (current_name, item->name) == 0))
	{
	  current_theme = item;
	}
    }
}

void
update_theme_entries(GtkWidget *disp_list)
{
  GList      *themes;
  gchar *d_theme = gnome_config_get_string ("/theme-switcher-capplet/settings/theme=Default");
  gchar *current_name = NULL;

  if (current_theme)
    current_name = g_strdup (current_theme->name);
  else
    current_name = d_theme;

  current_theme = NULL;
  initial_theme = NULL;
 
  /* Suppress an update here, because the BROWSE mode will
   * cause a false initial selection
   */
  ignore_change = TRUE;
 
  gtk_clist_clear (GTK_CLIST(disp_list));
  
  themes = list_system_themes();
  themes = g_list_sort (themes, sort_alpha);
  add_theme_list (disp_list, themes, d_theme, current_name);

  themes = list_user_themes();
  themes = g_list_sort (themes, sort_alpha);
  add_theme_list (disp_list, themes, d_theme, current_name);

  ignore_change = FALSE;

  if (!current_theme)
    current_theme = initial_theme;

  /* Suppress an update only if the current theme didn't change or
   * this was the first time around
   */
  if (current_theme)
    {
      if (current_name &&
	  strcmp (current_theme->name,
		  current_name) != 0)
	{
	  gtk_clist_select_row (GTK_CLIST (disp_list), current_theme->row, 0);
	}
      else
	{
	  ignore_change = TRUE;
	  gtk_clist_select_row (GTK_CLIST (disp_list), current_theme->row, 0);
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

static void
response_cb (GtkDialog *dialog, GtkResponseType response, gpointer data)
{
	switch (response)
	{
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
	case GTK_RESPONSE_APPLY:
		click_ok (NULL, NULL);
		break;
	}
}

static void state_changed (gboolean state)
{
  gtk_dialog_set_response_sensitive (GTK_DIALOG (capplet_widget),
		  		     GTK_RESPONSE_APPLY, state);
}
