#ifndef INCLUDED_RENDER_H
#define INCLUDED_RENDER_H

#define COLOR_WHITE   0x0FFFFFFF
#define COLOR_GREY    0xB2B2B2B2
#define COLOR_BLACK   0x00000000

#define COLOR_RED     0x00000FFF
#define COLOR_GREEN   0x0000FF00
#define COLOR_BLUE    0x00FF0000

#define COLOR_CYAN    0x00FFFF00
#define COLOR_MAGENTA 0x00FF00FF
#define COLOR_YELLOW  0x0000FFFF
#define COLOR_PURPLE  0x00FF0FFF


#define BRIGHTNESS_BRIGHT  1
#define BRIGHTNESS_NORMAL  0
#define BRIGHTNESS_DIM    -1

#define MODIFY_BRIGHT 0xFFFFFFFF
#define MODIFY_NORMAL 0xB2B2B2B2
#define MODIFY_DIM    0x51515151



/* messageBox for the Network/UI Thread to message the Render Thread */
//extern SceUID renderMessagebox;

//Renderer Buffer
extern unsigned int __attribute__((aligned(16))) terminal_pixels_full[(512*272) + (512*270)];
extern unsigned int __attribute__((aligned(16))) *terminal_pixels;//[512*272];
extern unsigned int __attribute__((aligned(16))) list[262144];

extern void* framebuffer;

//int renderThread(SceSize args, void *argp);
void renderInit();

enum {
RENDER_MAIN,
RENDER_RESET,
RENDER_GOTO,
RENDER_GOBY,
RENDER_REDRAW,
RENDER_PUTSTRING,
RENDER_MOVE,
RENDER_MODE,
RENDER_SAVELOAD,
RENDER_CLEAR,
RENDER_SET_REGION,
RENDER_SETFONT,
RENDER_DRAW_CURRENT_POS
};

//Add something to be rendered in the main render area
void renderMainBG(char* string, int textColor, int bgcolor);
#define renderMain(string, textColor) renderMainBG(string, textColor, COLOR_BLACK)

//Clear the main Render Area
void renderReset();

//Move the cursor to x,y. if either is negative it will be ignored
void renderGoto(int x, int y);

//Move the cursor by x,y. sticks to edges
#define renderGoBy(x, y) renderGoByReal(x,y,false)

//Same as above except scrolls the area if at an edge
#define renderGoByMove(x, y) renderGoByReal(x,y,true)

void renderGoByReal(int x, int y, bool scroll);

//Redraw the Current Screen
void renderRedraw();

//Put a string at a position without moving the current Position or scrolling etc...
#define renderPutString(x, y, textColor, string) renderPutStringBG(x, y, textColor, COLOR_BLACK, string)

void renderPutStringBG(int x, int y, int textColor, int BGColor, char* string);

//Change the currently rendered area of the screen
void renderMove(int xmov);

//Change the renderer Mode, slow -- immediate redraw, fast -- redraw only when told
void renderMode(int mode);
//if RENDER_FAST is set then the buffer must be manually flushed with redraw.
#define RENDER_SLOW 0
#define RENDER_FAST 1


//Make the renderer save/restore the current pos
void renderSaveLoad(int type);
#define SAVE 0
#define LOAD 1

//Clear portions of the screen
void renderClear(const int type, int color);


enum {
CLEAR_TO_EOL,
CLEAR_TO_SOL,
CLEAR_LINE,
CLEAR_LINE_TO_BOTTOM,
CLEAR_LINE_TO_TOP,
CLEAR_SCREEN	//resets current pos to 0,0
};

//Set a render region (behaves like a vt100 scroll region)
void renderSetRegion(const int start, const int end);
	
//Change the fontset the renderer is using
void renderSetFont(const int type);
#define FONT_DEFAULT 0
#define FONT_LINEDRAW 1

void renderDrawCurrentPos(const int yesno);

#endif
