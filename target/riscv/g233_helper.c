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
    target_ulong sa, target_ulong an,
    target_ulong sn)
{
    for (int i = 0; i < sn - 1; ++i) {
        int swapped = 0;
        for (int j = 0; j < sn -i -1; ++j) {
            target_ulong src = sa + j * 4;
            target_ulong dst = src + 4;
            int t1 = cpu_ldl_data(env, src);
            int t2 = cpu_ldl_data(env, dst);
            if (t1 > t2) {
                cpu_stl_data(env, src, t2);
                cpu_stl_data(env, dst, t1);
                swapped = 1;
            }
        }
        if (!swapped) {
            break;
        }
    }
}

void HELPER(crush)(CPURISCVState *env,
    target_ulong dst, target_ulong src,
    target_ulong num)
{
    for (int i = 0; i < num; i+=2) {
        u_int16_t data = cpu_ldsw_data(env, src + i);
        uint8_t cd = (uint8_t)((data & 0xf) | ((data >> 8)& 0xf) << 4);
        cpu_stb_data(env, dst + i/2, cd);
    }
}

void HELPER(expand)(CPURISCVState *env,
    target_ulong dst, target_ulong src,
    target_ulong num)
{
    for (int i = 0; i < num; i+=1) {
        uint8_t data = cpu_ldub_data(env, src + i);
        uint16_t cd = (uint16_t)(data & 0xf) | (uint16_t)(((data >> 4)& 0xf) << 8);
        cpu_stw_data(env, dst + i*2, cd);
    }
}
