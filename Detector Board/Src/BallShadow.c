//------------------------------------------------------------------------------
// ball shadow logging
// compute measured shadow
// compute expected shadow
// compute correlation

#include <stdint.h>
#include <math.h>
#include "UART.h"
#include "LEDsCommon.h"
#include "LEDs.h"
#include "Zero_bit.h"

//------------------------------------------------------------------------------
// forward references
float Correlation(float *x1, float *x2, int16_t len);
void  Compute_correlation        (void);
void  Construct_measured_shadow  (void);
void  Compute_expected_shadow_air(void);
void  Log_Emitter_Vector_Test    (uint32_t vector);

//------------------------------------------------------------------------------
typedef struct {
  #define  EMITTER_VECTOR_LOG_SIZE    BALL_SHADOW_SIZE
  uint16_t Emitter_Vector_Log_Index;
   int16_t Emitter_Vector_State;
  uint32_t Emitter_Vector_Log  [EMITTER_VECTOR_LOG_SIZE];
  float    Measured_shadow_high[EMITTER_VECTOR_LOG_SIZE];
  float    Measured_shadow_low [EMITTER_VECTOR_LOG_SIZE];
  float    Expected_ball_track [EMITTER_VECTOR_LOG_SIZE];
  float    Expected_shadow_high[EMITTER_VECTOR_LOG_SIZE];
  float    Expected_shadow_low [EMITTER_VECTOR_LOG_SIZE];
  float    y0;                                                  // initial vertical position in inches
  float    y_end;                                               // ending vertical position in inches
  float    flight_time_ms;                                      // number of consecutive vectors containing zeros
  float    v0_inch_per_sec;                                     // initial vertical velocity in inches / sec
  float    gravity;                                             // in inches per sec^2
  float    ball_speed_mph;                                      // computed horizontal ball speed in mph
  float    ball_diameter_inch;                                  // ball diameter in inches
  float    high_correlation;                                    // correlation of (Measured_shadow_high, Expected_shadow_high)
  float    low_correlation;                                     // correlation of (Measured_shadow_low,  Expected_shadow_low)
} Ball_shadow_t;
Ball_shadow_t Ball_shadow = {
  .Emitter_Vector_Log_Index =  0,
  .Emitter_Vector_State     = -2,
  .gravity                  = -9.81 * 100 / 2.54,               // (-9.81 meter/sec^2) * (100 cm/meter) * (1 inch/2.54 cm) = -386.2204724 inches / sec^2
  .ball_diameter_inch       = 7.0                               // ball diameter is 7 inches
};

//------------------------------------------------------------------------------
uint16_t Compute_elevation_pct(void) {
  uint32_t emitter_vector = Ball_shadow.Emitter_Vector_Log[1];
  uint16_t emitter        = 0;

  while (emitter_vector & 1) {
    emitter_vector >>= 1;
    emitter++;
    }

  // emitter contains emitter number corresponding
  // to the bit position of the light curtain first point of contact
  // compute percent elevation
  // leftmost  bit is top    of goal (emitter=31, 100%)
  // rightmost bit is bottom of goal (emitter= 0,   0%)
  // elevation percent = emitter number * 100 / 31
  // with rounding:                     y = x*100/31 + 0.5
  // with rounding, integer arithmetic: y = x*200/62 + 31/62
  //                                    y = (x*200 + 31)/62
  return (emitter*200 + 31)/62;
}

//------------------------------------------------------------------------------
float Error(float *x1, float *x2, int16_t len) {
  int16_t i;
  float error_sum = 0.0;

  if (len < 1) return 0.0;

  for (i = 0; i < len; i++) {
    float x = x1[i]-x2[i];
    error_sum += x*x;
    }

  return error_sum / (float) len;
}

