#!/bin/bash
# Lainux VM Installation Script
echo 'Starting Lainux VM installation...'

# Start VM with installation media
qemu-system-x86_64 \
  -m 4096 \
  -smp 4 \
  -drive file=lainux-vm.qcow2,format=qcow2 \
  -cdrom "lainux.iso" \
  -boot d \
  -net nic \
  -net user \
  -vga virtio \
  -display gtk \
if grep -q -E '(vmx|svm)' /proc/cpuinfo; then
  echo 'KVM acceleration enabled'
  qemu-system-x86_64 -enable-kvm \
    -m 4096 \
    -smp 4 \
    -drive file=lainux-vm.qcow2,format=qcow2 \
    -cdrom "lainux.iso" \
    -boot d \
    -net nic \
    -net user \
    -vga virtio \
    -display gtk
else
  echo 'Running without KVM acceleration'
  qemu-system-x86_64 \
    -m 4096 \
    -smp 4 \
    -drive file=lainux-vm.qcow2,format=qcow2 \
    -cdrom "lainux.iso" \
    -boot d \
    -net nic \
    -net user \
    -vga virtio \
    -display gtk
fi
