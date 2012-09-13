#include "PackedArray.h"

#include <assert.h>
#include <stdlib.h>

PackedArray* PackedArray_create(uint32_t bitsPerItem, uint32_t count)
{
  PackedArray* a;
  size_t bufferSize;

  assert(bitsPerItem > 0);
  assert(bitsPerItem <= 32);

  bufferSize = sizeof(uint32_t) * (((uint64_t)bitsPerItem * (uint64_t)count + 31) / 32);
  a = malloc(sizeof(PackedArray) + bufferSize);

  if (a != NULL)
  {
    a->buffer[((uint64_t)bitsPerItem * (uint64_t)count + 31) / 32 - 1] = 0;
    a->bitsPerItem = bitsPerItem;
    a->count = count;
  }

  return a;
}

void PackedArray_destroy(PackedArray* a)
{
  assert(a);
  free(a);
}

void PackedArray_pack(PackedArray* a, const uint32_t offset, const uint32_t* in, uint32_t count)
{
  uint32_t* __restrict out;
  uint32_t bitsPerItem;
  uint32_t startBit;
  uint32_t bitsAvailable;
  uint32_t mask;
  uint32_t packed;

  assert(a != NULL);
  assert(in != NULL);

  bitsPerItem = a->bitsPerItem;

  out = &a->buffer[(uint64_t)offset * (uint64_t)bitsPerItem / 32];
  startBit = ((uint64_t)offset * (uint64_t)bitsPerItem) % 32;

  bitsAvailable = 32 - startBit;

  mask = (uint32_t)(1ULL << bitsPerItem) - 1;

  packed = *out;

  while (count--)
  {
    uint32_t value = *in++;

    assert(0 == (~mask & value));

    if (bitsPerItem <= bitsAvailable)
    {
      packed = (packed & ~(mask << startBit)) | (value << startBit);

      startBit += bitsPerItem;
      bitsAvailable -= bitsPerItem;
    }
    else if (bitsAvailable == 0)
    {
      *out++ = packed;
      packed = *out;

      startBit = 0;
      bitsAvailable = 32;

      packed = (packed & ~mask) | value;

      startBit += bitsPerItem;
      bitsAvailable -= bitsPerItem;
    }
    else
    {
      // value spans 2 buffer cells
      uint32_t low, high;

      low = value << startBit;
      high = value >> bitsAvailable;

      packed = (packed & ~(mask << startBit)) | low;
      *out++ = packed;

      packed = *out;
      packed = (packed & ~(mask >> (32 - startBit))) | high;

      startBit = (startBit + bitsPerItem) % 32;
      bitsAvailable = 32 - startBit;
    }
  }
  *out = packed;
}

void PackedArray_unpack(const PackedArray* a, const uint32_t offset, uint32_t* out, uint32_t count)
{
  const uint32_t* __restrict in;
  uint32_t bitsPerItem;
  uint32_t startBit;
  uint32_t bitsAvailable;
  uint32_t mask;
  uint32_t packed;

  assert(a != NULL);
  assert(out != NULL);

  bitsPerItem = a->bitsPerItem;

  in = &a->buffer[(uint64_t)offset * (uint64_t)bitsPerItem / 32];
  startBit = ((uint64_t)offset * (uint64_t)bitsPerItem) % 32;

  bitsAvailable = 32 - startBit;

  mask = (uint32_t)(1ULL << bitsPerItem) - 1;

  packed = *in;

  while (count--)
  {
    uint32_t value;

    if (bitsPerItem <= bitsAvailable)
    {
      value = (packed >> startBit) & mask;
      *out++ = value;

      startBit += bitsPerItem;
      bitsAvailable -= bitsPerItem;
    }
    else if (bitsAvailable == 0)
    {
      packed = *++in;
      value = packed & mask;
      *out++ = value;

      startBit = bitsPerItem;
      bitsAvailable = 32 - bitsPerItem;
    }
    else
    {
      // value spans 2 buffer cells
      uint32_t low, high;

      low = packed >> startBit;
      packed = *++in;
      high = packed << (32 - startBit);

      value = low ^ ((low ^ high) & (mask >> bitsAvailable << bitsAvailable));
      *out++ = value;

      startBit = (startBit + bitsPerItem) % 32;
      bitsAvailable = 32 - startBit;
    }
  }
}


