#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define VM_MODE_U 0   
#define VM_MODE_S 1   
#define VM_MODE_M 2   

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
    struct vm_reg sepc;

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
    uint64 pmp_config;
    pagetable_t pagetable; 
    pagetable_t backuppagetable; 

};
struct vm_virtual_state *vmm;
// In your ECALL, add the following for prints
// struct proc* p = myproc();
// printf("(EC at %p)\n", p->trapframe->epc);

static struct vm_reg* csr_register(uint32 csr_num) {
    switch (csr_num) {
    /* User CSRs */
    case 0x000: return &vmm->ustatus;
    case 0x004: return &vmm->uie;
    case 0x005: return &vmm->utvec;
    case 0x041: return &vmm->uepc;

    case 0x040: return &vmm->uscratch;
    case 0x042: return &vmm->ucause;
    case 0x043: return &vmm->utval;
    case 0x044: return &vmm->uip;

    /* Supervisor CSRs */
    case 0x100: return &vmm->sstatus;
    case 0x104: return &vmm->sie;
    case 0x105: return &vmm->stvec;
    case 0x106: return &vmm->scounteren;
    case 0x141: return &vmm->sepc;
    case 0x180: return &vmm->satp;

    /* Machine info */
    case 0xF11: return &vmm->mvendorid;
    case 0xF12: return &vmm->marchid;
    case 0xF13: return &vmm->mimpid;
    case 0xF14: return &vmm->mhartid;
    case 0xF15: return &vmm->mconfigptr;

    /* Machine trap setup */
    case 0x300: return &vmm->mstatus;
    case 0x301: return &vmm->misa;
    case 0x302: return &vmm->medeleg;
    case 0x303: return &vmm->mideleg;
    case 0x304: return &vmm->mie;
    case 0x305: return &vmm->mtvec;
    case 0x306: return &vmm->mcounteren;

    /* Machine trap handling */
    case 0x340: return &vmm->mscratch;
    case 0x341: return &vmm->mepc;
    case 0x342: return &vmm->mcause;
    case 0x343: return &vmm->mtval;
    case 0x344: return &vmm->mip;
    case 0x34A: return &vmm->mtinst;
    case 0x34B: return &vmm->mtval2;

    /* PMP ranges */
    default:
        if (csr_num >= 0x3B0 && csr_num < 0x3B0 + 64)
            return &vmm->pmpaddr[csr_num - 0x3B0];
        if (csr_num >= 0x3A0 && csr_num < 0x3A0 + 8)
            return &vmm->pmpcfg[csr_num - 0x3A0];
        return NULL;
    }
}

static uint64 get_tf_reg(struct trapframe *tf, int r)
{
    switch (r) {
    case 0:  return 0;
    case 1:  return tf->ra;
    case 2:  return tf->sp;
    case 3:  return tf->gp;
    case 4:  return tf->tp;
    case 5:  return tf->t0;
    case 6:  return tf->t1;
    case 7:  return tf->t2;
    case 8:  return tf->s0;
    case 9:  return tf->s1;
    case 10: return tf->a0;
    case 11: return tf->a1;
    case 12: return tf->a2;
    case 13: return tf->a3;
    case 14: return tf->a4;
    case 15: return tf->a5;
    case 16: return tf->a6;
    case 17: return tf->a7;
    case 18: return tf->s2;
    case 19: return tf->s3;
    case 20: return tf->s4;
    case 21: return tf->s5;
    case 22: return tf->s6;
    case 23: return tf->s7;
    case 24: return tf->s8;
    case 25: return tf->s9;
    case 26: return tf->s10;
    case 27: return tf->s11;
    case 28: return tf->t3;
    case 29: return tf->t4;
    case 30: return tf->t5;
    case 31: return tf->t6;
    }
    return 0;
}

static void set_tf_reg(struct trapframe *tf, int r, uint64 val)
{
    if (r == 0) return;

    switch (r) {
    case 1:  tf->ra = val; break;
    case 2:  tf->sp = val; break;
    case 3:  tf->gp = val; break;
    case 4:  tf->tp = val; break;
    case 5:  tf->t0 = val; break;
    case 6:  tf->t1 = val; break;
    case 7:  tf->t2 = val; break;
    case 8:  tf->s0 = val; break;
    case 9:  tf->s1 = val; break;
    case 10: tf->a0 = val; break;
    case 11: tf->a1 = val; break;
    case 12: tf->a2 = val; break;
    case 13: tf->a3 = val; break;
    case 14: tf->a4 = val; break;
    case 15: tf->a5 = val; break;
    case 16: tf->a6 = val; break;
    case 17: tf->a7 = val; break;
    case 18: tf->s2 = val; break;
    case 19: tf->s3 = val; break;
    case 20: tf->s4 = val; break;
    case 21: tf->s5 = val; break;
    case 22: tf->s6 = val; break;
    case 23: tf->s7 = val; break;
    case 24: tf->s8 = val; break;
    case 25: tf->s9 = val; break;
    case 26: tf->s10 = val; break;
    case 27: tf->s11 = val; break;
    case 28: tf->t3 = val; break;
    case 29: tf->t4 = val; break;
    case 30: tf->t5 = val; break;
    case 31: tf->t6 = val; break;
    }
}

