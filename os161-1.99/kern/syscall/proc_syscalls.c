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
 #include <vfs.h>
 #include <kern/fcntl.h>
#endif
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {
  struct addrspace *as;
  struct proc *p = curproc;
  // // // // kprint("exiting proc %d \n", p->pid); 
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
 

    // case 1: parent exists, update exit code and wake parent (in case parent called waitpid on you)

    if(p->parent) {
      p->exited = true; 
      p->exit_code = _MKWAIT_EXIT(exitcode);
      // kprint("setting exitcode to %d for %d \n", p->exit_code, p->pid);

      //// // // kprint("parent exists, update exit code\n");
              // kprint("acquired lock %d  in sys_exit 1 \n", p->pid);

      lock_acquire(p->lk_child_procs);
      cv_broadcast(p->cv_exiting, p->lk_child_procs); // tell the parent the child that it is waiting for is exiting (if parent called waitpid) 
      // kprint("%d broadcasted to parents\n", p->pid);
        // kprint("released lock %d in sys_exit 2\n", p->pid);

      lock_release(p->lk_child_procs);
    } else {
      // case 2: no parent, destroy self

      //// // // kprint("in sys_exit: no parent, before destroy\n");
      proc_destroy(p);    // detaches your children too 
      //// // // kprint("in sys_exit: no parent, after destroy\n");
    }

  #else 
    (void)exitcode;

    /* detach this thread from its process */
    /* note: curproc cannot be used after this call */
    proc_destroy(p);
  #endif 

  //// // // kprint("in sys_exit: returned from proc_destroy \n");

  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("in sys_exit: return from thread_exit in sys_exit\n");

}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  // //// // // kprint("in getpid\n");
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
  //// // // kprint("in waitpid with waitpid proc pid # %d\n", pid); 

  int exitstatus;
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
      //// // // kprint("invalid pid"); 
      *retval = -1; //  has to be something less than 0 otherwise warning will not be issiued 
      // lock_release(curproc->lk_child_procs); 
      return EINVAL;  // err code for "no such process"
    }  
      // kprint("acquired lock %d  in WP 1\n", curproc->pid );

    lock_acquire(curproc->lk_child_procs);  
 // curproc->waitpid_called = true;

    bool found = false; 

    // check if is an existing child of the process 
		for (int i = array_num(curproc->child_procs) - 1; i >= 0; i--){
      //// // // kprint("waitpid looping through children: %d\n", i);

      struct proc * c = (struct proc *) array_get(curproc->child_procs, i);

      // if child is found, wait for child to exit 
			if(c->pid == pid){

        //// // // kprint("found child with pid %d in waitPid at index %d \n", pid, i);
        found = true;
        
        // curproc->parent->waitpid_called = true; 
            // kprint("acquired lock %d in WP 2\n", c->pid);

        lock_acquire(c->lk_child_procs); 

        while (!c->exited){
          //// // // kprint("in while before cv wait\n");
          cv_wait(c->cv_exiting, c->lk_child_procs);  // changes the child's exit code 
          // // // kprint("parent waiting for child %d to exit \n", c->pid);

        }  
        //// // // kprint("done \n");
        // kprint("released lock %d in WP 3.5\n", curproc->pid);

        lock_release(c->lk_child_procs); 

        exitstatus = c->exit_code; 
        //// // // kprint("exit status after cv_wait on pid %d is %d\n", pid, exitstatus);
        break; 
      }
		}
    // error if waitpid called on a valid pid but not a valid child  
    if(!found){
      //// // // kprint("valid pid, but no such child of it with that pid exists \n");
        // kprint("released lock %d in WP 3\n", curproc->pid);

      lock_release(curproc->lk_child_procs); 

      *retval = -1;    
      return ECHILD;  // or ESRCH ? 
    }
      // kprint("released lock %d in WP 4\n", curproc->pid);

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
  // // create a process structure for child process ( assign PID to child process  too)
  // // // // kprint("sys_fork\n");

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
  // curproc->tf?
  int err2 = thread_fork(child_fork->p_name, child_fork, (void*) &enter_forked_process, child_fork->tf, 0); // what # other than -234? 

  if (err2) {
    return ENOMEM;
  }
  
  *retval = child_fork->pid;
  return(0);
}

int sys_execv (const char * progname, char ** args, int *retval){
  (void)args;

  // // // // kprint("sys_execv\n");
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

  // copy program name and path from user space into the kernel 
  size_t nameLength = sizeof(char) * (strlen(progname)+1);
  char * kernel_progname = kmalloc(nameLength);
  if(kernel_progname == NULL){
    *retval = -1; 
    return ENOMEM; 
  }
  int res1 = copyin((const_userptr_t) progname, (void*)kernel_progname, nameLength);
  // // // // kprint("%s\n", kernel_progname);
  if(res1){
    kfree(kernel_progname); 
    *retval = -1;
    return res1;
  }

  // count arguments  
  int numArgs = 0; 
  while(args[numArgs] != NULL){
    numArgs++; 
  }
  // // // // kprint("%d\n", numArgs);      

  // copy arguments TO KERNEL
  char ** argv = kmalloc((numArgs+1) * sizeof(char*));
  if(argv == NULL){
    kfree(kernel_progname); 
    *retval = -1; 
    return ENOMEM;
  }
  
  for (int i = 0; i < numArgs; i++){
    const size_t argLength = 128; // HARDCODED 
    argv[i] = kmalloc(argLength * sizeof(char));
    
    if(argv[i] == NULL){
      kfree(kernel_progname); 
      for (int j = 0; j < i ; j++){
        kfree(argv[j]);
      }
      kfree(argv);
      *retval = -1; 
      return ENOMEM; 
    }
    result = copyin((const_userptr_t) args[i], argv[i], argLength * sizeof(char));
    if(result){
      kfree(kernel_progname);
      for (int j = 0; j <= i ; j++){
        kfree(argv[j]);
      }
      kfree(argv);
      *retval = -1; 
      return result; 
    }  
  }

  argv[numArgs] = NULL;   // last arg is NULL

  for (int i = 0; i < numArgs; i++){
    // // // // kprint("%s\n", argv[i]);
  }

	/* Open the file. */
	result = vfs_open(kernel_progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	// KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	struct addrspace * oldas = curproc_setas(as);    // switch 
	as_activate();      // mark current TLB entries invalid 

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

  // copy args TO USER STACK
  vaddr_t currStackPtr = stackptr;  
  vaddr_t * sArgs = kmalloc((numArgs+1) * sizeof(vaddr_t));

  sArgs[numArgs] = (vaddr_t) NULL;
  for(int i = numArgs-1; i>= 0; i--){
    const size_t requiredLen = 128;   // HARDCODED 
    size_t argSize = requiredLen * sizeof(char); 
    currStackPtr -= argSize; 
    int err = copyout((void*) argv[i], (userptr_t) currStackPtr, requiredLen);
    if(err){
      return err; 
    }
    sArgs[i] = currStackPtr;
  }

  for (int i = numArgs; i >= 0; i--){
    size_t sp_size = sizeof(vaddr_t); 
    currStackPtr -= sp_size;
    int err = copyout((void*) &sArgs[i], (userptr_t) currStackPtr, sp_size);
    if(err){
      return err; 
    }
  }

  as_destroy(oldas); 
  kfree(kernel_progname); 
  

	/* Warp to user mode. */
	enter_new_process(numArgs /*argc*/, (userptr_t) currStackPtr /*userspace addr of argv*/,
			  currStackPtr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;  

}

#endif
