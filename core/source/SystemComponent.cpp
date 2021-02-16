#include <iostream>

#include "Support.hpp"
#include "SystemComponent.hpp"

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

bool SystemComponent::write(const unsigned short &loc, unsigned char *src){ 
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
			std::cerr << " Warning! Failed to write to memory address " << getHex(writeLoc) << std::endl;
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
			std::cerr << " Warning! Failed to read from memory address " << getHex(readLoc) << std::endl;
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
	unsigned int nWritten = 0;	
	for(unsigned short i = 0; i < nBanks; i++){
		f.write((char*)&mem[i][0], nBytes);
		nWritten += nBytes;
	}

	return nWritten;
}

unsigned int SystemComponent::readMemoryFromFile(std::ifstream &f){
	if(!size)
		return 0;

	// Write memory contents to the output file.
	unsigned int nRead = 0;	
	for(unsigned short i = 0; i < nBanks; i++){
		f.read((char*)&mem[i][0], nBytes);
		if(f.eof() || !f.good())
			return nRead;
		nRead += nBytes;
	}

	return nRead;
}

unsigned int SystemComponent::writeSavestate(std::ofstream &f){
	unsigned int nWritten = 9; // The header is 9 bytes long
	writeSavestateHeader(f);
	nWritten += writeMemoryToFile(f);
	return nWritten;
}

unsigned int SystemComponent::readSavestate(std::ifstream &f){
	unsigned int nRead = 9; // The header is 9 bytes long
	readSavestateHeader(f);
	nRead += readMemoryFromFile(f);
	return nRead;
}

void SystemComponent::writeSavestateHeader(std::ofstream &f){
	f.write((char*)&readOnly, 1);
	f.write((char*)&offset, 2);
	f.write((char*)&nBytes, 2);
	f.write((char*)&nBanks, 2);
	f.write((char*)&bs, 2);
}

void SystemComponent::readSavestateHeader(std::ifstream &f){
	f.read((char*)&readOnly, 1);
	f.read((char*)&offset, 2);
	f.read((char*)&nBytes, 2);
	f.read((char*)&nBanks, 2);
	f.read((char*)&bs, 2);
	size = nBytes*nBanks;
}