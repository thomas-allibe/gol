#ifndef _GOL_H_
#define _GOL_H_

#include "layout.h"
#include "stdio.h"
#include "types.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GOL_DEBUG
#define GOL_DEBUG_FONT_SIZE 10.0f
#define GOL_DEBUG_TITLE_FONT_SIZE 32.0f
#define GOL_DEBUG_COLOR BLUE

#define GOL_FPS 60
#define GOL_INITIAL_SCREEN_WIDTH 1480.0
#define GOL_INITIAL_SCREEN_HEIGHT 720.0
#define GOL_INITIAL_GRID_WIDTH 50.0
#define GOL_INITIAL_CYCLE_PERIOD 1.0

#define GOL_ALIVE_CELL_SIZE_RATIO 0.9f

#define GOL_GRID_COLOR LIGHTGRAY
#define GOL_HOVER_COLOR DARKGREEN

typedef u32 Count;

typedef struct CellMap {
  Vector2 key; // Cell Corrdinates
  Count value; // Neighbour count (only when updating, else not used)
} CellMap;

typedef struct GolCtx {
  Rectangle screen;     // Screen bounds
  Rectangle g_screen;   // Game screen bounds
  Rectangle dbg_screen; // Debug screen bounds
  Vector2 cam_pos; // "Camera" position relative to the world, considered at the
                   // top left corner of g_screen

  bool draw_grid;  // Should render grid
  float cell_size; // Width (and height) of a cell

  bool mouse_on_g_screen;   // Is mouse in g_screen bounds
  Vector2 mouse_cell_coord; // Coordinates of the cell under cursor

  bool play; // If true, plays Game Of Life rules each GOL_CELL_UPDATE_PERIOD
  CellMap *alive_cells; // Array of alive cells on the grid
  bool toggle_cell;     // If the cell under cursor must be toggled (Ctrl+clic)
  double cycle_period;  // Time in seconds between two cycles
  double cycle_last_update;  // Time of the last cell lifecycle update
  double cycle_compute_time; // Time to compute a lifecycle
  u64 cycle_nb;              // Number of cycle since start

  Vector2 velocity; // Camera velocity

  bool show_dbg; // Shoul show debug info ?
} GolCtx;

int gol_run(GolCtx *const self, int argc, char *argv[]);

int gol_init(GolCtx *const self);

void gol_event(GolCtx *const self);
void gol_update(GolCtx *const self);

void gol_draw(GolCtx *const self);
void gol_draw_grid(const GolCtx *const self);
void gol_draw_cells(const GolCtx *const self);
void gol_draw_hovered_cell(const GolCtx *const self);
void gol_draw_dbg(const GolCtx *const self);

int gol_deinit(GolCtx *const self);

// Utility
float gol_move_ease(const double cur_time, const double start_time);
void gol_print_rec(const Rectangle rec, const char *const prefix);

#endif // !_GOL_H_
