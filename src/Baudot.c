#include "Baudot.h"

#define BAUDOT_SHIFT_FIGURES 0x1B
#define BAUDOT_SHIFT_LETTERS 0x1F

static const char letters[32] = {
  '\0', 'E', '\n', 'A', ' ', 'S', 'I', 'U', '\r', 'D', 'R', 'J', 'N', 'F', 'C', 'K',
  'T', 'Z', 'L', 'W', 'H', 'Y', 'P', 'Q', 'O', 'B', 'G', '\0', 'M', 'X', 'V', '\0'
};

static const char figures[32] = {
  '\0', '3', '\n', '-', ' ', '\'', '8', '7', '\r', '$', '4', '\a', ',', '!', ':', '(',
  '5', '"', ')', '2', '#', '6', '0', '1', '9', '?', '&', '\0', '.', '/', ';', '\0'
};

void baudot_init(BAUDOT_DECODER *decoder) {
  if (decoder == 0) {
    return;
  }
  decoder->state = BAUDOT_LETTERS;
}

char baudot_decode(BAUDOT_DECODER *decoder, uint8_t symbol) {
  if (decoder == 0) {
    return '\0';
  }
  symbol &= 0x1f;
  if (symbol == BAUDOT_SHIFT_FIGURES) {
    decoder->state = BAUDOT_FIGURES;
    return '\0';
  }
  if (symbol == BAUDOT_SHIFT_LETTERS) {
    decoder->state = BAUDOT_LETTERS;
    return '\0';
  }
  /*
   * RX Unshift On Space (UOS): a space is identical in both ITA2 shifts.
   * Return the decoded space first, but recover the decoder to LTRS for
   * the following character.  This limits a lost LTRS shift to the word
   * before the next space instead of leaving the receiver stuck in FIGS.
   */
  if (symbol == 0x04) {
    decoder->state = BAUDOT_LETTERS;
    return ' ';
  }
  return (decoder->state == BAUDOT_FIGURES) ? figures[symbol] : letters[symbol];
}
