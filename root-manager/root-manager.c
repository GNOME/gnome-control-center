/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
 * Copyright 2001 Ximian Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* 
 * UserTool suid helper program
 */

#if 0
#define USE_MCHECK 1
#endif

#define DEBUG_USERHELPER 1
#define ROOT_MANAGER     1

#include "config.h"

#include <assert.h>
#include <errno.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef USE_MCHECK
#include <mcheck.h>
#endif

#include <locale.h>
#include <libintl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "root-manager.h"
#include "shvar.h"

#define _(s) gettext(s)

/* Total GECOS field length... is this enough ? */
#define GECOS_LENGTH		80

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


/* ------ some static data objects ------- */

static char *full_name	= NULL; /* full user name */
static char *office	= NULL; /* office */
static char *office_ph	= NULL;	/* office phone */
static char *home_ph	= NULL;	/* home phone */
static char *user_name	= NULL; /* the account name */
static char *shell_path = NULL; /* shell path */

static char *the_username = NULL; /* used to mangle the conversation function */

/* manipulate the environment directly */
extern char **environ;

/* command line flags */
static int 	f_flg = 0; 	/* -f flag = change full name */
static int	o_flg = 0;	/* -o flag = change office name */
static int	p_flg = 0;	/* -p flag = change office phone */
static int 	h_flg = 0;	/* -h flag = change home phone number */
static int 	c_flg = 0;	/* -c flag = change password */
static int 	s_flg = 0;	/* -s flag = change shell */
static int 	t_flg = 0;	/* -t flag = direct text-mode -- exec'ed */
static int 	w_flg = 0;	/* -w flag = act as a wrapper for next args */
static int	d_flg = 0;      /* -d flag = three descriptor numbers for us */

/*
 * A handy fail exit function we can call from many places
 */
static int fail_error(int retval)
{
  /* this is a temporary kludge.. will be fixed shortly. */
    if(retval == ERR_SHELL_INVALID)
        exit(ERR_SHELL_INVALID);	  

    if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
	g_print(_("Got error %d.\n"), retval);
#endif
	switch(retval) {
	    case PAM_AUTH_ERR:
	    case PAM_PERM_DENIED:
		exit (ERR_PASSWD_INVALID);
	    case PAM_AUTHTOK_LOCK_BUSY:
		exit (ERR_LOCKS);
	    case PAM_CRED_INSUFFICIENT:
	    case PAM_AUTHINFO_UNAVAIL:
		exit (ERR_NO_RIGHTS);
	    case PAM_ABORT:
	    default:
		exit(ERR_UNK_ERROR);
	}
    }
    exit (0);
}

/*
 * Read a string from stdin, returns a malloced copy of it
 */
static char *read_string(void)
{
    char *buffer = NULL;
    char *check = NULL;
    int slen = 0;
    
    buffer = g_malloc(BUFSIZ);
    if (buffer == NULL)
	return NULL;
    
    check = fgets(buffer, BUFSIZ, stdin);
    if (!check)
	return NULL;
    slen = strlen(buffer);
    if((slen > 0) && ((buffer[slen - 1] == '\n') || isspace(buffer[slen - 1]))){
        buffer[slen-1] = '\0';
    }
    if(buffer[0] == UH_TEXT) {
        memmove(buffer, buffer + 1, BUFSIZ - 1);
    }
    return buffer;
}

/* Application data with some hints. */
static struct app_data_t {
     int fallback;
     char *user;
     char *service;
} app_data = {0, NULL, NULL};
static gboolean fallback_flag = FALSE;

/*
 * Conversation function for the boring change password stuff
 */
static int conv_func(int num_msg, const struct pam_message **msg,
		     struct pam_response **resp, void *appdata_ptr)
{
    int count = 0;
    int responses = 0;
    struct pam_response *reply = NULL;
    char *noecho_message;

    reply = (struct pam_response *)
	calloc(num_msg, sizeof(struct pam_response));
    if (reply == NULL)
	return PAM_CONV_ERR;
 
