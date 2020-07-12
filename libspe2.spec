%define release 0
Name: libspe2
Version: 2.1.0
Release: %{release}
License: LGPL
Group: System Environment/Base
URL: http://www.bsc.es/projects/deepcomputing/linuxoncell
Source: http://www.bsc.es/projects/deepcomputing/linuxoncell/development/release2.0/libspe/%{name}-%{version}.tar.gz
Buildroot: %{_tmppath}/libspe
Exclusivearch: ppc ppc64 noarch
Summary: SPE Runtime Management Library

%ifarch noarch
%define sysroot %{nil}
%define set_optflags %{nil}
%else
%define sysroot %{nil}
%define set_optflags OPTFLAGS="%{optflags}"
%endif

%ifarch ppc
%define _libdir /usr/lib
%endif
%ifarch ppc64
%define _libdir /usr/lib64
%endif
%define _includedir2 /usr/spu/include

%define _initdir /etc/init.d
%define _unpackaged_files_terminate_build 0

%ifarch ppcnone
%package -n libspe
Summary: SPE Runtime Management Wrapper Library
Group: Development/Libraries
Requires: %{name} = %{version}
%endif

%package devel
Summary: SPE Runtime Management Library
Group: Development/Libraries
Requires: %{name} = %{version}

%ifarch ppcnone
%package -n libspe-devel
Summary: SPE Runtime Management Library
Group: Development/Libraries
Requires: %{name} = %{version}
%endif

%ifarch ppc
%package -n elfspe2
Summary: Helper for standalong SPE applications
Group: Applications/System
Requires: %{name} = %{version}
%endif

%description
SPE Runtime Management Library for the
Cell Broadband Engine Architecture.

%ifarch ppcnone
%description -n libspe
Header and object files for SPE Runtime
Management Wrapoer Library.
%endif

%description devel
Header and object files for SPE Runtime
Management Library.

%ifarch ppcnone
%description -n libspe-devel
Header and object files for SPE Runtime
Management Library.
%endif

%ifarch ppc
%description -n elfspe2
This tool acts as a standalone loader for spe binaries.
%endif

%prep

%setup

%build
make SYSROOT=%{sysroot} %{set_optflags} prefix=%{_prefix} libdir=%{_libdir}
%ifarch ppc
make elfspe-all SYSROOT=%{sysroot} %{set_optflags} prefix=%{_prefix} libdir=%{_libdir}
%endif

%install
rm -rf $RPM_BUILD_ROOT%{sysroot}

make install DESTDIR=$RPM_BUILD_ROOT SYSROOT=%{sysroot} prefix=%{_prefix} libdir=%{_libdir} speinclude=%{_includedir2}
%ifarch ppc
make elfspe-install DESTDIR=$RPM_BUILD_ROOT SYSROOT=%{sysroot} prefix=%{_prefix} libdir=%{_libdir} speinclude=%{_includedir2}
%endif

mkdir -p $RPM_BUILD_ROOT%{sysroot}%{_initdir}
cat > $RPM_BUILD_ROOT%{sysroot}%{_initdir}/elfspe <<"EOF"
#!/bin/bash
#
#	/etc/rc.d/init.d/elfspe
#
#	registers elfspe handler
#
# chkconfig: 345 1 1 
# description: executes elfspe-register

# Source function library.
. /etc/init.d/functions


start() {
	echo -n "Starting elfspe: "
	sh /usr/bin/elfspe-register
	return 0
}	

stop() {
	echo -n "Shutting down elfspe: "
	sh /usr/bin/elfspe-unregister
	return 0
}

case "$1" in
    start)
	start
	;;
    stop)
	stop
	;;
    status)
	;;
    restart)
    	stop
	start
	;;
    reload)
	;;
    condrestart)
	[ -f /var/lock/subsys/elspe ] && restart || :
	;;
    *)
	echo "Usage: elfspe {start|stop|status|reload|restart"
	exit 1
	;;
esac
exit $?
EOF
chmod +x $RPM_BUILD_ROOT%{sysroot}%{_initdir}/elfspe
				
%clean
rm -rf $RPM_BUILD_ROOT

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%ifarch ppc
%preun -n elfspe2
[ -f %{_initdir}/elfspe ] && /sbin/chkconfig --del elfspe

%post -n elfspe2
[ -f %{_initdir}/elfspe ] && /sbin/chkconfig --add elfspe
[ -f %{_bindir}/elfspe-register ] && %{_bindir}/elfspe-register
%endif

%files
%defattr(-,root,root)
%{sysroot}%{_libdir}/*2.so.*

%ifarch ppcnone
%files -n libspe
%defattr(-,root,root)
%{sysroot}%{_libdir}/libspe.so.*
%endif

%files devel
%defattr(-,root,root)
%{sysroot}%{_libdir}/*2.so
%{sysroot}%{_libdir}/*2.a
%{sysroot}%{_includedir}/*2*.h
%{sysroot}%{_includedir}/cbea_map.h
%{sysroot}%{_includedir2}/*.h

%ifarch ppcnone
%files -n libspe-devel
%defattr(-,root,root)
%{sysroot}%{_libdir}/libspe.so
%{sysroot}%{_libdir}/libspe.a
%{sysroot}%{_includedir}/libspe.h
%endif

%ifarch ppc
%files -n elfspe2
%defattr(-,root,root)
%{sysroot}%{_bindir}/elfspe-register
%{sysroot}%{_bindir}/elfspe-unregister
%{sysroot}%{_bindir}/elfspe
%{sysroot}/etc/init.d/elfspe
%endif

%changelog
* Thu Nov 24 2005 - Arnd Bergmann <arnd@arndb.de> 1.0.1-3
- initial spec file for fc5

