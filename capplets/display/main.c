#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <gdk/gdkx.h>

#include <X11/extensions/Xrandr.h>

#include "capplet-util.h"

#define REVERT_COUNT 20

struct ScreenInfo
{
  int current_width;
  int current_height;
  SizeID current_size;
  short current_rate;
  Rotation current_rotation;

  SizeID old_size;
  short old_rate;
  Rotation old_rotation;
  
  XRRScreenConfiguration *config;
  XRRScreenSize *sizes;
  int n_sizes;
  
  GtkWidget *resolution_widget;
  GtkWidget *rate_widget;
};

struct DisplayInfo {
  int n_screens;
  struct ScreenInfo *screens;

  GtkWidget *per_computer_check;
  gboolean was_per_computer;
};


static void generate_rate_menu (struct ScreenInfo *screen_info);
static void generate_resolution_menu(struct ScreenInfo* screen_info);

struct DisplayInfo *
read_display_info (GdkDisplay *display)
{
  struct DisplayInfo *info;
  struct ScreenInfo *screen_info;
  GdkScreen *screen;
  GdkWindow *root_window;
  int i;

  info = g_new (struct DisplayInfo, 1);
  info->n_screens = gdk_display_get_n_screens (display);
  info->screens = g_new (struct ScreenInfo, info->n_screens);

  for (i = 0; i < info->n_screens; i++)
    {
      screen = gdk_display_get_screen (display, i);
      
      screen_info = &info->screens[i];
      screen_info->current_width = gdk_screen_get_width (screen);
      screen_info->current_height = gdk_screen_get_height (screen);

      root_window = gdk_screen_get_root_window (screen);
      screen_info->config = XRRGetScreenInfo (gdk_x11_display_get_xdisplay (display),
					      gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)));
      
      screen_info->current_rate = XRRConfigCurrentRate (screen_info->config);
      screen_info->current_size = XRRConfigCurrentConfiguration (screen_info->config, &screen_info->current_rotation);
      screen_info->sizes = XRRConfigSizes (screen_info->config, &screen_info->n_sizes);
    }

  return info;
}

void
update_display_info (struct DisplayInfo *info, GdkDisplay *display)
{
  struct ScreenInfo *screen_info;
  GdkScreen *screen;
  GdkWindow *root_window;
  int i;

  g_assert (info->n_screens == gdk_display_get_n_screens (display));
  
  for (i = 0; i < info->n_screens; i++)
    {
      screen = gdk_display_get_screen (display, i);
      
      screen_info = &info->screens[i];

      screen_info->old_rate = screen_info->current_rate;
      screen_info->old_size = screen_info->current_size;
      screen_info->old_rotation = screen_info->current_rotation;
      
      screen_info->current_width = gdk_screen_get_width (screen);
      screen_info->current_height = gdk_screen_get_height (screen);

      root_window = gdk_screen_get_root_window (screen);
      XRRFreeScreenConfigInfo (screen_info->config);
      screen_info->config = XRRGetScreenInfo (gdk_x11_display_get_xdisplay (display),
					      gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)));

      screen_info->current_rate = XRRConfigCurrentRate (screen_info->config);
      screen_info->current_size = XRRConfigCurrentConfiguration (screen_info->config, &screen_info->current_rotation);
      screen_info->sizes = XRRConfigSizes (screen_info->config, &screen_info->n_sizes);
    }
}

static int
get_current_resolution (struct ScreenInfo *screen_info)
{
  GtkWidget *menu;
  GList *children;
  GList *child;
  int i;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (screen_info->resolution_widget));
  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (screen_info->resolution_widget));
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  child = g_list_nth (children, i);

  if (child != NULL)
    return GPOINTER_TO_INT (g_object_get_data (child->data, "screen_nr"));
  else
    return 0;
}

static int
get_current_rate (struct ScreenInfo *screen_info)
{
  GtkWidget *menu;
  GList *children;
  GList *child;
  int i;

  i = gtk_option_menu_get_history (GTK_OPTION_MENU (screen_info->rate_widget));
  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (screen_info->rate_widget));
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  child = g_list_nth (children, i);

  if (child != NULL)
    return GPOINTER_TO_INT (g_object_get_data (child->data, "rate"));
  else
    return 0;
}

