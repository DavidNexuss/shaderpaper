#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <X11/keysym.h>
#include "stb_image.h"
#include "glad.h"
#include <GL/gl.h>
#include <GL/glx.h>
#include "parser.h"
#define GLT_IMPLEMENTATION
#include "glText.h"
#define NK_PRIVATE


#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear.h"
#define NK_XLIB_GL3_IMPLEMENTATION
#include "nuklear_xlib_gl3.h"

#define MAX_VERTEX_BUFFER  512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024
#define MAX_LINE_LENGTH    512
#define MAX_LOG_SIZE       512 * 8
#define MAX_TEXTURE_SLOTS  32


typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

int exists(const char* fname) {
  FILE* file;
  if ((file = fopen(fname, "r"))) {
    fclose(file);
    return 1;
  }
  return 0;
}

int strndump(char* dst, const char* source, int max) {
  if (source == 0 || max <= 0) {
    if (max > 0) {
      dst[0] = 0;
    }
    return 0;
  }

  strncpy(dst, source, max - 1);
  dst[max - 1] = 0;
  return 1;
}

//=========================[GL HELPERS]===================================================

struct nk_context* ctx;

char* resolve_path(const char* path) {
  if (path[0] == '~') {
    const char* home = getenv("HOME");
    if (!home) return NULL;

    size_t len      = strlen(home) + strlen(path);
    char*  fullpath = malloc(len);
    if (!fullpath) return NULL;

    strcpy(fullpath, home);
    strcat(fullpath, path + 1);
    return fullpath;
  } else {
    return strdup(path);
  }
}
const char* basedir   = "config/";
const char* configdir = "~/.config/shaderpaper/";
const char* systemdir = "/usr/share/shaderpaper/";

void sstrcat(char* dst, const char* dir, const char* file) {
  memcpy(dst, dir, strlen(dir));
  memcpy(dst + strlen(dir), file, strlen(file));
  dst[strlen(dir) + strlen(file)] = 0;
}

char* findfile(const char* file) {
  if (exists(file)) {
    printf("Found file %s\n", file);
    return strdup(file);
  }

  char        initialPath[512];
  char*       filepath;
  const char* lookup[] = {basedir, configdir, systemdir};

  for (int i = 0; i < 3; i++) {
    sstrcat(initialPath, lookup[i], file);
    filepath = resolve_path(initialPath);

    if (exists(filepath)) {
      printf("Found file %s -> %s\n", file, filepath);
      return filepath;
    }
    free(filepath);
  }

  return 0;
}

void* fileRead(const char* abpath) {
  char* path = findfile(abpath);

  FILE* file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "File not found: %s\n", path);
    return 0;
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  if (size == 0) {
    fprintf(stderr, "File empty: %s\n", path);
    fclose(file);
    free(path);
    return 0;
  }
  fseek(file, 0, SEEK_SET);

  char* source = malloc(size + 1);
  if (!source) {
    fclose(file);
    free(path);
    return 0;
  }

  fread(source, 1, size, file);
  source[size] = '\0';
  fclose(file);
  free(path);
  return source;
}


char infolog[MAX_LOG_SIZE];

GLuint glShaderCompile(const char* path, GLenum type) {
  void* source = fileRead(path);

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, (const char**)&source, NULL);
  glCompileShader(shader);
  free(source);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    glGetShaderInfoLog(shader, sizeof(infolog), NULL, infolog);
    fprintf(stderr, "Shader compile error (%s):\n%s\n", path, infolog);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

//Compiles shader program from shader files
GLuint glProgramCompile(const char* fs, const char* vs) {
  GLuint fragShader = glShaderCompile(fs, GL_FRAGMENT_SHADER);
  GLuint vertShader = glShaderCompile(vs, GL_VERTEX_SHADER);
  if (!fragShader || !vertShader) return 0;

  GLuint program = glCreateProgram();
  glAttachShader(program, vertShader);
  glAttachShader(program, fragShader);
  glLinkProgram(program);

  glDeleteShader(fragShader);
  glDeleteShader(vertShader);

  GLint status;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    char log[512];
    glGetProgramInfoLog(program, sizeof(log), NULL, log);
    fprintf(stderr, "Program link error:\n%s\n", log);
    glDeleteProgram(program);
    return 0;
  }

  return program;
}

struct glMesh {
  GLuint vbo;
  GLuint vao;
};

struct glMesh glQuadLoad() {
  struct glMesh mesh;

  float quadData[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f, 1.0f,
    -1.0f, 1.0f,
    1.0f, -1.0f,
    1.0f, 1.0f};

  glGenBuffers(1, &mesh.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quadData), quadData, GL_STATIC_DRAW);

  glGenVertexArrays(1, &mesh.vao);
  glBindVertexArray(mesh.vao);

  glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return mesh;
}

void glMeshDispose(struct glMesh* mesh) {
  glDeleteBuffers(1, &mesh->vbo);
  glDeleteVertexArrays(1, &mesh->vao);
}

//======================================[TEXTURE]=======================================================================

struct glTexture {
  GLuint id;
  void*  data;
  int    width;
  int    height;
  int    channelCount;
  char   hdr;
};

struct glTexturePack {
  struct glTexture textures[MAX_TEXTURE_SLOTS];
  int              textureCount;
};

struct glTexturePack glTexturePackLoad(int textureCount, char** texturePaths) {
  struct glTexturePack pack = {0};
  pack.textureCount         = textureCount;

