#ifndef RTTY_RX_H
#define RTTY_RX_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  double samplerate;
  double baud;
  double center_hz;
  double shift_hz;
  int reverse;
  float sql_open;
  float sql_close;
} RTTY_RX_CONFIG;

typedef struct {
  float mark_mag;
  float space_mag;
  float soft;
  int bit;
  int sync;
  int reverse;
  int carrier;
  int gate_open;
  float tone_level;
  float decision_quality;
  float xy_mark;
  float xy_space;
} RTTY_RX_SCOPE;

typedef struct RTTY_RX RTTY_RX;
typedef void (*RTTY_RX_XY_CALLBACK)(void *context, float mark, float space);

RTTY_RX *rtty_rx_create(const RTTY_RX_CONFIG *cfg);
void rtty_rx_destroy(RTTY_RX *rx);
void rtty_rx_reset(RTTY_RX *rx);
void rtty_rx_configure(RTTY_RX *rx, const RTTY_RX_CONFIG *cfg);
void rtty_rx_set_reverse(RTTY_RX *rx, int reverse);
int rtty_rx_process(RTTY_RX *rx, const float *samples, size_t nsamples, char *out, size_t outlen);
void rtty_rx_set_xy_callback(RTTY_RX *rx, RTTY_RX_XY_CALLBACK callback, void *context);
void rtty_rx_get_scope(const RTTY_RX *rx, RTTY_RX_SCOPE *scope);

#ifdef __cplusplus
}
#endif

#endif
