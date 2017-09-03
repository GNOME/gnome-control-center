/*
 * Copyright (C) 2014 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <gio/gio.h>
#include <grilo.h>

#define GOA_API_IS_SUBJECT_TO_CHANGE
#include <goa/goa.h>

#include "bg-pictures-source.h"
#include "cc-background-grilo-miner.h"

struct _CcBackgroundGriloMiner
{
  GObject parent_instance;

  GCancellable *cancellable;
  GList *accounts;
};

G_DEFINE_TYPE (CcBackgroundGriloMiner, cc_background_grilo_miner, G_TYPE_OBJECT)

enum
{
  MEDIA_FOUND,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

#define REMOTE_ITEM_COUNT 50

static gchar *
get_grilo_id (GoaObject *goa_object)
{
  GoaAccount *account;

  account = goa_object_peek_account (goa_object);
  return g_strdup_printf ("grl-flickr-%s", goa_account_get_id (account));
}

static void
is_online_data_cached (GObject      *object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  CcBackgroundGriloMiner *self;
  GError *error = NULL;
  GFileInfo *info = NULL;
  GFile *cache_file = G_FILE (object);
  GrlMedia *media;
  const gchar *uri;

  info = g_file_query_info_finish (cache_file, res, &error);
  if (info == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        goto out;
    }

  self = CC_BACKGROUND_GRILO_MINER (user_data);

  media = g_object_get_data (G_OBJECT (cache_file), "grl-media");
  uri = grl_media_get_url (media);

  if (info != NULL)
    {
      g_debug ("Ignored URL '%s' as it is already in the cache", uri);
      goto out;
    }

  g_signal_emit (self, signals[MEDIA_FOUND], 0, media);

 out:
  g_clear_object (&info);
  g_clear_error (&error);
}

static void
searched_online_source (GrlSource    *source,
                        guint         operation_id,
                        GrlMedia     *media,
                        guint         remaining,
                        gpointer      user_data,
                        const GError *error)
{
  CcBackgroundGriloMiner *self = CC_BACKGROUND_GRILO_MINER (user_data);
  g_autoptr(GFile) cache_file = NULL;
  const gchar *uri;
  g_autofree gchar *cache_path = NULL;

  if (error != NULL)
    {
      const gchar *source_id;

      source_id = grl_source_get_id (source);
      g_warning ("Error searching %s: %s", source_id, error->message);
      grl_operation_cancel (operation_id);
      remaining = 0;
      goto out;
    }

  uri = grl_media_get_url (media);
  cache_path = bg_pictures_source_get_unique_path (uri);
  cache_file = g_file_new_for_path (cache_path);
  g_object_set_data_full (G_OBJECT (cache_file), "grl-media", media, g_object_unref);
  g_file_query_info_async (cache_file,
                           G_FILE_ATTRIBUTE_STANDARD_TYPE,
                           G_FILE_QUERY_INFO_NONE,
                           G_PRIORITY_DEFAULT,
                           self->cancellable,
                           is_online_data_cached,
                           self);

 out:
  if (remaining == 0)
    g_object_unref (self);
}

static void
query_online_source (CcBackgroundGriloMiner *self, GrlSource *source)
{
  const GList *keys;
  GrlCaps *caps;
  GrlOperationOptions *options;

  keys = grl_source_supported_keys (source);
  caps = grl_source_get_caps (source, GRL_OP_BROWSE);
  options = grl_operation_options_new (caps);
  grl_operation_options_set_count (options, REMOTE_ITEM_COUNT);
  grl_operation_options_set_resolution_flags (options, GRL_RESOLVE_FAST_ONLY);
  grl_operation_options_set_type_filter (options, GRL_TYPE_FILTER_IMAGE);

  grl_source_search (source, NULL, keys, options, searched_online_source, g_object_ref (self));
  g_object_unref (options);
}

static void
add_online_source_cb (CcBackgroundGriloMiner *self,
                      GrlSource              *source)
{
  GList *l;
  gboolean found = FALSE;
  const gchar *source_id;

  source_id = grl_source_get_id (source);
  for (l = self->accounts; l != NULL && !found; l = l->next)
    {
      GoaObject *goa_object = GOA_OBJECT (l->data);
      g_autofree gchar *account_id = NULL;

      account_id = get_grilo_id (goa_object);
      if (g_strcmp0 (source_id, account_id) == 0)
        {
          query_online_source (self, source);
          found = TRUE;
        }
    }
}

static void
client_async_ready (GObject      *source,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  CcBackgroundGriloMiner *self;
  g_autoptr(GError) error = NULL;
  GList *accounts = NULL;
  GList *photo_accounts = NULL;
  GList *l;
  GoaClient *client = NULL;
  GrlRegistry *registry;

  client = goa_client_new_finish (res, &error);
  if (client == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to create GoaClient: %s", error->message);
      goto out;
    }

  self = CC_BACKGROUND_GRILO_MINER (user_data);

  accounts = goa_client_get_accounts (client);
  for (l = accounts; l != NULL; l = l->next)
    {
      GoaObject *goa_object = GOA_OBJECT (l->data);
      GoaAccount *account;
      GoaPhotos *photos;
      const gchar *provider_type;

      account = goa_object_peek_account (goa_object);
      provider_type = goa_account_get_provider_type (account);

      photos = goa_object_peek_photos (goa_object);
      if (photos != NULL && g_strcmp0 (provider_type, "flickr") == 0)
        photo_accounts = g_list_prepend (photo_accounts, g_object_ref (goa_object));
    }

  if (photo_accounts == NULL)
    goto out;

  registry = grl_registry_get_default ();

  for (l = photo_accounts; l != NULL; l = l->next)
    {
      GoaObject *goa_object = GOA_OBJECT (l->data);
      GrlSource *source;
      g_autofree gchar *account_id = NULL;

      account_id = get_grilo_id (goa_object);
      source = grl_registry_lookup_source (registry, account_id);
      if (source != NULL)
        query_online_source (self, source);
    }

  self->accounts = photo_accounts;
  photo_accounts = NULL;

  g_signal_connect_object (registry, "source-added", G_CALLBACK (add_online_source_cb), self, G_CONNECT_SWAPPED);

 out:
  g_list_free_full (photo_accounts, g_object_unref);
  g_list_free_full (accounts, g_object_unref);
  g_clear_object (&client);
}

static void
setup_online_accounts (CcBackgroundGriloMiner *self)
{
  goa_client_new (self->cancellable, client_async_ready, self);
}

static void
cc_background_grilo_miner_dispose (GObject *object)
{
  CcBackgroundGriloMiner *self = CC_BACKGROUND_GRILO_MINER (object);

  if (self->cancellable)
    {
      g_cancellable_cancel (self->cancellable);
      g_clear_object (&self->cancellable);
    }

  if (self->accounts)
    {
      g_list_free_full (self->accounts, g_object_unref);
      self->accounts = NULL;
    }

  G_OBJECT_CLASS (cc_background_grilo_miner_parent_class)->dispose (object);
}

static void
cc_background_grilo_miner_init (CcBackgroundGriloMiner *self)
{
  self->cancellable = g_cancellable_new ();
}

static void
cc_background_grilo_miner_class_init (CcBackgroundGriloMinerClass *klass)
{
  g_autoptr(GError) error = NULL;
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GrlRegistry *registry;

  object_class->dispose = cc_background_grilo_miner_dispose;

  signals[MEDIA_FOUND] = g_signal_new ("media-found",
                                       G_TYPE_FROM_CLASS (klass),
                                       G_SIGNAL_RUN_LAST,
                                       0,    /* class_offset */
                                       NULL, /* accumulator */
                                       NULL, /* accu_data */
                                       g_cclosure_marshal_VOID__OBJECT,
                                       G_TYPE_NONE,
                                       1,
                                       GRL_TYPE_MEDIA);

  grl_init (NULL, NULL);
  registry = grl_registry_get_default ();

  error = NULL;
  if (!grl_registry_load_all_plugins (registry, FALSE, &error) ||
      !grl_registry_activate_plugin_by_id (registry, "grl-flickr", &error))
      g_warning ("%s", error->message);
}

CcBackgroundGriloMiner *
cc_background_grilo_miner_new (void)
{
  return g_object_new (CC_TYPE_BACKGROUND_GRILO_MINER, NULL);
}

void
cc_background_grilo_miner_start (CcBackgroundGriloMiner *self)
{
  setup_online_accounts (self);
}
