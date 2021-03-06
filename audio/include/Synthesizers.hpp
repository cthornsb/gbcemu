#ifndef SYNTHESIZERS_HPP
#define SYNTHESIZERS_HPP

#include "AudioSampler.hpp"

namespace Synthesizers{
	class SimpleSynth : public AudioSampler {
	public:
		SimpleSynth() : 
			AudioSampler(),
			fAmplitude(1.f),
			fFrequency(440.f),
			fPeriod(1.f / 440.f)
		{
		}
		
		SimpleSynth(SoundManager* parent) : 
			AudioSampler(),
			fAmplitude(1.f),
			fFrequency(440.f),
			fPeriod(1.f / 440.f)
		{
		}
		
		~SimpleSynth() { }
		
		void setAmplitude(const float& A){ fAmplitude = (A <= 1.f ? A : 1.f); }
		
		void setFrequency(const PianoKeys::Key& key, const int& octave=4){
			setFrequency(PianoKeys::getFrequency(key, PianoKeys::Modifier::NONE, octave));
		}
		
		void setFrequency(const PianoKeys::Key& key, const PianoKeys::Modifier& mod, const int& octave=4){
			setFrequency(PianoKeys::getFrequency(key, mod, octave));
		}
		
		void setFrequency(const float& freq){
			fFrequency = freq;
			fPeriod = 1.f / freq;
		}
		
		float getAmplitude() const { return fAmplitude; }
		
		/** Return the current frequency (in Hz)
		  */
		float getFrequency() const { return fFrequency; }
		
		/** Return the current period (in s)
		  */
		float getPeriod() const { return (1.f / fFrequency); }
		
		float sample(const float& dt){
			return (fAmplitude * clamp(userSample(fPhase += dt)));
		}

	protected:
		float fAmplitude;
		float fFrequency;
		float fPeriod;
		
		virtual float userSample(const float& dt) = 0;
	};
	
	class SineWave : public SimpleSynth {
	public:
		SineWave() :
			SimpleSynth()
		{
		}

	protected:
		/** Sample the sine wave at a specified phase
		  */
		virtual float userSample(const float& dt);
	};

	class TriangleWave : public SimpleSynth {
	public:
		TriangleWave() :
			SimpleSynth()
		{
		}
		
	protected:
		/** Sample the wave at a specified phase
		  */
		virtual float userSample(const float& dt);
	};	
	
	class SquareWave : public SimpleSynth {
	public:
		SquareWave() :
			SimpleSynth(),
			nHarmonics(10)
		{
		}

	protected:
		int nHarmonics; ///< Maximum harmonic number
		
		/** Sample the wave at a specified phase
		  */
		virtual float userSample(const float& dt);
	};
		
	class SawtoothWave : public SimpleSynth {
	public:
		SawtoothWave() :
			SimpleSynth(),
			nHarmonics(10)
		{
		}

	protected:
		int nHarmonics; ///< Maximum harmonic number
	
		/** Sample the wave at a specified phase
		  */
		virtual float userSample(const float& dt);
	};
};

#endif
