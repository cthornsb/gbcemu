#include <iostream>
#include <string>
#include <string.h>

#include "Support.hpp"
#include "SystemGBC.hpp"
#include "SystemRegisters.hpp"
#include "Graphics.hpp"
#include "ColorGBC.hpp"
#include "ConfigFile.hpp"
#ifndef _WIN32
	#include "optionHandler.hpp"
#endif

#include "Cartridge.hpp"
#include "GPU.hpp"
#include "Sound.hpp"
#include "SystemClock.hpp"
#include "SystemTimer.hpp"
#include "Joystick.hpp"
#include "LR35902.hpp"
#include "WorkRam.hpp"
#include "HighRam.hpp"
#include "DmaController.hpp"
#include "Serial.hpp"
#include "SoundManager.hpp"
#include "MidiFile.hpp"

#ifdef USE_QT_DEBUGGER
	#include "mainwindow.h"
#endif

constexpr unsigned char SAVESTATE_VERSION = 0x1;

constexpr unsigned short VRAM_SWAP_START = 0x8000;
constexpr unsigned short CART_RAM_START  = 0xA000;
constexpr unsigned short WRAM_ZERO_START = 0xC000;
constexpr unsigned short OAM_TABLE_START = 0xFE00;
constexpr unsigned short HIGH_RAM_START  = 0xFF80;

constexpr unsigned short REGISTER_LOW    = 0xFF00;
constexpr unsigned short REGISTER_HIGH   = 0xFF80;

const std::string sysMessage    = " [System] ";
const std::string sysWarning    = " [System] Warning: ";
const std::string sysError      = " [System] Error! ";
const std::string sysFatalError = " [System] FATAL ERROR! ";

#ifdef GB_BOOT_ROM
	const std::string gameboyBootRomPath(GB_BOOT_ROM);
#else
	const std::string gameboyBootRomPath("");
#endif

#ifdef GBC_BOOT_ROM
	const std::string gameboyColorBootRomPath(GBC_BOOT_ROM);
#else
	const std::string gameboyColorBootRomPath("");
#endif

/** INTERRUPTS:
	0x40 - VBlank interrupt (triggered at beginning of VBlank period [~59.7 Hz])
	0x48 - LCDC status interrupt
	0x50 - Timer interrupt (triggered every time the timer overflows)
	0x58 - Serial interrupt (triggered after sending/receiving 8 bits)
	0x60 - Joypad interrupt (triggered any time a joypad button is pressed)
**/
	
ComponentList::ComponentList(SystemGBC *sys){
		list["APU"]       = (apu  = sys->sound.get());
		list["Cartridge"] = (cart = sys->cart.get());
		list["CPU"]       = (cpu = sys->cpu.get());
		list["DMA"]       = (dma = sys->dma.get());
		list["GPU"]       = (gpu = sys->gpu.get());
		list["HRAM"]      = (hram = sys->hram.get());
		list["Joypad"]    = (joy = sys->joy.get());
		list["OAM"]       = (oam = sys->oam.get());
		list["Clock"]     = (sclk = sys->sclk.get());
		list["Serial"]    = (serial = sys->serial.get());
		list["Timer"]     = (timer = sys->timer.get());
		list["WRAM"]      = (wram = sys->wram.get());
}
	
