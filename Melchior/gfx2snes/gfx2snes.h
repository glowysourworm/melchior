#ifndef GFX2SNES_H
#define GFX2SNES_H

#define GFX2SNESVERSION "Developer"
#define GFX2SNESDATE __DATE__

#define HI_BYTE(n) (((int)n >> 8) & 0x00ff) // extracts the hi-byte of a word
#define LOW_BYTE(n) ((int)n & 0x00ff)       // extracts the low-byte of a word
#define HIGH(n) ((int)n << 8)               // turn the char into high part of int

#define MAXTILES 1024

extern int quietmode;      // 0 = not quiet, 1 = i can't say anything :P
extern int hi512;          // 1 = create a 512 width map for mode 5 & 6
extern int tile_reduction; // 0 = no tile reduction (warning !)
extern int colortabinc;    // 16 for 16 color mode, 4 for 4 color mode
extern int pagemap32;      // 1 = create tile maps organized in 32x32 pages
extern int blanktile;      // 1 = blank tile generated
extern int palette_rnd;    // 1 = round palette up & down

#endif
