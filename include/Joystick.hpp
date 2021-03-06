#ifndef JOYSTICK_HPP
#define JOYSTICK_HPP

#include "SystemComponent.hpp"

class Window;
class ConfigFile;

class JoystickController : public SystemComponent {
public:
	JoystickController();
	
	void setWindow(Window *win){ window = win; }
	
	void setButtonMap(ConfigFile* config=0x0);

	void clearInput();
	
	// The joystick controller has no associated RAM, so return false to avoid trying to access it.
	bool preWriteAction() override { 
		return false; 
	}
	
	// The joystick controller has no associated RAM, so return false to avoid trying to access it.
	bool preReadAction() override { 
		return false; 
	}

	bool writeRegister(const unsigned short &reg, const unsigned char &val) override ;
	
	bool readRegister(const unsigned short &reg, unsigned char &val) override ;

	bool onClockUpdate() override ;

	void defineRegisters() override ;

private:
	bool selectButtonKeys;
	bool selectDirectionKeys;
	
	bool P13; // Down or Start
	bool P12; // Up or Select
	bool P11; // Left or B
	bool P10; // Right or A
	
	Window *window; ///< Pointer to the main LCD driver

	unsigned char keyMapArray[8]; ///< Array which maps the 8 gb buttons to keyboard keys
};

#endif