SystemGBC::SystemGBC(int& argc, char* argv[]) :
	dummyComponent("System"),
	nFrames(0),
	frameSkip(1),
	verboseMode(false),
	debugMode(false),
	cpuStopped(false),
	cpuHalted(false),
	emulationPaused(false),
	bootSequence(false),
	forceColor(false),
	displayFramerate(false),
	userQuitting(false),
	autoLoadExtRam(true),
	initSuccessful(false),
	fatalError(false),
	consoleIsOpen(false),
	bLockedVRAM(false),
	bLockedOAM(false),
	dmaSourceH(0),
	dmaSourceL(0),
	dmaDestinationH(0),
	dmaDestinationL(0),
	romPath(),
	romFilename(),
	romExtension(),
	pauseAfterNextInstruction(false),
	pauseAfterNextClock(false),
	pauseAfterNextHBlank(false),
	pauseAfterNextVBlank(false),
	audioInterface(&SoundManager::getInstance())
{ 
	// Disable memory region monitor
	memoryAccessWrite[0] = 1; 
	memoryAccessWrite[1] = 0;
	memoryAccessRead[0] = 1; 
	memoryAccessRead[1] = 0;
	
	// Configuration file handler
	ConfigFile cfgFile;

#ifndef _WIN32
	// Handle command line options
	optionHandler handler;
	handler.add(optionExt("config", required_argument, NULL, 'c', "<filename>", "Specify an input configuration file."));
	handler.add(optionExt("input", required_argument, NULL, 'i', "<filename>", "Specify an input geant macro."));
	handler.add(optionExt("framerate", required_argument, NULL, 'F', "<multiplier>", "Set target framerate multiplier (default=1)."));
	handler.add(optionExt("volume", required_argument, NULL, 'V', "<volume>", "Set initial output volume (in range 0 to 1)."));
	handler.add(optionExt("verbose", no_argument, NULL, 'v', "", "Toggle verbose mode."));
	handler.add(optionExt("scale-factor", required_argument, NULL, 'S', "<N>", "Set the integer size multiplier for the screen (default 2)."));
	handler.add(optionExt("use-color", no_argument, NULL, 'C', "", "Use GBC mode for original GB games."));
	handler.add(optionExt("no-load-sram", no_argument, NULL, 'n', "", "Do not load external cartridge RAM (SRAM) at boot."));
#ifdef USE_QT_DEBUGGER			
	handler.add(optionExt("debug", no_argument, NULL, 'd', "", "Enable Qt debugging GUI."));
	handler.add(optionExt("tile-viewer", no_argument, NULL, 'T', "", "Enable VRAM tile viewer (if debug gui enabled)."));
	handler.add(optionExt("layer-viewer", no_argument, NULL, 'L', "", "Enable BG/WIN layer viewer (if debug gui enabled)."));
#endif // ifdef USE_QT_DEBUGGER
	// Handle user input.
	if(!handler.setup(argc, argv)){
		fatalError = true;
		return;
	}
	if(handler.getOption(0)->active){ 
		std::cout << sysMessage << "Reading from configuration file (" << handler.getOption(0)->argument << ")" << std::endl;
		if(!cfgFile.read(handler.getOption(0)->argument)){ // Read configuration file
			std::cout << sysFatalError << "Failed to load input configuration file." << std::endl;
			fatalError = true;
			return;
		}
		// Get the ROM path
		romPath = cfgFile.getValue("ROM_DIRECTORY") + "/" + cfgFile.getValue("ROM_FILENAME");
	}
	if(handler.getOption(1)->active) // Set input filename
		romPath = handler.getOption(1)->argument;
#else // ifndef _WIN32	
	std::cout << sysMessage << "Reading from configuration file (default.cfg)" << std::endl;
	if(!cfgFile.read("default.cfg")){ // Read configuration file
		std::cout << sysFatalError << "Failed to load input configuration file." << std::endl;
		fatalError = true;
		return;
	}
#endif // ifndef _WIN32	

	// Get the ROM path from the config file
	if(romPath.empty() && cfgFile.good()){ 
		if(cfgFile.search("ROM_DIRECTORY", true))
			romPath += cfgFile.getValue() + "/";
		if(cfgFile.search("ROM_FILENAME", true))
			romPath += cfgFile.getValue();
	}

	// Check for ROM path
	if(romPath.empty()){
		std::cout << sysFatalError << "Input gb/gbc ROM file not specified!" << std::endl;
		fatalError = true;
		return;
	}

	// Get the ROM filename and file extension
	size_t index = romPath.find_last_of('/');
	if(index != std::string::npos)
		romFilename = romPath.substr(index+1);
	else
		romFilename = romPath;
	index = romFilename.find_last_of('.');
	if(index != std::string::npos){
		romExtension = romFilename.substr(index+1);
		romFilename = romFilename.substr(0, index);
	}

	// Define all system components
	serial.reset(new SerialController);
	dma.reset(new DmaController);
	cart.reset(new Cartridge);
	gpu.reset(new GPU);
	sound.reset(new SoundProcessor);
	oam.reset(new SpriteHandler);
	joy.reset(new JoystickController);
	wram.reset(new WorkRam);
	hram.reset(new HighRam);
	sclk.reset(new SystemClock);
	timer.reset(new SystemTimer);
	cpu.reset(new LR35902);
	
	// Disable dumping cartridge ROM to quicksave
	cart->disableSaveRAM();
	
	// Add all components to the subsystem list
	subsystems = std::unique_ptr<ComponentList>(new ComponentList(this));

	// Initialize registers vector
	registers = std::vector<Register>(REGISTER_HIGH-REGISTER_LOW, Register());

	// Initialize system components
	this->initialize();

#ifdef USE_QT_DEBUGGER
	// Pre-processor statements to avoid un-used variable warnings
	bool useTileViewer = false;
	bool useLayerViewer = false;
#endif // ifdef USE_QT_DEBUGGER	

	if(cfgFile.good()){ // Handle user input from config file
		if (cfgFile.search("MASTER_VOLUME", true)) // Set master output volume
			sound->getMixer()->setVolume(cfgFile.getFloat());
		if (cfgFile.search("FRAMERATE_MULTIPLIER", true)) // Set framerate multiplier
			sclk->setFramerateMultiplier(cfgFile.getFloat());
		if (cfgFile.searchBoolFlag("VERBOSE_MODE")) // Toggle verbose flag
			setVerboseMode(true);
		if (cfgFile.search("PIXEL_SCALE", true)) // Set pixel scaling factor
			gpu->setPixelScale(cfgFile.getUInt());
		if (cfgFile.searchBoolFlag("FORCE_COLOR")) // Use GBC mode for original GB games
			setForceColorMode(true);
		if (cfgFile.searchBoolFlag("DISABLE_AUTO_SAVE")) // Do not automatically save/load external cartridge RAM (SRAM)
			autoLoadExtRam = false;
#ifdef USE_QT_DEBUGGER			
		if (cfgFile.searchBoolFlag("DEBUG_MODE")) { // Toggle debug flag
			setDebugMode(true);
			if (cfgFile.searchBoolFlag("OPEN_TILE_VIEWER")) // Open tile viewer window
				useTileViewer = true;
			if (cfgFile.searchBoolFlag("OPEN_LAYER_VIEWER")) // Open layer viewer window
				useLayerViewer = true;
		}
#endif // ifdef USE_QT_DEBUGGER
		// Setup key mapping
		joy->setButtonMap(&cfgFile);
	}

#ifndef _WIN32
	if(handler.good()){ // Handle user command line arguments
		if(handler.getOption(2)->active) // Set framerate multiplier
			sclk->setFramerateMultiplier(strtod(handler.getOption(2)->argument.c_str(), NULL));
		if(handler.getOption(3)->active) // Set master output volume
			sound->getMixer()->setVolume(strtod(handler.getOption(3)->argument.c_str(), NULL));
		if(handler.getOption(4)->active) // Toggle verbose flag
			setVerboseMode(true);
		if(handler.getOption(5)->active) // Set pixel scaling factor
			gpu->setPixelScale(strtoul(handler.getOption(5)->argument.c_str(), NULL, 10));
		if(handler.getOption(6)->active) // Use GBC mode for original GB games
			forceColor = true;
		if(handler.getOption(7)->active) // Do not automatically save/load external cartridge RAM (SRAM)
			autoLoadExtRam = false;
#ifdef USE_QT_DEBUGGER			
		if(handler.getOption(8)->active){ // Toggle debug flag
			setDebugMode(true);
			if(handler.getOption(9)->active) // Open tile-viewer window
				useTileViewer = true;
			if(handler.getOption(10)->active) // Open layer-viewer window
				useLayerViewer = true;
		}
#endif // ifdef USE_QT_DEBUGGER
	}
#endif // ifndef _WIN32

#ifdef USE_QT_DEBUGGER
	if(debugMode){ // Open Gui window(s)
		//app = std::unique_ptr<QApplication>(new QApplication(argc, argv));
		//gui = std::unique_ptr<MainWindow>(new MainWindow(app.get()));
		//gui->connectToSystem(this);
		if(useTileViewer) // Open tile-viewer window
			gui->openTileViewer();
		if(useLayerViewer) // Open layer-viewer window
			gui->openLayerViewer();
	}
#endif // ifdef USE_QT_DEBUGGER

	pauseAfterNextInstruction = false;
	pauseAfterNextClock = false;
	pauseAfterNextHBlank = false;
	pauseAfterNextVBlank = false;
}

