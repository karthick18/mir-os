#!/bin/sh
img=${1:-grubimg}
mkdir -p /mir_img
losetup -d /dev/loop0 1>/dev/null 2>&1
losetup /dev/loop0 $img
mount /dev/loop0 /mir_img 2>/dev/null
cp mir.elf /mir_img/boot
cp -r modules /mir_img
umount /mir_img
losetup -d /dev/loop0 1>/dev/null 2>&1

