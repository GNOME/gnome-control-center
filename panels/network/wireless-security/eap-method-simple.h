/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2007 - 2010 Red Hat, Inc.
 */

#ifndef EAP_METHOD_SIMPLE_H
#define EAP_METHOD_SIMPLE_H

#include "wireless-security.h"

typedef enum {
	/* NOTE: when updating this table, also update eap_methods[] */
	EAP_METHOD_SIMPLE_TYPE_PAP = 0,
	EAP_METHOD_SIMPLE_TYPE_MSCHAP,
	EAP_METHOD_SIMPLE_TYPE_MSCHAP_V2,
	EAP_METHOD_SIMPLE_TYPE_MD5,
	EAP_METHOD_SIMPLE_TYPE_PWD,
	EAP_METHOD_SIMPLE_TYPE_CHAP,
	EAP_METHOD_SIMPLE_TYPE_GTC,

	/* Boundary value, do not use */
	EAP_METHOD_SIMPLE_TYPE_LAST
} EAPMethodSimpleType;

typedef enum {
	EAP_METHOD_SIMPLE_FLAG_NONE            = 0x00,
	/* Indicates the EAP method is an inner/phase2 method */
	EAP_METHOD_SIMPLE_FLAG_PHASE2          = 0x01,
	/* Set by TTLS to indicate that inner/phase2 EAP is allowed */
	EAP_METHOD_SIMPLE_FLAG_AUTHEAP_ALLOWED = 0x02,
	/* Set from nm-connection-editor or the GNOME network panel */
	EAP_METHOD_SIMPLE_FLAG_IS_EDITOR       = 0x04,
	/* Set to indicate that this request is only for secrets */
	EAP_METHOD_SIMPLE_FLAG_SECRETS_ONLY    = 0x08
} EAPMethodSimpleFlags;

typedef struct _EAPMethodSimple EAPMethodSimple;

EAPMethodSimple *eap_method_simple_new (WirelessSecurity *ws_parent,
                                        NMConnection *connection,
                                        EAPMethodSimpleType type,
                                        EAPMethodSimpleFlags flags);

#endif /* EAP_METHOD_SIMPLE_H */