int is_pmp_configured(void) {
    return (vmm != NULL && vmm->pmp_config == 1);
}

void uvmcopy_copmp(pagetable_t old, pagetable_t new, uint64 sz){
    pte_t *pte;
    uint64 pa, i;
    uint flags;
    char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0){
        printf("unable to kalloc men\n");
    }
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
    }
  }


  for(i = 0x80000000; i < 0x80400000; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0){
        printf("unable to kalloc men\n");
    }
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
    }
    }

}

void pmp_apply_rules(pagetable_t pt) {
    uint64 prev_addr = 0;
    
    for(int i = 0; i < 64; i++) {
        uint64 pmpaddr_val = vmm->pmpaddr[i].val;
        
        if(pmpaddr_val == 0) continue;
        
        int cfg_reg_idx = (i / 8) * 2;
        int cfg_byte_idx = i % 8;
        
        uint64 pmpcfg = vmm->pmpcfg[cfg_reg_idx].val;
        uint64 cfg_byte = (pmpcfg >> (cfg_byte_idx * 8)) & 0xFF;
        
        int A = (cfg_byte >> 3) & 0x3;
        
        uint64 region_end = pmpaddr_val << 2;
        uint64 region_start = prev_addr;
        
        prev_addr = region_end;
        
        if(A == 0) continue;  
        
        int R = cfg_byte & 0x1;
        int W = (cfg_byte >> 1) & 0x1;
        int X = (cfg_byte >> 2) & 0x1;
        
        if(R == 0 && W == 0 && X == 0) {
            for(uint64 va = region_start; va < region_end; va += PGSIZE) {
                pte_t *pte = walk(pt, va, 0);
                if(pte && (*pte & PTE_V)) {
                    uvmunmap(pt, va, 1, 0);
                }
            }
        }
    }
}
pagetable_t vmm_pagetable_backup(void){
    return vmm->backuppagetable;
}

void print_pmp_regions(void) {
    uint64 prev_addr = 0;
    
    for(int i = 0; i < 64; i++) {
        uint64 pmpaddr_val = vmm->pmpaddr[i].val;
        
        if(pmpaddr_val == 0) continue;
        
        int cfg_reg_idx = (i / 8) * 2;
        int cfg_byte_idx = i % 8;
        
        uint64 pmpcfg = vmm->pmpcfg[cfg_reg_idx].val;
        uint64 cfg_byte = (pmpcfg >> (cfg_byte_idx * 8)) & 0xFF;
        
        uint64 region_end = pmpaddr_val << 2;
        
        printf("Region: %p to %p, Perm: %p\n", prev_addr, region_end, cfg_byte);
        
        prev_addr = region_end;
    }
}

void do_pmp_switch(struct proc *p){

    if(vmm->pagetable == NULL) {
        vmm->backuppagetable = p->pagetable;
        vmm->pagetable = proc_pagetable(p);
        uvmcopy_copmp(p->pagetable, vmm->pagetable, p->sz);
        pmp_apply_rules(vmm->pagetable);
        //uvmunmap(vmm->pagetable, 0x0000000080000000, 1, 0);
    }
    
    p->pagetable = vmm->pagetable;
}

