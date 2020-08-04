/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <opt-A3.h>
/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
#if OPT_A3
static struct spinlock cm_lock = SPINLOCK_INITIALIZER;
static bool useCM = false;
static int * cm;						// coremap 
static paddr_t p_addr_low;				// phys addr start 
static paddr_t p_addr_high;				// phys addr end
static int core_size = 0;		// # of elements to be stored in coremap 

// 

#endif
void
vm_bootstrap(void)
{
	#if OPT_A3

	//how much ram you have 
	ram_getsize(&p_addr_low, &p_addr_high); 						// lowest and highest possible physical addresses you have - not necessarily continuous? 
	p_addr_low = ROUNDUP(p_addr_low, PAGE_SIZE); 					// make it page aligned 
	p_addr_high = ROUNDUP(p_addr_high, PAGE_SIZE) - PAGE_SIZE;		
		// ex 4GB and 7GB
	
	// chunk into frames 
	core_size = (p_addr_high - p_addr_low)/ PAGE_SIZE;	// # possible frames, and also the # of elements in the core map 
		// ex. cs = (3G/PAGE/SIZE )

	// kprintf("coresize = %d\n", core_size); 

	int cm_needed_pages = ROUNDUP((sizeof(int) * core_size), PAGE_SIZE) / PAGE_SIZE ; 		// # bytes needed for CM 
		// ex. roundup(100)/4096  = 1 
		
	cm = (int*) (PADDR_TO_KVADDR(p_addr_low)); 

	// temp kmallocing 	(no actual memory given)	
	for(int i = 0; i < cm_needed_pages; i++){
		cm[i] = 1; 		// 1 =  in use (by coremap)
	} 
	for(int i = cm_needed_pages; i < core_size; i++){
		cm[i] = 0;		// 0 = not in use (can allocate)
	}
	
	useCM = true; 

	#else
	/* Do nothing. */
	#endif
}

// DRAW STACK AND EVERYTHING 

static
paddr_t
getppages(unsigned long npages)		//getppages(1)
{
	paddr_t addr;
#if OPT_A3
	// use RAM_STEALMEM since coremap has not been initialized yet 
	if(useCM == false){

		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);	
		return addr; 	
	}
	
	// else: start using the coremap to allocated physical memory	
	spinlock_acquire(&cm_lock);
	addr = 0;
	// kprintf("looking for %d pages from COREMAP\n", (int)npages);

	/***** cm_stealmem *****/ 
	// find a place in coremap to put npages continous pages (and label them in the coremap) 
	for(int i = 0; i < core_size; i++){
		
		if (cm[i] == 0){


			unsigned int numPages= 1;

			if(numPages == npages){

				// label coremap with values of 1,2,3...., until npages are all labeled
				addr = p_addr_low + (PAGE_SIZE * i); 	// the start of physical addr of the npages  

				cm[i] = 1;	// label coremap 
				// kprintf("found 1 page at %x, putting into coremap[%d]\n",addr, i);

				spinlock_release(&cm_lock);
				return addr;	
			}
			
			for(int endIndex = i+1; endIndex < core_size; endIndex++){
				
				if(cm[endIndex] != 0){
					break; 
				}
							
				numPages++; 
				// kprintf("%d\n", numPages);
				if(numPages == npages){		// found a continuous  block of npages free 
					addr = p_addr_low + (PAGE_SIZE * i); 	// the start of physical addr of the npages  
					// kprintf("found %x\n",addr);

					// label coremap with values of 1,2,3...., until npages are all labeled
					for(unsigned int k = 1; k <= npages; k++){
						cm[i] = k;			// cm[3] = 1, cm[4] = 2, cm[5] = 3
						i++; 				// not technically the startAddr anymore (as it is incremented)
					}
				// kprintf("found a sequence of %d free pages at %x, puttin into coremap[?] to coremap[%d]\n",numPages, addr, i); 

					spinlock_release(&cm_lock);
					return addr;	
				}
			}
		}
		// if cm[i] != 0 then used. go onto next i available (i++)
	}
	/***** cm_stealmem   *****/ 

	spinlock_release(&cm_lock);
	// kprintf("not found\n");
	return addr;	
	/***** CM_USE END  *****/ 

