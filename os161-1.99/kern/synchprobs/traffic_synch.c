#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <queue.h>
/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */

static struct cv * cv_0; 
static struct cv * cv_1;
static struct cv * cv_2; 
static struct cv * cv_3; 
static struct lock * lk_mutex; 

static int volatile currDir; 
static int volatile numCars; // numCars in currDir
static int volatile waitingCars[4]; 

// queue 
// static int volatile waitQueue[4];  //volatile
static struct queue * waitQueue;  //volatile
static bool volatile inQueue [4];

struct cv * getCV(int cv_num);

struct cv * getCV(int cv_num){
	struct cv *chosen;
	if(cv_num == 0){
		chosen = cv_0;
	} else if(cv_num == 1){
		chosen = cv_1;
	} else if(cv_num == 2){
		chosen = cv_2;
	} else if(cv_num == 3){
		chosen = cv_3;
	} else {
		return NULL;
		//kprintf("no CV chosen %d \n", cv_num); 
	}
	return chosen;
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{

  cv_0 = cv_create("cv0");  
  if(cv_0 == NULL){
    panic("could not create cv0\n");
  }

  cv_1 = cv_create("cv1");  
  if(cv_1 == NULL){
    panic("could not create cv1\n");
  }
  cv_2 = cv_create("cv2");  
  if(cv_2 == NULL){
    panic("could not create cv2\n");
  }
  cv_3 = cv_create("cv3");  
  if(cv_3 == NULL){
    panic("could not create cv3\n");
  }

  lk_mutex = lock_create("lk_mutex");
  if(lk_mutex == NULL){
  	panic("could not create lk_mutex lock\n");
  }

  // for(int i = 0; i < 3; i++){
  //   queue[i] = -1; 
  // }
  waitQueue = q_create(4);
  if(waitQueue == NULL){
    panic("could not create waitQueue queue\n");
  }

  currDir = -1; // -1 means unset 
  
  // queueSize = 0; 

  //kprintf("done initializing\n"); 
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  //kprintf("starting cleanup\n");
  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL); 
  KASSERT(lk_mutex != NULL);
  KASSERT(waitQueue != NULL);

  cv_destroy(cv_0); 
  cv_destroy(cv_1);
  cv_destroy(cv_2); 
  cv_destroy(cv_3);   
  lock_destroy(lk_mutex);
  q_destroy(waitQueue);

  // kprintf("ending cleanup\n");
  return; 

}

/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  (void)destination; 

  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(lk_mutex != NULL);
  // KASSERT(waitQueue != NULL);

  lock_acquire(lk_mutex);
  
  // kprintf("before entry from %d :  %d cars", (int)origin, numCars);  

  //kprintf("entering car: %d -> %d\n", origin, destination); 

  // case 1: can go right now since intersection is unset 
  if(currDir == -1){
      currDir = origin; 
      // go 

  // case 2: can go right now since current direction of intersection matches vehicle direction 
  } else if (currDir == (int)origin){
    // go 

  // case 3: must wait until it is woken up by another car leaving the intersection (from another direction) that hands off the intersection to current car's direction
  } else {
   
    // go to sleep 
    waitingCars[(int)origin]++;

    // add to queue if not already in it
    if(!inQueue[(int)origin]){

      int* dir = (int *)kmalloc(sizeof(int));
      *dir = (int)origin; 
      q_addtail(waitQueue, (void*)(dir));
      inQueue[(int)origin] = true; 
      // kprintf("adding: queue length: %d,  head: %d \n", q_len(waitQueue), *dir);
    }

    cv_wait(getCV((int)origin), lk_mutex); // wait
    //kprintf("waiting for intersection to clear: %d -> %d\n", origin, destination); 

    waitingCars[(int)origin]--;

    // remove from queue if no more cars in that direction 
    if(!q_empty(waitQueue)){
      int* dir = (int *)q_peek(waitQueue);    // need to free this?

      if(waitingCars[*dir] == 0){
        q_remhead(waitQueue);
        // kprintf("removing: queue length: %d,  head: %d \n", q_len(waitQueue), *dir);
        inQueue[*dir] = false; 
        kfree(dir);   
      }
    }
  }

  // at this point, car is able to go through now 
  numCars++; 

  lock_release(lk_mutex);
  return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  (void)origin;
  (void)destination;
  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(lk_mutex != NULL);
  KASSERT(waitQueue != NULL);

  lock_acquire(lk_mutex); 
  //kprintf ("exiting car: %d ->  %d\n", origin, destination);
  
  numCars--; 

  if(numCars == 0){
    currDir = -1;
    // kprintf("handing off intersection from %d\n", (int)origin);

    
    if(!q_empty(waitQueue)){

      int* dir = (int *)q_peek(waitQueue);    // need to free this?
      // kprintf("handing off intersection to %d\n", *dir);
      
      currDir = *dir; 
      cv_broadcast(getCV(currDir), lk_mutex);
      // kprintf("waking up all cars in %d\n", currDir);  
    }

    /*
    for (int i = 1; i <= 3; i++){
      if(waitingCars[(origin+i)%4] > 0){
        currDir = (origin+i)%4;
        cv_broadcast(getCV(currDir), lk_mutex);
        //kprintf("waking up all cars in %d\n", currDir);  
        break;
      }
    } */
  }
  
  // //kprintf("after exit: %d remaining from %d \n",  numCars, (int)origin);  
  lock_release(lk_mutex); 
  return;


}
 
