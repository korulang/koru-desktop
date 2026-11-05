/* drm-virtio-gpu probe main — PCI driver registration; the work is in gpu.c */
#include <stdio.h>
#include <uk/bus/pci.h>

int gpu_probe(struct pci_device *dev);
static int gfound;
static int add(struct pci_device *dev)
{
	int r = gpu_probe(dev);
	if (!r) gfound = 1;
	return r;
}
static const struct pci_device_id ids[] = {
	{ PCI_DEVICE_ID(0x1af4, 0x1050) },   /* virtio-gpu modern */
	{ PCI_DEVICE_ID(0x1af4, 0x1010) },   /* virtio-gpu transitional */
	{ PCI_ANY_DEVICE_ID },
};
static struct pci_driver drv = { .device_ids = ids, .add_dev = add };
PCI_REGISTER_DRIVER(&drv);

int main(void)
{
	printf("vgpu: guest up found=%d\n", gfound);
	return 0;
}
