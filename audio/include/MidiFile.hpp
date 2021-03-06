#ifndef MIDI_FILE_HPP
#define MIDI_FILE_HPP

#include <fstream>
#include <vector>

namespace MidiFile{
	enum class MidiStatusType{
		NONE,
		RELEASED,      // 000 Note released
		PRESSED,       // 001 Note pressed
		POLYPRESSURE,  // 010 Aftertouch?
		CONTROLCHANGE, // 011 Control change
		PROGRAMCHANGE, // 100 Program change (midi instrument)
		CHANPRESSURE,  // 101 Channel pressure
		PITCHCHANGE,   // 110 Pitch wheel change
		CHANMESSAGE    // 011 Channel mode message
	};

	class MidiKey{
	public:
		/** Default constructor
		  */
		MidiKey() :
			bPressed(false),
			nChannel(0),
			nKeyNumber(0),
			nVelocity(0x40),
			nTime(0)
		{
		}

		/** Pressed key event
		  */
		MidiKey(const unsigned int& t, const unsigned char& ch, const unsigned char& key, const unsigned char& velocity = 0x40) :
			bPressed(true),
			nChannel(ch & 0x0f),
			nKeyNumber(key & 0x7f),
			nVelocity(velocity & 0x7f),
			nTime(t)
		{
		}

		/** Equality operator
		  */
		bool operator == (bool rhs) const {
			return (bPressed == rhs);
		}

		/** Equality operator
		  */
		bool operator != (bool rhs) const {
			return (bPressed != rhs);
		}

		/** Return true if the midi key is pressed down
		  */
		bool isPressed() const {
			return bPressed;
		}
		
		/** Get the current midi channel number (0 to 15)
		  */
		unsigned char getChannel() const {
			return nChannel;
		}
		
		/** Get the current midi key number (0 to 127)
		  */
		unsigned char getKeyNumber() const {
			return nKeyNumber;
		}
		
		/** Get the current velocity of a midi key press (0 to 127, 64 is default)
		  */
		unsigned char getKeyVelocity() const {
			return nVelocity;
		}
		
		/** Get the current midi clock time
		  */
		unsigned int getTime() const {
			return nTime;
		}
		
		/** Set to note press event
		  */
		virtual void press(){
			bPressed = true;
		}
		
		/** Set to note release event
		  */
		virtual void release(){
			bPressed = false;
		}
		
		/** Set the current midi channel number (0 to 15)
		  */ 
		void setChannel(const unsigned char& channel){
			nChannel = (channel & 0xf); // 0 to 15
		}
		
		/** Set the current midi key number (0 to 127)
		  */
		void setKeyNumber(const unsigned char& key){
			nKeyNumber = key;
		}
		
		/** Set the current velocity of a midi key press (0 to 127, 64 is default)
		  */
		void setKeyVelocity(const unsigned char& velocity){
			nVelocity = velocity;
		}		
		
		/** Set the event time in midi clock ticks
		  */
		void setTime(const unsigned int& t){
			nTime = t;
		}
	
	protected:
		bool bPressed; ///< Flag indiciating whether or not this event represents a key being (1) pressed or (0) released
	
		unsigned char nChannel; ///< Midi channel (0 to 15)
	
		unsigned char nKeyNumber; ///< Midi key number (0 to 127)
		
		unsigned char nVelocity; ///< Midi key velocity (0 to 127)
		
		unsigned int nTime; ///< Midi event time in midi clock ticks
	};

	class MidiKeyboard {
	public:
		/** Default constructor
		  */
		MidiKeyboard();
		
		/** Get the midi key whose ideal frequency is closest to the input frequency
		  */
		unsigned char operator () (const float& freq) const {
			unsigned char retval;
			getKeyNumber(freq, retval);
			return retval;
		}
		
		/** Get the midi key whose ideal frequency is closest to the input frequency
		  * @return The absolute difference between the ideal frequency and the input frequency
		  */
		float getKeyNumber(const float& freq, unsigned char& key) const;
		
	private:
		std::vector<float> frequencies; ///< The midi note map has 128 notes, C-1 to G9
	};

	class MidiChunk{
	public:
		/** Default constructor
		  */
		MidiChunk() :
			sType("    "),
			nLength(0),
			nIndex(0)
		{
		}
		
