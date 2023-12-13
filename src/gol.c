#include "gol.h"
#include <raylib.h>
#include <stdlib.h>
#include <threads.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#define FIFO_IMPLEMENTATION
#include "fifo.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#pragma GCC diagnostic pop

i32 gol_run(GolCtx *const self, i32 argc, char *argv[]) {
  (void)argc;
  (void)argv;
  Error err = {0};

  gol_init(self, &err);

#ifdef GOL_DEBUG
  SetTraceLogLevel(LOG_ALL);
#endif /* ifdef GOL_DEBUG */

  // Main game loop
  while (!WindowShouldClose() && !err.status) {

    gol_event(self, &err);

    gol_update(self);

    BeginDrawing();

    gol_draw(self, &err);

    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context

  gol_deinit(self, &err);

  if (err.status) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

void gol_init(GolCtx *const self, Error *const err) {

  *self = (GolCtx){0};

  self->screen = (Rectangle){.width = GOL_INITIAL_SCREEN_WIDTH,
                             .height = GOL_INITIAL_SCREEN_HEIGHT,
                             .x = 0.0f,
                             .y = 0.0f};
  self->cell_size = GOL_INITIAL_GRID_WIDTH;
  self->cycle_period = GOL_INITIAL_CYCLE_PERIOD, self->draw_grid = true;

  // Init arrays so it isn't NULL
  arrsetcap(self->alive_cells_render_buffer_1, 50);
  arrsetcap(self->alive_cells_render_buffer_2, 50);

  fifo_create(&self->cct_fifo, err);
  if (err->status) {
    TraceLog(LOG_FATAL, "Could not create fifo:\n\t%s", err->msg);
    return;
  }

  if (mtx_init(&self->buffer_index_mtx, mtx_timed) == thrd_error) {
    err->msg = "Could not initialize Mutex (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    TraceLog(LOG_FATAL, "%s", err->msg);
    return;
  }

  // Freed by Thread
  GolCctArgs *cct_args = malloc(sizeof(GolCctArgs));
  *cct_args = (GolCctArgs){
      .fifo = &self->cct_fifo,
      .cycle_period = &self->cycle_period,
      .cycle_nb = &self->cycle_nb,
      .cycle_compute_time = &self->cycle_compute_time,

      .buffer_index = &self->buffer_index,
      .buffer_index_mtx = &self->buffer_index_mtx,
      .alive_cells_render_buffer_1 = &self->alive_cells_render_buffer_1,
      .alive_cells_render_buffer_2 = &self->alive_cells_render_buffer_2};

  if (thrd_create(&self->cct, &gol_cct, cct_args) != thrd_success) {
    free(cct_args);
    err->msg = "Could not create thread (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    TraceLog(LOG_FATAL, "%s", err->msg);
    return;
  }

  // srand((u32)time(NULL));
  // for (u32 i = 0; i < 100; i++) {
  //   for (u32 j = 0; j < 100; j++) {
  //     if ((rand() * 2.0 / RAND_MAX) > 0.5) {
  //       const Vector2 cell = {
  //           .x = floorf(((float)rand() * 100.0f) / (float)RAND_MAX),
  //           .y = floorf(((float)rand() * 100.0f) / (float)RAND_MAX)};
  //       hmput(self->alive_cells, cell, 0);
  //     }
  //   }
  // }
  // for (u32 i = 0; i < 1000; i++) {
  //   for (u32 j = 0; j < 1000; j++) {
  //     const Vector2 cell = {.x = (float)i, .y = (float)j};
  //     hmput(self->alive_cells, cell, 0);
  //   }
  // }

#ifdef GOL_DEBUG
  self->show_dbg = true;
#endif /* ifdef GOL_DEBUG */

  InitWindow((i32)self->screen.width, (i32)self->screen.height, "Game Of Life");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(GOL_FPS); // Set our game to run at 60 frames-per-second

  return;
}

void gol_event(GolCtx *const self, Error *const err) {
  // const double cur_time = GetTime();
  const Vector2 mouse_pos = GetMousePosition();
  const Vector2 mouse_delta = GetMouseDelta();
  const Vector2 mouse_wheel = GetMouseWheelMoveV();

  if (IsWindowResized()) {
    self->screen.width = (float)GetRenderWidth();
    self->screen.height = (float)GetRenderHeight();
  }

  // Enable Debug View
  //
#ifdef GOL_DEBUG
  if (IsKeyPressed(KEY_D)) {
    self->show_dbg = !self->show_dbg;
  }
#endif /* ifdef GOL_DEBUG */

  if (IsKeyPressed(KEY_C)) {
    self->cam_pos.x = 0.0f;
    self->cam_pos.y = 0.0f;
  }

  if (IsKeyPressed(KEY_G)) {
    self->draw_grid = !self->draw_grid;
  }

  if (IsKeyPressed(KEY_SPACE)) {
    self->play = !self->play;
  }

  // Mouse on g_screen
  //
  self->mouse_on_g_screen = CheckCollisionPointRec(mouse_pos, self->g_screen);
  if (self->mouse_on_g_screen) {
    // Compute mouse grid pos coordinate
    //
    self->mouse_cell_coord = (Vector2){
        .x = floorf((mouse_pos.x - self->g_screen.x + self->cam_pos.x) /
                    self->cell_size),
        .y = floorf((mouse_pos.y - self->g_screen.y + self->cam_pos.y) /
                    self->cell_size)};

    if (IsKeyDown(KEY_LEFT_CONTROL)) {
      // Mouse Left + Ctrl: toggle cell
      //

      // self->toggle_cell = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        // Malloc must be freed in the thread enqueue succeeded!
        FifoMsg msg = {.state = gol_cct_toggle_cell,
                       .data = malloc(sizeof(GolMsgDataToggle))};
        assert(msg.data && "Not enough memory, this is the end...");

        ((GolMsgDataToggle *)msg.data)->cell_coord = self->mouse_cell_coord;
        fifo_enqueue_msg(&self->cct_fifo, msg, -1, err);

        if (err->status) {
          TraceLog(LOG_FATAL, "Could not message thread...\n\t%s", err->msg);
          return;
        }
      }
    } else {
      // Mouse grid dragging
      //
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        self->velocity.x = -1.0f * mouse_delta.x;
        self->velocity.y = -1.0f * mouse_delta.y;
      } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        self->velocity.x = 0.0f;
        self->velocity.y = 0.0f;
      }
    }
  }

  // Mouse wheel gri_width change
  //
  self->cell_size += mouse_wheel.y;

  // Keyboard Up Key
  //
  if (IsKeyPressed(KEY_UP)) {
    self->cycle_period /= 1.25;
  }

  // Keyboard Down Key
  //
  if (IsKeyPressed(KEY_DOWN)) {
    self->cycle_period *= 1.25;
  }
}

