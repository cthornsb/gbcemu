#include <iostream>

#include "Support.hpp"
#include "SystemComponent.hpp"

void SystemComponent::connectSystemBus(SystemGBC *bus){ 
	sys = bus; 
	this->defineRegisters();
	this->userAddSavestateValues();
}

void SystemComponent::initialize(const unsigned short &nB, const unsigned short &N/*=1*/){
	mem = std::vector<std::vector<unsigned char> >(N, std::vector<unsigned char>(nB, 0x0));
	nBytes = nB;
	nBanks = N;
	bs = 0;
	size = nB*N;
}

SystemComponent::~SystemComponent(){
	mem.clear();
}

bool SystemComponent::write(const unsigned short &loc, const unsigned char *src){ 
	return write(loc, bs, (*src));
}

bool SystemComponent::write(const unsigned short &loc, const unsigned char &src){ 
	return write(loc, bs, src);
}

bool SystemComponent::write(const unsigned short &loc, const unsigned short &bank, const unsigned char *src){ 
	return write(loc, bank, (*src));
}

bool SystemComponent::write(const unsigned short &loc, const unsigned short &bank, const unsigned char &src){ 
	writeLoc = loc; 
	writeBank = bank;
	writeVal = src;
	if(readOnly || !preWriteAction())
		return false;
#ifdef USE_QT_DEBUGGER
	if (nBytes == 0 || (writeLoc - offset) >= nBytes || writeBank >= nBanks) {
		if (verboseMode) {
			std::cout << " Warning! Failed to write to memory address " << getHex(writeLoc) << std::endl;
		}
		return false;
	}
#endif // ifdef USE_QT_DEBUGGER
	mem[writeBank][writeLoc-offset] = writeVal;
	return true;		
}	

void SystemComponent::writeFast(const unsigned short &loc, const unsigned char &src){
	if(readOnly)
		return;
	mem[bs][loc-offset] = src;
}

void SystemComponent::writeFastBank0(const unsigned short &loc, const unsigned char &src){
	if(readOnly)
		return;
	mem[0][loc-offset] = src;
}

bool SystemComponent::read(const unsigned short &loc, unsigned char *dest){
	return read(loc, bs, (*dest));
}

bool SystemComponent::read(const unsigned short &loc, unsigned char &dest){ 
	return read(loc, bs, dest);
}

bool SystemComponent::read(const unsigned short &loc, const unsigned short &bank, unsigned char *dest){ 
	return read(loc, bank, (*dest));
}

bool SystemComponent::read(const unsigned short &loc, const unsigned short &bank, unsigned char &dest){ 
	readLoc = loc;
	readBank = bank;
	if(!preReadAction())
		return false;
#ifdef USE_QT_DEBUGGER
	if (nBytes == 0 || (readLoc - offset) >= nBytes || readBank >= nBanks) {
		if (verboseMode) {
			std::cout << " Warning! Failed to read from memory address " << getHex(readLoc) << std::endl;
		}
		return false;
	}
#endif // ifdef USE_QT_DEBUGGER
	dest = mem[readBank][readLoc-offset];
	return true;
}

void SystemComponent::readFast(const unsigned short &loc, unsigned char &dest){ 
	dest = mem[bs][loc-offset];
}

void SystemComponent::readFastBank0(const unsigned short &loc, unsigned char &dest){ 
	dest = mem[0][loc-offset];
}

void SystemComponent::print(const unsigned short bytesPerRow/*=10*/){
}

unsigned int SystemComponent::writeMemoryToFile(std::ofstream &f){
	if(!size)
		return 0;

	// Write memory contents to the output file.	
	/*unsigned int nWritten = 0;	
	for(unsigned short i = 0; i < nBanks; i++){
		f.write((char*)&mem[i][0], nBytes);
		nWritten += nBytes;
	}*/
	f.write((char*)&mem[0][0], size);

	return size;
}

unsigned int SystemComponent::readMemoryFromFile(std::ifstream &f){
	if(!size)
		return 0;

	// Write memory contents to the output file.
	/*unsigned int nRead = 0;	
	for(unsigned short i = 0; i < nBanks; i++){
		f.read((char*)&mem[i][0], nBytes);
		if(f.eof() || !f.good())
			return nRead;
		nRead += nBytes;
	}*/
	f.read((char*)&mem[0][0], size);

	return size;
}

unsigned int SystemComponent::writeSavestate(std::ofstream &f){
	unsigned int nWritten = 0; 
	nWritten += writeSavestateHeader(f); // Write the component header
	for(auto val = userValues.cbegin(); val != userValues.cend(); val++){
		f.write(static_cast<char*>(val->first), val->second);
		nWritten += val->second;
	}
	if(bSaveRAM)
		nWritten += writeMemoryToFile(f); // Write associated component RAM
	return nWritten;
}

unsigned int SystemComponent::readSavestate(std::ifstream &f){
	unsigned int nRead = 0;
	nRead += readSavestateHeader(f); // Read component header
	for(auto val = userValues.cbegin(); val != userValues.cend(); val++){
		f.read(static_cast<char*>(val->first), val->second);
		nRead += val->second;
	}
	if(bSaveRAM)
		nRead += readMemoryFromFile(f); // Read associated component RAM
	return nRead;
}

unsigned int SystemComponent::writeSavestateHeader(std::ofstream &f){
	f.write((char*)&nComponentID, 4);
	f.write((char*)&readOnly, 1);
	f.write((char*)&offset, 2);
	f.write((char*)&nBytes, 2);
	f.write((char*)&nBanks, 2);
	f.write((char*)&bs, 2);
	return 13;
}

unsigned int SystemComponent::readSavestateHeader(std::ifstream &f){
	bool readBackReadOnly;
	unsigned int readBackComponentID;
	unsigned short readBackOffset;
	unsigned short readBackBytes;
	unsigned short readBackBanks;
	f.read((char*)&readBackComponentID, 4);
	f.read((char*)&readBackReadOnly, 1);
	f.read((char*)&readBackOffset, 2);
	f.read((char*)&readBackBytes, 2);
	f.read((char*)&readBackBanks, 2);
	if(
		(readBackComponentID != nComponentID) ||
		(readBackReadOnly != readOnly) ||
		(readBackOffset != offset) ||
		(readBackBytes != nBytes) ||
		(readBackBanks != nBanks))
	{
		std::cout << " [SystemComponent] Warning! Signature of savestate does not match signature for component name=" << sName << std::endl;
		std::cout << " [SystemComponent]  Unstable behavior will likely occur" << std::endl;
	}
	f.read((char*)&bs, 2); // Read the bank select
	return 13;
}
