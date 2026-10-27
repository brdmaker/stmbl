/*
 * vis3d: the shaders. Written once in a GLSL subset that the header below
 * maps to each dialect (render.c picks it from the context):
 *   GLSL ES 3.00 (Pi 4 GLES 3.1), ES 1.00 (GLES 2), 3.30 core, 1.20 (GL 2.1)
 * ATTR = vertex attribute, VOUT / VIN = varyings, FRAG = the output colour.
 * No derivatives, no texture lookups: ES 2.0 has neither guaranteed.
 */
#ifndef SHADERS_H
#define SHADERS_H

static const char *HDR_ES3 = "#version 300 es\nprecision highp float;\n#define ATTR in\n#define VOUT out\n#define VIN in\nout vec4 fragColor;\n#define FRAG fragColor\n";
static const char *HDR_GL33 = "#version 330 core\n#define ATTR in\n#define VOUT out\n#define VIN in\nout vec4 fragColor;\n#define FRAG fragColor\n";
static const char *HDR_ES2 = "#version 100\nprecision highp float;\n#define ATTR attribute\n#define VOUT varying\n#define VIN varying\n#define FRAG gl_FragColor\n";
static const char *HDR_GL21 = "#version 120\n#define ATTR attribute\n#define VOUT varying\n#define VIN varying\n#define FRAG gl_FragColor\n";

// shared by the lit shaders: tone map and gamma (no sRGB framebuffer on ES 2)
#define TONE                                                    \
  "vec3 tone(vec3 c) {\n"                                       \
  "  c = c / (c + vec3(0.85));\n"                               \
  "  return pow(c * 1.85, vec3(1.0 / 2.2));\n"                   \
  "}\n"

// ---- the arm's parts and the obstacles ------------------------------------------

static const char *MESH_VS =
    "ATTR vec3 aPos;\n"
    "ATTR vec3 aNrm;\n"
    "uniform mat4 uModel;\n"
    "uniform mat4 uViewProj;\n"
    "VOUT vec3 vPos;\n"
    "VOUT vec3 vNrm;\n"
    "void main() {\n"
    "  vec4 w = uModel * vec4(aPos, 1.0);\n"
    "  vPos = w.xyz;\n"
    "  vNrm = (uModel * vec4(aNrm, 0.0)).xyz;\n"  // rigid transforms: no inverse transpose; ES 1.00 has no mat3(mat4)
    "  gl_Position = uViewProj * w;\n"
    "}\n";

// a studio look: a cool key light, a warm fill from behind, a sky/ground
// ambient, Blinn specular by roughness, a fresnel rim in the accent colour,
// emissive parts that breathe slightly, fog into the background
static const char *MESH_FS =
    "VIN vec3 vPos;\n"
    "VIN vec3 vNrm;\n"
    "uniform vec3 uColor;\n"
    "uniform vec4 uMat;\n"  // metal, rough, emissive, alpha
    "uniform vec4 uTint;\n"  // rgb, strength
    "uniform vec3 uEye;\n"
    "uniform vec3 uAccent;\n"
    "uniform vec3 uFog;\n"
    "uniform vec2 uFogRange;\n"
    "uniform float uTime;\n"
    "uniform float uRim;\n" TONE
    "void main() {\n"
    "  vec3 N = normalize(vNrm);\n"
    "  vec3 V = normalize(uEye - vPos);\n"
    "  if(dot(N, V) < 0.0) N = -N;\n"  // open meshes from CAD: light the side we see
    "  float metal = uMat.x, rough = uMat.y;\n"
    "  vec3 base = mix(uColor, uTint.rgb, uTint.a * 0.6);\n"
    "  vec3 L1 = normalize(vec3(0.45, -0.35, 0.82));\n"
    "  vec3 L2 = normalize(vec3(-0.6, 0.7, 0.25));\n"
    "  float d1 = max(dot(N, L1), 0.0), d2 = max(dot(N, L2), 0.0);\n"
    "  float hemi = 0.5 + 0.5 * N.z;\n"
    "  vec3 amb = mix(vec3(0.05, 0.055, 0.065), vec3(0.20, 0.24, 0.30), hemi);\n"
    "  vec3 diff = base * (1.0 - 0.7 * metal) * (amb + d1 * vec3(1.0, 0.98, 0.95) * 1.1 + d2 * vec3(0.35, 0.30, 0.26));\n"
    "  float sh = exp2(11.0 * (1.0 - rough) + 1.0);\n"
    "  vec3 H1 = normalize(L1 + V);\n"
    "  vec3 f0 = mix(vec3(0.04), base, metal);\n"
    "  vec3 spec = f0 * pow(max(dot(N, H1), 0.0), sh) * (sh + 8.0) / 8.0 * d1;\n"
    "  float fr = pow(1.0 - max(dot(N, V), 0.0), 3.0);\n"
    "  vec3 rim = uAccent * fr * uRim + uTint.rgb * fr * uTint.a * 1.5;\n"
    "  float pulse = 0.88 + 0.12 * sin(uTime * 2.4);\n"
    "  vec3 emis = base * uMat.z * pulse * 1.6;\n"
    "  vec3 c = diff + spec + rim + emis;\n"
    "  float fog = clamp((distance(uEye, vPos) - uFogRange.x) / (uFogRange.y - uFogRange.x), 0.0, 1.0);\n"
    "  c = mix(tone(c), uFog, fog * fog);\n"
    "  FRAG = vec4(c, uMat.w);\n"
    "}\n";

