/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc.
 * Code available under the Gnu GPL.
 * Authors: Owen Taylor <otaylor@redhat.com>,
 *          Bradford Hovinen <hovinen@helixcode.com>
 */

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <libgnome/libgnome.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "wm-properties.h"


#define CONFIG_PREFIX "/desktop/gnome/applications/window_manager"

/* Current list of window managers */
GList *window_managers = NULL;

/* List on startup */
static GList *window_managers_save = NULL;

/* Current window manager */
static WindowManager *current_wm = NULL;

/* Window manager on startup */
static WindowManager *current_wm_save = NULL;

static gboolean   xml_read_bool          (xmlNodePtr node);
static xmlNodePtr xml_write_bool         (gchar *name,
					  gboolean value);

gboolean
is_blank (gchar *str)
{
        while (*str) {
                if (!isspace(*str))
                        return FALSE;
                str++;
        }
        return TRUE;
}

static gint
wm_compare (gconstpointer a, gconstpointer b)
{
        const WindowManager *wm_a = (const WindowManager *)a;
        const WindowManager *wm_b = (const WindowManager *)b;

        return g_strcasecmp (gnome_desktop_item_get_string (wm_a->dentry, GNOME_DESKTOP_ITEM_NAME), gnome_desktop_item_get_string (wm_b->dentry, GNOME_DESKTOP_ITEM_NAME));
}

static void
wm_free (WindowManager *wm)
{
        gnome_desktop_item_unref (wm->dentry);
        g_free (wm->config_exec);
        g_free (wm->config_tryexec);;
        g_free (wm);
}

void
wm_check_present (WindowManager *wm)
{
        gchar *path;

        if (gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_EXEC)) {
                if (gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_TRY_EXEC)) {
                        path = gnome_is_program_in_path (gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_TRY_EXEC));
                        wm->is_present = (path != NULL);
                        if (path)
                                g_free (path);
                } else
                        wm->is_present = TRUE;
        } else
                wm->is_present = FALSE;
        
        if (wm->config_exec) {
                if (wm->config_tryexec) {
                        path = gnome_is_program_in_path (wm->config_tryexec);
                        wm->is_config_present = (path != NULL);
                        if (path)
                                g_free (path);
                } else {
			path = gnome_is_program_in_path (wm->config_exec);
                        wm->is_config_present = (path != NULL);
                        if (path)
                                g_free (path);
		}
        } else
                wm->is_config_present = FALSE;
        
}

static WindowManager *
wm_copy (WindowManager *wm)
{
        WindowManager *result = g_new (WindowManager, 1);

        result->dentry = gnome_desktop_item_copy (wm->dentry);
        result->config_exec = g_strdup (wm->config_exec);
        result->config_tryexec = g_strdup (wm->config_tryexec);

        result->session_managed = wm->session_managed;
        result->is_user = wm->is_user;
        result->is_present = wm->is_present;
        result->is_config_present = wm->is_config_present;

        return result;
}


static WindowManager *
wm_list_find (GList *list, const char *name)
{
        GList *tmp_list = list;
        while (tmp_list) {
                WindowManager *wm = tmp_list->data;
                if (strcmp (gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_EXEC), name) == 0)
                        return wm;

                tmp_list = tmp_list->next;
        }
        
        return NULL;
}

static WindowManager *
wm_list_find_exec (GList *list, const char *name)
{
        GList *tmp_list = list;
        while (tmp_list) {
                WindowManager *wm = tmp_list->data;
		if (!gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_EXEC))
			continue;
                if (strcmp (gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_EXEC), name) == 0)
                        return wm;

                tmp_list = tmp_list->next;
        }
        
        return NULL;
}

