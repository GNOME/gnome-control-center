/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright (C) 2024 Adrien Plazas <aplazas@gnome.org>
 *
 * Licensed under the GNU General Public License Version 2
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/**
 * ce_option_or:
 * @a: (nullable): a pointer, or %NULL
 * @b: (nullable): a pointer, or %NULL
 *
 * A macro that uneagerly evaluates the arguments and returns @a if non-%NULL,
 * and @b otherwise.
 *
 * It means @a is evaluated only once, and @b is evaluated only if @a is %NULL.
 * In comparison, (@a != NULL ? @a : @b) may evaluate @a twice, and a function
 * would evaluate both arguments once.
 *
 * It is inspired by Rust's [`Option.or(…)`](https://doc.rust-lang.org/std/option/enum.Option.html#method.or),
 * and [`Option.or_else(…)`](https://doc.rust-lang.org/std/option/enum.Option.html#method.or_else).
 *
 * Returns: @a if non-%NULL, @b otherwise
 **/
#if defined(glib_typeof)
#define ce_option_or(a, b)                                                                                             \
    (G_GNUC_EXTENSION ({                                                                                               \
        G_STATIC_ASSERT (sizeof (*(a)) == sizeof (*(b)));                                                              \
        glib_typeof (*(a)) * a_temp_val;                                                                               \
        a_temp_val = (a);                                                                                              \
        a_temp_val != NULL ? a_temp_val : (b);                                                                         \
    }))
#else
#define ce_option_or(a, b)                                                                                             \
    (G_GNUC_EXTENSION ({                                                                                               \
        G_STATIC_ASSERT (sizeof (*(a)) == sizeof (*(b)));                                                              \
        gpointer a_temp_val;                                                                                           \
        a_temp_val = (a);                                                                                              \
        a_temp_val != NULL ? a_temp_val : (b);                                                                         \
    }))
#endif

/**
 * ce_option_nonzero_or:
 * @a: an integer
 * @b: an integer
 *
 * A macro that uneagerly evaluates the arguments and returns @a if non-zero,
 * and @b otherwise.
 *
 * It means @a is evaluated only once, and @b is evaluated only if @a is zero.
 * In comparison, (@a != 0 ? @a : @b) may evaluate @a twice, and a function
 * would evaluate both arguments once.
 *
 * It is inspired by Rust's [`Option<NonZero>.or(…)`](https://doc.rust-lang.org/std/option/enum.Option.html#method.or),
 * and [`Option<NonZero>.or_else(…)`](https://doc.rust-lang.org/std/option/enum.Option.html#method.or_else).
 *
 * Returns: @a if non-%NULL, @b otherwise
 **/
#if defined(glib_typeof)
#define ce_option_nonzero_or(a, b)                                                                                     \
    (G_GNUC_EXTENSION ({                                                                                               \
        glib_typeof ((a)) a_temp_val;                                                                                  \
        a_temp_val = (a);                                                                                              \
        a_temp_val != 0 ? a_temp_val : (b);                                                                            \
    }))
#else
#define ce_option_nonzero_or(a, b)                                                                                     \
    (G_GNUC_EXTENSION ({                                                                                               \
        gint64 a_temp_val;                                                                                             \
        a_temp_val = (a);                                                                                              \
        a_temp_val != 0 ? a_temp_val : (b);                                                                            \
    }))
#endif

/**
 * ce_str_nullify:
 * @str: (nullable): a string, or %NULL
 *
 * A function that returns @str if non-empty, and %NULL otherwise.
 *
 * Returns: @str if non-empty, %NULL otherwise
 **/
static inline const gchar *
ce_str_nullify (const gchar *str)
{
    return str != NULL && *str != '\0' ? str : NULL;
}

/**
 * ce_strv_strip:
 * @strv: (nullable): a %NULL-terminated string array
 *
 * Removes leading and trailing whitespace from the strings.
 *
 * This function doesn't allocate or reallocate any memory; it modifies the
 * strings in place. Therefore, it cannot be used on arrays containing
 * statically allocated strings.
 *
 * If @strv is %NULL, this function does nothing.
 *
 * The pointer to @strv is returned to allow the nesting of functions.
 *
 * Returns: @strv
 **/
static inline gchar **
ce_strv_strip (gchar **strv)
{
    if (strv == NULL) {
        return NULL;
    }

    for (gchar **strv_i = strv; *strv_i != NULL; strv_i++) {
        g_strstrip (*strv_i);
    }

    return strv;
}

G_END_DECLS