SystemGBC::~SystemGBC(){
}

void SystemGBC::initialize(){ 
	if(fatalError) // Check for fatal error
		return;
	if(initSuccessful) // Already initialized
		return;

	// Define system registers
	addSystemRegister(0x0F, rIF,   "IF",   "33333000");
	addSystemRegister(0x4D, rKEY1, "KEY1", "30000001");
	addSystemRegister(0x56, rRP,   "RP",   "31000033");
	addDummyRegister(0x0, 0x50); // The "register" used to disable the bootstrap ROM
	rIE  = new Register("IE",  "33333000");
	rIME = new Register("IME", "30000000");
	(*rIME) = 1; // Interrupts enabled by default

	// Undocumented registers
	addSystemRegister(0x6C, rFF6C, "FF6C", "30000000");
	addSystemRegister(0x72, rFF72, "FF72", "33333333");
	addSystemRegister(0x73, rFF73, "FF73", "33333333");
	addSystemRegister(0x74, rFF74, "FF74", "33333333");
	addSystemRegister(0x75, rFF75, "FF75", "00003330");
	addSystemRegister(0x76, rFF76, "FF76", "11111111");
	addSystemRegister(0x77, rFF77, "FF77", "11111111");

	// Define sub-system registers and connect to the system bus.
	for(auto comp = subsystems->list.begin(); comp != subsystems->list.end(); comp++){
		comp->second->connectSystemBus(this);
	}

	// Set memory offsets for all components	
	gpu->setOffset(VRAM_SWAP_START);
	cart->getRam()->setOffset(CART_RAM_START);
	wram->setOffset(WRAM_ZERO_START);
	oam->setOffset(OAM_TABLE_START);
	hram->setOffset(HIGH_RAM_START);

	// Must connect the system bus BEFORE initializing the CPU.
	cpu->initialize();
	
	// Disable the system timer
	timer->disableTimer();

	// Initialize the window and link it to the joystick controller	
	gpu->initialize();
	joy->setWindow(gpu->getWindow());

	// Initialization was successful
	initSuccessful = true;
}

bool SystemGBC::execute(){
	if(!initSuccessful)
		return false;
	// Run the ROM. Main loop.
	while(true){
		// Check the status of the GPU and LCD screen
		if(!gpu->getWindowStatus() || userQuitting)
			break;

		if(!emulationPaused && !cpuStopped){
			// Check for interrupt out of HALT
			if(cpuHalted){
				if(((*rIE) & (*rIF)) != 0)
					cpuHalted = false;
			}

			// Update system timer
			timer->onClockUpdate();

			// Update sound processor
			sound->onClockUpdate();

			// Update joypad handler
			joy->onClockUpdate();

			// Tick the system sclk
			sclk->onClockUpdate();
#ifdef USE_QT_DEBUGGER
			if(pauseAfterNextClock){
				pauseAfterNextClock = false;					
				pause();
			}
#endif

			// Check if the CPU is halted.
			if(!cpuHalted && !dma->onClockUpdate()){
				// Perform one instruction.
				if(cpu->onClockUpdate()){
#ifdef USE_QT_DEBUGGER
					if(pauseAfterNextInstruction){
						pauseAfterNextInstruction = false;					
						pause();
					}
					else if(breakpointOpcode.check(cpu->getLastOpcode()->nIndex) ||
					   breakpointProgramCounter.check(cpu->getLastOpcode()->nPC))
						pause();
#endif
				}
			}

			// Sync with the framerate.	
			if(sclk->pollVSync()){
				// Process window events
				gpu->processEvents();
				checkSystemKeys();
				
				// Render the current frame
				if(nFrames++ % frameSkip == 0 && !cpuStopped){
					if(displayFramerate)
						gpu->print(doubleToStr(sclk->getFramerate(), 1)+" fps", 0, 17);
					gpu->render();
				}
#ifdef USE_QT_DEBUGGER
				if(debugMode){
					if(!pauseAfterNextVBlank){
						updateDebugger();
					}
					else{
						pauseAfterNextVBlank = false;
						pause();
					}
				}
#endif
			}
		}
		else{
			if(cpuStopped){ // STOP
				std::cout << sysMessage << "Stopped! " << getHex(rIE->getValue()) << " " << getHex(rIF->getValue()) << std::endl;
				//if((*rIF) == 0x10)
					resumeCPU();
			}
		
			// Process window events
			gpu->processEvents();

			// 
			if(!consoleIsOpen)
				checkSystemKeys();
			else
				gpu->drawConsole();			

			// Maintain framerate but do not advance the system clock
			sclk->wait();
#ifndef USE_QT_DEBUGGER
		}
	}
#else
			if(debugMode) // Process events for the Qt GUI
				updateDebugger();
		}
	}
	if(debugMode)
		gui->quit();
#endif
	if(audioInterface) // Terminate audio stream
		audioInterface->quit();
	if(autoLoadExtRam) // Save save data (if available)
		writeExternalRam();
	return true;
}

void SystemGBC::handleHBlankPeriod(){
	if(!emulationPaused){
		if(nFrames % frameSkip == 0){
			sclk->setPixelClockPause( gpu->drawNextScanline(oam.get()) );
		}
		dma->onHBlank();
	}
#ifdef USE_QT_DEBUGGER
	if(debugMode && pauseAfterNextHBlank){
		pauseAfterNextHBlank = false;
		pause();
		gpu->render(); // Show the newly drawn scanline
	}
#endif
}
	
void SystemGBC::handleVBlankInterrupt(){ 
	rIF->setBit(0);
}

void SystemGBC::handleLcdInterrupt(){ 
	rIF->setBit(1);
}

void SystemGBC::handleTimerInterrupt(){ 
	rIF->setBit(2);
}

void SystemGBC::handleSerialInterrupt(){ 
	rIF->setBit(3); 
}

void SystemGBC::handleJoypadInterrupt(){ 
	rIF->setBit(4); 
}

void SystemGBC::enableInterrupts(){ 
	(*rIME) = 1; 
}

void SystemGBC::disableInterrupts(){ 
	(*rIME) = 0;
}

