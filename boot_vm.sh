qemu-system-x86_64 -m 256 -smp 2 \
    -chardev socket,id=char0,path=./vhost0 \
    -netdev type=vhost-user,id=mynet1,chardev=char0,vhostforce \
    -device virtio-net-pci,netdev=mynet1,mac=00:45:57:43:49:41\
    -object memory-backend-file,id=mem,size=256M,mem-path=/mnt/huge,share=on \
    -numa node,memdev=mem -mem-prealloc \
    -vnc :1 \
    ./cirros-0.3.4-x86_64-disk.img
