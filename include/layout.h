#ifndef _LAYOUT_H_
#define _LAYOUT_H_

#include "types.h"
#include <assert.h>
#include <raylib.h>
#include <stdio.h>

#define LAYOUT_STACK_SIZE 1024

typedef enum ContentDisposition { DISP_HORIZ, DISP_VERT } ContentDisposition;

void layout_start(Rectangle rec, ContentDisposition disposition, u32 size);
Rectangle layout_get();
void layout_stop();
Rectangle layout_box(Rectangle rec, Color border_color, Color inner_bg_color,
                     float margin, float border, float padding);

#endif // !_LAYOUT_H_
