/*
 * vis3d: drawing with OpenGL into the host's current context.
 *
 * The GL entry points are loaded through a getproc function (the host's:
 * Qt's QOpenGLContext.getProcAddress, EGL's, GLX's), so the library links
 * against no GL library and works in any of: OpenGL ES 2.0 / 3.x, desktop
 * 2.1 compatibility, 3.2+ core. One vertex array object is created and kept
 * bound where the context has them (core profiles need one).
 *
 * A frame: background (one triangle), floor grid (one quad), one draw per
 * part with its link's matrix and material, obstacles (transparent, edges),
 * the trail, the clearance shell and the tool point marker.
 */
#include "shaders.h"
#include "vis3d_int.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- GL enums and entry points (only what's used; values are the same in ES) -----

typedef unsigned int GLenum, GLuint, GLbitfield;
typedef int GLint, GLsizei;
typedef unsigned char GLboolean, GLubyte;
typedef float GLfloat;
typedef char GLchar;
typedef long GLsizeiptr, GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_LINES 0x0001
#define GL_LINE_STRIP 0x0003
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_LEQUAL 0x0203
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_FLOAT 0x1406
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_SHADING_LANGUAGE_VERSION 0x8B8C
#define GL_NO_ERROR 0

#define GLFUNCS(X)                                                                                  \
  X(void, Viewport, (GLint, GLint, GLsizei, GLsizei))                                               \
  X(void, Clear, (GLbitfield))                                                                      \
  X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                                         \
  X(void, Enable, (GLenum))                                                                         \
  X(void, Disable, (GLenum))                                                                        \
  X(void, BlendFunc, (GLenum, GLenum))                                                              \
  X(void, DepthMask, (GLboolean))                                                                   \
  X(void, DepthFunc, (GLenum))                                                                      \
  X(void, DrawArrays, (GLenum, GLint, GLsizei))                                                     \
  X(const GLubyte *, GetString, (GLenum))                                                           \
  X(GLenum, GetError, (void))                                                                       \
  X(void, GenBuffers, (GLsizei, GLuint *))                                                          \
  X(void, DeleteBuffers, (GLsizei, const GLuint *))                                                 \
  X(void, BindBuffer, (GLenum, GLuint))                                                             \
  X(void, BufferData, (GLenum, GLsizeiptr, const void *, GLenum))                                   \
  X(void, BufferSubData, (GLenum, GLintptr, GLsizeiptr, const void *))                              \
  X(GLuint, CreateShader, (GLenum))                                                                 \
  X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *))                    \
  X(void, CompileShader, (GLuint))                                                                  \
  X(void, GetShaderiv, (GLuint, GLenum, GLint *))                                                   \
  X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                 \
  X(void, DeleteShader, (GLuint))                                                                   \
  X(GLuint, CreateProgram, (void))                                                                  \
  X(void, AttachShader, (GLuint, GLuint))                                                           \
  X(void, BindAttribLocation, (GLuint, GLuint, const GLchar *))                                     \
  X(void, LinkProgram, (GLuint))                                                                    \
  X(void, GetProgramiv, (GLuint, GLenum, GLint *))                                                  \
  X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                \
  X(void, DeleteProgram, (GLuint))                                                                  \
  X(void, UseProgram, (GLuint))                                                                     \
  X(GLint, GetUniformLocation, (GLuint, const GLchar *))                                            \
  X(void, Uniform1f, (GLint, GLfloat))                                                              \
  X(void, Uniform2f, (GLint, GLfloat, GLfloat))                                                     \
  X(void, Uniform3f, (GLint, GLfloat, GLfloat, GLfloat))                                            \
  X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))                                   \
  X(void, UniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *))                           \
  X(void, EnableVertexAttribArray, (GLuint))                                                        \
  X(void, DisableVertexAttribArray, (GLuint))                                                       \
  X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))

