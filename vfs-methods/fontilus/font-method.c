/* -*- mode: C; c-basic-offset: 4 -*-
 * fontilus - a collection of font utilities for GNOME
 * Copyright (C) 2002-2003  James Henstridge <james@daa.com.au>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fontconfig/fontconfig.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-cancellable-ops.h>
#include <libgnomevfs/gnome-vfs-module.h>

#define FONT_METHOD_DIRECTORY DIRECTORY_DIR "/font-method.directory"

/* this is from gnome-vfs-monitor-private.h */
void gnome_vfs_monitor_callback (GnomeVFSMethodHandle *method_handle,
                                 GnomeVFSURI *info_uri,
                                 GnomeVFSMonitorEventType event_type);

static void invoke_monitors(void);

/* -------- code for creating the font list -------- */

/* list of fonts in fontconfig database */
G_LOCK_DEFINE_STATIC(font_list);
static FcFontSet *font_list = NULL;
static gchar **font_names = NULL;
static GHashTable *font_hash = NULL;

static gchar *
get_pango_name(FcPattern *pat)
{
    FcChar8 *family;
    GString *str;
    gint i;

    FcPatternGetString(pat, FC_FAMILY, 0, &family);
    str = g_string_new(family);
    g_string_append_c(str, ',');

    /* add weight word */
    if (FcPatternGetInteger(pat, FC_WEIGHT, 0, &i) == FcResultMatch) {
	gchar *weight = NULL;

	if (i < FC_WEIGHT_LIGHT)
	    weight = " Ultra-Light";
	else if (i < (FC_WEIGHT_LIGHT + FC_WEIGHT_MEDIUM) / 2)
	    weight = " Light";
	else if (i < (FC_WEIGHT_MEDIUM + FC_WEIGHT_DEMIBOLD) / 2)
	    weight = NULL;
	else if (i < (FC_WEIGHT_DEMIBOLD + FC_WEIGHT_BOLD) / 2)
	    weight = " Semi-Bold";
	else if (i < (FC_WEIGHT_BOLD + FC_WEIGHT_BLACK) / 2)
	    weight = " Bold";
	else
	    weight = " Ultra-Bold";

	if (weight)
	    g_string_append(str, weight);
    }

    /* add slant word */
    if (FcPatternGetInteger(pat, FC_SLANT, 0, &i) == FcResultMatch) {
	gchar *style = NULL;

	if (i == FC_SLANT_ROMAN)
	    style = NULL;
	else if (i == FC_SLANT_OBLIQUE)
	    style = " Oblique";
	else
	    style = " Italic";

	if (style)
	    g_string_append(str, style);
    }

    /* if ends in a comma, check to see if the last word matches a modifier.
     * if not, remove the comma. */
    if (str->str[str->len-1] == ',') {
	const gchar *lastword;
	gint wordlen, i;
	gboolean word_matches;
	const char *modifier_words[] = {
	    "Oblique", "Italic", "Small-Caps", "Ultra-Light", "Light",
	    "Medium", "Semi-Bold", "Bold", "Ultra-Bold", "Heavy",
	    "Ultra-Condensed", "Extra-Condensed", "Condensed",
	    "Semi-Condensed", "Semi-Expanded", "Expanded",
	    "Extra-Expanded", "Ultra-Expanded" };

	lastword = strrchr(str->str, ' ');
	if (lastword)
	    lastword++;
	else
	    lastword = str->str;
	wordlen = strlen(lastword) - 1; /* exclude comma */

	word_matches = FALSE;
	for (i = 0; i < G_N_ELEMENTS(modifier_words); i++) {
	    if (g_ascii_strncasecmp(modifier_words[i], lastword, wordlen)==0) {
		word_matches = TRUE;
		break;
	    }
	}

	/* if the last word doesn't match, then we can remove the comma */
	if (!word_matches)
	    g_string_truncate(str, str->len-1);
    }

    return g_string_free(str, FALSE);
}

