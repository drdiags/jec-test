#
# spec file for cmsr kernel module package
#
# Copyright 2013,2015 Cray Inc. All Rights Reserved.
#
%define intranamespace_name cmsr

Name: %{namespace}-%{intranamespace_name}
License:      GPLv2
Group:        System/Kernel
Version: 1.0
Release: 1.0
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release} 
Source: cmsr.tar.bz2
Summary: The kernel module cmsr
URL: %url
Vendor: Cray
Packager: Cray Inc.

BuildRequires: module-init-tools
BuildRequires: kernel-source
BuildRequires: kernel-syms

%description
A kernel module to expose specific MSRs to unprivileged userspace processes on compute nodes.

%prep
%incremental_setup -n cmsr

set -- *
%{__mkdir} source
%{__cp} -r "$@" source/
%{__mkdir} obj

%build
%{__rm} -rf obj/${flavor}
%{__cp} -r source obj/${flavor}
%{__make} -C /usr/src/linux-obj/x86_64/${flavor} modules \
	M=${PWD}/obj/${flavor} \
	%_smp_mflags

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=updates

%{__make} -C /usr/src/linux-obj/x86_64/${flavor} modules_install \
        M=${PWD}/obj/${flavor}

# Install modprobe config, with default MSR list
mkdir -p ${RPM_BUILD_ROOT}/etc/modprobe.d
install source/boottime/modprobe.d/cmsr-cray_ari_c.conf \
    ${RPM_BUILD_ROOT}/etc/modprobe.d/cmsr.conf

# Install required rc scripts
mkdir -p ${RPM_BUILD_ROOT}/etc/init.d
install source/boottime/init.d/cray-msr ${RPM_BUILD_ROOT}/etc/init.d

%post
if ! grep '^include /etc/modprobe.d$' /etc/modprobe.conf >/dev/null 2>/dev/null
then
    echo 'include /etc/modprobe.d' >>/etc/modprobe.conf
fi
/usr/lib/lsb/install_initd %{_base_sysconfdir}/init.d/cray-msr

%preun
/usr/lib/lsb/remove_initd %{_base_sysconfdir}/init.d/cray-msr

%clean
%clean_build_root

%files
%defattr(-,root,root,755)
%config(noreplace) %attr(0644,root,root) /etc/modprobe.d/cmsr.conf
/etc/init.d/cray-msr