		/** Read a midi chunk from an input stream
		  */
		MidiChunk(std::ifstream& f) :
			MidiChunk()
		{
			readMidiChunk(f);
		}
		
		/** Equality operator
		  */
		bool operator == (const std::string& name) const {
			return (name == sType);
		}
		
		/** Inequality operator
		  */
		bool operator != (const std::string& name) const {
			return (name != sType);
		}
		
		/** Direct chunk payload access
		  */
		unsigned char& operator [] (const size_t& index){
			return data[index];
		}
		
		/** Direct chunk payload access
		  */
		unsigned char operator [] (const size_t& index) const {
			return data[index];
		}
		
		/** Get the type string of the chunk
		  */
		std::string getType() const {
			return sType;
		}
		
		/** Return true if chunk payload is empty
		  */
		bool empty() const {
			return (nLength == 0);
		}
		
		/** Get total length of chunk data (in bytes)
		  */
		unsigned short getLength() const {
			return nLength;
		}
		
		/** Get current byte index which will be read
		  */
		unsigned short getIndex() const {
			return nIndex;
		}		
		
		/** Get number of bytes remaining to be read
		  */
		unsigned short getBytesRemaining() const {
			return (nLength - nIndex);
		}

		/** Skip the next N bytes
		  */
		void skipBytes(const unsigned int& N=1){
			nIndex += N;
		}

		/** Peek at the next byte but do not advance the byte index
		  */
		unsigned char peekByte() const {
			return data[nIndex];
		}

		/** Get an unsigned char from chunk data
		  * @return True if data retrieved successfully and return false if there was not enough data to read
		  */
		bool getUChar(unsigned char& val);
		
		/** Get an unsigned short from chunk data
		  * @return True if data retrieved successfully and return false if there was not enough data to read
		  */
		bool getUShort(unsigned short& val);
		
		/** Get an unsigned int from chunk data
		  * @return True if data retrieved successfully and return false if there was not enough data to read
		  */
		bool getUInt(unsigned int& val);
		
		/** Get a string of length len
		  * @return True if string retrieved successfully and return false if there were not enough bytes
		  */
		bool getString(std::string& str, const unsigned int& len);
		
		/** Copy chunk data into a generic destination pointer
		  * @return True if data retrieved successfully and return false if there were not enough bytes
		  */
		bool copyMemory(void* dest, const unsigned int& len);
		
		/** Set the midi chunk type string
		  */
		void setType(const std::string& type){
			sType = type;
		}
		
		/** Push a byte onto the chunk
		  */
		void pushUChar(const unsigned char& val);
		
		/** Push a short onto the chunk
		  * Bytes are reversed automatically before being pushed
		  */
		void pushUShort(const unsigned short& val);
		
		/** Push an int onto the chunk
		  * Bytes are reversed automatically before being pushed
		  */
		void pushUInt(const unsigned int& val);
		
		/** Push a string onto the chunk
		  */
		void pushString(const std::string& str);
		
		/** Push 1-4 bytes onto the chunk
		  * Bytes are reversed automatically before being pushed
		  */
		void pushMemory(const unsigned int& src, const unsigned int& len);
		
		/** Push a variable size integer onto the chunk
		  * Between one and four 7-bit bytes will be pushed onto the chunk, depending on
		  * size of the input integer value. Maximum size of input is 28 bits (0x0fffffff). 
		  */
		void pushVariableSize(const unsigned int& val);
		
		/** Read a midi chunk from an input stream
		  */
		bool readMidiChunk(std::ifstream& f);
		
		/** Write a midi chunk t an ouptut stream
		  */
		bool writeMidiChunk(std::ofstream& f);
		
		/** Clear chunk properties
		  */
		void clear();
	
		/** Read a variable size integer from the chunk
		  * Between one and four 7-bit bytes will be read, depending on the size of the
		  * integer value in the chunk. The maximum size which can be read is 28 bits (0x0fffffff).
		  */
		static unsigned int readVariableLength(MidiChunk& chunk);
	
	private:
		std::string sType; ///< Midi chunk type
	
		unsigned int nLength; ///< Midi chunk length (in bytes, excluding 8 byte header)
		
		unsigned int nIndex; ///< Current byte read index in chunk payload
		
		std::vector<unsigned char> data; ///< Midi chunk payload
	}; // class MidiChunk
	
