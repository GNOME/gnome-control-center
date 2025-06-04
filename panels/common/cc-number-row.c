/* cc-number-row.c
 *
 * Copyright 2024 Matthijs Velsink <mvelsink@gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-number-row"

#include "cc-number-row.h"
#include "cc-number-row-enums.h"
#include "cc-util.h"
#include "config.h"

/**
 * CcNumberObject:
 *
 * Simple object that wraps an integer number, with the option of
 * mapping it to a custom string and have it be sorted in a
 * specific manner.
 */

struct _CcNumberObject {
    GObject        parent_instance;

    int            value;
    char          *string;
    CcNumberOrder  order;
};

G_DEFINE_TYPE (CcNumberObject, cc_number_object, G_TYPE_OBJECT);

enum {
    OBJ_PROP_0,
    OBJ_PROP_VALUE,
    OBJ_PROP_STRING,
    OBJ_PROP_ORDER,
    OBJ_N_PROPS
};

static GParamSpec *obj_props[OBJ_N_PROPS];

static void
cc_number_object_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
    CcNumberObject *self = CC_NUMBER_OBJECT (object);

    switch (prop_id) {
    case OBJ_PROP_VALUE:
        g_value_set_int (value, self->value);
        break;
    case OBJ_PROP_STRING:
        g_value_set_string (value, self->string);
        break;
    case OBJ_PROP_ORDER:
        g_value_set_enum (value, self->order);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_number_object_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
    CcNumberObject *self = CC_NUMBER_OBJECT (object);

    switch (prop_id) {
    case OBJ_PROP_VALUE:
        self->value = g_value_get_int (value);
        break;
    case OBJ_PROP_STRING:
        self->string = g_value_dup_string (value);  /* Construct only, no need to free old str */
        break;
    case OBJ_PROP_ORDER:
        self->order = g_value_get_enum (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
cc_number_object_dispose (GObject *object)
{
    CcNumberObject *self = CC_NUMBER_OBJECT (object);

    g_clear_pointer (&self->string, g_free);

    G_OBJECT_CLASS (cc_number_object_parent_class)->dispose (object);
}

static void
cc_number_object_class_init (CcNumberObjectClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = cc_number_object_dispose;
    object_class->get_property = cc_number_object_get_property;
    object_class->set_property = cc_number_object_set_property;

    /**
    * CcNumberObject:value: (attributes org.gtk.Property.get=cc_number_object_get_value)
    *
    * The numeric value.
    */
    obj_props[OBJ_PROP_VALUE] =
        g_param_spec_int ("value", NULL, NULL,
                          INT_MIN, INT_MAX, 0,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberObject:string: (attributes org.gtk.Property.get=cc_number_object_get_string)
    *
    * The (optional) fixed string representation of the stored value.
    */
    obj_props[OBJ_PROP_STRING] =
        g_param_spec_string ("string", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberObject:order: (attributes org.gtk.Property.get=cc_number_object_get_order)
    *
    * The (optional) fixed ordering of the `CcNumberObject` inside a `CcNumberRow` list.
    */
    obj_props[OBJ_PROP_ORDER] =
        g_param_spec_enum ("order", NULL, NULL,
                           CC_TYPE_NUMBER_ORDER, CC_NUMBER_ORDER_DEFAULT,
                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, OBJ_N_PROPS, obj_props);
}

static void
cc_number_object_init (CcNumberObject *self)
{
}


/**
 * cc_number_object_new:
 * @value: the value to store in the `CcNumberObject`
 *
 * Creates a new `CcNumberObject` holding @value.
 *
 * Returns: the newly created `CcNumberObject`
 */
CcNumberObject *
cc_number_object_new (int value)
{
    return g_object_new (CC_TYPE_NUMBER_OBJECT, "value", value, NULL);
}

/**
 * cc_number_object_new_full:
 * @value: the value to store in the `CcNumberObject`
 * @string: (nullable): the fixed string representation of @value
 * @order: the fixed ordering of @value
 *
 * Creates a new `CcNumberObject` holding @value, with special @string
 * representation and @order ordering.
 *
 * Returns: the newly created `CcNumberObject`
 */
CcNumberObject *
cc_number_object_new_full (int            value,
                           const char    *string,
                           CcNumberOrder  order)
{
    return g_object_new (CC_TYPE_NUMBER_OBJECT, "value", value, "string", string,
                         "order", order, NULL);
}

/**
 * cc_number_object_get_value:
 * @self: a `CcNumberObject`
 *
 * Gets the stored value.
 *
 * Returns: the stored value
 */
int
cc_number_object_get_value (CcNumberObject *self)
{
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (self), 0);

    return self->value;
}

/**
 * cc_number_object_get_string:
 * @self: a `CcNumberObject`
 *
 * Gets the fixed string representation of the stored value.
 *
 * Returns: (transfer full) (nullable): the fixed string representation
 */
char*
cc_number_object_get_string (CcNumberObject *self)
{
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (self), NULL);

    return g_strdup (self->string);
}

/**
 * cc_number_object_get_order:
 * @self: a `CcNumberObject`
 *
 * Gets the fixed orderering of @self inside a `CcNumberRow` list.
 *
 * Returns: (nullable): the fixed orderering
 */
CcNumberOrder
cc_number_object_get_order (CcNumberObject *self)
{
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (self), CC_NUMBER_ORDER_DEFAULT);

    return self->order;
}

#define MILLIS_PER_SEC (1000)
#define MILLIS_PER_MIN (60 * MILLIS_PER_SEC)
#define MILLIS_PER_HOUR (60 * MILLIS_PER_MIN)

static char *
cc_number_object_to_string_for_seconds (CcNumberObject *self)
{
    if (self->string)
        return g_strdup (self->string);

    return cc_util_time_to_string_text ((gint64) MILLIS_PER_SEC * self->value);
}

static char *
cc_number_object_to_string_for_minutes (CcNumberObject *self)
{
    if (self->string)
        return g_strdup (self->string);

    return cc_util_time_to_string_text ((gint64) MILLIS_PER_MIN * self->value);
}

static char *
cc_number_object_to_string_for_hours (CcNumberObject *self)
{
    if (self->string)
        return g_strdup (self->string);

    return cc_util_time_to_string_text ((gint64) MILLIS_PER_HOUR * self->value);
}

static char *
cc_number_object_to_string_for_days (CcNumberObject *self)
{
    if (self->string)
        return g_strdup (self->string);

    return g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d day", "%d days", self->value),
                            self->value);
}

/**
 * CcNumberRow:
 *
 * `CcNumberRow` is an `AdwComboRow` with a model that wraps a GListStore of
 * `CcNumberObject`. It has convenient methods to add values directly.
 */

struct _CcNumberRow {
    AdwComboRow  parent_instance;

    GListStore        *store;
    CcNumberValueType  value_type;
    CcNumberSortType   sort_type;

    /* For binding GSettings to the row */
    GSettings *bind_settings;
    char      *bind_key;
    GType      bind_type;
    gulong     number_row_settings_changed_id;
    gulong     number_row_selected_changed_id;
};

G_DEFINE_TYPE(CcNumberRow, cc_number_row, ADW_TYPE_COMBO_ROW)

enum {
    ROW_PROP_0,
    ROW_PROP_VALUE_TYPE,
    ROW_PROP_SORT_TYPE,
    ROW_PROP_VALUES,
    ROW_PROP_SPECIAL_VALUE,
    ROW_N_PROPS
};

static GParamSpec *row_props[ROW_N_PROPS];

static void
cc_number_row_add_values_from_variant (CcNumberRow *self,
                                       GVariant    *variant)
{
    const int *values;
    gsize n_elements, i;
    g_autoptr(GPtrArray) numbers = NULL;

    if (!variant)
        return;

    /* Splice onto the store to not have a notify for every addition */
    values = g_variant_get_fixed_array (variant, &n_elements, sizeof (*values));
    numbers = g_ptr_array_new_full (n_elements, g_object_unref);

    for (i = 0; i < n_elements; i++)
        g_ptr_array_add (numbers, cc_number_object_new (values[i]));

    /* Just splice at the start, it gets sorted in constructed() anyways */
    g_list_store_splice (self->store, 0, 0, numbers->pdata, n_elements);
}

static void
cc_number_row_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    CcNumberRow *self = CC_NUMBER_ROW (object);
    CcNumberObject *number;

    switch (prop_id) {
    case ROW_PROP_VALUE_TYPE:
        self->value_type = g_value_get_enum (value);
        break;
    case ROW_PROP_SORT_TYPE:
        self->sort_type = g_value_get_enum (value);
        break;
    case ROW_PROP_VALUES:
        cc_number_row_add_values_from_variant (self, g_value_get_variant (value));
        break;
    case ROW_PROP_SPECIAL_VALUE:
        /* Construct-only property, so check for NULL, as NULL is passed if not provided */
        number = g_value_get_object (value);
        if (number)
            g_list_store_append (self->store, number);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static int
compare_numbers (CcNumberObject *number_a,
                 CcNumberObject *number_b,
                 CcNumberRow    *self)
{
    /* Handle special order first (works because of the ordering of CcNumberOrder) */
    if (number_a->order != CC_NUMBER_ORDER_DEFAULT ||
        number_b->order != CC_NUMBER_ORDER_DEFAULT)
        return number_a->order - number_b->order;

    /* Otherwise normal value comparison */
    if (self->sort_type == CC_NUMBER_SORT_ASCENDING)
        return number_a->value - number_b->value;
    else if (self->sort_type == CC_NUMBER_SORT_DESCENDING)
        return number_b->value - number_a->value;
    else
        return 0;
}

static void
cc_number_row_constructed (GObject *obj)
{
    CcNumberRow *self = CC_NUMBER_ROW (obj);

    /* Sort now, as construct-only values could be added before sort-type was changed */
    g_list_store_sort (self->store, (GCompareDataFunc) compare_numbers, self);

    /* Only now add it as a model */
    adw_combo_row_set_model (ADW_COMBO_ROW (self), G_LIST_MODEL (self->store));

    /* And set the expression based on the value-type */
    if (self->value_type != CC_NUMBER_VALUE_CUSTOM) {
        char * (*number_to_string_func)(CcNumberObject *);
        g_autoptr(GtkExpression) expression = NULL;

        switch (self->value_type) {
        case CC_NUMBER_VALUE_STRING:
            number_to_string_func = cc_number_object_get_string;
            break;
        case CC_NUMBER_VALUE_SECONDS:
            number_to_string_func = cc_number_object_to_string_for_seconds;
            break;
        case CC_NUMBER_VALUE_MINUTES:
            number_to_string_func = cc_number_object_to_string_for_minutes;
            break;
        case CC_NUMBER_VALUE_HOURS:
            number_to_string_func = cc_number_object_to_string_for_hours;
            break;
        case CC_NUMBER_VALUE_DAYS:
            number_to_string_func = cc_number_object_to_string_for_days;
            break;
        default:
            g_assert_not_reached ();
        }

        expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                                0, NULL,
                                                G_CALLBACK (number_to_string_func),
                                                NULL, NULL);
        adw_combo_row_set_expression (ADW_COMBO_ROW (self), expression);
    }

    G_OBJECT_CLASS (cc_number_row_parent_class)->constructed (obj);
}

static void
cc_number_row_clear_settings_binding (CcNumberRow *self)
{
    g_clear_signal_handler (&self->number_row_settings_changed_id, self->bind_settings);
    g_clear_signal_handler (&self->number_row_selected_changed_id, self);

    g_clear_object (&self->bind_settings);
    g_clear_pointer (&self->bind_key, g_free);

}

static void
cc_number_row_dispose (GObject *object)
{
    CcNumberRow *self = CC_NUMBER_ROW (object);

    cc_number_row_clear_settings_binding (self);

    g_clear_object (&self->store);

    G_OBJECT_CLASS (cc_number_row_parent_class)->dispose (object);
}

static void
cc_number_row_class_init (CcNumberRowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    g_autoptr(GVariantType) values_type = NULL;

    object_class->constructed = cc_number_row_constructed;
    object_class->dispose = cc_number_row_dispose;
    object_class->set_property = cc_number_row_set_property;

    /**
    * CcNumberRow:value-type:
    *
    * The interpretation of the values in the list of the row. Determines what
    * strings will be generated to represent the value in the list. The
    * `string` property of a number will always take priority if it is set.
    * Sets the `expression` property of the underlying AdwComboRow, unless the
    * value type is `CC_NUMBER_VALUE_CUSTOM`.
    */
    row_props[ROW_PROP_VALUE_TYPE] =
        g_param_spec_enum ("value-type", NULL, NULL,
                           CC_TYPE_NUMBER_VALUE_TYPE, CC_NUMBER_VALUE_CUSTOM,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberRow:sort-type:
    *
    * The sorting of the numbers in the list of the row.
    */
    row_props[ROW_PROP_SORT_TYPE] =
        g_param_spec_enum ("sort-type", NULL, NULL,
                           CC_TYPE_NUMBER_SORT_TYPE, CC_NUMBER_SORT_ASCENDING,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberRow:values:
    *
    * A variant array of integer values. Mainly useful in .ui files, where it
    * allows for convenient array notation like [num1, num2, num3, ...].
    */
    values_type = g_variant_type_new_array (G_VARIANT_TYPE_INT32);
    row_props[ROW_PROP_VALUES] =
        g_param_spec_variant ("values", NULL, NULL,
                              values_type, NULL,
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberRow:special-value:
    *
    * One special value to add to the list of the row. Mainly useful in .ui files.
    * If more special values are needed, use `cc_number_row_add_value_full()`.
    */
    row_props[ROW_PROP_SPECIAL_VALUE] =
        g_param_spec_object ("special-value", NULL, NULL,
                             CC_TYPE_NUMBER_OBJECT,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, ROW_N_PROPS, row_props);
}

static void
cc_number_row_init (CcNumberRow *self)
{
    self->store = g_list_store_new (CC_TYPE_NUMBER_OBJECT);
}

/**
 * cc_number_row_new:
 * @sort_type: the sorting of the numbers in the list of the row
 *
 * Creates a new `CcNumberRow`, with sorting based on @sort_type.
 *
 * Returns: the newly created `CcNumberRow`
 */
CcNumberRow *
cc_number_row_new (CcNumberValueType value_type,
                   CcNumberSortType  sort_type)
{
    return g_object_new (CC_TYPE_NUMBER_ROW,
                         "value-type", value_type,
                         "sort-type", sort_type,
                         NULL);
}

static guint
cc_number_row_add_number (CcNumberRow    *self,
                          CcNumberObject *number)
{
    g_return_val_if_fail (CC_IS_NUMBER_ROW (self), 0);
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (number), 0);

    return g_list_store_insert_sorted (self->store, number,
                                       (GCompareDataFunc) compare_numbers, self);
}

/**
 * cc_number_row_add_value:
 * @self: a `CcNumberRow`
 * @value: the value to store in the list of @self
 *
 * Adds a new `CcNumberObject` based on @value to the list of @self.
 * The value will be inserted with the correct sorting.
 *
 * Also see `cc_number_object_new()`.
 *
 * Returns: the position in the list where the value got stored
 */
guint
cc_number_row_add_value (CcNumberRow *self,
                         int          value)
{
    g_autoptr(CcNumberObject) number = cc_number_object_new (value);

    return cc_number_row_add_number (self, number);
}

/**
 * cc_number_row_add_value_full:
 * @self: a `CcNumberRow`
 * @value: the value to store in the list of @self
 * @string: (nullable): the fixed string representation of @value
 * @order: the fixed ordering of @value
 *
 * Adds a new `CcNumberObject` based on @value, @string, and @order to the
 * list of @self.
 * The value will be inserted with the correct sorting, which takes @order
 * into account. If two `CcNumberObject`s have the same special @order, their
 * ordering is based on the order in which they were added to the list.
 *
 * Also see `cc_number_object_new_full()`.
 *
 * Returns: the position in the list where the value got stored
 */
guint
cc_number_row_add_value_full (CcNumberRow   *self,
                              int            value,
                              const char    *string,
                              CcNumberOrder  order)
{
    g_autoptr(CcNumberObject) number = cc_number_object_new_full (value, string, order);

    return cc_number_row_add_number (self, number);
}

/**
 * cc_number_row_get_value:
 * @self: a `CcNumberRow`
 * @position: the position of the value to fetch
 *
 * Get the value at @position in the list of @self.
 *
 * Returns: the value at @position
 */
int
cc_number_row_get_value (CcNumberRow *self,
                         guint        position)
{
    g_autoptr(CcNumberObject) number = NULL;

    g_return_val_if_fail (CC_IS_NUMBER_ROW (self), -1);

    number = g_list_model_get_item (G_LIST_MODEL (self->store), position);

    g_return_val_if_fail (number != NULL, -1);

    return number->value;
}

static gboolean
equal_numbers (CcNumberObject *number,
               gconstpointer   not_a_number,
               int            *value)
{
    if (!number)
        return FALSE;

    return number->value == *value;
}

/**
 * cc_number_row_has_value:
 * @self: a `CcNumberRow`
 * @value: a value
 * @position: (out) (optional): the first position of @value, if it was found
 *
 * Looks up the given @value in the list of @self. If the value is not found,
 * @position will not be set and the return value will be false.
 *
 * Returns: true if the list contains @value, false otherwise
 */
gboolean
cc_number_row_has_value (CcNumberRow *self,
                         int          value,
                         guint       *position)
{
    g_return_val_if_fail (CC_IS_NUMBER_ROW (self), FALSE);

    return g_list_store_find_with_equal_func_full (self->store, NULL,
                                                   (GEqualFuncFull) equal_numbers, &value,
                                                   position);
}

static void
number_row_settings_changed_cb (CcNumberRow *self)
{
    int value;
    guint position = 0;

    switch (self->bind_type) {
    case G_TYPE_UINT:
        if (g_settings_get_uint (self->bind_settings, self->bind_key) <= INT_MAX) {
            value = g_settings_get_uint (self->bind_settings, self->bind_key);
        } else {
            g_warning ("Unsigned GSettings value out of range for CcNumberRow");
            position = GTK_INVALID_LIST_POSITION;
        }
        break;
    case G_TYPE_INT:
        value = g_settings_get_int (self->bind_settings, self->bind_key);
        break;
    case G_TYPE_ENUM:
        value = g_settings_get_enum (self->bind_settings, self->bind_key);
        break;
    default:
        g_assert_not_reached ();
    }

    g_signal_handler_block (self, self->number_row_selected_changed_id);
    if (position != GTK_INVALID_LIST_POSITION)
        if (!cc_number_row_has_value (self, value, &position))
            position = cc_number_row_add_value (self, value);

    adw_combo_row_set_selected (ADW_COMBO_ROW (self), position);
    g_signal_handler_unblock (self, self->number_row_selected_changed_id);
}

static void
number_row_selected_changed_cb (CcNumberRow *self)
{
    guint position;
    int value;

    position = adw_combo_row_get_selected (ADW_COMBO_ROW (self));
    g_return_if_fail (position != GTK_INVALID_LIST_POSITION);

    value = cc_number_row_get_value (self, position);

    g_signal_handler_block (self->bind_settings, self->number_row_settings_changed_id);
    switch (self->bind_type) {
    case G_TYPE_UINT:
        if (value >= 0)
            g_settings_set_uint (self->bind_settings, self->bind_key, value);
        else
            g_warning ("Negative CcNumberRow value out of range for unsigned GSettings value");
        break;
    case G_TYPE_INT:
        g_settings_set_int (self->bind_settings, self->bind_key, value);
        break;
    case G_TYPE_ENUM:
        g_settings_set_enum (self->bind_settings, self->bind_key, value);
        break;
    default:
        g_assert_not_reached ();
    }
    g_signal_handler_unblock (self->bind_settings, self->number_row_settings_changed_id);
}

/**
 * cc_number_row_bind_settings:
 * @self: a `CcNumberRow`
 * @settings: a `GSettings` object
 * @key: the key to bind
 *
 * Creates a binding between the @key in the @settings object, and the
 * selected value of @self. The value type of @key must be an (unsigned)
 * integer or an enum.
 *
 * If the value of @key does not exist yet in the the list of @self, it
 * will be added.
 *
 * If a binding already existed, it will be removed and replaced by the
 * new binding.
 */
void
cc_number_row_bind_settings (CcNumberRow *self,
                             GSettings   *settings,
                             const char  *key)
{
    g_autoptr(GSettingsSchema) schema = NULL;
    g_autoptr(GSettingsSchemaKey) schema_key = NULL;
    const GVariantType *value_type;
    g_autoptr(GVariant) schema_range = NULL;
    const char *key_type;
    g_autofree char *detailed_changed_key = NULL;

    g_return_if_fail (CC_IS_NUMBER_ROW (self));
    g_return_if_fail (G_IS_SETTINGS (settings));

    cc_number_row_clear_settings_binding (self);

    /* Extract the detailed key type, which includes "enum". Convoluted api... */
    g_object_get (settings, "settings-schema", &schema, NULL);

    g_return_if_fail (g_settings_schema_has_key (schema, key));

    schema_key = g_settings_schema_get_key (schema, key);
    value_type = g_settings_schema_key_get_value_type (schema_key);
    schema_range = g_settings_schema_key_get_range (schema_key);
    g_variant_get (schema_range, "(&sv)", &key_type, NULL);

    /* Make sure the key has (u)int or enum value type, otherwise it can't map to a CcNumberRow */
    if (g_variant_type_equal (value_type, G_VARIANT_TYPE_UINT32)) {
        self->bind_type = G_TYPE_UINT;
    } else if (g_variant_type_equal (value_type, G_VARIANT_TYPE_INT32)) {
        self->bind_type = G_TYPE_INT;
    } else if (g_strcmp0 (key_type, "enum") == 0) {
        self->bind_type = G_TYPE_ENUM;
    } else {
        g_critical ("GSettings key type must be uint, int or enum");
        return;
    }

    self->bind_settings = g_object_ref (settings);
    self->bind_key = g_strdup (key);
    detailed_changed_key = g_strdup_printf ("changed::%s", key);

    self->number_row_settings_changed_id =
        g_signal_connect_swapped (settings, detailed_changed_key,
                                  G_CALLBACK (number_row_settings_changed_cb), self);

    self->number_row_selected_changed_id =
        g_signal_connect (self, "notify::selected", G_CALLBACK (number_row_selected_changed_cb), NULL);

    number_row_settings_changed_cb (self);
}

/**
 * cc_number_row_unbind_settings:
 * @self: a `CcNumberRow`
 *
 * Removes the `GSettings` binding that was created using
 * `cc_number_row_bind_settings()`.
 *
 * This function is always safe to call, nothing happens if there was no
 * binding.
 */
void
cc_number_row_unbind_settings (CcNumberRow *self)
{
    g_return_if_fail (CC_IS_NUMBER_ROW (self));

    cc_number_row_clear_settings_binding (self);
}
