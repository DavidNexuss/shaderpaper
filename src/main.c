#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "nuklear.h"
#include "stb_image.h"
#include "string.h"
#include "glad.h"
#include <GL/gl.h>
#include <GL/glx.h>

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

//=========================[GL HELPERS]===================================================

void* fileRead(const char* path) {
  FILE* file = fopen(path, "rb");
  if (!file) return 0;

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* source = malloc(size + 1);
  if (!source) {
    fclose(file);
    return 0;
  }

  fread(source, 1, size, file);
  source[size] = '\0';
  fclose(file);
  return source;
}


//Loads shader from file path and type
GLuint glShaderCompile(const char* path, GLenum type) {
  FILE* file = fopen(path, "rb");
  if (!file) return 0;

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* source = malloc(size + 1);
  if (!source) {
    fclose(file);
    return 0;
  }

  fread(source, 1, size, file);
  source[size] = '\0';
  fclose(file);

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, (const char**)&source, NULL);
  glCompileShader(shader);
  free(source);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    char log[512];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compile error (%s):\n%s\n", path, log);
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

struct glTexture {
  GLuint id;
  int    width;
  int    height;
  int    channelCount;
  char   hdr;
};

struct glTexturePack {
  struct glTexture textures[32];
  int              textureCount;
};

struct glTexture glTextureLoad(const char* path) {
}
struct glTexturePack glTexturePackLoad(int textureCount, char** texturePaths) {
  struct glTexturePack pack;
  pack.textureCount = textureCount;
  for (int i = 0; i < textureCount; i++) {
    pack.textures[i] = glTextureLoad(texturePaths[i]);
  }
  return pack;
}

//=========================[CONFIG PARSER LIB]===================================================

#define MAX_LINE_LENGTH 512

int cfgGet(const char* currPos, const char* sec, const char* k, char* buf, size_t bufSize) {
  if (!currPos || !k || !buf || bufSize == 0) return 1;

  const char* searchEnd = NULL;

  if (sec) {
    char secHdr[MAX_LINE_LENGTH];
    snprintf(secHdr, MAX_LINE_LENGTH, "[%s]", sec);

    const char* secStartMarker = strstr(currPos, secHdr);
    if (!secStartMarker) return 1;

    currPos = strchr(secStartMarker, '\n');
    if (!currPos) return 1;
    currPos++;

    const char* nextSecMarker = strstr(currPos, "[");
    if (nextSecMarker) searchEnd = nextSecMarker;
    else
      searchEnd = currPos + strlen(currPos);
  } else {
    const char* firstSecMarker = strstr(currPos, "[");
    if (firstSecMarker) searchEnd = firstSecMarker;
    else
      searchEnd = currPos + strlen(currPos);
  }

  char lineBuf[MAX_LINE_LENGTH];
  char searchKeyPrefix[MAX_LINE_LENGTH];
  snprintf(searchKeyPrefix, MAX_LINE_LENGTH, "%s=", k);
  size_t keyPrefixLen = strlen(searchKeyPrefix);

  while (currPos && (searchEnd == NULL || currPos < searchEnd)) {
    const char* lineEnd = strchr(currPos, '\n');
    size_t      lineLen;

    if (lineEnd) lineLen = lineEnd - currPos;
    else
      lineLen = strlen(currPos);

    if (lineLen >= MAX_LINE_LENGTH) {
      currPos = lineEnd ? (lineEnd + 1) : NULL;
      continue;
    }

    strncpy(lineBuf, currPos, lineLen);
    lineBuf[lineLen] = '\0';

    if (strncmp(lineBuf, searchKeyPrefix, keyPrefixLen) == 0) {
      if (sscanf(lineBuf + keyPrefixLen, "%s", buf) == 1) {
        if (strlen(buf) >= bufSize) buf[bufSize - 1] = '\0';
        return 0;
      }
    }
    currPos = lineEnd ? (lineEnd + 1) : NULL;
  }
  return 1;
}


//==========================================[APPLICATION CODE]==========================================

enum ShaderMode {
  SHADER_MODE_NONE = 0,
  SHADER_MODE_SHADER
};

enum ShaderMode getShaderMode(const char* sessionmode) {
  enum ShaderMode mode;
  if (strcmp(sessionmode, "shader")) return SHADER_MODE_SHADER;
  return SHADER_MODE_NONE;
}

struct SessionConfiguration {
  enum ShaderMode mode;

  char vertexShader[MAX_LINE_LENGTH];
  char fragmentShader[MAX_LINE_LENGTH];
};

int SessionConfigurationParse(struct SessionConfiguration* configuration, const char* path) {
  void* fdata = fileRead(path);
  if (fdata == 0) {
    fprintf(stderr, "Config file not found %s\n", path);
    return 1;
  }

  char shadermode[256];
  if (cfgGet(fdata, "general", "shadermode", shadermode, MAX_LINE_LENGTH)) return 1;
  configuration->mode = getShaderMode(shadermode);

  if (configuration->mode == SHADER_MODE_SHADER) {
    if (cfgGet(fdata, "shader", "vertexshader", configuration->vertexShader, MAX_LINE_LENGTH)) return 1;
    if (cfgGet(fdata, "shader", "fragmentShader", configuration->fragmentShader, MAX_LINE_LENGTH)) return 1;
  }

  return 0;
}

