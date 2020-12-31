#ifndef INCLUDED_SUPPORT_H
#define INCLUDED_SUPPORT_H

//This file is licensed under the GPL V2
#include "gpl.txt"

//private for the render thread, use the elsewhere = badness :|

//Draw a line from x0,y0 to x1,y1 with color color
void drawLine(int x0, int y0, int x1, int y1, int color);


//Only call these from within the render thread
void smartPrint(int* scrX, int* scrY, const int color, const int bgcolor, char* inputStr, bool lineWrap);

void renderClearArea(const int clearType, const int color, int* x, int* y);


//Move up/down the current area, moving scrY in the same direction
void shiftDown(int *scrY);
void shiftUp(int *scrY);

//the rows the renderer is allowed to render within, defaults to 24 lines
#define default_top_row 0
#define default_bot_row 23
extern int top_row;
extern int bot_row;

extern bool font_set;

#define font_height 10
#define font_width 6
#define char_per_line 16

#endif