static GList *
wm_list_find_files (gchar *directory)
{
        DIR *dir;
        struct dirent *child;
        GList *result = NULL;
        gchar *suffix;

        dir = opendir (directory);
        if (dir == NULL)
                return NULL;

        while ((child = readdir (dir)) != NULL) {
                /* Ignore files without .desktop suffix, and ignore
                 * .desktop files with no prefix
                 */
                suffix = child->d_name + strlen (child->d_name) - 8;
                /* strlen(".desktop") == 8 */

                if (suffix <= child->d_name || 
                    strcmp (suffix, ".desktop") != 0)
                        continue;
                
                result = g_list_prepend (result, 
                                         g_concat_dir_and_file (directory,
                                                                child->d_name));
        }
        closedir (dir);

        return result;
}

static void
wm_list_read_dir (gchar *directory, gboolean is_user)
{
        WindowManager *wm;
        GList *tmp_list;
        GList *files;
        gchar *prefix;

        files = wm_list_find_files (directory);

        tmp_list = files;
        while (tmp_list) {
                wm = g_new (WindowManager, 1);

                wm->dentry = gnome_desktop_item_new_from_file (tmp_list->data, GNOME_DESKTOP_ITEM_TYPE_APPLICATION, NULL);
		gnome_desktop_item_set_entry_type (wm->dentry, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);

                if (!wm->dentry) {
                        g_free (wm);
                        tmp_list = tmp_list->next;
                        continue;
                }

                prefix = g_strconcat ("=", gnome_desktop_item_get_location (wm->dentry), "=/Window Manager/", NULL);
                gnome_config_push_prefix (prefix);
                g_free (prefix);

                wm->config_exec = gnome_config_get_string ("ConfigExec");
                wm->config_tryexec = gnome_config_get_string ("ConfigTryExec");
                wm->session_managed = gnome_config_get_bool ("SessionManaged=0");
                wm->is_user = is_user;

                if (wm->config_exec && is_blank (wm->config_exec)) {
                        g_free (wm->config_exec);
                        wm->config_exec = NULL;
                }
                
		if (wm->config_tryexec && is_blank (wm->config_tryexec)) {
                        g_free (wm->config_tryexec);
                        wm->config_tryexec = NULL;
                }

                gnome_config_pop_prefix ();

                wm_check_present (wm);

                if (gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_NAME) && gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_EXEC) &&
                    (wm->is_user || wm->is_present)) {
                        window_managers = 
                                g_list_insert_sorted (window_managers, 
                                                      wm,
                                                      wm_compare);
                        window_managers_save = 
                                g_list_insert_sorted (window_managers_save, 
                                                      wm_copy (wm),
                                                      wm_compare);
                } else {
                        wm_free (wm);
                }


                tmp_list = tmp_list->next;
        }
        g_list_free (files);
}

void           
wm_list_init (void)
{
        gchar *tempdir;
        gchar *name;
	GConfClient *client;
	
        tempdir = gnome_unconditional_datadir_file ("gnome/wm-properties/");
        wm_list_read_dir (tempdir, FALSE);
        g_free (tempdir);
        
	tempdir = gnome_util_home_file("wm-properties/");
        wm_list_read_dir (tempdir, TRUE);
        g_free (tempdir);

	client = gconf_client_get_default ();
	name = gconf_client_get_string (client, CONFIG_PREFIX "/current", NULL);
        if (name) {
                current_wm = wm_list_find (window_managers, name);
                g_free (name);
        }

        if (!current_wm) {
		name = gconf_client_get_string (client, CONFIG_PREFIX "/default", NULL);

                if (name) {
                        current_wm = wm_list_find_exec (window_managers, name);
                        g_free (name);
                }
        }

        if (!current_wm) {
                gchar *wmfile, *prefix;

                wmfile = gnome_unconditional_datadir_file ("default.wm");
                prefix = g_strconcat ("=", wmfile, "=/Default/WM", NULL);
                name = gnome_config_get_string (prefix);

                g_free (wmfile);
                g_free (prefix);

                if (name) {
                        current_wm = wm_list_find_exec (window_managers, name);
                        g_free (name);
                }
        }

        if (!current_wm && window_managers) 
                current_wm = window_managers->data;

        if(current_wm)
                current_wm_save = wm_list_find (window_managers_save, gnome_desktop_item_get_string (current_wm->dentry, GNOME_DESKTOP_ITEM_NAME));

	g_object_unref (G_OBJECT (client));
}

