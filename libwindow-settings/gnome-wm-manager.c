#include <config.h>
#include "gnome-wm-manager.h"
#include "wm-properties.h"

#include <gtk/gtk.h>
#include <libgnomeui/gnome-client.h>
#include <gnome.h>

/* prototypes */
static void restart         (gboolean force);
static void revert_callback (void);
static void cancel_callback (void);
static void update_session (void);

static WindowManager *selected_wm = NULL;

/* Enumeration describing the current state of the capplet.
 * in any other state than idle, all controls are !sensitive.
 */
typedef enum {
        STATE_IDLE,
        STATE_TRY,
        STATE_REVERT,
        STATE_OK,
        STATE_CANCEL,
        STATE_TRY_REVERT,  /* Currently trying, revert afterwards */
        STATE_TRY_CANCEL   /* Currently trying, cancel afterwards */
} StateType;

/* Current state
 */
static StateType state = STATE_IDLE;

/* Set TRUE when we've exited the main loop, but restart_pending
 */
static gboolean quit_pending = FALSE;

/* Set TRUE when we're waiting for the WM to restart
 */
static gboolean restart_pending = FALSE;

static GtkWidget *capplet;

void
gnome_wm_manager_init (GtkWidget *some_window)
{
  wm_list_init();
  selected_wm = wm_list_get_current();
}

GList *
gnome_wm_manager_get_list (void)
{
  GList *wm_ditem;
  GList *wms = NULL;
  GnomeDesktopItem *ditem;

  for (wm_ditem = window_managers; wm_ditem != NULL; wm_ditem = wm_ditem->next) {
    ditem = ((WindowManager *)wm_ditem->data)->dentry;
    wms = g_list_prepend (wms, gnome_window_manager_new (ditem));
  }
  
  return wms;
}

void
gnome_wm_manager_set_current  (GnomeWindowManager *wm)
{
  /* this line is bogus */
  /*selected_wm = gnome_window_manager_get_ditem (wm);*/
}

GnomeWindowManager *
gnome_wm_manager_get_current (void)
{
  WindowManager *current_wm = wm_list_get_current();
  if (current_wm == NULL)
    return NULL;
  return GNOME_WINDOW_MANAGER (gnome_window_manager_new (current_wm->dentry));
}

void gnome_wm_manager_change_wm_to_settings (void)
{
        state = STATE_TRY;
        restart(FALSE);
        wm_list_set_current (selected_wm);
        wm_list_save ();
        update_session ();
}

gboolean
gnome_wm_manager_same_wm (GnomeWindowManager *wm1, GnomeWindowManager *wm2)
{
  GnomeDesktopItem *item1, *item2;
  const char *location1, *location2;
  gboolean same;

  item1 = gnome_window_manager_get_ditem (wm1);
  item2 = gnome_window_manager_get_ditem (wm2);

  location1 = gnome_desktop_item_get_location (item1);
  location2 = gnome_desktop_item_get_location (item2);

  same = (strcmp (location1, location2) == 0);

  gnome_desktop_item_unref (item1);
  gnome_desktop_item_unref (item2);

  return same;
}

static GtkWidget *restart_dialog = NULL;
static GtkWidget *restart_label = NULL;
static guint restart_dialog_timeout;
static gchar *restart_name = NULL;

/* Time until dialog times out */
static gdouble restart_remaining_time;
static gint restart_displayed_time;

static GnomeClient *client = NULL;

/* The possible transitions between states are described below.
 * 

 *  operation  | try   revert      ok         cancel     finish  
 *  ===========+=================================================
 *  IDLE       | TRY   REVERT      OK         CANCEL             
 *  TRY        |       TRY_REVERT  OK         TRY_CANCEL IDLE    
 *  REVERT     |                   CANCEL     CANCEL     IDLE    
 *  OK         |                                         (quit)  
 *  CANCEL     |                                         (quit)  
 *  TRY_REVERT |                   TRY_CANCEL TRY_CANCEL REVERT
 *  TRY_CANCEL |                                         CANCEL
 *
 * When a restart fails, there are three cases
 *
 * (1) The failure was because the current window manager didn't
 *     die. We inform the user of the situation, and then
 *     abort the operation.
 *
 * (2) The window manager didn't start, and we don't have a
 *     a fallback. We pop up a error dialog, tell the user
 *     to start a new window manager, and abort the operation.
 *
 * (3) The window manager didn't start, and we previously had a
 *     window manager runnning. We pop up a warning dialog,
 *     then try to go back to the old window manager.
 *
 *  operation  | (1)     (2)       (3)
 *  ===========+=================================================
 *  IDLE       | 
 *  TRY        | IDLE    IDLE      TRY
 *  REVERT     | IDLE    IDLE      REVERT
 *  OK         | (quit)  (quit)    OK
 *  CANCEL     | (quit)  (quit)    CANCEL
 *  TRY_REVERT | REVERT  REVERT    REVERT
 *  TRY_CANCEL | CANCEL  CANCEL    CANCEL
 */



