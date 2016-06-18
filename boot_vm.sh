qemu-system-x86_64 --enable-kvm -m 1024 -smp 1 \
    -chardev socket,id=char0,path=./vhost$1 \
    -netdev type=vhost-user,id=mynet0,chardev=char0,vhostforce \
    -device virtio-net-pci,netdev=mynet0,mac=08:00:22:33:44:5$1 \
    -object memory-backend-file,id=mem,size=1024M,mem-path=/mnt/huge,share=on \
    -numa node,memdev=mem -mem-prealloc \
    -vnc :1$1 \
    -net user,hostfwd=tcp::1002$1-:22 -net nic \
    vm"$1".qcow2
#     cirros-0.3.2-x86_64-disk-$1.img
