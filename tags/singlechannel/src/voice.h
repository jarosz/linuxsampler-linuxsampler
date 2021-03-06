/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003 by Benno Senoner and Christian Schoenebeck         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston,                 *
 *   MA  02111-1307  USA                                                   *
 ***************************************************************************/

#ifndef __VOICE_H__
#define __VOICE_H__

#include "global.h"
#include "rtmath.h"
#include "diskthread.h"
#include "ringbuffer.h"
#include "stream.h"
#include "gig.h"
#include "eg_vca.h"
#include "eg_d.h"
#include "rtelmemorypool.h"
#include "audiothread.h"
#include "filter.h"
#include "lfo.h"

#define USE_LINEAR_INTERPOLATION	1  ///< set to 0 if you prefer cubic interpolation (slower, better quality)
#define ENABLE_FILTER			0  ///< if set to 0 then filter (VCF) code is ignored on compile time
#define FILTER_UPDATE_PERIOD		64 ///< amount of sample points after which filter parameters (cutoff, resonance) are going to be updated (higher value means less CPU load, but also worse parameter resolution)
#define FORCE_FILTER_USAGE		0  ///< if set to 1 then filter is always used, if set to 0 filter is used only in case the instrument file defined one
#define FILTER_CUTOFF_MAX		10000.0f ///< maximum cutoff frequency (10kHz)
#define FILTER_CUTOFF_MIN		100.0f   ///< minimum cutoff frequency (100Hz)

// Uncomment following line to override external cutoff controller
//#define OVERRIDE_FILTER_CUTOFF_CTRL	1  ///< set to an arbitrary MIDI control change controller (e.g. 1 for 'modulation wheel')

// Uncomment following line to override external resonance controller
//#define OVERRIDE_FILTER_RES_CTRL	91  ///< set to an arbitrary MIDI control change controller (e.g. 91 for 'effect 1 depth')

// Uncomment following line to override filter type
//#define OVERRIDE_FILTER_TYPE		gig::vcf_type_lowpass  ///< either gig::vcf_type_lowpass, gig::vcf_type_bandpass or gig::vcf_type_highpass


/// Reflects a MIDI controller
struct midi_ctrl {
    uint8_t controller; ///< MIDI control change controller number
    uint8_t value;      ///< Current MIDI controller value
    float   fvalue;     ///< Transformed / effective value (e.g. volume level or filter cutoff frequency)
};

class Voice {
    public:
        // Attributes
        int                 MIDIKey;          ///< MIDI key number of the key that triggered the voice
        uint                ReleaseVelocity;  ///< Reflects the release velocity value if a note-off command arrived for the voice.

        // Static Attributes
        static DiskThread*  pDiskThread;  ///< Pointer to the disk thread, to be able to order a disk stream and later to delete the stream again
        static AudioThread* pEngine;      ///< Pointer to the engine, to be able to access the event lists.

        // Methods
        Voice();
       ~Voice();
        void Kill();
        void Render(uint Samples);
        void Reset();
        int  Trigger(ModulationSystem::Event* pNoteOnEvent, int PitchBend, gig::Instrument* pInstrument);
        inline bool IsActive()                                              { return Active; }
        inline void SetOutputLeft(float* pOutput, uint MaxSamplesPerCycle)  { this->pOutputLeft  = pOutput; this->MaxSamplesPerCycle = MaxSamplesPerCycle; }
        inline void SetOutputRight(float* pOutput, uint MaxSamplesPerCycle) { this->pOutputRight = pOutput; this->MaxSamplesPerCycle = MaxSamplesPerCycle; }
    private:
        // Types
        enum playback_state_t {
            playback_state_ram,
            playback_state_disk,
            playback_state_end
        };

