pkgname=docc-to-md
_name=docc-to-md
pkgver=1.0.0
pkgrel=1
pkgdesc="Generate markdown documentation for C source code"
arch=("x86_64" "aarch64")
url="https://git.greensky.tf/Greensky/docc-to-md.git"
license=("unknown")
depends=()
makedepends=('git' 'gcc' 'make')
optdepends=()
source=("git+${url}")
options=("!debug")
sha256sums=("SKIP")

pkgver() {
  echo "$(cat "$srcdir/$_name/src/macros.h" | grep -E '^#define PROG_VERSION' | awk -F' ' '{print $3}' | tr -d '\"')"
}
build() {
  cd "$srcdir/$_name"

  make cleanbuild
}
package() {
  cd "$srcdir/$_name"

  # Binary
  install -Dm755 bin/main.uwu "$pkgdir/usr/bin/docc-to-md"
}
