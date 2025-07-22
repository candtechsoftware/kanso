#ifndef FONT_H
#define FONT_H

#include "base/base.h"
#include <stb_truetype.h>

struct Font_Handle
{
    u64 data[2]; 
};


struct Font_State
{
    Arena* arena;
};

void
font_init(void);

Font_Handle 
font_open(String path);




// TODO(Alex) we want to no use the stbtt types here and have this 
// be under an specific impl when we want to use other providers or 
// when we want to have our own. 
stbtt_fontinfo
font_from_handle(Font_Handle handle); 

Font_Handle 
font_to_handle(stbtt_fontinfo handle); 

#endif