#define DECL(ret, name, args) ret(*name) args;
#define LOAD(ret, name, args)                                       \
  g->name = (ret(*) args)gp("gl" #name);                            \
  if(!g->name) {                                                    \
    snprintf(err, errlen, "the GL context has no gl%s", #name);     \
    return -1;                                                      \
  }

enum { P_MESH, P_BG, P_GRID, P_LINE, P_SHELL, NPROG };

struct gl_state {
  GLFUNCS(DECL)
  void (*GenVertexArrays)(GLsizei, GLuint *);  // optional (core profiles need it)
  void (*BindVertexArray)(GLuint);
  void (*DeleteVertexArrays)(GLsizei, const GLuint *);
  GLuint vao, vbo, vbo_line, vbo_trail, vbo_quad, vbo_bg, vbo_sphere;
  int sphere_n;
  GLuint prog[NPROG];
  char info[256];
};

// ---- loading -------------------------------------------------------------------

static void *(*egl_gpa)(const char *);
static void *(*glx_gpa)(const char *);
static void *(*egl_cur)(void);

static void *default_getproc(const char *name) {
  if(!egl_gpa && !glx_gpa) {
    void *e = dlopen("libEGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    void *x = dlopen("libGL.so.1", RTLD_LAZY | RTLD_GLOBAL);
    if(e) {
      egl_gpa = (void *(*)(const char *))dlsym(e, "eglGetProcAddress");
      egl_cur = (void *(*)(void))dlsym(e, "eglGetCurrentContext");
    }
    if(x) {
      glx_gpa = (void *(*)(const char *))dlsym(x, "glXGetProcAddressARB");
    }
  }
  // an EGL context current: EGL's; else GLX's
  if(egl_gpa && (!glx_gpa || (egl_cur && egl_cur()))) {
    return egl_gpa(name);
  }
  return glx_gpa ? glx_gpa(name) : NULL;
}

static GLuint shader(struct gl_state *g, GLenum type, const char *hdr, const char *src, char *err, int errlen) {
  GLuint s            = g->CreateShader(type);
  const char *srcs[2] = {hdr, src};
  g->ShaderSource(s, 2, srcs, NULL);
  g->CompileShader(s);
  GLint ok = 0;
  g->GetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if(!ok) {
    char log[1024] = "";
    g->GetShaderInfoLog(s, sizeof(log), NULL, log);
    snprintf(err, errlen, "%s shader: %s", type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
    g->DeleteShader(s);
    return 0;
  }
  return s;
}

static GLuint program(struct gl_state *g, const char *hdr, const char *vs, const char *fs, char *err, int errlen) {
  GLuint v = shader(g, GL_VERTEX_SHADER, hdr, vs, err, errlen);
  GLuint f = v ? shader(g, GL_FRAGMENT_SHADER, hdr, fs, err, errlen) : 0;
  if(!f) {
    return 0;
  }
  GLuint p = g->CreateProgram();
  g->AttachShader(p, v);
  g->AttachShader(p, f);
  g->BindAttribLocation(p, 0, "aPos");
  g->BindAttribLocation(p, 1, "aNrm");
  g->LinkProgram(p);
  g->DeleteShader(v);
  g->DeleteShader(f);
  GLint ok = 0;
  g->GetProgramiv(p, GL_LINK_STATUS, &ok);
  if(!ok) {
    char log[1024] = "";
    g->GetProgramInfoLog(p, sizeof(log), NULL, log);
    snprintf(err, errlen, "link: %s", log);
    g->DeleteProgram(p);
    return 0;
  }
  return p;
}

// the GLSL dialect for a GL_VERSION string: "300 es", "100", "330" or
// "120" (desktop GL below 3.3: the Pi 4's 2.1 / 3.1 through Mesa V3D)
const char *v3_glsl_for(const char *ver) {
  int maj = 0, min = 0;
  if(!ver) {
    return NULL;
  }
  if(strstr(ver, "OpenGL ES")) {
    sscanf(strstr(ver, "OpenGL ES") + 9, " %d.%d", &maj, &min);
    return maj >= 3 ? "300 es" : "100";
  }
  sscanf(ver, "%d.%d", &maj, &min);
  return maj > 3 || (maj == 3 && min >= 3) ? "330" : "120";
}

// the shader header for the current context
static const char *dialect(struct gl_state *g, char *name, int len) {
  const char *ver = (const char *)g->GetString(GL_VERSION);
  const char *d   = v3_glsl_for(ver);
  if(!d) {
    snprintf(name, len, "no GL_VERSION");
    return NULL;
  }
  snprintf(name, len, "%s%s / GLSL %s", strstr(ver, "OpenGL ES") ? "" : "OpenGL ", ver, d);
  return !strcmp(d, "300 es") ? HDR_ES3 : !strcmp(d, "100") ? HDR_ES2 : !strcmp(d, "330") ? HDR_GL33 : HDR_GL21;
}

static void push_sphere(float *o, int *n, float th, float ph) {
  float x = sinf(th) * cosf(ph), y = sinf(th) * sinf(ph), z = cosf(th);
  o[(*n) * 6 + 0] = x, o[(*n) * 6 + 1] = y, o[(*n) * 6 + 2] = z;
  o[(*n) * 6 + 3] = x, o[(*n) * 6 + 4] = y, o[(*n) * 6 + 5] = z;
  (*n)++;
}

int vis3d_gl_init(vis3d_t *v, vis3d_getproc_t getproc, char *err, int errlen) {
  void *(*gp)(const char *) = getproc ? getproc : default_getproc;
  if(v->gl) {
    vis3d_gl_release(v);
  }
  struct gl_state *g = calloc(1, sizeof(*g));
  v->gl              = g;
  GLFUNCS(LOAD)
  g->GenVertexArrays    = (void (*)(GLsizei, GLuint *))gp("glGenVertexArrays");
  g->BindVertexArray    = (void (*)(GLuint))gp("glBindVertexArray");
  g->DeleteVertexArrays = (void (*)(GLsizei, const GLuint *))gp("glDeleteVertexArrays");

  const char *hdr = dialect(g, g->info, sizeof(g->info));
  if(!hdr) {
    snprintf(err, errlen, "no GL context current");
    return -1;
  }
  if(hdr != HDR_ES2 && hdr != HDR_GL21 && g->GenVertexArrays && g->BindVertexArray) {
    g->GenVertexArrays(1, &g->vao);
    g->BindVertexArray(g->vao);
  }
  const char *vs[NPROG] = {MESH_VS, BG_VS, GRID_VS, LINE_VS, MESH_VS};
  const char *fs[NPROG] = {MESH_FS, BG_FS, GRID_FS, LINE_FS, SHELL_FS};
  for(int i = 0; i < NPROG; i++) {
    g->prog[i] = program(g, hdr, vs[i], fs[i], err, errlen);
    if(!g->prog[i]) {
      return -1;
    }
  }

  g->GenBuffers(1, &g->vbo);
  g->BindBuffer(GL_ARRAY_BUFFER, g->vbo);
  g->BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)v->nvert * 6 * sizeof(float), v->vert, GL_STATIC_DRAW);

  // obstacle edges: xyz + alpha
  int nl     = v->nline;
  float *lin = malloc((nl ? nl : 1) * 4 * sizeof(float));
  for(int i = 0; i < nl; i++) {
    memcpy(lin + i * 4, v->line + i * 3, 3 * sizeof(float));
    lin[i * 4 + 3] = 1.0f;
  }
  g->GenBuffers(1, &g->vbo_line);
  g->BindBuffer(GL_ARRAY_BUFFER, g->vbo_line);
  g->BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(nl ? nl : 1) * 4 * sizeof(float), lin, GL_STATIC_DRAW);
  free(lin);

  g->GenBuffers(1, &g->vbo_trail);
  g->BindBuffer(GL_ARRAY_BUFFER, g->vbo_trail);
  g->BufferData(GL_ARRAY_BUFFER, V3_TRAIL * 6 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);  // a ribbon: 6 per segment

  float quad[] = {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0};
  g->GenBuffers(1, &g->vbo_quad);
  g->BindBuffer(GL_ARRAY_BUFFER, g->vbo_quad);
  g->BufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

  float bg[] = {-1, -1, 3, -1, -1, 3};  // one triangle over the screen
  g->GenBuffers(1, &g->vbo_bg);
  g->BindBuffer(GL_ARRAY_BUFFER, g->vbo_bg);
  g->BufferData(GL_ARRAY_BUFFER, sizeof(bg), bg, GL_STATIC_DRAW);

  // a unit sphere (the clearance shell, the tool point marker)
  const int R = 24, S = 40;
  float *sp   = malloc(R * S * 6 * 6 * sizeof(float));
  int n       = 0;
  for(int k = 0; k < R; k++) {
    for(int i = 0; i < S; i++) {
      float t0 = M_PI * k / R, t1 = M_PI * (k + 1) / R, p0 = 2 * M_PI * i / S, p1 = 2 * M_PI * (i + 1) / S;
      push_sphere(sp, &n, t0, p0), push_sphere(sp, &n, t1, p0), push_sphere(sp, &n, t1, p1);
      push_sphere(sp, &n, t0, p0), push_sphere(sp, &n, t1, p1), push_sphere(sp, &n, t0, p1);
    }
  }
  g->sphere_n = n;
  g->GenBuffers(1, &g->vbo_sphere);
  g->BindBuffer(GL_ARRAY_BUFFER, g->vbo_sphere);
  g->BufferData(GL_ARRAY_BUFFER, n * 6 * sizeof(float), sp, GL_STATIC_DRAW);
  free(sp);

  GLenum e = g->GetError();
  if(e != GL_NO_ERROR) {
    snprintf(err, errlen, "GL error 0x%x while uploading", e);
    return -1;
  }
  return 0;
}

const char *vis3d_gl_info(const vis3d_t *v) {
  return v->gl ? v->gl->info : "";
}

void vis3d_gl_release(vis3d_t *v) {
  struct gl_state *g = v->gl;
  if(!g) {
    return;
  }
  if(g->DeleteBuffers) {
    GLuint b[] = {g->vbo, g->vbo_line, g->vbo_trail, g->vbo_quad, g->vbo_bg, g->vbo_sphere};
    g->DeleteBuffers(6, b);
    for(int i = 0; i < NPROG; i++) {
      if(g->prog[i]) {
        g->DeleteProgram(g->prog[i]);
      }
    }
    if(g->vao && g->DeleteVertexArrays) {
      g->DeleteVertexArrays(1, &g->vao);
    }
  }
  free(g);
  v->gl = NULL;
}

void vis3d_free(vis3d_t *v) {
  if(!v) {
    return;
  }
  vis3d_gl_release(v);
  free(v->vert);
  free(v->line);
  free(v);
}

void vis3d_resize(vis3d_t *v, int w, int h) {
  v->w = w, v->h = h;
}

// ---- drawing -------------------------------------------------------------------

static GLint U(struct gl_state *g, int p, const char *name) {
  return g->GetUniformLocation(g->prog[p], name);
}

static void attribs(struct gl_state *g, GLuint vbo, int pos_n, int stride, int nrm) {
  g->BindBuffer(GL_ARRAY_BUFFER, vbo);
  g->EnableVertexAttribArray(0);
  g->VertexAttribPointer(0, pos_n, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void *)0);
  if(nrm) {
    g->EnableVertexAttribArray(1);
    g->VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride * sizeof(float), (void *)(3 * sizeof(float)));
  } else {
    g->DisableVertexAttribArray(1);
  }
}

