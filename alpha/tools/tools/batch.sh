echo 2048 > /sys/devices/system/node/node0/hugepages/hugepages-2048kB/nr_hugepages
mkdir /mnt/huge
mount -t hugetlbfs nodev /mnt/huge

cp /root/lwip/trunk/dpdk/x86_64-native-linuxapp-gcc/kmod/igb_uio.ko ./
cp /root/lwip/trunk/dpdk/x86_64-native-linuxapp-gcc/kmod/rte_kni.ko ./

modprobe uio
insmod ./igb_uio.ko
insmod ./rte_kni.ko

ifconfig eno33554984 down
./dpdk_nic_bind.py -b igb_uio 0000:02:05.0

