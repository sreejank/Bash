//Executes command tree made by parse 

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <limits.h>
#include <string.h>
#include "parse.h"
#include "stack.h"


// Extract status from value returned by waitpid(); ensure that a process
// that is killed has nonzero status; ignores the possibility of stop/continue.
#define STATUS(x) (WIFEXITED(x) ? WEXITSTATUS(x) : 128+WTERMSIG(x))


int doCommand(const CMD *cmdList);
int doSequence(const CMD *cmdList);


static Stack dirStack=STACK_EMPTY;
//static bool first=true;
int redirectionIn(const CMD *cmdList) {
	if(cmdList->fromType==RED_IN) {
		int file=open(cmdList->fromFile,O_RDONLY);
		if(file<0) {
			return errno;
		}
		dup2(file,0); //Standard In
		close(file);
	}
	else if(cmdList->fromType==RED_IN_HERE) { 
		char name[]="tempXXXXXX";

		int file=mkstemp(name);
		if(file<0) {

			return errno;
		}
		int writing_doc=write(file,cmdList->fromFile,strlen(cmdList->fromFile));
		if(writing_doc<0) {
			return errno;
		}
		close(file);
		file=open(name,O_RDONLY);
		if(file<0) {
			return errno;
		}
		dup2(file,0);
		close(file);
		unlink(name);
	}
	return EXIT_SUCCESS;
}

int redirectionOut(const CMD *cmdList) {
	if(cmdList->toType==RED_OUT) {
		int file=open(cmdList->toFile,O_CREAT| O_WRONLY | O_TRUNC,0666);

		if(file<0) {
			return errno;
		}
		dup2(file,1);
		close(file);
	}
	else if(cmdList->toType==RED_OUT_APP) {
		int file=open(cmdList->toFile,O_WRONLY | O_APPEND | O_CREAT,0666);
		if(file<0) {
			return errno;
		}
		dup2(file,1);
		close(file);
	}
	return EXIT_SUCCESS;
}


int cd(const CMD *cmdList) {
	int old_stdin=dup(0);
	int old_stdout=dup(1);
	if(cmdList->fromType!=NONE) {
		int redin=redirectionIn(cmdList);
		if(redin!=EXIT_SUCCESS) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("Redin error");
			return redin;
		}
	}
	if(cmdList->toType!=NONE) {
		int redout=redirectionOut(cmdList);
		if(redout!=EXIT_SUCCESS) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("Redout error");
			return redout;
		}
	}
	if(cmdList->argc>2) {
		fprintf(stderr,"Usage: cd or cd <dirName>\n");
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		return 1;
	}
	else if(cmdList->argc==1) {

		int changing=chdir(getenv("HOME"));
		if(changing!=0) {
			int error=errno;
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("chdir");
			return error;
		}
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		return changing;
	}
	else {
		

		char *directory=strdup(cmdList->argv[1]);
		int changing=chdir(directory);
		free(directory);
		if(changing!=0) {
			int error=errno;
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("no directory");
			return error;
		}
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		return changing;

	}
	dup2(old_stdin,0);
	dup2(old_stdout,1);

}

int pushd(const CMD *cmdList) {
	int old_stdin=dup(0);
	int old_stdout=dup(1);
	if(cmdList->fromType!=NONE) {
		int redin=redirectionIn(cmdList);
		if(redin!=EXIT_SUCCESS) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("Redin error");
			return redin;
		}
	}
	if(cmdList->toType!=NONE) {
		int redout=redirectionOut(cmdList);
		if(redout!=EXIT_SUCCESS) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("Redout error");
			return redout;
		}
	}

	if(cmdList->argc!=2) {
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		fprintf(stderr,"Usage: pushd <dir>\n");
		return 1;
	}
	if(stackEmpty(&dirStack)) {
		char *cwd=malloc(PATH_MAX+1);
		if(getcwd(cwd,PATH_MAX+1)==NULL) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("getcwd() error");
			return 1;
		}
		stackPush(&dirStack,cwd);
	}
	

	char *directory=strdup(cmdList->argv[1]);
	int changing=chdir(directory);
	free(directory);
	if(changing!=0) {
		int error=errno;
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		perror("no directory");
		return error;
	}

	char *cwd=malloc(PATH_MAX+1);
	if(getcwd(cwd,PATH_MAX+1)==NULL) {
		int error=errno;
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		perror("getcwd() error");
		return error;
	}

	stackPush(&dirStack,cwd);
	stackPrint(&dirStack);

	dup2(old_stdin,0);
	dup2(old_stdout,1);

	return 0;
}

