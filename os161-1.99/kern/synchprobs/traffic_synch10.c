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
static struct lock * cv_mutex; 

static int volatile currDir = -1; 
static int volatile queue[3]= {-1, -1, -1};
static int volatile numCars[4]; // numCars in each direction 

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
		kprintf("no CV chosen %d \n", cv_num); 
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

  cv_mutex = lock_create("cv_mutex");
  if(cv_mutex == NULL){
  	panic("could not create cv_mutex lock\n");
  }
  currDir = -1; // -1 means unset 


  kprintf("done initializing\n"); 
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
  kprintf("starting cleanup\n");
  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL); 
  KASSERT(cv_mutex != NULL);

  cv_destroy(cv_0); 
  cv_destroy(cv_1);
  cv_destroy(cv_2); 
  cv_destroy(cv_3);   
  lock_destroy(cv_mutex);
  kprintf("ending cleanup\n");
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
  KASSERT(cv_mutex != NULL);
  
  lock_acquire(cv_mutex);
  
  kprintf("before entry of %d: N:%d E:%d S:%d W:%d \n", (int)origin, numCars[0], numCars[1], numCars[2], numCars[3]);  

  kprintf("entering car: %d -> %d\n", origin, destination); 
  // case 1: can go right now since intersection is unset 
  if(currDir == -1){
      currDir = origin; // go 
  // case 2: can go right now since current direction of intersection matches vehicle direction 
  } else if (currDir == (int)origin){
  
  // case 3: must wait until it is woken up by another car leaving the intersection (from another direction) that hands off the intersection to current car's direction
  } else {
    // add the car's direction to waiting queue if the direction is not already in it 
    if (queue[0] != (int)origin && queue[1] != (int)origin && queue[2] != (int)origin){
      // find the first available position that the queue can occupy 
      int availablePos = 2; 
      if (queue[1] == -1){
        availablePos = 1; 
      } 
      if(queue[0] == -1){
        availablePos = 0;
      }
      
      queue[availablePos] = (int)origin;
      kprintf("added to queue %d %d %d \n", queue[0], queue[1], queue[2]);
    }
   
    // go to sleep 
    cv_wait(getCV((int)origin), cv_mutex); // wait
    kprintf("waiting for intersection to clear: %d -> %d\n", origin, destination); 
  }

  numCars[(int)origin]++; 

  lock_release(cv_mutex);

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
  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(cv_mutex != NULL);

  lock_acquire(cv_mutex); 
  kprintf ("exiting car: %d ->  %d\n", origin, destination);
  
  numCars[(int)origin]--; 

  if(numCars[(int)origin] == 0){
    kprintf("handing off intersection from %d\n", (int)origin);
    // case 1: no cars waiting somewhere else 
    if(queue[0] == -1){
      currDir = -1;
    // case 2: at least 1 car waiting somewhere else
    } else {

      // remove head of the queue 
      currDir = queue[0]; // guaranteed to not equal the previous currDir
      // shift all the elements of the queue over  
      queue[0] = queue[1];
      queue[1] = queue[2];
      queue[2] = -1; 
      kprintf("removed %d from queue. now %d %d %d \n", currDir, queue[0], queue[1], queue[2]);

      cv_broadcast(getCV(currDir), cv_mutex);
      kprintf("waking up all cars in %d\n", currDir);  // why won't this allow all the cars from currDir direction to go before releasing the lock in line 232? 
    }
  }
  
  kprintf("after exit of %d: N:%d E:%d S:%d W:%d \n", (int)origin, numCars[0], numCars[1], numCars[2], numCars[3]);  
  lock_release(cv_mutex); 
  return;


}
 
