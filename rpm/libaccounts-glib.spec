Name:           libaccounts-glib
Version:        0.58
Release:        1
License:        LGPLv2.1
Summary:        Nokia Maemo Accounts base library
Url:            http://gitorious.org/accounts-sso/accounts-glib
Group:          System/Libraries
Source0:        %{name}-%{version}.tar.gz
Source1:        libaccounts-glib.sh
BuildRequires:  automake
BuildRequires:  gtk-doc
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(check) >= 0.9.4
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libxml-2.0)
BuildRequires:  pkgconfig(sqlite3)

%description
%{summary}.

%package devel
Summary:        Development files for %{name}
Group:          Development/Libraries
Requires:       %{name} = %{version}
Requires:       pkgconfig(glib-2.0)
Requires:       pkgconfig

%description devel
The %{name}-devel package contains libraries and header files for developing
applications that use %{name}.

%package tests
Summary:        Tests for %{name}
Group:          Development/Libraries
Requires:       %{name} = %{version}

%description tests
This package contains %{name} tests.

%prep
%setup -q

%build
gtkdocize
autoreconf -vfi
%configure --disable-static --disable-gtk-doc
make %{?_smp_mflags}

%install
%make_install
install -D -p -m 0644 %{_sourcedir}/%{name}.sh \
%{buildroot}%{_sysconfdir}/profile.d/%{name}.sh

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%doc README COPYING
%config %{_sysconfdir}/profile.d/%{name}.sh
%{_libdir}/libaccounts-glib.so.*
%{_datadir}/backup-framework/applications/accounts.conf

%files devel
%defattr(-,root,root,-)
%{_includedir}/libaccounts-glib/ag-account.h
%{_includedir}/libaccounts-glib/ag-errors.h
%{_includedir}/libaccounts-glib/ag-manager.h
%{_includedir}/libaccounts-glib/ag-provider.h
%{_includedir}/libaccounts-glib/ag-service-type.h
%{_includedir}/libaccounts-glib/ag-service.h
%{_libdir}/libaccounts-glib.so
%{_libdir}/pkgconfig/libaccounts-glib.pc

%files tests
%defattr(-,root,root,-)
%{_bindir}/accounts-glib-test.sh
%{_bindir}/accounts-glib-testsuite
%{_bindir}/test-process
%{_datadir}/libaccounts-glib0-test/e-mail.service-type
%{_datadir}/libaccounts-glib0-test/MyProvider.provider
%{_datadir}/libaccounts-glib0-test/MyService.service
%{_datadir}/libaccounts-glib0-test/OtherService.service
%{_datadir}/libaccounts-glib0-test/tests.xml
%exclude %{_prefix}/doc/reference
