/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
extern int execvpe(const char *file, char *const argv[], char *const envp[]);
#include "variante.h"
#include "readcmd.h"
#include <assert.h>
#include <signal.h>


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);
	
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

typedef struct timeval timeval_t;

typedef struct process{
	pid_t pid;
	char * seq;
	struct timeval *tv;
	struct process* suiv;
} process_t;
typedef struct process_list{
	struct process* p;
} process_list_t;

void add_in_plist(process_list_t *list, process_t *proc){
	proc->suiv=list->p;
	list->p=proc;
}

struct process* get_in_plist(process_list_t *list, pid_t pid, int remove){
	struct process* temp=(list->p);
	if (temp==NULL){
		return NULL;
	}
	if(temp->pid==pid){
		if(remove){
			list->p=temp->suiv;
		}
		return temp;
	}
	while(temp->suiv!=NULL){
		if(temp->suiv->pid==pid){
			if(remove){
				temp->suiv=temp->suiv->suiv;
			}
			return temp->suiv;
		}
	}
	return NULL;

}



int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif



    process_list_t *plist=NULL;
    plist=malloc(sizeof(process_list_t));
    plist->p=NULL;
    int nbpipe=0;


	while (1) {
		struct cmdline *l;
		char *line = 0;
		//int i;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		 can not be cleaned at the end of the program. Thus
		 one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || !strncmp(line, "exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif

#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne,
					"(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))",
					line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
			continue;
		}
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd(&line);

		/* If input stream closed, normal termination */
		if (!l) {

			terminate(0);
		}

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in)
			printf("in: %s\n", l->in);
		if (l->out)
			printf("out: %s\n", l->out);
		if (l->bg)
			printf("background (&)\n");

		nbpipe = 0;
		while (l->seq[nbpipe] != NULL) {
			nbpipe++;
		}

		if (nbpipe <2) {

			if (l->seq[0] != NULL) {

				pid_t pid = fork();
				if (pid == 0) {

					if (strcmp(l->seq[0][0], "jobs") == 0) {
						process_t *lp = plist->p;
						char* fb[5];
						fb[0] = malloc(5 * sizeof(char));
						fb[0] = "echo";
						fb[1] = malloc(11 * sizeof(char));
						fb[1] = "\nRUNNING:\t";
						fb[3] = malloc(2 * sizeof(char));
						fb[3] = "\n";
						fb[4] = NULL;

						while (lp != NULL) {
							if (fork() == 0) {

								fb[2] = lp->seq;
								execvp(fb[0], fb);

							}

							lp = lp->suiv;
						}
						exit(0);

					} else {
						extern char** environ;
						if (execvpe(l->seq[0][0], l->seq[0], environ) == -1) {
							//error handling
							printf("error in: %s : error n %i \n", l->seq[0][0],
							errno);
							exit(-1);
						}
					}

				} else if (!l->bg) {
					waitpid(pid, NULL, 0);
				} else { //running in background

					process_t *pr = NULL;
					pr = malloc(sizeof(process_t));
					pr->pid = pid;
					pr->seq = malloc(sizeof(char) * strlen(l->seq[0][0]) + 1);
					pr->tv = malloc(sizeof(timeval_t));
					gettimeofday(pr->tv, NULL);
					assert(pr->tv!=NULL);
					strcpy(pr->seq, l->seq[0][0]);
					pr->suiv = NULL;

					add_in_plist(plist, pr);

				}

			}
		} else {

			int tuyau[2];
			pid_t pid;
			int n_inp = 0;
			int i = 0;

			while (l->seq[i] != NULL) {
				pipe(tuyau);
				pid = fork();
				if (pid == 0) {

					dup2(n_inp, 0);
					if (l->seq[i + 1] != NULL) {
						dup2(tuyau[1], 1);
					}
					close(tuyau[0]);

					if (strcmp(l->seq[0][0], "jobs") == 0) {
						process_t *lp = plist->p;
						char* fb[5];
						fb[0] = malloc(5 * sizeof(char));
						fb[0] = "echo";
						fb[1] = malloc(11 * sizeof(char));
						fb[1] = "\nRUNNING:\t";
						fb[3] = malloc(2 * sizeof(char));
						fb[3] = "\n";
						fb[4] = NULL;

						while (lp != NULL) {
							if (fork() == 0) {

								fb[2] = lp->seq;
								execvp(fb[0], fb);

							}

							lp = lp->suiv;
						}

						exit(0);

					} else {

						if (execvp(l->seq[i][0], l->seq[i]) == -1) {
							//error handling
							printf("error in: %s : error n %i \n", l->seq[0][0],
							errno);
							exit(-1);
						}
					}

				} else {
					waitpid(pid, NULL, 0);
					close(tuyau[1]);
					n_inp = tuyau[0];
					i++;
				}
			}
		}









			/*
			 for (int i = 0; i < nbpipe - 1; i++) {
			 if (fork() == 0) {
			 //
			 dup2(tuyau[i]);
			 close(tuyau[1]);
			 close(tuyau[0]);
			 //redirection?
			 extern char** environ;
			 if (execvpe(l->seq[i][0], l->seq[i], environ) == -1) {
			 //error handling
			 printf("error in: %s : error n %i \n", l->seq[0][0],
			 errno);
			 exit(-1);
			 }
			 }
			 pid_t pid=fork();
			 if(pid_t==0){
			 dup2(tuyau[i][1],)

			 }else if (!l->bg) {
			 waitpid(pid, NULL, 0);
			 } else { //running in background

			 process_t *pr = NULL;
			 pr = malloc(sizeof(process_t));
			 pr->pid = pid;
			 pr->seq = malloc(sizeof(char) * strlen(l->seq[0][0]) + 1);
			 pr->tv = malloc(sizeof(timeval_t));
			 gettimeofday(pr->tv, NULL);
			 assert(pr->tv!=NULL);
			 strcpy(pr->seq, l->seq[0][0]);
			 pr->suiv = NULL;

			 add_in_plist(plist, pr);

			 }
			 }
			 */

		pid_t waited_pid = waitpid(WAIT_ANY,NULL,WUNTRACED|WNOHANG);
		if(waited_pid>0){
			process_t *pr2=get_in_plist(plist,waited_pid,1);
			if(pr2!=NULL){
/*
				struct timeval *tv2=malloc(sizeof(timeval_t));
				gettimeofday(tv2, NULL);


				printf("\n Temps de calcul:\t %i secondes %i millisecondes \n",(int)(tv2->tv_sec-pr2->tv->tv_sec),(int)(tv2->tv_usec-pr2->tv->tv_usec));
				free(tv2);*/
				free(pr2->tv);
				free(pr2->seq);
				free(pr2);
			}
		}

	}

}
