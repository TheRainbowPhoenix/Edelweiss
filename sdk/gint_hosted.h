#ifndef EDL_GINT_HOSTED_H
#define EDL_GINT_HOSTED_H
#include <gint/image.h>
/* These functions are not implemented in an EDL. GenerateEDL.cmake assigns
   ABS-zero link-time values; the host resolves them from its own resident
   gint kernel. Calling raw dsubimage/dgetpixel can instead link a SECOND
   copy of the graphics runtime into the payload. */
void edl_native_dsubimage(int x,int y,const image_t *image,int left,int top,
                           int width,int height,int flags);
int edl_native_dgetpixel(int x,int y);
#endif
