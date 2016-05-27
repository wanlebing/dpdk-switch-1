qemu-system-x86_64 -m 1024 -smp 2 \
    -chardev socket,id=char$1,path=./vhost$1 \
    -netdev type=vhost-user,id=mynet$1,chardev=char$1,vhostforce \
    -device virtio-net-pci,netdev=mynet$1,mac=08:00:22:33:44:5$1\
    -object memory-backend-file,id=mem,size=1024M,mem-path=/mnt/huge,share=on \
    -numa node,memdev=mem -mem-prealloc \
    -vnc :$1 \
    cirros-0.3.4-x86_64-disk.img

