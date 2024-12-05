# Testing Wacom panel without a tablet or pen

## Setup

The `hid-replay` utility that can replay device events, simulating pen and touch behavior. This utility provides pre-built binaries ready for use on its [releases](https://github.com/hidutils/hid-replay/releases/) page, you can grab the `hid-replay.zip` file and run it with:
```
unzip hid-replay.zip
chmod +x hid-replay
sudo ./hid-replay <path-to-recording>
```

The tool can also be obtained from its source code with:

```bash
git clone https://github.com/hidutils/hid-replay/
```

To compile it use the `cargo` command after installing the required dependencies:

```bash
dnf install llvm-devel clang-devel  # on Fedora
apt install llvm-dev libclang-dev   # on Debian/Ubuntu

cargo build
sudo CARGO_INSTALL_ROOT=/usr/local cargo install hid-replay
```
The last command will install `hid-replay` as `/usr/local/bin/hid-replay`.

Peter Hutterer kindly maintains a collection of Wacom device event recordings in a git repository that can be downloaded with:

```bash
git clone https://github.com/whot/wacom-recordings
```

## Playing device recordings

After setting up the environment, you can run the **hid-replay** tool (inside the hid-tools folder):

```bash
sudo hid-replay <path-to-recording>
```

(where \<path-to-recording\> is one of the `.hid` files in the **wacom-recordings** collection.

## Testing GNOME Settings Wacom panel

While the hid-replay tool runs in the background, you can open GNOME Settings -> Wacom and you will see the settings for the correspondent device of the recording.

For example:

```bash
sudo hid-replay wacom-recordings/Wacom\ Intuos\ Pro\ M/pen.pen-light-horizontal.hid
```

![image_2023-11-01_11-23-33](uploads/6ac48ebd50b20ee5437d9b587d6621f7/image_2023-11-01_11-23-33.png)

## Limitations

This setup currently doesn't work in a container (such as Toolbx) as it has insufficient permissions.

This was tested in Fedora Workstation 39.
