/* -*- mode: C; c-basic-offset: 4 -*-
 * themus - utilities for GNOME themes
 * Copyright (C) 2003  Andrew Sobala <aes@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-cancellable-ops.h>
#include <libgnomevfs/gnome-vfs-module.h>

#include <gnome-theme-info.h>

#define THEME_METHOD_DIRECTORY DIRECTORY_DIR "/theme-method.directory"
#define _(s) gettext (s)

static void invoke_monitors(gchar *uri, gpointer data);


/* cc cut-paste */ 
static gboolean
transfer_done_targz_idle_cb (gpointer data)
{
	int status;
	gchar *command;
	gchar *path = data;

	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'gzip -d -c < \"%s\" | tar xf - -C \"%s/.themes\"'",
				   path, g_get_home_dir ());
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0)
		gnome_vfs_unlink (path);
	g_free (command);
	g_free (path);

	return FALSE;
}

static gboolean
transfer_done_tarbz2_idle_cb (gpointer data)
{
	int status;
	gchar *command;
	gchar *path = data;

	/* this should be something more clever and nonblocking */
	command = g_strdup_printf ("sh -c 'bzip2 -d -c < \"%s\" | tar xf - -C \"%s/.themes\"'",
				   path, g_get_home_dir ());
	if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0)
		gnome_vfs_unlink (path);
	g_free (command);
	g_free (path);

	return FALSE;
}
/* end cut-paste*/

static gchar *
get_path_from_uri (GnomeVFSURI const *uri)
{
    gchar *path;
    gint len;

    path = gnome_vfs_unescape_string (uri->text,
				      G_DIR_SEPARATOR_S);

    if (path == NULL) {
	return NULL;
    }

    if (path[0] != G_DIR_SEPARATOR) {
	g_free (path);
	return NULL;
    }

    len = strlen(path);
    if (path[len-1] == G_DIR_SEPARATOR) path[len-1] = '\0';

    return path;
}

static GnomeVFSURI *
create_local_uri(const GnomeVFSURI *orig_uri)
{
    gchar *themedir, *themedir_escaped, *basename;
    GnomeVFSURI *themedir_uri, *new_uri;

    /* make sure ~/.themes exists ... */
    themedir = g_strconcat(g_get_home_dir(), G_DIR_SEPARATOR_S, ".themes",NULL);
    if (mkdir(themedir, 0755) && errno != EEXIST) {
	g_free(themedir);
	return NULL;
    }
    /* get URI */
    themedir_escaped = gnome_vfs_get_uri_from_local_path(themedir);
    g_free(themedir);
    themedir_uri = gnome_vfs_uri_new(themedir_escaped);
    g_free(themedir_escaped);

    basename = gnome_vfs_uri_extract_short_name(orig_uri);
    new_uri = gnome_vfs_uri_append_file_name(themedir_uri, basename);
    g_free(basename);
    gnome_vfs_uri_unref(themedir_uri);
    return new_uri;
}

static gint
theme_meta_info_sort_func (GnomeThemeMetaInfo *a, GnomeThemeMetaInfo *b)
{
	return strcmp (a->readable_name, b->readable_name);
}

static GnomeThemeMetaInfo*
theme_meta_info_find (GnomeVFSURI *uri)
{
	GList *theme;
	gchar *path;
	
	path = get_path_from_uri (uri);
	for (theme = gnome_theme_meta_info_find_all (); theme != NULL;
								theme=theme->next)
		if (!strcmp ( g_strconcat ("/",
			((GnomeThemeMetaInfo*)(theme->data))->readable_name, NULL),
			path))
			{
				g_free (path);
				return ((GnomeThemeMetaInfo*)(theme->data));
			}
	
	g_free (path);
	return NULL;
}

