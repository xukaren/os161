#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#if OPT_A2
 #include <mips/trapframe.h>
#endif
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  struct addrspace *as;
  struct proc *p = curproc;
  //kprintf("exiting proc %d \n", p->pid); 
    // before proc_destroy, pass the exit code to the parent process (if exists)
    // (proc_destroy is the one that detaches children when proc is exiting) 
 
  /* for now, just include this to keep the compiler from complaining about
    an unused variable */

  // cause the current process to exit 
  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  proc_remthread(curthread);


  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  #if OPT_A2
    p->exited = true; 
    p->exit_code = _MKWAIT_EXIT(exitcode);

    //kprintf("parent exists, update exit code\n");
    lock_acquire(p->lk_child_procs);
    cv_broadcast(p->cv_exiting, p->lk_child_procs); // tell the parent the child that it is waiting for is exiting (if parent called waitpid) 
    //kprintf("broadcasted to parent\n"
    lock_release(p->lk_child_procs);
    

    // case 1: parent exists, update exit code and wake parent (in case parent called waitpid on you)

    if(p->parent != NULL) {

    } else {
      // case 2: no parent, destroy everything 

      //kprintf("in sys_exit: no parent, before destroy\n");
      proc_destroy(p);    // detaches your children too 
      //kprintf("in sys_exit: no parent, after destroy\n");
    }

  #else 
    (void)exitcode;

    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_destroy(p);
  #endif 

  //kprintf("in sys_exit: returned from proc_destroy \n");

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("in sys_exit: return from thread_exit in sys_exit\n");

}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  // //kprintf("in getpid\n");
  #if OPT_A2
    *retval = curproc->pid;
  #else 
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
    *retval = 1;
  #endif
  return(0);

}

/* stub handler for waitpid() system call                */

// curr process to wait for "pid" arg
int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  //kprintf("in waitpid with waitpid proc pid # %d\n", pid); 

  int exitstatus = -1;
  int result;

  if (options != 0) {
    *retval = -1;
    return(EINVAL);
  }
  
  #if OPT_A2
    if ((int)array_num(curproc->child_procs) == 0){
      return ESRCH; // no children 
    }
    if(status == NULL){  
      *retval = -1;
      return EFAULT;
    }

    // cannot wait for itself, or for a proc with pid < 1 
    // KASSERT(pid > 1);
    // KASSERT(curproc->pid != pid); 

    //change later so system handles error (below:)

   if (pid < 1 || curproc->pid == pid){    
      //kprintf("invalid pid"); 
      *retval = -1; //  has to be something less than 0 otherwise warning will not be issiued 
      lock_release(curproc->lk_child_procs); 
      return EINVAL;  // err code for "no such process"
    }  

    lock_acquire(curproc->lk_child_procs);  
    // curproc->waitpid_called = true;

    bool found = false; 

    // check if is an existing child of the process 
		for (int i = array_num(curproc->child_procs) - 1; i >= 0; i--){
      //kprintf("waitpid looping through children: %d\n", i);

      struct proc * c = (struct proc *) array_get(curproc->child_procs, i);

      // if child is found, wait for child to exit 
			if(c->pid == pid){

        //kprintf("found child with pid %d in waitPid at index %d \n", pid, i);
        found = true;
        
        // curproc->parent->waitpid_called = true; 

        lock_acquire(c->lk_child_procs); 
        
        while (!c->exited){
          //kprintf("in while before cv wait\n");
          cv_wait(c->cv_exiting, c->lk_child_procs);  // changes the child's exit code 
          //kprintf("in while after cv wait\n");

        }  
        //kprintf("done \n");

        lock_release(c->lk_child_procs); 

        exitstatus = c->exit_code;

        //kprintf("exit status after cv_wait on pid %d is %d\n", pid, exitstatus);
        break; 
      }
		}
    // error if waitpid called on a valid pid but not a valid child  
    if(!found){
      //kprintf("valid pid, but no such child of it with that pid exists \n");
      // lock_release(curproc->lk_child_procs); 
      *retval = -1;    
      return ECHILD;  // or ESRCH ? 
    }

    lock_release(curproc->lk_child_procs); 

  #else
    /* this is just a stub implementation that always reports an
      exit status of 0, regardless of the actual exit status of
      the specified process.   
      In fact, this will return 0 even if the specified process
      is still running, and even if it never existed in the first place.

      Fix this!
    */

    /* for now, just pretend the exitstatus is 0 */
    exitstatus = 0;

  #endif

  result = copyout ((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  
  *retval = pid;
  return(0);
}

#if OPT_A2

int sys_fork(struct trapframe *tf, pid_t * retval){
  // create a process structure for child process ( assign PID to child process  too)

  struct proc * child_fork = proc_create_runprogram(curproc->p_name);

  if(child_fork == NULL){
    *retval = -1;
    return ENOMEM;
  }
  
  KASSERT(child_fork->pid > 0); 

  // create parent/child relationship

  child_fork->parent = curproc; 
  array_add(curproc->child_procs, child_fork, NULL);

  // attach newly created addr space (with contents of parent's address space) to child process
  // create addr space and data from parent  
  int err = as_copy(curproc_getas(), &child_fork->p_addrspace);  

  if (err) {
    return ENOMEM;
  } 
  
  // clone a trapframe for the child's stack and modify it so it returns the current value 
  child_fork->tf = kmalloc(sizeof(struct trapframe));
  KASSERT(child_fork->tf != NULL);
  memcpy((void *)child_fork->tf, (const void *) tf, sizeof(struct trapframe));   // args: (dest, src, length)
  curproc->tf = child_fork->tf;
  
  // create a thread for child process 
  int err2 = thread_fork(child_fork->p_name, child_fork, (void*) &enter_forked_process, child_fork->tf, 0); // what # other than -234? 

  if (err2) {
    return ENOMEM;
  }
  
  *retval = child_fork->pid;
  return(0);
}
#endif
