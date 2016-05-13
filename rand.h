// DON'T use the functions from this file anywhere sensitive..

#include <stdint.h>
#include <stdlib.h>

// We are only guaranteed 15 bits so we do rand() 3 times
static inline uint32_t rand32(void) {
  return (rand() << 20) ^ (rand() << 10) ^ rand();
}

// Combine 2x 32 bit values to a 64 bit value
static inline uint64_t rand64(void) {
  return ((uint64_t)rand32() << 32ULL) | (uint64_t)rand32();
}
