#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define VM_MODE_U 0   
#define VM_MODE_S 1   
#define VM_MODE_M 3   

// Struct to keep VM registers (Sample; feel free to change.)
struct vm_reg {
    int     code;
    int     mode;
    uint64  val;
};

// Keep the virtual state of the VM's privileged registers
struct vm_virtual_state {
    // User trap setup
    struct vm_reg ustatus;
    struct vm_reg uie;
    struct vm_reg utvec;
    struct vm_reg uepc;


    // User trap handling
    struct vm_reg uscratch;
    struct vm_reg ucause;
    struct vm_reg utval;
    struct vm_reg uip;

    // Supervisor trap setup
    struct vm_reg sstatus;
    struct vm_reg sie;
    struct vm_reg stvec;
    struct vm_reg scounteren;

    // Supervisor page table register
    struct vm_reg satp;

    // Machine information registers
    struct vm_reg mvendorid;
    struct vm_reg marchid;
    struct vm_reg mimpid;
    struct vm_reg mhartid;
    struct vm_reg mconfigptr;

    // Machine trap setup registers
    struct vm_reg mstatus;
    struct vm_reg misa;
    struct vm_reg medeleg;
    struct vm_reg mideleg;
    struct vm_reg mie;
    struct vm_reg mtvec;
    struct vm_reg mcounteren;

    // Machine trap handling registers
    struct vm_reg mscratch;
    struct vm_reg mepc;
    struct vm_reg mcause;
    struct vm_reg mtval;
    struct vm_reg mip;
    struct vm_reg mtinst; 
    struct vm_reg mtval2;

    //Machine PMP
    struct vm_reg pmpaddr[64];
    struct vm_reg pmpcfg[8];

    uint64 current_exec_mode; 
};
struct vm_virtual_state *vmm;
// In your ECALL, add the following for prints
// struct proc* p = myproc();
// printf("(EC at %p)\n", p->trapframe->epc);

void trap_and_emulate(void) {
    struct proc *p = myproc();
    printf("(EC at %p)\n", p->trapframe->epc);

    uint32 instr = 0;
    if(copyin(p->pagetable, (char *)&instr, p->trapframe->epc, sizeof(instr)) < 0){
        printf("Cannot fetch instruction at %p\n", p->trapframe->epc);
    }

    /* Comes here when a VM tries to execute a supervisor instruction. */
    printf("entered trap_and_emulate\n");
    /* Retrieve all required values from the instruction */
    uint64 addr     = p->trapframe->epc;
    uint32 op       = instr & 0x7f;
    uint32 rd     = (instr >> 7) & 0x1f;
    uint32 funct3 = (instr >> 12) & 0x7;
    uint32 rs1    = (instr >> 15) & 0x1f;
    uint32 uimm   = (instr >> 20) & 0xfff;


    /* Print the statement */
    printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", 
                addr, op, rd, funct3, rs1, uimm);
}

