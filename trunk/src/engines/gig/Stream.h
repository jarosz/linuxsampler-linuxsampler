/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003,2004 by Benno Senoner and Christian Schoenebeck    *
 *   Copyright (C) 2005-2009 Christian Schoenebeck                         *
 *   Copyright (C) 2009 Grigor Iliev                                       *
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

#ifndef __LS_GIG_STREAM_H__
#define	__LS_GIG_STREAM_H__

#include "../common/StreamBase.h"

#if AC_APPLE_UNIVERSAL_BUILD
# include <libgig/gig.h>
#else
# include <gig.h>
#endif

namespace LinuxSampler { namespace gig {

    class Stream: public LinuxSampler::StreamBase< ::gig::DimensionRegion> {
        private:
            ::gig::buffer_t* pDecompressionBuffer;

        public:
            Stream( ::gig::buffer_t* pDecompressionBuffer, uint BufferSize, uint BufferWrapElements);
            virtual long Read(uint8_t* pBuf, long SamplesToRead);

            void Launch (
                Stream::Handle           hStream,
                reference_t*             pExportReference,
                ::gig::DimensionRegion*  pRgn,
                unsigned long            SampleOffset,
                bool                     DoLoop
            );
    };


}} // namespace LinuxSampler::gig

#endif	/* __LS_GIG_STREAM_H__ */