static GnomeVFSResult
fill_info_struct (GnomeVFSFileInfo *file_info, GnomeVFSFileInfoOptions options,
GnomeThemeMetaInfo *theme)
{
	GnomeVFSURI *theme_uri;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;

	theme_uri = gnome_vfs_uri_new (theme->path);
	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info_uri (theme_uri, info, options);
	if (result == GNOME_VFS_OK)
	{
		g_free(file_info->name);
		file_info->name = g_strdup (theme->readable_name);
		
		file_info->uid = info->uid;
		file_info->gid = info->gid;
		
		g_free (file_info->mime_type);
		file_info->mime_type = g_strdup ("application/x-gnome-theme-installed");
		file_info->valid_fields |=
					GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
		
		file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
		file_info->valid_fields |= 
					GNOME_VFS_FILE_INFO_FIELDS_TYPE;
		
		if (info->valid_fields || GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS)
		{
			file_info->permissions = info->permissions;
			file_info->valid_fields |= 
					GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS;
		}

		if (info->valid_fields || GNOME_VFS_FILE_INFO_FIELDS_ATIME)
		{
			file_info->atime = info->atime;
			file_info->valid_fields |= 
					GNOME_VFS_FILE_INFO_FIELDS_ATIME;
		}

		if (info->valid_fields || GNOME_VFS_FILE_INFO_FIELDS_CTIME)
		{
			file_info->ctime = info->ctime;
			file_info->valid_fields |= 
					GNOME_VFS_FILE_INFO_FIELDS_CTIME;
		}

		if (info->valid_fields || GNOME_VFS_FILE_INFO_FIELDS_SIZE)
		{
			file_info->size = info->size;
			file_info->valid_fields |= 
					GNOME_VFS_FILE_INFO_FIELDS_SIZE;
		}

		gnome_vfs_uri_unref (theme_uri);
		gnome_vfs_file_info_unref (info);
	}

	return result;
}

typedef struct _ThemeHandle ThemeHandle;
struct _ThemeHandle {
    GnomeVFSMethodHandle *handle;

    // if we're listing themes....
    GList *theme;
    GnomeVFSFileInfoOptions options;
    gboolean seen_dotdirectory;
    
    // if we're doing a file....
    gchar *uri; // the real URI
};

/* -- VFS method -- */

static GnomeVFSResult
do_open_directory(GnomeVFSMethod *method,
		  GnomeVFSMethodHandle **method_handle,
		  GnomeVFSURI *uri,
		  GnomeVFSFileInfoOptions options,
		  GnomeVFSContext *context)
{
	gchar* path;
	ThemeHandle *handle;
	path = get_path_from_uri (uri);
	if (!strcmp (path, ""))
	{
		handle = g_new0(ThemeHandle, 1);
		handle->handle = (GnomeVFSMethodHandle *) method_handle;
		handle->theme = gnome_theme_meta_info_find_all ();
		handle->theme = g_list_sort (handle->theme,
					(GCompareFunc)theme_meta_info_sort_func);
		handle->options = options;
		handle->seen_dotdirectory = FALSE;
		*method_handle = (GnomeVFSMethodHandle *)handle;
		g_free (path);
		return GNOME_VFS_OK;
	}
	else {
		g_free (path);
		return GNOME_VFS_ERROR_NOT_FOUND;
	}
}

static GnomeVFSResult
do_close_directory(GnomeVFSMethod *method,
		   GnomeVFSMethodHandle *method_handle,
		   GnomeVFSContext *context)
{
	g_free ((ThemeHandle*)method_handle);
	return GNOME_VFS_OK;
}

static GnomeVFSResult
do_read_directory(GnomeVFSMethod *method,
		  GnomeVFSMethodHandle *method_handle,
		  GnomeVFSFileInfo *file_info,
		  GnomeVFSContext *context)
{
	GList *theme;
	theme = ((ThemeHandle*)(method_handle))->theme;
	
	if (!((ThemeHandle*)(method_handle))->seen_dotdirectory)
	{
		g_free (file_info->name);
		file_info->name = g_strdup (".directory");
		file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_TYPE;
		file_info->mime_type = g_strdup("application/x-gnome-app-info");
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
		file_info->permissions = GNOME_VFS_PERM_USER_ALL |
					 GNOME_VFS_PERM_GROUP_READ |
					 GNOME_VFS_PERM_OTHER_READ;
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS;
		((ThemeHandle*)(method_handle))->seen_dotdirectory = TRUE;
		return GNOME_VFS_OK;
	}

	if (theme == NULL)
	{
		return GNOME_VFS_ERROR_EOF;
	}

	fill_info_struct (file_info, 0, (GnomeThemeMetaInfo*)(theme->data));
	
	((ThemeHandle*)(method_handle))->theme = ((ThemeHandle*)(method_handle))->theme->next;
	return GNOME_VFS_OK;
}

