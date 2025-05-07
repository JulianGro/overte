#
#  GenerateDebianServerPackage.cmake
#  cmake/modules
#
#  This gets called by cmake/macros/GenerateInstaller.cmake and is intended to package server Debian packages only.
#  "Server" meaning: domain-server, assignment-client, and oven.
#
#  Created by Julian Groß on 2025-05-03.
#  Copyright 2025 Overte e.V.
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
#

#~ find_program(BASH_EXECUTABLE
  #~ NAMES bash
  #~ PATHS /usr/bin/bash)

#~ find_program(APT_EXECUTABLE
  #~ NAMES apt-get
  #~ PATHS /usr/bin/apt-get)

find_program(DPKG_EXECUTABLE
    NAMES dpkg
    PATHS /usr/bin REQUIRED)

message(STATUS "Checking if required packages are installed…")
execute_process(
  COMMAND
    ${CMAKE_COMMAND} -E
    # dpkg-query returns exit code 1 if one of the packages wasn't found.
    dpkg-query --show chrpath binutils dh-make
    RESULT_VARIABLE packages-found
)

if (NOT packages-found)
    message(STATUS "One or more packages missing.")

    # Fail if apt or sudo are missing.
    find_program(APT_EXECUTABLE
        NAMES apt-get
        PATHS /usr/bin/ REQUIRED)
    find_program(SUDO_EXECUTABLE
        NAMES sudo
        PATHS /usr/bin/ REQUIRED)

    message(STATUS "Updating package index…")
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E
            sudo apt-get update --show chrpath binutils dh-make
            # We can try installing the dependencies, even if updating the index failed.
            COMMAND_ERROR_IS_FATAL NONE
        )
    message(STATUS "Installing packages…")
    execute_process(
        COMMAND
            ${CMAKE_COMMAND} -E
            sudo apt-get install chrpath binutils dh-make
            # Fail if the packages couldn't be installed.
            COMMAND_ERROR_IS_FATAL ANY
        )
else ()
    message(STATUS "Packages found.")
endif ()

message(STATUS "Copying Overte executables…")
file(INSTALL ${CPACK_PACKAGE_DIRECTORY}/assignment-client/assignment-client DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/)
file(INSTALL ${CPACK_PACKAGE_DIRECTORY}/domain-server/domain-server DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/)
file(INSTALL ${CPACK_PACKAGE_DIRECTORY}/tools/oven/oven DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/)
#~ file(COPY ${CPACK_PACKAGE_DIRECTORY}/libraries/*/*.so DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/)
# Copy libraries/*/*.so next to executables.
message(STATUS "Copying Overte libaries…")
file(INSTALL ${CPACK_PACKAGE_DIRECTORY}/libraries/
    DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/
    FILES_MATCHING PATTERN *.so
)

# Maybe done by INSTALL ??
# chrpath -d $DEB_BUILD_ROOT/*


message(STATUS "Copying Conan dependencies…")
# When Conan installs a library, it sets it's DIR to build/generators.
# We use this to find out of a library comes from Conan or is provided by the system.
# The string(FIND) command returns the index the string was found at, instead of true of false.
string(FIND "${libnode_DIR}" "generators" libNODE_IS_CONAN)
if (libNODE_IS_CONAN NOT EQUAL -1)
    message(STATUS "libnode is provided by Conan. Copying…")
    file(INSTALL ${CPACK_PACKAGE_DIRECTORY}/conanlibs/${CMAKE_BUILD_TYPE}/
        DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/
        FILES_MATCHING PATTERN libnode.so*
    )
else ()
    # We leave collecting system libraries to dpkg in a later step.
    message(STATUS "libnode is provided by the system. Nothing to do.")
endif ()

string(FIND "${Qt5_DIR}" "generators" Qt5_IS_CONAN)
if (libNODE_IS_CONAN NOT EQUAL -1)
    # TODO: get Qt from Conan if building without system Qt.
    message(FATAL_ERROR "Qt is provided by Conan. This case is not handled yet!")
else ()
    message(STATUS "Qt is provided by the system. Nothing to do.")
endif ()

