# CcPanel

[CcPanel](https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/main/shell/cc-panel.c?ref_type=heads) is the base class for each settings panel implementation.

`CcPanel` provides convenient navigation features for static and runtime subpages.

Static subpages are top-level setting pages such as "Screen Lock", "Location", "Thunderbolt" (in the Privacy settings) or "Date & Time", "Region & Language" (in the System settings).

Runtime subpages are pages that aren't likely part of the main execution hot path. These are pages usually hidden behind a non-toplevel row or an option.

The main conveniences of `CcPanel` are:

## Child packing

### Panel Subpages

`AdwNavigationPage` child pages of `CcPanel` defined with `<child type="subpage">`, will be added as pages to `CcPanel.navigation`. For example:

```xml
  <template class="CcPrivacyPanel" parent="CcPanel">
    <child type="subpage">
      <object class="AdwNavigationPage">
```

This will result in:

* ### CcPanel (AdwNavigationPage)
  - #### CcPanel.navigation (AdwNavigationView)
    - ##### CcPanel.navigation.children +=
      -  ###### **child (AdwNavigationPage)**

### Other widgets

Widgets of other types will overwrite `CcPanel.child` and avoid the navigation mechanism entirely. **This is the case for all panels that don't have subpages**.

## API

## `CcPanel.add_static_subpage`

Allows for panels to create subpages on demand, rather than create at panel initialization.

This is useful for hub panels (such as System and Privacy), where we don't want to load all subpages at startup, given that users might just visit one.

## `CcPanel.push_subpage`

Allows for panel implementations to simply add an `AdwNavigationPage` to the top of the `CcPanel` navigation.

## `CcPanel.pop_visible_subpage`

Allows for panels to pop the current visible subpage from the navigation view.

## `CcPanel.get_visible_subpage`

Allows for panels to query for the visible subpage. This is useful when the panel implementation has custom behaviour depending on the page that is shown. For example, in the Privacy settings we check the current visible subpage so that we can determine what's the best documentation page to offer.
