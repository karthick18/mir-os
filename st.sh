#!/bin/sh
img=${1:-grubimg}
mkdir -p /mir_img
losetup -d /dev/loop2 1>/dev/null 2>&1
losetup /dev/loop2 $img
mount /dev/loop2 /mir_img 2>/dev/null
cp mir.elf /mir_img/boot
cp -r modules /mir_img
umount /mir_img
losetup -d /dev/loop2 1>/dev/null 2>&1

