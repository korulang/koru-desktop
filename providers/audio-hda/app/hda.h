#ifndef AUDIO_HDA_H
#define AUDIO_HDA_H
#include <uk/bus/pci.h>
int hda_probe(struct pci_device *dev);
int hda_play_tone(void);
#endif
