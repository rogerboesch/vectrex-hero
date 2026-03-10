#ifndef __E6809_H
#define __E6809_H

/* user defined read and write functions */

extern unsigned char (*e6809_read8) (unsigned address);
extern void (*e6809_write8) (unsigned address, unsigned char data);

/* cpu registers */

extern unsigned reg_x, reg_y, reg_u, reg_s, reg_pc;
extern unsigned reg_a, reg_b, reg_dp, reg_cc;

void e6809_reset (void);
unsigned e6809_sstep (unsigned irq_i, unsigned irq_f);

#endif
