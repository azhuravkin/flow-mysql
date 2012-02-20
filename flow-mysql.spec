Name: flow-mysql
Version: 1
Release: 3%{?dist}
Group: Applications/System
Summary: Export netflow data to mysql database.
License: GPL
Source0: %{name}-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-root
Requires: flow-tools
BuildRequires: mysql-devel

%description
The program is called at rotation netflow files
and exports the data in mysql database.
For parsing netflow files it is used flow-export
from flow-tools pckage.

%prep
%setup

%install
make clean all

%{__mkdir_p} $RPM_BUILD_ROOT/etc
%{__mkdir_p} $RPM_BUILD_ROOT%{_sbindir}

install flow-mysql.conf $RPM_BUILD_ROOT/etc
install -s -m 755 %{name} $RPM_BUILD_ROOT%{_sbindir}

%files
%defattr(-,root,root,-)
%doc INSTALL netflow.sql
%config(noreplace) %attr(0600,root,root) /etc/flow-mysql.conf
%{_sbindir}/%{name}

%clean
[ $RPM_BUILD_ROOT != "/" ] && rm -rf $RPM_BUILD_ROOT

%changelog
* Thu Nov 25 2010 Juravkin Alexander <rinus@nsys.by>
- Build
