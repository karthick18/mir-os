#!/usr/bin/env sh
config=${1:-bochsrc.txt}
echo "Starting MIR-OS with config $config"
bochs -f $config -rc debug.rc -q
