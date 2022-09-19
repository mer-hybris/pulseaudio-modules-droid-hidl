%define pulseversion %{expand:%(rpm -q --qf '[%%{version}]' pulseaudio)}
%define pulsemajorminor %{expand:%(echo '%{pulseversion}' | cut -d+ -f1)}
%define moduleversion %{pulsemajorminor}.%{expand:%(echo '%{version}' | cut -d. -f3)}

Name:       pulseaudio-modules-droid-hidl

Summary:    PulseAudio Droid HIDL module
Version:    1.5.0
Release:    1
License:    LGPLv2+
URL:        https://github.com/mer-hybris/pulseaudio-modules-droid-hidl
Source0:    %{name}-%{version}.tar.bz2
Requires:   pulseaudio >= %{pulseversion}
Requires:   audiosystem-passthrough >= 1.0.0
Requires:   pulseaudio-modules-droid
BuildRequires:  libtool-ltdl-devel
BuildRequires:  meson
BuildRequires:  pkgconfig(pulsecore) >= %{pulsemajorminor}
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(android-headers)
BuildRequires:  pkgconfig(audiosystem-passthrough)

%description
PulseAudio Droid HIDL module.

%prep
%autosetup -n %{name}-%{version}

%build
echo "%{version}" > .tarball-version
%meson
%meson_build

%install
%meson_install

%files
%defattr(-,root,root,-)
%license COPYING
%{_libdir}/pulse-%{pulsemajorminor}/modules/module-droid-hidl.so
