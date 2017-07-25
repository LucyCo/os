/*
 * Interval-timer demo program.
 * Hebrew University OS course.
 * Questions: os@cs.huji.ac.il
 */

#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

int gotit = 0;

void timer_handler(int sig)
{
  gotit = 1;
  printf("Timer expired\n");
}

int main(void)
{
  signal(SIGVTALRM, timer_handler);

  struct itimerval tv;
  tv.it_value.tv_sec = 2;  /* first time interval, seconds part */
  tv.it_value.tv_usec = 0; /* first time interval, microseconds part */
  tv.it_interval.tv_sec = 2;  /* following time intervals, seconds part */
  tv.it_interval.tv_usec = 0; /* following time intervals, microseconds part */

  setitimer(ITIMER_VIRTUAL, &tv, NULL);
  for(;;) {
    if (gotit) {
      printf("Got it!\n");
      gotit = 0;
    }
  }
  return 0;
}

