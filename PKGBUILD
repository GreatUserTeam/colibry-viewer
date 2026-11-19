# Maintainer: Colibry OS contributors
pkgname=colibry-view
pkgver=0.2.0
pkgrel=1
pkgdesc="Colibry OS: Viewer for text, hex and images with a media player and background mode"
arch=('x86_64')
url="https://github.com/GreatUserTeam/colibry-view"
license=('MIT')
depends=('vulkan-icd-loader' 'libx11' 'libxi')
optdepends=('mpv: media player' 'ffmpeg: ffplay as a player')
makedepends=('cmake' 'vulkan-headers' 'git')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

# Raven (Apache-2.0) is not in the AUR: CMake downloads it at a pinned commit when no installed Raven is found.

build() {
    cmake -B build -S "$srcdir/$pkgname-$pkgver" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
        -DCOLIBRY_BUILD_TESTS=OFF
    cmake --build build
}

check() {
    cmake -B build-test -S "$srcdir/$pkgname-$pkgver" -DCOLIBRY_BUILD_TESTS=ON
    cmake --build build-test
    (cd build-test && ctest --output-on-failure)
}

package() {
    DESTDIR="$pkgdir" cmake --install "$srcdir/build"
    install -Dm644 "$srcdir/$pkgname-$pkgver/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
