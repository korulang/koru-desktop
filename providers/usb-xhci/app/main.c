/* usb-xhci acceptance guest: probe PCI for the xHCI class signature,
 * run the driver, print evidence to serial. SPDX: BSD-3-Clause */
#include <stdio.h>
#include <uk/bus/pci.h>
#include "xhci.h"

static int found;
static int add(struct pci_device *dev)
{
	printf("usb-xhci: pci probe dev=%04x:%04x\n", dev->id.vendor_id, dev->id.device_id);
	found = 1;
	return xhci_probe(dev);
}

static const struct pci_device_id xhci_ids[] = {
	{ PCI_DEVICE_ID(0x1b36, 0x000d) },   /* qemu-xhci */
	{ PCI_DEVICE_ID(0x1912, 0x0014) },   /* NEC uPD720202 */
	{ PCI_ANY_DEVICE_ID },
};

static struct pci_driver xhci_drv = {
	.device_ids = xhci_ids,
	.add_dev = add,
};
PCI_REGISTER_DRIVER(&xhci_drv);

int main(void)
{
	printf("usb-xhci: guest up\n");
	printf("usb-xhci: probe hit=%d\n", found);
	return 0;
}
