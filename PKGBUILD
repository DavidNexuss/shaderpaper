# Maintainer: Nexus Prime
pkgname=shaderpaper
pkgver=1.0.0
pkgrel=1
pkgdesc="GLSL-based animated wallpaper shader runner for Linux (uses X11 + OpenGL)"
arch=('x86_64')
url="https://github.com/DavidNexuss/shaderpaper"
license=('MIT')
depends=('libgl' 'libx11')
makedepends=('gcc' 'make')
source=(
  "$pkgname-$pkgver.tar.gz"
)
md5sums=('SKIP')

build() {
  cd "$srcdir/$pkgname-$pkgver"
  make
}

package() {
  cd "$srcdir/$pkgname-$pkgver"
  install -Dm755 shaderpaper "$pkgdir/usr/bin/shaderpaper"
}
