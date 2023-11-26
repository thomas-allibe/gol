#include "gol.h"
#include "layout.h"
#include <raylib.h>

int gol_run(GolCtx *const self, int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Initialization
  //--------------------------------------------------------------------------------------

  gol_init(self);

  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // Update
    //----------------------------------------------------------------------------------
    if (IsWindowResized()) {
      self->screen.width = (float)GetRenderWidth();
      self->screen.height = (float)GetRenderHeight();
    }

#ifdef GOL_DEBUG
    if (IsKeyPressed(KEY_D))
      self->show_dbg = !self->show_dbg;
#endif /* ifdef GOL_DEBUG */

    gol_watch_move_keys(self);

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

    gol_draw(self);

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}

int gol_init(GolCtx *const self) {
  self->screen =
      (Rectangle){.width = 1480.0f, .height = 720.0f, .x = 0.0f, .y = 0.0f};
  self->cam_pos = (Vector2){0};
  self->grid_width = 50.0f;

#ifdef GOL_DEBUG
  self->show_dbg = true;
#endif /* ifdef GOL_DEBUG */

  InitWindow((int)self->screen.width, (int)self->screen.height, "Game Of Life");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(GOL_FPS); // Set our game to run at 60 frames-per-second

  return 0;
}

void gol_draw(GolCtx *const self) {

  ClearBackground(RAYWHITE);

#ifdef GOL_DEBUG
  if (self->show_dbg) {

    layout_start(self->screen, DISP_HORIZ, 2);
    self->g_screen = layout_box(layout_get(), GRAY, BLANK, 10.0f, 5.0f, 0.0f);
    self->dbg_screen = layout_box(layout_get(), RED, BLANK, 10.0f, 5.0f, 5.0f);

    gol_draw_grid(self);
    gol_draw_dbg(self);
  } else
#endif /* ifdef GOL_DEBUG */
  {
    self->g_screen = layout_box(self->screen, GRAY, BLANK, 10.0f, 5.0f, 0.0f);
    gol_draw_grid(self);
  }
}

void gol_draw_grid(const GolCtx *const self) {

#ifdef GOL_DEBUG
  char str_coord[15];
#endif /* ifdef GOL_DEBUG */

  Vector2 start_pos, end_pos;

  // Compute interval between cam_pos and the closest lines from the left/top
  const Vector2 quotient = {.x = self->cam_pos.x / self->grid_width,
                            .y = self->cam_pos.y / self->grid_width};
  const Vector2 delta = {
      .x = (quotient.x - floorf(quotient.x)) * self->grid_width,
      .y = (quotient.y - floorf(quotient.y)) * self->grid_width};

  // Draw Vertical Lines
  //
  for (float i = self->g_screen.x + delta.x;
       i <= self->g_screen.x + self->g_screen.width; i += self->grid_width) {
    start_pos.x = i;
    start_pos.y = self->g_screen.y;
    end_pos.x = start_pos.x;
    end_pos.y = self->g_screen.y + self->g_screen.height;

    DrawLineV(start_pos, end_pos, GOL_GRID_COLOR);

#ifdef GOL_DEBUG
    if (self->show_dbg) {
      // Moving dot, starting in the center of the screen
      DrawCircle((int)(self->cam_pos.x + self->g_screen.x +
                       self->g_screen.width / 2.0f),
                 (int)(self->cam_pos.y + self->g_screen.y +
                       self->g_screen.height / 2.0f),
                 5, RED);

      // x coord on each line
      sprintf(str_coord, "x: %d", (int)start_pos.x);
      DrawTextPro(GetFontDefault(), str_coord,
                  (Vector2){.x = start_pos.x, .y = self->g_screen.y + 5},
                  (Vector2){.x = 0.0f, .y = 10.0f}, 90.0f, 10.0f, 2.0f, PURPLE);
    }
#endif /* ifdef GOL_DEBUG                                                      \
        */
  }

  // Draw Horizontal Lines
  //
  for (float i = self->g_screen.y + delta.y;
       i <= self->g_screen.y + self->g_screen.height; i += self->grid_width) {
    start_pos.x = self->g_screen.x;
    start_pos.y = i;
    end_pos.x = self->g_screen.x + self->g_screen.width;
    end_pos.y = start_pos.y;

    DrawLineV(start_pos, end_pos, GOL_GRID_COLOR);

#ifdef GOL_DEBUG
    if (self->show_dbg) {
      // x coord on each line
      sprintf(str_coord, "y: %d", (int)start_pos.y);
      DrawTextPro(GetFontDefault(), str_coord,
                  (Vector2){.x = self->g_screen.x + 5, .y = start_pos.y},
                  (Vector2){0}, 0.0f, 10.0f, 2.0f, PURPLE);
    }
#endif /* ifdef GOL_DEBUG */
  }

  // Draw origin
#ifdef GOL_DEBUG
  if (self->show_dbg) {
    // if (self->cam_pos.x)
    DrawCircle(
        (int)(self->cam_pos.x + self->g_screen.x + self->g_screen.width / 2),
        (int)(self->cam_pos.y + self->g_screen.y + self->g_screen.height / 2),
        5, RED);
  }
#endif /* ifdef GOL_DEBUG */
}