//------------------------------------------------------------------------------
float Correlation(float *x1, float *x2, int16_t len) {
  int16_t i;
  float x1_mean  = 0.0;
  float x2_mean  = 0.0;
  float sum_x1x2 = 0.0;
  float sum_x1x1 = 0.0;
  float sum_x2x2 = 0.0;

  if (len < 1) return 0.0;

  for (i = 0; i < len; i++) {
    x1_mean += x1[i];
    x2_mean += x2[i];
    }
  x1_mean /= (float) len;
  x2_mean /= (float) len;

  for (i = 0; i < len; i++) {
    float x1_  = x1[i] - x1_mean;
    float x2_  = x2[i] - x2_mean;
    sum_x1x2  += x1_ * x2_;
    sum_x1x1  += x1_ * x1_;
    sum_x2x2  += x2_ * x2_;
    }

  return sum_x1x2 / sqrt(sum_x1x1 * sum_x2x2);
}

//------------------------------------------------------------------------------
// compute object type: 0=non-ball, 1=ball
uint16_t Compute_object_type(void) {
  int16_t i;
  int16_t max_span = 0;
  bool    air_ball = true;

  Ball_shadow.high_correlation = Error(Ball_shadow.Measured_shadow_high, Ball_shadow.Expected_shadow_high, Ball_shadow.Emitter_Vector_Log_Index + 1);
  Ball_shadow.low_correlation  = Error(Ball_shadow.Measured_shadow_low,  Ball_shadow.Expected_shadow_low,  Ball_shadow.Emitter_Vector_Log_Index + 1);

//return 1;                             // force object-type to "ball" for testing

  // no ball is smaller than 4 emitters
  // if we see anything smaller than 4 emitters,
  // it must be a non-ball
  // future enhancment (not yet in this build):
  // blocked emitters must be contiguous (no voids within the span)
  for (i = 1; i < Ball_shadow.Emitter_Vector_Log_Index; i++) {
    uint32_t vector      = Ball_shadow.Emitter_Vector_Log[i];
    int16_t  shadow_high = Find_Most_Significant_zero_bit (vector);
    int16_t  shadow_low  = Find_Least_Significant_zero_bit(vector);
    int16_t  span        = shadow_high - shadow_low + 1;
    if (max_span < span)
        max_span = span;

    if ((vector & 0x80000001ul) != 0x80000001ul) air_ball = false;
    }

  // check for object too big or too small
  if ((max_span < 3) || (max_span > 8)) {
    // max span is too big or too small: non-ball
    return 0;
    }

  // check air ball
  if (air_ball) {
    return ((Ball_shadow.high_correlation + Ball_shadow.low_correlation) <= 3.0) ? 1 : 0;
    }

  // not an air ball
  // (it must be a high ball or low ball)
  // the object is at least 4 emitters, we are more forgiving and assume ball
  return 1;
}

//------------------------------------------------------------------------------
void Compute_correlation(void) {
  Ball_shadow.high_correlation = Correlation(Ball_shadow.Measured_shadow_high, Ball_shadow.Expected_shadow_high, Ball_shadow.Emitter_Vector_Log_Index + 1);
  Ball_shadow.low_correlation  = Correlation(Ball_shadow.Measured_shadow_low,  Ball_shadow.Expected_shadow_low,  Ball_shadow.Emitter_Vector_Log_Index + 1);
}