    if(appdata_ptr != NULL) {
        struct app_data_t *app_data = (struct app_data_t*) appdata_ptr;

        g_print("%d %d\n", UH_FALLBACK, app_data->fallback);
	if(app_data->user == NULL) {
            g_print("%d %s\n", UH_USER, "root");
	} else {
            g_print("%d %s\n", UH_USER, app_data->user);
	}
	if(app_data->service != NULL) {
            g_print("%d %s\n", UH_SERVICE_NAME, app_data->service);
	}
    }

    /*
     * We do first a pass on all items and output them;
     * then we do a second pass and read what we have to read
     * from stdin
     */
    for (count = responses = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		g_print("%d %s\n", UH_ECHO_ON_PROMPT, msg[count]->msg);
		responses++;
		break;
	    case PAM_PROMPT_ECHO_OFF:
		if (the_username && !strncasecmp(msg[count]->msg, "password", 8)) {
		    noecho_message = g_strdup_printf(_("Password for %s"),
                                                     the_username);
		} else {
		    noecho_message = g_strdup(msg[count]->msg);
		}
		g_print("%d %s\n", UH_ECHO_OFF_PROMPT, noecho_message);
		g_free(noecho_message);
		responses++;
		break;
	    case PAM_TEXT_INFO:
		g_print("%d %s\n", UH_INFO_MSG, msg[count]->msg);
		break;
	    case PAM_ERROR_MSG:
		g_print("%d %s\n", UH_ERROR_MSG, msg[count]->msg);
		break;
	    default:
		g_print("0 %s\n", msg[count]->msg);
	}
    }

    /* tell the other side how many messages we expect responses for */
    g_print("%d %d\n", UH_EXPECT_RESP, responses);
    fflush(NULL);

    /* now the second pass */
    for (count = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	    case PAM_TEXT_INFO:
		/* ignore it... */
		break;
	    case PAM_ERROR_MSG:
		/* also ignore it... */
		break;
	    case PAM_PROMPT_ECHO_ON:
		/* fall through */
	    case PAM_PROMPT_ECHO_OFF:
		reply[count].resp_retcode = PAM_SUCCESS;
		reply[count].resp = read_string();
		if((reply[count].resp != NULL) &&
		   (reply[count].resp[0] == UH_ABORT)) {
                    fallback_flag = TRUE;
		    free (reply);
		    return PAM_MAXTRIES; /* Shrug. */
		}
		break;
	    default:
		/* Must be an error of some sort... */
		free (reply);
		return PAM_CONV_ERR;
	}
    }
    if (reply)
	*resp = reply;
    return PAM_SUCCESS;
}

/*
 * the structure pointing at the conversation function for
 * auth and changing the password
 */
static struct pam_conv pipe_conv = {
     conv_func,
     &app_data,
};
static struct pam_conv text_conv = {
     misc_conv,
     &app_data,
};
    
/*
 * A function to process already existing gecos information
 */
static void process_gecos(char *gecos)
{
    char *idx;
    
    if (gecos == NULL)
	return;

    if (!full_name)
	full_name = gecos;    
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
	gecos = idx+1;
    }
    if ((idx == NULL) || (*gecos == '\0')) {
	/* no more fields */
	return;
    }

    if (!office)
	office = gecos;    
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
	gecos = idx+1;
    }

    if (!office_ph)
	office_ph = gecos;
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
	gecos = idx+1;
    }
    if ((idx == NULL) || (*gecos == '\0')) {
	/* no more fields */
	return;
    }
    
    if (!home_ph)
	home_ph = gecos;
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
    }
}

/*
 * invalid_field - insure that a field contains all legal characters
 *
 * The supplied field is scanned for non-printing and other illegal
 * characters.  If any illegal characters are found, invalid_field
 * returns -1.  Zero is returned for success.
 */

int invalid_field(const char *field, const char *illegal)
{
    const char *cp;

    for (cp = field; *cp && isprint (*cp) && ! strchr (illegal, *cp); cp++)
	;
    if (*cp)
	return -1;
    else
	return 0;
}

