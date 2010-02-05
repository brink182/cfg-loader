#ifndef _GRID_H_
#define _GRID_H_

#include "wpad.h"


/*------prototypes-----*/
void grid_init(int);
void grid_transition(int, int);
int grid_page_change(int);
void grid_transit_style(int);
void grid_get_scroll(ir_t *ir);
void grid_calc(void);
int grid_update_all(ir_t *ir);
void grid_draw(int);
void grid_print_title(int);

#endif
