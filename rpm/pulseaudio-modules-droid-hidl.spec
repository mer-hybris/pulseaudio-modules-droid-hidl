%define pulseversion %{expand:%(rpm -q --qf '[%%{version}]' pulseaudio)}
%define pulsemajorminor %{expand:%(echo '%{pulseversion}' | cut -d+ -f1)}
%define moduleversion %{pulsemajorminor}.%{expand:%(echo '%{version}' | cut -d. -f3)}

Name:       pulseaudio-modules-droid-hidl

Summary:    PulseAudio Droid HIDL module
Version:    1.2.0
Release:    1
Group:      Multimedia/PulseAudio
License:    LGPLv2.1+
URL:        https://github.com/mer-hybris/pulseaudio-modules-droid-hidl
Source0:    %{name}-%{version}.tar.bz2
Requires:   pulseaudio >= %{pulseversion}
BuildRequires:  automake
BuildRequires:  libtool
BuildRequires:  libtool-ltdl-devel
BuildRequires:  pkgconfig(pulsecore) >= %{pulsemajorminor}
BuildRequires:  pkgconfig(libdroid-util) >= %{pulsemajorminor}.41
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(android-headers)
BuildRequires:  pkgconfig(libgbinder) >= 1.0.32
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(gio-2.0)

%description
PulseAudio Droid HIDL module.


%prep
%setup -q -n %{name}-%{version}

%build
echo "%{version}" > .tarball-version
%reconfigure --disable-static --with-module-dir="%{_libdir}/pulse-%{pulsemajorminor}/modules"
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

%files
%defattr(-,root,root,-)
%{_libdir}/pulse-%{pulsemajorminor}/modules/module-droid-hidl.so
%{_libexecdir}/pulse/hidl-helper
