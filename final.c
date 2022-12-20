#include <stdio.h>
#include "simplebully.h"
#include <unistd.h>
double TX_PROB = 1.0 - ERROR_PROB;		// probability of transmitting a packet successfully

unsigned long int get_PRNG_seed() {
	struct timeval tv;
	gettimeofday(&tv,NULL);	
	unsigned long time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec + getpid();

	return time_in_micros;
}

bool is_timeout(time_t start_time) {
	// YOUR CODE GOES HERE
	time_t endtime;
    double diff_t;
    time(&endtime);
    diff_t = difftime(endtime,start_time);
    //printf("Waiting time = %f\n", diff_t);
    return diff_t>TIME_OUT_INTERVAL;
}


bool will_transmit() {
	// YOUR CODE GOES HERE
	float random = rand() / (float) RAND_MAX;
	return random>TX_PROB;
	
}
bool try_leader_elect() {
	// first toss a coin: if prob > 0.5 then attempt to elect self as leader
	// Otherwise, just keep listening for any message
	double prob = rand() / (double) RAND_MAX;
	bool leader_elect = (prob > THRESHOLD);
	return leader_elect;
}


int main(int argc, char *argv[]) {


	int myrank, np;
	int current_leader = 0;								// default initial leader node

	//////////////////////////////////
	// YOUR CODE GOES HERE
	int *recv_buf = (int *) malloc(2 * sizeof(int));
	int hello = HELLO_MSG;
	int option_val = 0;
	while((option_val =getopt(argc,argv,"c:r:t:hf:"))!=-1){
		switch(option_val)
		{
			case 'c':
				if(optarg!=NULL){
					current_leader=atoi(optarg);
				}
				printf("current_leader arg\n");
				break;
			case 'r':
				if(optarg!=NULL){
					MAX_ROUNDS=atoi(optarg);
				}
				printf("max round arg\n");
				break;

			case 't':
				if(optarg!=NULL){
					TX_PROB=atof(optarg);
				}
				printf("prob arg\n");
				break;

			case 'h':
				printf("Enter mpirun -np <output file name> <no. of processor> -c <current leader> -r <max round> -t <successful packet transfer probability>\n");

				printf("Enter -h for help\n");
				printf("Enter -c followed by integer to assign current leader \n");
				printf("Enter -r followed by integer to set Max Round\n");
				printf("Enter -t followed by float value less than 1 to set probability of transmitting a packet successfully\n");
				return 0;
				
			case '?':
				return 1;

			default:
				abort();
		}
	}

	// leader
	//current_leader = atoi(argv[1]);
	//round
	//MAX_ROUNDS = atoi(argv[2]);
	//success/failure probability
	//TX_PROB = atof(argv[3]);
	/////////////////////////////////

	printf("\n*******************************************************************");
	printf("\n*******************************************************************");
	printf("\n Initialization parameters:: \n\tMAX_ROUNDS = %d \n\tinitial leader = %d \n\tTX_PROB = %f\n", MAX_ROUNDS, current_leader, TX_PROB);
	printf("\n*******************************************************************");
	printf("\n*******************************************************************\n\n");

	// YOUR CODE FOR MPI Initiliazation GOES HERE 
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(comm, &myrank);
	MPI_Comm_size(comm, &np);

    MPI_Status status;

	srand(get_PRNG_seed());		// HINT: COMMENT THIS LINE UNTIL YOU ARE SURE THAT YOUR CODE IS CORRECT. THIS WILL AID IN THE DEBUGGING PROCESS
	
	// YOUR CODE FOR SETTING UP succ and pred GOES HERE

	int succ, pred;
	int mytoken;

	if (myrank == 0)
	{
			pred = np - 1;
	}
	else
	{
			pred = myrank - 1;
	}
	if (myrank == (np - 1))
	{
			succ = 0;
	}
	else
	{
			succ = myrank + 1;
	}

	for (int round = 0; round < MAX_ROUNDS; round++) {
		printf("\n*********************************** ROUND %d ******************************************\n", round);

		
		if (myrank == current_leader) {
            if (try_leader_elect()) {
                mytoken = generate_token();
                recv_buf[0] = myrank;
                recv_buf[1] = mytoken;

                MPI_Send(recv_buf, 2, MPI_INT, succ, LEADER_ELECTION_MSG_TAG, comm);
                printf("\n[rank %d][%d] SENT LEADER ELECTION MSG to node %d with TOKEN = %d, tag = %d\n", myrank, round, succ, mytoken, LEADER_ELECTION_MSG_TAG);
                fflush(stdout);
			} else {
				MPI_Send(&hello, 1, MPI_INT, succ, HELLO_MSG_TAG, comm);
				printf("\n[rank %d][%d] SENT HELLO MSG to node %d with TOKEN = %d, tag = %d\n", myrank, round, succ, mytoken, HELLO_MSG_TAG);
				fflush(stdout);
			}

			MPI_Request request;
			int recv_buf[2];
			int flag = 0;
			int flag1 =0;
			int flag2= 0;
			MPI_Status status1, status2;
			time_t recvStart;
			time(&recvStart);
			
			MPI_Irecv(recv_buf, 2, MPI_INT, pred, MPI_ANY_TAG, comm, &request);
            while (!flag)
			{				
				MPI_Test(&request, &flag, &status);
				if(is_timeout(recvStart)){
					MPI_Cancel(&request);
					break;
				}
                
			}
			
			if (flag) {
				switch (status.MPI_TAG ) {
					case HELLO_MSG_TAG:
						printf("\n[rank %d][%d] HELLO MESSAGE completed ring traversal!\n", myrank, round);
						fflush(stdout);
						break;
					case LEADER_ELECTION_MSG_TAG:
						current_leader = recv_buf[0];
						MPI_Send(recv_buf,	2, MPI_INT, succ, LEADER_ELECTION_RESULT_MSG_TAG, comm);
						MPI_Recv(recv_buf, 2, MPI_INT, pred, LEADER_ELECTION_RESULT_MSG_TAG, comm, MPI_STATUS_IGNORE);
						printf("\n[rank %d][%d] NEW LEADER FOUND! new leader = %d, with token = %d\n", myrank, round, current_leader, recv_buf[1]);
						fflush(stdout);
						break;
					default: ;
				}
			}
		} else {
			int flag = 0;
			time_t nonleadertime;
			time(&nonleadertime);
			while (!flag)
			{
				MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &flag, &status);
				if(is_timeout(nonleadertime)){
					printf("\n[rank %d][%d] timeout\n",myrank, round);
					flag=0;
					break;
				}
			}

			if (flag) {

				MPI_Recv(recv_buf, 2, MPI_INT, pred, MPI_ANY_TAG, comm, &status);

				if (status.MPI_TAG == HELLO_MSG_TAG) {
					if(!will_transmit()){
							printf("\n[rank %d][%d] Generated random number is less than TX_PROB, stopped forwarding HELLO MSG",myrank,round);							
					}
					else{
						MPI_Send(&hello, 1, MPI_INT, succ, HELLO_MSG_TAG, comm);
						printf("\n[rank %d][%d] Received and Forwarded HELLO MSG to next node = %d\n", myrank, round, succ);
						fflush(stdout);

					}
				} else if (status.MPI_TAG == LEADER_ELECTION_MSG_TAG) {
					if (try_leader_elect()) {
						mytoken = generate_token();
						if ((mytoken > recv_buf[1]) || ((mytoken == recv_buf[1]) && (myrank > recv_buf[0])))
						{
								recv_buf[0] = myrank;
								recv_buf[1] = mytoken;
						}
						printf("\n[rank %d][%d] My new TOKEN = %d\n", myrank, round, mytoken);
						fflush(stdout);
					} else {
							printf("\n[rank %d][%d] Will not participate in Leader Election.\n", myrank, round);
							fflush(stdout);
					}

					MPI_Send(recv_buf, 2,	MPI_INT, succ, LEADER_ELECTION_MSG_TAG, comm);

					MPI_Recv(recv_buf, 2, MPI_INT, MPI_ANY_SOURCE, LEADER_ELECTION_RESULT_MSG_TAG, comm, MPI_STATUS_IGNORE);

					current_leader = recv_buf[0];

					printf("\n[rank %d][%d] NEW LEADER :: node %d with TOKEN = %d\n", myrank, round, current_leader, recv_buf[1]);
					fflush(stdout);


					MPI_Send(recv_buf, 2, MPI_INT, succ, LEADER_ELECTION_RESULT_MSG_TAG, comm);
				}
			}
		}
		// End of steps, each node issues a MPI_Barrier()
		MPI_Barrier(comm);
	}
	printf("\n** Leader for NODE %d = %d\n", myrank, current_leader);


	MPI_Finalize();
	return 0;
}