void trap_and_emulate_init(void) {
    /* Create and initialize all state for the VM */
    vmm = (struct vm_virtual_state*)kalloc();
    if(vmm == NULL){
        panic("Could not allocate memory");
    }

    memset(vmm, 0, sizeof(struct vm_virtual_state));

    //
    // -------------------------------
    // User Trap Setup CSRs
    // -------------------------------
    //
    vmm->ustatus.code = 0x000;
    vmm->ustatus.mode =VM_MODE_U;
    vmm->ustatus.val  = 0;

    vmm->uie.code = 0x004;
    vmm->uie.mode =VM_MODE_U;
    vmm->uie.val  = 0;

    vmm->utvec.code = 0x005;
    vmm->utvec.mode =VM_MODE_U;
    vmm->utvec.val  = 0;

    vmm->uepc.code = 0x041;
    vmm->uepc.mode =VM_MODE_U;
    vmm->uepc.val  = 0;


    //
    // -------------------------------
    // User Trap Handling CSRs
    // -------------------------------
    //
    vmm->uscratch.code = 0x040;
    vmm->uscratch.mode =VM_MODE_U;
    vmm->uscratch.val  = 0;

    vmm->ucause.code = 0x042;
    vmm->ucause.mode =VM_MODE_U;
    vmm->ucause.val  = 0;

    vmm->utval.code = 0x043;   
    vmm->utval.mode =VM_MODE_U;
    vmm->utval.val  = 0;

    vmm->uip.code = 0x044;
    vmm->uip.mode =VM_MODE_U;
    vmm->uip.val  = 0;


    //
    // -------------------------------
    // Supervisor Trap Setup
    // -------------------------------
    //
    vmm->sstatus.code = 0x100;
    vmm->sstatus.mode = VM_MODE_S;
    vmm->sstatus.val  = 0;

    vmm->sie.code = 0x104;
    vmm->sie.mode = VM_MODE_S;
    vmm->sie.val  = 0;

    vmm->stvec.code = 0x105;
    vmm->stvec.mode = VM_MODE_S;
    vmm->stvec.val  = 0;

    vmm->scounteren.code = 0x106;
    vmm->scounteren.mode = VM_MODE_S;
    vmm->scounteren.val  = 0;


    //
    // -------------------------------
    // Supervisor Page Table Register
    // -------------------------------
    //
    vmm->satp.code = 0x180;
    vmm->satp.mode = VM_MODE_S;
    vmm->satp.val  = 0;


    //
    // -------------------------------
    // Machine Information Registers
    // -------------------------------
    //
    vmm->mvendorid.code = 0xF11;
    vmm->mvendorid.mode =VM_MODE_M;
    vmm->mvendorid.val  = 0x637365353336;

    vmm->marchid.code = 0xF12;
    vmm->marchid.mode =VM_MODE_M;
    vmm->marchid.val  = 0;

    vmm->mimpid.code = 0xF13;
    vmm->mimpid.mode =VM_MODE_M;
    vmm->mimpid.val  = 0;

    vmm->mhartid.code = 0xF14;
    vmm->mhartid.mode =VM_MODE_M;
    vmm->mhartid.val  = 0;

    vmm->mconfigptr.code = 0xF15;
    vmm->mconfigptr.mode =VM_MODE_M;
    vmm->mconfigptr.val  = 0;


    //
    // -------------------------------
    // Machine Trap Setup Registers
    // -------------------------------
    //
    vmm->mstatus.code = 0x300;
    vmm->mstatus.mode =VM_MODE_M;
    vmm->mstatus.val  = 0;

    vmm->misa.code = 0x301;
    vmm->misa.mode =VM_MODE_M;
    vmm->misa.val  = 0;

    vmm->medeleg.code = 0x302;
    vmm->medeleg.mode =VM_MODE_M;
    vmm->medeleg.val  = 0;

    vmm->mideleg.code = 0x303;
    vmm->mideleg.mode =VM_MODE_M;
    vmm->mideleg.val  = 0;

    vmm->mie.code = 0x304;
    vmm->mie.mode =VM_MODE_M;
    vmm->mie.val  = 0;

    vmm->mtvec.code = 0x305;
    vmm->mtvec.mode =VM_MODE_M;
    vmm->mtvec.val  = 0;

    vmm->mcounteren.code = 0x306;
    vmm->mcounteren.mode =VM_MODE_M;
    vmm->mcounteren.val  = 0;


    //
    // -------------------------------
    // Machine Trap Handling Registers
    // -------------------------------
    //
    vmm->mscratch.code = 0x340;
    vmm->mscratch.mode =VM_MODE_M;
    vmm->mscratch.val  = 0;

    vmm->mepc.code = 0x341;
    vmm->mepc.mode =VM_MODE_M;
    vmm->mepc.val  = 0;

    vmm->mcause.code = 0x342;
    vmm->mcause.mode =VM_MODE_M;
    vmm->mcause.val  = 0;

    vmm->mtval.code = 0x343;
    vmm->mtval.mode =VM_MODE_M;
    vmm->mtval.val  = 0;

    vmm->mip.code = 0x344;
    vmm->mip.mode =VM_MODE_M;
    vmm->mip.val  = 0;

    vmm->mtinst.code = 0x34A;
    vmm->mtinst.mode =VM_MODE_M;
    vmm->mtinst.val  = 0;

    vmm->mtval2.code = 0x34B;
    vmm->mtval2.mode =VM_MODE_M;
    vmm->mtval2.val  = 0;


    //
    // -------------------------------
    // PMP Registers
    // -------------------------------
    //
    for (int i = 0; i < 8; i+=2) {
        vmm->pmpcfg[i].code = 0x3A0 + i;
        vmm->pmpcfg[i].mode =VM_MODE_M;
        vmm->pmpcfg[i].val  = 0;
    }

    for (int i = 0; i < 64; i++) {
        vmm->pmpaddr[i].code = 0x3B0 + i;
        vmm->pmpaddr[i].mode =VM_MODE_M;
        vmm->pmpaddr[i].val  = 0;
    }

    vmm->current_exec_mode =VM_MODE_U;
}