// - 8< ------------------------------------------------------------------------

#if defined(PACKEDARRAY_SELF_TEST) && defined(PACKEDARRAY_SELF_BENCH)
  #error choose either PACKEDARRAY_SELF_TEST or PACKEDARRAY_SELF_BENCH
#endif

#if defined(PACKEDARRAY_SELF_TEST)

#include <stdio.h>
#include <string.h> // memcmp

int main(void)
{
  uint32_t bitsPerItem;

  printf("-- PackedArray self test -------------------------------------------------------\n");

  printf("\n");
  printf("1 by 1 packing / unpacking:\n");
  for (bitsPerItem = 1; bitsPerItem <= 32; ++bitsPerItem)
  {
    uint32_t mask = (uint32_t)(1ULL << bitsPerItem) - 1;
    int count;

    for (count = 1; count <= 1024; ++count)
    {
      PackedArray* array = PackedArray_create(bitsPerItem, count);
      int i;

      assert(array->count == (uint32_t)count);
      assert(array->bitsPerItem == bitsPerItem);

      for (i = 0; i < count; ++i)
      {
        uint32_t v1, v2;

        v1 = rand() & mask;
        v2 = v1 + 1;
        PackedArray_pack(array, i, &v1, 1);

        PackedArray_unpack(array, i, &v2, 1);
        assert(v1 == v2);
      }

      for (i = count - 1; i >= 0; --i)
      {
        uint32_t v1, v2;

        v1 = rand() & mask;
        v2 = v1 + 1;
        PackedArray_pack(array, i, &v1, 1);

        PackedArray_unpack(array, i, &v2, 1);
        assert(v1 == v2);
      }

      PackedArray_destroy(array);
    }
    printf("  %2d bits per item -- success.\n", bitsPerItem);
  }

  printf("\n");
  printf("bulk packing / unpacking:\n");
  for (bitsPerItem = 1; bitsPerItem <= 32; ++bitsPerItem)
  {
    uint32_t mask = (uint32_t)(1ULL << bitsPerItem) - 1;
    int count;

    for (count = 1; count <= 128; ++count)
    {
      uint32_t* v1;
      uint32_t* v2;
      PackedArray* array;
      int i, j;

      v1 = malloc(sizeof(uint32_t) * count);
      assert(v1 != NULL);
      v2 = malloc(sizeof(uint32_t) * count);
      assert(v2 != NULL);

      array = PackedArray_create(bitsPerItem, count);
      assert(array != NULL);
      
      for (i = 0; i < count; ++i)
        v1[i] = rand() & mask;

      for (i = 0; i < count; ++i)
      {
        for (j = 1; j <= count - i; ++j)
        {
          PackedArray_pack(array, i, v1, j);
          PackedArray_unpack(array, i, v2, j);

          assert(memcmp(v1, v2, j * sizeof(uint32_t)) == 0);
        }
      }

      PackedArray_destroy(array);
      free(v1);
      free(v2);
    }
    printf("  %2d bits per item -- success.\n", bitsPerItem);
  }

  return 0;
}

#elif defined(PACKEDARRAY_SELF_BENCH)  // end of: #if defined(PACKEDARRAY_SELF_TEST)

#ifndef NDEBUG
#error please define NDEBUG to inhibit asserts when compiling the benchmark
#endif

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <float.h>

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) < (y)) ? (y) : (x))
#endif

static double getChronometerTime()
{
  struct timeval now = { 0 };
  gettimeofday(&now, NULL);

  return (double)now.tv_sec + (double)now.tv_usec * 1e-6;
}