//------------------------------------------------------------------------------
// construct the high and low shadow from the vector log
void Construct_measured_shadow(void) {
  int16_t  i;
  int16_t  index       = Ball_shadow.Emitter_Vector_Log_Index;
  float   *shadow_high = Ball_shadow.Measured_shadow_high;
  float   *shadow_low  = Ball_shadow.Measured_shadow_low;
  float    endpoint;
  int16_t  flight_time_steps;

  // Find_zero_bit_test();

  // vector bit 31 corresponds to the top of the goal
  // vector bit  0 corresponds to the bottom of the goal
  for (i = 1; i < index; i++) {
    uint32_t vector = Ball_shadow.Emitter_Vector_Log[i];
    shadow_high[i]  = Find_Most_Significant_zero_bit (vector);
    shadow_low [i]  = Find_Least_Significant_zero_bit(vector);
    }

  // endpoints are average of adjacent low[] and high[]
  endpoint = (shadow_high[1] + shadow_low[1]) * 0.5;
  shadow_high[0] = endpoint;
  shadow_low [0] = endpoint;
  Ball_shadow.y0 = endpoint;

  endpoint = (shadow_high[index-1] + shadow_low[index-1]) * 0.5;
  shadow_high[index] = endpoint;
  shadow_low [index] = endpoint;
  Ball_shadow.y_end  = endpoint;

  // first and last vector contain all ones
  // for example, suppose Emitter_Vector_Log_Index was 4,
  // then we have the following:
  // [0] 0xffffffff
  // [1] vector containing contiguous group of zeros
  // [2] vector containing contiguous group of zeros
  // [3] vector containing contiguous group of zeros
  // [4] 0xffffffff
  // flight time is 3 (must be at least 1)
  // so (index-1) must be > 0
  // or index must be > 1
  flight_time_steps = index > 1                         // assure non-zero flight time
                    ? index - 1
                    : 1;

  // convert flight time in number of steps to flight time in ms
  // scan period is the time in microseconds for one step
  // (flight_time ms) = (flight_time steps) * (scan period us/step) * (1 ms/1000 us)
  #define scan_period_us_per_step (float) 1000.0        // this is the light-curtain scan time in microseconds
  Ball_shadow.flight_time_ms = (float) flight_time_steps * scan_period_us_per_step / 1000.0;
}

//------------------------------------------------------------------------------
// return true if the ball is a roller
// roller is defined as: at least some vectors are 0xfffffff0
// no zero bits in upper 28 bits, zero bits in lower 4 bits
bool Roller_ball(void) {
  int16_t i;

  for (i = 1; i < Ball_shadow.Emitter_Vector_Log_Index; i++) {
    if (Ball_shadow.Emitter_Vector_Log[i] == 0xfffffff0ul) return true;
    }

  return false;
}

#if 0
bool Roller_ball(void) {
  int16_t i;

  for (i = 0; i < Ball_shadow.Emitter_Vector_Log_Index; i++) {
    if ((Ball_shadow.Emitter_Vector_Log[i] & 0xfffffff0ul) != 0xfffffff0ul) return false;
    }

  return true;
}
#endif

#if 0
bool Roller_ball(void) {
  int16_t i;
  int16_t shadow_high = 0;

  for (i = 1; i < Ball_shadow.Emitter_Vector_Log_Index; i++) {
    int16_t vector_high = Find_Most_Significant_zero_bit (Ball_shadow.Emitter_Vector_Log[i]);
    if (shadow_high < vector_high)
        shadow_high = vector_high;
    }

  return (shadow_high <= 3);
}
#endif

