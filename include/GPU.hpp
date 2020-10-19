#ifndef GPU_HPP
#define GPU_HPP

#include "colors.hpp"
#include "SystemComponent.hpp"

class Window;

/////////////////////////////////////////////////////////////////////
// class SpriteAttHandler
/////////////////////////////////////////////////////////////////////

class SpriteAttHandler : public SystemComponent {
public:
	unsigned char yPos; // Y-position of the current sprite
	unsigned char xPos; // X-position of the current sprite
	unsigned char tileNum; // Tile index for the current sprite
	
	bool objPriority; // Object to Background priority (0: OBJ above BG, 1: OBJ behind BG color 1-3, BG color 0 always behind))
	bool yFlip; // (0: normal, 1: mirrored vertically)
	bool xFlip; // (0: normal, 1: mirrored horizontally)
	bool ngbcPalette; // (0: OBP0, 1: OBP1)
	bool gbcVramBank; // (0: Bank 0, 1: Bank 1)

	unsigned char gbcPalette; // (OBP0-7)

	SpriteAttHandler() : SystemComponent(160), index(0) { }

	virtual bool preWriteAction();
	
	bool getNextSprite(bool &visible);
	
private:
	unsigned short index; // Current sprite index [0,40)
};

/////////////////////////////////////////////////////////////////////
// class GPU
/////////////////////////////////////////////////////////////////////

class GPU : public SystemComponent {
public:
	GPU();

	~GPU();
	
	void initialize();

	void drawTileMaps();

	void drawNextScanline(SpriteAttHandler *oam);

	void render();

	Window *getWindow(){ return window; }

	bool getWindowStatus();
	
	void setPixelScale(const unsigned int &n);

	virtual bool preWriteAction();
	
	virtual bool preReadAction();

	virtual bool writeRegister(const unsigned short &reg, const unsigned char &val);
	
	virtual bool readRegister(const unsigned short &reg, unsigned char &val);

private:
	bool bgDisplayEnable;
	bool objDisplayEnable;
	bool objSizeSelect; ///< Sprite size [0: 8x8, 1: 8x16]
	bool bgTileMapSelect;
	bool bgWinTileDataSelect;
	bool winDisplayEnable;
	bool winTileMapSelect;
	bool lcdDisplayEnable;

	bool coincidenceFlag;
	bool mode0IntEnable;
	bool mode1IntEnable;
	bool mode2IntEnable;
	bool lycCoincIntEnable;

	bool bgPaletteIndexAutoInc;
	bool objPaletteIndexAutoInc;

	unsigned char lcdcStatusFlag;

	// Gameboy colors
	unsigned char ngbcPaletteColor[4]; ///< Original GB background palette
	unsigned char ngbcObj0PaletteColor[4]; ///< Original GB sprite palette 0
	unsigned char ngbcObj1PaletteColor[4]; ///< Original GB sprite palette 1

	unsigned char bgPaletteIndex;
	unsigned char objPaletteIndex;

	// GBC colors
	unsigned char bgPaletteData[64]; ///< GBC background palette 0-7
	unsigned char objPaletteData[64]; ///< GBC sprite palette 0-7

	ColorRGB bgPaletteColors[8][4]; ///< RGB colors for GBC background palettes 0-7
	ColorRGB objPaletteColors[8][4]; ///< RGB colors for GBC sprite palettes 0-7

	Window *window; ///< Pointer to the main renderer window
	
	unsigned char frameBuffer[256][256]; ///< Frame buffer containing 256x256 pixel shades

	ColorRGB *currentLineColors[256]; ///< Pointers to the RGB colors for the pixels on the current scanline

	/** Retrieve the color of a pixel in a tile bitmap.
	  * @param index The index of the tile in VRAM [0x8000,0x8FFF].
	  * @param dx    The horizontal pixel in the bitmap [0,7] where the right-most pixel is denoted as x=0.
	  * @param dy    The vertical pixel in the bitmap [0,7] where the top-most pixel is denoted as y=0.
	  * @return      The color of the pixel in the range [0,3]
	  */
	unsigned char getBitmapPixel(const unsigned short &index, const unsigned char &dx, const unsigned char &dy);
	
	/** Draw a background tile.
	  * @param x The current LCD screen horizontal pixel [0,160).
	  * @param y The vertical pixel row of the tile to draw.
	  * @param offset The memory offset of the selected tilemap in VRAM.
	  * @return The number of pixels drawn.
	  */
	unsigned short drawTile(const unsigned char &x, const unsigned char &y, 
	                        const unsigned char &x0, const unsigned char &y0,
	                        const unsigned short &offset);

	/** Draw the current sprite.
	  * @param y The current LCD screen scanline [0,144).
	  * @param oam Pointer to the sprite handler with the currently selected sprite.
	  */	
	void drawSprite(const unsigned char &y, SpriteAttHandler *oam);
};

#endif
