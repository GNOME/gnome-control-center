/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998-1999 Redhat Software Inc.
 * Code available under the Gnu GPL.
 * Authors: Jonathan Blandford <jrb@redhat.com>
 *          Owen Taylor <otaylor@redhat.com>
 */
#include <ctype.h>
#include <config.h>
#include <locale.h>
#include "wm-properties.h"
#include "capplet-widget.h"
#include <gnome.h>
#include <libgnomeui/gnome-window-icon.h>

/* prototypes */
static void restart         (gboolean force);
static void try_callback    (void);
static void help_callback     (void);
static void ok_callback     (void);
static void revert_callback (void);
static void cancel_callback (void);

/* structures */

typedef struct {
        GtkWidget *dialog;
        GtkWidget *name_entry;
        GtkWidget *exec_entry;
        GtkWidget *config_entry;
        GtkWidget *sm_toggle;
} WMDialog;

/* vars. */
static GtkWidget *capplet;
static GtkWidget *delete_button;
static GtkWidget *edit_button;
static GtkWidget *config_button;
static GtkWidget *clist;

static WindowManager *selected_wm = NULL;

static GtkWidget *restart_dialog = NULL;
static GtkWidget *restart_label = NULL;
guint restart_dialog_timeout;
gchar *restart_name = NULL;

/* Time until dialog times out */
gdouble restart_remaining_time;
gint restart_displayed_time;

GnomeClient *client = NULL;
gchar *argv0;

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



/* Current state
 */
StateType state = STATE_IDLE;

/* Set TRUE when we've exited the main loop, but restart_pending
 */
gboolean quit_pending = FALSE;

/* Set TRUE when we're waiting for the WM to restart
 */
gboolean restart_pending = FALSE;

/* Set TRUE while we are filling in the list
 */
gboolean in_fill = FALSE;

static GtkWidget *
left_aligned_button (gchar *label)
{
  GtkWidget *button = gtk_button_new_with_label (label);
  gtk_misc_set_alignment (GTK_MISC (GTK_BIN (button)->child),
			  0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (button)->child),
			GNOME_PAD_SMALL, 0);

  return button;
}
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
update_session (void)
{
        WindowManager *current_wm = wm_list_get_current();
        gchar *session_args[3];

        if (!current_wm)
                return;

        if (current_wm->session_managed) {
                gnome_client_set_restart_style (client, 
                                                GNOME_RESTART_NEVER);
                
        } else {
                session_args[0] = argv0;
                session_args[1] = "--init-session-settings";
                session_args[2] = NULL;
                /* We use a priority of 15 so that we start after
                 * session-managed WM's (priority 10), for safety.
                 */
                gnome_client_set_priority (client, 15);
                gnome_client_set_restart_style (client, 
                                                GNOME_RESTART_ANYWAY);
                gnome_client_set_restart_command (client, 2, 
                                                  session_args);
        }

        gnome_client_flush (client);
}

static void
init_session (void)
{
        GnomeClientFlags flags;
        gint token;

	client = gnome_master_client ();
	flags = gnome_client_get_flags (client);
        
	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		token = gnome_startup_acquire_token("GNOME_WM_PROPERTIES",
                                                    gnome_client_get_id(client));
                
		if (token)
                        update_session();
		else {
			gnome_client_set_restart_style (client, 
							GNOME_RESTART_NEVER);
                        gnome_client_flush (client);
                }
        }
}