bool SystemGBC::write(const unsigned short &loc, const unsigned char &src){
	// Check for system registers
	if(loc >= REGISTER_LOW && loc < REGISTER_HIGH){
		// Write the register
		if(!writeRegister(loc, src))
			return false;
	}
	else if(loc <= 0x7FFF){ // Cartridge ROM 
		cart->writeRegister(loc, src); // Write to cartridge MBC (if present)
	}
	else if(loc <= 0x9FFF){ // Video RAM (VRAM)
		if(bLockedVRAM) // PPU is using VRAM, access restricted
			return false;
		gpu->write(loc, src);
	}
	else if(loc <= 0xBFFF){ // External (cartridge) RAM
		if(cart->hasRam())
			cart->getRam()->write(loc, src);
	}
	else if(loc <= 0xFDFF){ // Work RAM (WRAM) bank 0, swap, and echo
		wram->write(loc, src);
	}
	else if(loc <= 0xFE9F){ // Sprite table (OAM)
		if(bLockedOAM) // PPU is using OAM, access restricted
			return false;
		oam->write(loc, src);
	}
	else if (loc <= 0xFF7F){ // System registers / Inaccessible
		return false;
	}
	else if(loc <= 0xFFFE){ // High RAM (HRAM)
		hram->write(loc, src);
	}
	else if(loc == 0xFFFF){ // Interrupt enable (IE)
		rIE->write(src);
	}
#ifdef USE_QT_DEBUGGER
	// Check for memory access watch
	if (loc >= memoryAccessWrite[0] && loc <= memoryAccessWrite[1]) {
		OpcodeData* op = cpu->getLastOpcode();
		std::cout << sysMessage << "(W) PC=" << getHex(op->nPC) << " " << getHex(src) << "->[" << getHex(loc) << "] ";
		if (op->op->nBytes == 2)
			std::cout << "d8=" << getHex(op->getd8());
		else if (op->op->nBytes == 3)
			std::cout << "d16=" << getHex(op->getd16());
		std::cout << std::endl;
	}
	// Check for memory write breakpoint
	if (breakpointMemoryWrite.check(loc))
		pause();
#endif // ifdef USE_QT_DEBUGGER
	return true; // Successfully wrote to memory location (loc)
}

bool SystemGBC::read(const unsigned short &loc, unsigned char &dest){
	// Check for system registers
	if(loc >= REGISTER_LOW && loc < REGISTER_HIGH){
		// Read the register
		if(!readRegister(loc, dest))
			return false;
	}
	else if(loc <= 0x3FFF){ // Cartridge ROM bank 0
		if(bootSequence && (loc < 0x100 || loc >= 0x200)){ // Startup sequence. 
			//Ignore bytes between 0x100 and 0x200 where the cartridge header lives.
			dest = bootROM[loc];
		}
		else
			cart->readFastBank0(loc, dest);
	}
	else if(loc <= 0x7FFF){ // Cartridge ROM swap bank
		cart->readFast(loc-0x4000, dest);
	}
	else if(loc <= 0x9FFF){ // Video RAM (VRAM)
		if(bLockedVRAM) // PPU is using VRAM, access restricted
			return false;
		gpu->read(loc, dest);
	}
	else if(loc <= 0xBFFF){ // External RAM (SRAM)
		if(cart->hasRam())
			cart->getRam()->read(loc, dest);
	}
	else if(loc <= 0xFDFF){ // Work RAM (WRAM) bank 0, swap, and echo
		wram->read(loc, dest);
	}
	else if(loc <= 0xFE9F){ // Sprite table (OAM)
		if(bLockedOAM) // PPU is using OAM, access restricted
			return false;
		oam->read(loc, dest);
	}
	else if (loc <= 0xFF7F) { // System registers / Inaccessible
		return false;
	}
	else if(loc <= 0xFFFE){ // High RAM (HRAM)
		hram->read(loc, dest);
	}
	else if(loc == 0xFFFF){ // Interrupt enable (IE)
		dest = rIE->read();
	}
#ifdef USE_QT_DEBUGGER
	// Check for memory read breakpoint
	if (breakpointMemoryRead.check(loc))
		pause();
	// Check for memory access watch
	if(loc >= memoryAccessRead[0] && loc <= memoryAccessRead[1]){
		OpcodeData *op = cpu->getLastOpcode();
		std::cout << sysMessage << "(R) PC=" << getHex(op->nPC) << " [" << getHex(loc) << "]=" << getHex(dest) << "" << std::endl;
	}
#endif // ifdef USE_QT_DEBUGGER
	return true; // Successfully read from memory location (loc)
}

unsigned char SystemGBC::getValue(const unsigned short &loc){
	unsigned char retval;
	read(loc, retval);
	return retval;
}

unsigned char *SystemGBC::getPtr(const unsigned short &loc){
	// Note: Direct access to ROM banks is restricted. 
	// Use write() and read() methods to access instead.
	unsigned char *retval = 0x0;
	if(loc >= 0x8000 && loc <= 0x9FFF){ // Video RAM (VRAM)
		retval = gpu->getPtr(loc);
	}
	else if(loc <= 0xBFFF){ // External (cartridge) RAM (if available)
		retval = cart->getRam()->getPtr(loc);
	}
	else if(loc <= 0xFDFF){ // Work RAM (WRAM) 0-1 and ECHO
		retval = wram->getPtr(loc);
	}
	else if(loc <= 0xFE9F){ // Sprite table (OAM)
		retval = oam->getPtr(loc);
	}
	else if(loc >= 0xFF00 && loc <= 0xFF7F){ // System registers
		retval = getPtrToRegisterValue(loc);
	}
	else if(loc <= 0xFFFE){ // High RAM (HRAM)
		retval = hram->getPtr(loc);
	}
	else if (loc >= 0xFFFF){ // Interrupt enable (IE)
		retval = rIE->getPtr();
	}
	return retval;
}

const unsigned char *SystemGBC::getConstPtr(const unsigned short &loc){
	const unsigned char *retval = 0x0;
	if(loc <= 0x7FFF){ // ROM
		retval = cart->getConstPtr(loc);
	}
	else if(loc <= 0x9FFF){ // Video RAM (VRAM)
		retval = gpu->getConstPtr(loc);
	}
	else if(loc <= 0xBFFF){ // External (cartridge) RAM (if available)
		retval = cart->getRam()->getConstPtr(loc);
	}
	else if(loc <= 0xFDFF){ // Work RAM (WRAM) 0-1 and ECHO
		retval = wram->getConstPtr(loc);
	}
	else if(loc <= 0xFE9F){ // Sprite table (OAM)
		retval = oam->getConstPtr(loc);
	}
	else if(loc >= 0xFF00 && loc <= 0xFF7F){ // System registers
		retval = getConstPtrToRegisterValue(loc);
	}
	else if(loc <= 0xFFFE){ // High RAM (HRAM)
		retval = hram->getConstPtr(loc);
	}
	else if(loc == 0xFFFF){ // Interrupt enable (IE)
		retval = rIE->getConstPtr();
	}
	return retval;
}