void gol_update(GolCtx *const self) {
  // Update cam position
  //
  self->cam_pos.x += self->velocity.x;
  self->cam_pos.y += self->velocity.y;

  const double time_start = GetTime();

  // #TODO: Remove
  if (self->play &&
      ((time_start - self->cycle_last_update) > self->cycle_period)) {
  }
}

i32 gol_cct(void *arg) {
  GolCctArgs *args = (GolCctArgs *)arg;

  GolCellMap *alive_cells = NULL;

  FifoMsg msg = {.state = gol_cct_compute};
  Error err = {0};
  double cycle_last_update = 0.0;

  while (msg.state != gol_cct_quit && !err.status) {
    msg = fifo_dequeue_msg(args->fifo, -1, &err);
    if (err.status) {
      msg.state = gol_cct_error;
    }

    switch (msg.state) {
    case gol_cct_quit: {
      // Do nothing
    } break;

    case gol_cct_error: {
      assert(0 && "Oh oh something gone wrong!");
    } break;

    case gol_cct_toggle_cell: {

      GolMsgDataToggle *msg_data = (GolMsgDataToggle *)msg.data;

      TraceLog(LOG_DEBUG, "Pos: %f, %f", msg_data->cell_coord.x,
               msg_data->cell_coord.y);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-conversion"
      const bool found = hmdel(alive_cells, msg_data->cell_coord);
#pragma GCC diagnostic pop
      if (!found) {
        hmput(alive_cells, msg_data->cell_coord, 0);
      }

      free(msg_data);
    } break;

    case gol_cct_compute: {

      const double time_start = GetTime();

      // Isolate the buffer to change. At this point, buffer_index still point
      // to the buffer used for render. It's why index 0 returns buffer n°2, not
      // n°1
      Vector2 **alive_cells_render_buffer =
          *args->buffer_index ? args->alive_cells_render_buffer_2
                              : args->alive_cells_render_buffer_1;

      // Iterate over alive cells to build a neighbour map of the board
      //
      GolCellMap *neighbour = NULL;

      for (u32 i = 0; i < hmlen(alive_cells); i++) {
        // Search cell 8 neighbour
        for (float x = alive_cells[i].key.x - 1.0f;
             x <= alive_cells[i].key.x + 1.0f; x++) {
          for (float y = alive_cells[i].key.y - 1.0f;
               y <= alive_cells[i].key.y + 1.0f; y++) {

            const Vector2 adj_cell = {.x = x, .y = y};
            const ptrdiff_t index = hmgeti(neighbour, adj_cell);

            if (index == -1) {
              // Doesn't exists
              if (alive_cells[i].key.x == x && alive_cells[i].key.y == y) {
                // Current cell
                hmput(neighbour, adj_cell, 0);
              } else {
                // Not current cell
                hmput(neighbour, adj_cell, 1);
              }
            } else {
              // Exists
              if (!(alive_cells[i].key.x == x && alive_cells[i].key.y == y)) {
                // Not Current cell
                neighbour[index].value += 1;
              }
            }
          }
        }
      }

      // Iterate over the neighbour map and add or remove alive cells depending
      // on count
      for (u32 i = 0; i < hmlen(neighbour); i++) {
        if (neighbour[i].value < 2 || neighbour[i].value > 3) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-conversion"
          hmdel(alive_cells, neighbour[i].key);
#pragma GCC diagnostic pop
        } else if (neighbour[i].value == 3) {
          hmput(alive_cells, neighbour[i].key, 0);
        }
      }

      hmfree(neighbour);

      // Clear the buffer (should be fast since it sets length to 0 and memmove
      // 0 bytes)
      arrdeln(*alive_cells_render_buffer, 0,
              arrlenu(*alive_cells_render_buffer));

      // Iterate over alive cells to build alive_cells_render_buffer
      //
      for (u32 i = 0; i < hmlen(alive_cells); i++) {
        arrput(*alive_cells_render_buffer, alive_cells[i].key);
      }

      if (mtx_lock(args->buffer_index_mtx) != thrd_success) {
        err.msg = "Could not lock Mutex (" error_print_err_location ").";
        err.status = true;
        err.code = error_generic;
        TraceLog(LOG_FATAL, "%s", err.msg);
      }
      // Toggle index 0<->1
      *args->buffer_index ^= 1;

      if (mtx_unlock(args->buffer_index_mtx) != thrd_success) {
        err.msg = "Could not unlock Mutex (" error_print_err_location ").";
        err.status = true;
        err.code = error_generic;
        TraceLog(LOG_FATAL, "%s", err.msg);
      }

      *args->cycle_nb += 1;
      cycle_last_update = GetTime();
      *args->cycle_compute_time = cycle_last_update - time_start;

    } break;

    default:
      assert(0 && "Don't go here");
    }
  }

  if (alive_cells) {
    free(alive_cells);
  }
  free(args);

  return err.status;
}

