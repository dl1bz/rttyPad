#ifndef BAUDOT_H
#define BAUDOT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BAUDOT_LETTERS = 0,
  BAUDOT_FIGURES = 1
} BAUDOT_SHIFT_STATE;

typedef struct {
  BAUDOT_SHIFT_STATE state;
} BAUDOT_DECODER;

void baudot_init(BAUDOT_DECODER *decoder);
char baudot_decode(BAUDOT_DECODER *decoder, uint8_t symbol);

#ifdef __cplusplus
}
#endif

#endif
