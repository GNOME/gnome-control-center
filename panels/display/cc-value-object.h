#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define CC_TYPE_VALUE_OBJECT (cc_value_object_get_type())

G_DECLARE_FINAL_TYPE (CcValueObject, cc_value_object, CC, VALUE_OBJECT, GObject)

CcValueObject  *cc_value_object_new (const GValue *value);
CcValueObject  *cc_value_object_new_collect (GType type, ...);

const GValue   *cc_value_object_get_value  (CcValueObject *self);
void            cc_value_object_copy_value (CcValueObject *self,
                                            GValue        *dest_value);

const gchar    *cc_value_object_get_string  (CcValueObject *self);
gchar          *cc_value_object_dup_string  (CcValueObject *self);

G_END_DECLS
