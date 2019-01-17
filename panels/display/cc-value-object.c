#include "cc-value-object.h"
#include <gobject/gvaluecollector.h>

struct _CcValueObject
{
  GObject parent_instance;

  GValue  value;
};

G_DEFINE_TYPE (CcValueObject, cc_value_object, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_VALUE,
  N_PROPS
};

static GParamSpec *props [N_PROPS];

CcValueObject *
cc_value_object_new (const GValue *value)
{
  return g_object_new (CC_TYPE_VALUE_OBJECT,
                       "value", value,
                       NULL);
}

/**
 * cc_value_object_new_collect(): (skip)
 *
 */
CcValueObject*
cc_value_object_new_collect (GType type, ...)
{
  GValue value = G_VALUE_INIT;
  g_autofree gchar *error = NULL;
  va_list var_args;

  va_start (var_args, type);

  G_VALUE_COLLECT_INIT (&value, type, var_args, 0, &error);

  va_end (var_args);

  if (error)
    g_critical ("%s: %s", G_STRFUNC, error);

  return g_object_new (CC_TYPE_VALUE_OBJECT,
                       "value", &value,
                       NULL);
}

static void
cc_value_object_finalize (GObject *object)
{
  CcValueObject *self = (CcValueObject *)object;

  g_value_unset (&self->value);

  G_OBJECT_CLASS (cc_value_object_parent_class)->finalize (object);
}

static void
cc_value_object_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  CcValueObject *self = CC_VALUE_OBJECT (object);

  switch (prop_id)
    {
    case PROP_VALUE:
      g_value_set_boxed (value, &self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_value_object_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  CcValueObject *self = CC_VALUE_OBJECT (object);
  GValue *real_value;

  switch (prop_id)
    {
    case PROP_VALUE:
      /* construct only */
      real_value = g_value_get_boxed (value);
      g_value_init (&self->value, real_value->g_type);
      g_value_copy (real_value, &self->value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_value_object_class_init (CcValueObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_value_object_finalize;
  object_class->get_property = cc_value_object_get_property;
  object_class->set_property = cc_value_object_set_property;

  props[PROP_VALUE] =
    g_param_spec_boxed ("value", "Value",
                         "The contained value",
                         G_TYPE_VALUE,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class,
                                     N_PROPS,
                                     props);
}

static void
cc_value_object_init (CcValueObject *self)
{
}

const GValue*
cc_value_object_get_value (CcValueObject *self)
{
  return &self->value;
}

void
cc_value_object_copy_value (CcValueObject *self,
                            GValue        *dest_value)
{
  g_value_copy (&self->value, dest_value);
}

const gchar*
cc_value_object_get_string (CcValueObject *self)
{
  return g_value_get_string (&self->value);
}

gchar*
cc_value_object_dup_string (CcValueObject *self)
{
  return g_value_dup_string (&self->value);
}

