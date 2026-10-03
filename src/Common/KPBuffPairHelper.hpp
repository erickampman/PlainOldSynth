//
//  KPBuffPairHelper.hpp
//  Vendored from PolygraphModular (AUApr25). Unmodified except added <cstring>/<algorithm>
//  includes (were transitively available on Apple; made explicit for a strict toolchain).
//

#ifndef KPBuffPairHelper_h
#define KPBuffPairHelper_h

#include <algorithm>
#include <cstring>

#include "CPPTypes.hpp"

class KPBuffPairHelper {
public:
   static const KPBuffPair* zeroes(uint32_t blockSize = 512) {
      static KPBuffPair zeroBuffers = makeZeroBuffers(blockSize);
      return &zeroBuffers;
   }

   static const KPBuffPair* ones(uint32_t blockSize = 512) {
      static KPBuffPair onesBuffers = makeOnesBuffers(blockSize);
      return &onesBuffers;
   }

   static void copyKPBuffPairContents(KPBuffPair* to, KPBuffPair* from) {
      float* toLeft   = to->first.get()->data;
      float* toRight  = to->second.get()->data;
      float* fromLeft = from->first.get()->data;
      float* fromRight = from->second.get()->data;

      memcpy(toLeft, fromLeft, sizeof(float) * MODULE_BUFF_SIZE);
      memcpy(toRight, fromRight, sizeof(float) * MODULE_BUFF_SIZE);
   }

private:
   static KPBuffPair makeZeroBuffers(uint32_t blockSize) {
      auto left  = std::make_unique<ModuleFloatBuff>();
      auto right = std::make_unique<ModuleFloatBuff>();
      std::fill(left->data, left->data + blockSize, 0);
      std::fill(right->data, right->data + blockSize, 0);
      return std::make_pair(std::move(left), std::move(right));
   }

   static KPBuffPair makeOnesBuffers(uint32_t blockSize) {
      auto left  = std::make_unique<ModuleFloatBuff>();
      auto right = std::make_unique<ModuleFloatBuff>();
      std::fill(left->data, left->data + blockSize, 1.0f);
      std::fill(right->data, right->data + blockSize, 1.0f);
      return std::make_pair(std::move(left), std::move(right));
   }
};

#endif /* KPBuffPairHelper_h */
