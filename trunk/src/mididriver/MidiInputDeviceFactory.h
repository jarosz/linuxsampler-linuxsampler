/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
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

#ifndef __LS_MIDI_INPUT_DEVICE_FACTORY_H__
#define __LS_MIDI_INPUT_DEVICE_FACTORY_H__

#include <map>
#include <vector>

#include "../common/LinuxSamplerException.h"
#include "MidiInputDevice.h"

#define REGISTER_MIDI_INPUT_DRIVER(DriverName,DriverClass)  static LinuxSampler::MidiInputDeviceFactory::InnerFactoryRegistrator<DriverClass> __auto_register_midi_input_driver__##DriverClass(DriverName)

namespace LinuxSampler {

  class MidiInputDeviceFactory {
      public:
          class InnerFactory {
              public:
                  virtual MidiInputDevice* Create(std::map<String,String>& Parameters)  = 0;
                  virtual std::map<String,DeviceCreationParameter*> AvailableParameters() = 0;
                  virtual String Description() = 0;
                  virtual String Version() = 0;
          };

          template <class Driver_T>
          class InnerFactoryTemplate : public InnerFactory {
              public:
                  virtual MidiInputDevice* Create(std::map<String,String>& Parameters)  { return new Driver_T(Parameters); }
                  virtual std::map<String,DeviceCreationParameter*> AvailableParameters() { return Driver_T::AvailableParameters(); }
                  virtual String Description() { return Driver_T::Description(); }
                  virtual String Version()     { return Driver_T::Version();     }
          };

          template <class Driver_T>
          class InnerFactoryRegistrator {
              public:
                  InnerFactoryRegistrator(String DriverName) {
                      MidiInputDeviceFactory::InnerFactories[DriverName] = new InnerFactoryTemplate<Driver_T>;
                  }
          };

          static MidiInputDevice*                          Create(String DriverName, std::map<String,String>& Parameters) throw (LinuxSamplerException);
          static std::vector<String>                       AvailableDrivers();
          static String                                    AvailableDriversAsString();
          static std::map<String,DeviceCreationParameter*> GetAvailableDriverParameters(String DriverName) throw (LinuxSamplerException);
          static DeviceCreationParameter*                  GetDriverParameter(String DriverName, String ParameterName) throw (LinuxSamplerException);
          static String                                    GetDriverDescription(String DriverName) throw (LinuxSamplerException);
          static String                                    GetDriverVersion(String DriverName) throw (LinuxSamplerException);

      protected:
          static std::map<String, InnerFactory*> InnerFactories;
  };

} // namespace LinuxSampler

#endif // __LS_AUDIO_OUTPUT_DEVICE_FACTORY_H__