  for (int i = 0; i < textureCount; i++) {
    char* path            = findfile(texturePaths[i]);
    pack.textures[i].data = stbi_load(path, &pack.textures[i].width, &pack.textures[i].height, &pack.textures[i].channelCount, 0);
    free(path);
  }

  for (int i = 0; i < textureCount; i++) {
    if (pack.textures[i].data) {
      int  w   = pack.textures[i].width;
      int  h   = pack.textures[i].height;
      int  c   = pack.textures[i].channelCount;
      char hdr = pack.textures[i].hdr;

      glGenTextures(1, &pack.textures[i].id);
      glBindTexture(GL_TEXTURE_2D, pack.textures[i].id);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      GLenum format = GL_RGB;
      if (c == 4) format = GL_RGBA;
      else if (c == 1)
        format = GL_RED;
      glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, pack.textures[i].data);
      glGenerateMipmap(GL_TEXTURE_2D);

      free(pack.textures[i].data);
    }
  }
  return pack;
}

void glTexturePackDispose(struct glTexturePack* textures) {
  GLuint textureIds[MAX_TEXTURE_SLOTS];
  for (int i = 0; i < textures->textureCount; i++)
    textureIds[i] = textures->textures[i].id;
  glDeleteTextures(textures->textureCount, textureIds);
}

//===================================================[TEXTURE ARRAY]===================================================

GLuint glTextureArrayLoad(int textureCount, const char** texturePaths) {
  GLuint textureArrayId;
  glGenTextures(1, &textureArrayId);
  glBindTexture(GL_TEXTURE_2D_ARRAY, textureArrayId);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  int    width = 0, height = 0, channels = 0;
  void*  data = NULL;
  GLenum format;

  data = stbi_load(texturePaths[0], &width, &height, &channels, 0);
  if (!data) {
    printf("Failed to load initial texture for array: %s\n", texturePaths[0]);
    return 0;
  }

  if (channels == 4) {
    format = GL_RGBA;
  } else if (channels == 3) {
    format = GL_RGB;
  } else if (channels == 1) {
    format = GL_RED;
  } else {
    printf("Unsupported channel count: %d\n", channels);
    stbi_image_free(data);
    return 0;
  }

  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width, height, textureCount);

  for (int i = 0; i < textureCount; i++) {
    int   layerWidth, layerHeight, layerChannels;
    void* layerData = stbi_load(texturePaths[i], &layerWidth, &layerHeight, &layerChannels, 0);

    if (layerWidth != width || layerHeight != height) {
      printf("Texture %s has mismatched dimensions and cannot be added to the array.\n", texturePaths[i]);
      stbi_image_free(layerData);
      continue;
    }

    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, layerWidth, layerHeight, 1, format, GL_UNSIGNED_BYTE, layerData);
    stbi_image_free(layerData);
  }

  glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  return textureArrayId;
}

void glTextureArrayDispose(GLuint textureArrayId) {
  glDeleteTextures(1, &textureArrayId);
}

//=========================================================[CUBEMAP]==========================================================


GLuint glCubemapLoad(const char** faces) {
  GLuint cubemapId;
  glGenTextures(1, &cubemapId);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapId);

  int width, height, channels;

  for (unsigned int i = 0; i < 6; i++) {
    unsigned char* data = stbi_load(faces[i], &width, &height, &channels, 0);
    if (data) {
      GLenum format;
      if (channels == 4)
        format = GL_RGBA;
      else if (channels == 3)
        format = GL_RGB;
      else
        format = GL_RED;

      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);
    } else {
      printf("Cubemap texture failed to load at path: %s\n", faces[i]);
      stbi_image_free(data);
    }
  }

  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  return cubemapId;
}

void glCubemapDispose(GLuint cubemapId) {
  glDeleteTextures(1, &cubemapId);
}

//====================================================[FRAMEBUFFER]============================================================

struct glFrameBuffer {
  GLuint fbo;
  GLuint rt[8];
  GLuint ds;
  int    rtcount;
  int    width;
  int    height;
};

