/***************************************************************************
 *                                                                         *
 *   LinuxSampler - modular, streaming capable sampler                     *
 *                                                                         *
 *   Copyright (C) 2003, 2004 by Benno Senoner and Christian Schoenebeck   *
 *   Copyright (C) 2005 - 2007 Christian Schoenebeck                       *
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

#include <sstream>

#include "InstrumentResourceManager.h"

#include "../../common/global_private.h"
#include "../../plugins/InstrumentEditorFactory.h"

// We need to know the maximum number of sample points which are going to
// be processed for each render cycle of the audio output driver, to know
// how much initial sample points we need to cache into RAM. If the given
// sampler channel does not have an audio output device assigned yet
// though, we simply use this default value.
#define GIG_RESOURCE_MANAGER_DEFAULT_MAX_SAMPLES_PER_CYCLE     128

namespace LinuxSampler { namespace gig {

    // data stored as long as an instrument resource exists
    struct instr_entry_t {
        InstrumentManager::instrument_id_t ID;
        ::gig::File*                       pGig;
        uint                               MaxSamplesPerCycle; ///< if some engine requests an already allocated instrument with a higher value, we have to reallocate the instrument
    };

    // some data needed for the libgig callback function
    struct progress_callback_arg_t {
        InstrumentResourceManager*          pManager;
        InstrumentManager::instrument_id_t* pInstrumentKey;
    };

    // we use this to react on events concerning an instrument on behalf of an instrument editor
    class InstrumentEditorProxy : public InstrumentConsumer {
    public:
        virtual void ResourceToBeUpdated(::gig::Instrument* pResource, void*& pUpdateArg) {
            //TODO: inform the instrument editor about the pending update
        }

        virtual void ResourceUpdated(::gig::Instrument* pOldResource, ::gig::Instrument* pNewResource, void* pUpdateArg) {
            //TODO:: inform the instrument editor about finished update
        }

        virtual void OnResourceProgress(float fProgress) {
            //TODO: inform the instrument editor about the progress of an update
        }

        // the instrument we borrowed on behalf of the editor
        ::gig::Instrument* pInstrument;
    };

    /**
     * Callback function which will be called by libgig during loading of
     * instruments to inform about the current progress. Or to be more
     * specific; it will be called during the GetInstrument() call.
     *
     * @param pProgress - contains current progress value, pointer to the
     *                    InstrumentResourceManager instance and
     *                    instrument ID
     */
    void InstrumentResourceManager::OnInstrumentLoadingProgress(::gig::progress_t* pProgress) {
        dmsg(7,("gig::InstrumentResourceManager: progress %f%", pProgress->factor));
        progress_callback_arg_t* pArg = static_cast<progress_callback_arg_t*>(pProgress->custom);
        // we randomly schedule 90% for the .gig file loading and the remaining 10% later for sample caching
        const float localProgress = 0.9f * pProgress->factor;
        pArg->pManager->DispatchResourceProgressEvent(*pArg->pInstrumentKey, localProgress);
    }

    std::vector<InstrumentResourceManager::instrument_id_t> InstrumentResourceManager::Instruments() {
        return Entries();
    }

    InstrumentManager::mode_t InstrumentResourceManager::GetMode(const instrument_id_t& ID) {
        return static_cast<InstrumentManager::mode_t>(AvailabilityMode(ID));
    }

    void InstrumentResourceManager::SetMode(const instrument_id_t& ID, InstrumentManager::mode_t Mode) {
        dmsg(2,("gig::InstrumentResourceManager: setting mode for %s (Index=%d) to %d\n",ID.FileName.c_str(),ID.Index,Mode));
        SetAvailabilityMode(ID, static_cast<ResourceManager<InstrumentManager::instrument_id_t, ::gig::Instrument>::mode_t>(Mode));
    }

    String InstrumentResourceManager::GetInstrumentName(instrument_id_t ID) {
        Lock();
        ::gig::Instrument* pInstrument = Resource(ID, false);
        String res = (pInstrument) ? pInstrument->pInfo->Name : "";
        Unlock();
        return res;
    }

    String InstrumentResourceManager::GetInstrumentDataStructureName(instrument_id_t ID) {
        return ::gig::libraryName();
    }

    String InstrumentResourceManager::GetInstrumentDataStructureVersion(instrument_id_t ID) {
        return ::gig::libraryVersion();
    }

    void InstrumentResourceManager::LaunchInstrumentEditor(instrument_id_t ID) throw (InstrumentManagerException) {
        const String sDataType    = GetInstrumentDataStructureName(ID);
        const String sDataVersion = GetInstrumentDataStructureVersion(ID);
        // find instrument editors capable to handle given instrument
        std::vector<String> vEditors =
            InstrumentEditorFactory::MatchingEditors(sDataType, sDataVersion);
        if (!vEditors.size())
            throw InstrumentManagerException(
                "There is no instrument editor capable to handle this instrument"
            );
        // simply use the first editor in the result set
        dmsg(1,("Found matching editor '%s' for instrument ('%s', %d) having data structure ('%s','%s')\n",
            vEditors[0].c_str(), ID.FileName.c_str(), ID.Index, sDataType.c_str(), sDataVersion.c_str()));
        InstrumentEditor* pEditor = InstrumentEditorFactory::Create(vEditors[0]);
        // register for receiving notifications from the instrument editor
        pEditor->AddListener(this);
        // create a proxy that reacts on notification on behalf of the editor
        InstrumentEditorProxy* pProxy = new InstrumentEditorProxy;
        // borrow the instrument on behalf of the instrument editor
        ::gig::Instrument* pInstrument = Borrow(ID, pProxy);
        // remember the proxy and instrument for this instrument editor
        pProxy->pInstrument = pInstrument;
        InstrumentEditorProxiesMutex.Lock();
        InstrumentEditorProxies[pEditor] = pProxy;
        InstrumentEditorProxiesMutex.Unlock();
        // launch the instrument editor for the given instrument
        pEditor->Launch(pInstrument, sDataType, sDataVersion);
    }

    /**
     * Will be called by the respective instrument editor once it left its
     * Main() loop. That way we can handle cleanup before its thread finally
     * dies.
     *
     * @param pSender - instrument editor that stops execution
     */
    void InstrumentResourceManager::OnInstrumentEditorQuit(InstrumentEditor* pSender) {
        dmsg(1,("InstrumentResourceManager: instrument editor quit, doing cleanup\n"));
        // hand back instrument and free proxy
        InstrumentEditorProxiesMutex.Lock();
        if (InstrumentEditorProxies.count(pSender)) {
            InstrumentEditorProxy* pProxy =
                dynamic_cast<InstrumentEditorProxy*>(
                    InstrumentEditorProxies[pSender]
                );
            InstrumentEditorProxies.erase(pSender);
            InstrumentEditorProxiesMutex.Unlock();
            HandBack(pProxy->pInstrument, pProxy);
            if (pProxy) delete pProxy;
        } else {
            InstrumentEditorProxiesMutex.Unlock();
            std::cerr << "Eeeek, could not find instrument editor proxy, this is a bug!\n" << std::flush;
        }
        // free the editor
        InstrumentEditorFactory::Destroy(pSender);
    }

    void InstrumentResourceManager::OnSamplesToBeRemoved(std::set<void*> Samples, InstrumentEditor* pSender) {
        if (Samples.empty()) {
            std::cerr << "gig::InstrumentResourceManager: WARNING, "
                         "OnSamplesToBeRemoved() called with empty list, this "
                         "is a bug!\n" << std::flush;
            return;
        }
        // TODO: ATM we assume here that all samples are from the same file
        ::gig::Sample* pFirstSample = (::gig::Sample*) *Samples.begin();
        ::gig::File* pCriticalFile = dynamic_cast< ::gig::File*>(pFirstSample->GetParent());
        // completely suspend all engines that use that same file
        SuspendEnginesUsing(pCriticalFile);
    }

    void InstrumentResourceManager::OnSamplesRemoved(InstrumentEditor* pSender) {
        // resume all previously, completely suspended engines
        // (we don't have to un-cache the removed samples here, since that is
        // automatically done by the gig::Sample destructor)
        ResumeAllEngines();
    }

    void InstrumentResourceManager::OnDataStructureToBeChanged(void* pStruct, String sStructType, InstrumentEditor* pSender) {
        //TODO: remove code duplication
        if (sStructType == "gig::File") {
            // completely suspend all engines that use that file
            ::gig::File* pFile = (::gig::File*) pStruct;
            SuspendEnginesUsing(pFile);
        } else if (sStructType == "gig::Instrument") {
            // completely suspend all engines that use that instrument
            ::gig::Instrument* pInstrument = (::gig::Instrument*) pStruct;
            SuspendEnginesUsing(pInstrument);
        } else if (sStructType == "gig::Region") {
            // only advice the engines to suspend the given region, so they'll
            // only ignore that region (and probably already other suspended
            // ones), but beside that continue normal playback
            ::gig::Region* pRegion = (::gig::Region*) pStruct;
            ::gig::Instrument* pInstrument =
                (::gig::Instrument*) pRegion->GetParent();
            Lock();
            std::set<gig::Engine*> engines =
                GetEnginesUsing(pInstrument, false/*don't lock again*/);
            std::set<gig::Engine*>::iterator iter = engines.begin();
            std::set<gig::Engine*>::iterator end  = engines.end();
            for (; iter != end; ++iter) (*iter)->Suspend(pRegion);
            Unlock();
        } else if (sStructType == "gig::DimensionRegion") {
            // only advice the engines to suspend the given DimensionRegions's
            // parent region, so they'll only ignore that region (and probably
            // already other suspended ones), but beside that continue normal
            // playback
            ::gig::DimensionRegion* pDimReg =
                (::gig::DimensionRegion*) pStruct;
            ::gig::Region* pRegion = pDimReg->GetParent();
            ::gig::Instrument* pInstrument =
                (::gig::Instrument*) pRegion->GetParent();
            Lock();
            std::set<gig::Engine*> engines =
                GetEnginesUsing(pInstrument, false/*don't lock again*/);
            std::set<gig::Engine*>::iterator iter = engines.begin();
            std::set<gig::Engine*>::iterator end  = engines.end();
            for (; iter != end; ++iter) (*iter)->Suspend(pRegion);
            Unlock();
        } else {
            std::cerr << "gig::InstrumentResourceManager: ERROR, unknown data "
                         "structure '" << sStructType << "' requested to be "
                         "suspended by instrument editor. This is a bug!\n"
                      << std::flush;
            //TODO: we should inform the instrument editor that something seriously went wrong
        }
    }

    void InstrumentResourceManager::OnDataStructureChanged(void* pStruct, String sStructType, InstrumentEditor* pSender) {
        //TODO: remove code duplication
        if (sStructType == "gig::File") {
            // resume all previously suspended engines
            ResumeAllEngines();
        } else if (sStructType == "gig::Instrument") {
            // resume all previously suspended engines
            ResumeAllEngines();
        } else if (sStructType == "gig::Region") {
            // advice the engines to resume the given region, that is to
            // using it for playback again
            ::gig::Region* pRegion = (::gig::Region*) pStruct;
            ::gig::Instrument* pInstrument =
                (::gig::Instrument*) pRegion->GetParent();
            Lock();
            std::set<gig::Engine*> engines =
                GetEnginesUsing(pInstrument, false/*don't lock again*/);
            std::set<gig::Engine*>::iterator iter = engines.begin();
            std::set<gig::Engine*>::iterator end  = engines.end();
            for (; iter != end; ++iter) (*iter)->Resume(pRegion);
            Unlock();
        } else if (sStructType == "gig::DimensionRegion") {
            // advice the engines to resume the given DimensionRegion's parent
            // region, that is to using it for playback again
            ::gig::DimensionRegion* pDimReg =
                (::gig::DimensionRegion*) pStruct;
            ::gig::Region* pRegion = pDimReg->GetParent();
            ::gig::Instrument* pInstrument =
                (::gig::Instrument*) pRegion->GetParent();
            Lock();
            std::set<gig::Engine*> engines =
                GetEnginesUsing(pInstrument, false/*don't lock again*/);
            std::set<gig::Engine*>::iterator iter = engines.begin();
            std::set<gig::Engine*>::iterator end  = engines.end();
            for (; iter != end; ++iter) (*iter)->Resume(pRegion);
            Unlock();
        } else {
            std::cerr << "gig::InstrumentResourceManager: ERROR, unknown data "
                         "structure '" << sStructType << "' requested to be "
                         "resumed by instrument editor. This is a bug!\n"
                      << std::flush;
            //TODO: we should inform the instrument editor that something seriously went wrong
        }
    }

    void InstrumentResourceManager::OnSampleReferenceChanged(void* pOldSample, void* pNewSample, InstrumentEditor* pSender) {
        // uncache old sample in case it's not used by anybody anymore
        if (pOldSample) {
            Lock();
            ::gig::Sample* pSample = (::gig::Sample*) pOldSample;
            ::gig::File* pFile = (::gig::File*) pSample->GetParent();
            std::vector< ::gig::Instrument*> instruments =
                GetInstrumentsCurrentlyUsedOf(pFile, false/*don't lock again*/);
            for (int i = 0; i < instruments.size(); i++)
                if (!SampleReferencedByInstrument(pSample, instruments[i]))
                    UncacheInitialSamples(pSample);
            Unlock();
        }
        // make sure new sample reference is cached
        if (pNewSample) {
            Lock();
            ::gig::Sample* pSample = (::gig::Sample*) pNewSample;
            ::gig::File* pFile = (::gig::File*) pSample->GetParent();
            // get all engines that use that same gig::File
            std::set<gig::Engine*> engines = GetEnginesUsing(pFile, false/*don't lock again*/);
            std::set<gig::Engine*>::iterator iter = engines.begin();
            std::set<gig::Engine*>::iterator end  = engines.end();
            for (; iter != end; ++iter)
                CacheInitialSamples(pSample, *iter);
            Unlock();
        }
    }

    ::gig::Instrument* InstrumentResourceManager::Create(instrument_id_t Key, InstrumentConsumer* pConsumer, void*& pArg) {
        // get gig file from internal gig file manager
        ::gig::File* pGig = Gigs.Borrow(Key.FileName, (GigConsumer*) Key.Index); // conversion kinda hackish :/

        // we pass this to the progress callback mechanism of libgig
        progress_callback_arg_t callbackArg;
        callbackArg.pManager       = this;
        callbackArg.pInstrumentKey = &Key;

        ::gig::progress_t progress;
        progress.callback = OnInstrumentLoadingProgress;
        progress.custom   = &callbackArg;

        dmsg(1,("Loading gig instrument ('%s',%d)...",Key.FileName.c_str(),Key.Index));
        ::gig::Instrument* pInstrument = pGig->GetInstrument(Key.Index, &progress);
        if (!pInstrument) {
            std::stringstream msg;
            msg << "There's no instrument with index " << Key.Index << ".";
            throw InstrumentManagerException(msg.str());
        }
        pGig->GetFirstSample(); // just to force complete instrument loading
        dmsg(1,("OK\n"));

        // cache initial samples points (for actually needed samples)
        dmsg(1,("Caching initial samples..."));
        uint iRegion = 0; // just for progress calculation
        ::gig::Region* pRgn = pInstrument->GetFirstRegion();
        while (pRgn) {
            // we randomly schedule 90% for the .gig file loading and the remaining 10% now for sample caching
            const float localProgress = 0.9f + 0.1f * (float) iRegion / (float) pInstrument->Regions;
            DispatchResourceProgressEvent(Key, localProgress);

            if (pRgn->GetSample() && !pRgn->GetSample()->GetCache().Size) {
                dmsg(2,("C"));
                CacheInitialSamples(pRgn->GetSample(), (gig::EngineChannel*) pConsumer);
            }
            for (uint i = 0; i < pRgn->DimensionRegions; i++) {
                CacheInitialSamples(pRgn->pDimensionRegions[i]->pSample, (gig::EngineChannel*) pConsumer);
            }

            pRgn = pInstrument->GetNextRegion();
            iRegion++;
        }
        dmsg(1,("OK\n"));
        DispatchResourceProgressEvent(Key, 1.0f); // done; notify all consumers about progress 100%

        // we need the following for destruction later
        instr_entry_t* pEntry = new instr_entry_t;
        pEntry->ID.FileName   = Key.FileName;
        pEntry->ID.Index      = Key.Index;
        pEntry->pGig          = pGig;

        gig::EngineChannel* pEngineChannel = dynamic_cast<gig::EngineChannel*>(pConsumer);
        // and we save this to check if we need to reallocate for a engine with higher value of 'MaxSamplesPerSecond'
        pEntry->MaxSamplesPerCycle =
            (!pEngineChannel) ? 0 /* don't care for instrument editors */ :
                (pEngineChannel->GetEngine()) ?
                    dynamic_cast<gig::Engine*>(pEngineChannel->GetEngine())->pAudioOutputDevice->MaxSamplesPerCycle()
                    : GIG_RESOURCE_MANAGER_DEFAULT_MAX_SAMPLES_PER_CYCLE;
        pArg = pEntry;

        return pInstrument;
    }

    void InstrumentResourceManager::Destroy( ::gig::Instrument* pResource, void* pArg) {
        instr_entry_t* pEntry = (instr_entry_t*) pArg;
        // we don't need the .gig file here anymore
        Gigs.HandBack(pEntry->pGig, (GigConsumer*) pEntry->ID.Index); // conversion kinda hackish :/
        delete pEntry;
    }

    void InstrumentResourceManager::OnBorrow(::gig::Instrument* pResource, InstrumentConsumer* pConsumer, void*& pArg) {
        instr_entry_t* pEntry = (instr_entry_t*) pArg;
        gig::EngineChannel* pEngineChannel = dynamic_cast<gig::EngineChannel*>(pConsumer);
        uint maxSamplesPerCycle =
            (pEngineChannel && pEngineChannel->GetEngine()) ? dynamic_cast<gig::Engine*>(pEngineChannel->GetEngine())->pAudioOutputDevice->MaxSamplesPerCycle()
                                          : GIG_RESOURCE_MANAGER_DEFAULT_MAX_SAMPLES_PER_CYCLE;
        if (pEntry->MaxSamplesPerCycle < maxSamplesPerCycle) {
            Update(pResource, pConsumer);
        }
    }

    /**
     * Give back an instrument. This should be used instead of
     * HandBack if there are some dimension regions that are still in
     * use. (When an instrument is changed, the voices currently
     * playing is allowed to keep playing with the old instrument
     * until note off arrives. New notes will use the new instrument.)
     */
    void InstrumentResourceManager::HandBackInstrument(::gig::Instrument* pResource, InstrumentConsumer* pConsumer,
                                                       ::gig::DimensionRegion** dimRegionsInUse) {
        DimRegInfoMutex.Lock();
        for (int i = 0 ; dimRegionsInUse[i] ; i++) {
            DimRegInfo[dimRegionsInUse[i]].refCount++;
            SampleRefCount[dimRegionsInUse[i]->pSample]++;
        }
        HandBack(pResource, pConsumer, true);
        DimRegInfoMutex.Unlock();
    }

    /**
     * Give back a dimension region that belongs to an instrument that
     * was previously handed back.
     */
    void InstrumentResourceManager::HandBackDimReg(::gig::DimensionRegion* pDimReg) {
        DimRegInfoMutex.Lock();
        dimreg_info_t& dimRegInfo = DimRegInfo[pDimReg];
        int dimRegRefCount = --dimRegInfo.refCount;
        int sampleRefCount = --SampleRefCount[pDimReg->pSample];
        if (dimRegRefCount == 0) {
            ::gig::File* gig = dimRegInfo.file;
            ::RIFF::File* riff = dimRegInfo.riff;
            DimRegInfo.erase(pDimReg);
            // TODO: we could delete Region and Instrument here if
            // they have become unused

            if (sampleRefCount == 0) {
                SampleRefCount.erase(pDimReg->pSample);

                if (gig) {
                    gig->DeleteSample(pDimReg->pSample);
                    if (!gig->GetFirstSample()) {
                        dmsg(2,("No more samples in use - freeing gig\n"));
                        delete gig;
                        delete riff;
                    }
                }
            }
        }
        DimRegInfoMutex.Unlock();
    }

    /**
     * Just a wrapper around the other @c CacheInitialSamples() method.
     *
     *  @param pSample - points to the sample to be cached
     *  @param pEngine - pointer to Gig Engine Channel which caused this call
     *                   (may be NULL, in this case default amount of samples
     *                   will be cached)
     */
    void InstrumentResourceManager::CacheInitialSamples(::gig::Sample* pSample, gig::EngineChannel* pEngineChannel) {
        gig::Engine* pEngine =
            (pEngineChannel && pEngineChannel->GetEngine()) ?
                dynamic_cast<gig::Engine*>(pEngineChannel->GetEngine()) : NULL;
        CacheInitialSamples(pSample, pEngine);
    }

    /**
     *  Caches a certain size at the beginning of the given sample in RAM. If the
     *  sample is very short, the whole sample will be loaded into RAM and thus
     *  no disk streaming is needed for this sample. Caching an initial part of
     *  samples is needed to compensate disk reading latency.
     *
     *  @param pSample - points to the sample to be cached
     *  @param pEngine - pointer to Gig Engine which caused this call
     *                   (may be NULL, in this case default amount of samples
     *                   will be cached)
     */
    void InstrumentResourceManager::CacheInitialSamples(::gig::Sample* pSample, gig::Engine* pEngine) {
        if (!pSample) {
            dmsg(4,("gig::InstrumentResourceManager: Skipping sample (pSample == NULL)\n"));
            return;
        }
        if (!pSample->SamplesTotal) return; // skip zero size samples

        if (pSample->SamplesTotal <= CONFIG_PRELOAD_SAMPLES) {
            // Sample is too short for disk streaming, so we load the whole
            // sample into RAM and place 'pAudioIO->FragmentSize << CONFIG_MAX_PITCH'
            // number of '0' samples (silence samples) behind the official buffer
            // border, to allow the interpolator do it's work even at the end of
            // the sample.
            const uint maxSamplesPerCycle =
                (pEngine) ? pEngine->pAudioOutputDevice->MaxSamplesPerCycle()
                          : GIG_RESOURCE_MANAGER_DEFAULT_MAX_SAMPLES_PER_CYCLE;
            const uint neededSilenceSamples = (maxSamplesPerCycle << CONFIG_MAX_PITCH) + 3;
            const uint currentlyCachedSilenceSamples = pSample->GetCache().NullExtensionSize / pSample->FrameSize;
            if (currentlyCachedSilenceSamples < neededSilenceSamples) {
                dmsg(3,("Caching whole sample (sample name: \"%s\", sample size: %d)\n", pSample->pInfo->Name.c_str(), pSample->SamplesTotal));
                ::gig::buffer_t buf = pSample->LoadSampleDataWithNullSamplesExtension(neededSilenceSamples);
                dmsg(4,("Cached %d Bytes, %d silence bytes.\n", buf.Size, buf.NullExtensionSize));
            }
        }
        else { // we only cache CONFIG_PRELOAD_SAMPLES and stream the other sample points from disk
            if (!pSample->GetCache().Size) pSample->LoadSampleData(CONFIG_PRELOAD_SAMPLES);
        }

        if (!pSample->GetCache().Size) std::cerr << "Unable to cache sample - maybe memory full!" << std::endl << std::flush;
    }

    void InstrumentResourceManager::UncacheInitialSamples(::gig::Sample* pSample) {
        dmsg(1,("Uncaching sample %x\n",pSample));
        if (pSample->GetCache().Size) pSample->ReleaseSampleData();
    }

    /**
     * Returns a list with all instruments currently in use, that are part of
     * the given file.
     *
     * @param pFile - search criteria
     * @param bLock - whether we should lock (mutex) the instrument manager
     *                during this call and unlock at the end of this call
     */
    std::vector< ::gig::Instrument*> InstrumentResourceManager::GetInstrumentsCurrentlyUsedOf(::gig::File* pFile, bool bLock) {
        if (bLock) Lock();
        std::vector< ::gig::Instrument*> result;
        std::vector< ::gig::Instrument*> allInstruments = Resources(false/*don't lock again*/);
        for (int i = 0; i < allInstruments.size(); i++)
            if (
                (::gig::File*) allInstruments[i]->GetParent()
                == pFile
            ) result.push_back(allInstruments[i]);
        if (bLock) Unlock();
        return result;
    }

    /**
     * Returns a list with all gig Engines that are currently using the given
     * instrument.
     *
     * @param pInstrument - search criteria
     * @param bLock - whether we should lock (mutex) the instrument manager
     *                during this call and unlock at the end of this call
     */
    std::set<gig::Engine*> InstrumentResourceManager::GetEnginesUsing(::gig::Instrument* pInstrument, bool bLock) {
        if (bLock) Lock();
        std::set<gig::Engine*> result; 
        std::set<ResourceConsumer< ::gig::Instrument>*> consumers = ConsumersOf(pInstrument);
        std::set<ResourceConsumer< ::gig::Instrument>*>::iterator iter = consumers.begin();
        std::set<ResourceConsumer< ::gig::Instrument>*>::iterator end  = consumers.end();
        for (; iter != end; ++iter) {
            gig::EngineChannel* pEngineChannel = dynamic_cast<gig::EngineChannel*>(*iter);
            if (!pEngineChannel) continue;
            gig::Engine* pEngine = dynamic_cast<gig::Engine*>(pEngineChannel->GetEngine());
            if (!pEngine) continue;
            result.insert(pEngine);
        }
        if (bLock) Unlock();
        return result;
    }

    /**
     * Returns a list with all gig Engines that are currently using an
     * instrument that is part of the given instrument file.
     *
     * @param pFile - search criteria
     * @param bLock - whether we should lock (mutex) the instrument manager
     *                during this call and unlock at the end of this call
     */
    std::set<gig::Engine*> InstrumentResourceManager::GetEnginesUsing(::gig::File* pFile, bool bLock) {
        if (bLock) Lock();
        // get all instruments (currently in usage) that use that same gig::File
        std::vector< ::gig::Instrument*> instrumentsOfInterest =
            GetInstrumentsCurrentlyUsedOf(pFile, false/*don't lock again*/);

        // get all engines that use that same gig::File
        std::set<gig::Engine*> result;
        {
            for (int i = 0; i < instrumentsOfInterest.size(); i++) {
                std::set<ResourceConsumer< ::gig::Instrument>*> consumers = ConsumersOf(instrumentsOfInterest[i]);
                std::set<ResourceConsumer< ::gig::Instrument>*>::iterator iter = consumers.begin();
                std::set<ResourceConsumer< ::gig::Instrument>*>::iterator end  = consumers.end();
                for (; iter != end; ++iter) {
                    gig::EngineChannel* pEngineChannel = dynamic_cast<gig::EngineChannel*>(*iter);
                    if (!pEngineChannel) continue;
                    gig::Engine* pEngine = dynamic_cast<gig::Engine*>(pEngineChannel->GetEngine());
                    if (!pEngine) continue;
                    // the unique, sorted container std::set makes
                    // sure we won't have duplicates
                    result.insert(pEngine);
                }
            }
        }
        if (bLock) Unlock();
        return result;
    }

    /**
     * Returns @c true in case the given sample is referenced somewhere by the
     * given instrument, @c false otherwise.
     *
     * @param pSample - sample reference
     * @param pInstrument - instrument that might use that sample
     */
    bool InstrumentResourceManager::SampleReferencedByInstrument(::gig::Sample* pSample, ::gig::Instrument* pInstrument) {
        for (
            ::gig::Region* pRegion = pInstrument->GetFirstRegion();
            pRegion; pRegion = pInstrument->GetNextRegion()
        ) {
            for (
                int i = 0; i < pRegion->DimensionRegions &&
                pRegion->pDimensionRegions[i]; i++
            ) {
                if (pRegion->pDimensionRegions[i]->pSample == pSample)
                    return true;
            }
        }
        return false;
    }

    /**
     * Suspend all gig engines that use the given instrument. This means
     * completely stopping playback on those engines and killing all their
     * voices and disk streams. This method will block until all voices AND
     * their disk streams are finally deleted and the engine turned into a
     * complete idle loop.
     *
     * All @c SuspendEnginesUsing() methods only serve one thread by one and
     * block all other threads until the current active thread called
     * @c ResumeAllEngines() .
     *
     * @param pInstrument - search criteria
     */
    void InstrumentResourceManager::SuspendEnginesUsing(::gig::Instrument* pInstrument) {
        // make sure no other thread suspends whole engines at the same time
        suspendedEnginesMutex.Lock();
        // get all engines that use that same gig::Instrument
        suspendedEngines = GetEnginesUsing(pInstrument, true/*lock*/);
        // finally, completely suspend all engines that use that same gig::Instrument
        std::set<gig::Engine*>::iterator iter = suspendedEngines.begin();
        std::set<gig::Engine*>::iterator end  = suspendedEngines.end();
        for (; iter != end; ++iter) (*iter)->SuspendAll();
    }

    /**
     * Suspend all gig engines that use the given instrument file. This means
     * completely stopping playback on those engines and killing all their
     * voices and disk streams. This method will block until all voices AND
     * their disk streams are finally deleted and the engine turned into a
     * complete idle loop.
     *
     * All @c SuspendEnginesUsing() methods only serve one thread by one and
     * block all other threads until the current active thread called
     * @c ResumeAllEngines() .
     *
     * @param pFile - search criteria
     */
    void InstrumentResourceManager::SuspendEnginesUsing(::gig::File* pFile) {
        // make sure no other thread suspends whole engines at the same time
        suspendedEnginesMutex.Lock();
        // get all engines that use that same gig::File
        suspendedEngines = GetEnginesUsing(pFile, true/*lock*/);
        // finally, completely suspend all engines that use that same gig::File
        std::set<gig::Engine*>::iterator iter = suspendedEngines.begin();
        std::set<gig::Engine*>::iterator end  = suspendedEngines.end();
        for (; iter != end; ++iter) (*iter)->SuspendAll();
    }

    /**
     * MUST be called after one called one of the @c SuspendEnginesUsing()
     * methods, to resume normal playback on all previously suspended engines.
     * As it's only possible for one thread to suspend whole engines at the
     * same time, this method doesn't take any arguments.
     */
    void InstrumentResourceManager::ResumeAllEngines() {
        // resume all previously completely suspended engines
        std::set<Engine*>::iterator iter = suspendedEngines.begin();
        std::set<Engine*>::iterator end  = suspendedEngines.end();
        for (; iter != end; ++iter) (*iter)->ResumeAll();
        // no more suspended engines ...
        suspendedEngines.clear();
        // allow another thread to suspend whole engines
        suspendedEnginesMutex.Unlock();
    }



    // internal gig file manager

    ::gig::File* InstrumentResourceManager::GigResourceManager::Create(String Key, GigConsumer* pConsumer, void*& pArg) {
        dmsg(1,("Loading gig file \'%s\'...", Key.c_str()));
        ::RIFF::File* pRIFF = new ::RIFF::File(Key);
        ::gig::File* pGig   = new ::gig::File(pRIFF);
        pArg                = pRIFF;
        dmsg(1,("OK\n"));
        return pGig;
    }

    void InstrumentResourceManager::GigResourceManager::Destroy(::gig::File* pResource, void* pArg) {
        dmsg(1,("Freeing gig file from memory..."));

        // Delete as much as possible of the gig file. Some of the
        // dimension regions and samples may still be in use - these
        // will be deleted later by the HandBackDimReg function.
        bool deleteFile = true;
        ::gig::Instrument* nextInstrument;
        for (::gig::Instrument* instrument = pResource->GetFirstInstrument() ;
             instrument ;
             instrument = nextInstrument) {
            nextInstrument = pResource->GetNextInstrument();
            bool deleteInstrument = true;
            ::gig::Region* nextRegion;
            for (::gig::Region *region = instrument->GetFirstRegion() ;
                 region ;
                 region = nextRegion) {
                nextRegion = instrument->GetNextRegion();
                bool deleteRegion = true;
                for (int i = 0 ; i < region->DimensionRegions ; i++)
                {
                    ::gig::DimensionRegion *d = region->pDimensionRegions[i];
                    std::map< ::gig::DimensionRegion*, dimreg_info_t>::iterator iter = parent->DimRegInfo.find(d);
                    if (iter != parent->DimRegInfo.end()) {
                        dimreg_info_t& dimRegInfo = (*iter).second;
                        dimRegInfo.file = pResource;
                        dimRegInfo.riff = (::RIFF::File*)pArg;
                        deleteFile = deleteInstrument = deleteRegion = false;
                    }
                }
                if (deleteRegion) instrument->DeleteRegion(region);
            }
            if (deleteInstrument) pResource->DeleteInstrument(instrument);
        }
        if (deleteFile) {
            delete pResource;
            delete (::RIFF::File*) pArg;
        } else {
            dmsg(2,("keeping some samples that are in use..."));
            ::gig::Sample* nextSample;
            for (::gig::Sample* sample = pResource->GetFirstSample() ;
                 sample ;
                 sample = nextSample) {
                nextSample = pResource->GetNextSample();
                if (parent->SampleRefCount.find(sample) == parent->SampleRefCount.end()) {
                    pResource->DeleteSample(sample);
                }
            }
        }
        dmsg(1,("OK\n"));
    }

}} // namespace LinuxSampler::gig
