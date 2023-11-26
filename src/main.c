#include "gol.h"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------

GolCtx gol = {0};

int main(int argc, char *argv[]) { return gol_run(&gol, argc, argv); }
