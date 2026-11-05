#ifndef USB_MSC_H
#define USB_MSC_H
#include <stdint.h>
int usb_msc_setup(void);
int usb_bulk_in(void *b, int n);
int usb_bulk_out(void *b, int n);
int msc_inquiry(void);
int msc_read(uint32_t lba, void *buf);
int msc_write(uint32_t lba, const void *buf);
#endif