/*
 * A simple function to compute the gecos field size
 */
static int gecos_size(void)
{
    int len = 0;
    
    if (full_name != NULL)
	len += strlen(full_name);
    if (office != NULL)
	len += strlen(office);
    if (office_ph != NULL)
	len += strlen(office_ph);
    if (home_ph != NULL)
	len += strlen(home_ph);
    return len;
}

/* Snagged straight from the util-linux source... May want to clean up
 * a bit and possibly merge with the code in userinfo that parses to
 * get a list.  -Otto
 *
 *  get_shell_list () -- if the given shell appears in the list of valid shells,
 *      return true.  if not, return false.
 *      if the given shell is NULL, the list of shells is outputted to stdout.
 */
static int get_shell_list(char* shell_name)
{
    gboolean found;
    char *shell;

    found = FALSE;
    setusershell();
    for(shell = getusershell(); shell != NULL; shell = getusershell()) {
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "got shell \"%s\"\n", shell);
#endif
        if (shell_name) {
            if (! strcmp (shell_name, shell)) {
	        found = TRUE;
                break;
            }
        }
        else g_print("%s\n", shell);
    }
    endusershell();
    return found;
}

#ifdef USE_MCHECK
void
mcheck_out(enum mcheck_status reason) {
    char *explanation;

    switch (reason) {
	case MCHECK_DISABLED:
	    explanation = _("Consistency checking is not turned on."); break;
	case MCHECK_OK:
	    explanation = _("Block is fine."); break;
	case MCHECK_FREE:
	    explanation = _("Block freed twice."); break;
	case MCHECK_HEAD:
	    explanation = _("Memory before the block was clobbered."); break;
	case MCHECK_TAIL:
	    explanation = _("Memory after the block was clobbered."); break;
    }
    g_print("%d %s\n", UH_ERROR_MSG, explanation);
    g_print("%d 1\n", UH_EXPECT_RESP);
}
#endif