//------------------------------------------------------------------------------
// no gravity for rollers
// the aperture physically allows 3.75 inches of ball below the bottom emitter
// the ball diameter is 7 inches, so the ball track is y = -0.25 inches
// the missing data interferes with doing a good job with ball/non-ball detection
// all data is clamped to a minimum of 0 inches
#if 0
// collected roller ball profile
// index        y value
//   0          light curtain closed
//   1          0
//   2          1
//  39          2
//  43          3
// 161          2
// 165          1
// 180          0
// 181          light curtain closed
//
// the trick is to scale this profile to match the collected data
void Compute_expected_shadow_roller(void) {
  int16_t t;                            // time in milliseconds
  float   scale        = (float) Ball_shadow.Emitter_Vector_Log_Index / 180.0f;
  int16_t profile_x[7] = {
    //           x value                    y value
    //        <= 0                          light curtain closed
    (int16_t) (  1.0f * scale),         //  profile_x[0], y = 0
    (int16_t) (  2.0f * scale),         //  profile_x[1], y = 1
    (int16_t) ( 39.0f * scale),         //  profile_x[2], y = 2
    (int16_t) ( 43.0f * scale),         //  profile_x[3], y = 3
    (int16_t) (161.0f * scale),         //  profile_x[4], y = 2
    (int16_t) (165.0f * scale),         //  profile_x[5], y = 1
    (int16_t) (180.0f * scale)          //  profile_x[6], y = 0
    //      >= 181                          light curtain closed
    };

  Ball_shadow.v0_inch_per_sec = 0;      // no gravity

  for (t = 0; t <= Ball_shadow.Emitter_Vector_Log_Index; t++) {
    int16_t y;
    if      (t >= profile_x[6]) y = 0;
    else if (t >= profile_x[5]) y = 1;
    else if (t >= profile_x[4]) y = 2;
    else if (t >= profile_x[3]) y = 3;
    else if (t >= profile_x[2]) y = 2;
    else if (t >= profile_x[1]) y = 1;
    else                        y = 0;

#if 0
    // for rollers, clamp measured high shadow to no more than 3
    // to match the maximum profile value above
    if (Ball_shadow.Measured_shadow_high[t] > 3.0)
        Ball_shadow.Measured_shadow_high[t] = 3.0;
#endif

    Ball_shadow.Expected_shadow_high[t] = y;
    Ball_shadow.Expected_ball_track [t] = 0;
    Ball_shadow.Expected_shadow_low [t] = 0;
    }

  // compute ball speed in mph
  // (7 inches / flight time ms) * (1000 ms / sec) * (3600 sec / hour) * (1 foot / 12 inches) * (1 mile / 5280 feet)
  Ball_shadow.ball_speed_mph = Ball_shadow.ball_diameter_inch * 1000.0 * 3600.0 / Ball_shadow.flight_time_ms / 12.0 / 5280.0;
}
#else
void Compute_expected_shadow_roller(void) {
  int16_t t;                                    // time in milliseconds

  Ball_shadow.v0_inch_per_sec = 0;              // no gravity

  for (t = 0; t <= Ball_shadow.Emitter_Vector_Log_Index; t++) {
    float xt         = Ball_shadow.ball_diameter_inch * ((float) t / Ball_shadow.Emitter_Vector_Log_Index - 0.50);      // x value for circle computation: -3.0 <= x <= +3.0
    float radical    = (float) Ball_shadow.ball_diameter_inch * Ball_shadow.ball_diameter_inch * 0.25 - xt * xt;        // the radical under the sqrt is separate so we can test for <= 0
    float ycircle    = radical > 0.0 ? sqrt(radical) : 0.0;                                                             // positive y value of circle = sqrt(r^2 - x^2)
    float ball_track = -0.25;                                                                                           // the ball track
    float temp       = ball_track;           Ball_shadow.Expected_ball_track [t] = (temp < 0.0) ? 0.0 : temp;
          temp       = ball_track + ycircle; Ball_shadow.Expected_shadow_high[t] = (temp < 0.0) ? 0.0 : temp;           // ball track + ycircle is expected top    line of ball shadow
          temp       = ball_track - ycircle; Ball_shadow.Expected_shadow_low [t] = (temp < 0.0) ? 0.0 : temp;           // ball track - ycircle is expected bottom line of ball shadow
    }

  // compute ball speed in mph
  // (7 inches / flight time ms) * (1000 ms / sec) * (3600 sec / hour) * (1 foot / 12 inches) * (1 mile / 5280 feet)
  Ball_shadow.ball_speed_mph = Ball_shadow.ball_diameter_inch * 1000.0 * 3600.0 / Ball_shadow.flight_time_ms / 12.0 / 5280.0;
}
#endif