#else 
	spinlock_acquire(&stealmem_lock);
	addr = ram_stealmem(npages);
	spinlock_release(&stealmem_lock);
	return addr;
#endif

}

/* Allocate/free some kernel-space virtual pages */
vaddr_t 
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
				// kprintf("GPP pa %d\n", npages);

	if (pa==0) {
		return ENOMEM;
	}
	
	return PADDR_TO_KVADDR(pa);
}

void 
free_kpages(vaddr_t addr)
{
	/* nothing - leak the memory. */
	#if OPT_A3

	/****** reverse of getppages  *****/ 
	
	spinlock_acquire(&cm_lock);

	paddr_t paddr = KVADDR_TO_PADDR(addr); 

	int i =  (paddr - p_addr_low)/PAGE_SIZE;  // REVERSE GETPPAGES
	// kprintf("freeing block of pages starting at  %x, index %d in cormap \n",addr, i);

	// free the same number of pages as was allocated and update coremap to make those rames available
	
	// figure out indices later 
	// int pgsSoFar = 0; 
	
	// i++;
	// kprintf("%d %d\n", cm[i], cm[i+1]);
	// kprintf("FREED page\n");
	cm[i] = 0; 
	// kprintf("FREED coremap[%d] \n", i);

	i++;
	while(i < core_size && cm[i]+1 == cm[i+1] ){
		// kprintf("FREED coremap[%d] \n", i);

		cm[i] = 0; 
		i++;
		// pgsSoFar++;
	}
	// kprintf("freed %d pages from coremap\n", pgsSoFar);
	spinlock_release(&cm_lock);
	
	/****** reverse of getppages done   *****/ 

	#else
	(void)addr;
	#endif
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;
#if OPT_A3
	bool is_code = false; 	//code seg or not 
#endif 

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
	#if OPT_A3
		return EACCES;			// permission denied 
	#else
		panic("dumbvm: got VM_FAULT_READONLY\n");		
	#endif

	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	#if OPT_A3

	// KASSERT(as->as_pbase1 != NULL);
	// KASSERT(as->as_pbase2 != NULL);
	// KASSERT(as->as_stackpbase != NULL);

	#else 
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_stackpbase != 0);

	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	#endif
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

#if OPT_A3	
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1[0];		
		is_code = true; 	// whether this is the code segment

	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2[0];
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase[0];
	}
	else {
		return EFAULT;
	}
#else
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;

	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}
#endif
	/* make sure it's page-aligned */
	// kprintf("%x\n", paddr); 
	// kprintf("%x\n", PAGE_FRAME); 	
	 
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	#if OPT_A3
		if(is_code && as->loadelf_done){
			elo &= ~TLBLO_DIRTY;
		}
	#endif 

		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

#if OPT_A3
	// allow full tlb to work 
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	if(is_code && as->loadelf_done){
		elo &= ~TLBLO_DIRTY;
	} 
	tlb_random(ehi, elo);
	splx(spl);
	return 0;
#else
	kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
#endif
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
				// kprintf("KM addrspace\n");

	if (as==NULL) {
		return NULL;
	}

	
#if OPT_A3
	as->loadelf_done = false;
	as->as_pbase1 = NULL;
	as->as_pbase2 = NULL;
	as->as_stackpbase = NULL;
#else
	as->as_pbase1 = 0;
	as->as_pbase2 = 0;
	as->as_stackpbase = 0;
#endif
as->as_vbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_npages2 = 0;
	return as;
}

