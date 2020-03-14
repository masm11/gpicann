# Maintainer: Yuuki Harano
pkgname=gpicann-git
pkgver=r100.eed0431
pkgrel=1
pkgdesc="Screenshot annotation tool"
arch=('x86_64')
url="https://github.com/masm11/gpicann"
license=('GPL3')
depends=('gtk3' 'cairo' 'pango' 'gdk-pixbuf2')
makedepends=('intltool' 'gcc' 'make')
provides=('gpicann')
conflicts=()
replaces=()
options=()
source=('git+https://github.com/masm11/gpicann')
sha256sums=('SKIP')

prepare() {
  :
}

pkgver() {
  cd "$srcdir/gpicann"

  # Git, no tags available
  printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  mkdir -p "$srcdir/build"
  cd "$srcdir/build"
  ../gpicann/configure --prefix=/usr
  make
}

package() {
  cd "$srcdir/build"
  DESTDIR="$pkgdir/" make install
}
