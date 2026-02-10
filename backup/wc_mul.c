/*************************************************
 * C program to count no of lines, words and 	 *
 * characters in a file.		                 *
 *************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PROC 100
#define MAX_FORK 1000

typedef struct count_t {
	int linecount;
	int wordcount;
	int charcount;
} count_t;

typedef struct plist_t {
	int pid;
	long offset;
	int pipefd[2];
} plist_t;

int CRASH = 0;

count_t word_count(FILE* fp, long offset, long size)
{
	char ch;
	long rbytes = 0;

	count_t count;
	// Initialize counter variables
	count.linecount = 0;
	count.wordcount = 0;
	count.charcount = 0;

	printf("[pid %d] reading %ld bytes from offset %ld\n", getpid(), size, offset);

	if(fseek(fp, offset, SEEK_SET) < 0) {
		printf("[pid %d] fseek error!\n", getpid());
	}

	while ((ch=getc(fp)) != EOF && rbytes < size) {
		// Increment character count if NOT new line or space
		if (ch != ' ' && ch != '\n') { ++count.charcount; }

		// Increment word count if new line or space character
		if (ch == ' ' || ch == '\n') { ++count.wordcount; }

		// Increment line count if new line character
		if (ch == '\n') { ++count.linecount; }
		rbytes++;
	}

	srand(getpid());
	if(CRASH > 0 && (rand()%100 < CRASH))
	{
		printf("[pid %d] crashed.\n", getpid());
		abort();
	}

	return count;
}

int main(int argc, char **argv)
{
	long fsize;
	FILE *fp;
	int numJobs;
	plist_t plist[MAX_PROC];
	count_t total, count;
	int i, pid, status;
	int nFork = 0;

	if(argc < 3) {
		printf("usage: wc_mul <# of processes> <filname>\n");
		return 0;
	}

	if(argc > 3) {
		CRASH = atoi(argv[3]);
		if(CRASH < 0) CRASH = 0;
		if(CRASH > 50) CRASH = 50;
	}
	printf("CRASH RATE: %d\n", CRASH);

	numJobs = atoi(argv[1]);
	if(numJobs > MAX_PROC) numJobs = MAX_PROC;

	total.linecount = 0;
	total.wordcount = 0;
	total.charcount = 0;

	// Open file in read-only mode
	fp = fopen(argv[2], "r");

	if(fp == NULL) {
		printf("File open error: %s\n", argv[2]);
		printf("usage: wc <# of processes> <filname>\n");
		return 0;
	}

	fseek(fp, 0L, SEEK_END);
	fsize = ftell(fp);

	fclose(fp);
	// calculate file offset and size to read for each child
	long chunkSize = fsize / numJobs;

	for(i = 0; i < numJobs; i++) {
		// set pipe
		if(pipe(plist[i].pipefd) < 0) {
			perror("pipe failed");
			exit(1);
		}

		plist[i].offset = i * chunkSize;
		long size = (i == numJobs - 1) ? (fsize - plist[i].offset) : chunkSize;

		if(nFork++ > MAX_FORK) return 0;

		pid = fork();
		if(pid < 0) {
			printf("Fork failed.\n");
		} else if(pid == 0) {
			// Child: close read end, do work, write result, exit
			close(plist[i].pipefd[0]);
			fp = fopen(argv[2], "r");
			count = word_count(fp, plist[i].offset, size);
			write(plist[i].pipefd[1], &count, sizeof(count_t));
			close(plist[i].pipefd[1]);
			fclose(fp);
			exit(0);
		} else {
			plist[i].pid = pid;
		}
	}

	// Parent: close all write ends
	for(i = 0; i < numJobs; i++) {
		close(plist[i].pipefd[1]);
	}

	// Wait for all children, read results, immediately retry failures
	for(i = 0; i < numJobs; i++) {
		waitpid(plist[i].pid, &status, 0);
		if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
			count_t childCount;
			if(read(plist[i].pipefd[0], &childCount, sizeof(count_t)) == sizeof(count_t)) {
				total.linecount += childCount.linecount;
				total.wordcount += childCount.wordcount;
				total.charcount += childCount.charcount;
			}
		} else {
			// Child failed - retry immediately after failure detected
			close(plist[i].pipefd[0]);
			long size = (i == numJobs - 1) ? (fsize - plist[i].offset) : chunkSize;

			while(nFork < MAX_FORK) {
				if(pipe(plist[i].pipefd) < 0) {
					perror("pipe failed");
					exit(1);
				}

				if(nFork++ > MAX_FORK) break;
				pid = fork();
				if(pid < 0) {
					printf("Fork failed.\n");
					break;
				} else if(pid == 0) {
					close(plist[i].pipefd[0]);
					fp = fopen(argv[2], "r");
					count = word_count(fp, plist[i].offset, size);
					write(plist[i].pipefd[1], &count, sizeof(count_t));
					close(plist[i].pipefd[1]);
					fclose(fp);
					exit(0);
				} else {
					plist[i].pid = pid;
					close(plist[i].pipefd[1]);
					waitpid(plist[i].pid, &status, 0);
					if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
						count_t childCount;
						if(read(plist[i].pipefd[0], &childCount, sizeof(count_t)) == sizeof(count_t)) {
							total.linecount += childCount.linecount;
							total.wordcount += childCount.wordcount;
							total.charcount += childCount.charcount;
						}
						break; // Success - move to next child
					}
					close(plist[i].pipefd[0]);
					// Failed again - loop continues to retry
				}
			}
			continue;
		}
		close(plist[i].pipefd[0]);
	}

	printf("\n========== Final Results ================\n");
	printf("Total Lines : %d \n", total.linecount);
	printf("Total Words : %d \n", total.wordcount);
	printf("Total Characters : %d \n", total.charcount);
	printf("=========================================\n");

	return(0);
}