	class MidiMessage : public MidiKey {
	public:
		/** Default constructor
		  */
		MidiMessage() :
			MidiKey(),
			nDeltaTime(0),
			nStatus(MidiStatusType::NONE)
		{
		}

		/** Pressed key event
		  */
		MidiMessage(const MidiKey& key, const unsigned int& prevTime) :
			MidiKey(key),
			nDeltaTime(nTime - prevTime),
			nStatus(MidiStatusType::PRESSED)
		{
		}

		/** Chunk constructor (read)
		  */
		MidiMessage(MidiChunk& chunk) :
			MidiKey(),
			nDeltaTime(0),
			nStatus(MidiStatusType::NONE)
		{
			read(chunk);
		}

		/** Get the current midi message status
		  */
		MidiStatusType getStatus() const {
			return nStatus;
		}

		/** Get the delta-time of the midi message
		  */
		unsigned int getDeltaTime() const {
			return nDeltaTime;
		}
		
		/** Set the current midi message status
		  */
		void setStatus(const MidiStatusType& status){
			nStatus = status;
		}
		
		/** Set midi event delta-time
		  * @param t Time of previous event
		  */
		void setDeltaTime(const unsigned int& t){
			nDeltaTime = nTime - t;
		}

		/** Set to note press event
		  */
		void press() override {
			bPressed = true;
			nStatus = MidiStatusType::PRESSED;
		}

		/** Set to note release event
		  */
		void release() override {
			bPressed = false;
			nStatus = MidiStatusType::RELEASED;
		}

		/** Read a midi message event from an input track chunk
		  */
		bool read(MidiChunk& chunk);
		
		/** Write a midi message event to an output track chunk
		  */
		bool write(MidiChunk& chunk);
		
	private:
		unsigned int nDeltaTime; ///< Number of midi clock ticks since last midi message
	
		MidiStatusType nStatus; ///< Midi status [0, 15]
	}; // class MidiMessage
	
	class MidiSysExclusive{
	public:
		/** Default constructor
		  */
		MidiSysExclusive() :
			nType(0),
			nLength(0)
		{
		}
		
		/** Chunk constructor
		  */
		MidiSysExclusive(MidiChunk& chunk) :
			MidiSysExclusive()
		{
			read(chunk);
		}
		
		/** Read a sys-exclusive event from an input track chunk
		  */
		bool read(MidiChunk& chunk);
	
	private:
		unsigned char nType; ///< Event type
	
		unsigned int nLength; ///< Variable length
	}; // class MidiSysExclusive
	
	class MidiMetaEvent{
	public:
		/** Default constructor
		  */
		MidiMetaEvent() :
			nType(0),
			nLength(0)
		{
		}
		
		/** Chunk constructor
		  */
		MidiMetaEvent(MidiChunk& chunk) :
			MidiMetaEvent()
		{
			read(chunk);
		}

		/** Read a meta event from a midi chunk
		  */
		bool read(MidiChunk& chunk);

	private:
		unsigned char nType; ///< Meta event type
		
		unsigned int nLength; ///< Variable length
	}; // class MidiMetaEvent
	
	class TrackEvent{
	public:
		/** Default constructor
		  */
		TrackEvent() :
			nProgramNumber(0),
			nStartTime(0xffffffff),
			nPrevTime(0),
			nDeltaTime(0)
		{
		}
		
		/** Chunk constructor
		  */
		TrackEvent(MidiChunk& chunk) :
			TrackEvent()
		{
			read(chunk);
		}
		
		/** Get the current midi program number (instrument type)
		  */
		unsigned char getProgramNumber() const {
			return nProgramNumber;
		}
		
		/** Get the earliest midi clock time
		  */
		unsigned int getStartTime() const {
			return nStartTime;
		}
		
		/** Set midi program number (instrument type)
		  */
		void setProgramNumber(const unsigned char& prog){
			nProgramNumber = prog;
		}

		/** Read a midi track event from an input track chunk
		  * @return True if at least two bytes remained in the chunk and return false otherwise
		  */
		bool read(MidiChunk& chunk);

	private:
		unsigned char nProgramNumber; ///< Midi program number (instrument)
	
		unsigned int nStartTime; ///< Start of midi time
	
		unsigned int nPrevTime; ///< Time of most recent note
		