//------------------------------------------------------------------------------
// important! collect measured shadow first
// because it computes values we use here (y0, y_end)
// the aperture physically allows 3.5 inches of ball above the top emitter
// the ball diameter is 7 inches, so we can miss the top half of the ball
// the missing data interferes with doing a good job with ball/non-ball detection
// all data is clamped to a maximum of 31 inches
void Compute_expected_shadow_air(void) {
  int16_t t;                                    // time in milliseconds
  float   k1;                                   // k1 = v0 / 1000
  float   k2;                                   // k2 = 1/2 * a / 1000 / 1000

  // compute initial vertical velocity v0:
  //  y(t) = y0 + v0*t + 1/2*a*t^2
  // (y(t) - y0) = v0*t + 1/2*a*t^2
  // (y(t) - y0) - 1/2*a*t^2 = v0*t
  // (y(t) - y0)/t - 1/2*a*t = v0
  //
  // convert log time in ms to sec:
  // (t sec) = (index ms) * (1 sec/1000 ms)
  float t_sec = Ball_shadow.Emitter_Vector_Log_Index / 1000.0;
  Ball_shadow.v0_inch_per_sec = (Ball_shadow.y_end - Ball_shadow.y0) / t_sec
                              - 0.50 * Ball_shadow.gravity * t_sec;

  // compute vertical ball track as a function of time:
  // the index represents time in milliseconds
  // y(t) = y0 + v0*t + 1/2*a*t^2
  // t = i/1000
  // let k1 = v0 / 1000
  // let k2 = 1/2 * gravity / 1000 / 1000
  // use horner's rule:
  // y(t) = (k2*t + k1)*t + y0
  k1 = Ball_shadow.v0_inch_per_sec / 1000.0;
  k2 = 0.50 * Ball_shadow.gravity / 1000.0 / 1000.0;
  for (t = 0; t <= Ball_shadow.Emitter_Vector_Log_Index; t++) {
    float xt         = Ball_shadow.ball_diameter_inch * ((float) t / Ball_shadow.Emitter_Vector_Log_Index - 0.50);      // x value for circle computation: -3.5 <= x <= +3.5
    float radical    = (float) Ball_shadow.ball_diameter_inch * Ball_shadow.ball_diameter_inch * 0.25 - xt * xt;        // the radical under the sqrt is separate so we can test for <= 0
    float ycircle    = radical > 0.0 ? sqrt(radical) : 0.0;                                                             // positive y value of circle = sqrt(r^2 - x^2)
    float ball_track = (k2*t + k1)*t + Ball_shadow.y0;                                                                  // the ball track including the effects of gravity
    float temp       = ball_track;           Ball_shadow.Expected_ball_track [t] = (temp > 31.0) ? 31.0 : temp;
          temp       = ball_track + ycircle; Ball_shadow.Expected_shadow_high[t] = (temp > 31.0) ? 31.0 : temp;         // ball track + ycircle is expected top    line of ball shadow
          temp       = ball_track - ycircle; Ball_shadow.Expected_shadow_low [t] = (temp > 31.0) ? 31.0 : temp;         // ball track - ycircle is expected bottom line of ball shadow
    }

  // compute ball speed in mph
  // (7 inches / flight time ms) * (1000 ms / sec) * (3600 sec / hour) * (1 foot / 12 inches) * (1 mile / 5280 feet)
  Ball_shadow.ball_speed_mph = Ball_shadow.ball_diameter_inch * 1000.0 * 3600.0 / Ball_shadow.flight_time_ms / 12.0 / 5280.0;
}

