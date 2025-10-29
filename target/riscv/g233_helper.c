#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "accel/tcg/cpu-ldst.h"

void HELPER(dma)(CPURISCVState *env,
    target_ulong da,  target_ulong sa,
    target_ulong sz)
{
    int m = 0;
    int n = 0;
    if (sz == 0) {
        m = n = 8;
    } else if (sz == 1) {
        m = n = 16;
    } else {
        m = n = 32;
    }
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            target_ulong dst = da +  4 * ((i * m) + j);
            target_ulong src = sa + 4 * ((j * n) + i);
            uint32_t data = cpu_ldl_data(env, src);
            cpu_stl_data(env, dst, data);
        }
    }
}

void HELPER(sort)(CPURISCVState *env,
    target_ulong sa, target_ulong sz,
    target_ulong da)
{

}

void HELPER(crush)(CPURISCVState *env,
    target_ulong sa, target_ulong sz,
    target_ulong da)
{

}

void HELPER(expand)(CPURISCVState *env,
    target_ulong sa, target_ulong sz,
    target_ulong da)
{

}
