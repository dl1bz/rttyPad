#include "RTTYRx.h"
#include "Baudot.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

#define RTTY_RX_STATE_IDLE 0
#define RTTY_RX_STATE_START 1
#define RTTY_RX_STATE_DATA 2
#define RTTY_RX_STATE_STOP 3
#define RTTY_RX_MAX_SYMBOL_SAMPLES 4096
#define RTTY_SQL_DEFAULT_OPEN 0.030f
#define RTTY_SQL_DEFAULT_CLOSE 0.015f
#define RTTY_SQL_ALPHA 0.0015f

struct RTTY_RX {
  RTTY_RX_CONFIG cfg;
  RTTY_RX_XY_CALLBACK xy_callback;
  void *xy_context;
  unsigned int xy_decimation;
  unsigned int xy_interval;
  RTTY_RX_SCOPE scope;
  double mark_phase;
  double space_phase;
  double mark_inc;
  double space_inc;
  float mark_i;
  float mark_q;
  float space_i;
  float space_q;
  float level_ref;
  float mark_smooth;
  float space_smooth;
  float soft_smooth;
  float decision_soft;
  float sql_avg;
  float gate_tone_avg;
  float gate_quality_avg;
  float gate_tone_floor;
  float gate_tone_peak;
  int gate_candidate_blocks;
  int sql_open;
  int gate_open;
  float mark_env;
  float space_env;
  float mark_noise;
  float space_noise;
  float lp_alpha;
  double samples_per_bit;
  int symbollen;
  int counter;
  int rx_state;
  int decision_bit;
  int bit_buf_pos;
  uint8_t bit_buf[RTTY_RX_MAX_SYMBOL_SAMPLES];
  int bitcntr;
  uint8_t rx_symbol;
  BAUDOT_DECODER baudot;
};

