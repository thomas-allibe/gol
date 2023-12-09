#ifndef _GOL_H_
#define _GOL_H_

#include "error.h"
#include "fifo.h"
#include "layout.h"
#include "types.h"
#include <math.h>
#include <raylib.h>
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

typedef enum GolState {
  gol_quit,
  gol_error,
  gol_compute,
  gol_toggle_cell

} GolState;

// Use to send pointers from GolCtx members to Compute Cycle Thread (CCT)
typedef struct GolThrdArg {
  Fifo *fifo;
  Vector2 **alive_cells_render_buffer_1; // Write
  Vector2 **alive_cells_render_buffer_2; // Write
  i32 *buffer_index;                     // Write
  double *cycle_period;       // Time in seconds between two cycles. Read Only
  double *cycle_compute_time; // Time to compute a lifecycle. R/W
  u64 *cycle_nb;              // Number of cycle since start. R/W
} GolThrdArg;

typedef struct GolMsgDataToggle {
  Vector2 cell_coord;
} GolMsgDataToggle;

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

  Vector2 velocity; // Camera velocity

  thrd_t thread_comp;    // Compute Cycle Thread (CCT)
  Fifo fifo_thread_comp; // Compute Cycle Thread fifo

  // These have their adresses shared with the Compute Cycle Thread (CCT)
  //
  Vector2 *alive_cells_render_buffer_1;
  Vector2 *alive_cells_render_buffer_2; // Two cells buffers. When one is
                                        // being built, the other one is used
                                        // for rendering. Main Thread Read Only
  i32 buffer_index;         // Which buffer to render. Main Thread Read Only
  double cycle_period;      // Time in seconds between two cycles. CCT Read Only
  double cycle_last_update; // Time of the last cell lifecycle update. Main
                            // Thread Read Only
  double cycle_compute_time; // Time to compute a lifecycle. Main
                             // Thread Read Only
  u64 cycle_nb;              // Number of cycle since start. Main
                             // Thread Read Only

  bool show_dbg; // Shoul show debug info ?
} GolCtx;

int gol_run(GolCtx *const self, int argc, char *argv[]);

void gol_init(GolCtx *const self, Error *err);

void gol_event(GolCtx *const self, Error *err);
void gol_update(GolCtx *const self);

i32 gol_thread_compute_cycle(void *arg);

void gol_draw(GolCtx *const self);
void gol_draw_grid(const GolCtx *const self);
void gol_draw_cells(const GolCtx *const self);
void gol_draw_hovered_cell(const GolCtx *const self);
void gol_draw_dbg(const GolCtx *const self);

int gol_deinit(GolCtx *const self, Error *err);

// Utility
float gol_move_ease(const double cur_time, const double start_time);
void gol_print_rec(const Rectangle rec, const char *const prefix);

#endif // !_GOL_H_
