/*
 * cx8_crt.h — Chromaplex 8 CRT Post-Processing
 *
 * Software-based CRT shader effect applied after rendering:
 *   - Scanlines (alternating row dimming)
 *   - Slight vignette (edge darkening)
 *   - Phosphor bloom (bright pixel bleed)
 *   - Optional curvature (barrel distortion)
 */

#ifndef CX8_CRT_H
#define CX8_CRT_H

#include "cx8.h"
#include <stdint.h>

/* CRT effect modes */
#define CX8_CRT_OFF          0
#define CX8_CRT_SCANLINES    1   /* scanlines only           */
#define CX8_CRT_FULL         2   /* scanlines + vignette     */
#define CX8_CRT_HEAVY        3   /* full + curvature + bloom */

/* Initialise CRT post-processing */
void cx8_crt_init(void);

/* Set CRT mode (CX8_CRT_OFF / SCANLINES / FULL / HEAVY) */
void cx8_crt_set_mode(int mode);

/* Get current CRT mode */
int  cx8_crt_get_mode(void);

/* Cycle to next CRT mode (for hotkey toggle) */
void cx8_crt_cycle(void);

/* Apply CRT effect to a framebuffer (in-place, ARGB32) */
void cx8_crt_apply(uint32_t *pixels, int w, int h);

/* Set individual parameters */
void cx8_crt_set_scanline_intensity(float intensity);  /* 0.0-1.0 */
void cx8_crt_set_vignette_strength(float strength);    /* 0.0-1.0 */
void cx8_crt_set_curvature(float amount);              /* 0.0-1.0 */
void cx8_crt_set_bloom(float amount);                  /* 0.0-1.0 */

#endif /* CX8_CRT_H */
