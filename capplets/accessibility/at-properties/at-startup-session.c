#include <config.h>
#include <string.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>

#include "at-startup-session.h"

#define AT_STARTUP_KEY     "/desktop/gnome/accessibility/startup/exec_ats"

#define GNOPERNICUS_MAGNIFIER_KEY  "/apps/gnopernicus/srcore/mag_active"
#define GNOPERNICUS_SPEECH_KEY     "/apps/gnopernicus/srcore/sp_active"
#define GNOPERNICUS_BRAILLE_KEY    "/apps/gnopernicus/srcore/br_active"

static AtStartupState at_startup_state_recent;

static GSList *
at_startup_get_list (GConfClient *client)
{
	GError *error = NULL;
	GSList *at_list = gconf_client_get_list (client, AT_STARTUP_KEY, GCONF_VALUE_STRING, &error);
	if (error) {
		g_warning ("Error getting value of " AT_STARTUP_KEY ": %s", error->message);
		g_error_free (error);
		return NULL;
	}
	return at_list;
}

gint
at_startup_string_compare (gconstpointer s1, gconstpointer s2)
{
	if (s1 && s2) {
		return strcmp (s1, s2);
	}
	else
		return ((char *)s2-(char *)s1);
}

static GSList *
at_startup_list_add (GSList *list, const gchar *exec_name) 
{
	GSList *l = g_slist_find_custom (list, exec_name, at_startup_string_compare);
	if (!l) {
		list = g_slist_append (list, g_strdup (exec_name));
	}
	return list;
}

static GSList *
at_startup_list_remove (GSList *list, const gchar *exec_name) 
{
	GSList *l = g_slist_find_custom (list, exec_name, at_startup_string_compare);
	if (l) {
		g_free (l->data);
		list = g_slist_delete_link (list, l);
	}
	return list;
}

void
at_startup_state_init (AtStartupState *startup_state)
{
	gboolean        mag_active, speech_active, braille_active;
	GSList         *l;
	GConfClient    *client = gconf_client_get_default ();
	GSList         *at_list = at_startup_get_list (client);
	gchar          *prog;

	for (l = at_list; l; l = l->next) {
		gchar *exec_name = (char *) l->data;
		if (exec_name && !strcmp (exec_name, "gnopernicus")) {
			braille_active = gconf_client_get_bool (client, 
								GNOPERNICUS_BRAILLE_KEY, 
								NULL);
			mag_active = gconf_client_get_bool (client, 
							    GNOPERNICUS_MAGNIFIER_KEY, 
							    NULL);
			speech_active = gconf_client_get_bool (client, 
							       GNOPERNICUS_SPEECH_KEY, 
							       NULL);

			startup_state->enabled.screenreader = (braille_active || speech_active);
			startup_state->enabled.magnifier = mag_active;
		}
		else if (exec_name && !strcmp(exec_name, "gok")) {
			startup_state->enabled.osk = TRUE;
		}
		g_free (exec_name);
	}

	g_slist_free (at_list);
	g_object_unref (client);
	at_startup_state_recent.flags = startup_state->flags;

	prog = g_find_program_in_path ("gok");
	if (prog != NULL) {
		startup_state->enabled.osk_installed = TRUE;
		g_free (prog);
	} else {
		startup_state->enabled.osk_installed = FALSE;
	}

	prog = g_find_program_in_path ("gnopernicus");
	if (prog != NULL) {
		startup_state->enabled.magnifier_installed = TRUE;
		startup_state->enabled.screenreader_installed = TRUE;
		g_free (prog);
	} else {
		startup_state->enabled.magnifier_installed = FALSE;
		startup_state->enabled.screenreader_installed = FALSE;
	}
}

void
at_startup_state_update (AtStartupState *startup_state)
{
	GError      *error = NULL;
	GConfClient *client = gconf_client_get_default ();
	GSList      *at_list = at_startup_get_list (client);

	if (startup_state->enabled.screenreader != at_startup_state_recent.enabled.screenreader) {
		gconf_client_set_bool (client, GNOPERNICUS_SPEECH_KEY, 
				       startup_state->enabled.screenreader, NULL);
		gconf_client_set_bool (client, GNOPERNICUS_BRAILLE_KEY, 
				       startup_state->enabled.screenreader, NULL);
	}

	if (startup_state->enabled.magnifier != at_startup_state_recent.enabled.magnifier) {
		gconf_client_set_bool (client, GNOPERNICUS_MAGNIFIER_KEY, 
				       startup_state->enabled.magnifier, NULL);
	}

	if (startup_state->enabled.screenreader || startup_state->enabled.magnifier) {
		if (!(at_startup_state_recent.enabled.screenreader || 
		      at_startup_state_recent.enabled.magnifier))
			/* new state includes SR or magnifier, initial one did not */
			at_list = at_startup_list_add (at_list, "gnopernicus");
	}
	else {
		if (at_startup_state_recent.enabled.screenreader || 
		    at_startup_state_recent.enabled.magnifier)
			at_list = at_startup_list_remove (at_list, "gnopernicus");
	}
	if (startup_state->enabled.osk) {
		if (!at_startup_state_recent.enabled.osk)
			at_list = at_startup_list_add (at_list, "gok");
	}
	else {
		if (at_startup_state_recent.enabled.osk)
			at_list = at_startup_list_remove (at_list, "gok");
	}
	if (at_startup_state_recent.flags != startup_state->flags) {
		at_startup_state_recent.flags = startup_state->flags;
		gconf_client_set_list (client, AT_STARTUP_KEY, GCONF_VALUE_STRING, at_list, &error);
	}
	g_object_unref (client);
	g_slist_free (at_list);
}


