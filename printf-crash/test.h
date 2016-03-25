#include <stdint.h>
#include <stdio.h>

typedef unsigned long ULong;

 struct
Bigint {
	struct Bigint *next;
	int k, maxwds, sign, wds;
	ULong x[1];
	};

 typedef struct Bigint Bigint;

//typedef _Bigint Bigint;

Bigint *pow5mult_nl
#ifdef KR_headers
	(b, k) Bigint *b; int k;
#else
	(Bigint *b, int k)
#endif
;

union double_union
{
  double d;
  uint32_t i[2];
};

void test() {
  union double_union d2;
  int i;

  d2.i[0] = 0x9999999A;
  d2.i[1] = 0x3FF99999;
  i = -3;

  double ds;

  ds = (d2.d - 1.5) * 0.289529654602168 + 0.1760912590558 + i * 0.301029995663981;

  union double_union ds_u;
  ds_u.d = ds;

  printf("Got 0x%08X%08X\n", ds_u.i[1], ds_u.i[0]);
}
