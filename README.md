[![Build Status](https://gitlab.gnome.org/GNOME/gnome-control-center/badges/master/pipeline.svg)](https://gitlab.gnome.org/GNOME/gnome-control-center/pipelines)
[![Coverage report](https://gitlab.gnome.org/GNOME/gnome-control-center/badges/master/coverage.svg)](https://gnome.pages.gitlab.gnome.org/gnome-control-center/)
[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.gnome.org/GNOME/gnome-control-center/blob/master/COPYING)

GNOME Settings
====================

GNOME Settings is GNOME's main interface for configuration of various aspects of your desktop.

## Contributing

See `docs/CONTRIBUTING.md` for details on the contribution process, and `docs/HACKING.md`
for the coding style guidelines.

## Reporting Bugs

Before reporting any bugs or opening feature requests, [read the communication guidelines][communication-guidelines].

Bugs should be reported to the GNOME bug tracking system under the product
gnome-control-center. It is available at [GitLab Issues](https://gitlab.gnome.org/GNOME/gnome-control-center/issues).

In the report please include the following information:

 * Operating system and version
 * For Linux, version of the C library
 * Exact error message
 * Steps to reproduce the bug
 * If the bug is a visual defect, attach a screenshot
 * If the bug is a crash, attach a backtrace if possible [see below]

### How to get a backtrace

If the crash is reproducible, follow the steps to obtain a 
backtrace:

Install debug symbols for gnome-control-center.

Run the program in gdb [the GNU debugger] or any other debugger.

    gdb gnome-control-center

Start the program.
    
    (gdb) run

Reproduce the crash and when the program exits to (gdb) prompt, get the backtrace.

    (gdb) bt full

Once you have the backtrace, copy and paste it into the 'Comments' field or attach it as
a file to the bug report.

## Testing Unstable Settings

It is quite easy to test and give feedback about the development version of GNOME
Settings. Just access https://gitlab.gnome.org/GNOME/gnome-control-center/environments,
get the latest version, download it, double-click the file, install and run.

Note that GNOME Settings Flatpak will only work if you are running
the latest GNOME version in your host system.


[communication-guidelines]: https://gitlab.gnome.org/GNOME/gnome-control-center/blob/master/docs/CONTRIBUTING.md#communication-guidelines
