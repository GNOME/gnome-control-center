/* Copyright (C) 1997-1999 Red Hat Software, Inc.
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

#ifndef __USERHELPER_H__
#define __USERHELPER_H__

#define _(String) gettext(String)

#define UH_ECHO_ON_PROMPT 1
#define UH_ECHO_OFF_PROMPT 2
#define UH_INFO_MSG 3
#define UH_ERROR_MSG 4
#define UH_EXPECT_RESP 5
#define UH_SERVICE_NAME 6
#define UH_FALLBACK 7
#define UH_USER 8

#define UH_TEXT 1
#define UH_ABORT 2

#define ERR_PASSWD_INVALID      1       /* password is not right */
#define ERR_FIELDS_INVALID      2       /* gecos fields invalid or
                                         * sum(lengths) too big */
#define ERR_SET_PASSWORD        3       /* password resetting error */
#define ERR_LOCKS               4       /* some files are locked */
#define ERR_NO_USER             5       /* user unknown ... */
#define ERR_NO_RIGHTS           6       /* insufficient rights  */
#define ERR_INVALID_CALL        7       /* invalid call to this program */
#define ERR_SHELL_INVALID       8       /* invalid call to this program */
#define ERR_NO_MEMORY		9	/* out of memory */
#define ERR_NO_PROGRAM		10	/* -w progname not found */
#define ERR_EXEC_FAILED		11	/* exec failed for some reason */
#define ERR_UNK_ERROR           255     /* unknown error */

#define UH_PATH "/gnome/sbin/root-manager"
#define UH_KEY_PIXMAP_PATH "/usr/share/pixmaps/userhelper-keys.xpm"
/*#define UH_PATH "./root-manager"*/

#endif /* __USERHELPER_H__ */
