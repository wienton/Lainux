pkgname=lainux-core
pkgver=0.1
pkgrel=1
pkgdesc="Core files and security tweaks for Lainux OS"
arch=('x86_64')
url="https://github.com/wienton/Lainux"
license=('GPL')
depends=('base' 'linux-firmware' 'nasm' 'gcc' 'git' 'vim')

package() {
    cp -r "$startdir/lainux-core/etc" "$pkgdir/"
    cp -r "$startdir/lainux-core/usr" "$pkgdir/"
    
    chmod +x "$pkgdir/usr/local/bin/"* 2>/dev/null || true
}
