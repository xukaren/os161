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

static struct cv * cv_0; // from north (origin direction is north)
static struct cv * cv_1; // from east
static struct cv * cv_2; // from south
static struct cv * cv_3; // from west
static  int * volatile  numCars [4]; //index 0 represents north, 1 is east, 2 is south, 3 is west)
static struct lock * cv_mutex; 

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
  kprintf("entering car: %d -> %d\n", origin, destination); 

  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(cv_mutex != NULL);
  
  lock_acquire(cv_mutex);
  //if blocked, wait.
  while(numCars[(origin+1)%4] != 0 || numCars[(origin+2)%4] != 0 || numCars[(origin+3)%4] != 0){
	  cv_wait(getCV(origin%4), cv_mutex);
  } 
  numCars[origin%4]++; ; 
  //kprintf("N:%d E:%d S:%d W:%d and releasing lock in before\n", (int)numCars[0], (int)numCars[1], (int)numCars[2], (int)numCars[3]);  
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

  kprintf ("exiting car: %d ->  %d\n", origin, destination);
  
  lock_acquire(cv_mutex);
  
  numCars[origin%4]--;
 
   // wake up cars from another direction if no more cars from current direction
  for (int i = 1; i <= 3; i++){
    cv_broadcast(getCV((origin+i)%4), cv_mutex);
    kprintf("waking up cars in %d\n", (origin+i)%4);
  }
  
  //kprintf("N:%d E:%d S:%d W:%d and releasing lock in exit\n", numCars[0], numCars[1], numCars[2], numCars[3]);  
  lock_release(cv_mutex); 
  return;


}
 
