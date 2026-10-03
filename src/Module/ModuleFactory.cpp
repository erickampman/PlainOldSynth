//
//  ModuleFactory.mm
//
//  Created by Eric Kampman on 4/12/25.
//


#include "ModuleFactory.hpp"
#include "Module.hpp"
//#include "SinOscillatorModule.hpp" // This should expose the init/process/midi procs
//#include "QADSRModule.hpp"
//#include "CombineModule.hpp"
//#include "FinalModule.hpp"

ModuleFactory::ModuleFactory(double sampleRate, uint32_t blockSize)
	: sampleRate(sampleRate), blockSize(blockSize) {}

std::unique_ptr<Module> ModuleFactory::createModule(AUParameterAddress id,
													const ModuleParamConstantsGroup *constantsGroup,
													ModuleInitProc initProc,
													ModuleDeinitProc deinitProc,
													ModuleProcessProc processProc,
													ModuleMIDIProc midiProc,
                                                    ModuleParamProc paramProc,
                                                    ModuleResetProc resetProc,
                                                    ModuleSampleRateProc sampleRateProc
                                                    )
{
	auto module = std::make_unique<Module>(
		id,
		sampleRate,
		blockSize,
        constantsGroup,
		initProc,
		deinitProc,
		processProc,
		midiProc,
        paramProc,
        resetProc,
        sampleRateProc
	);
	module->addParams(constantsGroup);
	// AFTER addParams(), not from Module's own constructor — see Module::callInit()'s comment:
	// an Init proc that reads paramByEnum() needs params[] already holding real defaults.
	module->callInit(initProc, blockSize);
	return module;
}

void ModuleFactory::updateSampleRate(double newSampleRate) {
    sampleRate = newSampleRate;
}
