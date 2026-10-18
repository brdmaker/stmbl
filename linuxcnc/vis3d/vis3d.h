/*
 * vis3d: a native 3D view of a serial arm for LinuxCNC screens.
 *
 * The library loads a model file (a chain of Craig DH joints like
 * genserkins / arm_sim, parts attached to the links, obstacles), and draws
 * it with shaders into whatever OpenGL context the host has made current:
 * a QtVCP QOpenGLWidget (vis3d.py / qtvcp/vis3d_widget.py), the offscreen
 * renderer (vis3d_render), or any other. It never creates a window, reads
 * HAL or blocks: the host feeds joint positions and overlay state each
 * frame and calls vis3d_draw.
 *
 * GL: OpenGL ES 2.0 / 3.x (Raspberry Pi 4: ES 3.1), desktop GL 2.1
 * (compatibility) and 3.2+ core; the shader dialect is picked from the
 * context's version. Geometry is uploaded once; a frame is one uniform
 * block per link and one draw call per part.
 *
 * Units: the model's length unit (m by default, `units mm` for mm);
 * angles in the API in degrees, as LinuxCNC's joint pins.
 */
#ifndef VIS3D_H
#define VIS3D_H

#ifdef __cplusplus
extern "C" {
#endif

#define VIS3D_MAXJ 9

typedef struct vis3d vis3d_t;

// ---- model (no GL needed) ----------------------------------------------------

// load a model file; NULL on error, the reason in err
vis3d_t *vis3d_load(const char *path, char *err, int errlen);
void vis3d_free(vis3d_t *v);  // with its GL objects: call with the context current, or after vis3d_gl_release

int vis3d_joints(const vis3d_t *v);
const char *vis3d_joint_pin(const vis3d_t *v, int j);  // the HAL pin the model names for joint j ("" if none)
void vis3d_joint_limits(const vis3d_t *v, int j, double *min, double *max);
int vis3d_obstacles(const vis3d_t *v);
const char *vis3d_obstacle_name(const vis3d_t *v, int i);

// joint positions [deg, or model units for a prismatic joint]
void vis3d_set_joints(vis3d_t *v, const double *q, int n);
// the frame of link k (0 = base, k = after joint k) or of the tool point
// (k = -1) in the base frame: position p[3], rotation R (row-major 3x3)
void vis3d_frame(const vis3d_t *v, int k, double p[3], double R[9]);

// ---- overlays ----------------------------------------------------------------

enum { VIS3D_GUARD_OFF = 0, VIS3D_GUARD_OK, VIS3D_GUARD_SLOW, VIS3D_GUARD_STOP };
// the guard's state: colors the clearance shell and the nearest obstacle
// (index, -1 none); radius: the clearance limit around the tool point
void vis3d_set_guard(vis3d_t *v, int state, double radius, int nearest);
// a link's tint (rgba 0..1, a = strength; 0 = none): a joint in fault, a selection
void vis3d_set_tint(vis3d_t *v, int link, float r, float g, float b, float a);
// the tool point's trail: on/off, and clear it
void vis3d_trail(vis3d_t *v, int on, int clear);
// accent color of the look (rim light, rings, grid axes)
void vis3d_set_accent(vis3d_t *v, float r, float g, float b);
void vis3d_show(vis3d_t *v, int obstacles, int grid);

// ---- camera ------------------------------------------------------------------

void vis3d_camera(vis3d_t *v, double yaw_deg, double pitch_deg, double dist, const double target[3]);
// drag (pixels), pan (pixels), zoom (factor, 1 = none)
void vis3d_orbit(vis3d_t *v, double dx, double dy);
void vis3d_pan(vis3d_t *v, double dx, double dy);
void vis3d_zoom(vis3d_t *v, double factor);
// a point in the base frame to window pixels (y down); 0 when behind the camera
int vis3d_project(const vis3d_t *v, const double p[3], double *x, double *y);

// ---- GL (the host's context current) -----------------------------------------

typedef void *(*vis3d_getproc_t)(const char *name);
// compile shaders and upload geometry; getproc NULL: eglGetProcAddress /
// glXGetProcAddressARB. 0 ok, else the reason in err
int vis3d_gl_init(vis3d_t *v, vis3d_getproc_t getproc, char *err, int errlen);
void vis3d_resize(vis3d_t *v, int w, int h);  // framebuffer pixels
void vis3d_draw(vis3d_t *v, double t);        // t: time [s] for the subtle pulses
void vis3d_gl_release(vis3d_t *v);
const char *vis3d_gl_info(const vis3d_t *v);  // "OpenGL ES 3.1 ... / GLSL 300 es"

// statistics of the model: triangles, draw calls
void vis3d_stats(const vis3d_t *v, int *triangles, int *draws);

#ifdef __cplusplus
}
#endif
#endif