#define LOOP_COUNT 1000
static double bench_memcpy(uint32_t* in, uint32_t* out, uint32_t count)
{
  double start, end;
  uint32_t i;

  start = getChronometerTime();

  for (i = 0; i < LOOP_COUNT; ++i)
    memcpy(out, in, count * sizeof(uint32_t));

  end = getChronometerTime();

  return 1e6 * (end - start) / LOOP_COUNT;
}

static double bench_loopcpy(uint32_t* in, uint32_t* out, uint32_t count)
{
  double start, end;
  uint32_t i;

  start = getChronometerTime();

  for (i = 0; i < LOOP_COUNT; ++i)
  {
    uint32_t j;

    for (j = 0; j < count; ++j)
      out[j] = in[j];
  }

  end = getChronometerTime();

  return 1e6 * (end - start) / LOOP_COUNT;
}

static double bench_pack(uint32_t* in, PackedArray* out, uint32_t count)
{
  double start, end;
  int i;

  start = getChronometerTime();

  for (i = 0; i < LOOP_COUNT; ++i)
    PackedArray_pack(out, 0, in, count);

  end = getChronometerTime();

  return 1e6 * (end - start) / LOOP_COUNT;
}

static double bench_unpack(PackedArray* in, uint32_t* out, uint32_t count)
{
  double start, end;
  int i;

  start = getChronometerTime();

  for (i = 0; i < LOOP_COUNT; ++i)
    PackedArray_unpack(in, 0, out, count);

  end = getChronometerTime();

  return 1e6 * (end - start) / LOOP_COUNT;
}