        // Attributes
        float                Volume;             ///< Volume level of the voice
        float*               pOutputLeft;        ///< Audio output buffer (left channel)
        float*               pOutputRight;       ///< Audio output buffer (right channel)
        uint                 MaxSamplesPerCycle; ///< Size of each audio output buffer
        double               Pos;                ///< Current playback position in sample
        double               PitchBase;          ///< Basic pitch depth, stays the same for the whole life time of the voice
        double               PitchBend;          ///< Current pitch value of the pitchbend wheel
        gig::Sample*         pSample;            ///< Pointer to the sample to be played back
        gig::Region*         pRegion;            ///< Pointer to the articulation information of the respective keyboard region of this voice
        bool                 Active;             ///< If this voice object is currently in usage
        playback_state_t     PlaybackState;      ///< When a sample will be triggered, it will be first played from RAM cache and after a couple of sample points it will switch to disk streaming and at the end of a disk stream we have to add null samples, so the interpolator can do it's work correctly
        bool                 DiskVoice;          ///< If the sample is very short it completely fits into the RAM cache and doesn't need to be streamed from disk, in that case this flag is set to false
        Stream::reference_t  DiskStreamRef;      ///< Reference / link to the disk stream
        unsigned long        MaxRAMPos;          ///< The upper allowed limit (not actually the end) in the RAM sample cache, after that point it's not safe to chase the interpolator another time over over the current cache position, instead we switch to disk then.
        bool                 RAMLoop;            ///< If this voice has a loop defined which completely fits into the cached RAM part of the sample, in this case we handle the looping within the voice class, else if the loop is located in the disk stream part, we let the disk stream handle the looping
        int                  LoopCyclesLeft;     ///< In case there is a RAMLoop and it's not an endless loop; reflects number of loop cycles left to be passed
        uint                 Delay;              ///< Number of sample points the rendering process of this voice should be delayed (jitter correction), will be set to 0 after the first audio fragment cycle
        EG_VCA*              pEG1;               ///< Envelope Generator 1 (Amplification)
        EG_VCA*              pEG2;               ///< Envelope Generator 2 (Filter cutoff frequency)
        EG_D*                pEG3;               ///< Envelope Generator 3 (Pitch)
        GigFilter            FilterLeft;
        GigFilter            FilterRight;
        midi_ctrl            VCFCutoffCtrl;
        midi_ctrl            VCFResonanceCtrl;
        int                  FilterUpdateCounter; ///< Used to update filter parameters all FILTER_UPDATE_PERIOD samples
        static const float   FILTER_CUTOFF_COEFF;
        LFO<VCAManipulator>* pLFO1;              ///< Low Frequency Oscillator 1 (Amplification)
        LFO<VCFCManipulator>* pLFO2;             ///< Low Frequency Oscillator 2 (Filter cutoff frequency)
        LFO<VCOManipulator>* pLFO3;              ///< Low Frequency Oscillator 3 (Pitch)
        ModulationSystem::Event* pTriggerEvent;  ///< First event on the key's list the voice should process (only needed for the first audio fragment in which voice was triggered, after that it will be set to NULL).

        // Static Methods
        static float CalculateFilterCutoffCoeff();

