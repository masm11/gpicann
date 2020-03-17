#include <stdio.h>
#include <math.h>

#include "tcos.h"

int main(void)
{
    for (int i = 0; i < TCOS_NR; i++)
	printf("%f, // %d/%d\n", sin(2 * M_PI / TCOS_NR * i), i, TCOS_NR);
    
    return 0;
}
