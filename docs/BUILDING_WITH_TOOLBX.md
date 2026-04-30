# Building GNOME Settings in a Toolbx

[Toolbx](https://containertoolbx.org/) provides a Fedora-based development container that is ideal for developing GNOME Settings without risking messing up with your host operating system.

## Create a Toolbx with:

```bash
toolbox create control-center-toolbox -i registry.gitlab.gnome.org/gnome/gnome-control-center/main:2026-04-17.1-fedora-rawhide
```

After creation, you can enter your toolbx with

```bash
toolbox enter control-center-toolbox
```

## Perform a local build

Inside the `gnome-control-center` repository you can prepare the build with:

```bash
meson setup _build
```

If all dependencies were satisfied, you should be able to build the project from inside the `_build` folder.

```bash
cd _build
ninja
```

At the first time you do this you should also run `sudo ninja install` so that gsettings and other resources are installed in system paths.

## Running

With that, you can run GNOME Settings from its executable:

```bash
./shell/gnome-control-center
```

## Additional packages for specific components

GNOME Settings should now open, but many components require additional packages inside the Toolbx to function properly.

- `rygel` and `gnome-user-share` for the Sharing Panel
- `gnome-shell` for Multitasking and the videos in Mouse
- `flatpak` for showing apps in Applications that are installed outside your Toolbx
- `gnome-backgrounds` and `fedora-workstation-backgrounds` to show some backgrounds in Appearance
- `NetworkManager-{vpnc,openvpn,openconnect,pptp,ssh}-gnome` for VPN functionality
- `yelp` and `gnome-user-docs` for showing help information (press F1)

To install all of these at once, you can use:
```bash
sudo dnf install -y rygel gnome-user-share gnome-shell flatpak \
                    gnome-backgrounds fedora-workstation-backgrounds \
                    NetworkManager-{vpnc,openvpn,openconnect,pptp,ssh}-gnome \
                    yelp gnome-user-docs
```