static void
update_gui (void)
{
        GList *tmp_list;
        WindowManager *current_wm = wm_list_get_current();
        gint new_row;
        gchar *tmpstr;

        gtk_clist_clear (GTK_CLIST (clist));

        in_fill = TRUE;
        
        tmp_list = window_managers;
        while (tmp_list) {
                gchar *row_text;
                WindowManager *wm;

                wm = tmp_list->data;

                if (wm == current_wm) {
                        row_text = g_strdup_printf (_("%s (Current)"),
                                              wm->dentry->name);

                        tmpstr = g_strdup_printf (_("Run Configuration Tool for %s"),
                                              wm->dentry->name);
                                              
                        gtk_label_set_text (GTK_LABEL (GTK_BIN (config_button)->child),
                                            tmpstr);
                        gtk_widget_set_sensitive (config_button,
                                                  wm->is_config_present);

                        g_free (tmpstr);
                } else if (wm->is_user && !wm->is_present) {
                        row_text = g_strconcat (wm->dentry->name,
                                                _(" (Not found)"), NULL);
                } else {
                        row_text = g_strdup (wm->dentry->name);
                }
                
                new_row = gtk_clist_append (GTK_CLIST (clist), &row_text);
                gtk_clist_set_row_data (GTK_CLIST (clist), new_row, wm);

                if (wm == selected_wm)
                        gtk_clist_select_row (GTK_CLIST (clist), new_row, 0);
                
                g_free (row_text);
                
                tmp_list = tmp_list->next;
        }
        
        in_fill = FALSE;

        if(selected_wm) {
                gtk_widget_set_sensitive (edit_button, selected_wm->is_user);
                gtk_widget_set_sensitive (delete_button, selected_wm->is_user);
        } else {
                gtk_widget_set_sensitive (edit_button, FALSE);
                gtk_widget_set_sensitive (delete_button, FALSE);
        }

        if (current_wm)
                gtk_widget_show(config_button);
        else
                gtk_widget_hide(config_button);
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
                update_gui();
                state = STATE_IDLE;
                break;
                
        case STATE_OK:
                wm_list_save ();
                update_session ();
                
                /* Fall through */
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
                                               selected_wm?selected_wm->dentry->name:"Unknown",
                                               current_wm?current_wm->dentry->name:"Unknown");
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
        GtkWidget *dialog;

        save_session = gnome_is_program_in_path ("save-session");
        if (save_session) {
                dialog = gnome_message_box_new (
                        _("Your current window manager has been changed. In order for\n"
                          "this change to be saved, you will need to save your current\n"
                          "session. You can do so immediately by selecting the \"Save session\n"
                          "now\" below, or you can save your session later.  This can be\n"
                          "done either selecting \"Save Current Session\" under \"Settings\"\n"
                          "in the main menu, or by turning on \"Save Current Setup\" when\n"
                          "you log out.\n"),
                        GNOME_MESSAGE_BOX_INFO, _("Save Session Later"), _("Save Session Now"), NULL);
        } else {
                dialog = gnome_message_box_new (
                        _("Your current window manager has been changed. In order for\n"
                          "this change to be saved, you will need to save your current\n"
                          "session. This can be done by either selecting \"Save Current Session\"\n"
                          "under \"Settings\" in the main menu, or by turning on\n"
                          "\"Save Current Setup\" when you log out.\n"),
                        GNOME_MESSAGE_BOX_INFO, GNOME_STOCK_BUTTON_CLOSE, NULL);
        }
        if ((gnome_dialog_run_and_close (GNOME_DIALOG (dialog)) == 1) && save_session) {
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
                restart_finalize();
                show_restart_info();
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
destroy_callback (GtkWidget *widget, void *data)
{
}

static void
restart (gboolean force)
{
        WindowManager *current_wm = wm_list_get_current(), *mywm;
        static gboolean last_try_was_twm = FALSE;
        const char *twm_argv[] = {"twm", NULL};
        GnomeDesktopEntry twm_dentry = {"twm", "twm",
                                              1, NULL, NULL,
                                              NULL, NULL, 0, NULL,
                                              NULL, NULL, 0, 0};
        WindowManager twm_fallback = {NULL, "twm", "twm", 0, 0, 1, 0};

        twm_dentry.exec = twm_argv;
        twm_fallback.dentry = &twm_dentry;
   
   
        if(selected_wm) {
                last_try_was_twm = FALSE;
                mywm = selected_wm;
        } else if(!last_try_was_twm) {
                last_try_was_twm = TRUE;
                mywm = (WindowManager*)&twm_fallback;
        } else {
                restart_finalize();
                return;
        }

        if (force || current_wm != mywm) {
                show_restart_dialog (mywm->dentry->name);
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
}

static void
try_callback (void)
{
        if (state != STATE_IDLE) {
                g_warning ("try_callback in state %d!!!\n", state);
                return;
        }

        state = STATE_TRY;
        restart(FALSE);
}

static void
help_callback (void)
{
    GnomeHelpMenuEntry help_entry= {"control-center",
    "desktop-intro.html#GCCWM"};
    gnome_help_display (NULL, &help_entry);
}

static void
ok_callback (void)
{
        switch (state) {
        case STATE_IDLE:
                state = STATE_OK;
                restart(FALSE);
                return;
                
        case STATE_TRY:
                state = STATE_OK;
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
                update_gui();

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
		/* If we don't know which window manager should be running (are there none in my list?) bail out */
		if (selected_wm == NULL) return;
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

static WMDialog *
create_dialog (gchar *title)
{
        GtkWidget *label;
        GtkWidget *alignment;
        GtkWidget *table;
        WMDialog *dialog;

        dialog = g_new (WMDialog, 1);
        
        dialog->dialog = gnome_dialog_new (_("Add New Window Manager"),
                                           GNOME_STOCK_BUTTON_OK,
                                           GNOME_STOCK_BUTTON_CANCEL,
                                           NULL);

        gnome_dialog_set_default (GNOME_DIALOG (dialog->dialog), 0);
        gnome_dialog_close_hides (GNOME_DIALOG (dialog->dialog), TRUE);

        table = gtk_table_new (4, 2, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
        gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (dialog->dialog)->vbox),
                           table);
        
        label = gtk_label_new (_("Name:"));
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label,
                          0, 1, 0, 1,
                          GTK_FILL, 0,
                          0, 0);

        dialog->name_entry = gtk_entry_new ();
        gtk_table_attach (GTK_TABLE (table), dialog->name_entry,
                          1, 2, 0, 1,
                          GTK_FILL | GTK_EXPAND, 0,
                          0, 0);

        label = gtk_label_new (_("Command:"));
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label,
                          0, 1, 1, 2,
                          GTK_FILL, 0,
                          0, 0);

        dialog->exec_entry = gtk_entry_new ();
        gtk_table_attach (GTK_TABLE (table), dialog->exec_entry,
                          1, 2, 1, 2,
                          GTK_FILL | GTK_EXPAND, 0,
                          0, 0);

        label = gtk_label_new (_("Configuration Command:"));
        gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
        gtk_table_attach (GTK_TABLE (table), label,
                          0, 1, 2, 3,
                          GTK_FILL, 0,
                          0, 0);

        dialog->config_entry = gtk_entry_new ();
        gtk_table_attach (GTK_TABLE (table), dialog->config_entry,
                          1, 2, 2, 3,
                          GTK_FILL | GTK_EXPAND, 0,
                          0, 0);

        alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
        gtk_table_attach (GTK_TABLE (table), alignment,
                          0, 2, 3, 4,
                          GTK_FILL | GTK_EXPAND, 0,
                          0, 0);
        
        dialog->sm_toggle = gtk_check_button_new_with_label (_("Window manager is session managed"));
        gtk_container_add (GTK_CONTAINER (alignment), dialog->sm_toggle);

        gtk_window_set_default_size (GTK_WINDOW (dialog->dialog), 400, -1);
        gtk_window_set_policy (GTK_WINDOW (dialog->dialog), FALSE, TRUE, FALSE);
        gtk_widget_show_all (dialog->dialog);

        return dialog;
}

static gchar *
extract_entry (GtkWidget *widget)
{
        gchar *tmp;
        
        g_return_val_if_fail (GTK_IS_ENTRY (widget), NULL);

        tmp = gtk_entry_get_text (GTK_ENTRY (widget));
        if (is_blank (tmp))
                return NULL;
        else
                return g_strdup (tmp);
}

static gchar *
make_filename (gchar *name)
{
        gchar *tempname = g_strconcat (name, ".desktop", NULL);
        gchar *tempdir = gnome_util_home_file("wm-properties/");
        gchar *tmp = tempname;
        gchar *result;

        while (*tmp) {
                if (isspace (*tmp) || (*tmp == '/'))
                        *tmp = '_';
                tmp++;
        }
        result = g_concat_dir_and_file (tempdir, tempname);
        g_free (tempname);
        g_free (tempdir);

        return result;
}

static gboolean
check_dialog (WMDialog *dialog)
{
        GtkWidget *msgbox;
        
        if (is_blank (gtk_entry_get_text (GTK_ENTRY (dialog->name_entry)))) {
                msgbox = gnome_message_box_new (_("Name cannot be empty"),
                                                GNOME_MESSAGE_BOX_ERROR,
                                                _("OK"), NULL);
                gnome_dialog_run (GNOME_DIALOG (msgbox));
                return FALSE;
        }
        if (is_blank (gtk_entry_get_text (GTK_ENTRY (dialog->exec_entry)))) {
                msgbox = gnome_message_box_new (_("Command cannot be empty"),
                                                GNOME_MESSAGE_BOX_ERROR,
                                                _("OK"), NULL);
                gnome_dialog_run (GNOME_DIALOG (msgbox));
                return FALSE;
        }
                
        return TRUE;
}

static void
get_dialog_contents (WMDialog *dialog, WindowManager *wm)
{
        gchar *tmp;
        
        if (wm->dentry->name)
                g_free (wm->dentry->name);
        wm->dentry->name = extract_entry (dialog->name_entry);

        if (wm->dentry->exec)
                g_strfreev (wm->dentry->exec);
        tmp = extract_entry (dialog->exec_entry);
        gnome_config_make_vector (tmp, &wm->dentry->exec_length,
                                  &wm->dentry->exec);
        g_free (tmp);
        
        if (wm->config_exec)
                g_free (wm->config_exec);
        wm->config_exec = extract_entry (dialog->config_entry);

        if (wm->dentry->location)
                g_free (wm->dentry->location);
        wm->dentry->location = make_filename (wm->dentry->name);

        wm->session_managed = !!GTK_TOGGLE_BUTTON (dialog->sm_toggle)->active;

        wm_check_present (wm);
}

static void
edit_dialog (void)
{
        WMDialog *dialog;
        gchar *tmp;
        gint result;

        if(!selected_wm)
                return;

        dialog = create_dialog (_("Edit Window Manager"));

        if (selected_wm->dentry->name)
                gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), selected_wm->dentry->name);
                
        if (selected_wm->dentry->exec) {
                tmp = gnome_config_assemble_vector (selected_wm->dentry->exec_length,
                                                    (const char **)selected_wm->dentry->exec);
                gtk_entry_set_text (GTK_ENTRY (dialog->exec_entry), tmp);
                g_free (tmp);
        }
        
        if (selected_wm->config_exec)
                gtk_entry_set_text (GTK_ENTRY (dialog->config_entry), selected_wm->config_exec);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->sm_toggle),
                                      selected_wm->session_managed);

        if (!selected_wm->is_user) {
                gtk_widget_set_sensitive (dialog->name_entry, FALSE);
                gtk_widget_set_sensitive (dialog->exec_entry, FALSE);
                gtk_widget_set_sensitive (dialog->config_entry, FALSE);
                gtk_widget_set_sensitive (dialog->sm_toggle, FALSE);
        }
                
        do {
                gtk_widget_show (dialog->dialog);
                result = gnome_dialog_run (GNOME_DIALOG (dialog->dialog));
        } while (result == 0 && !check_dialog (dialog));

        if (selected_wm->is_user && (result == 0)) {
                get_dialog_contents (dialog, selected_wm);
                update_gui();
                capplet_widget_state_changed (CAPPLET_WIDGET (capplet), TRUE);
        }

        gtk_widget_destroy (dialog->dialog);
        g_free (dialog);
}