void
wm_list_save (void)
{
        GList *old_files;
        GList *tmp_list;
        gchar *tempdir;
        gchar *prefix;
        WindowManager *wm;

        /* Clean out the current contents of .gnome/wm-desktops
         */

        tempdir = gnome_util_home_file("wm-properties/");
        old_files = wm_list_find_files (tempdir);
        g_free (tempdir);

        tmp_list = old_files;
        while (tmp_list) {
                prefix = g_strconcat ("=", tmp_list->data, "=", NULL);
                gnome_config_clean_file (prefix);
                gnome_config_sync_file (prefix);
                g_free (prefix);

                tmp_list = tmp_list->next;
        }
        g_list_free (old_files);
        

        /* Save the user's desktops
         */

        tmp_list = window_managers;
        while (tmp_list) {
                wm = tmp_list->data;

                if (wm->is_user) {
                        gnome_desktop_item_save (wm->dentry, NULL, TRUE, NULL);
                        
                        prefix = g_strconcat ("=", gnome_desktop_item_get_location (wm->dentry), "=/Window Manager/", NULL);
                        gnome_config_push_prefix (prefix);
                        g_free (prefix);

                        if (wm->config_exec)
                                gnome_config_set_string ("ConfigExec", wm->config_exec);
                        if (wm->config_tryexec)
                                gnome_config_set_string ("ConfigTryExec", wm->config_tryexec);
                        gnome_config_set_bool ("SessionManaged=0", wm->session_managed);
                        gnome_config_pop_prefix ();
                        
                }
                tmp_list = tmp_list->next;
        }

        /* Save the current window manager
         */
        if(current_wm)
	{
		GConfClient *client = gconf_client_get_default ();
		gconf_client_set_string (client, CONFIG_PREFIX "/current",
                                         gnome_desktop_item_get_string (current_wm->dentry, GNOME_DESKTOP_ITEM_EXEC),
					 NULL);
		g_object_unref (G_OBJECT (client));
	}

        gnome_config_sync ();
}

void
wm_list_revert (void)
{
        GList *tmp_list;
        gchar *old_name = NULL;

        if(current_wm)
                old_name = g_strdup (gnome_desktop_item_get_string (current_wm->dentry, GNOME_DESKTOP_ITEM_NAME));
        
        g_list_foreach (window_managers, (GFunc)wm_free, NULL);
        g_list_free (window_managers);
        window_managers = NULL;

        tmp_list = window_managers_save;
        while (tmp_list) {
                window_managers = g_list_prepend (window_managers,
                                                  wm_copy (tmp_list->data));
                tmp_list = tmp_list->next;
        }
        window_managers = g_list_reverse (window_managers);
        current_wm = wm_list_find (window_managers, old_name);
        g_free (old_name);
}

void
wm_list_add (WindowManager *wm)
{
        g_return_if_fail (wm != NULL);
        
        window_managers = g_list_insert_sorted (window_managers, wm,
                                                wm_compare);
}

void
wm_list_delete (WindowManager *wm)
{
        GList *node;

        g_return_if_fail (wm != NULL);
        g_return_if_fail (wm != current_wm);

        node = g_list_find (window_managers, wm);
        g_return_if_fail (node != NULL);
        
        window_managers = g_list_remove_link (window_managers, node);
        g_list_free_1 (node);
        wm_free (wm);
}

void
wm_list_set_current (WindowManager *wm)
{
        current_wm = wm;
}

WindowManager *
wm_list_get_current (void)
{
        return current_wm;
}

WindowManager *
wm_list_get_revert (void)
{
        if(current_wm_save)
                return wm_list_find (window_managers, gnome_desktop_item_get_string (current_wm_save->dentry, GNOME_DESKTOP_ITEM_NAME));
        else
                return NULL;
}

