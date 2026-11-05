/* usb-hid — HID-class layer over the usb-xhci enumeration path.
 * Derived from providers/usb-xhci/app (same author, BSD-3-Clause);
 * original work against the USB 2.0 + HID 1.11 + xHCI 1.0 specs.
 * SPDX-License-Identifier: BSD-3-Clause */
#ifndef USB_HID_H
#define USB_HID_H

#include <uk/bus/pci.h>

int hid_probe(struct pci_device *dev);

#endif
