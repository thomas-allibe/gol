#ifndef _GOL_H_
#define _GOL_H_

#include "layout.h"
#include "stdio.h"
#include "types.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define GOL_DEBUG
#define GOL_DEBUG_FONT_SIZE 10.0f
#define GOL_DEBUG_TITLE_FONT_SIZE 32.0f

#define GOL_FPS 60
#define GOL_CAMERA_MAX_VELOCITY 10.0f
#define GOL_GRID_COLOR LIGHTGRAY

typedef struct GolCtx {
  Rectangle screen;     // Screen bounds
  Rectangle g_screen;   // Game screen bounds
  Rectangle dbg_screen; // Debug screen bounds
  Vector2 cam_pos;      // "Camera" position

  float grid_width; // Width (and height) of grid

  double move_right_start; // Time at which user started a right key press
  double move_left_start;
  double move_up_start;
  double move_down_start;
  float velocity_right; // Camera velocity to the right
  float velocity_left;
  float velocity_up;
  float velocity_down;

  bool show_dbg; // Shoul show debug info ?
} GolCtx;

int gol_run(GolCtx *const self, int argc, char *argv[]);

int gol_init(GolCtx *const self);

void gol_draw(GolCtx *const self);
void gol_draw_grid(const GolCtx *const self);
void gol_draw_dbg(const GolCtx *const self);

void gol_watch_move_keys(GolCtx *const self);
void gol_update_position(GolCtx *const self);

// Utility
float gol_move_ease(const double cur_time, const double start_time);
void gol_print_rec(const Rectangle rec, const char *const prefix);

#endif // !_GOL_H_
