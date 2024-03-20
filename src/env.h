#ifndef ENV_H
#define ENV_H

/* define to sync RM/PM env data - consumes more memory */
#define SYNC_ENV 0

void set_env_seg(void);
void set_env_sel(void);
void set_env_size(void);
void get_env(void);
void put_env(void);
#if !SYNC_ENV
void set_env(const char *variable, const char *value);
void sync_env(void);
#endif
int realloc_env(unsigned new_size);
int get_env_size(void);

#endif