/* -------- handling of file objects -------- */

static GnomeVFSResult
do_open(GnomeVFSMethod *method,
	GnomeVFSMethodHandle **method_handle,
	GnomeVFSURI *uri,
	GnomeVFSOpenMode mode,
	GnomeVFSContext *context)
{
	gchar *path;
	GnomeVFSResult result;
	GnomeThemeMetaInfo *theme;
	GnomeVFSURI *theme_uri;
	ThemeHandle *handle;
	
	path = get_path_from_uri(uri);
	
	if (!path) {
		return GNOME_VFS_ERROR_INVALID_URI;
	}

	if (path[0] == '\0')
	{
		g_free (path);
		return GNOME_VFS_ERROR_IS_DIRECTORY;
	}
	
	/* FIXME: ro check */

	/* handle the .directory file */
	if (!strcmp(path, "/.directory")) {
		GnomeVFSURI *uri;

		uri = gnome_vfs_uri_new(THEME_METHOD_DIRECTORY);
		result = gnome_vfs_open_uri_cancellable(
			(GnomeVFSHandle **)method_handle, uri, mode, context);

		handle = g_new0(ThemeHandle, 1);
		handle->handle = *method_handle;
		*method_handle = (GnomeVFSMethodHandle *) handle;
		
		g_free (path);
		gnome_vfs_uri_unref(uri);
		return result;
	}
	
	g_free (path);
	
	theme = theme_meta_info_find (uri);
	if (theme)
	{
		theme_uri = gnome_vfs_uri_new (theme->path);
		result = gnome_vfs_open_uri_cancellable(
			(GnomeVFSHandle **)method_handle, theme_uri, mode, context);
		
		handle = g_new0(ThemeHandle, 1);
		handle->handle = *method_handle;
		handle->uri = gnome_vfs_uri_to_string (theme_uri, 
						GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
		*method_handle = (GnomeVFSMethodHandle *) handle;
		
		gnome_vfs_uri_unref (theme_uri);
		return result;
	}
	else
		return GNOME_VFS_ERROR_NOT_FOUND;
}

static GnomeVFSResult
do_create(GnomeVFSMethod *method,
	  GnomeVFSMethodHandle **method_handle,
	  GnomeVFSURI *uri,
	  GnomeVFSOpenMode mode,
	  gboolean exclusive,
	  guint perm,
	  GnomeVFSContext *context)
{
    GnomeVFSResult result;
    GnomeVFSURI *new_uri;
    ThemeHandle *handle;    
    
    new_uri = create_local_uri(uri);
    if (!new_uri)
	return gnome_vfs_result_from_errno();
	
    result = gnome_vfs_create_uri_cancellable((GnomeVFSHandle **)method_handle,
					      new_uri, mode, exclusive, perm,
					      context);
					      
    handle = g_new0 (ThemeHandle, 1);
    handle->uri = gnome_vfs_uri_to_string (new_uri, 
					GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
    handle->handle = *method_handle;
    *method_handle = (GnomeVFSMethodHandle*) handle;
    
    gnome_vfs_uri_unref(new_uri);

    return result;
}


static GnomeVFSResult
do_close(GnomeVFSMethod *method,
	 GnomeVFSMethodHandle *method_handle,
	 GnomeVFSContext *context)
{
	GnomeVFSResult result;
	gchar* path = NULL;
	gint len;
	
	path = ((ThemeHandle*) method_handle)->uri;
	result = gnome_vfs_close_cancellable ((GnomeVFSHandle*)(((ThemeHandle*)(method_handle))->handle), context);
	g_free (method_handle);
		       
	if (result != GNOME_VFS_OK)
		return result;
	
	if (path) {	
		len = strlen (path);
		if (path && len > 7 && !strcmp (path + len - 7, ".tar.gz"))
			transfer_done_targz_idle_cb (path);
		if (path && len > 8 && !strcmp (path + len - 8, ".tar.bz2"))
			transfer_done_tarbz2_idle_cb (path);
		
		invoke_monitors ("themes:///", NULL);
	}
	
	return result;
	
}

static GnomeVFSResult
do_read(GnomeVFSMethod *method,
	GnomeVFSMethodHandle *method_handle,
	gpointer buffer,
	GnomeVFSFileSize bytes,
	GnomeVFSFileSize *bytes_read,
	GnomeVFSContext *context)
{
	return gnome_vfs_read_cancellable(
			(GnomeVFSHandle*)(((ThemeHandle*)(method_handle))->handle),
			buffer, bytes, bytes_read, context);
}

static GnomeVFSResult
do_write(GnomeVFSMethod *method,
	 GnomeVFSMethodHandle *method_handle,
	 gconstpointer buffer,
	 GnomeVFSFileSize bytes,
	 GnomeVFSFileSize *bytes_written,
	 GnomeVFSContext *context)
{
	return gnome_vfs_write_cancellable(
			(GnomeVFSHandle*)(((ThemeHandle*)(method_handle))->handle),
			buffer, bytes, bytes_written, context);
}

static GnomeVFSResult
do_seek(GnomeVFSMethod *method,
	GnomeVFSMethodHandle *method_handle,
	GnomeVFSSeekPosition whence,
	GnomeVFSFileOffset offset,
	GnomeVFSContext *context)
{
	return gnome_vfs_seek_cancellable(
			(GnomeVFSHandle*)(((ThemeHandle*)(method_handle))->handle),
			whence, offset, context);
}

static GnomeVFSResult
do_tell(GnomeVFSMethod *method,
	GnomeVFSMethodHandle *method_handle,
	GnomeVFSFileOffset *offset_return)
{
	return gnome_vfs_tell(
			(GnomeVFSHandle*)(((ThemeHandle*)(method_handle))->handle),
			offset_return);
}

static GnomeVFSResult
do_get_file_info_from_handle(GnomeVFSMethod *method,
			     GnomeVFSMethodHandle *method_handle,
			     GnomeVFSFileInfo *file_info,
			     GnomeVFSFileInfoOptions options,
			     GnomeVFSContext *context)
{
	return gnome_vfs_get_file_info_from_handle_cancellable (
			(GnomeVFSHandle*)(((ThemeHandle*)(method_handle))->handle),
			file_info, options, context);
}


/* -------- file metadata -------- */

static GnomeVFSResult
do_get_file_info(GnomeVFSMethod *method,
		 GnomeVFSURI *uri,
		 GnomeVFSFileInfo *file_info,
		 GnomeVFSFileInfoOptions options,
		 GnomeVFSContext *context)
{
	gchar *path = NULL;
	GnomeThemeMetaInfo *theme;
	
	path = get_path_from_uri(uri);
	if (!path)
		return GNOME_VFS_ERROR_INVALID_URI;

	/* root directory */
	if (!strcmp(path, "")) {
		g_free(file_info->name);
		file_info->name = g_strdup(_("Themes"));

		file_info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_TYPE;

		g_free(file_info->mime_type);
		file_info->mime_type = g_strdup("x-directory/normal");
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;

		file_info->permissions = GNOME_VFS_PERM_USER_READ |
					 GNOME_VFS_PERM_GROUP_READ |
					 GNOME_VFS_PERM_OTHER_READ;
		file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS;

		g_free (path);
		return GNOME_VFS_OK;
	}
	else {
		g_free (path);
		
		theme = theme_meta_info_find (uri);
		if (theme)
			return fill_info_struct (file_info, options, theme);
	}
	
	return GNOME_VFS_ERROR_INTERNAL;
}

static gboolean
do_is_local(GnomeVFSMethod *method,
            const GnomeVFSURI *uri)
{
	return TRUE;
}

static GnomeVFSResult
do_unlink(GnomeVFSMethod *method,
	  GnomeVFSURI *uri,
	  GnomeVFSContext *context)
{
	/* we can only safely delete the metatheme; subthemes may be in use by other
	metathemes */
	
	if (!strcmp (gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE),
			"themes:///.vfs-write.tmp"))
	{
		return gnome_vfs_unlink (g_strconcat (g_get_home_dir(), 
				G_DIR_SEPARATOR_S, ".themes", G_DIR_SEPARATOR_S,
				".vfs-write.tmp", NULL));
		/* yuck */
	}
	
	if (!strcmp (gnome_vfs_uri_get_scheme (uri), "themes"))
	{
		GnomeThemeMetaInfo *theme;
		GnomeVFSResult result;
		
		theme = theme_meta_info_find (uri);
		if (theme) {
			result = gnome_vfs_unlink (theme->path);
			invoke_monitors (theme->path, NULL);
			return result;
		}
		else
			return GNOME_VFS_ERROR_INTERNAL;
	} else
		return GNOME_VFS_OK; /* FIXME */
}


/* -------- Directory monitor -------- */

/* list of monitors attached to themes:/// */
G_LOCK_DEFINE_STATIC(monitor_list);
static GList *monitor_list = NULL;

static void
invoke_monitors(gchar *uri, gpointer data)
{
    GList *tmp;

    G_LOCK(monitor_list);
    for (tmp = monitor_list; tmp != NULL; tmp = tmp->next) {
	GnomeVFSURI *uri = tmp->data;

	gnome_vfs_monitor_callback((GnomeVFSMethodHandle *)uri, uri,
				   GNOME_VFS_MONITOR_EVENT_CHANGED);
    }
    G_UNLOCK(monitor_list);
}

static GnomeVFSResult
do_monitor_add(GnomeVFSMethod *method,
	       GnomeVFSMethodHandle **method_handle,
	       GnomeVFSURI *uri,
	       GnomeVFSMonitorType monitor_type)
{
    char *path = NULL;
    GnomeVFSURI *new_uri;
    GnomeVFSResult result;

    path = get_path_from_uri(uri);
    if (!path)  /* invalid path */
	result = GNOME_VFS_ERROR_INVALID_URI;
    else
    {

	    if (path[0] == '\0' && monitor_type == GNOME_VFS_MONITOR_DIRECTORY) {
		/* it is a directory monitor on themes:/// */
		new_uri = gnome_vfs_uri_dup(uri);
		*method_handle = (GnomeVFSMethodHandle *)new_uri;
		G_LOCK(monitor_list);
		monitor_list = g_list_prepend(monitor_list, new_uri);
		G_UNLOCK(monitor_list);
		result = GNOME_VFS_OK;
	    }
	    else if (path[0] == '\0' && monitor_type != GNOME_VFS_MONITOR_DIRECTORY)
    		result = GNOME_VFS_ERROR_NOT_SUPPORTED;
	    else {
    		/* it is a theme monitor */
		result = GNOME_VFS_ERROR_NOT_SUPPORTED; /* though it should be */
	    }
    }

    g_free(path);
    return result;
}

static GnomeVFSResult
do_monitor_cancel(GnomeVFSMethod *method,
		  GnomeVFSMethodHandle *method_handle)
{
    GnomeVFSURI *uri;

    uri = (GnomeVFSURI *)method_handle;
    G_LOCK(monitor_list);
    monitor_list = g_list_remove(monitor_list, uri);
    G_UNLOCK(monitor_list);
    gnome_vfs_uri_unref(uri);
    return GNOME_VFS_OK;
}

/* -- method externs -- */

static GnomeVFSMethod method = {
    sizeof(GnomeVFSMethod),

    .open     = do_open,
    .create   = do_create,
    .close    = do_close,
    .read     = do_read,
    .write    = do_write,
    .seek     = do_seek,
    .tell     = do_tell,
    .get_file_info_from_handle = do_get_file_info_from_handle,

    .open_directory  = do_open_directory,
    .close_directory = do_close_directory,
    .read_directory  = do_read_directory,

    .get_file_info = do_get_file_info,
    .is_local      = do_is_local,
    .unlink        = do_unlink,

    .monitor_add = do_monitor_add,
    .monitor_cancel = do_monitor_cancel
};

GnomeVFSMethod*
vfs_module_init (const char *method_name, const char *args)
{
	gnome_theme_init (FALSE);
	if (!strcmp (method_name, "themes"))
	{
		gnome_theme_info_register_theme_change ((GFunc)invoke_monitors, NULL);
		return &method;
	}
	else
		return NULL;
}

void
vfs_module_shutdown (GnomeVFSMethod* method)
{

}
