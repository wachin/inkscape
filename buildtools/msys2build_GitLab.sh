#!/usr/bin/env bash

### functions
message() { echo -e "\e[1;32m\n${1}\n\e[0m"; }
warning() { echo -e "\e[1;33m\nWarning: ${1}\n\e[0m"; }
error()   { echo -e "\e[1;31m\nError: ${1}\n\e[0m";  exit 1; }



### setup

# reduce time required to install packages by disabling pacman's disk space checking
sed -i 's/^CheckSpace/#CheckSpace/g' /etc/pacman.conf

# if repo.msys2.org is unreachable, sort mirror lists by response time and use one of the remaining mirrors
wget --no-verbose --tries=1 --timeout=10 repo.msys2.org 2> /dev/nul || {
    warning "repo.msys2.org is unreachable; using a fall-back mirror"
    for repo in msys ${MSYSTEM_PREFIX#/*}; do
        grep -v .cn /etc/pacman.d/mirrorlist.$repo > /etc/pacman.d/mirrorlist.$repo.bak
        rankmirrors --repo $repo /etc/pacman.d/mirrorlist.$repo.bak > /etc/pacman.d/mirrorlist.$repo
    done
}

# update MSYS2-packages and MINGW-packages (but only for current architecture)
pacman -Quq | grep -v mingw-w64- | xargs pacman -S $PACMAN_OPTIONS
pacman -Quq | grep ${MINGW_PACKAGE_PREFIX} | xargs pacman -S $PACMAN_OPTIONS

# do everything in /build
mkdir build
cd build

# write custom fonts.conf to speed up fc-cache and use/download fonts required for tests
export FONTCONFIG_FILE=$(cygpath -a fonts.conf)
cat > "$FONTCONFIG_FILE" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig><dir>$(cygpath -aw fonts)</dir><cachedir>$(cygpath -aw fontconfig)</cachedir></fontconfig>
EOF

mkdir fonts
dejavufontsarchive="dejavu-fonts-ttf-$$.tar"
dejavufontsarchivebz2="$dejavufontsarchive.bz2"
wget -nv -O $dejavufontsarchivebz2 \
    https://github.com/dejavu-fonts/dejavu-fonts/releases/download/version_2_37/dejavu-fonts-ttf-2.37.tar.bz2 \
    || error "failed to download dejavu fonts"
# Unzip first as workaround to broken tar. https://github.com/actions/virtual-environments/issues/282
bunzip2 --keep $dejavufontsarchivebz2
tar -vxf $dejavufontsarchive -C fonts --strip-components 2 "dejavu-fonts-ttf-2.37/ttf" \
    || error "failed to extract dejavu fonts"
rm -vrf dejavu-fonts-ttf-*

# install dependencies
message "--- Installing dependencies"
source ../buildtools/msys2installdeps.sh
pacman -S $MINGW_PACKAGE_PREFIX-{ccache,gtest,ntldd-git,ghostscript} $PACMAN_OPTIONS

export CCACHE_DIR=$(cygpath -a ccache)
ccache --max-size=500M
ccache --set-config=sloppiness=include_file_ctime,include_file_mtime


### build / test

message "\n\n##### STARTING BUILD #####"

# configure
message "--- Configuring the build"
cmake .. -G Ninja \
    -DCMAKE_C_COMPILER_LAUNCHER="ccache" \
    -DCMAKE_CXX_COMPILER_LAUNCHER="ccache" \
    -DCMAKE_INSTALL_MESSAGE="NEVER" \
    -DWITH_IMAGE_MAGICK=OFF \
    -DWITH_GRAPHICS_MAGICK=ON \
    || error "cmake failed"

# build
message "--- Compiling Inkscape"
ccache --zero-stats
ninja || error "compilation failed"
ccache --show-stats

# install
message "--- Installing the project"
ninja install || error "installation failed"
python  ../buildtools/msys2checkdeps.py check inkscape -w inkscape/bin || error "missing libraries in installed project"

# test
message "--- Running tests"
# check if the installed executable works
inkscape/bin/inkscape.exe -V || error "installed executable won't run"
PATH= inkscape/bin/inkscape.exe -V >/dev/null || error "installed executable won't run with empty PATH (missing dependencies?)"
err=$(PATH= inkscape/bin/inkscape.exe -V 2>&1 >/dev/null)
if [ -n "$err" ]; then warning "installed executable produces output on stderr:"; echo "$err"; fi
# check if the uninstalled executable works
INKSCAPE_DATADIR=inkscape_datadir bin/inkscape.exe -V >/dev/null || error "uninstalled executable won't run"
err=$(INKSCAPE_DATADIR=inkscape_datadir bin/inkscape.exe -V 2>&1 >/dev/null)
if [ -n "$err" ]; then warning "uninstalled executable produces output on stderr:"; echo "$err"; fi
## run tests
#ninja check || {
#    "C:/Program Files/7-Zip/7z.exe" a testfiles.7z testfiles
#    error "tests failed"
#}

message "##### BUILD SUCCESSFUL #####\n\n"


### package
if [ "$CI_COMMIT_TAG" = "true" ]
then
    ninja dist-win-all
else
    ninja dist-win-7z-fast
fi

# create redirect to the 7z archive we just created (and are about to upload as an artifact)
FILENAME=$(ls inkscape*.7z)
URL=$CI_PROJECT_URL/-/jobs/$CI_JOB_ID/artifacts/raw/build/$FILENAME
BRANCH=$CI_COMMIT_BRANCH
HTMLNAME=latest_${BRANCH}_x${MSYSTEM#MINGW}.html
sed -e "s#\${FILENAME}#${FILENAME}#" -e "s#\${URL}#${URL}#" -e "s#\${BRANCH}#${BRANCH}#" ../buildtools/ci_redirect_template.html > $HTMLNAME
# upload redirect to http://alpha.inkscape.org/snapshots/
if [ "${CI_PROJECT_PATH}" == "inkscape/inkscape" ] && [ -n "${INKSCAPE_CI_SSH_KEY}" ]; then
    if [ "$BRANCH" == "master" ]; then
        echo -e "-----BEGIN RSA PRIVATE KEY-----\n${INKSCAPE_CI_SSH_KEY}\n-----END RSA PRIVATE KEY-----" > ssh_key
        scp -oStrictHostKeyChecking=no -i ssh_key $HTMLNAME appveyor-ci@alpha.inkscape.org:/var/www/alpha.inkscape.org/public_html/snapshots/
        rm -f ssh_key
    fi
fi
