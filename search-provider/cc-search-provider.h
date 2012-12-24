/*
 * Copyright (c) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _CC_SEARCH_PROVIDER_H
#define _CC_SEARCH_PROVIDER_H

#include <glib-object.h>
#include <gio/gio.h>
#include "cc-shell-search-provider-generated.h"
#include <shell/cc-shell-model.h>

G_BEGIN_DECLS

#define CC_TYPE_SEARCH_PROVIDER cc_search_provider_get_type()

#define CC_SEARCH_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  CC_TYPE_SEARCH_PROVIDER, CcSearchProvider))

#define CC_SEARCH_PROVIDER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  CC_TYPE_SEARCH_PROVIDER, CcSearchProviderClass))

#define CC_IS_SEARCH_PROVIDER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  CC_TYPE_SEARCH_PROVIDER))

#define CC_IS_SEARCH_PROVIDER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  CC_TYPE_SEARCH_PROVIDER))

#define CC_SEARCH_PROVIDER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  CC_TYPE_SEARCH_PROVIDER, CcSearchProviderClass))

typedef struct _CcSearchProvider CcSearchProvider;
typedef struct _CcSearchProviderClass CcSearchProviderClass;

GType cc_search_provider_get_type (void) G_GNUC_CONST;

CcSearchProvider *cc_search_provider_new (void);

gboolean cc_search_provider_dbus_register   (CcSearchProvider  *provider,
                                             GDBusConnection   *connection,
                                             const char        *object_path,
                                             GError           **error);
void     cc_search_provider_dbus_unregister (CcSearchProvider  *provider,
                                             GDBusConnection   *connection,
                                             const char        *object_path);

G_END_DECLS

#endif /* _CC_SEARCH_PROVIDER_H */