# hack: we get libttb.so.12 from conan-libs folder, because the dpkg command at the end of this file cannot always find libtbb12
# cp $OVERTE/build/conanlibs/Release/libttb.so.12 $DEB_BUILD_ROOT


#strip --strip-all $DEB_BUILD_ROOT/*

file(COPY ${CPACK_CMAKE_SOURCE_DIR}/pkg-scripts/new-server DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/)
file(COPY ${CPACK_CMAKE_SOURCE_DIR}/domain-server/resources/ DESTINATION ${CPACK_TEMPORARY_DIRECTORY}/resources/)


find $DEB_BUILD_ROOT/resources -name ".gitignore" -delete
find $DEB_BUILD_ROOT/resources -type f -executable -exec sh -c 'chmod -x {}' \;
cp $OVERTE/README.md $DEB_BUILD_ROOT
cp -a $OVERTE/build/assignment-client/plugins $DEB_BUILD_ROOT
strip --strip-all $DEB_BUILD_ROOT/plugins/*.so
strip --strip-all $DEB_BUILD_ROOT/plugins/*/*.so

#begin the debian package construction
cd $DEB_BUILD_ROOT
dh_make -p overte-server_$DEBVERSION -c apache -s --createorig -y

cp $OVERTE/pkg-scripts/overte-assignment-client.service debian
cp $OVERTE/pkg-scripts/overte-assignment-client@.service debian
cp $OVERTE/pkg-scripts/overte-domain-server.service debian
cp $OVERTE/pkg-scripts/overte-domain-server@.service debian
cp $OVERTE/pkg-scripts/overte-server.target debian
cp $OVERTE/pkg-scripts/overte-server@.target debian

cp $OVERTE/pkg-scripts/server-compat debian/compat
cp $OVERTE/pkg-scripts/server-control debian/control
cp $OVERTE/pkg-scripts/server-prerm debian/prerm
cp $OVERTE/pkg-scripts/server-postinst debian/postinst
cp $OVERTE/pkg-scripts/server-postrm debian/postrm
cp $OVERTE/LICENSE debian/copyright

echo /etc/opt/overte > debian/dirs
echo /var/lib/overte >> debian/dirs

echo README.md > debian/docs

echo assignment-client opt/overte > debian/install
echo domain-server opt/overte >> debian/install
echo oven opt/overte >> debian/install
echo new-server opt/overte >> debian/install
for so in *.so*; do
	echo $so opt/overte/lib >> debian/install
done
#for service in *.service; do
#	echo $service opt/overte/systemd >> debian/install
#done
#for target in *.target; do
#	echo $target opt/overte/systemd >> debian/install
#done
find resources -type f -exec sh -c 'echo {} opt/overte/$(dirname "{}") >> debian/install' \;
find plugins -type f -exec sh -c 'echo {} opt/overte/$(dirname "{}") >> debian/install' \;

if [ ! "$OVERTE_USE_SYSTEM_QT" ]; then
	SOFILES=`ls *.so *.so.*.*.* | grep -Po '^(.+\.so(\.\d+)?)' | sed 's/\./\\\./g' | paste -d'|' -s`
else
	SOFILES=`ls *.so | grep -Po '^(.+\.so(\.\d+)?)' | sed 's/\./\\\./g' | paste -d'|' -s`
fi

# dpkg -S can only find packages which are already installed on the system.
DEPENDS=`find * -path debian -prune -o -type f -executable -exec sh -c 'objdump -p {} | grep NEEDED' \; \
	| awk '{print $2}' | sort | uniq | egrep -v "^($SOFILES)$" \
	| xargs -n 1 -I {} sh -c 'dpkg -S {} | head -n 1' | cut -d ':' -f 1 | sort | uniq | paste -d',' -s`

cp $OVERTE/pkg-scripts/server-rules debian/rules
sed "s/{DEPENDS}/$DEPENDS/" $OVERTE/pkg-scripts/server-control > debian/control

dpkg-buildpackage -us -uc

mv $OVERTE/pkg-scripts/temp-make-deb/*.deb $OVERTE/pkg-scripts/  # move package out of temp-make-deb
