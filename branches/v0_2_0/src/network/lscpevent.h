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

#ifndef __LSCPEVENT_H_
#define __LSCPEVENT_H_
#include <iostream>
#include <sstream>
#include <string>
#include <list>
#include "../Sampler.h"
#include "../common/global.h"
#include "../common/LinuxSamplerException.h"

using namespace LinuxSampler;


/**
 * Helper class for producing result sets
 */
class LSCPEvent {
    public:
	    /**
	     * Event types
	     **/
	    enum event_t {
		    event_channels,
		    event_voice_count,
		    event_stream_count,
		    event_buffer_fill,
		    event_info,
		    event_misc
	    };

	    /* This constructor will do type lookup based on name
	     **/ 
	    LSCPEvent(String eventName) throw (LinuxSamplerException);

	    /* These constructors are used to create event and fill it with data for sending
	     * These will be used by the thread that wants to send an event to all clients
	     * that are subscribed to it
	     **/
	    LSCPEvent(event_t eventType, int uiData);
	    LSCPEvent(event_t eventType, String sData);
	    LSCPEvent(event_t eventType, int uiData1, int uiData2);
	    LSCPEvent(event_t eventType, String sData, int uiData);
	    LSCPEvent(event_t eventType, int uiData, String sData);
	    String Produce( void );

	    /* Returns event type */
	    event_t GetType( void ) { return type; }

	    /* These methods are used to registed and unregister an event */
	    static void RegisterEvent(event_t eventType, String EventName);
	    static void UnregisterEvent(event_t eventType);

	    /* This method returns a name for events of a given type */
	    static String Name(event_t eventType);

	    /* This method returs a list of all event types registered */
	    static std::list<event_t> List( void );

    private:
	    String storage;
	    event_t type;

	    static std::map<event_t, String> EventNames;
};

#endif // __LSCPEVENT_H_
