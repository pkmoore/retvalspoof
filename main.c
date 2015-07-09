#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <inttypes.h>

int main(int argc, char** argv) {

    int opt;
    bool c_present = false;
    bool r_present = false;
    bool s_present = false;
    char* command;
    int64_t retval = 0;
    int64_t syscall = 0;
    while((opt = getopt(argc, argv, "s:c:r:")) != -1) {
        switch(opt) {
            case 'c':
                command = optarg;
                c_present = true;
                break;
            case 'r':
                r_present = true;
                retval = atoll(optarg);
                break;
            case 's':
                s_present = true;
                syscall = atoll(optarg);
                break;
        }
    }

    if(!(c_present) || !(r_present) || !(s_present)) {
        printf("Invalid required arguments\n");
        printf("%s -s <syscall number> -c <command> -r <return value>\n", argv[0]);
        exit(1);
    }

    pid_t child;
    int64_t rax;
    int status;
    bool insyscall = false;
    struct user_regs_struct regs;

    child = fork();
    if(child == 0) {
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if(execlp(command, command, NULL) == -1) {
            printf("Failed to execute command in child process\n");
        }
    }
    else {
        while(true) {
            wait(&status);
            if(WIFEXITED(status)) {
                break;
            }
            rax = ptrace(PTRACE_PEEKUSER,
                    child, 8 * ORIG_RAX,
                    NULL);
            if(rax == syscall) {
                //if we are not in a system call currently, we must be entering one
                if(!insyscall) {
                    //we are now in a system call
                    insyscall = true;
                }
                //if we ARE in a system call we must be exiting one
                else {
                    insyscall = false;
                    rax = ptrace(PTRACE_PEEKUSER, child, 8 * RAX, NULL);
                    ptrace(PTRACE_GETREGS, child, NULL, &regs);
                    regs.rax = retval;
                    ptrace(PTRACE_SETREGS, child, NULL, &regs);
                }
            }
            ptrace(PTRACE_SYSCALL, child, NULL, NULL);
        }
        return 0;
    }
}
