/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc.
 * Code available under the Gnu GPL.
 * Authors: Owen Taylor <otaylor@redhat.com>,
 *          Bradford Hovinen <hovinen@helixcode.com>
 */

#include <gdk/gdk.h>
#include <libgnome/libgnome.h>
#include <libgnome/gnome-desktop-item.h>

#include <libxml/tree.h>

typedef struct _WindowManager WindowManager;

struct _WindowManager {
        GnomeDesktopItem *dentry;
        gchar *config_exec;
        gchar *config_tryexec;
        gboolean session_managed : 1;
        gboolean is_user : 1;
        gboolean is_present : 1;
        gboolean is_config_present : 1;
};

/* Utility functions */
gboolean is_blank (gchar *str);

/* Fill in the is_present and is_config_present fields */
void     wm_check_present (WindowManager *wm);

/* Management of window manager list */

void           wm_list_init   (void);
void           wm_list_save   (void);
void           wm_list_revert (void);
void           wm_list_add    (WindowManager *window_manager);
void           wm_list_delete (WindowManager *window_manager);
void           wm_list_set_current (WindowManager *window_manager);
WindowManager *wm_list_get_current (void);
WindowManager *wm_list_get_revert  (void);

void           wm_list_read_from_xml (xmlDocPtr doc);
xmlDocPtr      wm_list_write_to_xml (void);

extern GList *window_managers;

/* Management of current window manager */

typedef enum {
        WM_SUCCESS,
        WM_ALREADY_RUNNING,
        WM_CANT_START
} WMResult;

typedef void (*WMResultFunc) (WMResult result, gpointer data);

void           wm_restart       (WindowManager *new,
                                 GdkWindow     *client,
                                 WMResultFunc   callback,
                                 gpointer       data);
gboolean       wm_is_running    (void);
WindowManager *wm_guess_current (void);
