#!/bin/bash

########################################################################
# Install build-time and run-time dependencies
########################################################################

export DEBIAN_FRONTEND=noninteractive

########################################################################
# Build pixman 0.40 (needed for dithering)
########################################################################

git clone https://gitlab.freedesktop.org/pixman/pixman.git
cd pixman
git checkout pixman-0.40.0
./autogen.sh --prefix /usr --libdir /usr/lib/x86_64-linux-gnu
make install -j$(nproc)
cd ..

########################################################################
# Build Inkscape and install to appdir/
########################################################################

mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_BINRELOC=ON \
-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_C_COMPILER_LAUNCHER=ccache \
-DCMAKE_CXX_COMPILER_LAUNCHER=ccache -DWITH_INTERNAL_CAIRO=ON
make -j$(nproc)
make DESTDIR=$PWD/appdir -j$(nproc) install ; find appdir/
cp ../packaging/appimage/AppRun appdir/AppRun ; chmod +x appdir/AppRun
cp ./appdir/usr/share/icons/hicolor/256x256/apps/org.inkscape.Inkscape.png ./appdir/
sed -i -e 's|^Icon=.*|Icon=org.inkscape.Inkscape|g' ./appdir/usr/share/applications/org.inkscape.Inkscape.desktop # FIXME
cd appdir/

########################################################################
# Bundle everyhting
# to allow the AppImage to run on older systems as well
########################################################################

apt_bundle() {
    apt-get download "$@"
    find *.deb -exec dpkg-deb -x {} . \;
    find *.deb -delete
}

make_ld_launcher() {
    cat > "$2" <<EOF
#!/bin/sh
HERE="\$(dirname "\$(readlink -f "\${0}")")"
exec "\${HERE}/../../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2" "\${HERE}/$1" "\$@"
EOF
    chmod a+x "$2"
}

# Bundle all of glibc; this should eventually be done by linuxdeployqt
apt update
apt_bundle libc6

# Make absolutely sure it will not load stuff from /lib or /usr
sed -i -e 's|/usr|/xxx|g' lib/x86_64-linux-gnu/ld-linux-x86-64.so.2

# Bundle Gdk pixbuf loaders without which the bundled Gtk does not work;
# this should eventually be done by linuxdeployqt
apt_bundle librsvg2-common libgdk-pixbuf2.0-0
cp /usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/loaders/* usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/loaders/
cp /usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/loaders.cache usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/
sed -i -e 's|/usr/lib/x86_64-linux-gnu/gdk-pixbuf-.*/.*/loaders/||g' usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/loaders.cache

# Bundle fontconfig settings
mkdir -p etc/fonts/
cp /etc/fonts/fonts.conf etc/fonts/

# Bundle Python
PY_VER=3.8
apt_bundle \
    libpython${PY_VER}-stdlib \
    libpython${PY_VER}-minimal \
    python${PY_VER} \
    python${PY_VER}-minimal \
    python3-lxml \
    python3-numpy \
    python3-six \
    python3-scour \
    python3-requests \
    python3-cachecontrol \
    python3-urllib3 \
    python3-chardet \
    python3-certifi \
    python3-idna \
    python3-msgpack \
    python3-lockfile \
    python3-cssselect \
    python3-distutils \
    python3-packaging \
    python3-appdirs \
    python3-bs4 \
    python3-gi \
    python3-gi-cairo \
    python3-pil \
    gir1.2-glib-2.0 \
    gir1.2-gtk-3.0 \
    gir1.2-gdkpixbuf-2.0 \
    gir1.2-pango-1.0
(
    cd usr/bin
    make_ld_launcher "python${PY_VER}" python3
    make_ld_launcher "python${PY_VER}" python
)

# Compile GLib schemas if the subdirectory is present in the AppImage
# AppRun has to export GSETTINGS_SCHEMA_DIR for this to work
apt_bundle gnome-settings-daemon-common
mkdir -p usr/share/glib-2.0/schemas/
cp /usr/share/glib-2.0/schemas/*.gschema.xml usr/share/glib-2.0/schemas/
if [ -d usr/share/glib-2.0/schemas/ ] ; then
  ( cd usr/share/glib-2.0/schemas/ ; glib-compile-schemas . )
fi

cd -

########################################################################
# Generate AppImage
########################################################################

wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt-continuous-x86_64.AppImage

linuxdeployqtargs=()

for so in $(find \
    appdir/usr/lib/x86_64-linux-gnu/gdk-pixbuf-*/*/loaders \
    appdir/usr/lib/python3 \
    appdir/usr/lib/python${PY_VER} \
    -name \*.so); do
    linuxdeployqtargs+=("-executable=$(readlink -f "$so")")
done

./linuxdeployqt-continuous-x86_64.AppImage --appimage-extract-and-run appdir/usr/share/applications/org.inkscape.Inkscape.desktop \
  -appimage -unsupported-bundle-everything -executable=appdir/usr/bin/inkview \
  -executable=appdir/usr/lib/inkscape/libinkscape_base.so \
  -executable=appdir/usr/bin/python${PY_VER} \
  "${linuxdeployqtargs[@]}"

#  -executable=appdir/usr/lib/x86_64-linux-gnu/inkscape/libinkscape_base.so \
mv Inkscape*.AppImage* ../
