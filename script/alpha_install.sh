#!/bin/sh

GRUB_PARAMENTS=/etc/default/grub

CPU_CNT=$(cat /proc/cpuinfo | grep processor | wc -l)
CPU_CNT=`expr $CPU_CNT - 1`

#calc cpu isolcpus
i=1
str="1"
while [ $i -lt $CPU_CNT ];
do
    i=`expr $i + 1`
    str=`printf "%s,%s" "$str" "$i"`
done

isolcpus="isolcpus "$str

#add hugepage
cat /proc/cpuinfo  | grep pdpe1g > /dev/null 2>&1
if [ $? -eq 1 ]; then
    cat /proc/cpuinfo  | grep pse > /dev/null 2>&1
    if [ $? -eq 1 ]; then
        logger "No hugepages found, init dpdk failed"        
    else
        sed -i "s|^GRUB_CMDLINE_LINUX=\"\(.*\) rhgb quiet|GRUB_CMDLINE_LINUX=\"\1 hugepages=4096 $isolcpus rhgb quiet|g" $GRUB_PARAMENTS
        sed -i "s|^GRUB_TIMEOUT=\(.*\)|GRUB_TIMEOUT=0|g" $GRUB_PARAMENTS
        sudo grub2-mkconfig -o /boot/grub2/grub.cfg
    fi
else
    sed -i "s|^GRUB_CMDLINE_LINUX=\"\(.*\) rhgb quiet|GRUB_CMDLINE_LINUX=\"\1 default_hugepagesz=1G hugepagesz=1G hugepages=8 $isolcpus rhgb quiet|g" $GRUB_PARAMENTS
    sed -i "s|^GRUB_TIMEOUT=\(.*\)|GRUB_TIMEOUT=0|g" $GRUB_PARAMENTS
    sudo grub2-mkconfig -o /boot/grub2/grub.cfg    
fi

depmod