static void
add_dialog (void)
{
        WMDialog *dialog = create_dialog (_("Edit Window Manager"));
        WindowManager *wm;
        gint result;

        do {
                result = gnome_dialog_run (GNOME_DIALOG (dialog->dialog));
        } while (result == 0 && !check_dialog (dialog));

        if (result == 0) {
                wm = g_new0 (WindowManager, 1);
                wm->dentry = g_new0 (GnomeDesktopEntry, 1);
                get_dialog_contents (dialog, wm);

                wm->is_user = TRUE;
                
                wm_list_add (wm);

                selected_wm = wm;
                update_gui();
                
                capplet_widget_state_changed (CAPPLET_WIDGET (capplet), TRUE);
        }

        gtk_widget_destroy (dialog->dialog);
        g_free (dialog);
}

static void 
select_row (GtkCList   *the_clist,
            gint        row,
            gint        column,
            GdkEvent   *event,
            gpointer    data)
{
        WindowManager *wm;

        if (!in_fill) {
                wm = gtk_clist_get_row_data (GTK_CLIST (clist), row);
                gtk_widget_set_sensitive (edit_button, wm->is_user);
                gtk_widget_set_sensitive (delete_button, wm->is_user);
                
                if (wm != selected_wm) {
                        selected_wm = wm;
                        capplet_widget_state_changed (CAPPLET_WIDGET (capplet), TRUE);
                }
        }
}