int glFrameBufferCreate(struct glFrameBuffer* framebuffer, int width, int height) {
  GLuint fbo, colorTex, depthRb;

  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  glGenTextures(1, &colorTex);
  glBindTexture(GL_TEXTURE_2D, colorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

  glGenRenderbuffers(1, &depthRb);
  glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRb);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "[ERR] Framebuffer incomplete\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 1;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  framebuffer->fbo     = fbo;
  framebuffer->rt[0]   = colorTex;
  framebuffer->ds      = depthRb;
  framebuffer->rtcount = 1;
  framebuffer->width   = width;
  framebuffer->height  = height;

  fprintf(stderr, "[OK] Framebuffer created: %dx%d\n", width, height);
  return 0;
}
int glFrameBufferResize(struct glFrameBuffer* framebuffer, int width, int height) {
  if (width == framebuffer->width && height == framebuffer->height) return 0;

  for (int i = 0; i < framebuffer->rtcount; i++) {
    glBindTexture(GL_TEXTURE_2D, framebuffer->rt[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  }
  glBindRenderbuffer(GL_RENDERBUFFER, framebuffer->ds);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

  framebuffer->width  = width;
  framebuffer->height = height;

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  fprintf(stderr, "[OK] Framebuffer resized: %dx%d\n", width, height);
  return 0;
}

int glFrameBufferDispose(struct glFrameBuffer* framebuffer) {
  glDeleteFramebuffers(1, &framebuffer->fbo);
  glDeleteTextures(framebuffer->rtcount, framebuffer->rt);
  glDeleteRenderbuffers(1, &framebuffer->ds);
  return 0;
}

//=========================[CONFIG PARSER LIB]===================================================


enum ShaderMode {
  SHADER_MODE_NONE = 0,
  SHADER_MODE_SHADER
};

enum ShaderMode getShaderMode(const char* sessionmode) {
  if (sessionmode == 0) return 0;
  enum ShaderMode mode;
  if (strcmp(sessionmode, "shader") == 0) return SHADER_MODE_SHADER;
  return SHADER_MODE_NONE;
}

//===========================[CONFIG]===================================================================

struct SessionConfiguration {
  enum ShaderMode mode;
  int             upscalingFactor;
  char            vertexShader[MAX_LINE_LENGTH];
  char            fragmentShader[MAX_LINE_LENGTH];
  char            texturePath[MAX_TEXTURE_SLOTS][MAX_LINE_LENGTH];
  int             textureCount;
};

void sessionConfigurationPrint(struct SessionConfiguration* configuration) {
  printf("mode: %d\n", configuration->mode);
  printf("vertexshader: %s\n", configuration->vertexShader);
  printf("fragmentshader: %s\n", configuration->fragmentShader);
  printf("\n");
}

int sessionConfigurationParse(struct SessionConfiguration* configuration, const char* path) {
  void* fdata = fileRead(path);
  if (fdata == 0) {
    fprintf(stderr, "Config file not found %s\n", path);
    return 1;
  }

  struct ParseContext* ctx = parseContextCreate(fdata);

  configuration->mode            = getShaderMode(parseContextGetValue(ctx, "general", "shadermode"));
  configuration->upscalingFactor = 1;
  configuration->textureCount    = 0;

  if (configuration->mode == SHADER_MODE_SHADER) {
    strndump(configuration->fragmentShader, parseContextGetValue(ctx, "shadermode/shader", "fragmentshader"), MAX_LINE_LENGTH);
    strndump(configuration->vertexShader, parseContextGetValue(ctx, "shadermode/shader", "vertexshader"), MAX_LINE_LENGTH);

    for (int i = 0; i < MAX_TEXTURE_SLOTS; i++) {
      char uniformName[MAX_LINE_LENGTH];
      sprintf(uniformName, "usertexture%d", i + 1);

      const char* texturePath = parseContextGetValue(ctx, "shadermode/uniforms", uniformName);
      if (texturePath != NULL) {
        strndump(configuration->texturePath[configuration->textureCount], texturePath, MAX_LINE_LENGTH);
        configuration->textureCount++;
      }
    }
  }

  parseContextDispose(ctx);
  free(fdata);
  sessionConfigurationPrint(configuration);
  return 0;
}

//====================================================[UNIFORMS]=============================================

struct ShaderUniforms {
  //Uniform data
  float quality;

  float cameraPosition[3];
  float cameraVelocity[3];
  float time;
  float x;
  float y;
  float width;
  float height;
  float zoom;
  float scroll;
  float battery;
  float volume;
  int   keyStates[32];
  int   joyStates[32];
  int   sampleStates[128];
  int   userTextures[32];

  float maxVolume;

  //System data
  GLuint userTexturesId[MAX_TEXTURE_SLOTS];
  int    userTexturesCount;

  //Locations
  GLint iQuality;
  GLint iCameraPosition;
  GLint iCameraVelocity;
  GLint iTime;
  GLint iX;
  GLint iY;
  GLint iMouse;
  GLint iWidth;
  GLint iHeight;
  GLint iResolution;
  GLint iZoom;
  GLint iScroll;
  GLint iBattery;
  GLint iVolume;
  GLint iKeyStates;
  GLint iJoyStates;
  GLint iSampleStates;
  GLint iUserTextures;
  GLint iMaxVolume;
};

void shaderUniformsInitLocations(struct ShaderUniforms* u, GLuint program) {
#define GET_LOC(field, nameStr)                                \
  {                                                            \
    u->field = glGetUniformLocation(program, nameStr);         \
    if (u->field != -1) printf("Using uniform " nameStr "\n"); \
  }

  GET_LOC(iQuality, "iQuality");
  GET_LOC(iCameraPosition, "iCameraPosition");
  GET_LOC(iCameraVelocity, "iCameraVelocity");
  GET_LOC(iTime, "iTime");
  GET_LOC(iX, "iX");
  GET_LOC(iY, "iY");
  GET_LOC(iMouse, "iMouse");
  GET_LOC(iWidth, "iWidth");
  GET_LOC(iHeight, "iHeight");
  GET_LOC(iResolution, "iResolution"); // Get location for new iResolution uniform
  GET_LOC(iZoom, "iZoom");
  GET_LOC(iScroll, "iScroll");
  GET_LOC(iBattery, "iBattery");
  GET_LOC(iVolume, "iVolume");
  GET_LOC(iMaxVolume, "iMaxVolume");

  GET_LOC(iKeyStates, "iKeyStates");
  GET_LOC(iJoyStates, "iJoyStates");
  GET_LOC(iSampleStates, "iSampleStates");
  GET_LOC(iUserTextures, "iUserTextures");

#undef GET_LOC
}

void shaderUniformsUpload(struct ShaderUniforms* u) {
  if (u->iQuality != -1) glUniform1f(u->iQuality, u->quality);
  if (u->iCameraPosition != -1) glUniform3fv(u->iCameraPosition, 1, u->cameraPosition);
  if (u->iCameraVelocity != -1) glUniform3fv(u->iCameraVelocity, 1, u->cameraVelocity);
  if (u->iTime != -1) glUniform1f(u->iTime, u->time);
  if (u->iX != -1) glUniform1f(u->iX, u->x);
  if (u->iY != -1) glUniform1f(u->iY, u->y);
  if (u->iMouse != -1) glUniform2f(u->iMouse, u->x, u->y);
  if (u->iWidth != -1) glUniform1f(u->iWidth, u->width);
  if (u->iHeight != -1) glUniform1f(u->iHeight, u->height);
  if (u->iResolution != -1) glUniform3f(u->iResolution, u->width, u->height, 1.0f); // Upload iResolution
  if (u->iZoom != -1) glUniform1f(u->iZoom, u->zoom);
  if (u->iScroll != -1) glUniform1f(u->iScroll, u->scroll);
  if (u->iBattery != -1) glUniform1f(u->iBattery, u->battery);
  if (u->iVolume != -1) glUniform1f(u->iVolume, u->volume);
  if (u->iMaxVolume != -1) glUniform1f(u->iMaxVolume, u->maxVolume);

  if (u->iKeyStates != -1) glUniform1iv(u->iKeyStates, 32, u->keyStates);
  if (u->iJoyStates != -1) glUniform1iv(u->iJoyStates, 32, u->joyStates);
  if (u->iSampleStates != -1) glUniform1iv(u->iSampleStates, 128, u->sampleStates);

  if (u->iUserTextures != -1) {
    int textureUnits[MAX_TEXTURE_SLOTS];
    for (int i = 0; i < u->userTexturesCount; ++i) textureUnits[i] = i;
    glUniform1iv(u->iUserTextures, u->userTexturesCount, textureUnits);
  }

  for (int i = 0; i < u->userTexturesCount; ++i) {
    if (u->userTexturesId[i]) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, u->userTexturesId[i]);
    }
  }
}

