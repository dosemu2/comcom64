#ifndef DJANSI_H
#define DJANSI_H

int djansi_init(void);
void djansi_done(void);
void djansi_enable(void);
void djansi_disable(void);
void djansi_hook_int21(void);

#endif
