# Building GNOME Settings as a Flatpak in GNOME Builder

An alternative way of building GNOME Settings is using [GNOME Builder](https://flathub.org/apps/org.gnome.Builder)'s Flatpak support. This allows for easily cloning the repository and automatically handling the build of dependencies.

> The Flatpak container sandbox prevents GNOME Settings from performing a few of its functions, so this method is only recommended for superficial changes such as updating strings, UI, and style. If you face issues with the Settings functionality while running it in Builder, [considering building GNOME settings in a Toolbx instead](https://gitlab.gnome.org/GNOME/gnome-control-center/-/wikis/build/Building-GNOME-Settings-in-a-Toolbx).

Follow the steps described at https://welcome.gnome.org/en-GB/app/Settings/#getting-the-app-to-build

## Known limitations in the Settings Flatpak

- Apps Settings
  - The *Default Apps* settings won't get populated (your apps aren't visible inside the Flatpak sandbox)
  - App list won't show apps that aren't part of the sandbox (only utilities that are part of the flatpak runtime/sdk)
- Notification Settings
  - installed apps aren't visible inside the flatpak sandbox
- Search Settings
  - The [shell search providers](https://developer.gnome.org/documentation/tutorials/search-provider.html) aren't visible from within the Flatpak sandbox
- Keyboard Settings
  - Input sources list appears empty
- Printers Settings
  - There's no way to unlock the panel (it is not possible to use polkit within the flatpak container)
- System > Users Settings
  - There's no way to Unlock the panel (it is not possible to use polkit within the flatpak container)
- System -> Remote Desktop Settings
  - The panel won't load because it requires the remote-session-helper to be installed in a prefix and with permissions that aren't available within the flatpak container
- System -> About Settings
  - There's no way to change the "Device Name" because that requires escalating privileges with polkit (not possible within the flatpak container) 
  - The OS logo will fallback to the GNOME one (the distro provided one is not accessible from within the flatpak container)
