#ifndef PROBE_H
#define PROBE_H

#include <stdint.h>

typedef struct reading_t {
  int64_t timestamp_ms;
  float temp;
  float pres;
  float humi;
  uint32_t lux;
} reading_t;

int probes_init(void);

int read_air_probe(reading_t *);
int read_light_probe(reading_t *);

#endif