struct InputState {
  int   mouseX;
  int   mouseY;
  int   windowWidth;
  int   windowHeight;
  int   keyStates[32];
  float scrollDelta;
};

#define KEY_W_INDEX               0
#define KEY_A_INDEX               1
#define KEY_S_INDEX               2
#define KEY_D_INDEX               3
#define KEY_SPACE_INDEX           4
#define KEY_LEFT_SHIFT_INDEX      5
#define KEY_LEFT_CTRL_INDEX       6
#define KEY_ESC_INDEX             7
#define MOUSE_LEFT_BUTTON_INDEX   29
#define MOUSE_RIGHT_BUTTON_INDEX  30
#define MOUSE_MIDDLE_BUTTON_INDEX 31

int getSimplifiedKeyIndex(KeyCode keycode) {
  switch (keycode) {
    case 25: return KEY_W_INDEX;
    case 38: return KEY_A_INDEX;
    case 39: return KEY_S_INDEX;
    case 40: return KEY_D_INDEX;
    case 65: return KEY_SPACE_INDEX;
    case 50: return KEY_LEFT_SHIFT_INDEX;
    case 37: return KEY_LEFT_CTRL_INDEX;
    case 9: return KEY_ESC_INDEX;
    default: return -1; // Unmapped key
  }
}

void shaderUniformsUpdate(struct ShaderUniforms* u, const struct InputState* input, float currentTime) {
  u->time = currentTime;

  u->width  = (float)input->windowWidth;
  u->height = (float)input->windowHeight;

  u->x = (float)input->mouseX;
  u->y = (float)input->mouseY;

  for (int i = 0; i < 32; ++i) {
    u->keyStates[i] = input->keyStates[i];
  }

  u->scroll = input->scrollDelta;

  u->quality           = 1.0f; // Example default
  u->zoom              = 1.0f; // Example default
  u->battery           = 1.0f; // Placeholder (e.g., from a power management API)
  u->volume            = 0.5f; // Placeholder (e.g., from an audio mixer API)
  u->maxVolume         = 1.0f; // Placeholder
  u->cameraPosition[0] = 0.0f;
  u->cameraPosition[1] = 0.0f;
  u->cameraPosition[2] = 0.0f; // Example default
  u->cameraVelocity[0] = 0.0f;
  u->cameraVelocity[1] = 0.0f;
  u->cameraVelocity[2] = 0.0f;                         // Example default
  memset(u->joyStates, 0, sizeof(u->joyStates));       // Clear if not updated by joystick input
  memset(u->sampleStates, 0, sizeof(u->sampleStates)); // Clear if not updated by audio samples
}

//=========================================================[SESSION]=====================================================

struct ShaderSession {
  GLuint                      shaderProgram;
  struct SessionConfiguration config;
  struct glMesh               quad;
  struct glFrameBuffer        fbo;
  struct ShaderUniforms       uniforms;
  struct glTexturePack        usertextures;

  int      screenWidth;
  int      screenHeight;
  int      fboWidth;
  int      fboHeight;
  GLTtext* errorText;
};

int shaderSessionLoadProgram(struct ShaderSession* session) {
  GLuint program = glProgramCompile(session->config.fragmentShader, session->config.vertexShader);

  if (!program) {
    fprintf(stderr, "Error compiling shaders %s %s\n", session->config.vertexShader, session->config.fragmentShader);
    gltSetText(session->errorText, infolog);
    return 1;
  } else {
    fprintf(stderr, "[OK] Shader compilation.\n");
  }
  session->shaderProgram = program;
  glUseProgram(session->shaderProgram);

  shaderUniformsInitLocations(&session->uniforms, session->shaderProgram);
  shaderUniformsUpload(&session->uniforms);
  return 0;
}

