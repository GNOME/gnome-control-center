/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* Copyright (C) 1998 Redhat Software Inc. 
 * Authors: Jonathan Blandford <jrb@redhat.com>
 */
#ifndef MIME_DATA_H
#define MIME_DATA_H

#include <gnome.h>
#include <regex.h>

/* Typedefs */
typedef struct {
	char     *mime_type;
	GList    *ext[2];
        GList    *user_ext[2];
        GList    *keys;
} MimeInfo;

extern void add_to_key (char *mime_type, char *def, GHashTable *table, gboolean init_user);

int add_mime_vals_to_clist (gchar *mime_type, gpointer mi, gpointer cl);
void add_new_mime_type (gchar *mime_type, gchar *ext);
void write_user_mime (void);
void write_initial_mime (void);
void reread_list (void);

#endif
