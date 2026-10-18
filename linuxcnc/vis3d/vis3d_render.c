/*
 * vis3d_render: draw a vis3d model offscreen (EGL, no window, no X) to a
 * PNG, or time it. For the tests, for screenshots, and to measure a
 * board's frame rate before building a screen around it (on a Pi 4: run it
 * there with --api gles3 --bench 300).
 *
 *   vis3d_render MODEL [-o out.png] [-s WxH] [--api gles3|gles2|gl33|gl21]
 *                [-q J1,J2,..] [--path J1,J2,..] [--cam YAW,PITCH,DIST]
 *                [--guard STATE,RADIUS,NEAREST] [--tint LINK,R,G,B,A]
 *                [--ssaa N] [--bench FRAMES]
 *
 * --path: move from -q to these joints over 120 steps first (draws the
 * trail). --guard: 0 off, 1 ok, 2 slow, 3 stop (vis3d_set_guard).
 */
#include "vis3d.h"

#include <EGL/egl.h>
#include <math.h>
#include <EGL/eglext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

typedef unsigned int GLuint, GLenum;
typedef int GLint, GLsizei;
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_PACK_ALIGNMENT 0x0D05

static struct {
  void (*GenFramebuffers)(GLsizei, GLuint *);
  void (*BindFramebuffer)(GLenum, GLuint);
  void (*GenRenderbuffers)(GLsizei, GLuint *);
  void (*BindRenderbuffer)(GLenum, GLuint);
  void (*RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
  void (*FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
  void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
  GLenum (*CheckFramebufferStatus)(GLenum);
  void (*GenTextures)(GLsizei, GLuint *);
  void (*BindTexture)(GLenum, GLuint);
  void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
  void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);
  void (*PixelStorei)(GLenum, GLint);
  void (*Finish)(void);
  GLenum (*GetError)(void);
} gl;

static void *gp(const char *n) {
  return (void *)eglGetProcAddress(n);
}

static int png_write(const char *path, const unsigned char *rgba, int w, int h) {
  // filter byte 0 per row, RGB (no alpha)
  size_t raw_len    = (size_t)h * (1 + 3 * w);
  unsigned char *raw = malloc(raw_len);
  for(int y = 0; y < h; y++) {
    unsigned char *r = raw + (size_t)y * (1 + 3 * w);
    r[0]             = 0;
    const unsigned char *s = rgba + (size_t)(h - 1 - y) * 4 * w;  // GL rows bottom-up
    for(int x = 0; x < w; x++) {
      memcpy(r + 1 + 3 * x, s + 4 * x, 3);
    }
  }
  uLongf zlen        = compressBound(raw_len);
  unsigned char *z   = malloc(zlen);
  compress2(z, &zlen, raw, raw_len, 6);
  free(raw);
  FILE *f = fopen(path, "wb");
  if(!f) {
    free(z);
    return -1;
  }
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  fwrite(sig, 1, 8, f);
#define BE32(p, v) (p)[0] = (v) >> 24, (p)[1] = (v) >> 16, (p)[2] = (v) >> 8, (p)[3] = (v)
  unsigned char ihdr[13];
  BE32(ihdr, (unsigned)w), BE32(ihdr + 4, (unsigned)h);
  ihdr[8] = 8, ihdr[9] = 2, ihdr[10] = ihdr[11] = ihdr[12] = 0;
  struct {
    const char *type;
    const unsigned char *data;
    unsigned len;
  } chunks[3] = {{"IHDR", ihdr, 13}, {"IDAT", z, (unsigned)zlen}, {"IEND", NULL, 0}};
  for(int i = 0; i < 3; i++) {
    unsigned char b[4];
    BE32(b, chunks[i].len);
    fwrite(b, 1, 4, f);
    fwrite(chunks[i].type, 1, 4, f);
    if(chunks[i].len) {
      fwrite(chunks[i].data, 1, chunks[i].len, f);
    }
    uLong crc = crc32(0, (const Bytef *)chunks[i].type, 4);
    if(chunks[i].len) {
      crc = crc32(crc, chunks[i].data, chunks[i].len);
    }
    BE32(b, (unsigned)crc);
    fwrite(b, 1, 4, f);
  }
  fclose(f);
  free(z);
  return 0;
}

static int parse_list(const char *s, double *out, int max) {
  int n = 0;
  char *copy = strdup(s), *save = NULL;
  for(char *t = strtok_r(copy, ",", &save); t && n < max; t = strtok_r(NULL, ",", &save)) {
    out[n++] = atof(t);
  }
  free(copy);
  return n;
}

static double now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
  const char *model = NULL, *out = NULL, *api = "gles3";
  int w = 1280, h = 720, ssaa = 2, bench = 0, nq = 0, npath = 0, nguard = 0, ntint = 0, ncam = 0;
  double q[VIS3D_MAXJ] = {0}, path[VIS3D_MAXJ] = {0}, guard[3] = {0}, tint[5] = {0}, cam[3] = {0};
  for(int i = 1; i < argc; i++) {
    const char *a = argv[i], *n = i + 1 < argc ? argv[i + 1] : NULL;
    if(!strcmp(a, "-o") && n) {
      out = argv[++i];
    } else if(!strcmp(a, "-s") && n) {
      sscanf(argv[++i], "%dx%d", &w, &h);
    } else if(!strcmp(a, "--api") && n) {
      api = argv[++i];
    } else if(!strcmp(a, "-q") && n) {
      nq = parse_list(argv[++i], q, VIS3D_MAXJ);
    } else if(!strcmp(a, "--path") && n) {
      npath = parse_list(argv[++i], path, VIS3D_MAXJ);
    } else if(!strcmp(a, "--guard") && n) {
      nguard = parse_list(argv[++i], guard, 3);
    } else if(!strcmp(a, "--tint") && n) {
      ntint = parse_list(argv[++i], tint, 5);
    } else if(!strcmp(a, "--cam") && n) {
      ncam = parse_list(argv[++i], cam, 3);
    } else if(!strcmp(a, "--ssaa") && n) {
      ssaa = atoi(argv[++i]);
    } else if(!strcmp(a, "--bench") && n) {
      bench = atoi(argv[++i]);
    } else if(a[0] != '-') {
      model = a;
    } else {
      fprintf(stderr, "usage: vis3d_render MODEL [-o out.png] [-s WxH] [--api gles3|gles2|gl33|gl21] [-q ..]\n"
                      "       [--path ..] [--cam yaw,pitch,dist] [--guard s,r,n] [--tint l,r,g,b,a] [--ssaa N] [--bench N]\n");
      return 2;
    }
  }
  if(!model) {
    fprintf(stderr, "vis3d_render: no model\n");
    return 2;
  }
  ssaa = ssaa < 1 ? 1 : ssaa > 4 ? 4 : ssaa;
  char err[512];
  vis3d_t *v = vis3d_load(model, err, sizeof(err));
  if(!v) {
    fprintf(stderr, "vis3d_render: %s\n", err);
    return 1;
  }