void gol_draw_dbg(const GolCtx *const self) {
  gol_print_rec(self->dbg_screen, "title:");
  layout_start(self->dbg_screen, DISP_VERT, 2);

  const Rectangle title_rec = layout_get();
  const char *const title = "Debug Info:";
  const float title_x_offset =
      (title_rec.width - (float)MeasureText(title, GOL_DEBUG_TITLE_FONT_SIZE)) /
      2.0f;
  DrawText(title, (int)(title_rec.x + title_x_offset), (int)title_rec.y,
           GOL_DEBUG_TITLE_FONT_SIZE, RED);

  layout_start(layout_get(), DISP_HORIZ, 2);

  const Rectangle fps_rec = layout_get();
  DrawFPS((int)fps_rec.x, (int)fps_rec.y);

  const Rectangle velocity_rec = layout_get();
  char velocity_str[160];
  sprintf(velocity_str,
          "Velocity (px/s): \n\tright: %f\n\tleft: %f\n\tup: %f\n\tdown: %f",
          self->velocity_right, self->velocity_left, self->velocity_down,
          self->velocity_up);
  DrawText(velocity_str, (int)velocity_rec.x, (int)velocity_rec.y,
           GOL_DEBUG_FONT_SIZE, RED);
}

void gol_watch_move_keys(GolCtx *const self) {
  const double cur_time = GetTime();

  // #TODO: not quite ok, need delta
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
      CheckCollisionPointRec(GetMousePosition(), self->g_screen))
    self->cam_pos = GetMousePosition();

  if (IsKeyDown(KEY_RIGHT)) {
    if (IsKeyPressed(KEY_RIGHT)) {
      self->move_right_start = cur_time;
    }
    self->velocity_right = GOL_CAMERA_MAX_VELOCITY *
                           gol_move_ease(cur_time, self->move_right_start);

    self->cam_pos.x -= self->velocity_right;
  } else if (IsKeyReleased(KEY_RIGHT)) {
    self->move_right_start = 0.0;
    self->velocity_right = 0.0f;
  }

  if (IsKeyDown(KEY_LEFT)) {
    if (IsKeyPressed(KEY_LEFT)) {
      self->move_left_start = cur_time;
    }
    self->velocity_left = GOL_CAMERA_MAX_VELOCITY *
                          gol_move_ease(cur_time, self->move_left_start);

    self->cam_pos.x += self->velocity_left;
  } else if (IsKeyReleased(KEY_LEFT)) {
    self->move_left_start = 0.0;
    self->velocity_left = 0.0f;
  }

  if (IsKeyDown(KEY_UP)) {
    if (IsKeyPressed(KEY_UP)) {
      self->move_up_start = cur_time;
    }
    self->velocity_up =
        GOL_CAMERA_MAX_VELOCITY * gol_move_ease(cur_time, self->move_up_start);

    self->cam_pos.y += self->velocity_up;
  } else if (IsKeyReleased(KEY_UP)) {
    self->move_up_start = 0.0;
    self->velocity_up = 0.0f;
  }

  if (IsKeyDown(KEY_DOWN)) {
    if (IsKeyPressed(KEY_DOWN)) {
      self->move_down_start = cur_time;
    }
    self->velocity_down = GOL_CAMERA_MAX_VELOCITY *
                          gol_move_ease(cur_time, self->move_down_start);

    self->cam_pos.y -= self->velocity_down;
  } else if (IsKeyReleased(KEY_DOWN)) {
    self->move_down_start = 0.0;
    self->velocity_down = 0.0f;
  }
}

float gol_move_ease(const double cur_time, const double start_time) {
  const double delta = cur_time - start_time;

  return (float)(1.0 - exp(-2.0 * delta));
}
void gol_print_rec(const Rectangle rec, const char *const prefix) {
  printf("%s {x: %f, y: %f, w: %f, h: %f}\n", prefix, rec.x, rec.y, rec.width,
         rec.height);
}