static void
delete (void)
{
        WindowManager *current_wm = wm_list_get_current();
        GtkWidget *msgbox;
        
        if (current_wm == selected_wm) {
                msgbox = gnome_message_box_new (
                   _("You cannot delete the current Window Manager"),
                   GNOME_MESSAGE_BOX_ERROR, _("OK"), NULL);

                gnome_dialog_run (GNOME_DIALOG (msgbox));
                return;
        }

        wm_list_delete (selected_wm);
        selected_wm = current_wm;
        update_gui();
        capplet_widget_state_changed (CAPPLET_WIDGET (capplet), TRUE);
}


static void
run_config (GtkWidget *w)
{
        WindowManager *current_wm = wm_list_get_current();

        if (current_wm
            && current_wm->is_config_present
            && current_wm->config_exec != NULL) {
                gchar *argv[4];

                argv[0] = "/bin/sh";
                argv[1] = "-c";
                argv[2] = current_wm->config_exec;
                argv[3] = NULL;
                
                gnome_execute_async (NULL, 4, argv);
        }
}

static void
wm_setup (void)
{
        GtkWidget *hbox, *vbox, *bottom;
        GtkWidget *util_vbox;
        GtkWidget *add_button;
        GtkWidget *scrolled_window;

        capplet = capplet_widget_new ();
        vbox = gtk_vbox_new (FALSE, 0);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD);

	gtk_container_set_border_width (GTK_CONTAINER (hbox), GNOME_PAD_SMALL);
	bottom = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (bottom), GNOME_PAD_SMALL);
        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        
        clist = gtk_clist_new (1);
        gtk_clist_column_titles_hide (GTK_CLIST (clist));
        gtk_clist_set_column_auto_resize (GTK_CLIST (clist), 0, TRUE);
        gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);

        gtk_signal_connect (GTK_OBJECT (clist), "select_row",
                            GTK_SIGNAL_FUNC (select_row), NULL);
        
        gtk_container_add (GTK_CONTAINER (scrolled_window), clist);

        gtk_box_pack_start (GTK_BOX (hbox), scrolled_window, TRUE, TRUE, 0);

        util_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (hbox), util_vbox, FALSE, FALSE, 0);
        
        add_button = left_aligned_button (_("Add..."));
        gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
                            GTK_SIGNAL_FUNC (add_dialog), NULL);
        gtk_box_pack_start (GTK_BOX (util_vbox), add_button, FALSE, FALSE, 0);

        edit_button = left_aligned_button (_("Edit..."));
        gtk_signal_connect (GTK_OBJECT (edit_button), "clicked",
                            GTK_SIGNAL_FUNC (edit_dialog), NULL);
        gtk_box_pack_start (GTK_BOX (util_vbox), edit_button, FALSE, FALSE, 0);

        delete_button = left_aligned_button (_("Delete"));
        gtk_signal_connect (GTK_OBJECT (delete_button), "clicked",
                            GTK_SIGNAL_FUNC (delete), NULL);
        gtk_box_pack_start (GTK_BOX (util_vbox), delete_button, FALSE, FALSE, 0);
        config_button = gtk_button_new_with_label ("");
                                              
        gtk_misc_set_padding (GTK_MISC (GTK_BIN (config_button)->child),
                              GNOME_PAD_SMALL, 0);
        gtk_signal_connect (GTK_OBJECT (config_button), "clicked",
                            GTK_SIGNAL_FUNC (run_config), NULL);
        gtk_box_pack_start (GTK_BOX (bottom), config_button, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_box_pack_end (GTK_BOX (vbox), bottom, FALSE, FALSE, 0);

        gtk_container_add (GTK_CONTAINER (capplet), vbox);

        gtk_widget_show_all (capplet);

        update_gui();
}