//------------------------------------------------------------------------------
// construct a vector representing the light curtain entry, exit, and all points in between
// vector bit 31 corresponds to the top of the goal
// vector bit  0 corresponds to the bottom of the goal
uint32_t Compute_variable_length_smear(void) {
  uint32_t vector = 0;

  // find top and bottom range for entering light curtain
  int16_t v0_high = Find_Most_Significant_zero_bit (Ball_shadow.Emitter_Vector_Log[1]);
  int16_t v0_low  = Find_Least_Significant_zero_bit(Ball_shadow.Emitter_Vector_Log[1]);

  // find top and bottom range for exiting light curtain
  int16_t index   = Ball_shadow.Emitter_Vector_Log_Index;
  int16_t v1_high = Find_Most_Significant_zero_bit (Ball_shadow.Emitter_Vector_Log[index-1]);
  int16_t v1_low  = Find_Least_Significant_zero_bit(Ball_shadow.Emitter_Vector_Log[index-1]);

  int16_t j;          // highest index (closest to top    of goal)
  int16_t i;          // lowest  index (closest to bottom of goal)
    
  // find lowest and highest index
  j = v0_high; if (j < v1_high) j = v1_high;
  i = v0_low;  if (i > v1_low)  i = v1_low;

  // fill the vector between i and j inclusive
  for (;i <= j; i++) vector |= (1ul << i);

  return vector;
}

//------------------------------------------------------------------------------
// construct a length 5 smear centered at the point of light curtain entry
// don't allow the smear to fall off the top or bottom
uint32_t Compute_fixed_length_smear(void) {
  // find top and bottom range for entering light curtain
  int16_t v0_high = Find_Most_Significant_zero_bit (Ball_shadow.Emitter_Vector_Log[1]);
  int16_t v0_low  = Find_Least_Significant_zero_bit(Ball_shadow.Emitter_Vector_Log[1]);
  int16_t v0_avg  = (v0_high + v0_low) >> 1;

  // range check
  if (v0_avg <  2) v0_avg =  2;
  if (v0_avg > 29) v0_avg = 29;

  return (uint32_t) 0x1f << (v0_avg - 2);
}

//------------------------------------------------------------------------------
// layer 1 LEDs indicate the active zone (one-bits)
// invert and search for top and bottom zero bits
// convert zone endpoints to percent
// check elevation of ball is within zone
bool Compute_elevation_within_zone(int16_t elevation_pct) {
  uint32_t vector   = RGB_indicators.LEDs1_on;          // convert one-bits to zero-bits so we can search
  int16_t  high_bit = 31;                               // assume zone ms bit is a one-bit
  int16_t  low_bit  =  0;                               // assume zone ls bit is a zero-bit
  int16_t pct_high;
  int16_t pct_low;
  uint32_t mask;

  // if nothing is lit, return "not in zone"
  if (vector == 0ul) return false;

  // find zone high bit
  mask = 0x80000000ul;
  while ((vector & mask) == 0) {
    mask >>= 1;
    high_bit--;
    }

  // find zone low bit
  mask = 1ul;
  while ((vector & mask) == 0) {
    mask <<= 1;
    low_bit++;
    }

  pct_high = (high_bit * 200 + 31)/62;                  // convert zone to percent, see Compute_elevation_pct()
  pct_low  = (low_bit  * 200 + 31)/62;

  // from Steven Eannarino 8/25/20
  // "Confirmed with Bruviti that when a user is running an exercise with the Target half-lit,
  // if a Target reports a ball crossing at 65% elevation or lower,
  // the app will count it as a score.
  // If we can build the firmware to match that,
  // that would align the "score" indication of the target (white blink)
  // to the "score indication" of the app (scoreboard shows a goal is scored)."
  //
  // fully lit target: 31..0    (31 * 200 + 31)/62 = 100%
  //                            ( 0 * 200 + 31)/62 =   0%
  //
  // half lit target: 15..0     (15 * 200 + 31)/62 = 48%
  //                            ( 0 * 200 + 31)/62 =  0%
  //
  // for half lit target, we expect to accept 65% or lower,
  // 48% + 17% = 65%
  //
  // this also works for fully lit target
  return (elevation_pct <= (pct_high + 17)) && (elevation_pct >= pct_low);
}