static void
restart_label_update (void)
{
        gchar *tmp;

        if ((gint)restart_remaining_time != restart_displayed_time) {
                restart_displayed_time = restart_remaining_time;
                
                tmp = g_strdup_printf (_("Starting %s\n"
                                         "(%d seconds left before operation times out)"),
                                       restart_name,
                                       restart_displayed_time);
                gtk_label_set_text (GTK_LABEL (restart_label), tmp);
                g_free (tmp);
        }
}

static gboolean
restart_dialog_raise (gpointer data)
{
        if (restart_dialog && GTK_WIDGET_REALIZED (restart_dialog)) {
                restart_remaining_time -= 0.25;
                restart_label_update();
                gdk_window_raise (restart_dialog->window);
        }
        return TRUE;
}

static void
restart_dialog_destroyed (GtkWidget *widget)
{
        if (restart_dialog_timeout) {
                gtk_timeout_remove (restart_dialog_timeout);
                restart_dialog_timeout = 0;
        }

        restart_dialog = NULL;
}

static void
show_restart_dialog (gchar *name)
{
        GtkWidget *hbox;
        GtkWidget *frame;
        GtkWidget *pixmap;
        gchar *tmp;
        
        if (!restart_dialog) {
                restart_dialog = gtk_window_new (GTK_WINDOW_POPUP);
                gtk_window_set_position (GTK_WINDOW (restart_dialog),
                                         GTK_WIN_POS_CENTER);
                
                gtk_signal_connect (GTK_OBJECT (restart_dialog), "destroy",
                                    GTK_SIGNAL_FUNC (restart_dialog_destroyed),
                                    &restart_dialog);
                frame = gtk_frame_new (NULL);
                gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
                gtk_container_add (GTK_CONTAINER (restart_dialog), frame);
                
                hbox = gtk_hbox_new (FALSE, GNOME_PAD);
                gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD);
                gtk_container_add (GTK_CONTAINER (frame), hbox);

		tmp = gnome_unconditional_pixmap_file("gnome-info.png");
		if (tmp) {
                        pixmap = gnome_pixmap_new_from_file(tmp);
                        g_free(tmp);
                        gtk_box_pack_start (GTK_BOX (hbox), pixmap, FALSE, FALSE, 0);
                }

                restart_label = gtk_label_new ("");
                gtk_box_pack_start (GTK_BOX (hbox), restart_label, FALSE, FALSE, GNOME_PAD);
        }

        if (!restart_dialog_timeout) {
                restart_dialog_timeout = gtk_timeout_add (250, restart_dialog_raise, NULL);
        }

        restart_remaining_time = 10.0;
        restart_displayed_time = -1;
        if (restart_name)
                g_free (restart_name);

        restart_name = g_strdup (name);
        restart_label_update ();
                
        gtk_widget_show_all (restart_dialog);
}

static void
hide_restart_dialog (void)
{
        if (restart_dialog) 
                gtk_widget_destroy (restart_dialog);
}

static void
init_session (void)
{
        GnomeClientFlags flags;
        gint token;

        return;

	client = gnome_master_client ();
	flags = gnome_client_get_flags (client);
        
	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		/*token = gnome_startup_acquire_token("GNOME_WM_PROPERTIES",
                  gnome_client_get_id(client));*/
                
		if (token)
                        printf ("broken\n");
                        /*update_session();*/ 
		else {
			gnome_client_set_restart_style (client, 
							GNOME_RESTART_NEVER);
                        gnome_client_flush (client);
                }
        }
}

