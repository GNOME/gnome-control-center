/* copied from rp3 -- DO NOT EDIT HERE, ONLY COPY FROM rp3 */
/*
 * shvar.c
 *
 * Implementation of non-destructively reading/writing files containing
 * only shell variable declarations and full-line comments.
 *
 * Includes explicit inheritance mechanism intended for use with
 * Red Hat Linux ifcfg-* files.  There is no protection against
 * inheritance loops; they will generally cause stack overflows.
 * Furthermore, they are only intended for one level of inheritance;
 * the value setting algorithm assumes this.
 *
 * Copyright 1999 Red Hat, Inc.
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
 *
 */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shvar.h"

/* Open the file <name>, return shvarFile on success, NULL on failure */
shvarFile *
svNewFile(char *name)
{
    shvarFile *s = NULL;
    int closefd = 0;

    s = calloc(sizeof(shvarFile), 1);
    if (!s) return NULL;

    s->fd = open(name, O_RDWR); /* NOT O_CREAT */
    if (s->fd == -1) {
	/* try read-only */
	s->fd = open(name, O_RDONLY); /* NOT O_CREAT */
	if (s->fd) closefd = 1;
    }
    s->fileName = strdup(name);

    if (s->fd != -1) {
	struct stat buf;
	char *tmp;

	if (fstat(s->fd, &buf) < 0) goto bail;
	s->arena = calloc(buf.st_size, 1);
	if (!s->arena) goto bail;
	if (read(s->fd, s->arena, buf.st_size) < 0) goto bail;
	/* Yes, I know that strtok is evil, except that this is
	 * precisely what it was intended for in the first place...
	 */
	tmp = strtok(s->arena, "\n");
	while (tmp) {
	    s->lineList = g_list_append(s->lineList, tmp);
	    tmp = strtok(NULL, "\n");
	}
	if (closefd) {
	    close(s->fd);
	    s->fd = -1;
	}
    }

    return s;

bail:
    if (s->fd != -1) close(s->fd);
    if (s->arena) free (s->arena);
    if (s->fileName) free (s->fileName);
    free (s);
    return NULL;
}

/* remove escaped characters in place */
static void
unescape(char *s) {
    int len, i;

    len = strlen(s);
    if ((s[0] == '"' || s[0] == '\'') && s[0] == s[len-1]) {
	i = len - 2;
	memmove(s, s+1, i);
	s[i+1] = '\0';
	len = i;
    }
    for (i = 0; i < len; i++) {
	if (s[i] == '\\') {
	    memmove(s+i, s+i+1, len-(i+1));
	    len--;
	}
	s[len] = '\0';
    }
}


/* create a new string with all necessary characters escaped.
 * caller must free returned string
 */
static const char escapees[] = "\"'\\$~`";	/* must be escaped */
static const char spaces[] = " \t";		/* only require "" */
static char *
escape(char *s) {
    char *new;
    int i, j, mangle = 0, space = 0;
    int newlen, slen;
    static int esclen, splen;

    if (!esclen) esclen = strlen(escapees);
    if (!splen) splen = strlen(spaces);
    for (i = 0; i < esclen; i++) {
	if (strchr(s, escapees[i])) mangle++;
    }
    for (i = 0; i < splen; i++) {
	if (strchr(s, spaces[i])) space++;
    }
    if (!mangle && !space) return strdup(s);

    slen = strlen(s);
    newlen = slen + mangle + 3;	/* 3 is extra ""\0 */
    new = calloc(newlen, 1);
    if (!new) return NULL;

    new[0] = '"';
    for (i = 0, j = 1; i < slen; i++, j++) {
	if (strchr(escapees, s[i])) {
	    new[j++] = '\\';
	}
	new[j] = s[i];
    }
    new[j] = '"';

    return new;
}

/* Get the value associated with the key, and leave the current pointer
 * pointing at the line containing the value.  The char* returned MUST
 * be freed by the caller.
 */
char *
svGetValue(shvarFile *s, char *key)
{
    char *value = NULL;
    char *line;
    char *keyString;
    int len;

    assert(s);
    assert(key);

    keyString = calloc (strlen(key) + 2, 1);
    if (!keyString) return NULL;
    strcpy(keyString, key);
    keyString[strlen(key)] = '=';
    len = strlen(keyString);

    for (s->current = s->lineList; s->current; s->current = s->current->next) {
	line = s->current->data;
	if (!strncmp(keyString, line, len)) {
	    value = strdup(line + len);
	    unescape(value);
	    break;
	}
    }
    free(keyString);

    if (value) {
	if (value[0]) {
	    return value;
	} else {
	    free (value);
	    return NULL;
	}
    }
    if (s->parent) value = svGetValue(s->parent, key);
    return value;
}

/* return 1 if <key> resolves to any truth value (e.g. "yes", "y", "true")
 * return 0 if <key> resolves to any non-truth value (e.g. "no", "n", "false")
 * return <default> otherwise
 */
