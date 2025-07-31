#include <stdlib.h>
#include "parser.h"
#include <unordered_map>
#include <string>
#include <sstream>

struct ParseContext {
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> values;
};

ParseContext* parseContextCreate(const char* str) {
  ParseContext* ctx = new ParseContext;

  std::istringstream stream(str);
  std::string        line;
  std::string        currentSection;

  while (std::getline(stream, line)) {
    size_t start = line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos || line[start] == '#' || line[start] == ';')
      continue;

    line = line.substr(start);
    if (line[0] == '[') {
      size_t end = line.find(']');
      if (end != std::string::npos) {
        currentSection = line.substr(1, end - 1);
      }
    } else {
      size_t eq = line.find('=');
      if (eq != std::string::npos) {
        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        size_t key_start = key.find_first_not_of(" \t");
        size_t key_end   = key.find_last_not_of(" \t");
        key              = key.substr(key_start, key_end - key_start + 1);

        size_t val_start = value.find_first_not_of(" \t");
        size_t val_end   = value.find_last_not_of(" \t\r\n");
        value            = value.substr(val_start, val_end - val_start + 1);

        ctx->values[currentSection][key] = value;
      }
    }
  }

  return ctx;
}

const char* parseContextGetValue(ParseContext* ctx, const char* section, const char* key) {
  auto it = ctx->values[std::string(section)].find(std::string(key));
  if (it != ctx->values[std::string(section)].end()) return it->second.c_str();
  return 0;
}

void parseContextDispose(ParseContext* ctx) {
  delete ctx;
}