void trap_and_emulate(void) {
    struct proc *p = myproc();

    uint32 instr = 0;
    if(copyin(p->pagetable, (char *)&instr, p->trapframe->epc, sizeof(instr)) < 0){
        printf("Cannot fetch instruction at %p\n", p->trapframe->epc);
    }

    /* Comes here when a VM tries to execute a supervisor instruction. */
    //printf("entered trap_and_emulate\n");
    /* Retrieve all required values from the instruction */
    uint64 addr     = p->trapframe->epc;
    uint32 op       = instr & 0x7f;
    uint32 rd     = (instr >> 7) & 0x1f;
    uint32 funct3 = (instr >> 12) & 0x7;
    uint32 rs1    = (instr >> 15) & 0x1f;
    uint32 uimm   = (instr >> 20) & 0xfff;

    if(funct3 == 0 && uimm == 0){
        printf("(EC at %p)\n", p->trapframe->epc);
        if(vmm->current_exec_mode == VM_MODE_U)
        {
            vmm->current_exec_mode = VM_MODE_S;
            vmm->sepc.val = p->trapframe->epc;
            p->trapframe->epc = vmm->stvec.val;
        }else if(vmm->current_exec_mode == VM_MODE_S)
        {
            vmm->current_exec_mode = VM_MODE_M;
            vmm->mepc.val = p->trapframe->epc;
            p->trapframe->epc = vmm->mtvec.val;
            p->pagetable=vmm->pagetable;
        }
        return;
    }
    /* Print the statement */
    printf("(PI at %p) op = %x, rd = %x, funct3 = %x, rs1 = %x, uimm = %x\n", 
                addr, op, rd, funct3, rs1, uimm);

    //ecall for prints
    //SRET
    if (funct3 == 0 && uimm == 0x102) {
        //printf("entered sret handler\n");
        uint64 value_sstatus = vmm->sstatus.val;
        uint64 spp = (value_sstatus >> 8) & 0x1;
        
        if(vmm->current_exec_mode < 1){
            //printf("Called sret not in S mode\n");
            kill(p->pid);
        }
        else{
            if (spp == 1) {
                vmm->current_exec_mode = VM_MODE_S;
            } 
            else {
                if (vmm->current_exec_mode == VM_MODE_S) {
                    vmm->current_exec_mode = VM_MODE_U;
                    p->trapframe->epc = vmm->sepc.val;
                } 
                else{
                    kill(p->pid);
                }
            }
            //p->pagetable=vmm->pagetable;
        }
    }//MRET
    else if (funct3 == 0 && uimm == 0x302) {
        //printf("entered mret handler\n");
        uint64 value_mstatus = vmm->mstatus.val;
        uint64 mpp = (value_mstatus >> 11) & 0x3;
        if (mpp == 3) {
            vmm->current_exec_mode = VM_MODE_M;
            p->trapframe->epc = vmm->mepc.val;
        } else if (mpp == 2) {
            kill(p->pid);
        } else if (mpp == 1) {
            vmm->current_exec_mode = VM_MODE_S;
            p->trapframe->epc = vmm->mepc.val;
        } else if (mpp == 0) {
            vmm->current_exec_mode = VM_MODE_U;
            p->trapframe->epc = vmm->mepc.val;
        }
        if(vmm->pmp_config == 1 && vmm->current_exec_mode < VM_MODE_M) {
            // Print PMP regions FIRST
            print_pmp_regions();
            
            // Then switch to PMP-restricted pagetable
            do_pmp_switch(p);
        }
        
    } //csrwrite
    else if (funct3 == 0x1) {
        //printf("entered csrwrite handler\n");
        struct vm_reg* found_reg = csr_register(uimm);
        if (found_reg != NULL) {
            int source_val = get_tf_reg(p->trapframe, rs1);
            if(found_reg->code==0xF11 && source_val==0x0){//graceful vm shutdown
                kill(p->pid);
            }
            if (uimm >= 0x3A0 || uimm <= 0x3B0) {//meaning writing to pmp
                vmm->pmp_config=1;
            }
            if(vmm->current_exec_mode >=found_reg->mode){
                found_reg->val=source_val;
            }else{
                kill(p->pid);
            }
        } else {
            kill(p->pid);
        }
        p->trapframe->epc += 4;
    }//csrread
    else if (funct3 == 0x2) {
        //printf("entered csrread handler\n");
        struct vm_reg* found_reg = csr_register(uimm);
        if (found_reg == NULL) {
            kill(p->pid);
        } else {
            //printf("current mode execution is: %d and the register's mode I am lloking for is: %d\n",vmm->current_exec_mode,found_reg->mode);
            if(vmm->current_exec_mode >=found_reg->mode){
                //printf("right before set trapframe\n");
                set_tf_reg(p->trapframe, rd, found_reg->val);
            }
        }
        p->trapframe->epc += 4;
    }else {
        kill(p->pid);
    }
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

    vmm->sepc.code = 0x141;
    vmm->sepc.mode = VM_MODE_S;
    vmm->sepc.val  = 0;

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

    vmm->current_exec_mode =VM_MODE_M;
    vmm->pmp_config=0;
    vmm->pagetable=NULL; 
    vmm->backuppagetable=NULL; 

}