int
main (int argc, char **argv)
{
        gint init_results;

				setlocale(LC_ALL, "");
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

        argv0 = g_strdup (argv[0]);
	init_results = gnome_capplet_init("wm-properties", VERSION,
					  argc, argv, NULL, 0, NULL);
        gnome_window_icon_set_default_from_file (GNOME_ICONDIR"/gnome-ccwindowmanager.png");
	if (init_results < 0) {
                g_warning (_("an initialization error occurred while "
                             "starting 'wm-properties-capplet'.\n"
                             "aborting...\n"));
                exit (1);
	}

        /* Read in the list of window managers, and the current
         * window manager
         */
        wm_list_init();
        selected_wm = wm_list_get_current();

	if (init_results == 0) {
                init_session();
                wm_setup();
                gtk_signal_connect(GTK_OBJECT(capplet), "destroy", 
                                   GTK_SIGNAL_FUNC(destroy_callback), NULL);
                gtk_signal_connect (GTK_OBJECT (capplet), "help", 
                                    GTK_SIGNAL_FUNC (help_callback), NULL);
                gtk_signal_connect (GTK_OBJECT (capplet), "try", 
                                    GTK_SIGNAL_FUNC (try_callback), NULL);
                gtk_signal_connect (GTK_OBJECT (capplet), "revert", 
                                    GTK_SIGNAL_FUNC (revert_callback), NULL);
                gtk_signal_connect (GTK_OBJECT (capplet), "cancel", 
                                    GTK_SIGNAL_FUNC (cancel_callback), NULL);
                gtk_signal_connect (GTK_OBJECT (capplet), "ok", 
                                    GTK_SIGNAL_FUNC (ok_callback), NULL);
        
	        capplet_gtk_main ();

                if (restart_pending) {
                        quit_pending = TRUE;
                        gtk_main();
                }

        } else {
                if (selected_wm && 
                    !selected_wm->session_managed && 
                    !wm_is_running()) {

                        wm_restart (selected_wm, NULL, init_callback, 
                                    g_strdup (selected_wm->dentry->name));
                        gtk_main ();
                }

                init_session();
        }

        return 0;
}

