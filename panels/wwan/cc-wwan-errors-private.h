/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-errors-private.h
 *
 * Copyright 2019 Purism SPC
 *
 * Modified from mm-error-helpers.c from ModemManager
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib/gi18n.h>
#include <glib-object.h>
#include <libmm-glib.h>

typedef struct {
  guint code;
  const gchar *message;
} ErrorTable;


static ErrorTable me_errors[] = {
  { MM_MOBILE_EQUIPMENT_ERROR_PHONE_FAILURE,                      N_("Phone failure") },
  { MM_MOBILE_EQUIPMENT_ERROR_NO_CONNECTION,                      N_("No connection to phone") },
  { MM_MOBILE_EQUIPMENT_ERROR_LINK_RESERVED,                      N_("Phone-adaptor link reserved") },
  { MM_MOBILE_EQUIPMENT_ERROR_NOT_ALLOWED,                        N_("Operation not allowed") },
  { MM_MOBILE_EQUIPMENT_ERROR_NOT_SUPPORTED,                      N_("Operation not supported") },
  { MM_MOBILE_EQUIPMENT_ERROR_PH_SIM_PIN,                         N_("PH-SIM PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_PH_FSIM_PIN,                        N_("PH-FSIM PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_PH_FSIM_PUK,                        N_("PH-FSIM PUK required") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_NOT_INSERTED,                   N_("SIM not inserted") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN,                            N_("SIM PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK,                            N_("SIM PUK required") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_FAILURE,                        N_("SIM failure") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_BUSY,                           N_("SIM busy") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_WRONG,                          N_("SIM wrong") },
  { MM_MOBILE_EQUIPMENT_ERROR_INCORRECT_PASSWORD,                 N_("Incorrect password") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_PIN2,                           N_("SIM PIN2 required") },
  { MM_MOBILE_EQUIPMENT_ERROR_SIM_PUK2,                           N_("SIM PUK2 required") },
  { MM_MOBILE_EQUIPMENT_ERROR_MEMORY_FULL,                        N_("Memory full") },
  { MM_MOBILE_EQUIPMENT_ERROR_INVALID_INDEX,                      N_("Invalid index") },
  { MM_MOBILE_EQUIPMENT_ERROR_NOT_FOUND,                          N_("Not found") },
  { MM_MOBILE_EQUIPMENT_ERROR_MEMORY_FAILURE,                     N_("Memory failure") },
  { MM_MOBILE_EQUIPMENT_ERROR_NO_NETWORK,                         N_("No network service") },
  { MM_MOBILE_EQUIPMENT_ERROR_NETWORK_TIMEOUT,                    N_("Network timeout") },
  { MM_MOBILE_EQUIPMENT_ERROR_NETWORK_NOT_ALLOWED,                N_("Network not allowed - emergency calls only") },
  { MM_MOBILE_EQUIPMENT_ERROR_NETWORK_PIN,                        N_("Network personalization PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_NETWORK_PUK,                        N_("Network personalization PUK required") },
  { MM_MOBILE_EQUIPMENT_ERROR_NETWORK_SUBSET_PIN,                 N_("Network subset personalization PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_NETWORK_SUBSET_PUK,                 N_("Network subset personalization PUK required") },
  { MM_MOBILE_EQUIPMENT_ERROR_SERVICE_PIN,                        N_("Service provider personalization PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_SERVICE_PUK,                        N_("Service provider personalization PUK required") },
  { MM_MOBILE_EQUIPMENT_ERROR_CORP_PIN,                           N_("Corporate personalization PIN required") },
  { MM_MOBILE_EQUIPMENT_ERROR_CORP_PUK,                           N_("Corporate personalization PUK required") },
  { MM_MOBILE_EQUIPMENT_ERROR_UNKNOWN,                            N_("Unknown error") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_ILLEGAL_MS,                    N_("Illegal MS") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_ILLEGAL_ME,                    N_("Illegal ME") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_SERVICE_NOT_ALLOWED,           N_("GPRS services not allowed") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_PLMN_NOT_ALLOWED,              N_("PLMN not allowed") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_LOCATION_NOT_ALLOWED,          N_("Location area not allowed") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_ROAMING_NOT_ALLOWED,           N_("Roaming not allowed in this location area") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_SERVICE_OPTION_NOT_SUPPORTED,  N_("Service option not supported") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_SERVICE_OPTION_NOT_SUBSCRIBED, N_("Requested service option not subscribed") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_SERVICE_OPTION_OUT_OF_ORDER,   N_("Service option temporarily out of order") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_UNKNOWN,                       N_("Unspecified GPRS error") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_PDP_AUTH_FAILURE,              N_("PDP authentication failure") },
  { MM_MOBILE_EQUIPMENT_ERROR_GPRS_INVALID_MOBILE_CLASS,          N_("Invalid mobile class") },
};

static inline const gchar *
cc_wwan_error_get_message (GError *error)
{
 if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return _("Action Cancelled");

 if (g_error_matches (error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED))
   return _("Access denied");

  if (error->domain != MM_MOBILE_EQUIPMENT_ERROR)
    return error->message;

  for (guint i = 0; i < G_N_ELEMENTS (me_errors); i++)
    if (me_errors[i].code == error->code)
      return _(me_errors[i].message);

  return _("Unknown Error");
}
