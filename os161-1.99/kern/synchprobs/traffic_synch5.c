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
static  int * volatile  numCars [4];
static  int *  volatile waitTimes [4];
static struct lock * cv_mutex; 
static int currDir = -1;

struct cv * getCV(int cv_num);

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
  //if not blocked, go. if blocked, wait.
  if((currDir !=(int) origin && currDir != -1)|| numCars[(origin+1)%4] != 0 || numCars[(origin+2)%4] != 0 || numCars[(origin+3)%4] != 0){
  	waitTimes[origin%4]++;
	cv_wait(getCV(origin%4), cv_mutex);
  }
  //reset 
  if(currDir == -1){
  	currDir = origin;
  }
  waitTimes[origin] = 0;
  numCars[origin]++;

  
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
  if(numCars[0] == 0 && numCars[1] == 0 && numCars[2] == 0 && numCars[3] == 0){
  	if(waitTimes[0]==0 && waitTimes[1]==0 && waitTimes[2]==0 && waitTimes[3]==0){
		//reset 
		currDir = -1; 
		lock_release(cv_mutex);
		return;
	}
	for (int i = 1; i <= 3; i++){
		int newDir = (origin+i)%4;
		if(waitTimes[newDir] != 0){
			currDir = newDir;
			cv_broadcast(getCV(newDir), cv_mutex);
			break;
		}
	}
  }

  lock_release(cv_mutex); 
  return;


}
