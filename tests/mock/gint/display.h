#ifndef MOCK_DISPLAY_H
#define MOCK_DISPLAY_H
struct dwindow
{
    int left, top, right, bottom;
};
struct dwindow dwindow_set(struct dwindow window);
#endif