Register *SystemGBC::getPtrToRegister(const unsigned short &reg){
	if(reg < REGISTER_LOW || reg >= REGISTER_HIGH)
		return 0x0;
	return &registers[reg-0xFF00];
}

unsigned char *SystemGBC::getPtrToRegisterValue(const unsigned short &reg){
	if(reg < REGISTER_LOW || reg >= REGISTER_HIGH)
		return 0x0;
	return registers[reg-0xFF00].getPtr();
}

const unsigned char *SystemGBC::getConstPtrToRegisterValue(const unsigned short &reg){
	if(reg < REGISTER_LOW || reg >= REGISTER_HIGH)
		return 0x0;
	return registers[reg-0xFF00].getConstPtr();
}

Register* SystemGBC::getRegisterByName(const std::string& name){
	std::string capsname = toUppercase(name); // All registers are capitalized
	for(std::vector<Register>::iterator reg = registers.begin(); reg != registers.end(); reg++){
		if(capsname == reg->getName())
			return &(*reg);
	}
	return 0x0;
}

void SystemGBC::setDebugMode(bool state/*=true*/){
	debugMode = state;
	for(auto comp = subsystems->list.begin(); comp != subsystems->list.end(); comp++)
		comp->second->setDebugMode(state);
}

// Toggle verbose flag
void SystemGBC::setVerboseMode(bool state/*=true*/){
	verboseMode = state;
	for(auto comp = subsystems->list.begin(); comp != subsystems->list.end(); comp++)
		comp->second->setVerboseMode(state);
}

void SystemGBC::setMemoryWriteRegion(const unsigned short &locL, const unsigned short &locH/*=0*/){
	memoryAccessWrite[0] = locL;
	if(locH > locL){
		memoryAccessWrite[1] = locH;
		std::cout << sysMessage << "Watching writes to memory in range " << getHex(locL) << " to " << getHex(locH) << std::endl;
	}
	else{
		memoryAccessWrite[1] = locL;
		std::cout << sysMessage << "Watching writes to memory location " << getHex(locL) << std::endl;
	}
}

void SystemGBC::setMemoryReadRegion(const unsigned short &locL, const unsigned short &locH/*=0*/){
	memoryAccessRead[0] = locL;
	if(locH > locL){
		memoryAccessRead[1] = locH;
		std::cout << sysMessage << "Watching reads from memory in range " << getHex(locL) << " to " << getHex(locH) << std::endl;
	}
	else{
		memoryAccessRead[1] = locL;
		std::cout << sysMessage << "Watching reads from memory location " << getHex(locL) << std::endl;
	}
}

void SystemGBC::setBreakpoint(const unsigned short &pc){
	breakpointProgramCounter.enable(pc);
}

void SystemGBC::setMemWriteBreakpoint(const unsigned short &addr){
	breakpointMemoryWrite.enable(addr);
}

void SystemGBC::setMemReadBreakpoint(const unsigned short &addr){
	breakpointMemoryRead.enable(addr);
	std::cout << this << "\tread=" << getHex(addr) << std::endl;
}

void SystemGBC::setOpcodeBreakpoint(const unsigned char &op, bool cb/*=false*/){
	breakpointOpcode.enable(op);
}

void SystemGBC::setAudioInterface(SoundManager* ptr){
	audioInterface = ptr;
}

#ifdef USE_QT_DEBUGGER
void SystemGBC::setQtDebugger(MainWindow* ptr){
	ptr->connectToSystem(this);
	gui = ptr; 
}
#endif

void SystemGBC::setFramerateMultiplier(const float& freq){
	sclk->setFramerateMultiplier(freq);
	sound->getMixer()->setSampleRateMultiplier(freq);
}

void SystemGBC::clearBreakpoint(){
	breakpointProgramCounter.clear();
}

void SystemGBC::clearMemWriteBreakpoint(){
	breakpointMemoryWrite.clear();
}

void SystemGBC::clearMemReadBreakpoint(){
	breakpointMemoryRead.clear();
}

void SystemGBC::clearOpcodeBreakpoint(){
	breakpointOpcode.clear();
}

void SystemGBC::addSystemRegister(SystemComponent *comp, const unsigned char &reg, Register* &ptr, const std::string &name, const std::string &bits){
	registers[reg].setName(name);
	registers[reg].setMasks(bits);
	registers[reg].setAddress(0xFF00+reg);
	registers[reg].setSystemComponent(comp);
	ptr = &registers[reg];
}

void SystemGBC::addSystemRegister(const unsigned char &reg, Register* &ptr, const std::string &name, const std::string &bits){
	addSystemRegister(&dummyComponent, reg, ptr, name, bits);
}

void SystemGBC::addDummyRegister(SystemComponent *comp, const unsigned char &reg){
	registers[reg].setSystemComponent(comp);
}

void SystemGBC::clearRegister(const unsigned char &reg){ 
	registers[reg].clear(); 
}

bool SystemGBC::dumpMemory(const std::string &fname){
	std::cout << sysMessage << "Writing system memory to file \"" << fname << "\"... ";
	std::ofstream ofile(fname.c_str(), std::ios::binary);
	if(!ofile.good()){
		std::cout << "FAILED!" << std::endl;
		return false;
	}
		
	unsigned char byte;
	for(unsigned int i = 0; i <= 0xFFFF; i++){
		// Read a byte from memory.
		if(i >= 0xFEA0 && i < 0xFF00) // Not accessible
			byte = 0x00;
		else if(!read((unsigned short)i, byte)){
			if(i >= REGISTER_LOW && i < REGISTER_HIGH) // System registers
				byte = registers[i-REGISTER_LOW].getValue();
			else{
				std::cout << sysWarning << "Failed to read memory location " << getHex((unsigned short)i) << "!" << std::endl;
				byte = 0x00; // Write an empty byte
			}
		}
		ofile.write((char*)&byte, 1);
	}
	ofile.close();

	std::cout << "DONE" << std::endl;

	return true;
}

