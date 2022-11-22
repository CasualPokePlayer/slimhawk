#ifndef _MIN_MAX_H_
#define _MIN_MAX_H_

#define MIN(A, B) ({ __typeof__(A) __a = (A); __typeof__(B) __b = (B); __a < __b ? __a : __b; })
#define MAX(A, B) ({ __typeof__(A) __a = (A); __typeof__(B) __b = (B); __a < __b ? __b : __a; })

#endif
