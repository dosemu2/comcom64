#
# spec file for package comcom32
#

Name: comcom32
Version: 0.1alpha1
Release: 1%{?dist}
Summary: 32-bit command.com

Group: System/Emulator

License: GPL-3.0+
URL: http://www.github.com/stsp/comcom32
Source0: %{name}-%{version}.tar.gz

BuildRequires: djcross-gcc
BuildRequires: make

%description
comcom32 is a 32-bit command.com.

%prep
%setup -q

%build
make %{?_smp_mflags}

%check

%install
make DESTDIR=%{buildroot} PREFIX=%{_prefix} install

%files
%defattr(-,root,root)
%{_datadir}/comcom32/comcom32.exe
%{_datadir}/comcom32/command.com

%changelog
