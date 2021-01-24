#ifndef CC_SUBSCRIPTION_COMMON_H
#define CC_SUBSCRIPTION_COMMON_H

typedef enum {
  GSD_SUBMAN_SUBSCRIPTION_STATUS_UNKNOWN,
  GSD_SUBMAN_SUBSCRIPTION_STATUS_VALID,
  GSD_SUBMAN_SUBSCRIPTION_STATUS_INVALID,
  GSD_SUBMAN_SUBSCRIPTION_STATUS_DISABLED,
  GSD_SUBMAN_SUBSCRIPTION_STATUS_PARTIALLY_VALID,
  GSD_SUBMAN_SUBSCRIPTION_STATUS_NO_INSTALLED_PRODUCTS,
  GSD_SUBMAN_SUBSCRIPTION_STATUS_LAST
} GsdSubmanSubscriptionStatus;

static inline gboolean
get_subscription_status (GDBusProxy *subscription_proxy,
                         GsdSubmanSubscriptionStatus *status)
{
  g_autoptr(GVariant) status_variant = NULL;
  guint32 u;

  status_variant = g_dbus_proxy_get_cached_property (subscription_proxy, "SubscriptionStatus");
  if (!status_variant)
    {
      g_debug ("Unable to get SubscriptionStatus property");
      return FALSE;
    }

  g_variant_get (status_variant, "u", &u);
  *status = u;

  return TRUE;
}

#endif
