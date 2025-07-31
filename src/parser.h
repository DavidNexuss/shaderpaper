#pragma once
#ifdef __cplusplus
extern "C" {
#endif
struct ParseContext;
struct ParseContext* parseContextCreate(const char* str);
const char*          parseContextGetValue(struct ParseContext* ctx, const char* section, const char* key);
void                 parseContextDispose(struct ParseContext* ctx);

#ifdef __cplusplus
}
#endif
