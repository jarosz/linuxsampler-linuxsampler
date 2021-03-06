/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 Christian Schoenebeck                              *
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

#ifndef __LS_MIDIINPUTDEVICEALSA_H__
#define __LS_MIDIINPUTDEVICEALSA_H__

#include <alsa/asoundlib.h>

#include "../../common/global.h"
#include "../../common/Thread.h"
#include "../../common/RingBuffer.h"
#include "MidiInputDevice.h"

namespace LinuxSampler {

    /** ALSA MIDI input driver
     *
     * Implements MIDI input for the Advanced Linux Sound Architecture
     * (ALSA).
     */
    class MidiInputDeviceAlsa : public MidiInputDevice, public Thread {
        public:

            /**
             * MIDI Port implementation for the ALSA MIDI input driver.
             */
            class MidiInputPortAlsa : public MidiInputPort {
                public:
                    /** MIDI Port Parameter 'NAME'
                     *
                     * Used to assign an arbitrary name to the MIDI port.
                     */
                    class ParameterName : public MidiInputPort::ParameterName {
                        public:
                            ParameterName(MidiInputPort* pPort) throw (LinuxSamplerException);
                            virtual void OnSetValue(String s) throw (LinuxSamplerException);
                    };

                    /** MIDI Port Parameter 'ALSA_SEQ_BINDINGS'
                     *
                     * Used to connect to other Alsa sequencer clients.
                     */
                    class ParameterAlsaSeqBindings : public DeviceRuntimeParameterStrings {
                        public:
                            ParameterAlsaSeqBindings(MidiInputPortAlsa* pPort);
                            virtual String Description();
                            virtual bool Fix();
                            virtual std::vector<String> PossibilitiesAsString();
                            virtual void OnSetValue(std::vector<String> vS) throw (LinuxSamplerException);
                        protected:
                            MidiInputPortAlsa* pPort;
                    };

                    /** MIDI Port Parameter 'ALSA_SEQ_ID'
                     *
                     * Reflects the ALSA sequencer ID of this MIDI port,
                     * e.g. "128:0".
                     */
                    class ParameterAlsaSeqId : public DeviceRuntimeParameterString {
                        public:
                            ParameterAlsaSeqId(MidiInputPortAlsa* pPort);
                            virtual String              Description();
                            virtual bool                Fix();
                            virtual std::vector<String> PossibilitiesAsString();
                            virtual void                OnSetValue(String s);
                    };

                    void ConnectToAlsaMidiSource(const char* MidiSource);
                protected:
                    MidiInputPortAlsa(MidiInputDeviceAlsa* pDevice) throw (MidiInputException);
                    ~MidiInputPortAlsa();
                    friend class MidiInputDeviceAlsa;
                private:
                    MidiInputDeviceAlsa* pDevice;

                    friend class ParameterName;
                    friend class ParameterAlsaSeqBindings;
            };

            MidiInputDeviceAlsa(std::map<String,DeviceCreationParameter*> Parameters, void* pSampler);
            ~MidiInputDeviceAlsa();

            // derived abstract methods from class 'MidiInputDevice'
            void Listen();
            void StopListen();
            virtual String Driver();
            static String Name();
            static String Description();
            static String Version();

            MidiInputPortAlsa* CreateMidiPort();
        protected:
            int Main(); ///< Implementation of virtual method from class Thread
        private:
            snd_seq_t* hAlsaSeq;
            int        hAlsaSeqClient;       ///< Alsa Sequencer client ID

            friend class MidiInputPortAlsa;
            friend class MidiInputPortAlsa::ParameterName;
            friend class MidiInputPortAlsa::ParameterAlsaSeqBindings;
    };
}

#endif // __LS_MIDIINPUTDEVICEALSA_H__
