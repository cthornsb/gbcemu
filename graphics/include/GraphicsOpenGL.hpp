#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <vector>
#include <queue>

#include "colors.hpp"

class GPU;

class KeyStates{
public:
	/** Default constructor
	  */
	KeyStates();
	
	/** Enable text stream mode
	  */
	void enableStreamMode();
	
	/** Disable text stream mode and return to keypress mode
	  */
	void disableStreamMode();
	
	/** Return true if no keys are currently pressed
	  */
	bool empty() const { 
		return (nCount == 0); 
	}
	
	/** Check the current state of a key without modifying it
	  */
	bool check(const unsigned char &key) const { 
		return states[key]; 
	}

	/** Return true if left shift is currently pressed
	  */	
	bool leftShift() const {
		return bLeftShift;
	}

	/** Return true if right shift is currently pressed
	  */	
	bool rightShift() const {
		return bLeftShift;
	}

	/** Return true if left ctrl is currently pressed
	  */	
	bool leftCtrl() const {
		return bLeftShift;
	}

	/** Return true if right ctrl is currently pressed
	  */	
	bool rightCtrl() const {
		return bLeftShift;
	}

	/** Return true if left alt is currently pressed
	  */	
	bool leftAlt() const {
		return bLeftShift;
	}
	
	/** Return true if right alt is currently pressed
	  */
	bool rightAlt() const {
		return bLeftShift;
	}
	
	/** Poll the state of a key
	  * If the state of the key is currently pressed, mark it as released. 
	  * This method is useful for making the key sensitive to being pressed, but not to being held down.
	  */
	bool poll(const unsigned char &key);
	
	/** Press a key
	  * If bStreamMode is set, key is added to the stream buffer.
	  */
	void keyDown(const unsigned char &key);
	
	/** Release a key
	  */
	void keyUp(const unsigned char &key);
	
	/** Press or release a special key (e.g. function key, shift, etc)
	  */
	void keySpecial(const int &key, bool bKeyDown);
	
	/** Get a character from the stream buffer
	  * If the stream buffer is empty, return false.
	  */
	bool get(char& key);

	/** Reset the stream buffer and all key states
	  */
	void reset();

private:
	unsigned short nCount; ///< Number of standard keyboard keys which are currently pressed

	bool bStreamMode; ///< Flag to set keyboard to behave as stream buffer

	bool bLeftShift; ///< Set if left shift is pressed
	
	bool bLeftCtrl; ///< Set if left ctrl is pressed
	
	bool bLeftAlt; ///< Set if left alt is pressed
	
	bool bRightShift; ///< Set if right shift is pressed
	
	bool bRightCtrl; ///< Set if right ctrl is pressed
	
	bool bRightAlt; ///< Set if right alt is pressed

	std::queue<char> buffer; ///< Text stream buffer

	bool states[256]; ///< States of keyboard keys (true indicates key is down) 
};

class Window{
public:
	/** Default constructor
	  */
	Window() : 
		W(0), 
		H(0), 
		A(1),
		width(0),
		height(0),
		aspect(1),
		nMult(1), 
		winID(0), 
		init(false)
	{ 
	}
	
	/** Constructor taking the width and height of the window
	  */
	Window(const int &w, const int &h, const int& scale=1) : 
		W(w), 
		H(h),
		A(float(w)/h),
		width(w),
		height(h),
		aspect(float(w)/h),
		nMult(scale), 
		init(false)
	{
	}

	/** Destructor
	  */
	~Window();
	
	void close();

	bool processEvents();

	GPU *getGPU(){ return gpu; }

	/** Get the width of the window (in pixels)
	  */
	int getCurrentWidth() const { return W; }
	
	/** Get the height of the window (in pixels)
	  */
	int getCurrentHeight() const { return H; }

	/** Get the width of the window (in pixels)
	  */
	int getWidth() const { return W; }
	
	/** Get the height of the window (in pixels)
	  */
	int getHeight() const { return H; }

	/** Get screen scale multiplier.
	  */
	int getScale() const { return nMult; }

	/** Get the aspect ratio of the window (W/H)
	  */
	float getCurrentAspectRatio() const { return aspect; }

	/** Get the aspect ratio of the window (W/H)
	  */
	float getAspectRatio() const { return A; }

	/** Get the GLUT window ID number
	  */
	int getWindowID() const { return winID; }

	/** Get a pointer to the last user keypress event
	  */
	KeyStates* getKeypress(){ return &keys; }

	/** Set pointer to the pixel processor
	  */
	void setGPU(GPU *ptr){ gpu = ptr; }

	/** Set the width of the window (in pixels)
	  */
	void setWidth(const int &w){ width = w; }
	
	/** Set the height of the window (in pixels)
	  */
	void setHeight(const int &h){ height = h; }

	/** Set the integer pixel scaling multiplier (default = 1)
	  */
	void setScalingFactor(const int &scale);

	/** Set the current draw color
	  */
	static void setDrawColor(ColorRGB *color, const float &alpha=1);

	/** Set the current draw color
	  */
	static void setDrawColor(const ColorRGB &color, const float &alpha=1);

	/** Set this window as the current GLUT window
	  */
	void setCurrent();

	/** Clear the screen with a given color
	  */
	static void clear(const ColorRGB &color=Colors::BLACK);

	/** Draw a single pixel at position (x, y)
	  */
	static void drawPixel(const int &x, const int &y);

	/** Draw multiple pixels at positions (x1, y1) (x2, y2) ... (xN, yN)
	  * @param x Array of X pixel coordinates
	  * @param y Array of Y pixel coordinates
	  * @param N The number of elements in the arrays and the number of pixels to draw
	  */
	static void drawPixel(const int *x, const int *y, const size_t &N);
	
	/** Draw a single line to the screen between points (x1, y1) and (x2, y2)
	  */
	static void drawLine(const int &x1, const int &y1, const int &x2, const int &y2);

	/** Draw multiple lines to the screen
	  * @param x Array of X pixel coordinates
	  * @param y Array of Y pixel coordinates
	  * @param N The number of elements in the arrays. Since it is assumed that the number of elements 
		       in the arrays is equal to @a N, the total number of lines which will be drawn is equal to N-1
	  */
	static void drawLine(const int *x, const int *y, const size_t &N);

	/** Draw multiple lines to the screen
	  * @param x1 X coordinate of the upper left corner
	  * @param y1 Y coordinate of the upper left corner
	  * @param x2 X coordinate of the bottom right corner
	  * @param y2 Y coordinate of the bottom right corner
	  */
	static void drawRectangle(const int &x1, const int &y1, const int &x2, const int &y2);

	/** Render the current frame
	  */
	static void render();

	/** Return true if the window has been closed and return false otherwise
	  */
	bool status();

	/** Initialize OpenGL and open the window
	  */
	void initialize();

	void setKeyboardStreamMode();

	void setKeyboardToggleMode();

	void setupKeyboardHandler();

	virtual void paintGL();
	
	virtual void initializeGL();

	virtual void resizeGL(int width, int height);

private:
	int W; ///< Original width of the window (in pixels)
	int H; ///< Original height of the window (in pixels)
	float A; ///< Original aspect ratio of window

	int width; ///< Current width of window
	int height; ///< Current height of window
	float aspect; ///< Current aspect ratio of window
	
	int nMult; ///< Integer multiplier for window scaling
	int winID; ///< GLUT window identifier

	bool init; ///< Flag indicating that the window has been initialized

	GPU *gpu; ///< Pointer to the graphics processor

	KeyStates keys; ///< The last key which was pressed by the user
};
#endif
