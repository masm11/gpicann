/*    gpicann - Screenshot Annotation Tool
 *    Copyright (C) 2020 Yuuki Harano
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
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