bool SystemGBC::dumpVRAM(const std::string &fname){
	std::cout << sysMessage << "Writing VRAM to file \"" << fname << "\"... ";
	std::ofstream ofile(fname.c_str(), std::ios::binary);
	if(!ofile.good() || !gpu->writeMemoryToFile(ofile)){
		std::cout << "FAILED!" << std::endl;
		return false;
	}
	ofile.close();
	std::cout << "DONE" << std::endl;
	return true;
}

bool SystemGBC::saveSRAM(const std::string &fname){
	if(!cart->getSaveSupport()){
		if(verboseMode)
			std::cout << sysMessage << "Cartridge has no save data." << std::endl;
		return false;
	}
	std::ofstream ofile(fname.c_str(), std::ios::binary);
	if(!ofile.good() || !cart->getRam()->writeMemoryToFile(ofile)){
		if(verboseMode)
			std::cout << sysMessage << "Writing cartridge RAM to file \"" << fname << "\"... FAILED!" << std::endl;
		return false;
	}
	ofile.close();
	if(verboseMode)
		std::cout << sysMessage << "Writing cartridge RAM to file \"" << fname << "\"... DONE!" << std::endl;
	return true;
}

bool SystemGBC::loadSRAM(const std::string &fname){
	if(!cart->getSaveSupport()){
		if(verboseMode)
			std::cout << sysMessage << "Cartridge has no save data." << std::endl;
		return false;
	}
	std::ifstream ifile(fname.c_str(), std::ios::binary);
	if(!ifile.good() || !cart->getRam()->readMemoryFromFile(ifile)){
		if(verboseMode)
			std::cout << sysMessage << "Reading cartridge RAM from file \"" << fname << "\"... FAILED!" << std::endl;
		return false;
	}
	ifile.close();
	if(verboseMode)
		std::cout << sysMessage << "Reading cartridge RAM from file \"" << fname << "\"... DONE!" << std::endl;
	return true;
}

void SystemGBC::resumeCPU(){ 
	cpuStopped = false;
	if(rKEY1->getBit(0)){ // Prepare speed switch
		if(!bCPUSPEED){ // Normal speed
			sclk->setDoubleSpeedMode();
			sound->getMixer()->setDoubleSpeedMode();
			bCPUSPEED = true;
			rKEY1->clear();
			rKEY1->setBit(7);
		}
		else{ // Double speed
			sclk->setNormalSpeedMode();
			sound->getMixer()->setNormalSpeedMode();
			bCPUSPEED = false;
			rKEY1->clear();
		}
	}
}

#ifdef USE_QT_DEBUGGER
void SystemGBC::updateDebugger(){
	gui->update();
	gui->processEvents();
}
#endif

void SystemGBC::pause(){ 
	emulationPaused = true; 
#ifdef USE_QT_DEBUGGER
	if(debugMode){
		gui->updatePausedState(true);
		updateDebugger();
	}
#endif
	sound->pause(); // Stop audio output
}

void SystemGBC::unpause(bool resumeAudio/*=true*/){ 
	emulationPaused = false; 
#ifdef USE_QT_DEBUGGER
	if(debugMode)
		gui->updatePausedState(false);
#endif
	if(resumeAudio)
		sound->resume(); // Restart audio output
}

bool SystemGBC::reset() {
	if(!initSuccessful)
		return false;

	// Set default register values.
	cpu->reset();

	// Read the ROM into memory
	bool retval = cart->readRom(romPath, verboseMode);

	// Check that the ROM is loaded and the window is open
	if (!retval || !gpu->getWindowStatus()){
		std::cout << sysError << "Failed to read input ROM file (" << romPath << ")." << std::endl;
		return false;
	}

	// Load save data (if available)
	if(autoLoadExtRam)
		readExternalRam();

	// Enable GBC features for original GB games.
	if(forceColor){
		if(!bGBCMODE)
			bGBCMODE = true;
		else // Disable force color for GBC games
			forceColor = false;
	}

	// Load the boot ROM (if available)
	bool loadBootROM = false;
	std::ifstream bootstrap;
	if(bGBCMODE){
		if(!gameboyColorBootRomPath.empty()){
			bootstrap.open(gameboyColorBootRomPath.c_str(), std::ios::binary);
			if(!bootstrap.good())
				std::cout << sysWarning << "Failed to load GBC boot ROM \"" << gameboyColorBootRomPath << "\"." << std::endl;
			else
				loadBootROM = true;
		}
	}
	else{
		if(!gameboyBootRomPath.empty()){
			bootstrap.open(gameboyBootRomPath.c_str(), std::ios::binary);
			if(!bootstrap.good())
				std::cout << sysWarning << "Failed to load GB boot ROM \"" << gameboyBootRomPath << "\"." << std::endl;
			else
				loadBootROM = true;
		}
	}
	
	if(loadBootROM){
		bootstrap.seekg(0, bootstrap.end);
		bootLength = (unsigned short)bootstrap.tellg();
		bootstrap.seekg(0);
		bootROM.reserve(bootLength);
		bootstrap.read((char*)bootROM.data(), bootLength); // Read the entire boot ROM at once
		bootstrap.close();
		std::cout << sysMessage << "Successfully loaded " << bootLength << " B boot ROM." << std::endl;
		cpu->setProgramCounter(0);
		bootSequence = true;
	}
	else{ // Initialize the system registers with default values.
		// Timer registers
		(*rTIMA)  = 0x00;
		(*rTMA)   = 0x00;
		(*rTAC)   = 0x00;
		
		// Sound processor registers
		(*rNR10)  = 0x80;
		(*rNR11)  = 0xBF;
		(*rNR12)  = 0xFE;
		(*rNR14)  = 0xBF;
		(*rNR21)  = 0x3F;
		(*rNR22)  = 0x00;
		(*rNR24)  = 0xBF;
		(*rNR30)  = 0x7F;
		(*rNR31)  = 0xFF;
		(*rNR32)  = 0x9F;
		(*rNR33)  = 0xBF;
		(*rNR41)  = 0xFF;
		(*rNR42)  = 0x00;
		(*rNR43)  = 0x00;
		(*rNR44)  = 0xBF;
		(*rNR50)  = 0x77;
		(*rNR51)  = 0xF3;
		(*rNR52)  = 0xF1;

		// GPU registers
		(*rLCDC)  = 0x91;
		(*rSCY)   = 0x00;
		(*rSCX)   = 0x00;
		(*rLYC)   = 0x00;
		(*rBGP)   = 0xFC;
		(*rOBP0)  = 0xFF;
		(*rOBP1)  = 0xFF;
		(*rWY)    = 0x00;
		(*rWX)    = 0x00;

		// Interrupt enable
		(*rIE)    = 0x00;

		// Undocumented registers (for completeness)
		(*rFF6C)  = 0xFE;
		(*rFF72)  = 0x00;
		(*rFF73)  = 0x00;
		(*rFF74)  = 0x00;
		(*rFF75)  = 0x8F;
		(*rFF76)  = 0x00;
		(*rFF77)  = 0x00;

		// Set the PC to the entry point of the program. Skip the boot sequence.
		cpu->setProgramCounter(cart->getProgramEntryPoint());
		
		// Disable the boot sequence
		bootSequence = false;
	}

	return true;
}

