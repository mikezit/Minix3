/* The kernel call that is implemented in this file:
 *   m_type:	SYS_SIGSEND
 *
 * The parameters for this kernel call are:
 *     m2_i1:	SIG_PROC  	# process to call signal handler
 *     m2_p1:	SIG_CTXT_PTR 	# pointer to sigmsg structure
 *     m2_i3:	SIG_FLAGS    	# flags for S_SIGRETURN call(没有使用)	
 *
 */

#include "../system.h"
#include <signal.h>
#include <string.h>
#include <sys/sigcontext.h>

#if USE_SIGSEND

/*===========================================================================*
 *			      do_sigsend				     *
 *===========================================================================*/
PUBLIC int do_sigsend(m_ptr)
message *m_ptr;			/* pointer to request message */
{
/* Handle sys_sigsend, POSIX-style signal handling. */

  struct sigmsg smsg;
  register struct proc *rp;
  phys_bytes src_phys, dst_phys;
  struct sigcontext sc, *scp;
  struct sigframe fr, *frp;

  if (! isokprocn(m_ptr->SIG_PROC)) return(EINVAL);
  if (iskerneln(m_ptr->SIG_PROC)) return(EPERM); /* 不能向内核进程发送信号 */
  rp = proc_addr(m_ptr->SIG_PROC);

  /* Get the sigmsg structure into our address space.  */
  src_phys = umap_local(proc_addr(PM_PROC_NR), D, (vir_bytes) /* PM_PROC_NR是PM的进程号 */
      m_ptr->SIG_CTXT_PTR, (vir_bytes) sizeof(struct sigmsg)); /* 得到物理地址 */
  if (src_phys == 0) return(EFAULT);
  phys_copy(src_phys,vir2phys(&smsg),(phys_bytes) sizeof(struct sigmsg)); /* 内存拷贝 */

  /* Compute the user stack pointer where sigcontext will be stored. */
  scp = (struct sigcontext *) smsg.sm_stkptr - 1; /* 栈指针下移一位 */

  /* Copy the registers to the sigcontext structure. */
  memcpy(&sc.sc_regs, (char *) &rp->p_reg, sizeof(struct sigregs));

  /* Finish the sigcontext initialization. */
  sc.sc_flags = SC_SIGCONTEXT;
  sc.sc_mask = smsg.sm_mask;

  /* Copy the sigcontext structure to the user's stack. */
  dst_phys = umap_local(rp, D, (vir_bytes) scp,
      (vir_bytes) sizeof(struct sigcontext));
  if (dst_phys == 0) return(EFAULT);
  phys_copy(vir2phys(&sc), dst_phys, (phys_bytes) sizeof(struct sigcontext)); /* 调用phys_copy的时候会自动增加scp */

  /* Initialize the sigframe structure. */
  frp = (struct sigframe *) scp - 1;
  fr.sf_scpcopy = scp;
  fr.sf_retadr2= (void (*)()) rp->p_reg.pc; /* 调用该信号进程的下个执行地址 */
  fr.sf_fp = rp->p_reg.fp;
  rp->p_reg.fp = (reg_t) &frp->sf_fp;
  fr.sf_scp = scp;
  fr.sf_code = 0;	/* XXX - should be used for type of FP exception */
  fr.sf_signo = smsg.sm_signo;
  fr.sf_retadr = (void (*)()) smsg.sm_sigreturn;

  /* Copy the sigframe structure to the user's stack. */
  dst_phys = umap_local(rp, D, (vir_bytes) frp, 
      (vir_bytes) sizeof(struct sigframe));
  if (dst_phys == 0) return(EFAULT);
  phys_copy(vir2phys(&fr), dst_phys, (phys_bytes) sizeof(struct sigframe));

  /* Reset user registers to execute the signal handler. */
  rp->p_reg.sp = (reg_t) frp;
  rp->p_reg.pc = (reg_t) smsg.sm_sighandler;

  return(OK);
}

#endif /* USE_SIGSEND */

