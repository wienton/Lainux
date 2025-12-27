#!/usr/bin/env bash

set -euo pipefail

# Mount points for chroot
mount --make-rslave --rbind /sys /mnt/sys 2>/dev/null || true
mount --make-rslave --rbind /proc /mnt/proc 2>/dev/null || true
mount --make-rslave --rbind /dev /mnt/dev 2>/dev/null || true

# Basic system configuration
echo 'lainux' > /etc/hostname

cat > /etc/hosts << 'EOF'
127.0.0.1   localhost
::1         localhost
127.0.1.1   lainux.localdomain   lainux
EOF

# Create user
useradd -m -G wheel -s /bin/bash lain 2>/dev/null || true
echo 'lain:lain' | chpasswd
echo 'root:root' | chpasswd

# Configure sudo
echo '%wheel ALL=(ALL) ALL' >> /etc/sudoers

# Clean package cache
pacman -Scc --noconfirm

echo 'Customization completed!'
exit 0