int
svTrueValue(shvarFile *s, char *key, int def)
{
    char *tmp;
    int returnValue = def;

    tmp = svGetValue(s, key);
    if (!tmp) return returnValue;

    if ( (!strcasecmp("yes", tmp)) ||
	 (!strcasecmp("true", tmp)) ||
	 (!strcasecmp("t", tmp)) ||
	 (!strcasecmp("y", tmp)) ) returnValue = 1;
    else
    if ( (!strcasecmp("no", tmp)) ||
	 (!strcasecmp("false", tmp)) ||
	 (!strcasecmp("f", tmp)) ||
	 (!strcasecmp("n", tmp)) ) returnValue = 0;

    free (tmp);
    return returnValue;
}


/* Set the variable <key> equal to the value <value>.
 * If <key> does not exist, and the <current> pointer is set, append
 * the key=value pair after that line.  Otherwise, prepend the pair
 * to the top of the file.  Here's the algorithm, as the C code
 * seems to be rather dense:
 *
 * if (value == NULL), then:
 *     if val2 (parent): change line to key= or append line key=
 *     if val1 (this)  : delete line
 *     else noop
 * else use this table:
 *                                val2
 *             NULL              value               other
 * v   NULL    append line       noop                append line
 * a
 * l   value   noop              noop                noop
 * 1
 *     other   change line       delete line         change line
 *
 * No changes are ever made to the parent config file, only to the
 * specific file passed on the command line.
 *
 */
void
svSetValue(shvarFile *s, char *key, char *value)
{
    char *val1 = NULL, *val2 = NULL;
    char *keyValue;

    assert(s);
    assert(key);
    /* value may be NULL */

    if (value) value = escape(value);
    keyValue = g_malloc (strlen(key) + (value?strlen(value):0) + 2);
    if (!keyValue) return;
    sprintf(keyValue, "%s=%s", key, value?value:"");

    val1 = svGetValue(s, key);
    if (val1 && value && !strcmp(val1, value)) goto bail;
    if (s->parent) val2 = svGetValue(s->parent, key);

    if (!value) {
	/* delete value somehow */
	if (val2) {
	    /* change/append line to get key= */
	    if (s->current) s->current->data = keyValue;
	    else s->lineList = g_list_append(s->lineList, keyValue);
	    s->freeList = g_list_append(s->freeList, keyValue);
	    s->modified = 1;
	} else if (val1) {
	    /* delete line */
	    s->lineList = g_list_remove_link(s->lineList, s->current);
	    g_list_free_1(s->current);
	    s->modified = 1;
	    goto bail; /* do not need keyValue */
	}
	goto end;
    }

    if (!val1) {
	if (val2 && !strcmp(val2, value)) goto end;
	/* append line */
	s->lineList = g_list_append(s->lineList, keyValue);
	s->freeList = g_list_append(s->freeList, keyValue);
	s->modified = 1;
	goto end;
    }

    /* deal with a whole line of noops */
    if (val1 && !strcmp(val1, value)) goto end;

    /* At this point, val1 && val1 != value */
    if (val2 && !strcmp(val2, value)) {
	/* delete line */
	s->lineList = g_list_remove_link(s->lineList, s->current);
	g_list_free_1(s->current);
	s->modified = 1;
	goto bail; /* do not need keyValue */
    } else {
	/* change line */
	if (s->current) s->current->data = keyValue;
	else s->lineList = g_list_append(s->lineList, keyValue);
	s->freeList = g_list_append(s->freeList, keyValue);
	s->modified = 1;
    }

end:
    if (value) free(value);
    if (val1) free(val1);
    if (val2) free(val2);
    return;

bail:
    if (keyValue) free (keyValue);
    goto end;
}

/* Write the current contents iff modified.  Returns -1 on error
 * and 0 on success.  Do not write if no values have been modified.
 * The mode argument is only used if creating the file, not if
 * re-writing an existing file, and is passed unchanged to the
 * open() syscall.
 */
int
svWriteFile(shvarFile *s, int mode)
{
    FILE *f;
    int tmpfd;

    if (s->modified) {
	if (s->fd == -1)
	    s->fd = open(s->fileName, O_WRONLY|O_CREAT, mode);
	if (s->fd == -1)
	    return -1;
	if (ftruncate(s->fd, 0) < 0)
	    return -1;

	tmpfd = dup(s->fd);
	f = fdopen(tmpfd, "w");
	fseek(f, 0, SEEK_SET);
	for (s->current = s->lineList; s->current; s->current = s->current->next) {
	    char *line = s->current->data;
	    fprintf(f, "%s\n", line);
	}
	fclose(f);
    }

    return 0;
}

 
/* Close the file descriptor (if open) and delete the shvarFile.
 * Returns -1 on error and 0 on success.
 */
int
svCloseFile(shvarFile *s)
{

    assert(s);

    if (s->fd != -1) close(s->fd);

    free(s->arena);
    for (s->current = s->freeList; s->current; s->current = s->current->next) {
        free(s->current->data);
    }
    free(s->fileName);
    g_list_free(s->freeList);
    g_list_free(s->lineList); /* implicitly frees s->current */
    free(s);
    return 0;
}
