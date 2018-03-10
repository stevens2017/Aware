#!/bin/sh

ROOT_PATH=/root/alpha/dpdk

modprobe uio
insmod $ROOT_PATH/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko
insmod $ROOT_PATH/x86_64-native-linuxapp-gcc/kmod/rte_kni.ko kthread_mode=multiple

ifconfig  ens34 down
ifconfig  ens35 down
ifconfig  ens36 down
$ROOT_PATH/tools/dpdk-devbind.py -b igb_uio 0000:02:02.0
$ROOT_PATH/tools/dpdk-devbind.py -b igb_uio 0000:02:03.0
$ROOT_PATH/tools/dpdk-devbind.py -b igb_uio 0000:02:04.0

