//# link_flags: -lGL -pthread
// Like gl_buffer_restore, saving every frame while another thread frees memory of the game
// thread's malloc arena.
#define MALLOC_WORKER 1
#include "gl_buffer_restore.c"
