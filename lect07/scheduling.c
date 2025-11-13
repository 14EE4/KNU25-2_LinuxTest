#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>


#define PROCESS_NUM 10
#define QUANTUM 3

/*
부모 프로세스(커널)
자식 프로세스 10개 생성
PCB자료구조
-PID, 남은 타임퀀텀, 상태(running,ready,sleep,done ...)
round-robin scheduling수행
타이머 시그널 받을 때마다 
1. 실행중 프로세스 타임퀀텀 -1
2. 타임퀀텀 0 -> 다음 프로세스로 변경
3. 0이 아니면 계속
4. I/O요청시 I/O대기시간 랜덤으로 할당(1~5) -> sleep큐 -> 만료시 ready큐
5. 모든 프로세스의 타임퀀텀 0 -> 전체 프로세스의 타임퀀텀 초기화
*/

/*
 *자식 프로세스
 생성시 cpu버스트 랜덤 초기화(1~10)
 sleep상태로 대기하다가 부모 프로세스가 시그널 보내면 시작
 -실행시
 1. CPU버스트-1
 2. 0되면 종료or I/O(랜덤)
 3. I/O시 부모프로세스에서 I/O요청 시그널 보내기
 */
typedef enum State{
	
	STATE_READY,
	STATE_RUNNING,
	STATE_SLEEP,
	STATE_DONE
} State;

typedef struct PCB{
	pid_t pid;


	int remain_quantum;//남은 타임퀀텀(1~3)
	int remain_sleep;//남은 I/O시간

	State state;
} PCB; 

PCB pcb_table[PROCESS_NUM];

int ready_queue[PROCESS_NUM*2];
int rq_head=0;
int rq_tail=0;

int cur_process_idx=-1;//현재 실행중인 프로세스

int done_process_cnt=0;//끝난 프로세스 수

//signal flag
volatile sig_atomic_t alm_tick=0;
volatile sig_atomic_t io_request=0;
volatile sig_atomic_t is_terminated=0;

/*
 * 시그널
 * SIGALRM :커널이 스케줄러에게 1초가 지났음을 알려줌
 * SIGUSR1: 스케줄러가 자식 프로세스에게 CPU수행 지시
 * SIGUSR2: 자식 프로세스가 I/O요청
 * SIGCHLD: 자식 프로세스가 종료됨
 */

//큐 함수
void enqueue(int p_idx){
	ready_queue[rq_tail++]=p_idx;
}
int dequeue(){
	if (rq_head==rq_tail) return -1;
	return ready_queue[rq_head++];
}

//parant signal handler
void sig_alm_handler(int sig){
	//SIGALRM
	alm_tick=1;
}
void sig_io_handler(int sig, siginfo_t *info, void *context){
	//SIGUSR2
	io_request=info->si_pid;//io요청한 자식의 pid
}
void sig_terminated_handler(int sig){
	//SIGCHLD
	is_terminated=1;
}
//child process
volatile sig_atomic_t child_burst_left=0;//fork()로 각 프로세스의 남은 버스트 시간 저장됨

void child_sig_handler(int sig){
	if (sig==SIGUSR1){
		//새로 시작했거나 i/o에서 복귀했다면 cpu버스트 랜덤 할당
		if (child_burst_left==0){
			child_burst_left=(rand()%10)+1;			
			printf("[child %d] (new work)burst remain %d\n",getpid(),child_burst_left);

		}
		
		printf("[child %d] (running)burst remain %d\n",getpid(),--child_burst_left);
		if (child_burst_left==0){
			//종료하거나 i/o request
			if (rand()%2==0){
				printf("[child %d] end\n",getpid());
				kill(getppid(),SIGCHLD);
				exit(0);
			}else{
				
				printf("[child %d] io request\n",getpid());
				
				kill(getppid(),SIGUSR2);
			}
		}
	}
}
void child_main(){
	srand(time(NULL)^getpid());

	signal(SIGUSR1, child_sig_handler);//시그널 핸들러 설정
	while(1){
		pause();
	}
}

//parent
void scheduler(){
	if (cur_process_idx!=-1 && pcb_table[cur_process_idx].state==STATE_RUNNING){
		printf("[sched] process %d -> ready\n",cur_process_idx);
		pcb_table[cur_process_idx].state=STATE_READY;
		enqueue(cur_process_idx);
		
		cur_process_idx=dequeue();

		if (cur_process_idx==-1){
			printf("[sched] IDLE\n");
		}
		else{
			PCB *pcb=&pcb_table[cur_process_idx];
			pcb->state=STATE_RUNNING;

			pcb->remain_quantum=QUANTUM;
			printf("[sched] alloc time quantum :%d %d\n",cur_process_idx,pcb->pid);
			