int shaderSessionLoadUserTextures(struct ShaderSession* session) {
  char* texturesPaths[MAX_TEXTURE_SLOTS];

  for (int i = 0; i < session->config.textureCount; i++)
    texturesPaths[i] = session->config.texturePath[i];

  session->usertextures = glTexturePackLoad(session->config.textureCount, texturesPaths);
  return 0;
}

int shaderSessionLoadMesh(struct ShaderSession* session) {
  session->quad = glQuadLoad();
  return 0;
}

int shaderSessionCreate(struct ShaderSession* session, const char* configfile) {
  if (sessionConfigurationParse(&session->config, configfile)) {
    fprintf(stderr, "Error parsing config file %s\n", configfile);
    return 1;
  }

  session->errorText = gltCreateText();
  shaderSessionLoadUserTextures(session);
  shaderSessionLoadMesh(session);

  for (int i = 0; i < session->usertextures.textureCount; i++)
    session->uniforms.userTexturesId[i] = session->usertextures.textures[i].id;

  session->uniforms.userTexturesCount = session->usertextures.textureCount;

  shaderSessionLoadProgram(session);
  glFrameBufferCreate(&session->fbo, 720, 640);

  return 0;
}

void shaderSessionBeginFBO(struct ShaderSession* session) {

  session->screenWidth  = session->uniforms.width;
  session->screenHeight = session->uniforms.height;

  session->fboWidth  = session->screenWidth / session->config.upscalingFactor;
  session->fboHeight = session->screenHeight / session->config.upscalingFactor;

  glFrameBufferResize(&session->fbo, session->fboWidth, session->fboHeight);

  glBindFramebuffer(GL_FRAMEBUFFER, session->fbo.fbo);

  glViewport(0, 0, session->fboWidth, session->fboHeight);

  glClearColor(0.0, 0.0, 0.0, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void shaderSessionEndFBO(struct ShaderSession* session) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, session->fbo.fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

  glBlitFramebuffer(
    0, 0, session->fboWidth, session->fboHeight,
    0, 0, session->screenWidth, session->screenHeight,
    GL_COLOR_BUFFER_BIT,
    GL_LINEAR);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, session->screenWidth, session->screenHeight);
}

int shaderSessionDrawErrored(struct ShaderSession* session) {
  gltBeginDraw();

  gltColor(1.0f, 1.0f, 1.0f, 1.0f);
  gltDrawText2D(session->errorText, 20, 20, 1.0f);

  gltEndDraw();
  return 0;
}

int shaderSessionConfigMenu(struct ShaderSession* session) {
  if (nk_begin(ctx, "Render Configuration", nk_rect(50, 50, 200, 200),
               NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                 NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE)) {
    nk_layout_row_dynamic(ctx, 25, 1);
    nk_property_int(ctx, "upscalingFactor:", 1, &session->config.upscalingFactor, 12, 1, 1);
  }
  nk_end(ctx);
  return 0;
}

int shaderSessionDraw(struct ShaderSession* session) {
  if (session->shaderProgram == 0) {
    shaderSessionDrawErrored(session);
    return 0;
  }

  if (session->config.upscalingFactor > 1)
    shaderSessionBeginFBO(session);

  glUseProgram(session->shaderProgram);
  shaderUniformsUpload(&session->uniforms);
  glBindVertexArray(session->quad.vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);

  if (session->config.upscalingFactor > 1)
    shaderSessionEndFBO(session);

  return 0;
}

int shaderSessionDispose(struct ShaderSession* session) {
  gltDeleteText(session->errorText);
  glDeleteProgram(session->shaderProgram);
  glMeshDispose(&session->quad);
  glTexturePackDispose(&session->usertextures);
  glFrameBufferDispose(&session->fbo);
  return 0;
}

//====================================[APPLICATION]==================================================

void printUsage() {
  fprintf(stderr, "Usage: <executable> <config_file_path>\n");
}

struct nk_colorf bg;