static const float FOG[3] = {0.030f, 0.038f, 0.050f};  // the background at the horizon

static void mesh_uniforms(vis3d_t *v, struct gl_state *g, int p, mat4 vp, const float *eye, double t) {
  g->UseProgram(g->prog[p]);
  g->UniformMatrix4fv(U(g, p, "uViewProj"), 1, GL_FALSE, vp.m);
  g->Uniform3f(U(g, p, "uEye"), eye[0], eye[1], eye[2]);
  g->Uniform3f(U(g, p, "uAccent"), v->accent[0], v->accent[1], v->accent[2]);
  g->Uniform3f(U(g, p, "uFog"), FOG[0], FOG[1], FOG[2]);
  g->Uniform2f(U(g, p, "uFogRange"), v->dist * 1.2, v->dist * 4.0);
  g->Uniform1f(U(g, p, "uTime"), (float)t);
}

static void set_material(vis3d_t *v, struct gl_state *g, const v3_material *m, const float *tint, float rim) {
  const float *c = m->accent ? v->accent : m->color;
  g->Uniform3f(U(g, P_MESH, "uColor"), c[0], c[1], c[2]);
  g->Uniform4f(U(g, P_MESH, "uMat"), m->metal, m->rough, m->emissive, m->alpha);
  g->Uniform4f(U(g, P_MESH, "uTint"), tint ? tint[0] : 0, tint ? tint[1] : 0, tint ? tint[2] : 0, tint ? tint[3] : 0);
  g->Uniform1f(U(g, P_MESH, "uRim"), rim);
}