void gol_draw(GolCtx *const self, Error *const err) {

  ClearBackground(RAYWHITE);

#ifdef GOL_DEBUG
  if (self->show_dbg) {

    layout_start(self->screen, DISP_HORIZ, 2);
    self->g_screen = layout_box(layout_get(), GRAY, BLANK, 10.0f, 5.0f, 0.0f);
    self->dbg_screen =
        layout_box(layout_get(), GOL_DEBUG_COLOR, BLANK, 10.0f, 5.0f, 5.0f);

    if (self->draw_grid) {
      gol_draw_grid(self);
    }
    gol_draw_cells(self, err);
    gol_draw_hovered_cell(self);
    gol_draw_dbg(self);

  } else
#endif /* ifdef GOL_DEBUG */
  {
    self->g_screen = layout_box(self->screen, GRAY, BLANK, 10.0f, 5.0f, 0.0f);
    if (self->draw_grid) {
      gol_draw_grid(self);
    }
    gol_draw_cells(self, err);
    gol_draw_hovered_cell(self);
  }
}

void gol_draw_grid(const GolCtx *const self) {

  Vector2 start_pos, end_pos;

  i32 i = 0; // For debug

  // Compute interval between cam_pos and the closest lines from the left/top
  const Vector2 grid_frac = {.x = -1.0f * self->cam_pos.x / self->cell_size,
                             .y = -1.0f * self->cam_pos.y / self->cell_size};
  const Vector2 offset = {
      .x = (grid_frac.x - floorf(grid_frac.x)) * self->cell_size,
      .y = (grid_frac.y - floorf(grid_frac.y)) * self->cell_size};

  // Draw Vertical Lines
  //
  // Start by drawing the first leftmost vertical line
  i = 0; // For debug
  for (float x = self->g_screen.x + offset.x;
       x <= self->g_screen.x + self->g_screen.width; x += self->cell_size) {
    start_pos.x = x;
    start_pos.y = self->g_screen.y;
    end_pos.x = x;
    end_pos.y = self->g_screen.y + self->g_screen.height;

    DrawLineV(start_pos, end_pos, GOL_GRID_COLOR);

#ifdef GOL_DEBUG
    if (self->show_dbg) {
      // x coord on each line
      DrawTextPro(GetFontDefault(),
                  TextFormat("x: %d", (i32)(truncf(-1.0f * grid_frac.x)) + i),
                  (Vector2){.x = x, .y = self->g_screen.y + 5},
                  (Vector2){.x = 0.0f, .y = 10.0f}, 90.0f, 10.0f, 2.0f, PURPLE);
      i += 1; // Increment
    }
#endif /* ifdef GOL_DEBUG                                                      \
        */
  }

  // Draw Horizontal Lines
  //
  // Start by drawing the first topmost horizontal line
  i = 0; // For debug
  for (float y = self->g_screen.y + offset.y;
       y <= self->g_screen.y + self->g_screen.height; y += self->cell_size) {
    start_pos.x = self->g_screen.x;
    start_pos.y = y;
    end_pos.x = self->g_screen.x + self->g_screen.width;
    end_pos.y = y;

    DrawLineV(start_pos, end_pos, GOL_GRID_COLOR);

#ifdef GOL_DEBUG
    if (self->show_dbg) {
      // x coord on each line
      DrawTextPro(GetFontDefault(),
                  TextFormat("y: %d", (i32)(truncf(-1.0f * grid_frac.y)) + i),
                  (Vector2){.x = self->g_screen.x + 5, .y = y}, (Vector2){0},
                  0.0f, 10.0f, 2.0f, PURPLE);
      i += 1; // Increment
    }
#endif /* ifdef GOL_DEBUG */
  }
}

