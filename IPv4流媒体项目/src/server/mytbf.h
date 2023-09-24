#ifndef MYTBF_H__
#define MYTBF_H__

#define MAXTBF_NUM 1024//令牌桶数量

typedef void  mytbf_t;//即mytbf_t与void等价

mytbf_t* mytbf_init(int cps,int burst);

int mytbf_fetchtoken(mytbf_t *,int);

int mytbf_returntoken(mytbf_t *,int);

int mytbf_destory(mytbf_t *);


#endif