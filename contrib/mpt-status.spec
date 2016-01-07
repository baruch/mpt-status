Summary: MPT Fusion based raid inquiry tool for LSI Logic HBAs
Name: mpt-status
Version: 1.2.0
Release: 0
License: GPL
Group: Applications/System
URL: http://www.drugphish.ch/~ratz/mpt-status/

Packager: Rich Edelman (rich.edelman@openwave.com)
Vendor: OpenWave Systems

Source: mpt-status-%{version}.tar.bz2

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
mpt-status requests information about raid status for LSI Logic SCSI controllers.
%prep
%setup

%build
%{__make}

%install
%{__rm} -rf %{buildroot}
%{__install} -D -m0755 mpt-status %{buildroot}%{_sbindir}/mpt-status

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-, root, root, 0755)
%doc doc/*
%{_sbindir}/mpt-status

%changelog
* Fri Jun 30 2006 Roberto Nibali (ratz@drugphish.ch)
- Changed release version

* Fri Jun 30 2006 Rich Edelman (rich.edelman@openwave.com)
- Upgraded to version 1.2.0-RC7
- Changed Makefile to use /lib/modules/`uname -r`/build as the KERNEL_PATH 
  directory, instead of /usr/src/linux. This should make building this package
  across different distributions easier. (Patch mpt-status-fix-kernel-path.diff)

* Thu Jul 14 2005 Jean-Philippe CIVADE <jp.civade@100p100.net> - 1.1.3-0
- Initial package.
