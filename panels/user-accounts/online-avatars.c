/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright 2013 Red Hat, Inc,
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by:
 *   Jasper St. Pierre <jstpierre@mecheye.net>
 *   Bogdan Ciobanu <bgdn.ciobanu@gmail.com>
 */

#include "online-avatars.h"

#include <string.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>

#define FLICKR_API_KEY "9a942002593e262349dd084439b69640"
#define FLICKR_API_SECRET "967724da5ff77246"

#define IMAGE_SIZE 96

static void
got_image_cb (SoupSession *session,
              SoupMessage *msg,
              gpointer     user_data)
{
    GBytes *bytes = NULL;
    GTask *task = user_data;

    if (msg->status_code == 200) {
        SoupBuffer *buffer = soup_message_body_flatten (msg->response_body);
        bytes = soup_buffer_get_as_bytes (buffer);
        g_task_return_pointer (task, bytes, (GDestroyNotify) g_bytes_unref);
    } else {
        /* XXX -- return a real error? */
        g_task_return_pointer (task, NULL, NULL);
    }

    soup_session_abort (session);
    g_object_unref (session);
    g_object_unref (task);
}

static void
got_facebook_cb (SoupSession *session,
                 SoupMessage *msg,
                 gpointer     user_data)
{
    GTask *task = user_data;
    SoupMessage *newmsg;
    SoupBuffer *buffer;
    JsonParser *parser;
    JsonNode *root;
    JsonReader *reader;
    gboolean is_silhouette;
    GError *error = NULL;
    const char *url;

    buffer = soup_message_body_flatten (msg->response_body);
    parser = json_parser_new ();

    if (!json_parser_load_from_data (parser, buffer->data, buffer->length, &error)) {
        g_print ("Error loading json: %s\n", error->message);
        goto out;
    }

    root = json_parser_get_root (parser);
    reader = json_reader_new (root);

    json_reader_read_member (reader, "data");

    json_reader_read_member (reader, "is_silhouette");
    is_silhouette = json_reader_get_boolean_value (reader);
    /* The user doesn't have an avatar set on Facebook; use the image default */
    if (is_silhouette) {
        g_task_return_pointer (task, NULL, NULL);
        goto out;
    }
    json_reader_end_member (reader); /* is_silhouette */

    json_reader_read_member (reader, "url");
    url = json_reader_get_string_value (reader);
    json_reader_end_member (reader); /* url */

    json_reader_end_member (reader); /* data */

    newmsg = soup_message_new ("GET", url);
    soup_session_queue_message (session, newmsg, got_image_cb, task);

 out:
    soup_buffer_free (buffer);
    g_clear_object (&reader);
    g_clear_object (&parser);
}

static void
get_facebook_image (GoaAccount *account,
                    GTask      *task)
{
    SoupSession *session;
    SoupMessage *msg;

    gchar *url = g_strdup_printf ("http://graph.facebook.com/%s/picture?redirect=false&width=%d&height=%d",
                                  goa_account_get_identity (account), IMAGE_SIZE, IMAGE_SIZE);

    session = soup_session_new ();
    msg = soup_message_new ("GET", url);
    soup_message_headers_append (msg->request_headers, "Content-Type", "application/json");

    soup_session_queue_message (session, msg, got_facebook_cb, task);

    g_free (url);
}

static void
got_google_user_info_cb (SoupSession *session,
                         SoupMessage *msg,
                         gpointer     user_data)
{
    GTask *task = user_data;
    SoupMessage *newmsg;
    SoupBuffer *buffer;
    JsonParser *parser;
    JsonNode *root;
    JsonReader *reader;
    GError *error = NULL;
    const char *user_id;
    char *url;

    buffer = soup_message_body_flatten (msg->response_body);
    parser = json_parser_new ();

    if (!json_parser_load_from_data (parser, buffer->data, buffer->length, &error)) {
        g_print ("Error loading JSON response: %s\n", error->message);
        goto out;
    }

    root = json_parser_get_root (parser);
    reader = json_reader_new (root);

    json_reader_read_member (reader, "id");
    user_id = json_reader_get_string_value (reader);
    json_reader_end_member (reader); /* id */

    g_object_unref (reader);

    url = g_strdup_printf ("https://profiles.google.com/s2/photos/profile/%s", user_id);

    newmsg = soup_message_new ("GET", url);
    soup_session_queue_message (session, newmsg, got_image_cb, task);

 out:
    soup_buffer_free (buffer);
    g_object_unref (parser);
}

static void
got_google_access_token_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    GoaOAuth2Based *oauth2 = (GoaOAuth2Based*) source;
    SoupSession *session;
    SoupMessage *msg;
    GError *error = NULL;
    gint expires;
    gchar *access_token;
    gchar *url;

    if (!goa_oauth2_based_call_get_access_token_finish (oauth2, &access_token, &expires, res, &error)) {
        g_print("Error in getting the access token: %s\n", error->message);
        return;
    }

    url = g_strdup_printf ("https://www.googleapis.com/oauth2/v1/userinfo?access_token=%s", access_token);

    session = soup_session_new ();
    msg = soup_message_new ("GET", url);
    soup_message_headers_append (msg->request_headers, "Content-Type", "application/json");

    soup_session_queue_message (session, msg, got_google_user_info_cb, user_data);

    g_object_unref (oauth2);
    g_free (url);
}

