%define release %{_version}
%define build_all 1
Name: libspe2
Version: 2.3.0
Release: %{release}
License: LGPL
Group: System Environment/Base
URL: http://www.bsc.es/projects/deepcomputing/linuxoncell
Source: http://www.bsc.es/projects/deepcomputing/linuxoncell/development/release2.0/libspe/%{name}-%{version}.%{release}.tar.gz
Buildroot: %{_tmppath}/libspe
Exclusivearch: ppc ppc64 noarch
Summary: SPE Runtime Management Library

# to build the wrapper, call rpmbuild with --define="with_wrapper 1" 
%define WITH_WRAPPER  %{?with_wrapper:%with_wrapper}%{!?with_wrapper:0}

%if %{build_all}
%define build_common 1
%else
%ifarch ppc
%define build_common 1
%else
%define build_common 0
%endif
%endif

%ifarch noarch
%define sysroot %{nil}
%define set_optflags %{nil}
%else
%define sysroot %{nil}
%define set_optflags OPTFLAGS="%{optflags}"
%endif

%define sysroot %{nil}
%define set_optflags OPTFLAGS="%{optflags}"

%ifarch ppc
%define _libdir /usr/lib
%endif
%ifarch ppc64
%define _libdir /usr/lib64
%endif
%define _adabindingdir /usr/adainclude
%define _includedir2 /usr/spu/include
%define _spe_ld_dir /usr/lib/spe

%define _initdir /etc/init.d
%define _unpackaged_files_terminate_build 0

%if %{WITH_WRAPPER}
%package -n libspe
Summary: SPE Runtime Management Wrapper Library
Group: Development/Libraries
Requires: %{name} = %{version}
%endif

%package devel
Summary: SPE Runtime Management Library
Group: Development/Libraries
Requires: %{name} = %{version}

%if %{WITH_WRAPPER}
%package -n libspe-devel
Summary: SPE Runtime Management Library
Group: Development/Libraries
Requires: %{name} = %{version}
%endif

%if %{build_common}
%package -n elfspe2
Summary: Helper for standalong SPE applications
Group: Applications/System
Requires: %{name} = %{version}
%endif

%if %{build_common}
%package adabinding
Summary : Ada package files porting libspe headers
Group: Development/Libraries
Requires: %{name} = %{version}
%endif

%description
SPE Runtime Management Library for the
Cell Broadband Engine Architecture.

%if %{WITH_WRAPPER}
%description -n libspe
Header and object files for SPE Runtime
Management Wrapoer Library.
%endif

%description devel
Header and object files for SPE Runtime
Management Library.

%if %{WITH_WRAPPER}
%description -n libspe-devel
Header and object files for SPE Runtime
Management Library.
%endif

%if %{build_common}
%description adabinding
Ada package files porting libspe headers
Management Library.
%endif

%if %{build_common}
%description -n elfspe2
This tool acts as a standalone loader for spe binaries.
%endif

%prep

%setup

%build

%define _make_flags  ARCH=%{_target_cpu} SYSROOT=%{sysroot} %{set_optflags} prefix=%{_prefix} libdir=%{_libdir} spe_ld_dir=%{_spe_ld_dir}

make %{_make_flags}
%if %{build_common}
make elfspe-all %{_make_flags}
%endif

%install
rm -rf $RPM_BUILD_ROOT%{sysroot}

make install DESTDIR=$RPM_BUILD_ROOT %{_make_flags}
%if %{build_common}
make elfspe-install DESTDIR=$RPM_BUILD_ROOT %{_make_flags}
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

%if %{build_common}
%preun -n elfspe2
[ -f %{_initdir}/elfspe ] && /sbin/chkconfig --del elfspe
[ -f %{_bindir}/elfspe-unregister ] && %{_bindir}/elfspe-unregister

%post -n elfspe2
[ -f %{_initdir}/elfspe ] && /sbin/chkconfig --add elfspe
[ -f %{_bindir}/elfspe-register ] && %{_bindir}/elfspe-register
%endif

%files
%defattr(-,root,root)
%{sysroot}%{_libdir}/*2.so.*

%if %{WITH_WRAPPER}
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
%{sysroot}%{_libdir}/pkgconfig/libspe2.pc

%if %{WITH_WRAPPER}
%files -n libspe-devel
%defattr(-,root,root)
%{sysroot}%{_libdir}/libspe.so
%{sysroot}%{_libdir}/libspe.a
%{sysroot}%{_includedir}/libspe.h
%endif

%if %{build_common}
%files -n elfspe2
%defattr(-,root,root)
%{sysroot}%{_bindir}/elfspe-register
%{sysroot}%{_bindir}/elfspe-unregister
%{sysroot}%{_bindir}/elfspe
%{sysroot}/etc/init.d/elfspe
%endif

%if %{build_common}
%files adabinding
%defattr(-,root,root)
%{sysroot}%{_adabindingdir}/libspe2.ads
%{sysroot}%{_adabindingdir}/libspe2_types.ads
%{sysroot}%{_adabindingdir}/cbea_map.ads
%{sysroot}%{_adabindingdir}/README-libspe2
%endif

%changelog
* Thu Nov 24 2005 - Arnd Bergmann <arnd@arndb.de> 1.0.1-3
- initial spec file for fc5