int application(int argc, char** argv, Display* dpy, Window win) {
  if (!(argc > 1)) {
    fprintf(stderr, "Not enough config file expected...\n");
    printUsage();

    // Render a basic triangle implicit shading
    while (1) {
      XEvent ev;
      while (XPending(dpy)) {
        XNextEvent(dpy, &ev);
      }

      glClearColor(0.1f, 0.0f, 0.2f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);

      glBegin(GL_TRIANGLES);
      glColor3f(1.0, 0.0, 0.0);
      glVertex2f(-0.6f, -0.4f);
      glColor3f(0.0, 1.0, 0.0);
      glVertex3f(0.6f, -0.4f, 0.0f);
      glColor3f(0.0, 0.0, 1.0);
      glVertex3f(0.0f, 0.6f, 0.0f);
      glEnd();

      glXSwapBuffers(dpy, win);
      usleep(16000);
    }
    return 1;
  }

  struct ShaderSession session    = {0};
  struct InputState    inputState = {0};

  // Set initial window dimensions
  XWindowAttributes wa;
  XGetWindowAttributes(dpy, win, &wa);
  inputState.windowWidth  = wa.width;
  inputState.windowHeight = wa.height;
  glViewport(0, 0, inputState.windowWidth, inputState.windowHeight); // Set initial viewport

  // Select input events to listen for
  XSelectInput(dpy, win, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask); // For ConfigureNotify (resize)

  const char* configfile = argv[1];

  if (shaderSessionCreate(&session, configfile)) {
    fprintf(stderr, "Error initializing session\n");
    return 1;
  }

  struct timeval start_time, current_time;
  gettimeofday(&start_time, NULL);

  while (1) {
    XEvent ev;
    nk_input_begin(ctx);
    // Process all pending events
    while (XPending(dpy)) {
      XNextEvent(dpy, &ev);
      nk_x11_handle_event(&ev);

      switch (ev.type) {
        case ConfigureNotify:
          // Window was resized
          if (ev.xconfigure.width != inputState.windowWidth ||
              ev.xconfigure.height != inputState.windowHeight) {
            inputState.windowWidth  = ev.xconfigure.width;
            inputState.windowHeight = ev.xconfigure.height;
            glViewport(0, 0, inputState.windowWidth, inputState.windowHeight);
          }
          break;
        case MotionNotify:
          // Mouse moved
          inputState.mouseX = ev.xmotion.x;
          inputState.mouseY = ev.xmotion.y;
          break;
        case KeyPress: {
          // Key pressed
          int keyIdx = getSimplifiedKeyIndex(ev.xkey.keycode);
          if (keyIdx != -1) {
            inputState.keyStates[keyIdx] = 1; // Set key state to 1 (pressed)
          }
          break;
        }
        case KeyRelease: {
          // Key released
          // X11 can generate duplicate KeyRelease events if auto-repeat is on.
          // To avoid this, check if the key is truly up.
          if (XEventsQueued(dpy, QueuedAfterReading)) {
            XEvent next_ev;
            XPeekEvent(dpy, &next_ev);
            if (next_ev.type == KeyPress &&
                next_ev.xkey.time == ev.xkey.time &&
                next_ev.xkey.keycode == ev.xkey.keycode) {
              // This is an auto-repeat, ignore the KeyRelease
              XNextEvent(dpy, &ev); // Consume the KeyPress to remove it from queue
              break;
            }
          }
          int keyIdx = getSimplifiedKeyIndex(ev.xkey.keycode);
          if (keyIdx != -1) {
            inputState.keyStates[keyIdx] = 0; // Set key state to 0 (released)
          }
          break;
        }
        case ButtonPress:
          // Mouse button pressed
          switch (ev.xbutton.button) {
            case Button1: inputState.keyStates[MOUSE_LEFT_BUTTON_INDEX] = 1; break;
            case Button2: inputState.keyStates[MOUSE_MIDDLE_BUTTON_INDEX] = 1; break;
            case Button3: inputState.keyStates[MOUSE_RIGHT_BUTTON_INDEX] = 1; break;
            case Button4: inputState.scrollDelta += 1.0f; break; // Scroll up
            case Button5: inputState.scrollDelta -= 1.0f; break; // Scroll down
          }
          break;
        case ButtonRelease:
          // Mouse button released
          switch (ev.xbutton.button) {
            case Button1: inputState.keyStates[MOUSE_LEFT_BUTTON_INDEX] = 0; break;
            case Button2: inputState.keyStates[MOUSE_MIDDLE_BUTTON_INDEX] = 0; break;
            case Button3:
              inputState.keyStates[MOUSE_RIGHT_BUTTON_INDEX] = 0;
              break;
              // For scroll, we don't clear the delta on release, it's just a pulse.
              // The uniform will be updated with the current delta.
          }
          break;
      }
    }

    nk_input_end(ctx);

    shaderSessionConfigMenu(&session);
    //applicationGuiTest();

    // Calculate elapsed time
    gettimeofday(&current_time, NULL);
    float elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
      (current_time.tv_usec - start_time.tv_usec) / 1000000.0f;

    // Update uniforms with current input state and time
    shaderUniformsUpdate(&session.uniforms, &inputState, elapsed_time);

    glClearColor(0.0f, 0.0f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    shaderSessionDraw(&session);
    nk_x11_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);

    glXSwapBuffers(dpy, win);
    usleep(16000); // Approximately 60 FPS
  }
}

//====================================[DRIVER CODE]==========================================

enum theme { THEME_BLACK,
             THEME_WHITE,
             THEME_RED,
             THEME_BLUE,
             THEME_DARK };

void set_style(struct nk_context* ctx, enum theme theme);

