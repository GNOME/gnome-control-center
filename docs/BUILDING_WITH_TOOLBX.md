# Building GNOME Settings in a Toolbx

[Toolbx](https://containertoolbx.org/) provides a Fedora-based development container that is ideal for developing GNOME Settings without risking messing up with your host operating system.

## Create a Toolbx with:

```bash
 toolbox create --release <release-number>
```
(preferably choose the latest Fedora release)

After creation, you can enter your toolbx with

```bash
toolbox enter --release <release-number>
```

## Install development packages for GNOME Settings

To install the development dependencies in GNOME Settings you can use:

```bash
sudo dnf install -y gnome-desktop4-devel libgweather4-devel \
                    gnome-settings-daemon-devel libnma-gtk4-devel \
                    colord-gtk4-devel
sudo dnf builddep gnome-control-center
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
## Tips

* If `meson _build` is still missing dependencies, you can check which dependencies we are using in our Fedora CI build for reference to what to install. See `FDO_DISTRIBUTION_PACKAGES` for the whole list of packages in https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/main/.gitlab-ci.yml#L88


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
