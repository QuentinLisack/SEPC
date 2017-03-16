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

struct process{
	pid_t pid;
	char ** seq;
	struct process* suiv;
};
struct process_list{
	struct process* p;
};

void add_in_plist(struct process_list list, struct process proc){
	proc.suiv=list.p;
	list.p=&proc;
}

struct process* get_in_plist(struct process_list list, pid_t pid, int remove){
	struct process* temp=(list.p);
	if (temp==NULL){
		return NULL;
	}
	if(temp->pid==pid){
		if(remove){
			list.p=temp->suiv;
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

    struct process_list plist={NULL};

	while (1) {
		struct cmdline *l;
		char *line = 0;
		int i, j;
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

		if (l->seq[0] != NULL) {

			if (strcmp(l->seq[0][0], "jobs") == 0) {
				struct process* temp = plist.p;
				while (temp != NULL) {
					printf("%i\n", temp->pid);
					temp = temp->suiv;
				}
			} else {
				pid_t pid = fork();
				if (pid == 0) {
					extern char** environ;
					if (execvpe(l->seq[0][0], l->seq[0], environ) == -1) {
						//error handling
						printf("error in: %s : error n %i \n", l->seq[0][0],
								errno);
						exit(-1);
					}

				} else if (!l->bg) {
					waitpid(pid, NULL, 0);
				} else { //running in background


					struct process pr={pid,l->seq[0],NULL};
					add_in_plist(plist, pr);


					pid_t pid2 = fork();
					if (pid2 == 0) {

						waitpid(pid, NULL, 0);

						pr = *get_in_plist(plist, pid, 1);
						//do stuff
						exit(0);
					}
				}
			}

			/* Display each command of the pipe */
			for (i = 0; l->seq[i] != 0; i++) {
				char **cmd = l->seq[i];
				printf("seq[%d]: ", i);
				for (j = 0; cmd[j] != 0; j++) {
					printf("'%s' ", cmd[j]);
				}
				printf("\n");
			}
		}else{
			int cpt=0;
			struct process* temp = plist.p;
			while (temp != NULL) {
				cpt++;
				temp=temp->suiv;
			}
			printf("%i\n",cpt);
		}

		pid_t waited_pid = waitpid(-1,NULL,WNOHANG);
		if(waited_pid!=0){
			get_in_plist(plist,waited_pid,1);
		}

	}

}
