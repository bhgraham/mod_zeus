Name:           mod_zeus
BuildRequires:  httpd-devel
Version:        0.2
Release:        1%{?dist}
License:        BSD
Group:          System Environment/Daemons
Requires:       httpd
Summary:        Changes the remote IP in Apache to use client IP and not proxy IP
URL:            https://github.com/bhgraham/mod_zeus
Source:         http://splash.riverbed.com/servlet/JiveServlet/download/12372-1916/modzeus-10-1.zip
Source2:        mod_zeus.conf

%define apxs /usr/sbin/apxs
%define mod_name zeus
%define apache_libexecdir %(%{apxs} -q LIBEXECDIR)

%description
mod_zeus spoofs the source IP address of server-side connections from a Zeus
load balancer so that they appear to originate from the remote client.

%prep
%setup -q -c %{name}-%{version}

%build
%{apxs} -c -n 'zeus' apache-2.x/mod_zeus.c
ld --build-id -Bshareable -o mod_zeus.so apache-2.x/mod_zeus.o

%install
mkdir -p %{buildroot}/%{apache_libexecdir}
install -D -m755 %{name}.so %{buildroot}/%{apache_libexecdir}/%{name}.so
install -D -m644 %{SOURCE2} %{buildroot}/%{_sysconfdir}/httpd/conf.d/mod_zeus.conf

%files
%{apache_libexecdir}/%{name}.so
%config(noreplace) %{_sysconfdir}/httpd/conf.d/mod_zeus.conf

%changelog -n mod_zeus
* Sun Jan 27 2013 - Troy C <troxor0@yahoo.com> - 0.2-1.el6
- initial package for CentOS/RHEL, adapted from Bug 851768