static gboolean
apply_config (struct DisplayInfo *info)
{
  int i;
  GdkDisplay *display;
  Display *xdisplay;
  GdkScreen *screen;
  gboolean changed;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  changed = FALSE;
  for (i = 0; i < info->n_screens; i++)
    {
      struct ScreenInfo *screen_info = &info->screens[i];
      Status status;
      GdkWindow *root_window;
      int new_res, new_rate;

      screen = gdk_display_get_screen (display, i);
      root_window = gdk_screen_get_root_window (screen);

      new_res = get_current_resolution (screen_info);
      new_rate = get_current_rate (screen_info);
      
      if (new_res != screen_info->current_size ||
	  new_rate != screen_info->current_rate)
	{
	  changed = TRUE; 
	  status = XRRSetScreenConfigAndRate (xdisplay, 
					      screen_info->config,
					      gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)),
					      new_res,
					      screen_info->current_rotation,
					      new_rate,
					      GDK_CURRENT_TIME);
	}
    }

  update_display_info (info, display);
  
  /* xscreensaver should handle this itself, but does not currently so we hack
   * it.  Ignore failures in case xscreensaver is not installed */
  if (changed)
   g_spawn_command_line_async ("xscreensaver-command -restart", NULL);

  return changed;
}

static int
revert_config (struct DisplayInfo *info)
{
  int i;
  GdkDisplay *display;
  Display *xdisplay;
  GdkScreen *screen;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  
  for (i = 0; i < info->n_screens; i++)
    {
      struct ScreenInfo *screen_info = &info->screens[i];
      Status status;
      GdkWindow *root_window;

      screen = gdk_display_get_screen (display, i);
      root_window = gdk_screen_get_root_window (screen);

      status = XRRSetScreenConfigAndRate (xdisplay, 
					  screen_info->config,
					  gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)),
					  screen_info->old_size,
					  screen_info->old_rotation,
					  screen_info->old_rate,
					  GDK_CURRENT_TIME);
      
    }

  update_display_info (info, display);

  /* Need to update the menus to the new settings */
  for (i = 0; i < info->n_screens; i++)
    {
      struct ScreenInfo *screen_info = &info->screens[i];
      
      generate_resolution_menu (screen_info);
      generate_rate_menu (screen_info);
    }

  /* xscreensaver should handle this itself, but does not currently so we hack
   * it.  Ignore failures in case xscreensaver is not installed */
  g_spawn_command_line_async ("xscreensaver-command -restart", NULL);
  
  return 0;
}

static GtkWidget *
wrap_in_label (GtkWidget *child, char *text)
{
  GtkWidget *vbox, *hbox;
  GtkWidget *label;
  char *str;

  vbox = gtk_vbox_new (FALSE, 6);
  label = 0;

  label = gtk_label_new ("");

  str = g_strdup_printf ("<b>%s</b>", text);
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox),
		      label,
		      FALSE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 0);

  label = gtk_label_new ("    ");
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox),
		      label,
		      FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (hbox),
		      child,
		      TRUE, TRUE, 0);

  gtk_widget_show (hbox);
  
  gtk_box_pack_start (GTK_BOX (vbox),
		      hbox,
		      FALSE, FALSE, 0);

  gtk_widget_show (vbox);

  return vbox;
}

static gboolean
show_resolution (int width, int height)
{
  if (width >= 800 && height >= 600)
    return TRUE;

  if (width == 640 && height == 480)
    return TRUE;

  return FALSE;
}

static void
resolution_changed_callback (GtkWidget *optionmenu, struct ScreenInfo *screen_info)
{
  generate_rate_menu(screen_info);
}

static void
generate_rate_menu (struct ScreenInfo *screen_info)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  short *rates;
  int nrates, i;
  int size_nr;
  char *str;
  int closest_rate_nr;
  
  gtk_option_menu_remove_menu (GTK_OPTION_MENU (screen_info->rate_widget));

  menu = gtk_menu_new ();

  size_nr = get_current_resolution (screen_info);
  
  closest_rate_nr = -1;
  rates = XRRConfigRates (screen_info->config, size_nr, &nrates);
  for (i = 0; i < nrates; i++)
    {
      str = g_strdup_printf (_("%d Hz"), rates[i]);

      if ((closest_rate_nr < 0) ||
	  (ABS (rates[i] - screen_info->current_rate) <
	   ABS (rates[closest_rate_nr] - screen_info->current_rate)))
	closest_rate_nr = i;
      
      menuitem = gtk_menu_item_new_with_label (str);

      g_object_set_data (G_OBJECT (menuitem), "rate", GINT_TO_POINTER ((int)rates[i]));
	  
      g_free (str);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (screen_info->rate_widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (screen_info->rate_widget),
			       closest_rate_nr);
}

