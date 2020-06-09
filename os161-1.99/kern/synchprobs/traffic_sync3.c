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
//static struct semaphore *intersectionSem;
/*
static struct cv * from_n;
static struct cv * from_e;
static struct cv * from_s;
static struct cv * from_w;

static volatile int numDir [4]; // number of cars at intersection coming from each direction, where 0-3 correspond to NESW in that order
*/ 
//static volatile struct cv * cvDir[12]; 
static struct cv * cv_0; 
static struct cv * cv_1; 
static struct cv * cv_2; 
static struct cv * cv_3;
static struct cv * cv_4; 
static struct cv * cv_5; 
static struct cv * cv_6; 
static struct cv * cv_7;
static struct cv * cv_8; 
static struct cv * cv_9; 
static struct cv * cv_10; 
static struct cv * cv_11;


static volatile int numDir[12]; 
static struct lock * cv_mutex; //only allows all the cv's to be modified one at a time

int convertFromDir(Direction origin, Direction destination);
int convertDir(int origin, int destination);
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
  /* 
  intersectionSem = sem_create("intersectionSem",1);
    panic("could not create intersection semaphore");
  }
  */
  
  /*for (int i = 0; i < 12; i++){
    cvDir[i] = cv_create("CV"+i);
    if(!cvDir[i]){
    	panic("could not create cv" +i);
    }
  
  }
  */
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
  cv_4 = cv_create("cv4");  
  if(cv_4 == NULL){
    panic("could not create cv4\n");
  }
  cv_5 = cv_create("cv5");  
  if(cv_5 == NULL){
    panic("could not create cv5\n");
  }
  cv_6 = cv_create("cv6");  
  if(cv_6 == NULL){
    panic("could not create cv6\n");
  }
  cv_7 = cv_create("cv7");  
  if(cv_7 == NULL){
    panic("could not create cv7\n");
  }
  cv_8 = cv_create("cv8");  
  if(cv_8 == NULL){
    panic("could not create cv8\n");
  }
  cv_9 = cv_create("cv9");  
  if(cv_9 == NULL){
    panic("could not create cv9\n");
  }
  cv_10 = cv_create("cv10");  
  if(cv_10 == NULL){
    panic("could not create cv10\n");
  }
  cv_11 = cv_create("cv11");  
  if(cv_11== NULL){
    panic("could not create cv11\n");
  }
  
  /*
  from_n = cv_create("from_n"); 
  if(from_in == NULL){
    panic("could not create from_n condition variable\n");
  }

  from_e = cv_create("from_e"); 
  if(from_e == NULL){
    panic("could not create from_e condition variable\n");
  }

  from_s = cv_create("from_s"); 
  if(from_s == NULL){
    panic("could not create from_s condition variable\n");
  }

  from_w = cv_create("from_w"); 
  if(from_w == NULL){
    panic("could not create from_w condition variable\n");
  }
*/

  cv_mutex = lock_create("cv_mutex");
  if(cv_mutex == NULL){
  	panic("could not create cv_mutex lock\n");
  }

  kprintf("done initializing"); 
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
  //KASSERT(intersectionSem != NULL);
  //sem_destroy(intersectionSem);
  /*
  KASSERT(from_n != NULL);
  KASSERT(from_e != NULL);
  KASSERT(from_s != NULL);
  KASSERT(from_w != NULL);*/
  /*
  for (int i = 0; i < 12; i++){
  	KASSERT(cvDir[i] != NULL); 
	cv_destroy(&cvDir[i]);
  }*/

  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(cv_4 != NULL);
  KASSERT(cv_5 != NULL);
  KASSERT(cv_6 != NULL);
  KASSERT(cv_7 != NULL);
  KASSERT(cv_8 != NULL);
  KASSERT(cv_9 != NULL);
  KASSERT(cv_10!= NULL);
  KASSERT(cv_11!= NULL);

  KASSERT(cv_mutex != NULL);
/*
  cv_destroy(from_n); 
  cv_destroy(from_e);
  cv_destroy(from_s); 
  cv_destroy(from_w); 
  */
  cv_destroy(cv_0); 
  cv_destroy(cv_1);
  cv_destroy(cv_2); 
  cv_destroy(cv_3); 
  cv_destroy(cv_4); 
  cv_destroy(cv_5);
  cv_destroy(cv_6); 
  cv_destroy(cv_7); 
  cv_destroy(cv_8); 
  cv_destroy(cv_9);
  cv_destroy(cv_10); 
  cv_destroy(cv_11); 


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

