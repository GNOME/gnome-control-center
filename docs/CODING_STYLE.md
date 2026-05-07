# Style

GNOME Settings uses clang-format rules for a consistent code style.
Please read them carefully and, if in doubt, ask a maintainer for directions.

## clang-format

C sources are meant to match the configuration in the **`.clang-format`** file.
Before opening a merge request, format touched files, for example:

```sh
clang-format -i path/to/file.c
```

## General

The most important rule is: **see the surrounding code, and copy its style**, and
**run `clang-format`** on modified C files so they stay consistent.

Broad conventions:

 * **4 spaces** of indentation
 * **120 columns** maximum line length (soft limit enforced by `clang-format`)
 * **Function definitions:** return type (and `static`, if any) go on the line(s)
   before the function name; the opening `{` of the function body is on its own
   line
 * **Control flow with braces:** `if (` … `) {` keeps `{` on the same line as
   the closing `)`

## Comments

Comment blocks should be formatted as following:

```c
/* Single line comment */

/* Multiline comments start at the first line of the comment block,
 * but have the closing slash a line after. Every line starts with
 * an asterisk that is aligned with the rest of the block.
 */
```

Use `/* ... */` for comments in C sources (not `//`).

## Conditionals

Conditionals should either be all in one line, or one condition per line.
Continuation lines are aligned following GNU/`clang-format` rules (see
`BreakBeforeBinaryOperators`).

Some examples below:

```c
/* Single line if */
if (a || b || (c && d))
    return;

/* Multiline if */
if (long_boolean_variable_used_in_this_condition_a ||
    long_boolean_variable_used_in_this_condition_b ||
    (long_boolean_variable_used_in_this_condition_c &&
     long_boolean_variable_used_in_this_condition_d))
    return;

/* Braced body: brace stays on the same line as the closing parenthesis */
if (something) {
    do_work ();
    if (nested)
        early_exit ();
}
```

## Structs and Enums

Structures and enums should follow the layout `clang-format` applies after the
GNU base style, for example:

```c
struct _FooBar {
    guint32 field_one;
    gchar *text;
};

typedef struct {
    FooParent parent;

    guint32 field_one;
    gchar *text;

    struct {
        CustomType *something;
        guint something_else;
    } inner_struct;

    gboolean flag : 1;
} FooBar;

enum {
    FIRST,
    SECOND,
    LAST,
};

typedef enum {
    FOO_BAR_FIRST,
    FOO_BAR_SECOND,
    FOO_BAR_LAST,
} FooEnumBar;
```

## Header (.h) files

Headers are organized by the following structure:

 1. GPL header
 2. Local includes
 3. System includes
 4. `G_BEGIN_DECLS`
 5. `#defines`
 6. `G_DECLARE_{FINAL,DERIVABLE}_TYPE`
 7. Public API
 8. `G_END_DECLS`

Apply `clang-format` to `.h` files as well as `.c` files so prototypes and types
stay consistent. The following conventions still matter on top of formatting:

 * The `*` and the type name are written as `gchar *foo`, not `gchar * foo`.
 * No newline at the end of the file (POSIX text files end with a newline. Avoid
   extra blank lines beyond the final newline. `clang-format` usually keeps this tidy).

As an example, this is how the start of a small header can look (adapted from
`shell/cc-object-storage.h`):

```c
/* cc-object-storage.h
 *
 * Copyright 2018 …
 * …
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

/* Default storage keys */
#define CC_OBJECT_NMCLIENT "CcObjectStorage::nm-client"

#define CC_TYPE_OBJECT_STORAGE (cc_object_storage_get_type ())
G_DECLARE_FINAL_TYPE (CcObjectStorage, cc_object_storage, CC, OBJECT_STORAGE, GObject);
gboolean cc_object_storage_has_object (const gchar *key);

void cc_object_storage_add_object (const gchar *key, gpointer object);

gpointer cc_object_storage_create_dbus_proxy_sync (GBusType bus_type, GDBusProxyFlags flags, const gchar *name,
                                                   const gchar *path, const gchar *interface, GCancellable *cancellable,
                                                   GError **error);

G_END_DECLS
```

## Source code

The source file keeps an order of methods. The order will be as following:

  1. GPL header
  2. Structures
  3. Function prototypes
  4. `G_DEFINE_TYPE()`
  5. Enums
  6. Static variables
  7. Auxiliary methods
  8. Callbacks
  9. Interface implementations
  10. Parent class overrides
  11. class_init and init
  12. Public API

### Structures

Structure members are indented with four spaces. `clang-format` aligns pointers
and similar types within the struct when needed.

```c
typedef struct {
    GBusType bus_type;
    GDBusProxyFlags flags;
    gchar *name;
    gchar *path;
    gchar *interface;
    gboolean cached;
} TaskData;
```

### Function Prototypes

Function prototypes in `.c` files should match the style of the corresponding
declarations in headers (run `clang-format` on both).

### Auxiliary Methods

Auxiliary method names must have a verb in the dictionary form, and should always
perform an action over something. They don't have the `cc_` prefix. For example:

```c
static void
execute_something_on_data (Foo *data,
                           Bar *bar)
{
    /* ... */
}
```

### Callbacks

 * Callbacks always have the `_cb` suffix
 * Signal callbacks always have the `on_<object_name>` prefix
 * Callback names must have the name of the signal in the past

For example:

```c
static void
on_foo_size_allocated_cb (GtkWidget *widget,
                          GtkAllocation *allocation,
                          gpointer user_data)
{
    /* ... */
}
```

### Line splitting

`clang-format` handles most wrapping at the 120-column limit. If a call still
looks unreadable after formatting, small manual adjustments are acceptable, but
avoid fighting the tool without a good reason.

For example, this feels extremely unbalanced:

```c
foo_bar_do_something_sync (a,
                           1,
                           object,
                           data,
                           something
                           cancellable,
                           &error);
```

Re-running `clang-format` on the file will usually produce a clearer break. If
something is still off, prefer a short manual tweak that stays under the column
limit and keeps operands aligned.

# Contributing guidelines

See the [CONTRIBUTING.md](CONTRIBUTING.md) file for the contribution guidelines,
and the Code of Conduct that contributors are expected to follow.
