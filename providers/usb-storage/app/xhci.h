/* usb-xhci acceptance driver — xHCI host-controller bring-up and
 * enumeration on qemu-xhci. Original work written against the xHCI 1.2
 * spec register/TRB model (see ../design.md sections a-c); prior-art
 * structure read for orientation only (xhci-nostd, Apache-2.0; NONOS
 * capsule, AGPL-3.0+ — commentary only, never derived).
 *
 * SPDX-License-Identifier: BSD-3-Clause */
#ifndef USB_XHCI_H
#define USB_XHCI_H

#include <uk/essentials.h>
#include <uk/bus/pci.h>

int xhci_probe(struct pci_device *dev);

#endif
