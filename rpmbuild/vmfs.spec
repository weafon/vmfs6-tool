%define vmfs %{_builddir}/vmfs

Summary: VMFS
Name: vmfs6-tools
Version: 1
Release: RPM_VER
License: GNU GENERAL PUBLIC LICENSE
Group: System Environment/Base
Vendor: AccelStor Corp.
URL: http://www.accelstor.com/
AutoReq: no


Source: PROJTAR.tar.bz2


Buildroot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch: x86_64

%description
The project is cloned from Glandium.org. More introductions can be found in https://glandium.org/projects/vmfs-tools/
Most code is developped by Christophe Fillot <cf (at) utc.fr> and Mike Hommey <mh (at) glandium.org>.
I cloned the project for the modification of vmfs6 supporting.
The code in the project is only for vmfs6 for now since the format of vmfs6 changed significantly.
It is experimental code. Only debugvmfs and vmfsfuse have been applied on a few cases.

%prep
%setup -q -c

%build
LDFLAGS=-static ./configure
make -j

%check
# Run any sanity checks.

%install
rm -rf %{buildroot}
#mkdir -p %{buildroot}/lib64
mkdir -p %{buildroot}/sbin
#cp libreadcmd/libreadcmd.a %{buildroot}/lib64/
#cp libvmfs/libvmfs.a %{buildroot}/lib64/
#cp debugvmfs/debugvmfs %{buildroot}/sbin/
cp vmfs-lvm/vmfs-lvm %{buildroot}/sbin/vmfs6-lvm
cp vmfs-fuse/vmfs-fuse %{buildroot}/sbin/vmfs6-fuse
#cp imager/imager %{buildroot}/sbin/imager
cp fsck.vmfs/fsck.vmfs %{buildroot}/sbin/fsck.vmfs6

%clean

%files
%defattr(-,root,root,-)
/sbin/*
/lib64/*

%changelog
* CHANGELOG_DATE REL_USER REL_EMAIL %{version}-%{sku}-%{revision}
- release build
