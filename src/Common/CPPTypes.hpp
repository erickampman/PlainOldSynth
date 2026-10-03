//
//  CPPTypes.hpp
//  Vendored from PolygraphModular (AUApr25) — unmodified. Pure C++, no platform types.
//

#ifndef CPPTypes_h
#define CPPTypes_h

#include <memory> // required for unique_ptr

#define DEFAULT_SAMPLE_RATE 44100.0
// Max samples to process at once.
#define MODULE_BUFF_SIZE 512
struct ModuleFloatBuff {
   float data[MODULE_BUFF_SIZE];
};

typedef std::pair<std::unique_ptr<ModuleFloatBuff>, std::unique_ptr<ModuleFloatBuff>> KPBuffPair;

// Forward declarations
struct Module;

// Extra becomes a type-erased runtime context
typedef void* Extra;

typedef double (*ToneProc)(double, double); // (phase, cyclesPerSample)

#endif /* CPPTypes_h */