void gol_draw_cells(GolCtx *const self, Error *const err) {
  const Rectangle cam_window = {.x = self->cam_pos.x,
                                .y = self->cam_pos.y,
                                .width = self->g_screen.width,
                                .height = self->g_screen.height};
  const float cell_size_offset =
      self->cell_size * (1.0f - GOL_ALIVE_CELL_SIZE_RATIO);
  const float cell_pos_offset = cell_size_offset / 2;

  if (mtx_lock(&self->buffer_index_mtx) != thrd_success) {
    err->msg = "Could not lock Mutex (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    TraceLog(LOG_FATAL, "%s", err->msg);
    return;
  }

  const Vector2 *const alive_cells_render_buffer =
      self->buffer_index ? self->alive_cells_render_buffer_1
                         : self->alive_cells_render_buffer_2;

  for (u32 i = 0; i < arrlen(alive_cells_render_buffer); i++) {
    const Rectangle cell_rec = {
        .x = alive_cells_render_buffer[i].x * self->cell_size,
        .y = alive_cells_render_buffer[i].y * self->cell_size,
        .width = self->cell_size,
        .height = self->cell_size};
    if (CheckCollisionRecs(cell_rec, cam_window)) {
      Rectangle cell_to_draw = GetCollisionRec(cell_rec, cam_window);

      cell_to_draw.x =
          cell_to_draw.x - self->cam_pos.x + self->g_screen.x + cell_pos_offset;
      cell_to_draw.y =
          cell_to_draw.y - self->cam_pos.y + self->g_screen.y + cell_pos_offset;
      cell_to_draw.width -= cell_size_offset;
      cell_to_draw.height -= cell_size_offset;

      DrawRectangleRec(cell_to_draw, BLACK);
    }
  }

  if (mtx_unlock(&self->buffer_index_mtx) != thrd_success) {
    err->msg = "Could not unlock Mutex (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    TraceLog(LOG_FATAL, "%s", err->msg);
    return;
  }
}