void SessionConfigurationPrint(struct SessionConfiguration* configuration) {
}

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
  GLuint userTexturesId[32];

  //Locations
  GLuint uQuality;
  GLuint uCameraPosition;
  GLuint uCameraVelocity;
  GLuint uTime;
  GLuint uX;
  GLuint uY;
  GLuint uWidth;
  GLuint uHeight;
  GLuint uZoom;
  GLuint uScroll;
  GLuint uBattery;
  GLuint uVolume;
  GLuint uKeyStates;
  GLuint uJoyStates;
  GLuint uSampleStates;
  GLuint uUserTextures;
  GLuint uMaxVolume;
};

void shaderUniformsInitLocations(struct ShaderUniforms* u, GLuint program) {
#define GET_LOC(field, nameStr) u->field = glGetUniformLocation(program, nameStr)

  GET_LOC(uQuality, "uQuality");
  GET_LOC(uCameraPosition, "uCameraPosition");
  GET_LOC(uCameraVelocity, "uCameraVelocity");
  GET_LOC(uTime, "uTime");
  GET_LOC(uX, "uX");
  GET_LOC(uY, "uY");
  GET_LOC(uWidth, "uWidth");
  GET_LOC(uHeight, "uHeight");
  GET_LOC(uZoom, "uZoom");
  GET_LOC(uScroll, "uScroll");
  GET_LOC(uBattery, "uBattery");
  GET_LOC(uVolume, "uVolume");
  GET_LOC(uMaxVolume, "uMaxVolume");

  GET_LOC(uKeyStates, "uKeyStates");
  GET_LOC(uJoyStates, "uJoyStates");
  GET_LOC(uSampleStates, "uSampleStates");
  GET_LOC(uUserTextures, "uUserTextures");

#undef GET_LOC
}

void shaderUniformsLoad(struct ShaderUniforms* u, GLuint program) {
  if (u->uQuality != -1) glUniform1f(u->uQuality, u->quality);
  if (u->uCameraPosition != -1) glUniform3fv(u->uCameraPosition, 1, u->cameraPosition);
  if (u->uCameraVelocity != -1) glUniform3fv(u->uCameraVelocity, 1, u->cameraVelocity);
  if (u->uTime != -1) glUniform1f(u->uTime, u->time);
  if (u->uX != -1) glUniform1f(u->uX, u->x);
  if (u->uY != -1) glUniform1f(u->uY, u->y);
  if (u->uWidth != -1) glUniform1f(u->uWidth, u->width);
  if (u->uHeight != -1) glUniform1f(u->uHeight, u->height);
  if (u->uZoom != -1) glUniform1f(u->uZoom, u->zoom);
  if (u->uScroll != -1) glUniform1f(u->uScroll, u->scroll);
  if (u->uBattery != -1) glUniform1f(u->uBattery, u->battery);
  if (u->uVolume != -1) glUniform1f(u->uVolume, u->volume);
  if (u->uMaxVolume != -1) glUniform1f(u->uMaxVolume, u->maxVolume);

  if (u->uKeyStates != -1) glUniform1iv(u->uKeyStates, 32, u->keyStates);
  if (u->uJoyStates != -1) glUniform1iv(u->uJoyStates, 32, u->joyStates);
  if (u->uSampleStates != -1) glUniform1iv(u->uSampleStates, 128, u->sampleStates);

  if (u->uUserTextures != -1) {
    int textureUnits[32];
    for (int i = 0; i < 32; ++i) textureUnits[i] = i;
    glUniform1iv(u->uUserTextures, 32, textureUnits);
  }

  for (int i = 0; i < 32; ++i) {
    if (u->userTexturesId[i]) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, u->userTexturesId[i]);
    }
  }

  glUseProgram(0);
}
struct ShaderSession {
  GLuint                      shaderProgram;
  struct SessionConfiguration config;
  struct glMesh               quad;
  struct ShaderUniforms       uniforms;
};

int shaderSessionLoad(struct ShaderSession* session, const char* configfile) {
  if (SessionConfigurationParse(&session->config, configfile)) {
    fprintf(stderr, "Error parsing config file %s\n", configfile);
    return 1;
  }

  GLuint program = glProgramCompile(session->config.fragmentShader, session->config.vertexShader);

  if (!program) {
    fprintf(stderr, "Error compiling shaders %s %s\n", session->config.vertexShader, session->config.fragmentShader);
    return 1;
  }
  session->shaderProgram = program;
  session->quad          = glQuadLoad();

  shaderUniformsLoad(&session->uniforms, session->shaderProgram);
  return 0;
}

int shaderSessionDispose(struct ShaderSession* session) {
  glDeleteProgram(session->shaderProgram);
  glMeshDispose(&session->quad);
  return 0;
}

void printUsage() {}

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
      glVertex2f(0.6f, -0.4f);
      glColor3f(0.0, 0.0, 1.0);
      glVertex2f(0.0f, 0.6f);
      glEnd();

      glXSwapBuffers(dpy, win);
      usleep(16000);
    }
    return 1;
  }

  struct ShaderSession session;

  const char* configfile = argv[1];

  if (shaderSessionLoad(&session, configfile)) {
    fprintf(stderr, "Error initializing session\n");
    return 1;
  }

  while (1) {
    XEvent ev;
    while (XPending(dpy)) {
      XNextEvent(dpy, &ev);
    }

    glClearColor(0.1f, 0.0f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glXSwapBuffers(dpy, win);
    usleep(16000);
  }
}

//====================================[DRIVER CODE]==========================================

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
  swa.event_mask = ExposureMask;

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

  application(argc, argv, dpy, win);

  glXMakeCurrent(dpy, None, NULL);
  glXDestroyContext(dpy, glc);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);

  return 0;
}