static const float GUARD_COL[4][3] = {{0.5f, 0.55f, 0.6f}, {0.5f, 0.87f, 0.35f}, {0.94f, 0.68f, 0.25f}, {1.0f, 0.36f, 0.33f}};

void vis3d_draw(vis3d_t *v, double t) {
  struct gl_state *g = v->gl;
  if(!g || v->w <= 0 || v->h <= 0) {
    return;
  }
  if(g->vao) {
    g->BindVertexArray(g->vao);
  }
  g->Viewport(0, 0, v->w, v->h);
  g->ClearColor(FOG[0], FOG[1], FOG[2], 1);
  g->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mat4 view = v3_view(v), proj = v3_proj(v), vp = m4_mul(proj, view);
  float eye[3];
  {
    double y = v->yaw * M_PI / 180, p = v->pitch * M_PI / 180;
    eye[0] = v->target[0] + v->dist * cos(p) * cos(y);
    eye[1] = v->target[1] + v->dist * cos(p) * sin(y);
    eye[2] = v->target[2] + v->dist * sin(p);
  }
  float size = fmaxf(fmaxf(v->hi[0] - v->lo[0], v->hi[1] - v->lo[1]), v->hi[2] - v->lo[2]);

  // background
  g->Disable(GL_DEPTH_TEST);
  g->DepthMask(GL_FALSE);
  g->Disable(GL_BLEND);
  g->UseProgram(g->prog[P_BG]);
  g->Uniform3f(U(g, P_BG, "uTop"), 0.075f, 0.090f, 0.115f);
  g->Uniform3f(U(g, P_BG, "uBottom"), FOG[0], FOG[1], FOG[2]);
  attribs(g, g->vbo_bg, 2, 2, 0);
  g->DrawArrays(GL_TRIANGLES, 0, 3);

  g->Enable(GL_DEPTH_TEST);
  g->DepthFunc(GL_LEQUAL);
  g->DepthMask(GL_TRUE);

  // floor
  if(v->show_grid) {
    float cell  = powf(10.0f, floorf(log10f(size / 4.0f)));
    float reach = 0;
    for(int j = 0; j < v->n; j++) {
      reach += fabs(v->a[j]) + fabs(v->d[j]) * (j > 0);
    }
    g->UseProgram(g->prog[P_GRID]);
    g->UniformMatrix4fv(U(g, P_GRID, "uViewProj"), 1, GL_FALSE, vp.m);
    g->Uniform3f(U(g, P_GRID, "uEye"), eye[0], eye[1], eye[2]);
    g->Uniform3f(U(g, P_GRID, "uAccent"), v->accent[0], v->accent[1], v->accent[2]);
    g->Uniform3f(U(g, P_GRID, "uFog"), FOG[0], FOG[1], FOG[2]);
    g->Uniform1f(U(g, P_GRID, "uCell"), cell);
    g->Uniform1f(U(g, P_GRID, "uReach"), reach);
    g->Uniform1f(U(g, P_GRID, "uBase"), 0.12f * size);
    g->Uniform1f(U(g, P_GRID, "uSize"), 10.0f * size);
    attribs(g, g->vbo_quad, 3, 3, 0);
    g->DrawArrays(GL_TRIANGLES, 0, 6);
  }

  // the arm: opaque parts
  mesh_uniforms(v, g, P_MESH, vp, eye, t);
  attribs(g, g->vbo, 3, 6, 1);
  for(int i = 0; i < v->npart; i++) {
    const v3_part *pt     = &v->part[i];
    const v3_material *m = &v->mat[pt->material];
    if(pt->link < 0 || m->alpha < 1.0f) {
      continue;
    }
    g->UniformMatrix4fv(U(g, P_MESH, "uModel"), 1, GL_FALSE, v->link[pt->link].m);
    set_material(v, g, m, v->tint[pt->link][3] > 0 ? v->tint[pt->link] : NULL, 0.55f);
    g->DrawArrays(GL_TRIANGLES, pt->first, pt->count);
  }

  // transparent: obstacles (the nearest in the guard's colour), see-through parts
  g->Enable(GL_BLEND);
  g->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  g->DepthMask(GL_FALSE);
  mat4 I = m4_identity();
  for(int i = 0; i < v->npart; i++) {
    const v3_part *pt     = &v->part[i];
    const v3_material *m = &v->mat[pt->material];
    int ob               = pt->link < 0 ? -1 - pt->link : -1;
    if(ob >= 0 && !v->show_obs) {
      continue;
    }
    if(ob < 0 && m->alpha >= 1.0f) {
      continue;
    }
    g->UniformMatrix4fv(U(g, P_MESH, "uModel"), 1, GL_FALSE, ob >= 0 ? I.m : v->link[pt->link].m);
    float tint[4] = {0, 0, 0, 0};
    if(ob >= 0 && ob == v->nearest && v->guard > VIS3D_GUARD_OFF) {
      memcpy(tint, GUARD_COL[v->guard], sizeof(GUARD_COL[0]));
      tint[3] = 0.9f;
    }
    set_material(v, g, m, tint, ob >= 0 ? 0.3f : 0.55f);
    g->DrawArrays(GL_TRIANGLES, pt->first, pt->count);
  }

  // obstacle edges
  g->UseProgram(g->prog[P_LINE]);
  g->UniformMatrix4fv(U(g, P_LINE, "uViewProj"), 1, GL_FALSE, vp.m);
  if(v->show_obs && v->nline) {
    attribs(g, g->vbo_line, 4, 4, 0);
    for(int i = 0; i < v->nobs; i++) {
      const float *c = (i == v->nearest && v->guard > VIS3D_GUARD_OFF) ? GUARD_COL[v->guard] : (const float[]){0.85f, 0.62f, 0.25f};
      g->Uniform4f(U(g, P_LINE, "uColor"), c[0], c[1], c[2], 0.85f);
      if(v->obs[i].line_count) {
        g->DrawArrays(GL_LINES, v->obs[i].line_first, v->obs[i].line_count);
      }
    }
  }

  // the trail: a ribbon facing the camera (core profiles have no wide
  // lines), oldest faint, newest bright
  if(v->trail_on && v->trail_n > 1) {
    static float buf[V3_TRAIL * 6 * 4];
    float wdt = 0.004f * size;
    int nv    = 0;
    for(int i = 0; i + 1 < v->trail_n; i++) {
      const float *p0 = v->trail[(v->trail_head - v->trail_n + i + V3_TRAIL) % V3_TRAIL];
      const float *p1 = v->trail[(v->trail_head - v->trail_n + i + 1 + V3_TRAIL) % V3_TRAIL];
      float d[3], e[3], s[3];
      v3_sub(p1, p0, d), v3_sub(eye, p0, e), v3_cross(d, e, s), v3_norm(s);
      float a0 = (float)(i + 1) / v->trail_n, a1 = (float)(i + 2) / v->trail_n;
      float q[4][4] = {{p0[0] - s[0] * wdt, p0[1] - s[1] * wdt, p0[2] - s[2] * wdt, a0},
                       {p0[0] + s[0] * wdt, p0[1] + s[1] * wdt, p0[2] + s[2] * wdt, a0},
                       {p1[0] + s[0] * wdt, p1[1] + s[1] * wdt, p1[2] + s[2] * wdt, a1},
                       {p1[0] - s[0] * wdt, p1[1] - s[1] * wdt, p1[2] - s[2] * wdt, a1}};
      int idx[6] = {0, 1, 2, 0, 2, 3};
      for(int k = 0; k < 6; k++) {
        memcpy(buf + (nv++) * 4, q[idx[k]], 4 * sizeof(float));
      }
    }
    g->BindBuffer(GL_ARRAY_BUFFER, g->vbo_trail);
    g->BufferSubData(GL_ARRAY_BUFFER, 0, nv * 4 * sizeof(float), buf);
    attribs(g, g->vbo_trail, 4, 4, 0);
    g->Uniform4f(U(g, P_LINE, "uColor"), 0.98f, 0.75f, 0.30f, 0.9f);
    g->DrawArrays(GL_TRIANGLES, 0, nv);
  }

  // the clearance shell
  if(v->guard > VIS3D_GUARD_OFF && v->guard_r > 0) {
    mat4 M = m4_mul(m4_translate(v->tcp_world[0], v->tcp_world[1], v->tcp_world[2]), m4_scale(v->guard_r, v->guard_r, v->guard_r));
    mesh_uniforms(v, g, P_SHELL, vp, eye, t);
    g->UniformMatrix4fv(U(g, P_SHELL, "uModel"), 1, GL_FALSE, M.m);
    const float *c = GUARD_COL[v->guard];
    g->Uniform4f(U(g, P_SHELL, "uColor"), c[0], c[1], c[2], v->guard == VIS3D_GUARD_STOP ? 0.9f : 0.6f);
    attribs(g, g->vbo_sphere, 3, 6, 1);
    g->DrawArrays(GL_TRIANGLES, 0, g->sphere_n);
  }

  // the tool point
  g->DepthMask(GL_TRUE);
  g->Disable(GL_BLEND);
  mesh_uniforms(v, g, P_MESH, vp, eye, t);
  float r  = 0.012f * size;
  mat4 M   = m4_mul(m4_translate(v->tcp_world[0], v->tcp_world[1], v->tcp_world[2]), m4_scale(r, r, r));
  v3_material dot = {"", {0.98f, 0.75f, 0.30f}, 0, 0.3f, 1.0f, 1.0f, 0};
  g->UniformMatrix4fv(U(g, P_MESH, "uModel"), 1, GL_FALSE, M.m);
  set_material(v, g, &dot, NULL, 0);
  attribs(g, g->vbo_sphere, 3, 6, 1);
  g->DrawArrays(GL_TRIANGLES, 0, g->sphere_n);

  g->UseProgram(0);
  g->BindBuffer(GL_ARRAY_BUFFER, 0);
  g->DisableVertexAttribArray(1);
}