void gol_draw_hovered_cell(const GolCtx *const self) {
  if (self->mouse_on_g_screen) {
    const Rectangle cam_window = {.x = self->cam_pos.x,
                                  .y = self->cam_pos.y,
                                  .width = self->g_screen.width,
                                  .height = self->g_screen.height};
    const Rectangle cell_rec = {.x = self->mouse_cell_coord.x * self->cell_size,
                                .y = self->mouse_cell_coord.y * self->cell_size,
                                .width = self->cell_size,
                                .height = self->cell_size};
    if (CheckCollisionRecs(cell_rec, cam_window)) {
      Rectangle cell_to_draw = GetCollisionRec(cell_rec, cam_window);

      cell_to_draw.x = cell_to_draw.x - self->cam_pos.x + self->g_screen.x;
      cell_to_draw.y = cell_to_draw.y - self->cam_pos.y + self->g_screen.y;

      DrawRectangleLinesEx(cell_to_draw, 5.0f, GOL_HOVER_COLOR);
    }
  }
}

void gol_draw_dbg(const GolCtx *const self) {
  const Vector2 mouse_pos = GetMousePosition();
  const Vector2 mouse_pos_rel_g = {
      .x = mouse_pos.x - self->g_screen.x,
      .y = mouse_pos.y -
           self->g_screen.y}; // Mouse position relative to g_screen

  // Draw Point at Origin
  //
  const Vector2 origin_on_screen = {.x = self->g_screen.x - self->cam_pos.x,
                                    .y = self->g_screen.y - self->cam_pos.y};
  if (CheckCollisionPointRec(origin_on_screen, self->g_screen)) {
    DrawCircle((i32)origin_on_screen.x, (i32)origin_on_screen.y, 15.0f,
               GOL_DEBUG_COLOR);
  }

  // Draw mouse coordinates relative to screen
  //
  DrawText(TextFormat("s(%d, %d)", (i32)mouse_pos.x, (i32)mouse_pos.y),
           (i32)mouse_pos.x + 20, (i32)mouse_pos.y, GOL_DEBUG_FONT_SIZE,
           GOL_DEBUG_COLOR);

  // Draw mouse coordinates relative to g_screen
  //
  if (self->mouse_on_g_screen) {
    DrawText(TextFormat("gs(%d, %d)\ng(%d, %d)", (i32)mouse_pos_rel_g.x,
                        (i32)mouse_pos_rel_g.y, (i32)self->mouse_cell_coord.x,
                        (i32)self->mouse_cell_coord.y),
             (i32)mouse_pos.x + 20,
             (i32)(mouse_pos.y + GOL_DEBUG_FONT_SIZE + 5.0f),
             GOL_DEBUG_FONT_SIZE, GOL_DEBUG_COLOR);
  }

  // Draw Debug Panel
  //
  //
  // Draw FPS top left debug screen
  //
  DrawFPS((i32)self->dbg_screen.x, (i32)self->dbg_screen.y);

  layout_start(self->dbg_screen, DISP_VERT, 4);

  const Rectangle title_rec = layout_get();
  const char *const title = "Debug Info:";
  const float title_x_offset =
      (title_rec.width - (float)MeasureText(title, GOL_DEBUG_TITLE_FONT_SIZE)) /
      2.0f;
  DrawText(title, (i32)(title_rec.x + title_x_offset), (i32)title_rec.y,
           GOL_DEBUG_TITLE_FONT_SIZE, GOL_DEBUG_COLOR);

  const Rectangle screens_rec = layout_get();
  DrawText(
      TextFormat(
          "Screen Rectangles:\n\tApp: {w: %f, h: %f}\n\tGame: {x: %f, y: %f, "
          "w: %f, h: %f}\n\tDebug: {x: %f, y: %f, w: %f, h: %f}",
          self->screen.width, self->screen.height, self->g_screen.x,
          self->g_screen.y, self->g_screen.width, self->g_screen.height,
          self->dbg_screen.x, self->dbg_screen.y, self->dbg_screen.width,
          self->dbg_screen.height),
      (i32)screens_rec.x, (i32)screens_rec.y, GOL_DEBUG_FONT_SIZE,
      GOL_DEBUG_COLOR);

  layout_start(layout_get(), DISP_HORIZ, 2);

  const Rectangle velocity_rec = layout_get();
  DrawText(TextFormat("Camera Velocity (px/s): \n\tx: %f\n\ty: %f",
                      self->velocity.x, self->velocity.y),
           (i32)velocity_rec.x, (i32)velocity_rec.y, GOL_DEBUG_FONT_SIZE,
           GOL_DEBUG_COLOR);

  const Rectangle cam_coord_rec = layout_get();
  DrawText(TextFormat("Camera: \n\tx: %f\n\ty: %f", self->cam_pos.x,
                      self->cam_pos.y),
           (i32)cam_coord_rec.x, (i32)cam_coord_rec.y, GOL_DEBUG_FONT_SIZE,
           GOL_DEBUG_COLOR);

  const Rectangle cell_nb_rec = layout_get();
  DrawText(TextFormat("Cycle: %lu, Number of cells: %d, Compute time: %lf ms",
                      self->cycle_nb, hmlen(self->alive_cells),
                      self->cycle_compute_time * 1000.0),
           (i32)cell_nb_rec.x, (i32)cell_nb_rec.y, GOL_DEBUG_FONT_SIZE,
           GOL_DEBUG_COLOR);
}

