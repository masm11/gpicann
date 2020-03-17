#include <stdio.h>
#include <math.h>

#include "tcos.h"

/* sin table for shadow */

const double tsin_table[TCOS_NR] = {
#include "tcos.inc"
};