/* make sure the font_list is valid */
static gboolean
ensure_font_list(void)
{
    gboolean result = FALSE;
    FcPattern *pat;
    FcObjectSet *os;
    gint i;

    G_LOCK(font_list);
    /* if the config exists, and is up to date, return */
    if (font_list != NULL) {
	if (FcInitBringUptoDate()) {
	    result = TRUE;
	    goto end;
	}

	/* otherwise, destroy the fonts list and recreate */
	FcFontSetDestroy(font_list);
	font_list = NULL;
	g_strfreev(font_names);
	font_names = NULL;
	g_hash_table_destroy(font_hash);
	font_hash = NULL;
    }

    pat = FcPatternCreate();
    os = FcObjectSetBuild(FC_FILE, FC_FAMILY, FC_WEIGHT, FC_SLANT, 0);

    font_list = FcFontList(NULL, pat, os);

    FcPatternDestroy(pat);
    FcObjectSetDestroy(os);

    if (!font_list) {
	result = FALSE;
	goto end;
    }

    /* set up name list and hash */
    font_names = g_new(gchar *, font_list->nfont);
    font_hash = g_hash_table_new(g_str_hash, g_str_equal);
    for (i = 0; i < font_list->nfont; i++) {
	font_names[i] = get_pango_name(font_list->fonts[i]);
	g_hash_table_insert(font_hash, font_names[i], font_list->fonts[i]);
    }

    result = TRUE;

    /* invoke monitors */
    invoke_monitors();

 end:
    G_UNLOCK(font_list);
    return result;
}

static GnomeVFSURI *
create_local_uri(const GnomeVFSURI *orig_uri)
{
    gchar *fontsdir, *fontsdir_escaped, *basename;
    GnomeVFSURI *fontsdir_uri, *new_uri;

    /* make sure ~/.fonts exists ... */
    fontsdir = g_strconcat(g_get_home_dir(), G_DIR_SEPARATOR_S, ".fonts",NULL);
    if (mkdir(fontsdir, 0755) && errno != EEXIST) {
	g_free(fontsdir);
	return NULL;
    }
    /* get URI for fontsdir */
    fontsdir_escaped = gnome_vfs_get_uri_from_local_path(fontsdir);
    g_free(fontsdir);
    fontsdir_uri = gnome_vfs_uri_new(fontsdir_escaped);
    g_free(fontsdir_escaped);

    basename = gnome_vfs_uri_extract_short_name(orig_uri);
    new_uri = gnome_vfs_uri_append_file_name(fontsdir_uri, basename);
    g_free(basename);
    gnome_vfs_uri_unref(fontsdir_uri);

    return new_uri;
}

/* -------- VFS method ------ */

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

static GnomeVFSResult
fill_file_info(GnomeVFSFileInfo *file_info, GnomeVFSFileInfoOptions options,
	       FcChar8 *file, gchar *name)
{
    gchar *uri;
    GnomeVFSResult result;

    uri = gnome_vfs_get_uri_from_local_path(file);
    result = gnome_vfs_get_file_info(uri, file_info, options);
    g_free (uri);
    if (result == GNOME_VFS_OK) {
	g_free(file_info->name);
	file_info->name = g_strdup(name);

	file_info->valid_fields &= ~GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME;
	g_free(file_info->symlink_name);
	file_info->symlink_name = NULL;
	file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
	GNOME_VFS_FILE_INFO_SET_SYMLINK(file_info, FALSE);
    }

    return result;
}

typedef struct _FontListHandle FontListHandle;
struct _FontListHandle {
    gint font;
    GnomeVFSFileInfoOptions options;
    gboolean seen_dotdirectory;
};

static GnomeVFSResult
do_open_directory(GnomeVFSMethod *method,
		  GnomeVFSMethodHandle **method_handle,
		  GnomeVFSURI *uri,
		  GnomeVFSFileInfoOptions options,
		  GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_ERROR_NOT_SUPPORTED;
    char *path = NULL;
    FontListHandle *handle;

    path = get_path_from_uri(uri);
    if (!path) {
	result = GNOME_VFS_ERROR_INVALID_URI;
	goto end;
    }
    if (strcmp(path, "") != 0) {
	result = GNOME_VFS_ERROR_NOT_FOUND;
	goto end;
    }

    if (!ensure_font_list()) {
	result = GNOME_VFS_ERROR_INTERNAL;
	goto end;
    }

    /* handle used to iterate over font names */
    handle = g_new0(FontListHandle, 1);
    handle->font = 0;
    handle->options = options;
    handle->seen_dotdirectory = FALSE;
    *method_handle = (GnomeVFSMethodHandle *)handle;
    result = GNOME_VFS_OK;

 end:
    g_free(path);
    return result;
}

