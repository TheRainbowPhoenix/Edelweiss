#ifndef MOCK_GINT_H
#define MOCK_GINT_H
struct mock_call { int (*function)(void *); void *arg; };
#define GINT_CALL(fn, arg) ((struct mock_call){(int (*)(void *))(fn),(arg)})
static inline int gint_world_switch(struct mock_call c) { return c.function(c.arg); }
#endif
