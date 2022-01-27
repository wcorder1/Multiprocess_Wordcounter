#include "wc.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>






// Fills arrays identifying points in the file for a child to search
void fill_offset_arrays(int nChildProc, long fsize, int offsets[], int bytes_to_count[]) 
{
		// Calculates number of smaller offsets to use
		int nSmallOffsets = nChildProc - (fsize % nChildProc);
		// Base integer to offset by
		int offset_increase = fsize / nChildProc;
		// Offset starts at 0
		int offset = 0;

		for (int i=0; i<nChildProc; i++) {

				if (i < nSmallOffsets) {
						bytes_to_count[i] = offset_increase;
				} else {
						bytes_to_count[i] = offset_increase + 1;
				}

				offsets[i] = offset;
				offset += bytes_to_count[i];
		}

}






int main(int argc, char **argv)
{

		long fsize;
		FILE *fp;
		count_t count;
		struct timespec begin, end;
		int nChildProc = 0;

		/* 1st arg: filename */
		if (argc < 2) {
				printf("usage: wc <filname> [# processes] [crash rate]\n");
				return 0;
		}
		
		/* 2nd (optional) arg: number of child processes */
		if (argc > 2) {
				nChildProc = atoi(argv[2]);
				if (nChildProc < 1) nChildProc = 1;
				if (nChildProc > 10) nChildProc = 10;
		}

		/* 3rd (optional) arg: crash rate between 0% and 100%. Each child process has that much chance to crash*/
		if (argc > 3) {
				crashRate = atoi(argv[3]);
				if (crashRate < 0) crashRate = 0;
				if (crashRate > 50) crashRate = 50;
		}

		int fd[nChildProc][2];
		// Initializes arrays to store values needed for word_count()
		pid_t children[nChildProc];
		int offsets[nChildProc];
		int bytes_to_count[nChildProc];
		
		printf("# of Child Processes: %d\n", nChildProc);
		printf("crashRate RATE: %d\n", crashRate);

		count.linecount = 0;
		count.wordcount = 0;
		count.charcount = 0;

	// start to measure time
		clock_gettime(CLOCK_REALTIME, &begin);

		// Open file in read-only mode
		fp = fopen(argv[1], "r");

		if (fp == NULL) {
				printf("File open error: %s\n", argv[1]);
				printf("usage: wc <filname>\n");
				return 0;
		}
		
		// get a file size
		fseek(fp, 0L, SEEK_END);
		fsize = ftell(fp);
		fclose(fp);



		// Fills nChildProc offset arrays to assign portions of each array to pieces of the file fp
		fill_offset_arrays(nChildProc, fsize, offsets, bytes_to_count);



		// Totals to be added as child processes return
		int total_lines = 0, total_words = 0, total_chars = 0;

		// Loops through nChildProc to access each section of the file fp
		for (int i=0; i<nChildProc; i++) {

				// Opens pipe[i]
				if (pipe(fd[i]) < 0) {
						printf("Pipe failure: %s\n", argv[1]);
						return 0;
				}

				// Creates child process
				pid_t looping_process = fork();

				// Checks for fork failure
				if (looping_process < 0) {
						printf("Fork failure: %s\n", argv[1]);
						return 0;
				
				/*
				Looping process starts subprocess to count all the words in section i of file fp
				If subprocess fails then it crashed s.t. (status != 0) and a new child is created
				If subprocess succeeds then (status = 0) and looping process closes
				*/
				} else if (looping_process == 0) {

						/* 
						Open file in read-only mode
						A file must be opened FOR EACH section of the file being counted,
						Allowing multiple pointers to have access simultaneously
						*/
						fp = fopen(argv[1], "r");
						if (fp == NULL) {
								return 0;
						}

						/* 
						Creates new process until child safely exits 
						Recognized when (status = 0) because of exit(0)
						*/
						int status;
						while (status != 0) {

								// Creates child process w/ fork() for section i of file fp
								children[i] = fork();

								// Checks for fork failure
								if (children[i] < 0) {
										printf("Fork failure: %s\n", argv[1]);
										return 0;	

								// Child runs word_count on section i of file fp
								} else if (children[i] == 0) {

										//  Collects count data for specified section of file
										count = word_count(fp, offsets[i], bytes_to_count[i], crashRate);

										// Writes and closes pipe[i] for children[i]
										write(fd[i][1], &count, sizeof(count));
										close(fd[i][1]);

										// children[i] safely exits
										exit(0);
								}

								/* 
								Waits for children[i] to end to change status
								If status is changed to 0 then then while loop exits
								*/
								waitpid(children[i], &status, 0);

						} // while

						// closes opened file
						fclose(fp);
						// looping_process safely exits
						exit(0);

				} // else if
		} // for

		// Waits for all child processes to end
		wait(NULL);

		// Loops through all children and collects count data
		for (int i=0; i<nChildProc; i++) {

				// Reads and closes pipe for children[i]
				read(fd[i][0], &count, sizeof(count));
				close(fd[i][0]);

				total_lines += count.linecount;
				total_words += count.wordcount;
				total_chars += count.charcount;
		}

		clock_gettime(CLOCK_REALTIME, &end);
		long seconds = end.tv_sec - begin.tv_sec;
		long nanoseconds = end.tv_nsec - begin.tv_nsec;
		double elapsed = seconds + nanoseconds*1e-9;

		printf("\n========= %s =========\n", argv[1]);
		printf("Total Lines : %d \n", total_lines);
		printf("Total Words : %d \n", total_words);
		printf("Total Characters : %d \n", total_chars);
		printf("======== Took %.3f seconds ========\n", elapsed);
	
}






