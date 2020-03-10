#ifndef STATE_MGMT_H__INCLUDED
#define STATE_MGMT_H__INCLUDED

enum {
    MODE_EDIT,
    MODE_RECT,
    MODE_ARROW,
    MODE_TEXT,
    MODE_MASK,
};

void mode_init(GtkWidget *widget);
void mode_handle(GdkEvent *ev);
void mode_switch(int new_mode);

#endif	/* ifndef STATE_MGMT_H__INCLUDED */