void nuklearInit(Window win, Display* dpy) {
  ctx = nk_x11_init(dpy, win);
  struct nk_font_atlas* atlas;
  nk_x11_font_stash_begin(&atlas);

  char* fontfile = findfile("font.ttf");

  if (!fontfile) {
    fprintf(stderr, "Could not find font file font.ttf");
  }

  struct nk_font* font = nk_font_atlas_add_from_file(atlas, fontfile, 18, 0);

  nk_x11_font_stash_end();
  nk_style_set_font(ctx, &font->handle);

  set_style(ctx, THEME_DARK);
  free(fontfile);
}
void nuklearDispose() {
  nk_x11_shutdown();
}
int main(int argc, char** argv) {
  Display* dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "Cannot open display\n");
    return 1;
  }

  int    screen = DefaultScreen(dpy);
  Window root   = RootWindow(dpy, screen);

  static int visualAttribs[] = {
    GLX_X_RENDERABLE, True,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 8,
    GLX_DEPTH_SIZE, 24,
    GLX_STENCIL_SIZE, 8,
    GLX_DOUBLEBUFFER, True,
    None};

  int fbCount = 0;

  GLXFBConfig* fbConfigs = glXChooseFBConfig(dpy, screen, visualAttribs, &fbCount);
  if (!fbConfigs || fbCount == 0) {
    fprintf(stderr, "No suitable framebuffer configs found\n");
    return 1;
  }

  GLXFBConfig fbConfig = fbConfigs[0];
  XFree(fbConfigs); // Clean up

  XVisualInfo* vi = glXGetVisualFromFBConfig(dpy, fbConfig);
  if (!vi) {
    fprintf(stderr, "No suitable visual found from FBConfig\n");
    return 1;
  }

  Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  XSetWindowAttributes swa;
  swa.colormap   = cmap;
  swa.event_mask = ExposureMask; // Initial event mask, will be updated in application()

  int width  = DisplayWidth(dpy, screen);
  int height = DisplayHeight(dpy, screen);

  Window win = XCreateWindow(dpy, root,
                             0, 0,
                             width, height,
                             0,
                             vi->depth,
                             InputOutput,
                             vi->visual,
                             CWColormap | CWEventMask,
                             &swa);

  XSizeHints* size_hints = XAllocSizeHints();
  if (size_hints) {
    size_hints->flags      = PMinSize | PMaxSize;
    size_hints->min_width  = width;
    size_hints->max_width  = width;
    size_hints->min_height = height;
    size_hints->max_height = height;
    XSetWMNormalHints(dpy, win, size_hints);
    XFree(size_hints);
  }

  Atom wm_type         = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  Atom wm_type_desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
  XChangeProperty(dpy, win, wm_type, XA_ATOM, 32, PropModeReplace,
                  (unsigned char*)&wm_type_desktop, 1);

  Atom wm_state       = XInternAtom(dpy, "_NET_WM_STATE", False);
  Atom wm_state_below = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
  XChangeProperty(dpy, win, wm_state, XA_ATOM, 32, PropModeAppend,
                  (unsigned char*)&wm_state_below, 1);

  XMapWindow(dpy, win);
  XFlush(dpy);

  XLowerWindow(dpy, win);
  XFlush(dpy);

  int majorVersion = 3;
  int minorVersion = 3;

  int context_attribs[] = {
    GLX_CONTEXT_MAJOR_VERSION_ARB, majorVersion,
    GLX_CONTEXT_MINOR_VERSION_ARB, minorVersion,
    GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
    None};

  glXCreateContextAttribsARBProc glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

  GLXContext glc = glXCreateContextAttribsARB(dpy, fbConfig, 0, True, context_attribs);

  glXMakeCurrent(dpy, win, glc);

  if (!gladLoadGL()) {
    fprintf(stderr, "Failed to load glad.\n");
    return 1;
  }

  gltInit();
  nuklearInit(win, dpy);

  application(argc, argv, dpy, win);

  nuklearDispose();
  gltTerminate();

  glXMakeCurrent(dpy, None, NULL);
  glXDestroyContext(dpy, glc);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);

  return 0;
}

