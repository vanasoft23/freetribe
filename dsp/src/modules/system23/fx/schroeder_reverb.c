// // types: int32_t sample in Q1.31
// #define NUM_COMBS 4
// #define NUM_ALLP  2

// typedef struct {
//   int32_t *buf;
//   int32_t len;
//   int32_t idx;
//   int32_t feedback_gain; // Q1.31
//   int32_t lp_state;      // for simple damping in comb
//   int32_t lp_coeff;      // Q1.31 (damping)
// } Comb;

// typedef struct {
//   int32_t *buf;
//   int32_t len;
//   int32_t idx;
//   int32_t g; // allpass coeff Q1.31
// } Allpass;

// Comb combs[NUM_COMBS];
// Allpass allp[NUM_ALLP];

// static inline int32_t mul32(int32_t a, int32_t b) {
//   // Replace with BF523 MAC intrinsic for best speed
//   // returns (a*b)>>31  (Q1.31 multiply)
//   int64_t t = (int64_t)a * (int64_t)b;
//   return (int32_t)(t >> 31);
// }

// int32_t process_sample(int32_t in) {
//   int64_t comb_sum = 0;
//   // parallel combs
//   for (int i=0;i<NUM_COMBS;i++) {
//     Comb *c = &combs[i];
//     int32_t out = c->buf[c->idx];           // delayed sample
//     // damping: simple one-pole on feedback path
//     int32_t damped = mul32(c->lp_coeff, c->lp_state) + mul32((~c->lp_coeff)+1, out);
//     // new feedback sample = in + g * damped
//     int32_t fb = in + mul32(c->feedback_gain, damped);
//     c->buf[c->idx] = fb;
//     if (++c->idx >= c->len) c->idx = 0;
//     c->lp_state = damped;
//     comb_sum += out;
//   }
//   int32_t x = (int32_t)(comb_sum / NUM_COMBS); // mix down

//   // series allpasses
//   for (int i=0;i<NUM_ALLP;i++) {
//     Allpass *a = &allp[i];
//     int32_t delayed = a->buf[a->idx];
//     int32_t y = mul32(a->g, x) + delayed;
//     a->buf[a->idx] = x - mul32(a->g, y);
//     if (++a->idx >= a->len) a->idx = 0;
//     x = y;
//   }

//   // mix wet/dry outside if desired
//   return x; // wet out (combine with dry elsewhere)
// }