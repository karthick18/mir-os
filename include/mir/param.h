/*A.R.Karthick for MIR:
  Set up the different parameters:
  We increase the clock cycles per second
  to 100 or HZ: Earlier it was 20,but 100 or 10 milli secs.
  timeslice should be okay
*/
#ifdef __KERNEL__
#ifndef _PARAM_H
#define _PARAM_H

/*Increasing the value or decreasing the value
  will have an impact on the timer interrupt being
  triggered per sec. Now its 100 times per second,
  which implies that the scheduler would be invoked 
  a 100 times per second or on a 10 milli sec. basis
  which is usually the standard in most of the kernels.
*/

#define HZ 100 
/*This can vary it seems for AMD ELAN. We dont care about that for now:
 */
#define CLOCK_TICK_RATE  1193180
/*define the cycles per sec*/
#define LATCH    ( (CLOCK_TICK_RATE + HZ/2) / HZ )

#define MILLISECS_PER_HZ ( 1000 / HZ )

#endif
#endif
