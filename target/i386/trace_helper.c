#include <stdint.h>

#include "cpu.h"
#include "helper.h"
#include "tracewrap.h"
#include "qemu/log.h"


static const char* const regs[CPU_NB_REGS] = {
#ifdef TARGET_X86_64
    [R_EAX] = "RAX",
    [R_EBX] = "RBX",
    [R_ECX] = "RCX",
    [R_EDX] = "RDX",
    [R_ESI] = "RSI",
    [R_EDI] = "RDI",
    [R_EBP] = "RBP",
    [R_ESP] = "RSP",
    [8]  = "R8",
    [9]  = "R9",
    [10] = "R10",
    [11] = "R11",
    [12] = "R12",
    [13] = "R13",
    [14] = "R14",
    [15] = "R15",
#else
    [R_EAX] = "EAX",
    [R_EBX] = "EBX",
    [R_ECX] = "ECX",
    [R_EDX] = "EDX",
    [R_ESI] = "ESI",
    [R_EDI] = "EDI",
    [R_EBP] = "EBP",
    [R_ESP] = "ESP",
#endif
};

#define CPU_NB_SEGS 6
static const char* const segs[CPU_NB_SEGS] = {
    [R_ES] = "ES_BASE",
    [R_CS] = "CS_BASE",
    [R_SS] = "SS_BASE",
    [R_DS] = "DS_BASE",
    [R_FS] = "FS_BASE",
    [R_GS] = "GS_BASE"
};

void HELPER(trace_newframe)(target_ulong pc)
{
    qemu_trace_newframe(pc, 0);
}

void HELPER(trace_endframe)(CPUArchState *env, target_ulong old_pc, target_ulong size)
{
    //qemu_trace_endframe(env, env->eip - size, size);
    qemu_trace_endframe(env, old_pc, size);
}

OperandInfo * load_store_reg(target_ulong reg, target_ulong val, int ls) {
    RegOperand *ro = g_new(RegOperand,1);
    reg_operand__init(ro);
    int isSeg = reg & (1 << SEG_BIT);
    reg &= ~(1 << SEG_BIT);

    const char* reg_name = NULL;
    if (isSeg) {
        reg_name = reg < CPU_NB_SEGS ? segs[reg] : "<UNDEF>";
    } else {
        reg_name = reg < CPU_NB_REGS ? regs[reg] :
#ifdef TARGET_X86_64
        "RFLAGS";
#else
        "EFLAGS";
#endif
    }
    ro->name = g_strdup(reg_name);

    OperandInfoSpecific *ois = g_new(OperandInfoSpecific, 1);
    operand_info_specific__init(ois);
    ois->reg_operand = ro;

    OperandUsage *ou = g_new(OperandUsage,1);
    operand_usage__init(ou);
    if (ls == 0) {
        ou->read = 1;
    } else {
        ou->written = 1;
    }
    OperandInfo *oi = g_new(OperandInfo,1);
    operand_info__init(oi);
    oi->bit_length = 0;
    oi->operand_info_specific = ois;
    oi->operand_usage = ou;
    oi->value.len = sizeof(val);
    oi->value.data = g_malloc(oi->value.len);
    memcpy(oi->value.data, &val, sizeof(val));
    return oi;
}

void HELPER(trace_load_reg)(target_ulong reg, target_ulong val)
{
    qemu_log("This register (r" TARGET_FMT_ld ") was read. Value 0x" TARGET_FMT_lx "\n", reg, val);

    OperandInfo *oi = load_store_reg(reg, val, 0);

    qemu_trace_add_operand(oi, 0x1);
}

void HELPER(trace_store_reg)(target_ulong reg, target_ulong val)
{
    qemu_log("This register (r" TARGET_FMT_ld ") was written. Value: 0x" TARGET_FMT_lx "\n", reg, val);

    OperandInfo *oi = load_store_reg(reg, val, 1);

    qemu_trace_add_operand(oi, 0x2);
}

void HELPER(trace_load_eflags)(CPUArchState *env)
{
    uint32_t val = cpu_compute_eflags(env);

    OperandInfo *oi = load_store_reg(REG_EFLAGS, val, 0);

    //OperandInfo *oi = load_store_reg(REG_EFLAGS, cpu_compute_eflags(env), 0);

    qemu_trace_add_operand(oi, 0x1);
}

void HELPER(trace_store_eflags)(CPUArchState *env)
{
    uint32_t val = cpu_compute_eflags(env);

    OperandInfo *oi = load_store_reg(REG_EFLAGS, val, 1);

    //OperandInfo *oi = load_store_reg(REG_EFLAGS, cpu_compute_eflags(env), 1);

    qemu_trace_add_operand(oi, 0x2);
}

OperandInfo * load_store_mem(target_ulong addr, target_ulong val, int ls, int len)
{
    MemOperand * mo = g_new(MemOperand, 1);
    mem_operand__init(mo);

    mo->address = addr;

    OperandInfoSpecific *ois = g_new(OperandInfoSpecific, 1);
    operand_info_specific__init(ois);
    ois->mem_operand = mo;

    OperandUsage *ou = g_new(OperandUsage, 1);
    operand_usage__init(ou);
    if (ls == 0) {
        ou->read = 1;
    } else {
        ou->written = 1;
    }
    OperandInfo *oi = g_new(OperandInfo, 1);
    operand_info__init(oi);
    oi->bit_length = len*8;
    oi->operand_info_specific = ois;
    oi->operand_usage = ou;
    oi->value.len = len;
    oi->value.data = g_malloc(oi->value.len);
    memcpy(oi->value.data, &val, len);
    return oi;
}

void HELPER(trace_ld)(CPUArchState *env, target_ulong val, target_ulong addr)
{
    qemu_log("This was a read 0x" TARGET_FMT_lx " addr:0x" TARGET_FMT_lx " value:0x" TARGET_FMT_lx "\n", env->eip, addr, val);

    OperandInfo *oi = load_store_mem(addr, val, 0, sizeof(val));

    qemu_trace_add_operand(oi, 0x1);
}

void HELPER(trace_st)(CPUArchState *env, target_ulong val, target_ulong addr)
{
    qemu_log("This was a store 0x" TARGET_FMT_lx " addr:0x" TARGET_FMT_lx " value:0x" TARGET_FMT_lx "\n", env->eip, addr, val);

    OperandInfo *oi = load_store_mem(addr, val, 1, sizeof(val));

    qemu_trace_add_operand(oi, 0x2);
}