		unsigned int nDeltaTime; ///< Midi event delta-time read from midi file
	}; // class TrackEvent

	class MidiFileReader{
	public:
		/** Default file constructor
		  */
		MidiFileReader();

		/** Midi filename constructor
		  */
		MidiFileReader(const std::string& filename, const std::string& title = "");

		/** Destructor
		  */	
		~MidiFileReader() { }
		
		/** Set the input clock to output midi clock conversion factor (M) such that Tin = Tmidi * M
		  */
		void setClockMultiplier(const float& clk) {
			fClockMultiplier = clk;
		}

		/** Add a pressed note event
		  * @param chan Audio channel (1, 2, 3, or 4)
		  * @param t Absolute time in midi ticks
		  * @param freq Audio frequency of note (the midi note with the closest matching ideal frequency will be used)
		  */
		void press(const unsigned char& ch, const unsigned int& t, const float& freq);

		/** Release the key currently pressed on the specified audio channel (1, 2, 3, or 4)
		  * If no keys are pressed, do nothing
		  */
		void release(const unsigned char& ch, const unsigned int& t);
		
		/** Set the midi program number (instrument)
		  * @param ch Midi channel [0, 15]
		  * @param nPC Midi program number [0, 127]
		  */
		void setMidiInstrument(const unsigned char& ch, const unsigned char& nPC);

		/** Finalize the midi track chunk and prepare it for writing to disk
		  * @param t Absolute final time in midi ticks
		  */
		void finalize(const unsigned int& t);

		/** Read an input midi file with the specified filename
		  * @param filename Path to midi file (uses sFilename if filename not specified)
		  */
		bool read(const std::string& filename="");

		/** Finalize midi file and write it to disk with a specified filename
		  * @param filename Path to midi file (uses sFilename if filename not specified)
		  */
		bool write(const std::string& filename="");
		
		/** Print midi file header information
		  */
		void print();

	private:
		bool bFirstNote; ///< The next midi note event will be the first
	
		bool bFinalized; ///< The midi track chunk has been finalized and is ready for writing

		unsigned int nTime; ///< Global midi clock tick counter
	
		unsigned short nFormat; ///< Midi file format
		
		unsigned short nTracks; ///< Number of midi track chunks in file
		
		unsigned short nDivision; ///< Meaning of delta-times
		
		unsigned short nDeltaTicksPerQuarter; ///< Number of delta-time ticks per quarter note
		
		float fClockMultiplier; ///< Input clock tick to midi clock conversion factor

		std::string sFilename; ///< Input/output midi filename
		
		std::string sTrackname; ///< Midi track title
		
		MidiKeyboard notemap; ///< Midi note dictionary

		MidiChunk header; ///< Midi header chunk

		MidiChunk track; ///< Midi track chunk

		MidiKey bNotePressed[16]; ///< A note is currently being played

		/** Read midi file header (MThd=0x4d546864)
		  */
		bool readHeaderChunk(MidiChunk& hdr);
		
		/** Read midi file track chunk (MTrk=0x4d54726b)
		  */
		bool readTrackChunk(MidiChunk& chunk);

		/** Set midi header
		  * @param div Number of midi clock ticks per quarter note
		  */
		void midiHeader(const unsigned short& div = 24);

		/** Set the midi track name string
		  */
		void midiTrackName(const std::string& str);

		/** [broken] Set midi tempo in beats per minute (bpm)
		  */
		void midiTempo(const unsigned short& bpm = 120);

		/** Set the midi time signature
		  * @param nn Time signature numerator
		  * @param dd Time signature denominator
		  * @param cc Number of midi clocks per metronome tick
		  * @param bb Number of 1/32 notes per 24 midi clock ticks
		  */
		void midiTimeSignature(const unsigned char& nn = 4, const unsigned char& dd = 4, const unsigned char& cc = 24, const unsigned char& bb = 8);

		/** Set the midi key signature
		  * @param sf Number of sharps or flats (0 is C key)
		  * @param minor Is this a minor key
		  */
		void midiKeySignature(const unsigned char& sf = 0, bool minor = false);

		/** Finalize the midi track chunk
		  * This flag is required by the midi format
		  */
		void midiEndOfTrack();
	}; // class MidiFileReader
} // namespace MidiFileReader

#endif
