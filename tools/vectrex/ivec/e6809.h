#ifndef __E6809_H
#define __E6809_H

/* user defined read and write functions */

extern unsigned char (*e6809_read8) (unsigned address);
extern void (*e6809_write8) (unsigned address, unsigned char data);

void e6809_reset (void);
unsigned e6809_sstep (unsigned irq_i, unsigned irq_f);
void e6809_get_regs(unsigned *pc, unsigned *a, unsigned *b,
                    unsigned *x, unsigned *y, unsigned *u,
                    unsigned *s, unsigned *dp, unsigned *cc);

#endif