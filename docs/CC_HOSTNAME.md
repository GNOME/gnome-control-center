# CcHostname

[CcHostname](https://gitlab.gnome.org/GNOME/gnome-control-center/-/blob/main/panels/common/cc-hostname.h?ref_type=heads) is a singleton class useful for obtaining information about the host system throughout systemd-hostnamed.

`CcHostname` contains a proxy to the `org.freedesktop.hostname1` interface over DBus.

The main `CcHostname` methods are:

# `CcHostname.get_display_hostname`

Obtains the system "Pretty Hostname". See https://www.freedesktop.org/software/systemd/man/latest/hostnamectl.html#hostname%20%5BNAME%5D

# `CcHostname.get_display_hostname`

Obtains the system "Static Hostname".

# `CcHostname.set_hostname`

Allows for setting the system "pretty" and "static" hostnames (SetPrettyHostname and SetStaticHostname).

# `CcHostname.get_chassis_type`

Useful for when we want to condition the behaviour based on whether the device running Settings is a laptop, desktop, server, tablet, handset, etc.... See https://www.freedesktop.org/software/systemd/man/latest/hostnamectl.html#chassis%20%5BTYPE%5D

# `CcHostname.is_vm_chassis`

A convenient function that calls `CcHostname.get_chassis_type` internally to determine whether Settings is running in a virtual machine environment.

# `CcHostname.get_property`

Allows for queying other systemd-hostnamed properties. See `hostnamectl status` to list the available options.