// ---- background: a vertical gradient with a vignette ------------------------------

static const char *BG_VS =
    "ATTR vec2 aPos;\n"
    "VOUT vec2 vUv;\n"
    "void main() { vUv = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.999, 1.0); }\n";

static const char *BG_FS =
    "VIN vec2 vUv;\n"
    "uniform vec3 uTop;\n"
    "uniform vec3 uBottom;\n"
    "void main() {\n"
    "  vec3 c = mix(uBottom, uTop, smoothstep(0.45, 1.0, vUv.y));\n"  // the lower half is the fog the floor fades into
    "  vec2 q = vUv - 0.5;\n"
    "  c *= 1.0 - 0.55 * dot(q, q);\n"
    "  FRAG = vec4(c, 1.0);\n"
    "}\n";

// ---- the floor: a procedural grid, the reach circle, a soft shadow at the base ----

static const char *GRID_VS =
    "ATTR vec3 aPos;\n"
    "uniform mat4 uViewProj;\n"
    "uniform float uSize;\n"
    "VOUT vec2 vXY;\n"
    "void main() { vXY = aPos.xy * uSize; gl_Position = uViewProj * vec4(vXY, -0.002 * uSize, 1.0); }\n";  // a hair below 0: no fighting with parts on the floor

static const char *GRID_FS =
    "VIN vec2 vXY;\n"
    "uniform vec3 uEye;\n"
    "uniform vec3 uAccent;\n"
    "uniform vec3 uFog;\n"
    "uniform float uCell;\n"   // minor grid [m]
    "uniform float uReach;\n"  // the arm's reach [m]
    "uniform float uBase;\n"   // shadow radius [m]
    "uniform float uSize;\n"
    "float line(float x, float w) { float d = abs(fract(x - 0.5) - 0.5); return 1.0 - smoothstep(0.0, w, d); }\n"
    "void main() {\n"
    "  float dist = distance(uEye, vec3(vXY, 0.0));\n"
    "  float w = 0.0012 * dist / uCell + 0.008;\n"  // about a pixel wide at any distance
    "  float minor = max(line(vXY.x / uCell, w), line(vXY.y / uCell, w));\n"
    "  float major = max(line(vXY.x / (uCell * 10.0), w * 0.1), line(vXY.y / (uCell * 10.0), w * 0.1));\n"
    "  float r = length(vXY);\n"
    "  float reach = 1.0 - smoothstep(0.0, w * uCell * 2.0, abs(r - uReach));\n"
    "  float ax = max(1.0 - smoothstep(0.0, w * uCell, abs(vXY.y)) * step(0.0, vXY.x), 1.0 - smoothstep(0.0, w * uCell, abs(vXY.x)) * step(0.0, vXY.y));\n"
    "  vec3 c = vec3(0.035, 0.045, 0.055);\n"
    "  c += vec3(0.07, 0.09, 0.11) * minor * 0.55 + vec3(0.10, 0.13, 0.16) * major;\n"
    "  c += uAccent * reach * 0.55;\n"
    "  c += uAccent * 0.18 * (1.0 - smoothstep(0.0, uReach, r));\n"  // the work area glows faintly
    "  c *= mix(0.35, 1.0, smoothstep(uBase * 0.6, uBase * 1.8, r));\n"  // contact shadow
    "  float fade = smoothstep(uSize * 0.45, uSize * 0.12, r);\n"
    "  FRAG = vec4(mix(uFog, c, fade), 1.0);\n"
    "}\n";

// ---- lines: obstacle edges and the tool point's trail -----------------------------

static const char *LINE_VS =
    "ATTR vec4 aPos;\n"  // xyz, alpha
    "uniform mat4 uViewProj;\n"
    "VOUT float vA;\n"
    "void main() { vA = aPos.w; gl_Position = uViewProj * vec4(aPos.xyz, 1.0); }\n";

static const char *LINE_FS =
    "VIN float vA;\n"
    "uniform vec4 uColor;\n"
    "void main() { FRAG = vec4(uColor.rgb, uColor.a * vA); }\n";

// ---- the clearance shell around the tool point: transparent, bright at the rim -----

static const char *SHELL_FS =
    "VIN vec3 vPos;\n"
    "VIN vec3 vNrm;\n"
    "uniform vec3 uEye;\n"
    "uniform vec4 uColor;\n"
    "uniform float uTime;\n"
    "void main() {\n"
    "  vec3 N = normalize(vNrm), V = normalize(uEye - vPos);\n"
    "  float fr = pow(1.0 - abs(dot(N, V)), 2.5);\n"
    "  float bands = 0.5 + 0.5 * sin(vPos.z * 160.0 - uTime * 3.0);\n"  // slow scan lines
    "  float a = uColor.a * (0.05 + 0.75 * fr + 0.06 * bands);\n"
    "  FRAG = vec4(uColor.rgb * (0.6 + fr), a);\n"
    "}\n";

#endif
