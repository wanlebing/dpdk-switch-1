qemu-system-x86_64 -m 1024 -smp 2 \
    -chardev socket,id=char0,path=./vhost0 \
    -netdev type=vhost-user,id=mynet1,chardev=char0,vhostforce \
    -device virtio-net-pci,netdev=mynet1,mac=52:54:00:02:d9:01 \
    -object memory-backend-file,id=mem,size=1024M,mem-path=/mnt/huge,share=on \
    -numa node,memdev=mem -mem-prealloc \
    -vnc :1 \
    ./cirros-0.3.4-x86_64-disk.img
