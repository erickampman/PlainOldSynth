//
//  ModuleGraph.hpp
//
//  Created by Eric Kampman on 4/19/25.
//

#if 0
#ifndef ModuleGraph_hpp
#define ModuleGraph_hpp

#include <memory>
#include <unordered_set>

#import <CoreMIDI/CoreMIDI.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioToolbox/AUParameters.h>

#import "Module.hpp"

class ModuleGraph {
public:
	bool add(std::unique_ptr<Module> module);
	bool connect(AUParameterAddress fromID, AUParameterAddress toID, size_t inputIndex);
	bool disconnect(AUParameterAddress fromID, AUParameterAddress toID);
	bool contains(AUParameterAddress id) const;

	Module* get(AUParameterAddress id);
	void clear();
	
	bool disconnectUpstream(AUParameterAddress rootID,
										 AUParameterAddress fromID,
										 AUParameterAddress toID);
	// the above disconnectUpstream calls this.
	bool dfsDisconnect(AUParameterAddress currentID,
									AUParameterAddress fromID,
									AUParameterAddress toID,
									std::unordered_set<AUParameterAddress>& visited);

	// Used by Voice
	std::vector<Module*> topologicalSortFrom(AUParameterAddress rootID) const;

private:
	std::unordered_map<AUParameterAddress, std::unique_ptr<Module>> modules;
};

#endif /* ModuleGraph_hpp */

#endif /* 0 maybe remove */