int gol_deinit(GolCtx *const self, Error *const err) {
  if (self->alive_cells) {
    hmfree(self->alive_cells);
  }

  FifoMsg msg = {.state = gol_cct_quit};

  fifo_enqueue_msg(&self->cct_fifo, msg, -1, err);
  if (err->status) {
    TraceLog(LOG_FATAL, "Could not message thread...\n\t%s", err->msg);
  }

  i32 result;
  if (thrd_join(self->cct, &result)) {
    err->status = true;
    // #TODO: Better error
    TraceLog(LOG_FATAL, "Error joining thread...", err->msg);
  }

  mtx_destroy(&self->buffer_index_mtx);

  fifo_destroy(&self->cct_fifo, err);

  if (self->alive_cells_render_buffer_1) {
    arrfree(self->alive_cells_render_buffer_1);
  }

  if (self->alive_cells_render_buffer_2) {
    arrfree(self->alive_cells_render_buffer_2);
  }

  return err->status ? EXIT_FAILURE : EXIT_SUCCESS;
}

float gol_move_ease(const double cur_time, const double start_time) {
  const double delta = cur_time - start_time;

  return (float)(1.0 - exp(-2.0 * delta));
}
void gol_print_rec(const Rectangle rec, const char *const prefix) {
  TraceLog(LOG_DEBUG, "%s {x: %f, y: %f, w: %f, h: %f}\n", prefix, rec.x, rec.y,
           rec.width, rec.height);
}
