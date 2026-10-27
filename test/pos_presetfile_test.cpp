// pos_presetfile_test — the Save / Load file format (src/UI/PresetFile.hpp), headless: a patch written
// and read back keeps every value, every factory preset in Presets/ parses and names only known
// params, and files that are not presets are refused.
//
// Usage: pos-presetfile-test <Presets dir> <scratch dir>

#include "PresetFile.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace plainoldsynth;

namespace {

int g_fails = 0;
void check(bool ok, const std::string& what) {
   std::printf("  %s: %s\n", ok ? "ok  " : "FAIL", what.c_str());
   if (!ok) ++g_fails;
}

bool find(const PresetFileData& d, const std::string& key, float& v) {
   for (const auto& kv : d.values)
      if (kv.first == key) { v = kv.second; return true; }
   return false;
}

} // namespace

int main(int argc, char** argv) {
   if (argc < 3) { std::fprintf(stderr, "usage: %s <Presets dir> <scratch dir>\n", argv[0]); return 2; }
   namespace fs = std::filesystem;
   const fs::path presets(argv[1]), scratch(argv[2]);
   fs::create_directories(scratch);

   std::printf("round trip\n");
   {
      float values[kParamCount];
      for (uint32_t i = 0; i < kParamCount; ++i) {
         // somewhere inside each range, not the default, so a dropped key shows
         const float t = 0.25f + 0.5f * float(i) / float(kParamCount);
         values[i] = kParams[i].min + t * (kParams[i].max - kParams[i].min);
         if (kParams[i].integer) values[i] = std::round(values[i]);
      }
      const fs::path file = scratch / "round \"trip\".json";
      check(writePresetFile(file.string(), values, "round \"trip\""), "writes");
      PresetFileData d;
      check(readPresetFile(file.string(), d), "reads back as a preset");
      check(d.name == "round \"trip\"", "keeps the name, quotes and all");
      bool all = true;
      for (uint32_t i = 0; i < kParamCount; ++i) {
         float v = 0.0f;
         const bool has = find(d, kParams[i].symbol, v);
         if (i == kParamMpe) { if (has) all = false; continue; }   // controller setup: left out
         if (!has || v != values[i]) {   // written so that it reads back exactly
            std::printf("    %s: %s\n", kParams[i].symbol, has ? "differs" : "missing");
            all = false;
         }
      }
      check(all, "every sound param comes back exactly, MPE left out");
   }

   std::printf("factory presets\n");
   int count = 0;
   for (const auto& e : fs::directory_iterator(presets)) {
      if (e.path().extension() != ".json") continue;
      ++count;
      PresetFileData d;
      const bool ok = readPresetFile(e.path().string(), d);
      bool known = ok && !d.name.empty();
      for (const auto& kv : d.values)
         if (paramIndexForSymbol(kv.first.c_str()) < 0) {
            std::printf("    unknown param %s\n", kv.first.c_str());
            known = false;
         }
      check(known, e.path().filename().string() + " parses, named, every key a param");
   }
   check(count > 0, "found factory presets");

   std::printf("not presets\n");
   {
      PresetFileData d;
      check(!parsePresetJson("", d), "empty");
      check(!parsePresetJson("{\"name\": \"x\", \"params\": {}}", d), "no schema and no values");
      check(parsePresetJson("{\"name\": \"x\", \"params\": {\"cutoff\": 0.5}}", d),
            "no schema but values, as the factory files are");
      check(!parsePresetJson("{\"schema\": \"other.preset\", \"params\": {\"cutoff\": 0.5}}", d),
            "another schema");
      check(!parsePresetJson("{\"schema\": \"plainoldsynth.preset\"", d), "truncated");
      check(parsePresetJson("{\"schema\": \"plainoldsynth.preset\", \"x\": [1, {\"y\": true}, null]}", d),
            "arrays, bools and null are skipped, not refused");
   }

   std::printf(g_fails == 0 ? "ALL PASS\n" : "%d FAILED\n", g_fails);
   return g_fails == 0 ? 0 : 1;
}