void
as_destroy(struct addrspace *as)
{
	#if OPT_A3
	for(size_t i = 0; i < as->as_npages1; i++){
		free_kpages(PADDR_TO_KVADDR(as->as_pbase1[i]));
	}
	kfree(as->as_pbase1); 
	for(size_t i = 0; i < as->as_npages2; i++){
		free_kpages(PADDR_TO_KVADDR(as->as_pbase2[i]));
	}
		kfree(as->as_pbase2); 

	for(size_t i = 0; i < DUMBVM_STACKPAGES; i++){
		free_kpages(PADDR_TO_KVADDR(as->as_stackpbase[i]));
	}
		kfree(as->as_stackpbase); 

	#endif
	kfree(as);
	// KASSERT(as == NULL);
		// kprintf("freeing as\n");
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
		// kprintf("as_define_region\n");

	size_t npages; 

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;

	#if OPT_A3
		as->as_pbase1 = kmalloc(sizeof(paddr_t)* npages);
			// kprintf("KM as->as_pbase1\n");

		if(as->as_pbase1 == NULL){
			return ENOMEM;
		}
	#endif

		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;

	#if OPT_A3
		as->as_pbase2 = kmalloc(sizeof(paddr_t)* npages);
					// kprintf("KM as->as_pbase2\n");

		if(as->as_pbase2 == NULL){
			return ENOMEM;
		}
	#endif

		return 0;
	}

	
	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
	// kprintf("as_prepare_load\n");
#if OPT_A3
	// KASSERT(as->as_pbase1 == NULL);  	//set by as_create()
	// KASSERT(as->as_pbase2 == NULL); 	//set by as_create()
	// KASSERT(as->as_stackpbase != NULL);
	for(size_t i = 0; i < as->as_npages1; i++){
		as->as_pbase1[i] = getppages(1);
					// kprintf("GPP as_pbase1[i]\n");

		if (as->as_pbase1 == 0) {		// possible to not have enough mem for getppages
			return ENOMEM;
		}
		as_zero_region(as->as_pbase1[i], 1);
	}

	for(size_t i = 0; i < as->as_npages2; i++){
		as->as_pbase2[i] = getppages(1);
					// kprintf("GPP as_pbase2\n");

		if (as->as_pbase2 == 0) {
			return ENOMEM;
		}
		as_zero_region(as->as_pbase2[i], 1);	
	}	

	as->as_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);
				// kprintf("KM as->as_stackpbase\n");

	if(as->as_stackpbase == NULL){
		return ENOMEM;
	}
	
	for(size_t i = 0; i < DUMBVM_STACKPAGES; i++){
		as->as_stackpbase[i] = getppages(1);
			// kprintf("GPP as->as_stackpbase[i]\n");

		if (as->as_stackpbase == 0) {
			return ENOMEM;
		}
		as_zero_region(as->as_stackpbase[i], 1);
	} 
 
#else

	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
		// kprintf("GPP as_PL = as->as_pbase1\n");

	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
			// kprintf("GPP as_PL = as->as_npages2\n");

	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
			// kprintf("GPP as+PL = DUMBVM_STACKPAGES\n");

	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}
	
	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);
#endif
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	// kprintf("as_complete_load\n");
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	// kprintf("as_define_stack\n");

	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	// kprintf("as_copy\n");
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

#if OPT_A3
	new->as_pbase1 = kmalloc(sizeof(paddr_t) * old->as_npages1);
	new->as_pbase2 = kmalloc(sizeof(paddr_t) * old->as_npages2);
	new->as_stackpbase = kmalloc(sizeof(paddr_t) * DUMBVM_STACKPAGES);
			// kprintf("KM new->as_pbase1\n");
			// kprintf("KM new->as_pbase2\n");
			// kprintf("KM new->as_stackpbase\n");

#endif
	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

#if OPT_A3

	KASSERT(new->as_pbase1 != NULL);
	KASSERT(new->as_pbase2 != NULL);
	KASSERT(new->as_stackpbase != NULL);

	for(size_t i = 0; i < old->as_npages1; i++){
		memmove((void *)PADDR_TO_KVADDR(new->as_pbase1[i]),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1[i]),
		PAGE_SIZE);
	}

	for(size_t i = 0; i < old->as_npages2; i++){
		memmove((void *)PADDR_TO_KVADDR(new->as_pbase2[i]),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2[i]),
		PAGE_SIZE);
	}

	for(size_t i = 0; i < DUMBVM_STACKPAGES; i++){
		memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase[i]),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase[i]),
		PAGE_SIZE);
	}
	
#else
	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);


	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);
#endif
	*ret = new;
	return 0;
}
