#ifndef NEXT_LEVEL_HUD_H
#define NEXT_LEVEL_HUD_H
#include <stdint.h>
/* Align with native left HUD, including its widescreen offset. */
typedef struct { int x, y, size; uint32_t color; const unsigned char *text; } NextLevelText;
static void next_level_layout(NextLevelText out[4], const unsigned char *label,
                              const unsigned char *number, float opacity, int left_offset) {
    if (!(opacity > 0)) opacity=0;
    if (opacity>1) opacity=1;
    uint32_t alpha=(uint32_t)(opacity*128.0f);
    out[0]=(NextLevelText){17+left_offset,67,12,(alpha*3/4)<<24,label};
    out[1]=(NextLevelText){16+left_offset,66,12,0x00FF9CBE|(alpha<<24),label};
    out[2]=(NextLevelText){17+left_offset,83,18,(alpha*3/4)<<24,number};
    out[3]=(NextLevelText){16+left_offset,82,18,0x00FFD6E6|(alpha<<24),number};
}
#endif