bool SystemGBC::screenshot(){
	std::cout << sysMessage << "Not implemented" << std::endl;
	return false;
}

bool SystemGBC::quicksave(const std::string& fname/*=""*/){
	std::cout << sysMessage << "Quicksaving... ";
	std::ofstream ofile;
	if(fname.empty())
		ofile.open((romFilename+".sav").c_str(), std::ios::binary);
	else
		ofile.open(fname.c_str(), std::ios::binary);
	if(!ofile.good()){
		std::cout << "FAILED!" << std::endl;
		return false;
	}

	unsigned int nBytesWritten = 0;
	
	unsigned char nVersion = SAVESTATE_VERSION;
	unsigned char nFlags = 0;
	bool cartRam = cart->hasRam();
	if(bGBCMODE) // CGB mode flag
		bitSet(nFlags, 0);
	if(cpuStopped) // STOP flag
		bitSet(nFlags, 1);
	if(cpuHalted) // HALT flag
		bitSet(nFlags, 2);
	if(cartRam) // Internal cartridge RAM flag
		bitSet(nFlags, 3);

	// Write the cartridge title and system flags
	ofile.write((char*)&nFlags, 1); // System flags
	ofile.write((char*)&nVersion, 1); // Savestate version number
	ofile.write(cart->getRawTitleString(), 12);
	ofile.write((char*)rIE->getConstPtr(), 1); // Interrupt enable
	ofile.write((char*)rIME->getConstPtr(), 1); // Master interrupt enable
	nBytesWritten += 16;

	// Write cartridge RAM (if enabled)
	if(cartRam)
		nBytesWritten += cart->getRam()->writeSavestate(ofile);

	// Write state of all system components
	for(auto comp = subsystems->list.cbegin(); comp != subsystems->list.cend(); comp++){
		nBytesWritten += comp->second->writeSavestate(ofile);
	}
	
	// Write system registers
	for(std::vector<Register>::const_iterator reg = registers.cbegin(); reg != registers.cend(); reg++){
		ofile.write((char*)reg->getConstPtr(), 1);
		nBytesWritten++;
	}
		
	ofile.close();
	std::cout << "DONE! Wrote " << nBytesWritten << " B" << std::endl;
	
	return true;
}

bool SystemGBC::quickload(const std::string& fname/*=""*/){
	std::cout << sysMessage << "Loading quicksave... ";
	std::ifstream ifile;
	if(fname.empty())
		ifile.open((romFilename+".sav").c_str(), std::ios::binary);
	else
		ifile.open(fname.c_str(), std::ios::binary);
	if(!ifile.good()){
		std::cout << "FAILED!" << std::endl;
		return false;
	}
	
	unsigned int nBytesRead = 0;

	char readTitle[12];
	unsigned char nVersion;
	unsigned char nFlags;

	// Read the cartridge title and system flags
	ifile.read((char*)&nFlags, 1); // System flags
	ifile.read((char*)&nVersion, 1); // Savestate version number
	ifile.read(readTitle, 12);
	ifile.read((char*)rIE->getConstPtr(), 1); // Interrupt enable
	ifile.read((char*)rIME->getConstPtr(), 1); // Master interrupt enable
	nBytesRead += 16;
	
	// Check incoming savestate version
	if(nVersion != SAVESTATE_VERSION){
		std::cout << sysWarning << "Unexpected savestate version number (" << getHex(nVersion) << " != " << getHex(SAVESTATE_VERSION) << ")" << std::endl;
	}
	
	bGBCMODE = bitTest(nFlags, 0); // CGB mode flag
	cpuStopped = bitTest(nFlags, 1); // STOP flag
	cpuHalted = bitTest(nFlags, 2); // HALT flag
	bool cartRam = bitTest(nFlags, 3); // Savestate contains internal cartridge RAM
	
	// Check the title against the title of the loaded ROM
	if(strcmp(readTitle, cart->getRawTitleString()) != 0){
		std::cout << sysWarning << "ROM title of quicksave does not match loaded ROM!" << std::endl;
	}

	// Copy cartridge RAM (if enabled)
	if(cartRam)
		nBytesRead += cart->getRam()->readSavestate(ifile);

	// Copy state of all system components
	for(auto comp = subsystems->list.cbegin(); comp != subsystems->list.cend(); comp++){
		nBytesRead += comp->second->readSavestate(ifile);
	}

	// Copy system registers
	for(std::vector<Register>::iterator reg = registers.begin(); reg != registers.end(); reg++){
		ifile.read((char*)reg->getPtr(), 1);
		nBytesRead++;
	}
		
	ifile.close();
	std::cout << "DONE! Read " << nBytesRead << " B" << std::endl;

	return true;
}

bool SystemGBC::writeExternalRam(){
	return saveSRAM(romFilename+".sram");
}

bool SystemGBC::readExternalRam(){
	return loadSRAM(romFilename+".sram");
}

