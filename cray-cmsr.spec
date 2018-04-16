#
# spec file for cmsr kernel module package
#
# Copyright 2013,2015 Cray Inc. All Rights Reserved.
#
%define intranamespace_name cmsr

Name: %{namespace}-%{intranamespace_name}
License:      GPLv2
Group:        System/Kernel
Version: %{_tag}
Release: %release
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release} 
Source: %{source_name}.tar.bz2
Summary: The kernel module cmsr
URL: %url
Vendor: Cray
Packager: Cray Inc.

BuildRequires: module-init-tools
BuildRequires: kernel-source
BuildRequires: kernel-syms
%if %sles_version == 10
BuildRequires: cray-rpm
%endif

%if %{with ari}
%cray_kernel_module_package -x cray_ari_c
%endif

%if %{with gni}
%cray_kernel_module_package -x cray_gem_c
%endif

%description
A kernel module to expose specific MSRs to unprivileged userspace processes on compute nodes.

%prep
%incremental_setup -n %{source_name}

set -- *
%{__mkdir} source
%{__cp} -r "$@" source/
%{__mkdir} obj

%build
for flavor in %{flavors_to_build}
do
    %{__rm} -rf obj/${flavor}
    %{__cp} -r source obj/${flavor}
    %{__make} -C /usr/src/linux-obj/%{_target_cpu}/${flavor} modules \
	M=${PWD}/obj/${flavor} \
	%_smp_mflags
done

%install
export INSTALL_MOD_PATH=$RPM_BUILD_ROOT
export INSTALL_MOD_DIR=updates

for flavor in %{flavors_to_build}
do
    %{__make} -C /usr/src/linux-obj/%{_target_cpu}/${flavor} modules_install \
        M=${PWD}/obj/${flavor}
done

# Install modprobe config, with default MSR list
mkdir -p ${RPM_BUILD_ROOT}/etc/modprobe.d
install source/boottime/modprobe.d/cmsr-${flavor}.conf \
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