int popd(const CMD *cmdList) {
	int old_stdin=dup(0);
	int old_stdout=dup(1);
	if(cmdList->fromType!=NONE) {
		int redin=redirectionIn(cmdList);
		if(redin!=EXIT_SUCCESS) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("Redin error");
			return redin;
		}
	}
	if(cmdList->toType!=NONE) {
		int redout=redirectionOut(cmdList);
		if(redout!=EXIT_SUCCESS) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("Redout error");
			return redout;
		}
	}

	if(cmdList->argc!=1) {
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		fprintf(stderr,"Usage: popd\n");
		return 1;
	}
	if(!stackEmpty(&dirStack)) {
		char *directory=stackPop(&dirStack);
		if(!stackEmpty(&dirStack)) {
			free(directory);
			directory=strdup(stackTop(&dirStack));
		}
		
		int changing=chdir(directory);
		free(directory);
		if(changing!=0) {
			int error=errno;
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			perror("no directory");
			return error;
		}
		if(stackEmpty(&dirStack)) {
			dup2(old_stdin,0);
			dup2(old_stdout,1);
			fprintf(stderr,"popd: directory stack empty\n");
			return 1;
		}
		stackPrint(&dirStack);
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		return 0;
	}
	else {
		dup2(old_stdin,0);
		dup2(old_stdout,1);
		perror("popd: directory stack empty");
		return 1;
	}
	
}


int doSimple(const CMD *cmdList) {
	
	int stat=EXIT_SUCCESS;
	int pid;
	if(cmdList!=NULL) {
		if(cmdList->nLocal>0) {
			for(int i=0;i<cmdList->nLocal;i++) {
				setenv(cmdList->locVar[i],cmdList->locVal[i],1);
			}
		}
		if(strcmp("cd",cmdList->argv[0])==0) {
			stat=cd(cmdList);
		}
		else if(strcmp("pushd",cmdList->argv[0])==0) {
			stat=pushd(cmdList);
		}
		else if(strcmp("popd",cmdList->argv[0])==0) {
			stat=popd(cmdList);
		}
		else {
			pid=fork();
			if(pid<0) {
				perror("Fork error");
				exit(errno);
			}
			else if(pid==0) {
				//fprintf(stderr,"Executing child\n");
				if(cmdList->fromType!=NONE) {
					int redin=redirectionIn(cmdList);

					if(redin!=EXIT_SUCCESS) {
						perror("Redin error");
						exit(redin);
					}
				}
				if(cmdList->toType!=NONE) {
					int redout=redirectionOut(cmdList);
					if(redout!=EXIT_SUCCESS) {
						perror("Redout error");
						exit(redout);
					}
				}
				execvp(cmdList->argv[0],cmdList->argv);
				//Failed
				perror("Simple command error");
				exit(errno);
			}
			else {
				signal(SIGINT,SIG_IGN);
				int chld_pid;
				stat=0;
				while((chld_pid=wait(&stat))!=pid) {
					fprintf(stderr,"Completed: %d (%d)\n",chld_pid,STATUS(stat));
				}
				signal(SIGINT,SIG_DFL);
				stat=STATUS(stat);
			}
		}
	}
	return stat;
}

int doStage(const CMD *cmdList) {
	
	int stat=EXIT_SUCCESS;
	if(cmdList!=NULL) {
		if(cmdList->type==SUBCMD) {
			if(cmdList->nLocal>0) {
				for(int i=0;i<cmdList->nLocal;i++) {
					setenv(cmdList->locVar[i],cmdList->locVal[i],1);
				}
			}
			int pid=fork();
			if(pid<0) {
				perror("Fork error");
				exit(errno);
			}
			else if(pid==0) {
				//fprintf(stderr,"Executing child\n");
				if(cmdList->fromType!=NONE) {
					int redin=redirectionIn(cmdList);

					if(redin!=EXIT_SUCCESS) {
						perror("Redin error");
						exit(redin);
					}
				}
				if(cmdList->toType!=NONE) {
					int redout=redirectionOut(cmdList);
					if(redout!=EXIT_SUCCESS) {
						perror("Redout error");
						exit(redout);
					}
				}
				exit(doCommand(cmdList->left)); //SHOULD BE FULL COMMAND
			}
			else {
				signal(SIGINT,SIG_IGN);
				int chld_pid;
				while((chld_pid=wait(&stat))!=pid) {
					fprintf(stderr,"Completed: %d (%d)\n",chld_pid,STATUS(stat));
				}
				signal(SIGINT,SIG_DFL);
				stat=STATUS(stat);
			}
		}
		else if(cmdList->type==SIMPLE) {
			stat=doSimple(cmdList);
		}
	}
	return stat;
}


