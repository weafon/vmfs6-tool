#!/bin/bash -e
set -x
PROJ=vmfs
PROJSRC=.
PROJSPEC=rpmbuild/${PROJ}.spec
function tarballGen() {
	local NAME=$1
	local PROJPATH=$2 # rev path
	local GIT_HASH=""
	if [ -e "${PROJPATH}/.git" ]
	then
		GIT_HASH=$(git --git-dir=${PROJPATH}/.git rev-parse --short HEAD)
		git --git-dir=${PWD}/${PROJPATH}/.git archive --format=tar HEAD -o ${RPMBUILD_SRC}/${NAME}-${GIT_HASH}.tar
		rm -f ${RPMBUILD_SRC}/${NAME}-${GIT_HASH}.tar.bz2
		bzip2 ${RPMBUILD_SRC}/${NAME}-${GIT_HASH}.tar
		echo "${NAME}-${GIT_HASH}"
	else
		echo "$0: Error - project folder is not a git repository"
		exit 1
	fi
}
# general variables
REL_USER="Builder"
REL_EMAIL="builder@accelstor.com"
REL_DATE=$(date +%Y%m%d)
CHANGELOG_DATE=$(date +"%a %b %_d %Y")
# rpmbuild folders
RPMBUILD=$(echo /tmp/rpmbuild/${PROJ} | sed s#-#_#g)
RPMBUILD_SRC="${RPMBUILD}/SOURCES"
RPMBUILD_SPEC="${RPMBUILD}/SPECS/${PROJ}.spec"
# decide rpm version 
if [ -d "${PROJSRC}/.git" ]
then
	GIT_HASH=$(git --git-dir=${PROJSRC}/.git rev-parse --short HEAD)
	RPM_VER="${REL_DATE}_${GIT_HASH}"
else
	echo "$0: Error - project folder is not a git repository"
	exit 1
fi
# prepare rpmbuild folder
rm -rf ${RPMBUILD}/BUILD
rm -rf ${RPMBUILD}/BUILDROOT
rm -rf ${RPMBUILD}/SOURCES
mkdir -p ${RPMBUILD}/{SOURCES,SPECS}
# generate tarball
PROJTAR=$(tarballGen ${PROJ} ${PROJSRC})
# general spec file
cp -f ${PROJSPEC} ${RPMBUILD_SPEC}
# generate replacement
sed -i -e "s#PROJTAR#${PROJTAR}#g" ${RPMBUILD_SPEC}
sed -i -e "s#RPM_VER#${RPM_VER}#g" ${RPMBUILD_SPEC}
sed -i -e "s#GIT_HASH#${GIT_HASH}#g" ${RPMBUILD_SPEC}
sed -i -e "s#REL_USER#${REL_USER}#g" ${RPMBUILD_SPEC}
sed -i -e "s#REL_EMAIL#${REL_EMAIL}#g" ${RPMBUILD_SPEC}
sed -i -e "s#CHANGELOG_DATE#${CHANGELOG_DATE}#g" ${RPMBUILD_SPEC}
# generate changelog
# echo "generating changelog"
# git --git-dir=${PROJSRC}/.git log -20 --format="* %cd %aN%n- (%h) %s%d%n" --date=local | sed -r 's/[0-9]+:[0-9]+:[0-9]+ //' >> ${RPMBUILD_SPEC}
# copy other stuff to source folder
if [ -d "rpmbuild/sources" ]
then
	cp -f rpmbuild/sources/* ${RPMBUILD_SRC}/
fi
# build rpm
rpmbuild -D _topdir${RPMBUILD} -ba ${RPMBUILD_SPEC}
if [ $? -eq 0 ]; then
	rm -f ${RPMBUILD}/RPMS/x86_64/*debuginfo*
	mkdir -p dist
	mv ${RPMBUILD}/SRPMS/*.rpm dist/
	mv ${RPMBUILD}/RPMS/x86_64/*${RPM_VER}*.rpm dist/
fi