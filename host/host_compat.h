/*
 * Host-build compatibility shims.
 *
 * The firmware is compiled against newlib, which defines some extra math
 * constants that glibc does not. Force-included by host/Makefile so the
 * shared components compile unchanged on the host.
 */
#pragma once
#include <math.h>

#ifndef M_SQRT3
#define M_SQRT3 1.73205080756887719000
#endif

#ifndef M_SQRT1_3
#define M_SQRT1_3 0.57735026918962576451
#endif
