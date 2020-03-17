#ifndef TCOS_H__INCLUDED
#define TCOS_H__INCLUDED

#define TCOS_NR	16

extern const double tsin_table[TCOS_NR];

static inline double tsin(int idx)
{
    return tsin_table[idx & (TCOS_NR - 1)];
}

static inline double tcos(int idx)
{
    return tsin(idx + TCOS_NR / 4);
}

#endif	/* ifndef TCOS_H__INCLUDED */
