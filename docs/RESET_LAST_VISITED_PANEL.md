# Workaround: A crashing panel preventing Settings from starting up

GNOME Settings saves the last opened panel when it is closed and attempts to start on that same panel when starting up again. If the panel has issues, you won't be able to start GNOME Settings.

To workaround the issue you can reset the last-opened setting with:

```gsettings reset org.gnome.Settings last-panel```

Please, try to [obtain a stack trace](https://wiki.gnome.org/GettingInTouch/Bugzilla/GettingTraces/Details) of the crashing panel and [report an issue](https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/new) attaching it.
