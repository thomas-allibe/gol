#include "layout.h"
#include <raylib.h>

static Rectangle layout_stack[LAYOUT_STACK_SIZE];
static u32 layout_stack_ptr = 0;

void layout_start(Rectangle rec, ContentDisposition disposition, u32 size) {
  assert(layout_stack_ptr + size <= LAYOUT_STACK_SIZE &&
         "CRITICAL: Layout Stack Full");

  Rectangle tmp_rec = rec;

  switch (disposition) {

  case DISP_VERT:
    // Keep same width & x
    tmp_rec.height /= (float)size;
    // Loop backwards
    for (u32 i = size; i-- > 0;) {
      tmp_rec.y = tmp_rec.height * (float)i;

      layout_stack[layout_stack_ptr++] = tmp_rec;
    }
    break;

  case DISP_HORIZ:
    // Keep same height & y
    tmp_rec.width /= (float)size;
    // Loop backwards
    for (u32 i = size; i-- > 0;) {
      tmp_rec.x = tmp_rec.width * (float)i;

      layout_stack[layout_stack_ptr++] = tmp_rec;
    }
    break;

  default:
  }
}

Rectangle layout_get() { return layout_stack[--layout_stack_ptr]; }

void layout_stop() {
  // assert(layout_stack_ptr == layout_stack_get_ptr &&
  //        "CRITICAL: Layout Stack is empty, layout_start() and layout_end() "
  //        "don't match!");
  // printf("%u\n", layout_stack_ptr);
}

// #TODO: limits
// #TODO: worth checking if margin & padding > 0 to skip some things ?
Rectangle layout_box(Rectangle rec, Color border_color, Color inner_bg_color,
                     float margin, float border, float padding) {
  if (margin > 0) {
    rec.x = rec.x + margin;
    rec.y = rec.y + margin;
    rec.width = rec.width - 2 * margin;
    rec.height = rec.height - 2 * margin;
  }

  if (border > 0)
    DrawRectangleLinesEx(rec, border, border_color);

  if (inner_bg_color.r || inner_bg_color.g || inner_bg_color.b)
    DrawRectangle((int)(rec.x + border), (int)(rec.y + border),
                  (int)(rec.width - 2 * border), (int)(rec.height - 2 * border),
                  inner_bg_color);

  rec.x = rec.x + padding + border;
  rec.y = rec.y + padding + border;
  rec.width = rec.width - 2 * (padding + border);
  rec.height = rec.height - 2 * (padding + border);

  return rec;
}
