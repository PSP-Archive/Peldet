#define REALLY_RENDER
#include "global.h"
#include <psppower.h>

void fixXandY(bool moveIfNeeded);


//This file is licensed under the GPL V2
#include "gpl.txt"

//SceUID renderMessagebox;
unsigned int __attribute__((aligned(16))) terminal_pixels_full[(512*272) + (512*270)];
unsigned int __attribute__((aligned(16))) *terminal_pixels;//[512*272];

unsigned int __attribute__((aligned(16))) *viewport;

unsigned int __attribute__((aligned(16))) list[262144];

unsigned int __attribute__((aligned(16))) testpic[6*1];

int scrX = 0, scrY = 0;
int savedX = 0, savedY = 0;
int x_pos = 270; //screen scrolling with analog
void* framebuffer = 0;

bool drawCurrentPos = false;

//Render the Frame
void RenderFrame()
{
	scePowerTick(0);			//power tick to stop display turning off
	sceGuStart(GU_DIRECT,list);
	sceGuCopyImage(GU_PSM_8888,0,0,480,272,512,viewport,0,0,512,(void*)(0x04000000+(u32)framebuffer));
	if (drawCurrentPos)
	{	
		sceGuCopyImage(GU_PSM_8888,
		0,0,		/*source x,y*/
		6,1		/*image size*/
		,6/*source width*/
		,testpic/*source buffer*/
		,scrX*font_width,((scrY+1)*font_height)-2+270-x_pos/*destination x,y*/
		,512,(void*)(0x04000000+(u32)framebuffer)); /*dest buffer stuff*/
	}
	pic_draw(framebuffer);
	sceKernelDcacheWritebackAll();
	sceGuFinish();
	sceGuSync(0,0);
	framebuffer = sceGuSwapBuffers();
}



int render_mode = RENDER_SLOW;

void renderInit()
{
	terminal_pixels = &terminal_pixels_full[(512*270)]; //Half way down the full backbuffer
	viewport = &terminal_pixels_full[(512*270)]; //Half way down the full backbuffer
	pic_init();

	testpic[0] = COLOR_GREEN;
	testpic[1] = COLOR_GREEN;
	testpic[2] = COLOR_GREEN;
	testpic[3] = COLOR_GREEN;
	testpic[4] = COLOR_GREEN;
	testpic[5] = COLOR_GREEN;
}

void renderMainBG(char* string, int textColor, int bgColor)
{
	smartPrint(&scrX, &scrY, textColor, bgColor, string, true);
	if (render_mode == RENDER_SLOW)
		RenderFrame();
}

void renderReset()
{
	scrX = 0;
	scrY = 0;
	int x, y;	//TODO MEMSET
	for(y=0;y<24*10;y++)
		for (x=0;x<480;x++)
			terminal_pixels[x+(y*512)]=0;
	if (render_mode == RENDER_SLOW)
		RenderFrame();
}

void renderGoto(int x, int y)
{
	scrX = x;
	scrY = y;
	fixXandY(false);
}

void renderGoByReal(int x, int y, bool scroll)
{
	scrX += x;
	scrY += y;
	fixXandY(scroll);

	if (render_mode == RENDER_SLOW && scroll == true) //true -> posibly scrolled the screen
		RenderFrame();
}

void renderRedraw()
{
	RenderFrame();
}

void renderPutStringBG(int x, int y, int textColor, int BGColor, char* string)
{
	smartPrint(&x, &y, textColor, BGColor, string, false);
	if (render_mode == RENDER_SLOW)
		RenderFrame();
}

void renderMove(int xmov)
{
	int old_x = x_pos;
	//move up/down
	x_pos += xmov;
	if (x_pos > 270) x_pos = 270;
	if (x_pos < 0) x_pos = 0;
	viewport = &terminal_pixels_full[(512*x_pos)];
	if (x_pos != old_x)
		RenderFrame();
}

void renderMode(int mode)
{
	render_mode = mode;
}

void renderSaveLoad(int type)
{
	// Save or Load the current Position
	if (type == SAVE)
	{
		savedX = scrX;
		savedY = scrY;
	}
	else if (type == LOAD)
	{
		scrX = savedX ;
		scrY = savedY;
	}
	else
	{
		smartPrint(&scrX, &scrY, COLOR_RED, COLOR_BLACK, "Bad Flag in Renderer(Save/Load)", true);
		RenderFrame();
	}
}

void renderClear(const int type, const int color)
{
	renderClearArea(type, color, &scrX, &scrY);
	if (render_mode == RENDER_SLOW)
		RenderFrame();
}

void renderSetRegion(const int start, const int end)
{
	//Set the current render area
	top_row = start;
	bot_row = end;
	
	scrX = 0;
	scrY = start;
}

void renderSetFont(const int type)
{
	font_set = type;
}

void renderDrawCurrentPos(const int yesno)
{
	drawCurrentPos = yesno;
}

//moveIfNeeded = true means that we rotate the current area through instead of clamping to the edge
void fixXandY(bool moveIfNeeded)
{
	if (scrX < 0) scrX = 0;
	if (scrX >= 80) scrX = 79;
	if (moveIfNeeded)
	{
		while (scrY < top_row)
		{
			shiftDown(&scrY);
//			scrY++;
		}
		while (scrY > bot_row)
		{
			shiftUp(&scrY);
//			scrY--;
		}
	}
	else
	{
		if (scrY < top_row) scrY = top_row;
		if (scrY > bot_row) scrY = bot_row;
	}
}
