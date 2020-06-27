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
  kprintf("exiting proc %d \n", p->pid); 

  #if OPT_A2
    // modifications in a2a: 
    // before causing the current process to exxit, we pass the exit code to the parent process (if it still exists)

    // if parent of current process still exists
    if(curproc->parent){
      lock_acquire(curproc->parent->lk_child_procs);
    
      // go through all children of current process 
      for (int i = array_num(curproc->parent->child_procs) - 1; i >= 0; i--){
        kprintf("exit looping through children: %d\n", i);

        if(((struct child_arr_elem*) array_get(curproc->parent->child_procs, i))->pid == curproc->pid){
          kprintf("found equal pid %d procs at index %d, so setting exit code to %d\n", curproc->pid, i, exitcode);
          // if pids equal, set exit code of child in parent's array of children, not the zombie child 
          ((struct child_arr_elem *) array_get(curproc->parent->child_procs, i))->exit_code = exitcode;
          
          break; 
        }
      }
    
      lock_release(curproc->parent->lk_child_procs);
      cv_signal(curproc->cv_exiting, curproc->lk_child_procs); // tell the parent the child that it is waiting for is exiting (if parent called waitpid) 
    }
  #else
    /* for now, just include this to keep the compiler from complaining about
     an unused variable */
    (void)exitcode;
  #endif

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

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  kprintf("in getpid\n");
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

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  kprintf("in waitpid with waitpid proc pid # %d\n", pid); 

  int exitstatus;
  int result;

  if (options != 0) {
    return(EINVAL);
  }
  
  #if OPT_A2
    if(status == NULL){
      return EFAULT;
    }
    
    // check if valid pid 
    if (pid < 1 || curproc->pid == pid){     // check 
      kprintf("invalid pid"); 
      *retval = -1; //  has to be something less than 0 otherwise warning will not be issiued 
      lock_release(curproc->lk_child_procs); 
      return EINVAL;  // err code for "no such process"
    }    

    lock_acquire(curproc->lk_child_procs);   // in case a child  exits while loop is being performed / added to

    bool existingChild = false; 
    // check if is an existing child of the process 
		for (int i = array_num(curproc->child_procs) - 1; i >= 0; i--){
      kprintf("waitpid looping through children: %d\n", i);

      struct child_arr_elem * child_to_wait_for = (struct child_arr_elem*) array_get(curproc->child_procs, i);

			if(child_to_wait_for->pid == pid){
        kprintf("found child with pid %d in waitPid at index %d \n", pid, i);
        existingChild = true;
        while (child_to_wait_for->exit_code != 0){
          cv_wait(child_to_wait_for->actual_proc->cv_exiting, curproc->lk_child_procs); 
        }
        kprintf("done cv wait");

        exitstatus = _MKWAIT_EXIT(child_to_wait_for->exit_code);

        // child_to_wait_for = array_get(curproc->child_procs, i);
        break; 
      }
		}
    // waitpid called on a valid pid error 
    if(!existingChild){
      kprintf("killing zombie because child already freed \n");
      // kfree(child_to_wait_for); // zombie 
      lock_release(curproc->lk_child_procs); 
      *retval = -1;    
      return ECHILD;  // or ESRCH ? 
    }

    // if child is not exited, wait 

    

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

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2

int sys_fork(struct trapframe *tf, pid_t * retval){
  // 1: create a process structure for child process 
  
  struct proc * child_fork = proc_create_runprogram(curproc->p_name);
  
  if(child_fork == NULL){
    *retval = -1;
    return ENOMEM;
  }
  
  KASSERT(child_fork->pid > 0); 

  // 3: attach newly created addr space (with contents of parent's address space) to child process
  // child_fork->p_addrspace = parent_as_copy;

  // 4: assign PID to child process 
  //child_fork->pid = 
  
  
  // create parent/child relationship

  child_fork->parent = curproc; 

  //create child proc to put in parent's array 
  struct child_arr_elem * arr_elem = kmalloc(sizeof(struct child_arr_elem));
  arr_elem->actual_proc = child_fork; 
  // child_elem->exited = false; 
  arr_elem->exit_code = -1; 
  arr_elem->pid = child_fork->pid; 
  array_add(curproc->child_procs, arr_elem, NULL);

// 2: create addr space and data from parent  
  //struct addrspace * parent_as_copy = NULL;
  // int err = as_copy(curproc->p_addrspace, &child_fork->p_addrspace);  
  int err = as_copy(curproc_getas(), &child_fork->p_addrspace);  

  if (err) {
    panic("sys_fork(): as_copy failed: %s\n", strerror(err));
  } 
  // errors 
  if(err != 0){
    // need to free memory first? nah already panic == broken 
    panic("sys_fork(): problem creating child address space");
    // return ENOMEM;  //never gets here 
  }
  
  // clone a trapframe for the child's  stack and modify it so it returns the current value 
  child_fork->tf = kmalloc(sizeof(struct trapframe));
  KASSERT(child_fork->tf != NULL);
  memcpy((void *)child_fork->tf, (const void *) tf, sizeof(struct trapframe));   // args: (dest, src, length)
  curproc->tf = child_fork->tf;
  
  // 5: create a thread for child process 
  int err2 = thread_fork(child_fork->p_name, child_fork, (void*) &enter_forked_process, child_fork->tf, 0); // what # other than -234? 

  if (err2) {
    panic("sys_fork(): thread_fork failed: %s\n", strerror(err));
  } 
  
  *retval = child_fork->pid;
  return(0);
}
#endif
