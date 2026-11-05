/* usb-storage acceptance guest: xhci -> BOT -> INQUIRY/READ/WRITE.
 * Pattern planted by the host on the usb-storage image; serial is the
 * evidence. SPDX: BSD-3-Clause */
#include <stdio.h>
#include <string.h>
#include <uk/bus/pci.h>
#include <uk/alloc.h>
#include "xhci.h"
#include "msc.h"

static int add(struct pci_device *dev)
{
	printf("usb-storage: pci xhci %04x:%04x\n", dev->id.vendor_id, dev->id.device_id);
	return xhci_probe(dev);
}
static const struct pci_device_id ids[] = {
	{ PCI_DEVICE_ID(0x1b36, 0x000d) }, { PCI_ANY_DEVICE_ID },
};
static struct pci_driver drv = { .device_ids = ids, .add_dev = add };
PCI_REGISTER_DRIVER(&drv);

int main(void)
{
	printf("usb-storage: guest up\n");
	if (usb_msc_setup() < 0) { printf("usb-storage: no msc device\n"); return 1; }
	if (msc_inquiry() < 0) return 2;
	uint8_t *sec = uk_malloc(uk_alloc_get_default(), 512);
	if (msc_read(8, sec) < 0) { printf("usb-storage: read fail\n"); return 3; }
	printf("usb-storage: sector8 pattern='%.32s'\n", sec);
	uint8_t *w = uk_malloc(uk_alloc_get_default(), 512);
	memset(w, 0, 512);
	memcpy(w, "USBS-GUEST-WROTE-THIS", 21);
	if (msc_write(9, w) < 0) { printf("usb-storage: write fail\n"); return 4; }
	if (msc_read(9, sec) < 0) return 5;
	printf("usb-storage: sector9 pattern='%.32s'\n", sec);
	printf("usb-storage: done\n");
	return 0;
}
