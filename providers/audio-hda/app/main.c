/* audio-hda acceptance guest. SPDX: BSD-3-Clause */
#include <stdio.h>
#include <uk/bus/pci.h>
#include "hda.h"

static int found;
static int add(struct pci_device *dev)
{
	printf("audio-hda: pci probe %04x:%04x\n", dev->id.vendor_id, dev->id.device_id);
	found = 1;
	return hda_probe(dev);
}
static const struct pci_device_id ids[] = {
	{ PCI_DEVICE_ID(0x8086, 0x2668) },	/* ich6 */
	{ PCI_DEVICE_ID(0x8086, 0x27d8) },	/* ich9? */
	{ PCI_DEVICE_ID(0x8086, 0x293e) },	/* ich9 hda */
	{ PCI_ANY_DEVICE_ID },
};
static struct pci_driver drv = { .device_ids = ids, .add_dev = add };
PCI_REGISTER_DRIVER(&drv);

int main(void)
{
	printf("audio-hda: guest up\n");
	printf("audio-hda: probe hit=%d\n", found);
	if (found >= 0) hda_play_tone();
	return 0;
}