static WindowManager *
wm_read_from_xml (xmlNodePtr wm_node) 
{
        xmlNodePtr node;
        WindowManager *wm;
        gboolean is_current = FALSE;

        if (strcmp (wm_node->name, "window-manager")) return NULL;

        wm = g_new0 (WindowManager, 1);

        wm->dentry = gnome_desktop_item_new_from_file
                (xmlGetProp (wm_node, "desktop-entry"),
		 GNOME_DESKTOP_ITEM_TYPE_APPLICATION, NULL);
	gnome_desktop_item_set_entry_type (wm->dentry, GNOME_DESKTOP_ITEM_TYPE_APPLICATION);

        for (node = wm_node->children; node; node = node->next) {
                if (!strcmp (node->name, "config-exec"))
                        wm->config_exec = xmlNodeGetContent (node);
                else if (!strcmp (node->name, "config-tryexec"))
                        wm->config_tryexec = xmlNodeGetContent (node);
                else if (!strcmp (node->name, "session-managed"))
                        wm->session_managed = xml_read_bool (node);
                else if (!strcmp (node->name, "is-user"))
                        wm->is_user = xml_read_bool (node);
                else if (!strcmp (node->name, "is-current"))
                        is_current = xml_read_bool (node);  /* FIXME: sanity check */
        }

        wm_check_present (wm);

        if (wm->dentry == NULL || 
            (wm->config_exec != NULL && is_blank (wm->config_exec)) ||
            gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_NAME) == NULL || gnome_desktop_item_get_string (wm->dentry, GNOME_DESKTOP_ITEM_EXEC) == NULL || 
            !(wm->is_user || wm->is_present)) 
        {
                g_free (wm);
                return NULL;
        }

        if (is_current) current_wm = wm;

        return wm;
}

void
wm_list_read_from_xml (xmlDocPtr doc) 
{
        xmlNodePtr root_node, node;
        WindowManager *wm;

        root_node = xmlDocGetRootElement (doc);
        if (strcmp (root_node->name, "wm-prefs")) return;

        for (node = root_node; node; node = node->next) {
                if (!strcmp (node->name, "window-manager")) {
                        wm = wm_read_from_xml (node);
                        if (wm) window_managers = 
                                        g_list_insert_sorted
                                        (window_managers, wm, wm_compare);
                }
        }
}

static xmlNodePtr
wm_write_to_xml (WindowManager *wm) 
{
        xmlNodePtr node;

        node = xmlNewNode (NULL, "window-manager");

        xmlNewProp (node, "desktop-entry", gnome_desktop_item_get_location (wm->dentry));

        if (wm->config_exec != NULL)
                xmlNewChild (node, NULL, "config-exec", wm->config_exec);

        xmlAddChild (node, xml_write_bool ("session-managed",
                                           wm->session_managed));

        xmlAddChild (node, xml_write_bool ("is-user", wm->is_user));
        xmlAddChild (node, xml_write_bool ("is-current", wm == current_wm));

        return node;
}

xmlDocPtr
wm_list_write_to_xml (void) 
{
        xmlDocPtr doc;
        xmlNodePtr root_node, node;
        GList *wm_node;

        doc = xmlNewDoc ("1.0");
        root_node = xmlNewDocNode (doc, NULL, "wm-prefs", NULL);

        for (wm_node = window_managers; wm_node; wm_node = wm_node->next) {
                node = wm_write_to_xml ((WindowManager *) wm_node->data);
                if (node) xmlAddChild (root_node, node);
        }

        xmlDocSetRootElement (doc, root_node);

        return doc;
}

/* Read a boolean value from a node */

static gboolean
xml_read_bool (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (!g_strcasecmp (text, "true")) 
		return TRUE;
	else
		return FALSE;
}

/* Write out a boolean value in a node */

static xmlNodePtr
xml_write_bool (gchar *name, gboolean value) 
{
	xmlNodePtr node;

	g_return_val_if_fail (name != NULL, NULL);

	node = xmlNewNode (NULL, name);

	if (value)
		xmlNodeSetContent (node, "true");
	else
		xmlNodeSetContent (node, "false");

	return node;
}