static GnomeVFSResult
do_close_directory(GnomeVFSMethod *method,
		   GnomeVFSMethodHandle *method_handle,
		   GnomeVFSContext *context)
{
    FontListHandle *handle;

    handle = (FontListHandle *)method_handle;
    g_free(handle);

    return GNOME_VFS_OK;
}

static GnomeVFSResult
do_read_directory(GnomeVFSMethod *method,
		  GnomeVFSMethodHandle *method_handle,
		  GnomeVFSFileInfo *file_info,
		  GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_ERROR_NOT_SUPPORTED;
    FontListHandle *handle;
    FcChar8 *file;

    handle = (FontListHandle *)method_handle;

    G_LOCK(font_list);
    if (!font_list) {
	result = GNOME_VFS_ERROR_INTERNAL;
	goto end;
    }

    /* list the .directory file */
    if (!handle->seen_dotdirectory) {
	g_free(file_info->name);
	file_info->name = g_strdup(".directory");
	file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_TYPE;
	file_info->mime_type = g_strdup("application/x-gnome-app-info");
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;
	handle->seen_dotdirectory = TRUE;
	result = GNOME_VFS_OK;
	goto end;
    }

    if (handle->font >= font_list->nfont) {
	result = GNOME_VFS_ERROR_EOF;
	goto end;
    }

    /* get information about this font, skipping unfound fonts */
    result = GNOME_VFS_ERROR_NOT_FOUND;
    while (handle->font < font_list->nfont &&
	   result == GNOME_VFS_ERROR_NOT_FOUND) {
	FcPatternGetString(font_list->fonts[handle->font], FC_FILE, 0, &file);
	result = fill_file_info(file_info, handle->options, file,
				font_names[handle->font]);

	/* move on to next font */
	handle->font++;
    }

 end:
    G_UNLOCK(font_list);
    return result;
}

/* -------- handling of file objects -------- */

static GnomeVFSResult
do_open(GnomeVFSMethod *method,
	GnomeVFSMethodHandle **method_handle,
	GnomeVFSURI *uri,
	GnomeVFSOpenMode mode,
	GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_ERROR_NOT_FOUND;
    char *path = NULL;
    FcPattern *font;

    path = get_path_from_uri(uri);
    if (!path) {
	result = GNOME_VFS_ERROR_INVALID_URI;
	goto end;
    }

    if (!ensure_font_list()) {
	result = GNOME_VFS_ERROR_INTERNAL;
	goto end;
    }

    if (path[0] == '\0') {
	result = GNOME_VFS_ERROR_IS_DIRECTORY;
	goto end;
    }

    /* we don't support openning existing files for writing */
    if ((mode & GNOME_VFS_OPEN_WRITE) != 0) {
	result = GNOME_VFS_ERROR_READ_ONLY;
	goto end;
    }

    /* handle the .directory file */
    if (!strcmp(path, "/.directory")) {
	GnomeVFSURI *uri;

	uri = gnome_vfs_uri_new(FONT_METHOD_DIRECTORY);
	result = gnome_vfs_open_uri_cancellable(
		(GnomeVFSHandle **)method_handle, uri, mode, context);
	gnome_vfs_uri_unref(uri);
	goto end;
    }

    G_LOCK(font_list);
    font = g_hash_table_lookup(font_hash, &path[1]);
    if (font) {
	FcChar8 *file;
	gchar *text_uri;
	GnomeVFSURI *font_uri;

	FcPatternGetString(font, FC_FILE, 0, &file);
	text_uri = gnome_vfs_get_uri_from_local_path(file);
	font_uri = gnome_vfs_uri_new(text_uri);
	g_free(text_uri);

	result = gnome_vfs_open_uri_cancellable(
		(GnomeVFSHandle **)method_handle, font_uri, mode, context);

	gnome_vfs_uri_unref(font_uri);
    } else {
	result = GNOME_VFS_ERROR_NOT_FOUND;
    }
    G_UNLOCK(font_list);

 end:
    g_free(path);
    return result;
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

    new_uri = create_local_uri(uri);
    if (!new_uri)
	return gnome_vfs_result_from_errno();
    result = gnome_vfs_create_uri_cancellable((GnomeVFSHandle **)method_handle,
					      new_uri, mode, exclusive, perm,
					      context);
    gnome_vfs_uri_unref(new_uri);

    return result;
}


