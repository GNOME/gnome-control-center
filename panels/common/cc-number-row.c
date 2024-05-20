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
    * The (optional) fixed ordering of the `CcNumberObject` inside a `CcNumberList`.
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
 * Returns: (nullable): the fixed string representation
 */
const char*
cc_number_object_get_string (CcNumberObject *self)
{
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (self), NULL);

    return self->string;
}

/**
 * cc_number_object_get_order:
 * @self: a `CcNumberObject`
 *
 * Gets the fixed orderering of @self inside a `CcNumberList`.
 *
 * Returns: (nullable): the fixed orderering
 */
CcNumberOrder
cc_number_object_get_order (CcNumberObject *self)
{
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (self), CC_NUMBER_ORDER_DEFAULT);

    return self->order;
}

/**
 * cc_number_object_to_string_for_seconds:
 * @self: a `CcNumberObject`
 *
 * Gets the string representation of @self, assuming the stored value is
 * a number of seconds. If @self has the special `string` set, that is
 * returned instead.
 *
 * This function is useful in expressions.
 *
 * Returns: (transfer full): the resulting string
 */
char *
cc_number_object_to_string_for_seconds (CcNumberObject *self)
{
    if (self->string)
        return g_strdup (self->string);

    return cc_util_time_to_string_text ((gint64) 1000 * self->value);
}

/**
 * cc_number_object_to_string_for_minutes:
 * @self: a `CcNumberObject`
 *
 * Gets the string representation of @self, assuming the stored value is
 * a number of minutes. If @self has the special `string` set, that is
 * returned instead.
 *
 * This function is useful in expressions.
 *
 * Returns: (transfer full): the resulting string
 */
char *
cc_number_object_to_string_for_minutes (CcNumberObject *self)
{
    if (self->string)
        return g_strdup (self->string);

    return cc_util_time_to_string_text ((gint64) 1000 * 60 * self->value);
}

/**
 * CcNumberList:
 *
 * `CcNumberList` is a simple list model that wraps a GListStore of
 * `CcStringObject`. It has convenient methods to add values directly.
 */

struct _CcNumberList {
    GObject      parent_instance;

    GListStore  *store;
    GtkSortType  sort_type;
};

static GType
cc_number_list_get_item_type (GListModel *list)
{
    CcNumberList *self = CC_NUMBER_LIST (list);

    return g_list_model_get_item_type (G_LIST_MODEL (self->store));
}

static guint
cc_number_list_get_n_items (GListModel *list)
{
    CcNumberList *self = CC_NUMBER_LIST (list);

    return g_list_model_get_n_items (G_LIST_MODEL (self->store));
}

static gpointer
cc_number_list_get_item (GListModel *list,
                         guint       position)
{
    CcNumberList *self = CC_NUMBER_LIST (list);

    return g_list_model_get_item (G_LIST_MODEL (self->store), position);
}

static void
cc_number_list_model_init (GListModelInterface *iface)
{
    iface->get_item_type = cc_number_list_get_item_type;
    iface->get_n_items = cc_number_list_get_n_items;
    iface->get_item = cc_number_list_get_item;
}

G_DEFINE_TYPE_WITH_CODE (CcNumberList, cc_number_list, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                                                cc_number_list_model_init))

enum {
    LST_PROP_0,
    LST_PROP_SORT_TYPE,
    LST_PROP_VALUES,
    LST_PROP_SPECIAL_VALUE,
    LST_N_PROPS
};

static GParamSpec *lst_props[LST_N_PROPS];

static void
cc_number_list_add_values_from_variant (CcNumberList *self,
                                        GVariant     *variant)
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
cc_number_list_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
    CcNumberList *self = CC_NUMBER_LIST (object);
    CcNumberObject *number;

    switch (prop_id) {
    case LST_PROP_SORT_TYPE:
        self->sort_type = g_value_get_enum (value);
        break;
    case LST_PROP_VALUES:
        cc_number_list_add_values_from_variant (self, g_value_get_variant (value));
        break;
    case LST_PROP_SPECIAL_VALUE:
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
                 CcNumberList   *self)
{
    /* Handle special order first (works because of the ordering of CcNumberOrder) */
    if (number_a->order != CC_NUMBER_ORDER_DEFAULT ||
        number_b->order != CC_NUMBER_ORDER_DEFAULT)
        return number_a->order - number_b->order;

    /* Otherwise normal value comparison */
    if (self->sort_type == GTK_SORT_ASCENDING)
        return number_a->value - number_b->value;
    else
        return number_b->value - number_a->value;
}

static void
cc_number_list_constructed (GObject *obj)
{
    CcNumberList *self = CC_NUMBER_LIST (obj);

    /* Sort now, as construct-only values could be added before sort-type was changed */
    g_list_store_sort (self->store, (GCompareDataFunc) compare_numbers, self);

    G_OBJECT_CLASS (cc_number_list_parent_class)->constructed (obj);
}

static void
cc_number_list_dispose (GObject *object)
{
    CcNumberList *self = CC_NUMBER_LIST (object);

    g_clear_object (&self->store);

    G_OBJECT_CLASS (cc_number_list_parent_class)->dispose (object);
}