/* ------- the application itself -------- */
int main(int argc, char *argv[])
{
    int		arg;
    int 	retval;
    char	*progname = NULL;
    pam_handle_t 	*pamh = NULL;
    struct passwd	*pw;
    struct pam_conv     *conv;
    int		stdin_fileno = -1, stdout_fileno = -1, stderr_fileno = -1;
    int fd;
    FILE *fp;

#ifdef USE_MCHECK
    mtrace();
    mcheck(mcheck_out);
#endif

    bindtextdomain (PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);

    if (geteuid() != 0) {
	fprintf(stderr, _("userhelper must be setuid root\n"));
	exit(ERR_NO_RIGHTS);
    }

    if (argc < 2) {
	    fprintf(stderr, _("Usage: root-helper fd\n"));
	    exit(ERR_INVALID_CALL);
    }

    fd = atoi (argv[1]);
    if (fd <= STDERR_FILENO) {
	    fprintf (stderr, _("Usage: root-helper fd\n"));
	    exit(ERR_INVALID_CALL);
    }
    
#ifdef DEBUG_USERHELPER
    fprintf (stderr, "fd is %d\n", fd);
#endif
    fp = fdopen (fd, "r");
	
    conv = &pipe_conv;

    /* now try to identify the username we are doing all this work for */
    user_name = getlogin();
    if (user_name == NULL) {
	struct passwd *tmp;
	
	tmp = getpwuid(getuid());
        if ((tmp != NULL) && (tmp->pw_name != NULL)) {
	    user_name = g_strdup(tmp->pw_name);
	} else {
            /* weirdo, bail out */
	    exit (ERR_UNK_ERROR);
	}
    }
#ifdef DEBUG_USERHELPER
    fprintf(stderr, "user is %s\n", user_name);
#endif

    {
	/* pick the first existing of /usr/sbin/<progname> and /sbin/<progname>
	 * authenticate <progname>
	 * argv[optind-1] = <progname> (boondoggle unless -- used)
	 * (we know there is at least one open slot in argv for this)
	 * execv(constructed_path, argv+optind);
	 */
	char *constructed_path;
	char *apps_filename;
	char *user, *apps_user;
	char *retry;
	char *env_home, *env_term, *env_display, *env_shell;
        char *env_user, *env_logname, *env_lang, *env_lcall, *env_lcmsgs;
	char *env_xauthority;
	int session, fallback, try;
	size_t aft;
	struct stat sbuf;
	shvarFile *s;

	env_home = getenv("HOME");
	env_term = getenv("TERM");
	env_display = getenv("DISPLAY");
	env_shell = getenv("SHELL");
	env_user = getenv("USER");
	env_logname = getenv("LOGNAME");
        env_lang = getenv("LANG");
        env_lcall = getenv("LC_ALL");
        env_lcmsgs = getenv("LC_MESSAGES");
        env_xauthority = getenv("XAUTHORITY");

	if (env_home && (strstr(env_home, "..") || strchr(env_home, '%')))
	    env_home=NULL;
	if (env_shell && (strstr(env_shell, "..") || strchr(env_shell, '%')))
	    env_shell=NULL;
	if (env_term && (strstr(env_term, "..") || strchr(env_term, '%')))
	    env_term="dumb";
	if (env_lang && (strchr(env_lang, '/') || strchr(env_lang, '%')))
	    env_lang=NULL;
	if (env_lcall && (strchr(env_lcall, '/') || strchr(env_lcall, '%')))
	    env_lcall=NULL;
	if (env_lcmsgs && (strchr(env_lcmsgs, '/') || strchr(env_lcmsgs, '%')))
	    env_lcmsgs=NULL;

	environ = (char **) calloc (1, 2 * sizeof (char *));
	/* note that XAUTHORITY not copied -- do not let attackers get at
	 * others' X authority records
	 */
	if (env_home) setenv("HOME", env_home, 1);
	if (env_term) setenv("TERM", env_term, 1);
	if (env_display) setenv("DISPLAY", env_display, 1);
	if (env_shell) setenv("SHELL", env_shell, 1);
	if (env_user) setenv("USER", env_user, 1);
	if (env_logname) setenv("LOGNAME", env_logname, 1);
        /* we want _, but only if it is safe */
        if (env_lang) setenv("LANG", env_lang, 1);
        if (env_lcall) setenv("LC_ALL", env_lcall, 1);
        if (env_lcmsgs) setenv("LC_MESSAGES", env_lcmsgs, 1);

	setenv("PATH",
	       "/usr/sbin:/usr/bin:/sbin:/bin:/usr/X11R6/bin:/root/bin", 1);

	progname = "root-manager";
	user = "root";
	constructed_path = "cat";

	retval = pam_start(progname, user, conv, &pamh);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

	session = TRUE;
	app_data.fallback = fallback = FALSE;
	app_data.user = user;
	app_data.service = progname;

	do {
#ifdef DEBUG_USERHELPER
	    fprintf(stderr, _("PAM returned = %d\n"), retval);
	    fprintf(stderr, _("about to authenticate \"%s\"\n"), user);
#endif
	    retval = pam_authenticate(pamh, 0);
	} while (--try && retval != PAM_SUCCESS && !fallback_flag);
	if (retval != PAM_SUCCESS) {
	    pam_end(pamh, retval);
	    if (fallback) {
		    g_assert_not_reached ();
		setuid(getuid());
		if(geteuid() != getuid()) {
		    exit (ERR_EXEC_FAILED);
		}
		argv[optind-1] = progname;
		if(d_flg) {
		    dup2(stdin_fileno, STDIN_FILENO);
		    dup2(stdout_fileno, STDOUT_FILENO);
		    dup2(stderr_fileno, STDERR_FILENO);
		}
		execv(constructed_path, argv+optind-1);
		exit (ERR_EXEC_FAILED);
	    } else {
		fail_error(retval);
	    }
	}

	retval = pam_acct_mgmt(pamh, 0);
	if (retval != PAM_SUCCESS) {
	    pam_end(pamh, retval);
	    fail_error(retval);
	}

	/* reset the XAUTHORITY so that X stuff will work now */
        if (env_xauthority) setenv("XAUTHORITY", env_xauthority, 1);

	if (session) {
	    int child, status;

	    retval = pam_open_session(pamh, 0);
	    if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		fail_error(retval);
	    }

	    if ((child = fork()) == 0) {
                struct passwd *pw;
		setuid(0);
		pw = getpwuid(getuid());
		if (pw) setenv("HOME", pw->pw_dir, 1);

#define BUFSIZE 1024
		{
			char buf[BUFSIZE];
			fprintf (stdout, "success\n");
#ifdef DEBUG_USERHELPER
			fprintf (stderr, "looping for input\n");
#endif
			
			while (fgets (buf, BUFSIZE, fp)) {
				if (!fork ()) {
					char **args;
					buf[strlen (buf) - 1] = '\0';
					args = g_strsplit (buf, " ", -1);
#ifdef DEBUG_USERHELPER
					fprintf (stderr, "running: %s\n", args[0]);
#endif
					
					setenv("PATH",
					       "/usr/sbin:/usr/bin:/sbin:/bin:/usr/X11R6/bin:/root/bin", 1);
					
					execv (args[0], args);
					g_error ("%s", g_strerror (errno));
				}
			}
		}	    
		
		argv[optind-1] = progname;
#ifdef DEBUG_USERHELPER
		g_print(_("about to exec \"%s\"\n"), constructed_path);
#endif
		if(d_flg) {
		    dup2(stdin_fileno, STDIN_FILENO);
		    dup2(stdout_fileno, STDOUT_FILENO);
		    dup2(stderr_fileno, STDERR_FILENO);
		}
		execv(constructed_path, argv+optind-1);
		exit (ERR_EXEC_FAILED);
	    }

	    close(STDIN_FILENO);
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);

	    wait4 (child, &status, 0, NULL);

	    retval = pam_close_session(pamh, 0);
	    if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		fail_error(retval);
	    }

	    if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
		pam_end(pamh, PAM_SUCCESS);
		retval = 1;
	    } else {
		pam_end(pamh, PAM_SUCCESS);
		retval = ERR_EXEC_FAILED;
	    }
	    exit (retval);

	} else {
	    /* this is not a session, so do not do session management */

	    pam_end(pamh, PAM_SUCCESS);
	    
	    
#define BUFSIZE 1024
	    {
		    char buf[BUFSIZE];
#ifdef DEBUG_USERHELPER
		    fprintf (stderr, "looping for input\n");
#endif

		    while (fgets (buf, BUFSIZE, fp)) {
			    if (!fork ()) {
				    char **args;
				    buf[strlen (buf) - 1] = '\0';
				    args = g_strsplit (buf, " ", -1);
#ifdef DEBUG_USERHELPER
				    fprintf (stderr, "running: %s\n", args[0]);
#endif

				    setenv("PATH",
					   "/usr/sbin:/usr/bin:/sbin:/bin:/usr/X11R6/bin:/root/bin", 1);

				    execv (args[0], args);
				    g_error ("%s", g_strerror (errno));
			    }
		    }
	    }	    


	    /* time for an exec */
	    setuid(0);
	    argv[optind-1] = progname;
#ifdef DEBUG_USERHELPER
            g_print(_("about to exec \"%s\"\n"), constructed_path);
#endif
	    if(d_flg) {
	        dup2(stdin_fileno, STDIN_FILENO);
	        dup2(stdout_fileno, STDOUT_FILENO);
	        dup2(stderr_fileno, STDERR_FILENO);
	    }
	    execv(constructed_path, argv+optind-1);

	    exit (ERR_EXEC_FAILED);
	}

    }

    /* all done */     
    if (pamh != NULL)
	retval = pam_end(pamh, PAM_SUCCESS);
    if (retval != PAM_SUCCESS)
	fail_error(retval);
    exit (0);
}
