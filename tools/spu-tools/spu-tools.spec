Summary: user space tools for Cell/B.E.
Name: spu-tools
Version: 1.0
Release: 2
License: GPL
Group: Applications/System
Source0: spu-tools.tar.bz2

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: ncurses
BuildRequires: help2man
%define _prefix /usr

%description
The spu-tools package contains user space tools for Cell/B.E.
Currently, it contain two tools:
- spu-top: a tool like top to watch the SPU's on a Cell BE
System. It shows information about SPUs and running SPU contexts.
- spu-ps: a tool like ps, which dumps a report on the currently
running SPU contexts.

%prep
%setup -c -q

%build
cd src/
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd src/
make DESTDIR=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir /%{_prefix}/bin/spu-top
%dir /%{_prefix}/bin/spu-ps
%dir /%{_prefix}/share/man/man1/spu-top.1.gz
%dir /%{_prefix}/share/man/man1/spu-ps.1.gz

%changelog
* Thu Jun 14 2007  Andre Detsch <adetsch@br.ibm.com> 1.0-2
- Setting BuildRoot at spec file.
- General fixes (src/ChangeLog)

* Tue May 29 2007  Andre Detsch <adetsch@br.ibm.com> 1.0
- Package rename: spu-top to spu-tools.
- Complete rewrite for Cell SDK 3.0.

* Tue Apr 03 2007  Markus Deuling <deuling@de.ibm.com> 0.2
- Seperate Process info and IRQ stats.
- Use ncurses library.
- Retrieve number of CPUs in the system.
- Remove valid flag.

* Tue Mar 13 2007  Markus Deuling <deuling@de.ibm.com> 0.1
- Initial version.
