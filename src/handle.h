#ifndef HANDLE_H__INCLUDED
#define HANDLE_H__INCLUDED

struct handle_t {
    double x, y, width, height;
    double cx, cy;
};

void handle_calc_geom(struct handle_t *handles, int nr);
void handle_draw(struct handle_t *handles, int nr, cairo_t *cr);

#endif	/* ifndef HANDLE_H__INCLUDED */