static void
generate_resolution_menu(struct ScreenInfo* screen_info)
{
  GtkWidget *menu;
  GtkWidget *menuitem;
  int i, item, current_item;
  XRRScreenSize *sizes;
  char *str;
  SizeID current_size;
  Rotation rot;

  gtk_option_menu_remove_menu(GTK_OPTION_MENU(screen_info->resolution_widget));
  menu = gtk_menu_new ();
  current_size = XRRConfigCurrentConfiguration (screen_info->config, &rot);
  
  current_item = 0;
  item = 0;
  sizes = screen_info->sizes;
  for (i = 0; i < screen_info->n_sizes; i++)
    {
      if (i == current_size || show_resolution (sizes[i].width, sizes[i].height))
	{
	  str = g_strdup_printf ("%dx%d", sizes[i].width, sizes[i].height);
	  
	  if (i == current_size)
	    current_item = item;
	  
	  menuitem = gtk_menu_item_new_with_label (str);

	  g_object_set_data (G_OBJECT (menuitem), "screen_nr", GINT_TO_POINTER (i));
	  
	  g_free (str);
	  gtk_widget_show (menuitem);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
	  item++;
	}
    }
  
	gtk_option_menu_set_menu (GTK_OPTION_MENU (screen_info->resolution_widget), menu);
	gtk_option_menu_set_history (GTK_OPTION_MENU (screen_info->resolution_widget), current_item);

	g_signal_connect (screen_info->resolution_widget, "changed", G_CALLBACK (resolution_changed_callback), screen_info);
  
	gtk_widget_show (screen_info->resolution_widget);
}

static GtkWidget *
create_resolution_menu (struct ScreenInfo *screen_info) 
{
  screen_info->resolution_widget = gtk_option_menu_new ();
  generate_resolution_menu (screen_info);

  return screen_info->resolution_widget;
}

static GtkWidget *
create_rate_menu (struct ScreenInfo *screen_info)
{
  GtkWidget *optionmenu;

  screen_info->rate_widget = optionmenu = gtk_option_menu_new ();

  generate_rate_menu (screen_info);
  
  gtk_widget_show (optionmenu);
  return optionmenu;
}

static GtkWidget *
create_screen_widgets (struct ScreenInfo *screen_info, int nr, gboolean no_header)
{
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *option_menu;
  GtkWidget *ret;
  char *str;

  table = gtk_table_new (2, 2, FALSE);

  gtk_table_set_row_spacings ( GTK_TABLE (table), 6);
  gtk_table_set_col_spacings ( GTK_TABLE (table), 12);
  
  label = gtk_label_new_with_mnemonic (_("_Resolution:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table),
		    label,
		    0, 1,
		    0, 1,
		    GTK_FILL, 0,
		    0, 0);

  option_menu = create_resolution_menu (screen_info);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), option_menu);
  gtk_table_attach (GTK_TABLE (table),
		    option_menu,
		    1, 2,
		    0, 1,
		    GTK_FILL | GTK_EXPAND, 0,
		    0, 0);
  
  label = gtk_label_new_with_mnemonic (_("Re_fresh rate:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_widget_show (label);
  gtk_table_attach (GTK_TABLE (table),
		    label,
		    0, 1,
		    1, 2,
		    GTK_FILL, 0,
		    0, 0);
  gtk_widget_show (table);
  
  option_menu = create_rate_menu (screen_info);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), option_menu);
  gtk_table_attach (GTK_TABLE (table),
		    option_menu,
		    1, 2,
		    1, 2,
		    GTK_FILL | GTK_EXPAND, 0,
		    0, 0);
  
  if (nr == 0)
    str = g_strdup (_("Default Settings"));
  else
    str = g_strdup_printf (_("Screen %d Settings\n"), nr+1);
  ret = wrap_in_label (table, str);
  g_free (str);
  return ret;
}


