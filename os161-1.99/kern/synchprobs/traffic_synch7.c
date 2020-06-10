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

static volatile int  fromDir [100]; //index 0 represents north, 1 is east, 2 is south, 3 is west)
static volatile int  toDir [100]; //index 0 represents north, 1 is east, 2 is south, 3 is west)
static volatile queue * ;
static struct lock * ts_mutex; 
static struct cv * ts_cv ; // from north (origin direction is north)
static int numCars;
bool canEnter(Direction origin, Direction destination);

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

  ts_cv = cv_create("cvts");  
  if(ts_cv == NULL){
    panic("could not create cv\n");
  }
  ts_mutex = lock_create("ts_mutex");
  if(ts_mutex == NULL){
  	panic("could not create ts_mutex lock\n");
  }
  numCars=0;

  
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
  KASSERT(ts_cv != NULL);

  cv_destroy(ts_cv); 
  lock_destroy(ts_mutex);
  
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

bool canEnter(Direction origin, Direction destination){
  for (int i = 0 ; i < numCars; i++){
    // if not equal orig vehicle in queue, not same origin, not opp directions, and neither right turning, then false
    if(  !(fromDir[i] == (int)origin  && toDir[i] == (int)destination)  // same vehicle 
      && !(fromDir[i] == (int)origin)   // same origin 
      && !(fromDir[i] == (int)destination && toDir[i] == (int)origin) //opposite directions 
      && !(((origin+3)%4 == destination%4 || (fromDir[i]+3)%4 == (toDir[i]+3)%4) && (int)destination != toDir[i])) {  // diff destinations and one is right turning
      return false; 
    }
  }
  return true; 
}
void
intersection_before_entry(Direction origin, Direction destination) 
{
  (void)destination; 
  kprintf("entering car: %d -> %d\n", origin, destination); 

  KASSERT(ts_cv != NULL);
  KASSERT(ts_mutex != NULL);
  KASSERT(numCars >= 0 && numCars < 100);

  lock_acquire(ts_mutex);
  //if blocked, wait.
  while(!canEnter(origin, destination)){
	  cv_wait(ts_cv, ts_mutex);
  } 
  fromDir[numCars] = (int)origin; 
  toDir[numCars] = (int)destination;
  numCars++;  
  lock_release(ts_mutex);

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
  KASSERT(ts_cv != NULL);
  KASSERT(ts_mutex != NULL);
  KASSERT(numCars >= 0 && numCars < 100);

  kprintf ("exiting car: %d ->  %d\n", origin, destination);
  
  lock_acquire(ts_mutex);
  
  // wake up cars from another direction if no more cars from current direction
  // find currCar in queue 
  int carIndex = -1;
  for (int i = 0; i < numCars; i++){
    if((int)origin == fromDir[i] && (int)destination == toDir[i]){
      carIndex = i;
    }  
  }
  // remove from head of queue
  for (int i = carIndex; i < numCars; i++){
    fromDir[i] = fromDir[i+1];
    toDir[i] = toDir[i+1];
  }
  numCars--;
  cv_broadcast(ts_cv, ts_mutex);
  lock_release(ts_mutex); 
  return;


}
 