static void
update_session (void)
{
        WindowManager *current_wm = wm_list_get_current();
        //gchar *session_args[3];

        if (!current_wm)
                return;

        if (current_wm->session_managed) {
                gnome_client_set_restart_style (client, 
                                                GNOME_RESTART_NEVER);
                
        } else {
	  g_warning ("We don't properly handle non-session managed Window Managers right now");
	}
	//else {
          //      session_args[0] = argv0;
          //      session_args[1] = "--init-session-settings";
          //      session_args[2] = NULL;
          //     /* We use a priority of 15 so that we start after
          //       * session-managed WM's (priority 10), for safety.
          ///       */
					   //      gnome_client_set_priority (client, 15);
					   //      gnome_client_set_restart_style (client, 
					   //                                       GNOME_RESTART_ANYWAY);
					   //      gnome_client_set_restart_command (client, 2, 
					   //                                                  session_args);
//}

        gnome_client_flush (client);
}

static void 
init_callback (WMResult result, gpointer data)
{
        switch (result) {
        case WM_SUCCESS:
                break;
        case WM_ALREADY_RUNNING:
                g_warning (_("wm-properties-capplet: Unable to initialize window manager.\n"
                           "\tAnother window manager is already running and could not be killed\n"));
                break;
        case WM_CANT_START:
                g_warning (_("wm-properties-capplet: Unable to initialize window manager.\n"
                           "\t'%s' didn't start\n"), (gchar *)data);
                break;
        }

        g_free (data);
        gtk_main_quit ();
}

static void
restart_finalize ()
{
        wm_list_set_current (selected_wm);
        hide_restart_dialog();

        switch (state) {
        case STATE_TRY:
        case STATE_REVERT:
	  gtk_widget_set_sensitive (capplet, TRUE);
	  //update_gui();
                state = STATE_IDLE;
                break;
                
        case STATE_OK:
        case STATE_CANCEL:
                if (quit_pending)
                        gtk_main_quit();
                break;
        default:
                g_warning ("Finalize in state %d!!!\n", state);
                return;
        }

        restart_pending = FALSE;
}

static void 
restart_failure (WMResult reason)
{
        GtkWidget *msgbox;
        WindowManager *current_wm;
        gchar *msg = NULL;
        gboolean modal = FALSE;

        current_wm = wm_list_get_current ();

        /* Did the previous window manager not die?
         */
        if (reason == WM_ALREADY_RUNNING) {
                msg = g_strdup (_("Previous window manager did not die\n"));

                switch (state) {
                case STATE_TRY:
                case STATE_REVERT:
                case STATE_OK:
                case STATE_CANCEL:
                        selected_wm = current_wm;
                        restart_finalize ();
                        break;
                        
                case STATE_TRY_REVERT:
                        revert_callback ();
                        break;
                        
                case STATE_TRY_CANCEL:
                        cancel_callback ();
                        break;
                        
                default:
                        g_warning ("Failure in state %d!!!\n", state);
                        return;
                }
        }
        /* Is there something reasonable to try to fall back to?
         */
        else if (current_wm != selected_wm) {
                
                switch (state) {
                case STATE_TRY:
                case STATE_REVERT:
                case STATE_OK:
                case STATE_CANCEL:
                        msg = g_strdup_printf (_("Could not start '%s'.\n"
                                                 "Falling back to previous window manager '%s'\n"),
                                               selected_wm?gnome_desktop_item_get_string (selected_wm->dentry, GNOME_DESKTOP_ITEM_NAME):"Unknown",
                                               current_wm?gnome_desktop_item_get_string (current_wm->dentry, GNOME_DESKTOP_ITEM_NAME):"Unknown");
                        selected_wm = current_wm;
                        restart(TRUE);
                        break;
                        
                case STATE_TRY_REVERT:
                        revert_callback ();
                        break;
                        
                case STATE_TRY_CANCEL:
                        cancel_callback ();
                        break;
                        
                default:
                        g_warning ("Failure in state %d!!!\n", state);
                        return;
                }

        /* Give up */
        } else {

                switch (state) {
                case STATE_OK:
                case STATE_CANCEL:
                        modal = TRUE;  /* prevent an immediate exit */
                        /* Fall through */
                case STATE_TRY:
                case STATE_REVERT:
                        msg = g_strdup (_("Could not start fallback window manager.\n"
                                          "Please run a window manager manually. You can\n"
                                          "do this by selecting \"Run Program\" in the\n"
                                          "foot menu\n"));
                        
                        restart_finalize();
                        break;
                        
                case STATE_TRY_REVERT:
                        revert_callback ();
                        break;
                        
                case STATE_TRY_CANCEL:
                        cancel_callback ();
                        break;
                        
                default:
                        g_warning ("Failure in state %d!!!\n", state);
                        return;
                }
        }

        if (msg) {
                msgbox = gnome_message_box_new (msg,
                                                GNOME_MESSAGE_BOX_ERROR,
                                                _("OK"), NULL);
                if (modal)
                        gnome_dialog_run (GNOME_DIALOG (msgbox));
                else
                        gtk_widget_show (msgbox);
                g_free (msg);
        }
}

