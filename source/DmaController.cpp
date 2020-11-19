#include <iostream>

#include "DmaController.hpp"
#include "SystemGBC.hpp"
#include "SystemRegisters.hpp"
#include "Support.hpp"

void DmaController::startTransferOAM(){
	// Source:      XX00-XX9F with XX in range [00,F1]
	// Destination: FE00-FE9F
	// DMA transfer takes 160 us (80 us in double speed) and CPU may only access HRAM during this interval
	index = 0;
	destStart = 0xFE00;
	srcStart = rDMA->getValue() << 8;
	nBytes = 1;
	nCyclesRemaining = 160;
	nBytesRemaining = 160;
	transferMode = false;
	oldDMA = true;
}

void DmaController::startTransferVRAM(){
	unsigned char dmaSourceH = rHDMA1->getValue();
	unsigned char dmaSourceL = rHDMA2->getBits(4,7); // Bits (4-7) of LSB of source address
	unsigned char dmaDestinationH = rHDMA3->getBits(0,4); // Bits (0-4) of MSB of destination address
	unsigned char dmaDestinationL = rHDMA4->getBits(4,7); // Bits (4-7) of LSB of destination address
					
	// Start a VRAM DMA transfer
	// source:      0000-7FF0 or A000-DFF0
	// destination: 8000-9FF0 (VRAM)
	index = 0;
	destStart = 0x8000 + ((dmaDestinationH & 0x00FF) << 8) + dmaDestinationL;
	srcStart = ((dmaSourceH & 0x00FF) << 8) + dmaSourceL;
	nBytes = 2; // VRAM DMA takes ~1 us per two bytes transferred

	// Number of bytes to transfer.
	nBytesRemaining = (rHDMA5->getBits(0,6) + 1) * 0x10;
	nCyclesRemaining = nBytesRemaining / nBytes;
	
	// Transfer mode:
	// 0: Transfer all bytes at once
	// 1: Transfer 16 bytes per HBlank
	transferMode = rHDMA5->getBit(7); // 0: General DMA, 1: H-Blank DMA
	if(transferMode)
		nCyclesRemaining = 0; // Only transfer after an HBlank

	oldDMA = false;
}

void DmaController::terminateTransfer(){
	// Only HBlank transfer is allowed to be terminated
	if(!nBytesRemaining || oldDMA || !transferMode) 
		return;
	nBytesRemaining = 0;
	nCyclesRemaining = 0;
	rHDMA5->setValue(0xFF);
}

bool DmaController::onClockUpdate(){
	if(!nCyclesRemaining)
		return false;
	transferByte();
	nCyclesRemaining--;
	if(!oldDMA){ // Update registers
		if(nBytesRemaining){
			// Number of bytes remaining
			rHDMA5->setValue(nBytesRemaining/16 - 1);
			(*rHDMA5) |= 0x80; // Set bit 7, indicating transfer still active
		}
		else
			rHDMA5->setValue(0xFF); // Transfer complete
		return true;
	}
	return false;
}

void DmaController::onHBlank(){
	if(transferMode && nBytesRemaining) // Transfer 16 bytes per HBlank
		nCyclesRemaining = (nBytesRemaining >= 16) ? 8 : (nBytesRemaining+1)/2;
}

void DmaController::transferByte(){
	unsigned char byte;
	for(unsigned short i = 0; i < nBytes; i++){
		if(!nBytesRemaining)
			break;
		// Read a byte from memory.
		sys->read(srcStart + index, byte);
		// Write it to a different location.
		sys->write(destStart + index, byte);
		nBytesRemaining--;
		index++;
	}
}

bool DmaController::writeRegister(const unsigned short &reg, const unsigned char &val){
	switch(reg){
		case 0xFF46: // DMA transfer from ROM/RAM to OAM
			if(!active())
				startTransferOAM();
			break;
		case 0xFF51: // HDMA1 - new DMA source, high byte (GBC only)
			break;
		case 0xFF52: // HDMA2 - new DMA source, low byte (GBC only)
			break;
		case 0xFF53: // HDMA3 - new DMA destination, high byte (GBC only)
			break;
		case 0xFF54: // HDMA4 - new DMA destination, low byte (GBC only)
			break;
		case 0xFF55: // HDMA5 - new DMA source, length/mode/start (GBC only)
			// Start a VRAM DMA transfer
			if(active() && !rHDMA5->getBit(7)){
				(*rHDMA5) |= 0x80; // ???
				terminateTransfer();
			}
			else{ // Start a transfer
				startTransferVRAM();
			}
			break;
		default:
			return false;
	}
	return true;
}

bool DmaController::readRegister(const unsigned short &reg, unsigned char &dest){
	switch(reg){
		case 0xFF46: // DMA transfer from ROM/RAM to OAM
			break;
		case 0xFF51: // HDMA1 - new DMA source, high byte (GBC only)
			break;
		case 0xFF52: // HDMA2 - new DMA source, low byte (GBC only)
			break;
		case 0xFF53: // HDMA3 - new DMA destination, high byte (GBC only)
			break;
		case 0xFF54: // HDMA4 - new DMA destination, low byte (GBC only)
			break;
		case 0xFF55: // HDMA5 - new DMA source, length/mode/start (GBC only)
			break;
		default:
			return false;
	}
	return true;
}

void DmaController::defineRegisters(){
	sys->addSystemRegister(this, 0x46, rDMA,   "DMA",   "22222222");
	sys->addSystemRegister(this, 0x51, rHDMA1, "HDMA1", "33333333");
	sys->addSystemRegister(this, 0x52, rHDMA2, "HDMA2", "33333333");
	sys->addSystemRegister(this, 0x53, rHDMA3, "HDMA3", "33333333");
	sys->addSystemRegister(this, 0x54, rHDMA4, "HDMA4", "33333333");
	sys->addSystemRegister(this, 0x55, rHDMA5, "HDMA5", "33333333");
}