//------------------------------------------------------------------------------
void Log_Emitter_Vector(uint32_t vector) {
  static int16_t holdoff_count = 0;
         int16_t i;
        uint16_t object_type;                                           // 0=non-ball 1=ball
        uint16_t elevation_pct;                                         // leftmost  bit is top of goal (emitter=31, 100%),  rightmost bit is bottom of goal (emitter= 0, 0%)
            bool elevation_within_zone;                                 // ball elevation lies within the active target zone (plus 2 emitters of tolerance)
        uint32_t fixed_length_smear;                                    // fixed-length smear vector
//      uint32_t variable_length_smear;                                 // variable-length smear vector

  switch (Ball_shadow.Emitter_Vector_State) {
    case -2: // init with 1 second holdoff
             if (POST_state < 2) break;
             Ball_shadow.Emitter_Vector_State++;
    case -1: // capture complete, idle
             if (vector != ~0ul)            Ball_shadow.Emitter_Vector_State--;
             else if (--holdoff_count <= 0) Ball_shadow.Emitter_Vector_State++;
             break;
    case  0: // init for logging
             // force logging one entry of all ones
             Send_Ball_Crossing_Event_Holdoff_complete();
             Ball_shadow.Emitter_Vector_Log_Index                                   =  0;
             Ball_shadow.Emitter_Vector_Log[Ball_shadow.Emitter_Vector_Log_Index++] = ~0ul;
             Ball_shadow.Emitter_Vector_State++;
    case  1: // wait until gap has opened up
             if (vector == ~0ul) break;
             // a gap has opened up in the vector, start logging
             Ball_shadow.Emitter_Vector_State++;
    case  2: // check to see if the gap has closed
             if ((vector != ~0ul) && (Ball_shadow.Emitter_Vector_Log_Index < (EMITTER_VECTOR_LOG_SIZE-1))) {
               // the gap is still open,
               // and the log is not full,
               // log the data
               Ball_shadow.Emitter_Vector_Log[Ball_shadow.Emitter_Vector_Log_Index++] = vector;
               }
             else {
               // the gap is closed
               // or the log is full
               // fill out the log with all ones
               for (i = Ball_shadow.Emitter_Vector_Log_Index; i < EMITTER_VECTOR_LOG_SIZE; i++) {
                 Ball_shadow.Emitter_Vector_Log[i] = ~0ul;
                 }

               // find measured shadow,
               // then compute expected shadow,
               // then compute correlation,
               // order is important!
               Construct_measured_shadow();                                     // construct the high and low shadow from the vector log
               #if 0
               if (Roller_ball()) Compute_expected_shadow_roller();
               else               Compute_expected_shadow_air();
               #else
               Compute_expected_shadow_air();
               #endif

               // determine "ball" or "non-ball"
               object_type = Compute_object_type();                             // Compute_correlation();

               // compute elevation percent
               elevation_pct = Compute_elevation_pct();

               // compute elevation within target zone (where the LEDs are lit to indicate "shoot here")
               elevation_within_zone = Compute_elevation_within_zone(elevation_pct);

               // compute variable-length and fixed-length smear
               fixed_length_smear    = Compute_fixed_length_smear();            // used for ball objects
             //variable_length_smear = Compute_variable_length_smear();         // used for non-ball objects

               // send ball crossing event to mobile app
               Send_Ball_Crossing_Event(object_type,                            // object-type is "ball" or "non-ball"
                                        elevation_pct,
                                        (uint16_t) Ball_shadow.ball_speed_mph);

               // done logging, fill ball shadow packet and send it to PC
               for (i = 0; i <= Ball_shadow.Emitter_Vector_Log_Index; i++) {
                 BallShadowMsg.Measured_shadow_high[i] = Ball_shadow.Measured_shadow_high[i];
                 BallShadowMsg.Measured_shadow_low [i] = Ball_shadow.Measured_shadow_low [i];
                 BallShadowMsg.Expected_ball_track [i] = Ball_shadow.Expected_ball_track [i];
                 BallShadowMsg.Expected_shadow_high[i] = Ball_shadow.Expected_shadow_high[i];
                 BallShadowMsg.Expected_shadow_low [i] = Ball_shadow.Expected_shadow_low [i];
                 }

               BallShadowMsg.v0_inch_per_sec  = Ball_shadow.v0_inch_per_sec;
               BallShadowMsg.ball_speed_mph   = Ball_shadow.ball_speed_mph;
               BallShadowMsg.high_correlation = Ball_shadow.high_correlation;
               BallShadowMsg.low_correlation  = Ball_shadow.low_correlation;
               BallShadowMsg.num_entries      = Ball_shadow.Emitter_Vector_Log_Index+1;

               // send ball shadow message to PC
               Send_BallShadow_task(1);

               // show ball crossing on LEDs
               if (LightShow_Selector < 0) {
                 // app is controlling the light show
                 // setup layer 2 LEDs task
                 if (object_type) LEDs_layer2_task(true, object_type, elevation_within_zone, fixed_length_smear);
                 else             LEDs_layer2_task(true, object_type, elevation_within_zone, fixed_length_smear);
               //else             LEDs_layer2_task(true, object_type, elevation_within_zone, variable_length_smear);
                 RGB_indicators.ball_crossing_event = 1;
                 }
               else {
                 // app is not controlling the light show
                 // show fixed-length smear for valid ball crossing
                 // else show variable-length smear
                 if (object_type) Send_Start_Light_Show(e_LightShow_Ball_Crossing,     fixed_length_smear);
                 else             Send_Start_Light_Show(e_LightShow_Non_Ball_Crossing, fixed_length_smear);
               //else             Send_Start_Light_Show(e_LightShow_Non_Ball_Crossing, variable_length_smear);
                 }

               // start holdoff before next ball crossing
               if (object_type) holdoff_count = 2000;                           // ball: holdoff is 2 second
               else             holdoff_count = 0;                              // non-ball: no holdoff
               Ball_shadow.Emitter_Vector_State = -2;
               }
             break;
    }
}