void set_style(struct nk_context* ctx, enum theme theme) {
  struct nk_color table[NK_COLOR_COUNT];
  if (theme == THEME_WHITE) {
    table[NK_COLOR_TEXT]                    = nk_rgba(70, 70, 70, 255);
    table[NK_COLOR_WINDOW]                  = nk_rgba(175, 175, 175, 255);
    table[NK_COLOR_HEADER]                  = nk_rgba(175, 175, 175, 255);
    table[NK_COLOR_BORDER]                  = nk_rgba(0, 0, 0, 255);
    table[NK_COLOR_BUTTON]                  = nk_rgba(185, 185, 185, 255);
    table[NK_COLOR_BUTTON_HOVER]            = nk_rgba(170, 170, 170, 255);
    table[NK_COLOR_BUTTON_ACTIVE]           = nk_rgba(160, 160, 160, 255);
    table[NK_COLOR_TOGGLE]                  = nk_rgba(150, 150, 150, 255);
    table[NK_COLOR_TOGGLE_HOVER]            = nk_rgba(120, 120, 120, 255);
    table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(175, 175, 175, 255);
    table[NK_COLOR_SELECT]                  = nk_rgba(190, 190, 190, 255);
    table[NK_COLOR_SELECT_ACTIVE]           = nk_rgba(175, 175, 175, 255);
    table[NK_COLOR_SLIDER]                  = nk_rgba(190, 190, 190, 255);
    table[NK_COLOR_SLIDER_CURSOR]           = nk_rgba(80, 80, 80, 255);
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(70, 70, 70, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(60, 60, 60, 255);
    table[NK_COLOR_PROPERTY]                = nk_rgba(175, 175, 175, 255);
    table[NK_COLOR_EDIT]                    = nk_rgba(150, 150, 150, 255);
    table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(0, 0, 0, 255);
    table[NK_COLOR_COMBO]                   = nk_rgba(175, 175, 175, 255);
    table[NK_COLOR_CHART]                   = nk_rgba(160, 160, 160, 255);
    table[NK_COLOR_CHART_COLOR]             = nk_rgba(45, 45, 45, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR]               = nk_rgba(180, 180, 180, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(140, 140, 140, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgba(150, 150, 150, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(160, 160, 160, 255);
    table[NK_COLOR_TAB_HEADER]              = nk_rgba(180, 180, 180, 255);
    nk_style_from_table(ctx, table);
  } else if (theme == THEME_RED) {
    table[NK_COLOR_TEXT]                    = nk_rgba(190, 190, 190, 255);
    table[NK_COLOR_WINDOW]                  = nk_rgba(30, 33, 40, 215);
    table[NK_COLOR_HEADER]                  = nk_rgba(181, 45, 69, 220);
    table[NK_COLOR_BORDER]                  = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_BUTTON]                  = nk_rgba(181, 45, 69, 255);
    table[NK_COLOR_BUTTON_HOVER]            = nk_rgba(190, 50, 70, 255);
    table[NK_COLOR_BUTTON_ACTIVE]           = nk_rgba(195, 55, 75, 255);
    table[NK_COLOR_TOGGLE]                  = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_TOGGLE_HOVER]            = nk_rgba(45, 60, 60, 255);
    table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(181, 45, 69, 255);
    table[NK_COLOR_SELECT]                  = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_SELECT_ACTIVE]           = nk_rgba(181, 45, 69, 255);
    table[NK_COLOR_SLIDER]                  = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_SLIDER_CURSOR]           = nk_rgba(181, 45, 69, 255);
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(186, 50, 74, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(191, 55, 79, 255);
    table[NK_COLOR_PROPERTY]                = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_EDIT]                    = nk_rgba(51, 55, 67, 225);
    table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(190, 190, 190, 255);
    table[NK_COLOR_COMBO]                   = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_CHART]                   = nk_rgba(51, 55, 67, 255);
    table[NK_COLOR_CHART_COLOR]             = nk_rgba(170, 40, 60, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR]               = nk_rgba(30, 33, 40, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(64, 84, 95, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgba(70, 90, 100, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
    table[NK_COLOR_TAB_HEADER]              = nk_rgba(181, 45, 69, 220);
    nk_style_from_table(ctx, table);
  } else if (theme == THEME_BLUE) {
    table[NK_COLOR_TEXT]                    = nk_rgba(20, 20, 20, 255);
    table[NK_COLOR_WINDOW]                  = nk_rgba(202, 212, 214, 215);
    table[NK_COLOR_HEADER]                  = nk_rgba(137, 182, 224, 220);
    table[NK_COLOR_BORDER]                  = nk_rgba(140, 159, 173, 255);
    table[NK_COLOR_BUTTON]                  = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_BUTTON_HOVER]            = nk_rgba(142, 187, 229, 255);
    table[NK_COLOR_BUTTON_ACTIVE]           = nk_rgba(147, 192, 234, 255);
    table[NK_COLOR_TOGGLE]                  = nk_rgba(177, 210, 210, 255);
    table[NK_COLOR_TOGGLE_HOVER]            = nk_rgba(182, 215, 215, 255);
    table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_SELECT]                  = nk_rgba(177, 210, 210, 255);
    table[NK_COLOR_SELECT_ACTIVE]           = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_SLIDER]                  = nk_rgba(177, 210, 210, 255);
    table[NK_COLOR_SLIDER_CURSOR]           = nk_rgba(137, 182, 224, 245);
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(142, 188, 229, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(147, 193, 234, 255);
    table[NK_COLOR_PROPERTY]                = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_EDIT]                    = nk_rgba(210, 210, 210, 225);
    table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(20, 20, 20, 255);
    table[NK_COLOR_COMBO]                   = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_CHART]                   = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_CHART_COLOR]             = nk_rgba(137, 182, 224, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR]               = nk_rgba(190, 200, 200, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(64, 84, 95, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgba(70, 90, 100, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(75, 95, 105, 255);
    table[NK_COLOR_TAB_HEADER]              = nk_rgba(156, 193, 220, 255);
    nk_style_from_table(ctx, table);
  } else if (theme == THEME_DARK) {
    table[NK_COLOR_TEXT]                    = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_WINDOW]                  = nk_rgba(0, 0, 0, 230);     // 90% black
    table[NK_COLOR_HEADER]                  = nk_rgba(0, 0, 0, 220);     // Accent color
    table[NK_COLOR_BORDER]                  = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_BUTTON]                  = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_BUTTON_HOVER]            = nk_rgba(0, 128, 255, 255); // Same as button
    table[NK_COLOR_BUTTON_ACTIVE]           = nk_rgba(0, 128, 255, 255); // Same as button
    table[NK_COLOR_TOGGLE]                  = nk_rgba(0, 0, 0, 255);
    table[NK_COLOR_TOGGLE_HOVER]            = nk_rgba(45, 53, 56, 255);
    table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_SELECT]                  = nk_rgba(57, 67, 61, 255);
    table[NK_COLOR_SELECT_ACTIVE]           = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_SLIDER]                  = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SLIDER_CURSOR]           = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_PROPERTY]                = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_EDIT]                    = nk_rgba(50, 58, 61, 225);
    table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(210, 210, 210, 255);
    table[NK_COLOR_COMBO]                   = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART]                   = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_CHART_COLOR]             = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(255, 0, 0, 255);
    table[NK_COLOR_SCROLLBAR]               = nk_rgba(50, 58, 61, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(0, 128, 255, 255); // Accent color
    table[NK_COLOR_TAB_HEADER]              = nk_rgba(0, 128, 255, 255); // Accent color

    nk_style_from_table(ctx, table);
  } else {
    nk_style_default(ctx);
  }
}