static GtkWidget *
create_dialog (struct DisplayInfo *info)
{
  GtkWidget *dialog;
  GtkWidget *screen_widget;
  GtkWidget *per_computer_check;
  int i;
  GtkWidget *wrapped;
  GtkWidget *vbox;
  GConfClient *client;
  char *key;
  char *resolution;
  char *str;
#ifdef HOST_NAME_MAX
  char hostname[HOST_NAME_MAX + 1];
#else
  char hostname[256];
#endif
  
  dialog = gtk_dialog_new_with_buttons (_("Screen Resolution Preferences"),
					NULL,
					GTK_DIALOG_NO_SEPARATOR,
 					"gtk-close",
 					GTK_RESPONSE_CLOSE,
					"gtk-apply",
					GTK_RESPONSE_APPLY,
					"gtk-help",
					GTK_RESPONSE_HELP,
					NULL);
					
  gtk_window_set_resizable(GTK_WINDOW (dialog), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);  
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
  capplet_set_icon (dialog, "display-capplet.png");
  
  vbox = gtk_vbox_new (FALSE, 18);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      vbox, FALSE, FALSE, 0);
  gtk_widget_show (vbox);
  
  for (i = 0; i < info->n_screens; i++)
    {
      screen_widget = create_screen_widgets (&info->screens[i], i, info->n_screens == 1);
      gtk_box_pack_start (GTK_BOX (vbox),
			  screen_widget, FALSE, FALSE, 0);
      gtk_widget_show (screen_widget);
    }

  per_computer_check = NULL;
  info->was_per_computer = FALSE;
  if (gethostname (hostname, sizeof (hostname)) == 0 &&
      strcmp (hostname, "localhost") != 0 &&
      strcmp (hostname, "localhost.localdomain") != 0)
    {
      
      str = g_strdup_printf (_("_Make default for this computer (%s) only"), hostname);
      per_computer_check = gtk_check_button_new_with_mnemonic (str);

      /* If we previously set the resolution specifically for this hostname, default
	 to it on */
      client = gconf_client_get_default ();
      key = g_strconcat ("/desktop/gnome/screen/", hostname,  "/0/resolution",NULL);
      resolution = gconf_client_get_string (client, key, NULL);
      g_free (resolution);
      g_free (key);
      g_object_unref (client);
      
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (per_computer_check),
				    resolution != NULL);
      info->was_per_computer = resolution != NULL;
      
      gtk_widget_show (per_computer_check);
      
      wrapped = wrap_in_label (per_computer_check, _("Options"));
      gtk_box_pack_start (GTK_BOX (vbox),
			  wrapped, FALSE, FALSE, 0);
      gtk_widget_show (wrapped);
    }

  info->per_computer_check = per_computer_check;
  
  return dialog;
}

struct TimeoutData {
  int time;
  GtkLabel *label;
  GtkDialog *dialog;
  gboolean timed_out;
};

char *
timeout_string (int time)
{
  return g_strdup_printf (ngettext ("Testing the new settings. If you don't respond in %d second the previous settings will be restored.", "Testing the new settings. If you don't respond in %d seconds the previous settings will be restored.", time), time);
}

gboolean
save_timeout_callback (gpointer _data)
{
  struct TimeoutData *data = _data;
  char *str;
  
  data->time--;

  if (data->time == 0)
    {
      gtk_dialog_response (data->dialog, GTK_RESPONSE_NO);
      data->timed_out = TRUE;
      return FALSE;
    }

  str = timeout_string (data->time);
  gtk_label_set_text (data->label, str);
  g_free (str);
  
  return TRUE;
}

static int
run_revert_dialog (struct DisplayInfo *info,
		   GtkWidget *parent)
{
  GtkWidget *dialog;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *label_sec;
  GtkWidget *image;
  int res;
  struct TimeoutData timeout_data;
  guint timeout;
  char *str;

  dialog = gtk_dialog_new ();
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), "");
  
  label = gtk_label_new (NULL);
  str = g_strdup_printf ("<b>%s</b>", _("Do you want to keep this resolution?"));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
  
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

  str = timeout_string (REVERT_COUNT);
  label_sec = gtk_label_new (str);
  g_free (str);
  gtk_label_set_line_wrap (GTK_LABEL (label_sec), TRUE);
  gtk_label_set_selectable (GTK_LABEL (label_sec), TRUE);
  gtk_misc_set_alignment (GTK_MISC (label_sec), 0.0, 0.5);

  hbox = gtk_hbox_new (FALSE, 6);
  vbox = gtk_vbox_new (FALSE, 6);

  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), label_sec, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
  gtk_dialog_add_buttons (GTK_DIALOG (dialog),_("Use _previous resolution"), GTK_RESPONSE_NO, _("_Keep resolution"), GTK_RESPONSE_YES, NULL);
  
  gtk_widget_show_all (hbox);

  timeout_data.time = REVERT_COUNT;
  timeout_data.label = GTK_LABEL (label_sec);
  timeout_data.dialog = GTK_DIALOG (dialog);
  timeout_data.timed_out = FALSE;
  
  timeout = g_timeout_add (1000, save_timeout_callback, &timeout_data);
  res = gtk_dialog_run (GTK_DIALOG (dialog));

  if (!timeout_data.timed_out)
    g_source_remove (timeout);

  gtk_widget_destroy (dialog);
  
  return res == GTK_RESPONSE_YES;
}

