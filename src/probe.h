#ifndef PROBE_H
#define PROBE_H

#include <stdint.h>
#include <time.h>

typedef struct reading {
  int64_t timestamp_ms;
  float temp;
  float pres;
  float humi;
  uint32_t lux;
} reading;

int sensors_init(void);

int read_air_probe(reading *);
int read_light_probe(reading *);

#endif