#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* for fork */
#include <sys/types.h> /* for pid_t */
#include <sys/wait.h> /* for wait */
#include <sys/select.h> /*for select*/
#include <poll.h> /*for poll*/
#include <sys/epoll.h> /*for epoll*/
#include <errno.h>
#include <string.h>

#define MAXEVENTS 64

// This code conver an int to string. I had to implement
// this as sprintf would cause some issues in printing 
// on STDOUT.
void tostring(char str[], int num, int len)
{
    int i, rem, n;
 
    n = num;
    
    for (i = 0; i < len; i++)
    {
        rem = num % 10;
        num = num / 10;
        str[len - (i + 1)] = rem + '0';
    }
    str[len] = '\0';
}


int main(int argc, char *argv[])
{
	float result, ret_val; //Store the final result and value returned after every read
	fd_set pipes_fd_set; // for select()

	FD_ZERO(&pipes_fd_set); // initialise the fd_set

   	if (argc < 9)
    {
    	printf("Please provide correct number of arguments\n");
		printf("--worker_path <path of the worker binary>\n");
		printf("--wait_mechanism <the mechanism to wait for process>\n");
		printf("-x <value of x>\n");
		printf("-n <value of n>\n");
		exit(EXIT_FAILURE);
    }


    if ((strcmp(argv[4], "sequential") != 0) && (strcmp(argv[4], "select") != 0) && 
    	(strcmp(argv[4], "epoll") != 0) && (strcmp(argv[4], "poll") != 0))
    {
    	printf("Incorrect mechanism\n");
    	exit(EXIT_FAILURE);
    }

	int total_workers = atoi(argv[8]) + 1; // total number of workers
	int pipes[total_workers][2];
	pid_t *children = (pid_t *)malloc(total_workers * sizeof(pid_t));
	int worker_number = 0;
	int x = atoi(argv[6]);

	

	for (worker_number =0; worker_number < total_workers; worker_number++)
	{
		/*Initialise pipes for a worker*/
		if (pipe(pipes[worker_number]) < 0)
		{
			perror("Pipe creation error");
			exit(EXIT_FAILURE);
		}

		/*Fork a process*/
		children[worker_number] = fork();
		if (children[worker_number] == -1)
		{
			perror("Forking error");
			exit(EXIT_FAILURE);
		}
		/*Execute a worker on the forked process*/
		if (children[worker_number] == 0)
		{	
			/*In worker*/
			int temp = worker_number, len = 0;
			while (temp != 0)
    		{
        		len++;
        		temp /= 10;
    		} /*Compute length of 'n'*/

    		char *notostr = (char *)malloc(len * sizeof(char));
			tostring(notostr, worker_number, len); /*Convert worker_number to string*/
			
			close(STDOUT_FILENO); /*close this fd, so that it can be duped*/
			int d = dup2(pipes[worker_number][1], STDOUT_FILENO);/*duplicate write end of pipe on STDOUT_FILENO*/
			if (d == -1)
				fprintf(stderr, "error%s\n", strerror(errno));
			
			close(pipes[worker_number][0]); /*Close the read end*/
			close(pipes[worker_number][1]); /*will no longer be used*/
			char *arg[] = {argv[2], "-x", argv[6],"-n", notostr, (char *)NULL}; /*arg list for worker*/
			execvp(argv[2], arg);/*start worker*/
		}
	}

	//cloase all write ends of all pipes
  	for (worker_number = 0; worker_number < total_workers; worker_number++)
  		close(pipes[worker_number][1]);

	
	if (strcmp(argv[4],"sequential") == 0)
	{
		worker_number = 0;
		while(worker_number < total_workers)
		{
			int status;
			waitpid(children[worker_number], &status, 0);
			if (status != 0)
			{
				printf("Error in worker: %d\n", worker_number);
				exit(EXIT_FAILURE);
			}

			ssize_t value = read(pipes[worker_number][0], &ret_val, sizeof(float));
			if (value == -1)
			{
				fprintf(stderr, "Read fail%s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}

			printf("worker %d: %d^%d / %d! : %f\n", worker_number, x, worker_number, worker_number, ret_val);
			close(pipes[worker_number][0]);
			result += ret_val; //add to the final result 
			worker_number++;
		}
	}
	else if (strcmp(argv[4],"select") == 0)
	{
		int ret;
		
		result = 0;

		int max_fd = pipes[0][0];
   		for (int c = 1; c < total_workers; c++)
		{
    		if (pipes[c][0] > max_fd)
       		max_fd = pipes[c][0];
  		}/*Compute the greatest value of fd*/
  		
  		/*timeout*/
  		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		
		int counter = 0;
		int logger[total_workers];//to maintain list of fd already read once

		for (worker_number = 0; worker_number < total_workers; worker_number++)
		{
			/*Initialise array and add pipe to fd_set*/
			logger[worker_number] = 0;
			FD_SET(pipes[worker_number][0], &pipes_fd_set);
		}

		while(counter < total_workers )
		{
			ret = select(max_fd + 1, &pipes_fd_set, NULL, NULL, &timeout);
			if (ret == 0)
			{
				printf("Timeout...\n");
				exit(EXIT_FAILURE);
			}
			else if (ret < 0)
			{
    			fprintf(stdout, "Error%s\n", strerror(errno));
    			exit(EXIT_FAILURE);
			}
			else
			{
				for (worker_number = 0; worker_number < total_workers; worker_number++)
				{
					int tester = FD_ISSET(pipes[worker_number][0], &pipes_fd_set);
					if (tester)
					{
						ssize_t value = read(pipes[worker_number][0], &ret_val, sizeof(float));
						if (value == -1)
						{
							fprintf(stderr, "Read fail%s\n", strerror(errno));
							exit(EXIT_FAILURE);
						}

						printf("worker %d: %d^%d / %d! : %f\n", worker_number, x, worker_number, worker_number, ret_val);
						result += ret_val;
						counter++;
						logger[worker_number] = 1;
					}
				}
				FD_ZERO(&pipes_fd_set); //Clear fd_set
				for (worker_number = 0; worker_number < total_workers; worker_number++)
				{
					//add fd to fd_set if not read.
					if (logger [worker_number] == 0)
						FD_SET(pipes[worker_number][0], &pipes_fd_set);
				}
			}
		}
	}
	else if (strcmp(argv[4],"poll") == 0)
	{
		/* code */
		int ret, counter = 0, logger[total_workers];

		result = 0;
		
		struct pollfd poll_fd_set[total_workers];//fd_set for polling
		
		for (worker_number = 0; worker_number < total_workers; worker_number++)
		{
			//Initialise all the instance of poll_fd_set 
			poll_fd_set[worker_number].fd = pipes[worker_number][0];
			poll_fd_set[worker_number].events = POLLIN;
			poll_fd_set[worker_number].revents = 0;
			logger[worker_number] = 0;
		}
		while(counter < total_workers)
		{

			ret = poll(poll_fd_set, total_workers, 500);
			if (ret == 0)
			{
				printf("Timeout...\n");
				exit(EXIT_FAILURE);
			}
			else if (ret < 0)
			{
				fprintf(stderr, "Error during poll %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			else
			{
				for (worker_number = 0; worker_number < total_workers; worker_number++)
				{
					if ((poll_fd_set[worker_number].revents & POLLIN) && (logger[worker_number] == 0))
					{
						//read on POLLIN
						ssize_t value = read(pipes[worker_number][0], &ret_val, sizeof(float));
						if (value == -1)
						{
							fprintf(stderr, "Read fail%s\n", strerror(errno));
							exit(EXIT_FAILURE);
						}

						printf("worker %d: %d^%d / %d! : %f\n", worker_number, x, worker_number, worker_number, ret_val);
						logger[worker_number] = 1;
						counter++;
						result += ret_val;
					}
					
				}
			}
		}	
		
	}
	else if (strcmp(argv[4],"epoll") == 0)
	{
		int epfd, ep_ctl, status, logger[total_workers], counter = 0;
		struct epoll_event event[total_workers]; //event for every fd
		struct epoll_event *events; // number of events to listen
		events = malloc(MAXEVENTS * sizeof(struct epoll_event));

		ret_val = 0;

		for (worker_number = 0; worker_number < total_workers; worker_number++)
			logger[worker_number] = 0;

		epfd = epoll_create(total_workers);
		if (epfd < 0)
		{
			fprintf(stderr, "Epolling error %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		for (worker_number = 0; worker_number < total_workers; worker_number++)
		{
			event[worker_number].data.fd = pipes[worker_number][0];
			event[worker_number].events = EPOLLIN;
			ep_ctl = epoll_ctl(epfd, EPOLL_CTL_ADD, pipes[worker_number][0], event + worker_number);
			if (ep_ctl == -1)
			{
				fprintf(stderr, "Adding pipe error %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		while(counter < total_workers)
		{
			status = epoll_wait(epfd, events, MAXEVENTS, 500);
			if (status == 0)
			{
				printf("Timeout...\n");
				exit(EXIT_FAILURE);
			}
			else if (status == -1)
			{
				fprintf(stderr, "EPOLL WAIT ERROR %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			else
			{	
				for (worker_number = 0; worker_number < total_workers; worker_number++)
				{
					for (int i = 0; i < status; i++)
					{
						if ((pipes[worker_number][0] == events[i].data.fd) && (logger[worker_number] == 0))
						{
							ssize_t value = read(pipes[worker_number][0], &ret_val, sizeof(float));
							if (value == -1)
							{
								fprintf(stderr, "Read fail%s\n", strerror(errno));
								exit(EXIT_FAILURE);
							}
							counter++;
							result += ret_val;
							printf("worker %d: %d^%d / %d! : %f\n", worker_number, x, worker_number, worker_number, ret_val);
							logger[worker_number] = 1;
							break;
						}
					}
				}
			}
		}
	}
	printf("The final result is: %f\n", result);
}