static void
cc_number_list_class_init (CcNumberListClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    g_autoptr(GVariantType) values_type = NULL;

    object_class->constructed = cc_number_list_constructed;
    object_class->dispose = cc_number_list_dispose;
    object_class->set_property = cc_number_list_set_property;

    /**
    * CcNumberList:sort-type:
    *
    * The sorting of the numbers in the list.
    */
    lst_props[LST_PROP_SORT_TYPE] =
        g_param_spec_enum ("sort-type", NULL, NULL,
                           GTK_TYPE_SORT_TYPE, GTK_SORT_ASCENDING,
                           G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberList:values:
    *
    * A variant array of integer values. Mainly useful in .ui files, where it
    * allows for convenient array notation like [num1, num2, num3, ...].
    */
    values_type = g_variant_type_new_array (G_VARIANT_TYPE_INT32);
    lst_props[LST_PROP_VALUES] =
        g_param_spec_variant ("values", NULL, NULL,
                              values_type, NULL,
                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    /**
    * CcNumberList:special-value:
    *
    * One special value to add to the list. Mainly useful in .ui files.
    * If more special values are needed, use `cc_number_list_add_value_full()`.
    */
    lst_props[LST_PROP_SPECIAL_VALUE] =
        g_param_spec_object ("special-value", NULL, NULL,
                             CC_TYPE_NUMBER_OBJECT,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, LST_N_PROPS, lst_props);
}

static void
cc_number_list_init (CcNumberList *self)
{
    self->store = g_list_store_new (CC_TYPE_NUMBER_OBJECT);

    /* Simply loop the GListModel signal back around to self */
    g_signal_connect_swapped (self->store, "items-changed",
                              G_CALLBACK (g_list_model_items_changed), self);
}

/**
 * cc_number_list_new:
 * @sort_type: the sorting of the numbers in the list
 *
 * Creates a new `CcNumberList`, with sorting based on @sort_type.
 *
 * Returns: the newly created `CcNumberList`
 */
CcNumberList *
cc_number_list_new (GtkSortType sort_type)
{
    return g_object_new (CC_TYPE_NUMBER_LIST,
                         "sort-type", sort_type,
                         NULL);
}

static guint
cc_number_list_add_number (CcNumberList   *self,
                           CcNumberObject *number)
{
    g_return_val_if_fail (CC_IS_NUMBER_LIST (self), 0);
    g_return_val_if_fail (CC_IS_NUMBER_OBJECT (number), 0);

    return g_list_store_insert_sorted (self->store, number,
                                       (GCompareDataFunc) compare_numbers, self);
}

/**
 * cc_number_list_add_value:
 * @self: a `CcNumberList`
 * @value: the value to store in the list
 *
 * Adds a new `CcNumberObject` based on @value to the list.
 * The value will be inserted with the correct sorting.
 *
 * Also see `cc_number_object_new()`.
 *
 * Returns: the position in the list where the value got stored
 */
guint
cc_number_list_add_value (CcNumberList *self,
                          int           value)
{
    g_autoptr(CcNumberObject) number = cc_number_object_new (value);

    return cc_number_list_add_number (self, number);
}

/**
 * cc_number_list_add_value_full:
 * @self: a `CcNumberList`
 * @value: the value to store in the list
 * @string: (nullable): the fixed string representation of @value
 * @order: the fixed ordering of @value
 *
 * Adds a new `CcNumberObject` based on @value, @string, and @order to the
 * list.
 * The value will be inserted with the correct sorting, which takes @order
 * into account. If two `CcNumberObject`s have the same special @order, their
 * ordering is based on the order in which they were added to the list.
 *
 * Also see `cc_number_object_new_full()`.
 *
 * Returns: the position in the list where the value got stored
 */
guint
cc_number_list_add_value_full (CcNumberList  *self,
                               int            value,
                               const char    *string,
                               CcNumberOrder  order)
{
    g_autoptr(CcNumberObject) number = cc_number_object_new_full (value, string, order);

    return cc_number_list_add_number (self, number);
}

/**
 * cc_number_list_get_value:
 * @self: a `CcNumberList`
 * @position: the position of the value to fetch
 *
 * Get the value at @position.
 *
 * Returns: the value at @position
 */
int
cc_number_list_get_value (CcNumberList *self,
                          guint         position)
{
    g_autoptr(CcNumberObject) number = NULL;

    g_return_val_if_fail (CC_IS_NUMBER_LIST (self), -1);

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
 * cc_number_list_has_value:
 * @self: a `CcNumberList`
 * @value: a value
 * @position: (out) (optional): the first position of @value, if it was found
 *
 * Looks up the given @value in the list. If the value is not found, @position
 * will not be set and the return value will be false.
 *
 * Returns: true if the list contains @value, false otherwise
 */
gboolean
cc_number_list_has_value (CcNumberList *self,
                          int           value,
                          guint        *position)
{
    g_return_val_if_fail (CC_IS_NUMBER_LIST (self), FALSE);

    return g_list_store_find_with_equal_func_full (self->store, NULL,
                                                   (GEqualFuncFull) equal_numbers, &value,
                                                   position);
}