  // a headless context
  PFNEGLGETPLATFORMDISPLAYEXTPROC gpd = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
  EGLDisplay dpy = gpd ? gpd(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL) : EGL_NO_DISPLAY;
  if(dpy == EGL_NO_DISPLAY) {
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  }
  EGLint maj, min;
  if(!eglInitialize(dpy, &maj, &min)) {
    fprintf(stderr, "vis3d_render: no EGL display\n");
    return 1;
  }
  int es = !strncmp(api, "gles", 4);
  eglBindAPI(es ? EGL_OPENGL_ES_API : EGL_OPENGL_API);
  EGLint ca_es3[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
  EGLint ca_es2[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
  EGLint ca_gl33[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK,
                      EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
  EGLint ca_gl21[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
  EGLint *ca       = !strcmp(api, "gles2") ? ca_es2 : es ? ca_es3 : !strcmp(api, "gl21") ? ca_gl21 : ca_gl33;
  EGLContext ctx   = eglCreateContext(dpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ca);
  if(ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    fprintf(stderr, "vis3d_render: cannot make a %s context (EGL 0x%x)\n", api, eglGetError());
    return 1;
  }
#define L(n) gl.n = (typeof(gl.n))gp("gl" #n)
  L(GenFramebuffers), L(BindFramebuffer), L(GenRenderbuffers), L(BindRenderbuffer), L(RenderbufferStorage);
  L(FramebufferRenderbuffer), L(FramebufferTexture2D), L(CheckFramebufferStatus), L(GenTextures), L(BindTexture);
  L(TexImage2D), L(ReadPixels), L(PixelStorei), L(Finish), L(GetError);

  // the framebuffer: a texture for colour, a renderbuffer for depth
  int W = w * ssaa, H = h * ssaa;
  GLuint fb, tex, depth;
  gl.GenFramebuffers(1, &fb);
  gl.BindFramebuffer(GL_FRAMEBUFFER, fb);
  gl.GenTextures(1, &tex);
  gl.BindTexture(GL_TEXTURE_2D, tex);
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  gl.GenRenderbuffers(1, &depth);
  gl.BindRenderbuffer(GL_RENDERBUFFER, depth);
  gl.RenderbufferStorage(GL_RENDERBUFFER, !strcmp(api, "gles2") ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24, W, H);
  gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
  if(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    fprintf(stderr, "vis3d_render: framebuffer incomplete\n");
    return 1;
  }

  if(vis3d_gl_init(v, gp, err, sizeof(err)) < 0) {
    fprintf(stderr, "vis3d_render: %s\n", err);
    return 1;
  }
  printf("vis3d_render: %s\n", vis3d_gl_info(v));
  vis3d_resize(v, W, H);
  if(ncam == 3) {
    vis3d_camera(v, cam[0], cam[1], cam[2], NULL);
  }
  vis3d_set_joints(v, q, nq);
  if(npath) {  // a move from q to path, drawing the trail
    vis3d_trail(v, 1, 1);
    for(int s = 0; s <= 120; s++) {
      double x[VIS3D_MAXJ], f = 0.5 - 0.5 * cos(M_PI * s / 120.0);
      for(int j = 0; j < VIS3D_MAXJ; j++) {
        x[j] = q[j] + (path[j] - q[j]) * f;
      }
      vis3d_set_joints(v, x, npath > nq ? npath : nq);
    }
  }
  if(nguard >= 2) {
    vis3d_set_guard(v, (int)guard[0], guard[1], nguard >= 3 ? (int)guard[2] : -1);
  }
  if(ntint == 5) {
    vis3d_set_tint(v, (int)tint[0], tint[1], tint[2], tint[3], tint[4]);
  }

  if(bench > 0) {
    vis3d_resize(v, w, h);  // a real frame: no supersampling
    double t0 = now();
    for(int i = 0; i < bench; i++) {
      double x[VIS3D_MAXJ];
      for(int j = 0; j < VIS3D_MAXJ; j++) {
        x[j] = q[j] + 20.0 * sin(i * 0.02 + j);
      }
      vis3d_set_joints(v, x, vis3d_joints(v));
      vis3d_draw(v, i / 60.0);
    }
    gl.Finish();
    double dt = now() - t0;
    int tris, draws;
    vis3d_stats(v, &tris, &draws);
    printf("vis3d_render: %d frames %dx%d in %.2f s: %.1f fps (%d triangles, %d draws)\n", bench, w, h, dt, bench / dt,
           tris, draws);
    vis3d_resize(v, W, H);
  }

  vis3d_draw(v, 1.0);
  GLenum e = gl.GetError();
  if(e) {
    fprintf(stderr, "vis3d_render: GL error 0x%x\n", e);
    return 1;
  }
  if(out) {
    unsigned char *px = malloc((size_t)W * H * 4), *small = malloc((size_t)w * h * 4);
    gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
    gl.ReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for(int y = 0; y < h; y++) {  // box filter the supersampled image
      for(int x = 0; x < w; x++) {
        for(int c = 0; c < 4; c++) {
          int s = 0;
          for(int dy = 0; dy < ssaa; dy++) {
            for(int dx = 0; dx < ssaa; dx++) {
              s += px[((size_t)(y * ssaa + dy) * W + x * ssaa + dx) * 4 + c];
            }
          }
          small[((size_t)y * w + x) * 4 + c] = s / (ssaa * ssaa);
        }
      }
    }
    if(png_write(out, small, w, h) < 0) {
      fprintf(stderr, "vis3d_render: cannot write %s\n", out);
      return 1;
    }
    free(px);
    free(small);
    printf("vis3d_render: %s\n", out);
  }
  vis3d_free(v);
  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  return 0;
}