static GnomeVFSResult
do_close(GnomeVFSMethod *method,
	 GnomeVFSMethodHandle *method_handle,
	 GnomeVFSContext *context)
{
    return gnome_vfs_close_cancellable((GnomeVFSHandle *)method_handle,
				       context);
}

static GnomeVFSResult
do_read(GnomeVFSMethod *method,
	GnomeVFSMethodHandle *method_handle,
	gpointer buffer,
	GnomeVFSFileSize bytes,
	GnomeVFSFileSize *bytes_read,
	GnomeVFSContext *context)
{
    return gnome_vfs_read_cancellable((GnomeVFSHandle *)method_handle,
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
    return gnome_vfs_write_cancellable((GnomeVFSHandle *)method_handle,
				       buffer, bytes, bytes_written, context);
}

static GnomeVFSResult
do_seek(GnomeVFSMethod *method,
	GnomeVFSMethodHandle *method_handle,
	GnomeVFSSeekPosition whence,
	GnomeVFSFileOffset offset,
	GnomeVFSContext *context)
{
    return gnome_vfs_seek_cancellable((GnomeVFSHandle *)method_handle,
				      whence, offset, context);
}

static GnomeVFSResult
do_tell(GnomeVFSMethod *method,
	GnomeVFSMethodHandle *method_handle,
	GnomeVFSFileOffset *offset_return)
{
    return gnome_vfs_tell((GnomeVFSHandle *)method_handle, offset_return);
}

static GnomeVFSResult
do_get_file_info_from_handle(GnomeVFSMethod *method,
			     GnomeVFSMethodHandle *method_handle,
			     GnomeVFSFileInfo *file_info,
			     GnomeVFSFileInfoOptions options,
			     GnomeVFSContext *context)
{
    return gnome_vfs_get_file_info_from_handle_cancellable
	((GnomeVFSHandle *)method_handle, file_info, options, context);
}


/* -------- file metadata -------- */

static GnomeVFSResult
do_get_file_info(GnomeVFSMethod *method,
		 GnomeVFSURI *uri,
		 GnomeVFSFileInfo *file_info,
		 GnomeVFSFileInfoOptions options,
		 GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_ERROR_NOT_FOUND;
    char *path = NULL;

    path = get_path_from_uri(uri);
    if (!path) {
	result = GNOME_VFS_ERROR_INVALID_URI;
	goto end;
    }

    if (!ensure_font_list()) {
	result = GNOME_VFS_ERROR_INTERNAL;
	goto end;
    }

    /* root directory */
    if (!strcmp(path, "")) {
	g_free(file_info->name);
	file_info->name = g_strdup("Fonts");

	file_info->type = GNOME_VFS_FILE_TYPE_DIRECTORY;
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_TYPE;

	g_free(file_info->mime_type);
	file_info->mime_type = g_strdup("x-directory/normal");
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;

	result = GNOME_VFS_OK;
    } else if (!strcmp(path, "/.directory")) {
	g_free(file_info->name);
	file_info->name = g_strdup(".directory");
	file_info->type = GNOME_VFS_FILE_TYPE_REGULAR;
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_TYPE;
	g_free(file_info->mime_type);
	file_info->mime_type = g_strdup("application/x-gnome-app-info");
	file_info->valid_fields |= GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE;

	result = GNOME_VFS_OK;
    } else {
	FcPattern *font;

	G_LOCK(font_list);
	font = g_hash_table_lookup(font_hash, &path[1]);
	if (font) {
	    FcChar8 *file;

	    FcPatternGetString(font, FC_FILE, 0, &file);
	    result = fill_file_info(file_info, options, file, &path[1]);
	}
	G_UNLOCK(font_list);
    }

 end:
    G_UNLOCK(font_list);
    g_free(path);
    return result;
}

static gboolean
do_is_local(GnomeVFSMethod *method,
            const GnomeVFSURI *uri)
{
    gboolean result = FALSE;
    char *path = NULL;

    path = get_path_from_uri(uri);
    if (!path) { /* invalid path */
	goto end;
    }
    if (!ensure_font_list()) { /* could not build font list */
	goto end;
    }

    /* root directory */
    if (!strcmp(path, "")) { /* base dir */
	result = TRUE;
    } else if (!strcmp(path, "/.directory")) {
	result = TRUE;
    } else {
	FcPattern *font;

	G_LOCK(font_list);
	font = g_hash_table_lookup(font_hash, &path[1]);
	if (font) { /* check if underlying uri is local */
	    FcChar8 *file;
	    gchar *file_text_uri;
	    GnomeVFSURI *file_uri;

	    FcPatternGetString(font, FC_FILE, 0, &file);
	    file_text_uri = gnome_vfs_get_uri_from_local_path(file);
	    file_uri = gnome_vfs_uri_new(file_text_uri);
	    g_free(file_text_uri);

	    result = gnome_vfs_uri_is_local(file_uri);
	    gnome_vfs_uri_unref(file_uri);
	}
	G_UNLOCK(font_list);
    }

 end:
    g_free(path);
    return result;
}

static GnomeVFSResult
do_unlink(GnomeVFSMethod *method,
	  GnomeVFSURI *uri,
	  GnomeVFSContext *context)
{
    GnomeVFSResult result = GNOME_VFS_ERROR_NOT_SUPPORTED;
    char *path = NULL;

    path = get_path_from_uri(uri);
    if (!path) { /* invalid path */
	result = GNOME_VFS_ERROR_INVALID_URI;
	goto end;
    }
    if (!ensure_font_list()) { /* could not build font list */
	result = GNOME_VFS_ERROR_INTERNAL;
	goto end;
    }

    if (!strcmp(path, "")) { /* base dir */
	result = GNOME_VFS_ERROR_NOT_PERMITTED;
    } else {
	FcPattern *font;

	G_LOCK(font_list);
	font = g_hash_table_lookup(font_hash, &path[1]);
	if (font) { /* check if underlying uri is local */
	    FcChar8 *file;
	    gchar *file_text_uri;
	    GnomeVFSURI *file_uri;

	    FcPatternGetString(font, FC_FILE, 0, &file);
	    file_text_uri = gnome_vfs_get_uri_from_local_path(file);
	    file_uri = gnome_vfs_uri_new(file_text_uri);
	    g_free(file_text_uri);

	    result = gnome_vfs_unlink_from_uri_cancellable(file_uri, context);
	    gnome_vfs_uri_unref(file_uri);
	} else {
	    result = GNOME_VFS_ERROR_NOT_FOUND;
	}
	G_UNLOCK(font_list);
    }
 end:
    g_free(path);
    return result;
}


/* -------- Directory monitor -------- */

/* list of monitors attached to fonts:/// */
G_LOCK_DEFINE_STATIC(monitor_list);
static GList *monitor_list = NULL;

static void
invoke_monitors(void)
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
    GnomeVFSURI *uri_copy;

    path = get_path_from_uri(uri);
    if (!path) { /* invalid path */
	return GNOME_VFS_ERROR_INVALID_URI;
    }

    if (path[0] != '\0' || monitor_type != GNOME_VFS_MONITOR_DIRECTORY) {
	g_free(path);
	return GNOME_VFS_ERROR_NOT_SUPPORTED;
    }
    g_free(path);

    /* it is a directory monitor on fonts:/// */
    uri_copy = gnome_vfs_uri_dup(uri);
    *method_handle = (GnomeVFSMethodHandle *)uri_copy;
    G_LOCK(monitor_list);
    monitor_list = g_list_prepend(monitor_list, uri_copy);
    G_UNLOCK(monitor_list);

    return GNOME_VFS_OK;
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


/* -------- Initialisation of the method -------- */

static GnomeVFSMethod method = {
    sizeof(GnomeVFSMethod),

    do_open,
    do_create,
    do_close,
    do_read,
    do_write,
    do_seek,
    do_tell,
    NULL,
    do_open_directory,
    do_close_directory,
    do_read_directory,
    do_get_file_info,
    do_get_file_info_from_handle,
    do_is_local,
    NULL,
    NULL,
    NULL,
    do_unlink,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    do_monitor_add,
    do_monitor_cancel,
    NULL
};

GnomeVFSMethod *
vfs_module_init(const char *method_name, const char *args)
{
    if (!strcmp(method_name, "fonts")) {
	if (!FcInit()) {
	    g_warning("can't init fontconfig library");
	    return NULL;
	}
	return &method;
    }
    return NULL;
}

void
vfs_module_shutdown(GnomeVFSMethod *method)
{
    /* clean up font list */
    if (font_list)  FcFontSetDestroy(font_list);
    if (font_names) g_strfreev(font_names);
    if (font_hash) g_hash_table_destroy(font_hash);
    font_list = NULL;
    font_names = NULL;
    font_hash = NULL;
}
