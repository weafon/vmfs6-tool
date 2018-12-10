%define vmfs %{_builddir}/vmfs

Summary: VMFS
Name: vmfs
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
Yooo

%prep
%setup -q -c

%build
make -j

%check
# Run any sanity checks.

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/lib64
mkdir -p %{buildroot}/sbin
cp libreadcmd/libreadcmd.a %{buildroot}/lib64/
cp libvmfs/libvmfs.a %{buildroot}/lib64/
cp debugvmfs/debugvmfs %{buildroot}/sbin/
cp vmfs-lvm/vmfs-lvm %{buildroot}/sbin/
cp vmfs-fuse/vmfs-fuse %{buildroot}/sbin/
cp imager/imager %{buildroot}/sbin/
cp fsck.vmfs/fsck.vmfs %{buildroot}/sbin/

%clean

%files
%defattr(-,root,root,-)
/sbin/*
/lib64/*

%changelog
* CHANGELOG_DATE REL_USER REL_EMAIL %{version}-%{sku}-%{revision}
- release build
