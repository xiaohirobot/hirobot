#ifndef ROBOT_WIRE_H
#define ROBOT_WIRE_H
#include <stdint.h>
#include <string.h>
#define RW_SIZE 32
#define RW_SUBMIT 1
#define RW_QUERY 0
#define RW_REPLY 128
#define RW_OK 0
#define RW_INVALID 1
#define RW_REMOTE 2
#define RW_SERVO 3
#define RW_BUSY 4
static inline uint32_t rw_get(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static inline void rw_put(uint8_t *p, uint32_t n) {
    for (unsigned i=0;i<4;i++) p[i]=(uint8_t)(n>>(8*i));
}
static inline uint8_t rw_crc(const uint8_t *p) {
    uint8_t c=0;
    for(unsigned i=0;i<31;i++) { c^=p[i]; for(unsigned b=0;b<8;b++) c=(uint8_t)((c<<1)^((c&128)?7:0)); }
    return c;
}
static inline void rw_finish(uint8_t *p) { p[31]=rw_crc(p); }
static inline int rw_valid(const uint8_t *p) { return p[0]==0xA5 && p[1]==0x5A && p[2]==1 && p[31]==rw_crc(p); }
static inline void rw_make(uint8_t *p,uint8_t op,uint32_t id,uint8_t mode,int32_t a,int32_t b) {
    memset(p,0,RW_SIZE); p[0]=0xA5;p[1]=0x5A;p[2]=1;p[3]=op;
    rw_put(p+4,id);p[9]=mode;rw_put(p+10,(uint32_t)a);rw_put(p+14,(uint32_t)b);rw_finish(p);
}
#endif