static void
save_to_gconf (struct DisplayInfo *info, gboolean save_computer, gboolean clear_computer)
{
  GConfClient    *client;
  gboolean res;
#ifdef HOST_NAME_MAX
  char hostname[HOST_NAME_MAX + 1];
#else
  char hostname[256];
#endif
  char *path, *key, *str;
  int i;

  gethostname (hostname, sizeof(hostname));
  
  client = gconf_client_get_default ();

  if (clear_computer)
    {
      for (i = 0; i < info->n_screens; i++)
	{
	  key = g_strdup_printf ("/desktop/gnome/screen/%s/%d/resolution",
				 hostname, i);
	  gconf_client_unset (client, key, NULL);
	  g_free (key);
	  key = g_strdup_printf ("/desktop/gnome/screen/%s/%d/rate",
				 hostname, i);
	  gconf_client_unset (client, key, NULL);
	  g_free (key);
	}
    }
  
  if (save_computer)
    {
      path = g_strconcat ("/desktop/gnome/screen/",
			  hostname,
			  "/",
			  NULL);
    }
  else
    path = g_strdup ("/desktop/gnome/screen/default/");
	       
  for (i = 0; i < info->n_screens; i++)
    {
      struct ScreenInfo *screen_info = &info->screens[i];
      int new_res, new_rate;

      new_res = get_current_resolution (screen_info);
      new_rate = get_current_rate (screen_info);

      key = g_strdup_printf ("%s%d/resolution", path, i);
      str = g_strdup_printf ("%dx%d",
			     screen_info->sizes[new_res].width,
			     screen_info->sizes[new_res].height);
      
      res = gconf_client_set_string  (client, key, str, NULL);
      g_free (str);
      g_free (key);
      
      key = g_strdup_printf ("%s%d/rate", path, i);
      res = gconf_client_set_int  (client, key, new_rate, NULL);
      g_free (key);
    }

  g_free (path);
  g_object_unref (client);
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id, struct DisplayInfo *info)
{
  gboolean save_computer, clear_computer;
  switch (response_id)
    {
    case GTK_RESPONSE_DELETE_EVENT:
      gtk_main_quit ();
      break;
    case GTK_RESPONSE_HELP:
      capplet_help (GTK_WINDOW (dialog), "user-guide.xml", "goscustdesk-70");
      break;
    case GTK_RESPONSE_APPLY:
      save_computer = info->per_computer_check != NULL && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (info->per_computer_check));
      clear_computer = !save_computer && info->was_per_computer;
	  
      if (apply_config (info))
	{
	  if (!run_revert_dialog (info, GTK_WIDGET (dialog)))
	    {
	      revert_config (info);
	      return;
	    }
	}
      
      save_to_gconf (info, save_computer, clear_computer);
      gtk_main_quit ();
      break;
	case GTK_RESPONSE_CLOSE:
		gtk_main_quit ();
		break;
    }
}

int
main (int argc, char *argv[])
{
  int major, minor;
  int event_base, error_base;
  GdkDisplay *display;
  GtkWidget *dialog;
  struct DisplayInfo *info;
  Display *xdisplay;
 
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-display-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);


  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  
  if (!XRRQueryExtension (xdisplay, &event_base, &error_base) ||
      XRRQueryVersion (xdisplay, &major, &minor) == 0)
    {
      GtkWidget *msg_dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
						   _("The X Server does not support the XRandR extension.  Runtime resolution changes to the display size are not available."));
      gtk_dialog_run (GTK_DIALOG (msg_dialog));
      gtk_widget_destroy (msg_dialog);
      exit (0);
    }
  else if (major != 1 || minor < 1)
    {
      GtkWidget *msg_dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
						      _("The version of the XRandR extension is incompatible with this program. Runtime changes to the display size are not available."));
      gtk_dialog_run (GTK_DIALOG (msg_dialog));
      gtk_widget_destroy (msg_dialog);
      exit (0);
    }
  
  info = read_display_info (display);
  dialog = create_dialog (info);

  g_signal_connect (G_OBJECT (dialog), "response", G_CALLBACK (cb_dialog_response), info);
  gtk_widget_show (dialog);

  gtk_main ();
  return 0;
}
