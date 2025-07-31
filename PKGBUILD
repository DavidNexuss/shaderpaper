# Maintainer: DavidNexuss
pkgname=shaderpaper
pkgver=1.0.0
pkgrel=1
pkgdesc="GLSL-based animated wallpaper shader runner for Linux (uses X11 + OpenGL)"
arch=('x86_64')
url="https://github.com/DavidNexuss/shaderpaper"
license=('MIT')
depends=('libgl' 'libx11' 'glew')
makedepends=('git' 'gcc' 'make')
source=("git+${url}.git#tag=v$pkgver")
md5sums=('SKIP')

build() {
  cd "$srcdir/$pkgname"
  make
}

package() {
  cd "$srcdir/$pkgname"
  make DESTDIR="$pkgdir" install
}
