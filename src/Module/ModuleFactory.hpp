//
//  ModuleFactory.h
//
//  Created by Eric Kampman on 4/11/25.
//

#ifndef ModuleFactory_h
#define ModuleFactory_h

#include <memory>

#include "pm_compat.h"

#include "Module.hpp"

#include "pm_params.h"

class ModuleFactory {
public:
	ModuleFactory(double sampleRate, uint32_t blockSize);
	
	std::unique_ptr<Module> createModule(AUParameterAddress id,
										 const ModuleParamConstantsGroup *constantsGroup,
										 ModuleInitProc initProc,
										 ModuleDeinitProc deinitProc,
										 ModuleProcessProc processProc,
										 ModuleMIDIProc midiProc,
                                         ModuleParamProc paramProc,
                                         ModuleResetProc resetProc,
                                         ModuleSampleRateProc sampleRateProc
                                         );
    
    void updateSampleRate(double newSampleRate);

    double sampleRate;
	uint32_t blockSize;
};

#endif /* ModuleFactory_h */