int convertFromDir(Direction origin, Direction destination){
	if(origin == north && destination == east){
		return 0; 
	} else if (origin == north && destination == south){
		return 1;
	} else if (origin == north && destination == west){
		return 2;
	} else if (origin == east && destination == north){
		return 3;
	} else if (origin == east && destination == south){
		return 4;
	} else if (origin == east && destination == west){
		return 5;
	} else if (origin == south && destination == north){
		return 6;
	} else if (origin == south && destination == east){
		return 7;
	} else if (origin == south && destination == west){
		return 8;
	} else if (origin == west && destination == north){
		return 9;
	} else if (origin == west && destination == east){
		return 10;
	} else if(origin == west && destination == south) {
		return 11;
	} else {
		kprintf("ERROR Converting directions");
		return -1; 
	}
}
int convertDir(int origin, int destination){
	if(origin == 0 && destination == east){
		return 0; 
	} else if (origin == 0 && destination == 2){
		return 1;
	} else if (origin == 0 && destination == 3){
		return 2;
	} else if (origin == 1 && destination == 0){
		return 3;
	} else if (origin == 1 && destination == 2){
		return 4;
	} else if (origin == 1 && destination == 3){
		return 5;
	} else if (origin == 2 && destination == 0){
		return 6;
	} else if (origin == 2 && destination == 1){
		return 7;
	} else if (origin == 2 && destination == 3){
		return 8;
	} else if (origin == 3 && destination == 0){
		return 9;
	} else if (origin == 3 && destination == 1){
		return 10;
	} else if(origin == 3 && destination == 2) {
		return 11;
	} else {
		kprintf("ERROR Converting int directions");
		return -1; 
	}
}
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
		} else if(cv_num == 4){
			chosen = cv_4;
		} else if(cv_num == 5){
			chosen = cv_5;
		} else if(cv_num == 6){
			chosen = cv_6;
		} else if(cv_num == 7){
			chosen = cv_7;
		} else if(cv_num == 8){
			chosen = cv_8;
		} else if(cv_num == 9){
			chosen = cv_9;
		} else if(cv_num == 10){
			chosen = cv_10;
		} else if(cv_num == 11){
			chosen = cv_11;
		}
		return chosen;
}
void
intersection_before_entry(Direction origin, Direction destination) 
{
  kprintf("%d -> %d\n", origin, destination); 
  /*
  KASSERT(from_n != NULL);
  KASSERT(from_e != NULL);
  KASSERT(from_s != NULL);
  KASSERT(from_w != NULL);
  */
  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(cv_4 != NULL);
  KASSERT(cv_5 != NULL);
  KASSERT(cv_6 != NULL);
  KASSERT(cv_7 != NULL);
  KASSERT(cv_8 != NULL);
  KASSERT(cv_9 != NULL);
  KASSERT(cv_10!= NULL);
  KASSERT(cv_11!= NULL);
/*
  for (int i = 0; i < 12; i++){
  	KASSERT(cvDir[i] != NULL); 
  }*/
  KASSERT(cv_mutex != NULL);

  lock_acquire(cv_mutex); 
  
	
  // check if any of the current cars in the intersection have conflicts with the current car about to enter 

  bool wait = true; 
  while (wait){
  	wait = false;

	// car wants to go straight 
	if((origin+2)%4 == destination%4){
	  	// 6 possible conflicts 	
		// cannot go if there is conflict with cars moving straight  

		if(numDir[convertDir((origin+1)%4, (origin+3)%4)] >  0 || numDir[convertDir((origin+3)%4, (origin+1)%4)] > 0){
			wait = true; 
		}
		// cannot go if conflict with left turning cars 
		else if (numDir[convertDir((origin+1)%4, (origin+2)%4)] > 0 || numDir[convertDir((origin+2)%4, (origin+3)%4)] > 0 || numDir[convertDir((origin+3)%4, (origin+4)%4)] > 0){
			wait = true;
		}
		// cannot go if conflict with cars making right turns with same destination 
		else if (numDir[convertDir((origin+3)%4, (origin+2)%4)] > 0){
			wait = true;
		}

	// car wants to turn right 
	} else if ((origin+3)%4 == destination%4){
		// 2 possible conflicts	
		// cannot go if cars are going straight with same destination 
		if(numDir[convertDir((origin+1)%4, (origin+3)%4)] > 0){
			wait = true; 
		} 
		// cannot go if cars are turning left 
		else if(numDir[convertDir((origin+2)%4, (origin+3)%4)] > 0){
			wait = true;
		}

	// car wants to turn left
	} else if((origin+1)%4 == destination%4){
		// 7 possible conflicts 
		// cannot go if cars are going straight 
		if(numDir[convertDir((origin+1)%4, (origin+3)%4)] > 0 || numDir[convertDir((origin+2)%4, (origin)%4)] > 0 || numDir[convertDir((origin+3)%4, (origin+1)%4)] > 0){
			wait = true; 
		} 
		// cannot go if cars turning right 
		else if (numDir[convertDir((origin+2)%4, (origin+1)%4)] > 0){
			wait = true;
		} 
		// cannot go if cars turning left 
		else if(numDir[convertDir((origin+1)%4, (origin+2)%4)] > 0 || numDir[convertDir((origin+2)%4, (origin+3)%4)] > 0 || numDir[convertDir((origin+3)%4, origin%4)] > 0){
			wait = true;
		}
	}

	//////// 

	if(wait){
		cv_wait(getCV(convertFromDir(origin, destination))
, cv_mutex);

		
		//cv_wait(convertFromDir(origin, destination), cv_mutex);
	} else{
	  	numDir[convertFromDir(origin, destination)]++; 
	  	//break;
	}

  }
  
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
  /*
  KASSERT(from_n != NULL);
  KASSERT(from_e != NULL);
  KASSERT(from_s != NULL);
  KASSERT(from_w != NULL);
  */
  KASSERT(cv_0 != NULL);
  KASSERT(cv_1 != NULL);
  KASSERT(cv_2 != NULL);
  KASSERT(cv_3 != NULL);
  KASSERT(cv_4 != NULL);
  KASSERT(cv_5 != NULL);
  KASSERT(cv_6 != NULL);
  KASSERT(cv_7 != NULL);
  KASSERT(cv_8 != NULL);
  KASSERT(cv_9 != NULL);
  KASSERT(cv_10!= NULL);
  KASSERT(cv_11!= NULL);

  /*
  for (int i = 0; i < 12; i++){
  	KASSERT(cvDir[i] != NULL);
  }
  */
  KASSERT(cv_mutex != NULL);

  lock_acquire(cv_mutex);
  kprintf ("exiting car: %d ->  %d\n", origin, destination);
  
  numDir[convertFromDir(origin, destination)]--;
  // car wants to go straight 
  if((origin+2)%4 == destination%4){
  	// 6 possible conflicts 
  	//straight 
  	cv_broadcast(getCV(convertDir((origin+1)%4, (origin+3)%4)), cv_mutex);
  	cv_broadcast(getCV(convertDir((origin+3)%4, (origin+1)%4)), cv_mutex);
  	//left 
  	cv_broadcast(getCV(convertDir((origin+1)%4, (origin+2)%4)), cv_mutex);
  	cv_broadcast(getCV(convertDir((origin+2)%4, (origin+3)%4)), cv_mutex);
 	 cv_broadcast(getCV(convertDir((origin+3)%4, (origin+0)%4)), cv_mutex);
  	//right
  	cv_broadcast(getCV(convertDir((origin+3)%4, (origin+2)%4)), cv_mutex);

  // car wants to go right 
  } else if((origin+3)%4 == destination%4){
	// 2 possible conflicts	
	// straight 
	cv_broadcast(getCV(convertDir((origin+1)%4, (origin+3)%4)), cv_mutex);
	//left
	cv_broadcast(getCV(convertDir((origin+2)%4, (origin+3)%4)), cv_mutex); 
		
  // car wants to turn left
  } else if((origin+1)%4 == destination%4){
	// 7 possible conflicts 
	// straight 
	cv_broadcast(getCV(convertDir((origin+1)%4, (origin+3)%4)), cv_mutex);
	cv_broadcast(getCV(convertDir((origin+2)%4, (origin)%4)), cv_mutex);
	cv_broadcast(getCV(convertDir((origin+3)%4, (origin+1)%4)), cv_mutex); 
	
	// right 
	cv_broadcast(getCV(convertDir((origin+2)%4, (origin+1)%4)), cv_mutex);
	
	// left 
	cv_broadcast(getCV(convertDir((origin+1)%4, (origin+2)%4)), cv_mutex);
	cv_broadcast(getCV(convertDir((origin+2)%4, (origin+3)%4)), cv_mutex);
	cv_broadcast(getCV(convertDir((origin+3)%4, origin%4)), cv_mutex); 
  }
 
  

  /*
  if(origin == north){
    numDir[0]--;
    cv_broadcast(from_n, cv_mutex);
  } else if(origin==east){
    numDir[1]--;
    cv_broadcast(from_e, cv_mutex);
  } else if(origin == south){
    numDir[2]--;
    cv_broadcast(from_s, cv_mutex);
  } else if(origin == west){
    numDir[3]--;
    cv_broadcast(from_w, cv_mutex);
  }*/
  

  lock_release(cv_mutex); 
  return;


}
