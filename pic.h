#ifndef INCLUDED_PIC_H
#define INCLUDED_PIC_H

//CALL ONLY ONCE, initializes a blitable array for the pic, render will call it
void pic_init();

void pic_enable();

void pic_disable();

//blit the pic to screen
void pic_draw(void* framebuffer);

//returns true if there is one saved at the moment
bool pic_isEnabled();

#endif
