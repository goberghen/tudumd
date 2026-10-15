#!/usr/bin/env bash
# Build an installable Debian/Ubuntu .deb package for tudumd (Markdown-native fork of TuDu)
# Modeled on the official Debian tudu packaging layout
# Original TuDu Copyright (C) 2007-2020 Ruben Pollan <meskio@sindominio.net>
# tudumd fork & packaging Copyright (C) 2026 goberghen & Arena.ai Agent
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_NAME="tudumd"
UPSTREAM_VER="0.10.5"
DEB_VER="${UPSTREAM_VER}-1"
ARCH="$(dpkg --print-architecture 2>/dev/null || echo amd64)"
DEB_BASENAME="${PKG_NAME}_${DEB_VER}_${ARCH}"
BUILD_DIR="/tmp/tudumd-deb-build"
STAGE_DIR="/tmp/${DEB_BASENAME}"

rm -rf "$BUILD_DIR" "$STAGE_DIR"
mkdir -p "$BUILD_DIR" "$STAGE_DIR/DEBIAN"
cp -a "$SCRIPT_DIR/." "$BUILD_DIR/"

echo "[1/4] Configuring and compiling ${PKG_NAME} ${UPSTREAM_VER}..."
cd "$BUILD_DIR"
./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man
make -j"$(nproc)"
strip --strip-unneeded "$BUILD_DIR/src/tudumd"

echo "[2/4] Installing files into staging root ${STAGE_DIR}..."
make DESTDIR="$STAGE_DIR" install

# Compress manual pages per Debian Policy 12.1
if [ -d "$STAGE_DIR/usr/share/man/man1" ]; then
    find "$STAGE_DIR/usr/share/man/man1" -type f -name "*.1" -exec gzip -9n -f {} +
fi

echo "[3/4] Installing /usr/share/doc/${PKG_NAME} metadata and DEBIAN control files..."
DOC_DIR="$STAGE_DIR/usr/share/doc/${PKG_NAME}"
mkdir -p "$DOC_DIR"
install -m 0644 "$SCRIPT_DIR/README" "$DOC_DIR/README"
install -m 0644 "$SCRIPT_DIR/AUTHORS" "$DOC_DIR/AUTHORS"
install -m 0644 "$SCRIPT_DIR/CONTRIBUTORS" "$DOC_DIR/CONTRIBUTORS"
gzip -9n -c "$SCRIPT_DIR/ChangeLog" > "$DOC_DIR/changelog.gz"

cat > "$DOC_DIR/copyright" <<'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: tudumd (TuDu Markdown fork)
Upstream-Contact: Ruben Pollan <meskio@sindominio.net>
Source: https://code.meskio.net/tudu

Files: *
Copyright: 2007-2020 Ruben Pollan <meskio@sindominio.net>
           2010 Enrique Matías Sánchez <cronopios@gmail.com>
           2026 goberghen & Arena.ai Agent (tudumd Markdown fork)
License: GPL-3
 tudumd is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 3 of the License.
 .
 tudumd is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 On Debian systems, the complete text of the GNU General Public License
 version 3 can be found in '/usr/share/common-licenses/GPL-3'.
EOF
chmod 0644 "$DOC_DIR/copyright"

# Register /etc/tudumdrc as a conffile so dpkg preserves local user edits
cat > "$STAGE_DIR/DEBIAN/conffiles" <<'EOF'
/etc/tudumdrc
EOF
chmod 0644 "$STAGE_DIR/DEBIAN/conffiles"

INSTALLED_KB="$(du -sk "$STAGE_DIR/usr" "$STAGE_DIR/etc" | awk '{sum += $1} END {print sum}')"

cat > "$STAGE_DIR/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${DEB_VER}
Section: utils
Priority: optional
Architecture: ${ARCH}
Installed-Size: ${INSTALLED_KB}
Depends: libc6 (>= 2.34), libgcc-s1 (>= 3.0), libncursesw6 (>= 6), libstdc++6 (>= 11), libtinfo6 (>= 6)
Maintainer: goberghen <goberghen@users.noreply.github.com>
Homepage: https://code.meskio.net/tudu/
Description: Command-line hierarchical ToDo list manager (Markdown-native fork of TuDu)
 tudumd is a Markdown-native fork of TuDu, an ncurses TUI hierarchical ToDo list
 manager originally written by Ruben Pollan.
 .
  Unlike upstream tudu (which uses XML), tudumd reads and writes human-readable
  Markdown (\$XDG_DATA_HOME/tudumd/tudu.md, default ~/.local/share/tudumd/tudu.md)
  compatible with Okular, Ghostwriter, and Quillpad while preserving all TuDu
  features:
   * Hierarchical task trees with collapse/expand states
   * Long multi-line text descriptions
   * Deadlines (@due) with approaching-date warnings
   * Scheduled dates (@sched) and scheduler pane
   * Categories (@tag) and priorities ([!1]..[!5])
   * Automatic import of legacy TuDu XML files and bidirectional tuduconvert CLI
   * Desktop launcher (Terminal=true) for opening directly in a terminal emulator
EOF
chmod 0644 "$STAGE_DIR/DEBIAN/control"

# Generate md5sums
(
    cd "$STAGE_DIR"
    find usr etc -type f -exec md5sum {} + | sort -k2 > "$STAGE_DIR/DEBIAN/md5sums"
)
chmod 0644 "$STAGE_DIR/DEBIAN/md5sums"

OUT_DEB="${SCRIPT_DIR}/${DEB_BASENAME}.deb"
echo "[4/4] Building ${OUT_DEB}..."
dpkg-deb --root-owner-group --build "$STAGE_DIR" "$OUT_DEB"

cd "$SCRIPT_DIR"
make distclean >/dev/null 2>&1 || true
rm -rf "$BUILD_DIR" "$STAGE_DIR"
echo "Successfully built: ${OUT_DEB}"