#define MAX_ELEMENT_COUNT (1 << 18)
#define LOG2_MAX_ELEMENT_COUNT 18
int main(void)
{
  uint32_t* b1;
  uint32_t* b2;
  uint32_t count, bitsPerItem;
  PackedArray** packed;
  uint32_t i;
  double* speed_memcpy;
  double avg_memcpy, min_memcpy, max_memcpy;
  double* speed_loopcpy;
  double avg_loopcpy, min_loopcpy, max_loopcpy;
  double* speed_pack[32];
  double avg_pack, min_pack, max_pack;
  double* speed_unpack[32];
  double avg_unpack, min_unpack, max_unpack;

  printf("-- PackedArray self bench ------------------------------------------------------\n");

  b1 = malloc(sizeof(uint32_t) * MAX_ELEMENT_COUNT);
  assert(b1 != NULL);
  b2 = malloc(sizeof(uint32_t) * MAX_ELEMENT_COUNT);
  assert(b2 != NULL);

  packed = malloc(sizeof(PackedArray) * 32);
  assert(packed != NULL);
  for (i = 0; i < 32; ++i)
    packed[i] = PackedArray_create(i + 1, MAX_ELEMENT_COUNT);

  for (i = 0; i < MAX_ELEMENT_COUNT; ++i)
    b1[i] = rand();

  speed_memcpy = malloc(sizeof(double) * (LOG2_MAX_ELEMENT_COUNT + 1));
  assert(speed_memcpy != NULL);
  avg_memcpy = 0;
  min_memcpy = DBL_MAX;
  max_memcpy = 0;

  printf("memcpy:\n");
  printf("bits\tsize (B)\ttime (µs)\tspeed (B/µs)\n");

  for (count = 1, i = 0; count <= MAX_ELEMENT_COUNT; count *= 2, ++i)
  {
    double elapsed = bench_memcpy(b1, b2, count);
    double speed = count * sizeof(uint32_t) / elapsed;
    printf("%4d\t%8d\t%9.3f\t%12.3f\n", 32, (uint32_t)(count * sizeof(uint32_t)), elapsed, speed);

    avg_memcpy += speed;
    min_memcpy = MIN(min_memcpy, speed);
    max_memcpy = MAX(max_memcpy, speed);

    speed_memcpy[i] = speed;
  }

  avg_memcpy /= i;

  printf("avg (B/µs)\tmin (B/µs)\tmax (B/µs)\n");
  printf("%10.3f\t%10.3f\t%10.3f\n", avg_memcpy, min_memcpy, max_memcpy);
  printf("\n");

  speed_loopcpy = malloc(sizeof(double) * (LOG2_MAX_ELEMENT_COUNT + 1));
  assert(speed_loopcpy != NULL);
  avg_loopcpy = 0;
  min_loopcpy = DBL_MAX;
  max_loopcpy = 0;

  printf("loopcpy:\n");
  printf("bits\tsize (B)\ttime (µs)\tspeed (B/µs)\n");

  for (count = 1, i = 0; count <= MAX_ELEMENT_COUNT; count *= 2, ++i)
  {
    double elapsed = bench_loopcpy(b1, b2, count);
    double speed = count * sizeof(uint32_t) / elapsed;
    printf("%4d\t%8d\t%9.3f\t%12.3f\n", 32, (uint32_t)(count * sizeof(uint32_t)), elapsed, speed);

    avg_loopcpy += speed;
    min_loopcpy = MIN(min_loopcpy, speed);
    max_loopcpy = MAX(max_loopcpy, speed);

    speed_loopcpy[i] = speed;
  }

  avg_loopcpy /= i;

  printf("avg (B/µs)\tmin (B/µs)\tmax (B/µs)\n");
  printf("%10.3f\t%10.3f\t%10.3f\n", avg_loopcpy, min_loopcpy, max_loopcpy);
  printf("\n");

  for (bitsPerItem = 1; bitsPerItem <= 32; ++bitsPerItem)
  {
    avg_pack = 0;
    min_pack = DBL_MAX;
    max_pack = 0;
    avg_unpack = 0;
    min_unpack = DBL_MAX;
    max_unpack = 0;

    printf("pack:\t        \t         \t            \t");
    printf("unpack:\t        \t         \t            \t");
    printf("\n");
    printf("bits\tsize (B)\ttime (µs)\tspeed (B/µs)");
    printf("\t");
    printf("bits\tsize (B)\ttime (µs)\tspeed (B/µs)");
    printf("\n");

    speed_pack[bitsPerItem - 1] = malloc(sizeof(double) * (LOG2_MAX_ELEMENT_COUNT + 1));
    assert(speed_pack[bitsPerItem - 1] != NULL);
    speed_unpack[bitsPerItem - 1] = malloc(sizeof(double) * (LOG2_MAX_ELEMENT_COUNT + 1));
    assert(speed_unpack[bitsPerItem - 1] != NULL);
    for (count = 1, i = 0; count <= MAX_ELEMENT_COUNT; count *= 2, ++i)
    {
      uint32_t mask = (uint32_t)(1ULL << bitsPerItem) - 1;
      uint32_t j;
      double elapsed, speed;

      for (j = 0; j < count; ++j)
        b2[j] = b1[j] & mask;

      elapsed = bench_pack(b2, packed[bitsPerItem - 1], count);
      speed = count * sizeof(uint32_t) / elapsed;
      printf("%4d\t%8d\t%9.3f\t%12.3f", bitsPerItem, (uint32_t)(count * sizeof(uint32_t)), elapsed, speed);

      avg_pack += speed;
      min_pack = MIN(min_pack, speed);
      max_pack = MAX(max_pack, speed);

      speed_pack[bitsPerItem - 1][i] = speed;

      printf("\t");

      elapsed = bench_unpack(packed[bitsPerItem - 1], b2, count);
      speed = count * sizeof(uint32_t) / elapsed;
      printf("%4d\t%8d\t%9.3f\t%12.3f", bitsPerItem, (uint32_t)(count * sizeof(uint32_t)), elapsed, speed);

      avg_unpack += speed;
      min_unpack = MIN(min_unpack, speed);
      max_unpack = MAX(max_unpack, speed);

      speed_unpack[bitsPerItem - 1][i] = speed;

      printf("\n");
    }
    assert(i == LOG2_MAX_ELEMENT_COUNT);

    printf("avg (B/µs)\tmin (B/µs)\tmax (B/µs)");
    printf("\t\t");
    printf("avg (B/µs)\tmin (B/µs)\tmax (B/µs)");
    printf("\n");

    avg_pack /= i;
    printf("%10.3f\t%10.3f\t%10.3f", avg_pack, min_pack, max_pack);

    printf("\t\t");

    avg_unpack /= i;
    printf("%10.3f\t%10.3f\t%10.3f", avg_unpack, min_unpack, max_unpack);
    printf("\n");
    printf("\n");
  }

  printf("\n");

  printf("stats by bits per item\n");
  printf("pack:\t          \t          \t          \t");
  printf("unpack:\t          \t          \t          \t");
  printf("\n");
  printf("bits\tavg (B/µs)\tmin (B/µs)\tmax (B/µs)");
  printf("\t");
  printf("avg (B/µs)\tmin (B/µs)\tmax (B/µs)");
  printf("\n");
  for (bitsPerItem = 1; bitsPerItem <= 32; ++bitsPerItem)
  {
    avg_pack = 0;
    min_pack = DBL_MAX;
    max_pack = 0;
    avg_unpack = 0;
    min_unpack = DBL_MAX;
    max_unpack = 0;

    for (count = 1, i = 0; count <= MAX_ELEMENT_COUNT; count *= 2, ++i)
    {
      double speed;

      speed = speed_pack[bitsPerItem - 1][i];

      avg_pack += speed;
      min_pack = MIN(min_pack, speed);
      max_pack = MAX(max_pack, speed);

      speed = speed_unpack[bitsPerItem - 1][i];

      avg_unpack += speed;
      min_unpack = MIN(min_unpack, speed);
      max_unpack = MAX(max_unpack, speed);
    }
    assert(i == LOG2_MAX_ELEMENT_COUNT);

    printf("%4d\t", bitsPerItem);

    avg_pack /= i;
    printf("%10.3f\t%10.3f\t%10.3f", avg_pack, min_pack, max_pack);
    printf("\t");

    avg_unpack /= i;
    printf("%10.3f\t%10.3f\t%10.3f", avg_unpack, min_unpack, max_unpack);
    printf("\n");
  }

  printf("\n");

  printf("stats by size\n");
  printf("pack:\t          \t          \t          \t");
  printf("unpack:\t          \t          \t          \t");
  printf("\n");
  printf("size(B)\tavg (B/µs)\tmin (B/µs)\tmax (B/µs)");
  printf("\t");
  printf("avg (B/µs)\tmin (B/µs)\tmax (B/µs)");
  printf("\n");
  for (count = 1, i = 0; count <= MAX_ELEMENT_COUNT; count *= 2, ++i)
  {
    avg_pack = 0;
    min_pack = DBL_MAX;
    max_pack = 0;
    avg_unpack = 0;
    min_unpack = DBL_MAX;
    max_unpack = 0;

    for (bitsPerItem = 1; bitsPerItem <= 32; ++bitsPerItem)
    {
      double speed;

      speed = speed_pack[bitsPerItem - 1][i];

      avg_pack += speed;
      min_pack = MIN(min_pack, speed);
      max_pack = MAX(max_pack, speed);

      speed = speed_unpack[bitsPerItem - 1][i];

      avg_unpack += speed;
      min_unpack = MIN(min_unpack, speed);
      max_unpack = MAX(max_unpack, speed);
    }

    printf("%7d\t", (uint32_t)sizeof(uint32_t) * count);

    avg_pack /= 32;
    printf("%10.3f\t%10.3f\t%10.3f", avg_pack, min_pack, max_pack);
    printf("\t");

    avg_unpack /= 32;
    printf("%10.3f\t%10.3f\t%10.3f", avg_unpack, min_unpack, max_unpack);
    printf("\n");
  }

  printf("\n");

  free(b1);
  free(b2);
  free(speed_memcpy);
  free(speed_loopcpy);

  for (i = 0; i < 32; ++i)
  {
    PackedArray_destroy(packed[i]);
    free(speed_pack[i]);
    free(speed_unpack[i]);
  }

  free(packed);

  return 0;
}

#endif // end of: #elif defined(PACKEDARRAY_SELF_BENCH)

