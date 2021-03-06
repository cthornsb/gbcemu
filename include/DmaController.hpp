#ifndef DMA_CONTROLLER_HPP
#define DMA_CONTROLLER_HPP

#include "SystemComponent.hpp"

class Register;

class DmaController : public SystemComponent {
public:
	DmaController() : 
		SystemComponent("DMA", 0x20414d44), // "DMA " 
		transferMode(0), 
		oldDMA(1), 
		nBytesRemaining(0), 
		nCyclesRemaining(0), 
		index(0), 
		nBytes(1), 
		srcStart(0), 
		destStart(0),
		length(0),
		currentCycle(0) { }

	bool active() const ;

	/** Get the current DMA transfer mode.
	  * @return 0 for OAM transfer, 1 for General VRAM transfer, 2 for HBlank VRAM transfer.
	  */
	unsigned char getTransferMode() const {
		return (oldDMA ? 0 : (!transferMode ? 1 : 2));
	}
	
	unsigned short getNumBytesRemaining() const {
		return nBytesRemaining;
	}

	unsigned short getNumCyclesRemaining() const {
		return nCyclesRemaining;
	}

	unsigned short getNumBytesPerCycle() const {
		return nBytes;
	}
	
	unsigned short getCurrentMemoryIndex() const {
		return index;
	}

	unsigned short getTotalLength() const {
		return length;
	}

	unsigned short getSourceStartAddress() const {
		return srcStart;
	}
	
	unsigned short getSourceEndAddress() const {
		return srcStart+length;
	}

	unsigned short getDestinationStartAddress() const {
		return destStart;
	}
	
	unsigned short getDestinationEndAddress() const {
		return destStart+length;
	}

	void startTransferOAM();
	
	void startTransferVRAM();
	
	/** Stop a DMA transfer which is in progress.
	  * Only applies to HBlank DMA (transferMode==1).
	  */
	void terminateTransfer();

	// The system timer has no associated RAM, so return false.
	bool preWriteAction() override { 
		return false; 
	}
	
	// The system timer has no associated RAM, so return false.
	bool preReadAction() override { 
		return false; 
	}
	
	bool onClockUpdate() override ;

	bool writeRegister(const unsigned short &reg, const unsigned char &val) override ;

	bool readRegister(const unsigned short &reg, unsigned char &dest) override ;

	void defineRegisters() override ;
	
	void onHBlank();

private:
	bool transferMode; ///< 0: General DMA, 1: H-Blank DMA
	
	bool oldDMA; ///< 0: Old DMA (OAM), 1: New DMA (VRAM)
	
	unsigned short nBytesRemaining; ///< Remaining number of bytes to transfer
	
	unsigned short nCyclesRemaining; ///< The number of system clock cycles remaining.
	
	unsigned short index; ///< Current index in system memory.
	
	unsigned short nBytes; ///< The number of bytes transferred per cycle.
	
	unsigned short srcStart; ///< Start location of the source block in memory.
	
	unsigned short destStart; ///< Start location of the destination block in memory.
	
	unsigned short length; ///< Total number of bytes to transfer.
	
	unsigned short currentCycle; ///< Current clock cycle number since HDMA transfer began.
	
	/** Transfer the next chunk of bytes. Increment the memory index by nBytes.
	  */
	void transferByte();
	
	/** Add elements to a list of values which will be written to / read from an emulator savestate
	  */
	void userAddSavestateValues() override;
};

#endif
