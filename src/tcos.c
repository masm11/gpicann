#include <stdio.h>
#include <math.h>

#include "tcos.h"

/* cos/sin table for shadow */
double tcos[TCOS_NR];
double tsin[TCOS_NR];

void tcos_init(void)
{
    for (int i = 0; i < TCOS_NR; i++) {
	double theta = 2 * M_PI / TCOS_NR * i;
	tcos[i] = cos(theta);
	tsin[i] = sin(theta);
    }
}
