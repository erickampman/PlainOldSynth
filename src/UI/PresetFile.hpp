// PresetFile.hpp — a patch as a preset file: the same JSON as Presets/*.json, written by the UI's
// Save and read back by its Load. Each value is keyed by its ParamSpec::symbol (PlainOldSynthParams.h),
// as in the factory files, so a saved patch can also be dropped into Presets/ as a factory preset
// unchanged (tools/gen_presets.py reads "name" and "params" and ignores "schema").
//
// The reader is a small JSON parser of its own, not a library: it keeps only what the loader needs
// (schema, name, and the "params" leaves as symbol -> float) and skips everything else.

#ifndef PLAINOLDSYNTH_PRESETFILE_HPP
#define PLAINOLDSYNTH_PRESETFILE_HPP

#include "PlainOldSynthParams.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace plainoldsynth {

static constexpr const char* kPresetSchema = "plainoldsynth.preset";

inline std::string jsonEscape(const std::string& s)
{
   std::string out;
   for (const char c : s)
   {
      if (c == '"' || c == '\\') { out += '\\'; out += c; }
      else if (static_cast<unsigned char>(c) < 0x20) out += ' ';
      else out += c;
   }
   return out;
}

// Build the preset JSON for `values` (kParamCount entries), one param per line in table order.
// Numbers, not strings, as the factory files have them (gen_presets.py requires numbers): integers
// as integers, the rest to 6 significant digits, or 9 when 6 would not read back as the same float.
inline std::string buildPresetJson(const float* values, const std::string& name = "Untitled")
{
   std::string j;
   j += "{\n";
   j += "  \"schema\": \"" + std::string(kPresetSchema) + "\",\n";
   j += "  \"name\": \"" + jsonEscape(name) + "\",\n";
   j += "  \"params\": {\n";
   bool first = true;
   for (uint32_t i = 0; i < kParamCount; ++i)
   {
      if (i == kParamMpe)   // controller setup, not sound: Save and Load leave it alone
         continue;
      char val[32];
      if (kParams[i].integer)
         std::snprintf(val, sizeof val, "%d", static_cast<int>(std::lround(values[i])));
      else
      {
         std::snprintf(val, sizeof val, "%.6g", values[i]);
         if (std::strtof(val, nullptr) != values[i])
            std::snprintf(val, sizeof val, "%.9g", values[i]);
      }
      if (!first) j += ",\n";
      first = false;
      j += "    \"" + std::string(kParams[i].symbol) + "\": " + val;
   }
   j += "\n  }\n";
   j += "}\n";
   return j;
}

inline bool writePresetFile(const std::string& path, const float* values, const std::string& name)
{
   const std::string json = buildPresetJson(values, name);
   std::ofstream out(path, std::ios::binary | std::ios::trunc);
   if (!out) return false;
   out.write(json.data(), static_cast<std::streamsize>(json.size()));
   return static_cast<bool>(out);
}

// What the loader keeps from a preset file.
struct PresetFileData
{
   std::string schema;
   std::string name;                                   // "" if absent
   std::vector<std::pair<std::string, float>> values;  // symbol -> value
};

namespace detail {

class PresetJsonReader
{
public:
   PresetJsonReader(const std::string& text, PresetFileData& out) : s(text), out(out) {}

   bool parse()
   {
      skipWs();
      if (!value()) return false;
      skipWs();
      return pos == s.size();
   }

private:
   void skipWs()
   {
      while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
         ++pos;
   }

   bool string(std::string& v)
   {
      if (pos >= s.size() || s[pos] != '"') return false;
      ++pos;
      v.clear();
      while (pos < s.size())
      {
         const char c = s[pos++];
         if (c == '"') return true;
         if (c != '\\') { v += c; continue; }
         if (pos >= s.size()) return false;
         const char e = s[pos++];
         switch (e)
         {
            case '"': case '\\': case '/': v += e; break;
            case 'n': case 't': case 'r': case 'b': case 'f': v += ' '; break;
            case 'u':   // not needed for any key or number; keep the name readable
               if (pos + 4 > s.size()) return false;
               pos += 4; v += '?'; break;
            default: return false;
         }
      }
      return false;
   }

   // A leaf: record it if it sits where the loader looks.
   void leaf(const std::string& text)
   {
      if (path.size() == 1 && path[0] == "schema") out.schema = text;
      else if (path.size() == 1 && path[0] == "name") out.name = text;
      else if (path.size() == 2 && path[0] == "params")
      {
         char* end = nullptr;
         const float v = std::strtof(text.c_str(), &end);
         if (end != text.c_str() && std::isfinite(v))
            out.values.emplace_back(path[1], v);
      }
   }

   bool value()
   {
      if (++depth > 32) return false;
      bool ok = false;
      if (pos >= s.size()) ok = false;
      else if (s[pos] == '{') ok = object();
      else if (s[pos] == '[') ok = array();
      else if (s[pos] == '"') { std::string v; ok = string(v); if (ok) leaf(v); }
      else
      {
         // number, true, false, null
         const size_t start = pos;
         while (pos < s.size() && std::strchr(",}] \t\r\n", s[pos]) == nullptr) ++pos;
         const std::string tok = s.substr(start, pos - start);
         ok = !tok.empty();
         if (ok && tok != "null") leaf(tok == "true" ? "1" : tok == "false" ? "0" : tok);
      }
      --depth;
      return ok;
   }

   bool object()
   {
      ++pos; skipWs();
      if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
      while (true)
      {
         std::string key;
         skipWs();
         if (!string(key)) return false;
         skipWs();
         if (pos >= s.size() || s[pos] != ':') return false;
         ++pos; skipWs();
         path.push_back(key);
         const bool ok = value();
         path.pop_back();
         if (!ok) return false;
         skipWs();
         if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
         if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
         return false;
      }
   }

   bool array()
   {
      ++pos; skipWs();
      if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
      path.push_back("[]");   // nothing the loader wants lives in an array
      while (true)
      {
         skipWs();
         if (!value()) { path.pop_back(); return false; }
         skipWs();
         if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
         path.pop_back();
         if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
         return false;
      }
   }

   const std::string&       s;
   PresetFileData&          out;
   size_t                   pos = 0;
   int                      depth = 0;
   std::vector<std::string> path;
};

} // namespace detail

// Parse preset JSON. False for malformed JSON or a file that is not a PlainOldSynth preset. The
// factory files in Presets/ have no schema (only name and params), so a file without one is taken
// when it has parameter values; one with another plugin's schema is not.
inline bool parsePresetJson(const std::string& text, PresetFileData& out)
{
   out = PresetFileData();
   detail::PresetJsonReader reader(text, out);
   if (!reader.parse()) return false;
   return out.schema == kPresetSchema || (out.schema.empty() && !out.values.empty());
}

inline bool readPresetFile(const std::string& path, PresetFileData& out)
{
   std::ifstream in(path, std::ios::binary);
   if (!in) return false;
   const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
   if (text.size() > (1u << 20)) return false;   // a preset is ~1 KB; this is not one
   return parsePresetJson(text, out);
}

} // namespace plainoldsynth

#endif // PLAINOLDSYNTH_PRESETFILE_HPP
