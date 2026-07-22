#ifndef DJTERM_H
#define DJTERM_H

int djterm_init(void);
void djterm_done(void);
void djterm_enable(void);
void djterm_disable(void);
void djterm_hook_int21(void);

#endif
