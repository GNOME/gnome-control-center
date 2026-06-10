# CcPanel

[CcPanel](https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/main/shell/cc-panel.c?ref_type=heads) is the base class for each settings panel implementation.

`CcPanel` is an AdwNavigationPage and it can contain other CcPanels as well as regular AdwNavigationPage pages.

## Static Subpages

Static subpages are pages such as "Screen Lock", "Location", "Thunderbolt" (in the Privacy settings) or "Date & Time", "Region & Language" (in the System settings). These pages are created on demand, to avoid expensive loading operations at startup.

`CcPanel.static_subpages` is a hash table where the key is the page `tag` and the value is the subpage GType (`CC_TYPE_USERS_PAGE`, for example).

When a `navigation.push` action is triggered, CcWindow's `navigation_push_cb` calls `cc_panel_get_static_subpage` on the current panel. This method will construct the subpage so that it can be added to the `CcWindow.navigation`.

## Child Packing

`AdwNavigationPage` child pages of `CcPanel` defined with `[subpage]` will be added as pages to `CcWindow.navigation`. For example:

```xml
template $CcPrivacyPanel: $CcPanel {
  [subpage]
  $CcScreenPage {}
}
```