        // Methods
        void        ProcessEvents(uint Samples);
        void        Interpolate(uint Samples, sample_t* pSrc, uint Skip);
        void        InterpolateAndLoop(uint Samples, sample_t* pSrc, uint Skip);
        inline void InterpolateOneStep_Stereo(sample_t* pSrc, int& i, float& effective_volume, float& pitch, float& cutoff, float& resonance) {
            int   pos_int   = RTMath::DoubleToInt(this->Pos);  // integer position
            float pos_fract = this->Pos - pos_int;             // fractional part of position
            pos_int <<= 1;

            #if ENABLE_FILTER
                UpdateFilter_Stereo(cutoff + FILTER_CUTOFF_MIN, resonance);
            #endif // ENABLE_FILTER

            #if USE_LINEAR_INTERPOLATION
                #if ENABLE_FILTER
                    // left channel
                    this->pOutputLeft[i]    += this->FilterLeft.Apply(effective_volume * (pSrc[pos_int]   + pos_fract * (pSrc[pos_int+2] - pSrc[pos_int])));
                    // right channel
                    this->pOutputRight[i++] += this->FilterRight.Apply(effective_volume * (pSrc[pos_int+1] + pos_fract * (pSrc[pos_int+3] - pSrc[pos_int+1])));
                #else // no filter
                    // left channel
                    this->pOutputLeft[i]    += effective_volume * (pSrc[pos_int]   + pos_fract * (pSrc[pos_int+2] - pSrc[pos_int]));
                    // right channel
                    this->pOutputRight[i++] += effective_volume * (pSrc[pos_int+1] + pos_fract * (pSrc[pos_int+3] - pSrc[pos_int+1]));
                #endif // ENABLE_FILTER
            #else // polynomial interpolation
                // calculate left channel
                float xm1 = pSrc[pos_int];
                float x0  = pSrc[pos_int+2];
                float x1  = pSrc[pos_int+4];
                float x2  = pSrc[pos_int+6];
                float a   = (3 * (x0 - x1) - xm1 + x2) / 2;
                float b   = 2 * x1 + xm1 - (5 * x0 + x2) / 2;
                float c   = (x1 - xm1) / 2;
                #if ENABLE_FILTER
                    this->pOutputLeft[i] += this->FilterLeft.Apply(effective_volume * ((((a * pos_fract) + b) * pos_fract + c) * pos_fract + x0));
                #else // no filter
                    this->pOutputLeft[i] += effective_volume * ((((a * pos_fract) + b) * pos_fract + c) * pos_fract + x0);
                #endif // ENABLE_FILTER

                //calculate right channel
                xm1 = pSrc[pos_int+1];
                x0  = pSrc[pos_int+3];
                x1  = pSrc[pos_int+5];
                x2  = pSrc[pos_int+7];
                a   = (3 * (x0 - x1) - xm1 + x2) / 2;
                b   = 2 * x1 + xm1 - (5 * x0 + x2) / 2;
                c   = (x1 - xm1) / 2;
                #if ENABLE_FILTER
                    this->pOutputRight[i++] += this->FilterRight.Apply(effective_volume * ((((a * pos_fract) + b) * pos_fract + c) * pos_fract + x0));
                #else // no filter
                    this->pOutputRight[i++] += effective_volume * ((((a * pos_fract) + b) * pos_fract + c) * pos_fract + x0);
                #endif // ENABLE_FILTER
            #endif // USE_LINEAR_INTERPOLATION

            this->Pos += pitch;
        }
        inline void InterpolateOneStep_Mono(sample_t* pSrc, int& i, float& effective_volume, float& pitch, float& cutoff, float& resonance) {
            int   pos_int   = RTMath::DoubleToInt(this->Pos);  // integer position
            float pos_fract = this->Pos - pos_int;             // fractional part of position

            #if ENABLE_FILTER
                UpdateFilter_Mono(cutoff + FILTER_CUTOFF_MIN, resonance);
            #endif // ENABLE_FILTER

            #if USE_LINEAR_INTERPOLATION
                float sample_point  = effective_volume * (pSrc[pos_int] + pos_fract * (pSrc[pos_int+1] - pSrc[pos_int]));
            #else // polynomial interpolation
                float xm1 = pSrc[pos_int];
                float x0  = pSrc[pos_int+1];
                float x1  = pSrc[pos_int+2];
                float x2  = pSrc[pos_int+3];
                float a   = (3 * (x0 - x1) - xm1 + x2) / 2;
                float b   = 2 * x1 + xm1 - (5 * x0 + x2) / 2;
                float c   = (x1 - xm1) / 2;
                float sample_point = effective_volume * ((((a * pos_fract) + b) * pos_fract + c) * pos_fract + x0);
            #endif // USE_LINEAR_INTERPOLATION

            #if ENABLE_FILTER
                sample_point = this->FilterLeft.Apply(sample_point);
            #endif // ENABLE_FILTER

            this->pOutputLeft[i]    += sample_point;
            this->pOutputRight[i++] += sample_point;

            this->Pos += pitch;
        }
        inline void UpdateFilter_Stereo(float cutoff, float& resonance) {
            if (!(++FilterUpdateCounter % FILTER_UPDATE_PERIOD) && (cutoff != FilterLeft.Cutoff() || resonance != FilterLeft.Resonance())) {
                FilterLeft.SetParameters(cutoff, resonance, ModulationSystem::SampleRate());
                FilterRight.SetParameters(cutoff, resonance, ModulationSystem::SampleRate());
            }
        }
        inline void UpdateFilter_Mono(float cutoff, float& resonance) {
            if (!(++FilterUpdateCounter % FILTER_UPDATE_PERIOD) && (cutoff != FilterLeft.Cutoff() || resonance != FilterLeft.Resonance())) {
                FilterLeft.SetParameters(cutoff, resonance, ModulationSystem::SampleRate());
            }
        }
        inline float Constrain(float ValueToCheck, float Min, float Max) {
            if      (ValueToCheck > Max) ValueToCheck = Max;
            else if (ValueToCheck < Min) ValueToCheck = Min;
            return ValueToCheck;
        }
};

#endif // __VOICE_H__