static void
ensure_google_credentials_ready_cb (GObject      *source,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
    GoaAccount *account = GOA_ACCOUNT (source);
    GTask *task = user_data;
    GoaObject *goa_object;
    GoaOAuth2Based *oauth;
    GError *error = NULL;
    gint expires;

    if (!goa_account_call_ensure_credentials_finish (account, &expires, res, &error)) {
        g_warning ("Failed to get credentials for Google: %s\n", error->message);
        return;
    }

    goa_object = GOA_OBJECT (g_dbus_interface_get_object (G_DBUS_INTERFACE (account)));
    oauth = goa_object_get_oauth2_based (goa_object);
    goa_oauth2_based_call_get_access_token (oauth, g_task_get_cancellable (task),
                                            got_google_access_token_cb, user_data);
}

static void
get_google_image (GoaAccount   *account,
                  GTask        *task)
{
    goa_account_call_ensure_credentials (account, g_task_get_cancellable (task),
                                         ensure_google_credentials_ready_cb, task);
}

static void
got_flickr_details_cb (SoupSession *session,
                       SoupMessage *msg,
                       gpointer     user_data)
{
    GTask *task = user_data;
    GoaAccount *account;
    SoupMessage *newmsg;
    SoupBuffer *buffer;
    JsonParser *parser;
    JsonNode *root;
    JsonReader *reader;
    GError *error = NULL;
    int icon_farm;
    char *json = NULL;
    char *url = NULL;
    const char *icon_server;

    buffer = soup_message_body_flatten (msg->response_body);

    /* Eliminates jsonFlickrApi() from the response so it can be parsed correctly */
    json = g_strndup (buffer->data + 14, buffer->length - 15);

    parser = json_parser_new ();
    json_parser_load_from_data (parser, json, -1, &error);
    if (error) {
        g_print ("Error parsing json: %s\n", error->message);
        goto out;
    }

    root = json_parser_get_root (parser);
    reader = json_reader_new (root);
    json_reader_read_member (reader, "person");

    json_reader_read_member (reader, "iconserver");
    icon_server = json_reader_get_string_value (reader);
    json_reader_end_member (reader); /* iconserver */

    json_reader_read_member (reader, "iconfarm");
    icon_farm = json_reader_get_int_value (reader);
    json_reader_end_member (reader); /* iconfarm */

    json_reader_end_member (reader); /* person */

    account = GOA_ACCOUNT (g_task_get_source_object (task));

    url = g_strdup_printf ("http://farm%d.staticflickr.com/%s/buddyicons/%s.jpg",
                           icon_farm, icon_server, goa_account_get_identity (account));
    newmsg = soup_message_new ("GET", url);
    soup_session_queue_message (session, newmsg, got_image_cb, user_data);

 out:
    soup_buffer_free (buffer);
    g_clear_object (&reader);
    g_clear_object (&parser);
    g_free (json);
    g_free (url);
}

static void
get_flickr_image (GoaAccount *account,
                  GTask      *task)
{
    SoupSession *session;
    SoupMessage *msg;
    gchar *url;

    url = g_strdup_printf ("https://api.flickr.com/services/rest/?method=flickr.people.getInfo&user_id=%s&api_key=%s&format=json",
                           goa_account_get_identity (account), FLICKR_API_KEY);

    session = soup_session_new ();
    msg = soup_message_new ("GET", url);
    g_free (url);

    soup_session_queue_message (session, msg, got_flickr_details_cb, task);
}

void
get_avatar_from_online_account (GoaAccount          *account,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
    const char *provider_type = goa_account_get_provider_type (account);
    GTask *task = g_task_new (account, cancellable, callback, user_data);

    if (strcmp (provider_type, "facebook") == 0)
        get_facebook_image (account, task);
    else if (strcmp (provider_type, "google") == 0)
        get_google_image (account, task);
    else if (strcmp (provider_type, "flickr") == 0)
        get_flickr_image (account, task);
    else
        g_task_return_pointer (task, NULL, NULL);
}

GBytes *
get_avatar_from_online_account_finish (GoaAccount    *account,
                                       GAsyncResult  *result,
                                       GError       **error)
{
    g_return_val_if_fail (g_task_is_valid (result, account), NULL);
    return g_task_propagate_pointer (G_TASK (result), error);
}

void
get_gravatar_from_email (const char          *email,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
    SoupSession *session;
    SoupMessage *msg;
    gchar *copy, *hash, *url;
    GTask *task = g_task_new (NULL, cancellable, callback, user_data);

    copy = g_strdup (email);
    copy = g_strstrip (copy);

    hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, copy, -1);

    url = g_strdup_printf ("https://secure.gravatar.com/avatar/%s.png?s=%d&d=404",
                           hash, IMAGE_SIZE);

    session = soup_session_new ();
    msg = soup_message_new ("GET", url);
    soup_session_queue_message (session, msg, got_image_cb, task);

    g_free (url);
    g_free (hash);
    g_free (copy);
}

GBytes *
get_gravatar_from_email_finish (GAsyncResult  *result,
                                GError       **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}
