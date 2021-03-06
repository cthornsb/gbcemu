
#include <iostream>

#include <SDL2/SDL.h>

#include "GraphicsSDL.hpp"
#include "SystemGBC.hpp"
#include "GPU.hpp"

Window *MAIN_WINDOW = 0x0;

void KeypressEvent::decode(const SDL_KeyboardEvent* evt, const bool &isDown){
	key = evt->keysym.sym;
	unsigned short modifier = evt->keysym.mod;
	none   = modifier & KMOD_NONE;
	lshift = modifier & KMOD_LSHIFT;
	rshift = modifier & KMOD_RSHIFT;
	lctrl  = modifier & KMOD_LCTRL;
	rctrl  = modifier & KMOD_RCTRL;
	lalt   = modifier & KMOD_LALT;
	ralt   = modifier & KMOD_RALT;	
	lgui   = modifier & KMOD_LGUI;
	rgui   = modifier & KMOD_RGUI;
	num    = modifier & KMOD_NUM;
	caps   = modifier & KMOD_CAPS;
	mode   = modifier & KMOD_MODE;
	down = isDown;
}

void MouseEvent::decode(const SDL_MouseButtonEvent* evt, const bool &isDown){
	lclick = evt->button & SDL_BUTTON_LMASK;
	mclick = evt->button & SDL_BUTTON_MMASK;
	rclick = evt->button & SDL_BUTTON_RMASK;
	x1     = evt->button & SDL_BUTTON_X1MASK;
	x2     = evt->button & SDL_BUTTON_X2MASK;
	clicks = evt->clicks;
	down = isDown;
}

void MouseEvent::decode(const SDL_MouseMotionEvent* evt){
	lclick = evt->state & SDL_BUTTON_LMASK;
	mclick = evt->state & SDL_BUTTON_MMASK;
	rclick = evt->state & SDL_BUTTON_RMASK;
	x1     = evt->state & SDL_BUTTON_X1MASK;
	x2     = evt->state & SDL_BUTTON_X2MASK;
	x = evt->x;
	y = evt->y;
	xrel = evt->xrel;
	yrel = evt->yrel;
}

Window::~Window(){
	//delete rectangle;
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void Window::setDrawColor(const ColorRGB &color, const float &alpha/*=1*/){
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, ColorRGB::toUChar(alpha));
}

void Window::clear(const ColorRGB &color/*=Colors::BLACK*/){
	setDrawColor(color);
	SDL_RenderClear(renderer);
}

void Window::drawPixel(const int &x, const int &y){
	if(nMult == 1)
		SDL_RenderDrawPoint(renderer, x, y);
	else{
		rectangle->h = nMult;
		rectangle->w = nMult;
		rectangle->x = x*nMult;
		rectangle->y = y*nMult;
		SDL_RenderDrawRect(renderer, rectangle);
	}
}

void Window::drawPixel(const int *x, const int *y, const size_t &N){
	for(size_t i = 0; i < N; i++) // Draw N pixels
		drawPixel(x[i], y[i]);
}

void Window::drawLine(const int &x1, const int &y1, const int &x2, const int &y2){
	SDL_RenderDrawLine(renderer, x1*nMult, y1*nMult, x2*nMult, y2*nMult);
}

void Window::drawLine(const int *x, const int *y, const size_t &N){
	if(N == 0) // Nothing to draw
		return;
	for(size_t i = 0; i < N-1; i++)
		drawLine(x[i], y[i], x[i+1], y[i+1]);
}

void Window::render(){
	SDL_RenderPresent(renderer);
}

bool Window::status(){
	static SDL_Event event;
	if(SDL_PollEvent(&event)){
		switch(event.type){
			case SDL_KEYDOWN:
				lastKey.decode(&event.key, true);
				if(lastKey.key == 0x1B) // Escape
					return false;
				break;
			case SDL_KEYUP:
				lastKey.decode(&event.key, false);
				break;
			case SDL_MOUSEBUTTONDOWN:
				lastMouse.decode(&event.button, true);
				break;
			case SDL_MOUSEBUTTONUP:
				lastMouse.decode(&event.button, false);
				break;
			case SDL_MOUSEMOTION:
				lastMouse.decode(&event.motion);
				break;
			case SDL_QUIT:
				return false;
			default:
				break;
		}
	}
	return true;
}

void Window::initialize(){
	if(init) return;

	rectangle = new SDL_Rect;

	// Open the SDL window
	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer(W*nMult, H*nMult, 0, &window, &renderer);
	clear();
	
	init = true;
}

#endif