int doPipeline(const CMD *cmdList) {
	
	int stat=EXIT_SUCCESS;
	int stat2=EXIT_SUCCESS;
	if(cmdList!=NULL) {
		if(cmdList->type!=PIPE) {
			stat=doStage(cmdList);
		}
		else {
			int fd[2];
			int pid;
			if(pipe(fd)<0) {
				perror("Pipe");
				return errno;
			}
			pid=fork();

			if(pid<0) {
				perror("Fork");
				exit(errno);
			}
			else if(pid==0) {
				close(fd[0]);
				dup2(fd[1],1);
				close(fd[1]);
				stat=doPipeline(cmdList->left);
				exit(stat);
			}
			else {

				close(fd[1]);
				int pid2=fork();
				if(pid2<0) {
					perror("Fork");
					exit(errno);
				}
				else if(pid2==0) {
					close(fd[1]);
					dup2(fd[0],0);
					close(fd[0]);
					stat2=doPipeline(cmdList->right);
					exit(stat2);
				}
				else {
					bool finish_left=false;
					bool finish_right=false;
					int stat_aux;
					int child_pid;

					signal(SIGINT,SIG_IGN);
					while((child_pid=(wait(&stat_aux)))>0) {
						if(child_pid==pid2) {
							stat2=stat_aux;
							finish_right=true;
						}
						else if(child_pid==pid) {
							stat=stat_aux;
							finish_left=true;
						}
						else {
							fprintf(stderr,"Completed: %d (%d)\n",child_pid,STATUS(stat_aux));
						}

						if(finish_left && finish_right) {
							break;
						}
					}
					signal(SIGINT,SIG_DFL);
					stat=STATUS(stat);
					stat2=STATUS(stat2);
					if(stat2!=EXIT_SUCCESS) {
						return stat2;
					}
					else {
						return stat;
					}
				}
				
			}
		}
	}
	return stat;	
}

int doAndOr(const CMD *cmdList) {
	int stat=EXIT_SUCCESS;
	if(cmdList!=NULL) {
		if(cmdList->type==SEP_AND) {
			stat=doAndOr(cmdList->left);
			if(stat==EXIT_SUCCESS) {
				stat=doAndOr(cmdList->right);
			}
		}
		else if(cmdList->type==SEP_OR) {
			stat=doAndOr(cmdList->left);
			if(stat!=EXIT_SUCCESS) {
				stat=doAndOr(cmdList->right);
			}
		}
		else {
			stat=doPipeline(cmdList);
		}
	}
	return stat; 
}


int doSequence_background(const CMD *cmdList) {
	int stat=EXIT_SUCCESS;
	if(cmdList!=NULL) {
		if(cmdList->type==SEP_END) {
			stat=doCommand(cmdList->left);
			stat=doSequence_background(cmdList->right);
		}
		else if(cmdList->type==SEP_BG) {
			stat=doSequence_background(cmdList->left);
			stat=doSequence_background(cmdList->right);
		}
		else {
			int pid=fork();
			if(pid<0) {
				perror("Fork");
				return errno;
			}
			else if(pid==0) {
				fprintf(stderr,"Backgrounded: %d\n",getpid());
				stat=doAndOr(cmdList);
				exit(stat);
			}
			else {
				return stat;
			}
		}
	}
	return stat;
}

int doSequence(const CMD *cmdList) {
	
	int stat=EXIT_SUCCESS;
	if(cmdList!=NULL) {
		if(cmdList->type==SEP_END) {
			stat=doCommand(cmdList->left);
			stat=doCommand(cmdList->right);
		}
		else if(cmdList->type==SEP_BG) {
			stat=doSequence_background(cmdList->left);
			if(cmdList->right!=NULL) {
				stat=doCommand(cmdList->right);
			}
		}
		else {
			stat=doAndOr(cmdList);
		}
	}
	

	return stat;
}



int doCommand(const CMD *cmdList) {
	int stat=doSequence(cmdList);
	char *status_str=malloc(10*sizeof(char));
	sprintf(status_str,"%d",stat);
	setenv("?",status_str,1);
	free(status_str);

	return stat;
}



// Execute command list CMDLIST and return status of last command executed
int process (const CMD *cmdList) {
	int chld_stat;
	int zombie_pid;
	while((zombie_pid=waitpid(-1,&chld_stat,WNOHANG))>0) {
		fprintf(stderr,"Completed: %d (%d)\n",zombie_pid,STATUS(chld_stat));
	}	

	int stat=doCommand(cmdList);
	
	return stat;
}



