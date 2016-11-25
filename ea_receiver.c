/*
ea_receiver - A lightweight Elster EnergyAxis Receiver
Copyright (C) 2016 Shaun R. Hey <shaun@shaunhey.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <complex.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_NUM_CHANNELS  6

#define BLOCK_SIZE      16384 // Number of samples to read at a time per channel
#define MODE_1          0 // 35.5555kBaud, manchester encoded
#define MODE_2          1 // 142.222kBaud, NRZ encoded
#define MODE_1_SPS      1125 // 11.25 samples per symbol @ 400ksps
#define MODE_2_SPS      281  //  2.81 samples per symbol @ 400ksps
#define MODE_1_PREAMBLE 0xAAAAAAAA55A59AA6 // Preamble + Syncword
#define MODE_2_PREAMBLE 0xAAAAAAAA9A99A656 // Preamble + Syncword
#define MODE_1_XOR_KEY  0x55 // Each byte after the preamble and syncword is
#define MODE_2_XOR_KEY  0xAA // XOR'd with one of these values
#define NOISE_THRESHOLD 5 // Number of noisy samples to tolerate

#define STATE_SEARCHING         0 // Searching for preamble + syncword
#define STATE_RECEIVING_MSG_LEN 1 // ...found, receiving message length
#define STATE_RECEIVING_MSG     2 // ...receiving message body

struct program_options {
  FILE    *input;
  uint8_t  num_channels;
} o;

struct globals {
  uint8_t mode;
  uint8_t state;
} g;

float u8f_table[UINT8_MAX];

void usage()
{
  fprintf(stderr,
      "ea_receiver - A lightweight Elster EnergyAxis receiver\n"
      "Usage: ea_receiver [options] FILE\n"
      "\n"
      "  FILE        Unsigned 8-bit IQ file to process (or \"-\" for stdin)\n"
      "  -c N        Number of 400kHz channels to receive (1-255, default 6)\n"
      "\n");
}

void reset()
{
  g.mode = MODE_1;
  g.state = STATE_SEARCHING;
}

// Calculates message checksum
uint16_t crc_ccitt(uint8_t *msg, uint8_t len)
{
  uint16_t crc = 0xFFFF;
  uint16_t poly = 0x8408;
  uint16_t i, j, k;

  for (i = 0; i < len; i++) {
    for (j = 0; j < 8; j++) {
      k = crc & 1;
      crc >>= 1;
      if (msg[i] & (1 << j)) {
        k ^= 1;
      }
      if (k) {
        crc ^= poly;
      }
    }
  }

  crc ^= 0xFFFF;
  return crc;
}

// Validate the 16-bit CRC found at the end of a message
bool validate_crc(uint8_t *msg, uint16_t len)
{
  uint16_t calculated_crc = crc_ccitt(msg, len-2);
  uint16_t expected_crc = msg[len-2] | (msg[len-1] << 8);
  return (calculated_crc == expected_crc ? true : false);
}

// Called when a message has been received
void on_message(uint8_t *msg, uint16_t len)
{
  if (validate_crc(msg, len)) {
    for (uint16_t i = 0; i < len; i++) {
      printf("%02x", msg[i]);
    }
    printf("\n");
    fflush(stdout);
  }
}

// Called when a symbol has been received
void on_symbol(uint8_t symbol)
{
  static uint8_t  bit;              // Current bit position
  static uint8_t  byte;             // Current byte position
  static uint64_t history;          // Last 64 bits received
  static uint16_t msg_len;          // Message length
  static uint8_t  msg[UINT16_MAX];  // Message
  static bool     toggle;           // Manchester bit value indicator
  static uint8_t  xor_key;          // Value to be xor'ed with message

  switch (g.state) {
  case STATE_SEARCHING:
    history = (history << 1) | symbol;
    if (history == MODE_1_PREAMBLE) {
      g.mode  = MODE_1;
      g.state = STATE_RECEIVING_MSG_LEN;
      bit     = 0;
      byte    = 0;
      msg_len = 0;
      toggle  = true;
      xor_key = MODE_1_XOR_KEY;
    } else if (history == MODE_2_PREAMBLE) {
      g.mode  = MODE_2;
      g.state = STATE_RECEIVING_MSG_LEN;
      bit     = 0;
      byte    = 0;
      msg_len = 0;
      toggle  = true;
      xor_key = MODE_2_XOR_KEY;
    }
    break;
  case STATE_RECEIVING_MSG_LEN:
    if (toggle || g.mode == MODE_2) {
      msg[byte] = (msg[byte] << 1) | symbol;
      if (bit < 7) {
        bit++;
      } else {
        msg[byte] = msg[byte] ^ xor_key;
        if (g.mode == MODE_1) {
          msg_len = msg[byte] + 2; // +2 for CRC16 at end of message
          bit = 0;
          byte++;
          g.state = STATE_RECEIVING_MSG;
        } else {
          if (byte == 1) { // 2 bytes for message length on mode 2 messages
            msg_len = msg[0] << 8 | msg[1];
            msg_len += 2; // +2 for CRC16 at end of message
            g.state = STATE_RECEIVING_MSG;
          }
          bit = 0;
          byte++;
        }
      }
    }
    toggle = !toggle;
    break;
  case STATE_RECEIVING_MSG:
    if (toggle || g.mode == MODE_2) {
      msg[byte] = (msg[byte] << 1) | symbol;
      if (bit < 7) {
        bit++;
      } else {
        msg[byte] = msg[byte] ^ xor_key;
        bit = 0;
        byte++;
        if (byte == msg_len) {
          on_message(msg, msg_len);
          reset();
        }
      }
    }
    toggle = !toggle;
    break;
  default:
    break;
  }
}

// Convert an 8-bit i/q sample to complex floating point
float complex cu8_to_cf(uint8_t i, uint8_t q)
{
  float fi = u8f_table[i];
  float fq = u8f_table[q];
  return fi + fq * I;
}

// Calculate the angle between two samples.
float calc_angle(float complex new, float complex old)
{
  float complex d = new * conj(old);
  return atan2f(cimagf(d), crealf(d));
}

// Determine number of symbols represented by a consecutive number of samples
uint8_t calc_symbol_count(uint8_t sample_count)
{
  uint32_t symbol_count = 0;
  if (g.mode == MODE_1) { //11.25 samples per symbol @ 400ksps
    symbol_count = ((sample_count * 1000 / MODE_1_SPS) + 5) / 10;
  } else {                // 2.81 samples per symbol @ 400ksps
    symbol_count = ((sample_count * 1000 / MODE_2_SPS) + 5) / 10;
  }
  return (uint8_t)symbol_count;
}


void run()
{
  int i, j, n;
  uint8_t *samples, symbol, symbol_count, high_symbol, low_symbol, noise_count;
  uint16_t sample_count;
  uint32_t block_size;
  float complex sample, last_sample;
  float angle, last_angle;

  // If we have an even number of channels, the center frequency will actually
  // be between channels. So when we decimate the signal, high and low will be
  // flipped.
  if (o.num_channels % 2 == 0) {
    high_symbol = 0;
    low_symbol = 1;
  } else {
    high_symbol = 1;
    low_symbol = 0;
  }

  block_size = BLOCK_SIZE * o.num_channels;
  samples = calloc(block_size, sizeof(uint8_t) * 2);

  sample = last_sample = 0.0f;
  angle = last_angle = 0.0f;
  sample_count = 0;
  symbol_count = 0;
  noise_count = 0;

  while ((n = fread(samples, sizeof(uint8_t) * 2, block_size, o.input)) > 0) {

    // You may notice that we're not properly shifting and filtering
    // (channelizing) the input. That's because we want to (ab)use aliasing to
    // process multiple channels. This will result in corruption if more than
    // one transmission is received at the same time, but we're willing to risk
    // that to keep CPU usage low. The CRC check later will discard any
    // corrupted packets.
    for (i = 0; i < n; i += o.num_channels) {

      // Convert to a complex float for ease of use with atan2 later
      sample = cu8_to_cf(samples[i*2], samples[i*2+1]);

      // Calcuate the angle between the last sample and this one to determine
      // if we are measuring a negative or positve instantaneous frequency.
      angle = calc_angle(sample, last_sample);

      // If the polarity of the signal has flipped, we need to look at the
      // number of samples we have received, and figure out how many (if any)
      // symbols it represents.
      if (angle * last_angle > 0.0f) {
        sample_count++;
        noise_count = 0;
      } else {
        symbol = last_angle > 0.0f ? high_symbol : low_symbol;
        symbol_count = calc_symbol_count(sample_count);
        if (symbol_count) {
          for (j = 0; j < symbol_count; j++) {
            on_symbol(symbol);
          }
        } else {
          // If we were receiving, but the last group of samples didn't result
          // in a symbol, we've lost sync. There are opportunities here to
          // handle this better.
          if (g.state > STATE_SEARCHING) {
            noise_count++;
            if (noise_count > NOISE_THRESHOLD) {
              noise_count = 0;
              reset();
            }
          }
        }
        sample_count = 1;
      }
      last_sample = sample;
      last_angle = angle;
    }
  }

  free(samples);
}

void init()
{
  // Precompute the mapping between unsigned 8-bit integers and their
  // floating-point equivalents to reduce CPU usage.
  for (int i = 0; i < UINT8_MAX; i++) {
    float f = i;
    f -= INT8_MAX;
    f /= INT8_MAX;
    u8f_table[i] = f;
  }
}

int main(int argc, char *argv[])
{
  int opt, val;

  o.num_channels = DEFAULT_NUM_CHANNELS;

  while ((opt = getopt(argc, argv, "c:")) != -1) {
    switch (opt) {
      case 'c':
        val = atoi(optarg);
        if (val > 0 && val <= UINT8_MAX) {
          o.num_channels = (uint8_t)val;
        } else {
          fprintf(stderr, "Number of channels out of range!\n");
          exit(EXIT_FAILURE);
        }
        break;
      default:
        usage();
        exit(EXIT_FAILURE);
        break;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Please specify input file\n");
    exit(EXIT_FAILURE);
  }

  if (strcmp("-", argv[optind]) == 0) {
    o.input = stdin;
  } else {
    o.input = fopen(argv[optind], "rb");
    if (!o.input) {
      perror(argv[optind]);
      exit(EXIT_FAILURE);
    }
  }

  init();
  run();

  if (o.input != stdin) {
    fclose(o.input);
  }

  return 0;
}
