/*
 *	led.c - program to simulate cars passing along one lane ledyard bridge during 
 *			construction
 *				written for CS58, 16F
 *				Andy Werchniak
 *
 *			compiling: gcc -o led led.c -lpthread
 *
 *			usage: led [nCars]
 *			
 *			Takes one argument, nCars. If no argument is given, led treats nCars as 20.
 *			The program creates a bridge structure that can hold MAX_CARS cars at a time
 *			and only allows the cars on the bridge to be travelling the same direction.
 *			led generates nCars threads with random directions, and prints the status of
 *			the bridge to the terminal.
 *
 *
 */
 
/******** Libraries ********/
#include <stdio.h>			//printf, etc.
#include <stdlib.h>			//malloc
#include <pthread.h>		//POSIX threads
#include <unistd.h>			//sleep

/******** Preprocessor Definitions ********/
#define TO_NORWICH 1
#define TO_HANOVER 2
#define MAX_CARS 6
 
/******** Local Types ********/
typedef struct bridge
{
	int direction;
	int num_slots;
	int cars;
} bridge_t;
 
/******** Global Variables ********/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
bridge_t ledyard;

/******** Function Declarations ********/
void *OneVehicle(void *arg);
void ArriveBridge(int direction);
void OnBridge(int direction);
void ExitBridge(int direction);
void print_bridge(bridge_t bridge);

int main(int argc, char *argv[])
{
	int stat, num_cars;
	
	ledyard.direction = 0;
	ledyard.num_slots = MAX_CARS;
	ledyard.cars = 0;
	
	//get how many cars
	if(argc == 1)
		num_cars = 20;				//default: 20 cars
	else
		num_cars = atoi(argv[1]);	//otherwise user specifies
	
	pthread_t *threads = malloc(sizeof(pthread_t) * num_cars);
	int *direction = malloc(sizeof(int) * num_cars);
	
	for(int count = 0; count<(num_cars); count++){
		direction[count] = (rand()%2 ? TO_NORWICH : TO_HANOVER);
		
		stat = pthread_create(&threads[count], NULL, OneVehicle, &direction[count]);
		//printf("created thread going %s\n", direction[count]==TO_NORWICH ? "TO_NORWICH" : "TO_HANOVER");
		if(stat){
			fprintf(stderr, "pthread_create failed, rc=%d\n", stat);
			return(1);
		}
		
		//sleep(rand()%5);	//sleep anywhere from 0-4 seconds before new thread, to simulate
							//	random cars coming				
	}
	
	// Make main wait for the threads to finish by joining all of the threads
	for(int i = 0; i<(num_cars); i++) {
		pthread_join(threads[i], NULL);
	}

	//pthread_mutex_lock(&mutex);
	printf("\nFinal state: \n");
	print_bridge(ledyard);
	//pthread_mutex_unlock(&mutex);
	return(0);
}

void *OneVehicle(void *arg)
{
	int direction = *((int *) arg);
	
	ArriveBridge(direction);
    // now the car is on the bridge!

    OnBridge(direction);
	
    ExitBridge(direction);
    // now the car is off
    
    return(NULL);
}

void ArriveBridge(int direction)
{
	//printf("Car going %s arriving at bridge.\n", direction==TO_NORWICH ? "TO_NORWICH" : "TO_HANOVER");
	pthread_mutex_lock(&mutex);
	//wait while the max number of cars is on the bridge or the bridge is going the other direction
	while(ledyard.cars >= ledyard.num_slots || (ledyard.direction && ledyard.direction != direction))
		pthread_cond_wait(&cond,&mutex);
		
	//now that we're through
	ledyard.cars++;
	ledyard.direction = direction;
	
	pthread_mutex_unlock(&mutex);
}

void OnBridge(int direction)
{
	//printf("Car going %s on bridge.\n", direction==TO_NORWICH ? "TO_NORWICH" : "TO_HANOVER");
	char *direc;
	switch(direction){
		case 0:
			direc = "NO_DIRECTION";
			break;
		case TO_NORWICH:
			direc = "TO_NORWICH";
			break;
		case TO_HANOVER:
			direc = "TO_HANOVER";
			break;
		default:
			fprintf(stderr, "direction = %d.\n", direction);
	}
	
	//print the bridge, but lock the mutex first!
	pthread_mutex_lock(&mutex);
	printf("%s: ", direc);
	print_bridge(ledyard);
	pthread_mutex_unlock(&mutex);
}

void ExitBridge(int direction)
{	
	sleep(2);	//keep it on the bridge for 2 seconds before exiting
	
	//printf("exiting bridge\n");
	pthread_mutex_lock(&mutex);
	
	//update bridge structure
	if(ledyard.cars > 0)	//it never should be, but just in case...
		ledyard.cars--;
	
	if(0 == ledyard.cars)
		ledyard.direction = 0;
	
	//signal to other threads that there is a new space
	pthread_cond_broadcast(&cond);
	
	pthread_mutex_unlock(&mutex);
}

void print_bridge(bridge_t bridge)
{
	if(ledyard.cars > MAX_CARS){
		fprintf(stderr, "TOO MANY CARS! DANGER!\n");
		exit(2);
	}
	
	//print cars
	for(int i = 0; i<ledyard.cars; i++){
		printf("x");
	}

	//print spaces
	for(int i = 0; i<ledyard.num_slots-ledyard.cars; i++){
		printf("_");
	}

	//print line
	printf("\n");
}