#if 0
//------------------------------------------------------------------------------
#define QUICK_TOSS_STRAIGHT_THROUGH_SIZE        31
extern const uint32_t quick_toss_straight_through[QUICK_TOSS_STRAIGHT_THROUGH_SIZE];
void Log_Emitter_Vector_Test(uint32_t vector) {
  int16_t i;

  if (Ball_shadow.Emitter_Vector_State == 0) {
    for (i = 0; i < QUICK_TOSS_STRAIGHT_THROUGH_SIZE; i++) {
      Log_Emitter_Vector(quick_toss_straight_through[i]);
      }
    }
}

const uint32_t quick_toss_straight_through[QUICK_TOSS_STRAIGHT_THROUGH_SIZE] = {
  0xffffffff,           // 1111 1111 1111 1111 1111111111111111    ffff
  0xff7fffff,           // 1111 1111 0111 1111 1111111111111111    ff7f
  0xff3fffff,           // 1111 1111 0011 1111 1111111111111111    ff3f
  0xff1fffff,           // 1111 1111 0001 1111 1111111111111111    ff1f
  0xfe1fffff,           // 1111 1110 0001 1111 1111111111111111    fe1f
  0xfe0fffff,           // 1111 1110 0000 1111 1111111111111111    fe0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc07ffff,           // 1111 1100 0000 0111 1111111111111111    fc07
  0xfc07ffff,           // 1111 1100 0000 0111 1111111111111111    fc07
  0xfc07ffff,           // 1111 1100 0000 0111 1111111111111111    fc07
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfc0fffff,           // 1111 1100 0000 1111 1111111111111111    fc0f
  0xfe0fffff,           // 1111 1110 0000 1111 1111111111111111    fe0f
  0xfe0fffff,           // 1111 1110 0000 1111 1111111111111111    fe0f
  0xff1fffff,           // 1111 1111 0001 1111 1111111111111111    ff1f
  0xff3fffff,           // 1111 1111 0011 1111 1111111111111111    ff3f
  0xffffffff            // 1111 1111 1111 1111 1111111111111111    ffff
};
#endif
