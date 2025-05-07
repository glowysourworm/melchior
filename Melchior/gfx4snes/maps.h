#ifndef _GFX4SNES_MAPS_H
#define _GFX4SNES_MAPS_H

#include <stdbool.h>

//-------------------------------------------------------------------------------------------------
extern unsigned short* map_convertsnes(unsigned char* imgbuf, int* nbtiles, int blksizex, int blksizey, int nbblockx, int nbblocky, int nbcolors, int offsetpal, int graphicmode, bool isnoreduction, bool isblanktile, bool is32size, bool isquiet);
extern void map_save(const char* filename, unsigned short* map, int snesmode, int nbtilex, int nbtiley, int tileoffset, int priority, bool isquiet);

#endif