void SystemGBC::help(){
	std::cout << "HELP: Press escape to exit program." << std::endl << std::endl;

	std::cout << " Button Map-" << std::endl;
	std::cout << "  Start = Enter" << std::endl;
	std::cout << " Select = Tab" << std::endl;
	std::cout << "      B = j" << std::endl;
	std::cout << "      A = k" << std::endl;
	std::cout << "     Up = w (up)" << std::endl;
	std::cout << "   Down = s (down)" << std::endl;
	std::cout << "   Left = a (left)" << std::endl;
	std::cout << "  Right = d (right)" << std::endl << std::endl;

	std::cout << " System Keys-" << std::endl;
	std::cout << "  F1 : Display this help screen" << std::endl;
	std::cout << "  F2 : Pause emulation" << std::endl;
	std::cout << "  F3 : Resume emulation" << std::endl;
	std::cout << "  F4 : Reset emulator" << std::endl;
	std::cout << "  F5 : Quicksave state" << std::endl;
	std::cout << "  F6 : Decrease frame-skip (slower)" << std::endl;
	std::cout << "  F7 : Increase frame-skip (faster)" << std::endl;
	std::cout << "  F8 : Save cart SRAM to \"sram.dat\"" << std::endl;
	std::cout << "  F9 : Quickload state" << std::endl;
	std::cout << "  F10: Start/stop midi recording" << std::endl;
	std::cout << "  F12: Take screenshot" << std::endl;
	std::cout << "   ` : Open interpreter console" << std::endl;
	std::cout << "   - : Decrease volume" << std::endl;
	std::cout << "   + : Increase volume" << std::endl;
	std::cout << "   f : Show/hide FPS counter on screen" << std::endl;
	std::cout << "   m : Mute output audio" << std::endl;
}

void SystemGBC::openDebugConsole(){
	gpu->getWindow()->setKeyboardStreamMode();
	consoleIsOpen = true;
	pause();
}

void SystemGBC::closeDebugConsole(){
	gpu->getWindow()->setKeyboardToggleMode();
	consoleIsOpen = false;
	unpause();
}

void SystemGBC::stepThrough(){
	unpause(false); // Do not resume audio
	pauseAfterNextInstruction = true;
}

void SystemGBC::advanceClock(){
	unpause(false); // Do not resume audio
	pauseAfterNextClock = true;
}

void SystemGBC::resumeUntilNextHBlank(){
	unpause(false); // Do not resume audio
	pauseAfterNextHBlank = true;
}

void SystemGBC::resumeUntilNextVBlank(){
	unpause(false); // Do not resume audio
	pauseAfterNextVBlank = true;
}

void SystemGBC::lockMemory(bool lockVRAM, bool lockOAM){
	bLockedVRAM = lockVRAM;
	bLockedOAM = lockOAM;
}

bool SystemGBC::writeRegister(const unsigned short &reg, const unsigned char &val){
	if(reg < REGISTER_LOW || reg > REGISTER_HIGH)
		return false;
	Register *ptr = &registers[reg - REGISTER_LOW];
	if(ptr->getSystemComponent()){ // Registers with an associated system component
		if(!ptr->getSystemComponent()->checkRegister(reg))
			return false;
		ptr->write(val); // Write the new register value.
		ptr->getSystemComponent()->writeRegister(reg, val);
	}
	else{ // Registers with no associated system component
		ptr->write(val); // Write the new register value.
		switch(reg){
			case 0xFF0F: // IF (Interrupt Flag)
				break;
			case 0xFF4D: // KEY1 (Speed switch register)
				break;
			case 0xFF50: // Enable/disable ROM boot sequence
				bootSequence = false;
				if(forceColor) // Disable GBC mode so that original GB games display correctly after boot.
					bGBCMODE = false;
				break;
			case 0xFF56: // RP (Infrared comms port (not used))
				break;
			default:
				return false;
		}
	}
	return true;
}

bool SystemGBC::readRegister(const unsigned short &reg, unsigned char &val){
	if(reg < REGISTER_LOW || reg > REGISTER_HIGH)
		return false;
	Register *ptr = &registers[reg - REGISTER_LOW];
	val = ptr->read();
	if(ptr->getSystemComponent()){ // Registers with an associated system component
		ptr->getSystemComponent()->readRegister(reg, val);	
	}
	else{ // Registers with no associated system component
		switch(reg){
			case 0xFF0F: // IF (Interrupt Flag)
				break;
			case 0xFF4D: // KEY1 (Speed switch register)
				break;
			case 0xFF56: // RP (Infrared comms port (not used))
				break;
			default:
				return false;
		}
	}
	return true; // Read register
}

void SystemGBC::checkSystemKeys(){
	KeyStates *keys = gpu->getWindow()->getKeypress();
	if(keys->empty()) 
		return;
	// Function keys
	if (keys->poll(0xF1))      // F1  Help
		help();
	else if (keys->poll(0xF2)) // F2  Pause emulation
		pause();
	else if (keys->poll(0xF3)) // F3  Resume emulation
		unpause();
	else if (keys->poll(0xF4)) // F4  Reset emulator
		reset();
	else if (keys->poll(0xF5)) // F5  Quicksave
		quicksave();
	else if (keys->poll(0xF6)) // F6  Decrease frame-skip (slower)
		frameSkip = (frameSkip > 1 ? frameSkip-1 : 1);
	else if (keys->poll(0xF7)) // F7  Increase freme-skip (faster)
		frameSkip++;
	else if (keys->poll(0xF8)) // F8  Save cartridge RAM to file
		writeExternalRam();
	else if (keys->poll(0xF9)) // F9  Quickload
		quickload();
	else if (keys->poll(0xFA)){ // F10 Start / stop midi recording
		if(sound->midiFileEnabled()){
			std::cout << sysMessage << "Finalizing MIDI recording." << std::endl;
			sound->stopMidiFile();
		}
		else{
			std::cout << sysMessage << "Starting MIDI recording." << std::endl;
			sound->startMidiFile("out.mid");
		}
	}
	else if (keys->poll(0xFB)) // F11 (No function)
		return;
	else if (keys->poll(0xFC)) // F12 Screenshot
		screenshot();
	else if (keys->poll(0x2D)) // '-'    Decrease volume
		sound->getMixer()->decreaseVolume();
	else if (keys->poll(0x3D)) // '=(+)' Increase volume
		sound->getMixer()->increaseVolume();
	else if (keys->poll(0x60)) // '`'   Open debugging console
		openDebugConsole();
	else if (keys->poll(0x66)) // 'f'    Display framerate
		displayFramerate = !displayFramerate;
	else if (keys->poll(0x6D)) // 'm'    Mute
		sound->getMixer()->mute();
}
