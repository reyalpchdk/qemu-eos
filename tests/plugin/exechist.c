/*
 * Copyright (C) 2023 reyalp at gmail dot com
 *
 * License: GNU GPL, version 2.
 *   See the COPYING file in the top-level directory.
 */
/*
store circular buffer of addresses most recently executed
assumes single core guest CPU
*/
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

#define PC_HIST_COUNT 32 // must be power of 2
#define PC_HIST_MASK (PC_HIST_COUNT - 1)
#define PC_HIST(x) ((x)&PC_HIST_MASK)

static uint64_t pc_hist[PC_HIST_COUNT];
static int pc_hist_i;

static void print_hist(void)
{
    int i = pc_hist_i;
    int j = PC_HIST_COUNT;
    printf("PC history\n");
    do {
        i = PC_HIST(i+1); // start from oldest
        printf("%02d: 0x%08lx\n",j,pc_hist[i]);
        j--;
    } while (i != pc_hist_i);
}
static void vcpu_insn_exec_before(unsigned int cpu_index, void *udata)
{
    uint64_t vaddr = (uint64_t)udata;
    pc_hist_i = PC_HIST(pc_hist_i+1);
    pc_hist[pc_hist_i] = vaddr;
    // addresses of interest
    if(vaddr == 0 || vaddr == 0xffc079e0) {
        print_hist();
    }
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb);
    size_t i;

    for (i = 0; i < n; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t vaddr = qemu_plugin_insn_vaddr(insn);
        // naughty uint64 to pointer cast because:
        // can't pass vaddr by reference because callback is called after this function returns
        // can't store history here because not in execution order
        qemu_plugin_register_vcpu_insn_exec_cb(
            insn, vcpu_insn_exec_before, QEMU_PLUGIN_CB_NO_REGS, (void *)vaddr);
    }
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    return 0;
}