static void
show_restart_info (void)
{
        gchar *save_session;

        save_session = gnome_is_program_in_path ("save-session");

        if (save_session != NULL) {
                system (save_session);
        }

        g_free (save_session);
}

static void 
restart_finish (void)
{
        switch (state) {
        case STATE_TRY:
        case STATE_REVERT:
        case STATE_OK:
        case STATE_CANCEL:
                hide_restart_dialog();
                show_restart_info ();
                restart_finalize();
                break;

        case STATE_TRY_REVERT:
                revert_callback ();
                break;

        case STATE_TRY_CANCEL:
                cancel_callback ();
                break;

        default:
                g_warning ("Finished in state %d!!!\n", state);
                return;
        }
}

static void 
restart_callback (WMResult result, gpointer data)
{
        if (result == WM_SUCCESS)
                restart_finish ();
        else
                restart_failure (result);
}

static void
restart (gboolean force)
{
        WindowManager *current_wm = wm_list_get_current(), *mywm;
        static gboolean last_try_was_twm = FALSE;
        GnomeDesktopItem *twm_dentry = gnome_desktop_item_new ();
        WindowManager twm_fallback = {twm_dentry, "twm", "twm", 0, 0, 1, 0};

	gnome_desktop_item_set_entry_type (twm_dentry, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);
	gnome_desktop_item_set_string (twm_dentry,
				       GNOME_DESKTOP_ITEM_NAME, "twm");
	gnome_desktop_item_set_string (twm_dentry,
				       GNOME_DESKTOP_ITEM_COMMENT, "twm");
	gnome_desktop_item_set_string (twm_dentry,
				       GNOME_DESKTOP_ITEM_EXEC, "twm");

        if(selected_wm) {
                last_try_was_twm = FALSE;
                mywm = selected_wm;
        } else if(!last_try_was_twm) {
                last_try_was_twm = TRUE;
                mywm = (WindowManager*)&twm_fallback;
        } else {
                restart_finalize();
		gnome_desktop_item_unref (twm_dentry);
                return;
        }

        if (force || current_wm != mywm) {
                show_restart_dialog (g_strdup (gnome_desktop_item_get_string (mywm->dentry, GNOME_DESKTOP_ITEM_NAME)));
                if (state != STATE_OK && state != STATE_CANCEL)
		  gtk_widget_set_sensitive (capplet, FALSE);
                restart_pending = TRUE;
                wm_restart (mywm, 
                            capplet->window, 
                            restart_callback, 
                            NULL);
        } else {
                restart_finalize ();
        }
	
	gnome_desktop_item_unref (twm_dentry);
}

static void
revert_callback (void)
{
        StateType old_state = state;
        
        switch (state) {
        case STATE_IDLE:
        case STATE_TRY_REVERT:
                wm_list_revert();
                selected_wm = wm_list_get_revert();
                state = STATE_REVERT;

                restart (old_state == STATE_TRY_REVERT);
                /*update_gui();*/

                break;
                
        case STATE_TRY:
                state = STATE_TRY_REVERT;
                break;

        default:
                g_warning ("revert callback in state %d!!!\n", state);
                return;
        }
}

static void
cancel_callback (void)
{
        StateType old_state = state;
        
        switch (state) {
        case STATE_IDLE:
        case STATE_TRY_CANCEL:
                wm_list_revert();
                selected_wm = wm_list_get_revert();
                state = STATE_CANCEL;

                restart (old_state == STATE_TRY_CANCEL);

                break;
                
        case STATE_TRY:
                state = STATE_TRY_CANCEL;
                break;

        case STATE_REVERT:
                state = STATE_CANCEL;
                break;
                
        case STATE_TRY_REVERT:
                state = STATE_TRY_CANCEL;
                break;

        default:
                g_warning ("ok callback in state %d!!!\n", state);
                return;
        }
}


