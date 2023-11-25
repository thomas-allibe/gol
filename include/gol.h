#ifndef _GOL_H_
#define _GOL_H_

#include "layout.h"
#include "types.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define GOL_DEBUG

#define GOL_FPS 60
#define CAMERA_VELOCITY 1
#define GRID_COLOR LIGHTGRAY

int gol_run(int argc, char *argv[]);

void gol_draw_grid(Rectangle rec, Vector2 pos, float grid_width);

#endif // !_GOL_H_
