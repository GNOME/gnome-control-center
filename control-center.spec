# Note that this is NOT a relocatable package
%define ver      0.99.0
%define rel      1
%define prefix   /usr

Summary: GNOME control center
Name: control-center
Version: %ver
Release: %rel
Copyright: LGPL
Group: X11/Libraries
Source: ftp://ftp.gnome.org/pub/control-center-%{ver}.tar.gz
BuildRoot: /var/tmp/control-center-root
Obsoletes: gnome
Packager: Jonathan Blandford <jrb@redhat.com>
URL: http://www.gnome.org
Docdir: %{prefix}/doc
Requires: xscreensaver >= 2.34
Requires: gnome-core >= 0.99
Requires: ORBit >= 0.3.0

%description
A Configuration tool for easily setting up your GNOME environment.

GNOME is the GNU Network Object Model Environment.  That's a fancy
name but really GNOME is a nice GUI desktop environment.  It makes
using your computer easy, powerful, and easy to configure.

%package devel
Summary: GNOME control-center includes
Group: X11/Libraries
Requires: control-center

%description devel
Capplet development stuff

%changelog

* Wed Dec 16 1998 Jonathan Blandford <jrb@redhat.com>

- Created for the new control-center branch


%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%prefix

%install
rm -rf $RPM_BUILD_ROOT

make prefix=$RPM_BUILD_ROOT%{prefix} install

%clean
#rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-, root, root)

%doc AUTHORS COPYING ChangeLog NEWS README
%{prefix}/bin/*
%{prefix}/lib/lib*.so.*
%{prefix}/share/control-center/*
%{prefix}/share/locale/*/*/*

%files devel
%defattr(-, root, root)

%{prefix}/lib/lib*.so
%{prefix}/lib/*a
%{prefix}/share/idl
%{prefix}/include/*