static double rtty_clamp_double(double value, double min_value, double max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static float rtty_clamp_float(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static void rtty_rx_update_derived(RTTY_RX *rx) {
  double samplerate;
  double baud;
  double center_hz;
  double shift_hz;
  double mark_hz;
  double space_hz;
  double filter_hz;
  double tau;
  if (rx == NULL) {
    return;
  }
  samplerate = rx->cfg.samplerate;
  if (samplerate < 8000.0) {
    samplerate = 48000.0;
    rx->cfg.samplerate = samplerate;
  }
  baud = rx->cfg.baud;
  if (baud < 10.0) {
    baud = 45.45;
    rx->cfg.baud = baud;
  }
  center_hz = rx->cfg.center_hz;
  if (center_hz < 100.0) {
    center_hz = 2295.0;
    rx->cfg.center_hz = center_hz;
  }
  shift_hz = rx->cfg.shift_hz;
  if (shift_hz < 10.0) {
    shift_hz = 170.0;
    rx->cfg.shift_hz = shift_hz;
  }
  if (rx->cfg.sql_open <= 0.0f) {
    rx->cfg.sql_open = RTTY_SQL_DEFAULT_OPEN;
  }
  if (rx->cfg.sql_close <= 0.0f || rx->cfg.sql_close >= rx->cfg.sql_open) {
    rx->cfg.sql_close = rx->cfg.sql_open * 0.5f;
  }
  mark_hz = center_hz - (shift_hz * 0.5);
  space_hz = center_hz + (shift_hz * 0.5);
  mark_hz = rtty_clamp_double(mark_hz, 1.0, samplerate * 0.45);
  space_hz = rtty_clamp_double(space_hz, 1.0, samplerate * 0.45);
  rx->mark_inc = 2.0 * M_PI * mark_hz / samplerate;
  rx->space_inc = 2.0 * M_PI * space_hz / samplerate;
  filter_hz = baud * 1.5;
  if (filter_hz < 40.0) {
    filter_hz = 40.0;
  }
  if (filter_hz > 300.0) {
    filter_hz = 300.0;
  }
  tau = 1.0 / (2.0 * M_PI * filter_hz);
  rx->lp_alpha = (float)(1.0 - exp(-1.0 / (samplerate * tau)));
  rx->lp_alpha = rtty_clamp_float(rx->lp_alpha, 0.001f, 1.0f);
  rx->samples_per_bit = samplerate / baud;
  if (rx->samples_per_bit < 1.0) {
    rx->samples_per_bit = 1.0;
  }
  rx->symbollen = (int)(rx->samples_per_bit + 0.5);
  if (rx->symbollen < 8) {
    rx->symbollen = 8;
  }
  if (rx->symbollen > RTTY_RX_MAX_SYMBOL_SAMPLES) {
    rx->symbollen = RTTY_RX_MAX_SYMBOL_SAMPLES;
  }
}

RTTY_RX *rtty_rx_create(const RTTY_RX_CONFIG *cfg) {
  RTTY_RX *rx = (RTTY_RX *)calloc(1, sizeof(*rx));
  if (rx == NULL) {
    return NULL;
  }
  rtty_rx_configure(rx, cfg);
  return rx;
}

void rtty_rx_destroy(RTTY_RX *rx) {
  free(rx);
}

void rtty_rx_reset(RTTY_RX *rx) {
  if (rx == NULL) {
    return;
  }
  memset(&rx->scope, 0, sizeof(rx->scope));
  rx->xy_decimation = 0;
  rx->mark_phase = 0.0;
  rx->space_phase = 0.0;
  rx->mark_i = 0.0f;
  rx->mark_q = 0.0f;
  rx->space_i = 0.0f;
  rx->space_q = 0.0f;
  rx->level_ref = 1.0e-6f;
  rx->mark_smooth = 0.0f;
  rx->space_smooth = 0.0f;
  rx->soft_smooth = 0.0f;
  rx->decision_soft = 0.0f;
  rx->sql_avg = 0.0f;
  rx->gate_tone_avg = 0.0f;
  rx->gate_quality_avg = 0.0f;
  rx->gate_tone_floor = 1.0e-6f;
  rx->gate_tone_peak = 1.0e-6f;
  rx->gate_candidate_blocks = 0;
  rx->sql_open = 0;
  rx->gate_open = 0;
  rx->mark_env = 0.0f;
  rx->space_env = 0.0f;
  rx->mark_noise = 0.0f;
  rx->space_noise = 0.0f;
  rx->counter = 0;
  rx->rx_state = RTTY_RX_STATE_IDLE;
  rx->decision_bit = 1;
  rx->bit_buf_pos = 0;
  memset(rx->bit_buf, 1, sizeof(rx->bit_buf));
  rx->bitcntr = 0;
  rx->rx_symbol = 0;
  baudot_init(&rx->baudot);
}

void rtty_rx_configure(RTTY_RX *rx, const RTTY_RX_CONFIG *cfg) {
  if (rx == NULL) {
    return;
  }
  if (cfg != NULL) {
    rx->cfg = *cfg;
  } else {
    rx->cfg.samplerate = 48000.0;
    rx->cfg.baud = 45.45;
    rx->cfg.center_hz = 2295.0;
    rx->cfg.shift_hz = 170.0;
    rx->cfg.reverse = 0;
    rx->cfg.sql_open = RTTY_SQL_DEFAULT_OPEN;
    rx->cfg.sql_close = RTTY_SQL_DEFAULT_CLOSE;
  }
  rtty_rx_update_derived(rx);
  /* Keep the XY capture rate near 1 kHz at every input sample rate. */
  rx->xy_interval = (unsigned int)(rx->cfg.samplerate / 1000.0 + 0.5);
  if (rx->xy_interval < 1) { rx->xy_interval = 1; }
  rtty_rx_reset(rx);
}

void rtty_rx_set_reverse(RTTY_RX *rx, int reverse) {
  if (rx == NULL) {
    return;
  }
  rx->cfg.reverse = reverse ? 1 : 0;
  rx->scope.reverse = rx->cfg.reverse;
}


static float rtty_rx_track_peak(float current, float value) {
  float alpha;
  if (current <= 0.0f) {
    return value;
  }
  alpha = (value > current) ? 0.010f : 0.0007f;
  return current + (alpha * (value - current));
}

static float rtty_rx_track_noise(float current, float value) {
  float alpha;
  if (current <= 0.0f) {
    return value;
  }
  alpha = (value < current) ? 0.004f : 0.00003f;
  return current + (alpha * (value - current));
}

static float rtty_rx_atc_soft(RTTY_RX *rx, float mark_mag, float space_mag) {
  float noise_floor;
  float mark_env;
  float space_env;
  float mark_signal;
  float space_signal;
  float mark_clipped;
  float space_clipped;
  float v3;
  float norm;
  rx->mark_env = rtty_rx_track_peak(rx->mark_env, mark_mag);
  rx->space_env = rtty_rx_track_peak(rx->space_env, space_mag);
  rx->mark_noise = rtty_rx_track_noise(rx->mark_noise, mark_mag);
  rx->space_noise = rtty_rx_track_noise(rx->space_noise, space_mag);
  noise_floor = (rx->mark_noise < rx->space_noise) ? rx->mark_noise : rx->space_noise;
  mark_env = rx->mark_env - noise_floor;
  space_env = rx->space_env - noise_floor;
  if (mark_env < 1.0e-9f) {
    mark_env = 1.0e-9f;
  }
  if (space_env < 1.0e-9f) {
    space_env = 1.0e-9f;
  }
  mark_clipped = (mark_mag < rx->mark_env) ? mark_mag : rx->mark_env;
  space_clipped = (space_mag < rx->space_env) ? space_mag : rx->space_env;
  mark_signal = mark_clipped - noise_floor;
  space_signal = space_clipped - noise_floor;
  if (mark_signal < 0.0f) {
    mark_signal = 0.0f;
  }
  if (space_signal < 0.0f) {
    space_signal = 0.0f;
  }
  v3 = (mark_signal * mark_env) - (space_signal * space_env) - (0.25f * ((mark_env * mark_env) -
    (space_env * space_env)));
  norm = (mark_env * mark_env) + (space_env * space_env) + 1.0e-9f;
  return rtty_clamp_float(v3 / norm, -1.0f, 1.0f);
}

static void rtty_rx_store_char(char ch, char *out, size_t outlen, size_t *outpos) {
  if (ch == '\0' || out == NULL || outpos == NULL || outlen == 0) {
    return;
  }
  if (*outpos + 1 >= outlen) {
    return;
  }
  out[*outpos] = ch;
  (*outpos)++;
  out[*outpos] = '\0';
}

static void rtty_rx_abort_character(RTTY_RX *rx) {
  rx->rx_state = RTTY_RX_STATE_IDLE;
  rx->counter = 0;
  rx->bitcntr = 0;
  rx->rx_symbol = 0;
  rx->scope.sync = 0;
}

static void rtty_rx_force_mark_idle(RTTY_RX *rx) {
  rx->rx_state = RTTY_RX_STATE_IDLE;
  rx->counter = 0;
  rx->bitcntr = 0;
  rx->rx_symbol = 0;
  rx->decision_bit = 1;
  rx->scope.sync = 0;
  if (rx->symbollen > 0) {
    memset(rx->bit_buf, 1, (size_t)rx->symbollen);
  }
}

static int rtty_rx_update_sql(RTTY_RX *rx, float mark_mag, float space_mag, float soft) {
  float decision_metric;
  (void)mark_mag;
  (void)space_mag;
  decision_metric = fabsf(soft);
  rx->sql_avg += RTTY_SQL_ALPHA * (decision_metric - rx->sql_avg);
  if (rx->sql_open) {
    if (rx->sql_avg < rx->cfg.sql_close) {
      rx->sql_open = 0;
    }
  } else {
    if (rx->sql_avg > rx->cfg.sql_open) {
      rx->sql_open = 1;
    }
  }
  return rx->sql_open;
}

static void rtty_rx_update_strict_gate(RTTY_RX *rx, float mark_raw, float space_raw) {
  float tone_raw;
  float floor_alpha;
  float tone_range;
  float tone_level;
  float decision_quality;
  float tone_open;
  float tone_close;
  float quality_open;
  float quality_close;
  int candidate;
  tone_raw = mark_raw + space_raw;
  if (tone_raw < 0.0f) {
    tone_raw = 0.0f;
  }
  if (rx->gate_tone_floor <= 0.0f) {
    rx->gate_tone_floor = tone_raw;
  }
  floor_alpha = (tone_raw < rx->gate_tone_floor) ? 0.020f : 0.0007f;
  rx->gate_tone_floor += floor_alpha * (tone_raw - rx->gate_tone_floor);
  rx->gate_tone_peak *= 0.9975f;
  if (tone_raw > rx->gate_tone_peak) {
    rx->gate_tone_peak += 0.120f * (tone_raw - rx->gate_tone_peak);
  }
  if (rx->gate_tone_peak < rx->gate_tone_floor + 1.0e-6f) {
    rx->gate_tone_peak = rx->gate_tone_floor + 1.0e-6f;
  }
  tone_range = rx->gate_tone_peak - rx->gate_tone_floor;
  if (tone_range < 1.0e-6f) {
    tone_range = 1.0e-6f;
  }
  tone_level = rtty_clamp_float((tone_raw - rx->gate_tone_floor) / tone_range, 0.0f, 1.0f);
  decision_quality = rtty_clamp_float(fabsf(rx->decision_soft), 0.0f, 1.0f);
  rx->gate_tone_avg += 0.12f * (tone_level - rx->gate_tone_avg);
  rx->gate_quality_avg += 0.12f * (decision_quality - rx->gate_quality_avg);
  tone_open = 0.46f + (rx->cfg.sql_open * 1.8f);
  if (tone_open > 0.78f) {
    tone_open = 0.78f;
  }
  tone_close = tone_open * 0.55f;
  quality_open = rx->cfg.sql_open * 3.2f;
  if (quality_open < 0.095f) {
    quality_open = 0.095f;
  }
  if (quality_open > 0.42f) {
    quality_open = 0.42f;
  }
  quality_close = quality_open * 0.55f;
  candidate = rx->sql_open &&
              rx->gate_tone_avg > tone_open &&
              rx->gate_quality_avg > quality_open;
  if (candidate) {
    if (rx->gate_candidate_blocks < 1000) {
      rx->gate_candidate_blocks++;
    }
  } else if (rx->gate_tone_avg < tone_close ||
             rx->gate_quality_avg < quality_close ||
             !rx->sql_open) {
    rx->gate_candidate_blocks = 0;
    rx->gate_open = 0;
  } else if (rx->gate_candidate_blocks > 0) {
    rx->gate_candidate_blocks--;
  }
  if (!rx->gate_open && rx->gate_candidate_blocks >= 6) {
    rx->gate_open = 1;
  }
  rx->scope.gate_open = rx->gate_open;
  rx->scope.tone_level = rx->gate_tone_avg;
  rx->scope.decision_quality = rx->gate_quality_avg;
}

static int rtty_rx_bit_at(const RTTY_RX *rx, int index) {
  int symbollen;
  int pos;
  symbollen = rx->symbollen;
  if (symbollen <= 0) {
    return 1;
  }
  while (index < 0) {
    index += symbollen;
  }
  index %= symbollen;
  pos = rx->bit_buf_pos + index;
  if (pos >= symbollen) {
    pos -= symbollen;
  }
  return rx->bit_buf[pos] ? 1 : 0;
}

static int rtty_rx_is_mark(const RTTY_RX *rx) {
  return rtty_rx_bit_at(rx, rx->symbollen / 2);
}

static int rtty_rx_is_mark_space(const RTTY_RX *rx, int *correction) {
  int sum = 0;
  int target;
  int tolerance;
  if (correction == NULL || rx->symbollen <= 0) {
    return 0;
  }
  *correction = 0;
  if (!rtty_rx_bit_at(rx, 0) || rtty_rx_bit_at(rx, rx->symbollen - 1)) {
    return 0;
  }
  for (int i = 0; i < rx->symbollen; i++) {
    sum += rtty_rx_bit_at(rx, i);
  }
  target = rx->symbollen / 2;
  tolerance = rx->symbollen / 10;
  if (tolerance < 6) {
    tolerance = 6;
  }
  if (abs(target - sum) <= tolerance) {
    *correction = sum;
    if (*correction < 1) {
      *correction = 1;
    }
    return 1;
  }
  return 0;
}

static void rtty_rx_push_bit(RTTY_RX *rx, int bit) {
  int symbollen = rx->symbollen;
  if (symbollen <= 0) {
    return;
  }
  rx->bit_buf[rx->bit_buf_pos] = bit ? 1u : 0u;
  rx->bit_buf_pos++;
  if (rx->bit_buf_pos >= symbollen) {
    rx->bit_buf_pos = 0;
  }
}

static void rtty_rx_handle_sample_bit(RTTY_RX *rx, int bit, char *out, size_t outlen, size_t *outpos) {
  int correction;
  rtty_rx_push_bit(rx, bit);
  switch (rx->rx_state) {
  case RTTY_RX_STATE_IDLE:
    rx->scope.sync = 0;
    if (rtty_rx_is_mark_space(rx, &correction)) {
      rx->rx_state = RTTY_RX_STATE_START;
      rx->counter = correction;
      rx->bitcntr = 0;
      rx->rx_symbol = 0;
      rx->scope.sync = 1;
    }
    break;
  case RTTY_RX_STATE_START:
    rx->counter--;
    if (rx->counter <= 0) {
      if (!rtty_rx_is_mark(rx)) {
        rx->rx_state = RTTY_RX_STATE_DATA;
        rx->counter = rx->symbollen;
        rx->bitcntr = 0;
        rx->rx_symbol = 0;
        rx->scope.sync = 1;
      } else {
        rtty_rx_abort_character(rx);
      }
    }
    break;
  case RTTY_RX_STATE_DATA:
    rx->counter--;
    if (rx->counter <= 0) {
      if (rtty_rx_is_mark(rx)) {
        rx->rx_symbol |= (uint8_t)(1u << rx->bitcntr);
      }
      rx->bitcntr++;
      rx->counter = rx->symbollen;
      if (rx->bitcntr >= 5) {
        rx->rx_state = RTTY_RX_STATE_STOP;
      }
      rx->scope.sync = 1;
    }
    break;
  case RTTY_RX_STATE_STOP:
    rx->counter--;
    if (rx->counter <= 0) {
      if (rtty_rx_is_mark(rx)) {
        char decoded = baudot_decode(&rx->baudot, rx->rx_symbol);
        rtty_rx_store_char(decoded, out, outlen, outpos);
      }
      rtty_rx_abort_character(rx);
    }
    break;
  default:
    rtty_rx_abort_character(rx);
    break;
  }
}

int rtty_rx_process(RTTY_RX *rx, const float *samples, size_t nsamples, char *out, size_t outlen) {
  float mark_acc = 0.0f;
  float space_acc = 0.0f;
  float mark_xy_acc = 0.0f;
  float space_xy_acc = 0.0f;
  float alpha;
  size_t outpos = 0;
  if (rx == NULL || samples == NULL || out == NULL || outlen == 0) {
    return 0;
  }
  out[0] = '\0';
  alpha = rx->lp_alpha;
  for (size_t i = 0; i < nsamples; i++) {
    float sample = samples[i];
    float mark_c = (float)cos(rx->mark_phase);
    float mark_s = (float)sin(rx->mark_phase);
    float space_c = (float)cos(rx->space_phase);
    float space_s = (float)sin(rx->space_phase);
    float mark_mag;
    float space_mag;
    float soft;
    int bit;
    rx->mark_i += alpha * ((sample * mark_c) - rx->mark_i);
    rx->mark_q += alpha * ((sample * -mark_s) - rx->mark_q);
    rx->space_i += alpha * ((sample * space_c) - rx->space_i);
    rx->space_q += alpha * ((sample * -space_s) - rx->space_q);
    if (rx->xy_callback != NULL && ++rx->xy_decimation >= rx->xy_interval) {
      float scale = rx->level_ref > 1.0e-6f ? rx->level_ref : 1.0e-6f;
      rx->xy_callback(rx->xy_context,
                      rtty_clamp_float((rx->mark_i + rx->mark_q) * 0.70710678f / scale, -1.0f, 1.0f),
                      rtty_clamp_float((rx->space_i + rx->space_q) * 0.70710678f / scale, -1.0f, 1.0f));
      rx->xy_decimation = 0;
    }
    mark_mag = sqrtf((rx->mark_i * rx->mark_i) + (rx->mark_q * rx->mark_q));
    space_mag = sqrtf((rx->space_i * rx->space_i) + (rx->space_q * rx->space_q));
    mark_acc += mark_mag;
    space_acc += space_mag;
    mark_xy_acc += (rx->mark_i + rx->mark_q) * 0.70710678f;
    space_xy_acc += (rx->space_i + rx->space_q) * 0.70710678f;
    soft = rtty_rx_atc_soft(rx, mark_mag, space_mag);
    if (rx->cfg.reverse) {
      soft = -soft;
    }
    if (!rtty_rx_update_sql(rx, mark_mag, space_mag, soft)) {
      rx->decision_soft += 0.002f * (0.0f - rx->decision_soft);
      rtty_rx_force_mark_idle(rx);
      rtty_rx_push_bit(rx, 1);
      bit = 1;
    } else {
      rx->decision_soft += 0.02f * (soft - rx->decision_soft);
      bit = rx->decision_bit;
      if (rx->decision_soft > 0.04f) {
        bit = 1;
      } else if (rx->decision_soft < -0.04f) {
        bit = 0;
      }
      rx->decision_bit = bit;
      rtty_rx_handle_sample_bit(rx, bit, out, outlen, &outpos);
    }
    rx->mark_phase += rx->mark_inc;
    rx->space_phase += rx->space_inc;
    if (rx->mark_phase >= 2.0 * M_PI) {
      rx->mark_phase -= 2.0 * M_PI;
    }
    if (rx->space_phase >= 2.0 * M_PI) {
      rx->space_phase -= 2.0 * M_PI;
    }
  }
  if (nsamples > 0) {
    float mark_avg = mark_acc / (float)nsamples;
    float space_avg = space_acc / (float)nsamples;
    float block_ref;
    float mark_norm;
    float space_norm;
    float mark_xy;
    float space_xy;
    float soft;
    float smooth_alpha = 0.20f;
    float ref_decay = 0.998f;
    if (rx->mark_smooth <= 0.0f && rx->space_smooth <= 0.0f) {
      rx->mark_smooth = mark_avg;
      rx->space_smooth = space_avg;
    } else {
      rx->mark_smooth += smooth_alpha * (mark_avg - rx->mark_smooth);
      rx->space_smooth += smooth_alpha * (space_avg - rx->space_smooth);
    }
    block_ref = rx->mark_smooth;
    if (rx->space_smooth > block_ref) {
      block_ref = rx->space_smooth;
    }
    rx->level_ref *= ref_decay;
    if (block_ref > rx->level_ref) {
      rx->level_ref = block_ref;
    }
    if (rx->level_ref < 1.0e-6f) {
      rx->level_ref = 1.0e-6f;
    }
    mark_norm = rtty_clamp_float(rx->mark_smooth / rx->level_ref, 0.0f, 1.0f);
    space_norm = rtty_clamp_float(rx->space_smooth / rx->level_ref, 0.0f, 1.0f);
    mark_xy = rtty_clamp_float((mark_xy_acc / (float)nsamples) / rx->level_ref, -1.0f, 1.0f);
    space_xy = rtty_clamp_float((space_xy_acc / (float)nsamples) / rx->level_ref, -1.0f, 1.0f);
    soft = (mark_norm - space_norm) / (mark_norm + space_norm + 1.0e-6f);
    if (rx->cfg.reverse) {
      soft = -soft;
    }
    rx->soft_smooth += 0.35f * (soft - rx->soft_smooth);
    rx->scope.mark_mag = mark_norm;
    rx->scope.space_mag = space_norm;
    rx->scope.soft = rtty_clamp_float(rx->decision_soft, -1.0f, 1.0f);
    rx->scope.bit = rx->decision_bit;
    rx->scope.reverse = rx->cfg.reverse;
    rx->scope.carrier = rx->sql_open;
    rtty_rx_update_strict_gate(rx, rx->mark_smooth, rx->space_smooth);
    rx->scope.xy_mark = mark_xy;
    rx->scope.xy_space = space_xy;
  }
  return (int)outpos;
}

void rtty_rx_get_scope(const RTTY_RX *rx, RTTY_RX_SCOPE *scope) {
  if (rx == NULL || scope == NULL) {
    return;
  }
  *scope = rx->scope;
}

void rtty_rx_set_xy_callback(RTTY_RX *rx, RTTY_RX_XY_CALLBACK callback, void *context) {
  if (rx == NULL) { return; }
  rx->xy_callback = callback;
  rx->xy_context = context;
  rx->xy_decimation = 0;
}
