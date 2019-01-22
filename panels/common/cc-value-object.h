/*
 * Copyright (C) 2019 Red Hat Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define CC_TYPE_VALUE_OBJECT (cc_value_object_get_type())

G_DECLARE_FINAL_TYPE (CcValueObject, cc_value_object, CC, VALUE_OBJECT, GObject)

CcValueObject *cc_value_object_new             (const GValue *value);
CcValueObject *cc_value_object_new_collect     (GType         type,
                                                ...);
CcValueObject *cc_value_object_new_string      (const gchar  *string);
CcValueObject *cc_value_object_new_take_string (gchar        *string);

const GValue*   cc_value_object_get_value  (CcValueObject *value);
void            cc_value_object_copy_value (CcValueObject *value,
                                            GValue         *dest);
const gchar*    cc_value_object_get_string (CcValueObject *value);
gchar*          cc_value_object_dup_string (CcValueObject *value);

G_END_DECLS
