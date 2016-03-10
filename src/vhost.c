#include <rte_virtio_net.h>

int
vhost_dev_create()
{
    int ret = rte_vhost_driver_register("vhost0");
    return ret;
}
