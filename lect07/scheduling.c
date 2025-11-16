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
int elapsed_time=0;//출력용 지난 시간


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
int is_queue_empty(){
	return rq_head==rq_tail;
}

//parant signal handler
void sig_alm_handler(int sig){
	//SIGALRM
	alm_tick=1;
	elapsed_time++;
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
//SIGUSR1
void child_sig_handler(int sig){
	if (sig==SIGUSR1){
		//새로 시작했거나 i/o에서 복귀했다면 cpu버스트 랜덤 할당
		if (child_burst_left==0){
			child_burst_left=(rand()%10)+1;			
			printf("[child %d] (new work)burst remain %d\n",getpid(),child_burst_left);

		}
		child_burst_left--;
		printf("[child %d] (running)burst remain %d\n",getpid(),child_burst_left);
		if (child_burst_left==0){
			//종료하거나 i/o request
			if (rand()%2==0){
				printf("[child %d] end\n",getpid());
				//kill(getppid(),SIGCHLD);
				exit(0);//자식이 종료되면 커널이 SIGCHLD보냄
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
//타임 퀀텀 만료시 실행되는 함수
void scheduler(){
	if (cur_process_idx!=-1 && pcb_table[cur_process_idx].state==STATE_RUNNING){
		printf("[sched] process %d -> ready\n",cur_process_idx);
		pcb_table[cur_process_idx].state=STATE_READY;
		enqueue(cur_process_idx);
	}		
	cur_process_idx=dequeue();

	if (cur_process_idx==-1){
		printf("[sched] IDLE, 종료된 프로세스 수: %d\n",done_process_cnt);
	}
	else{
		PCB *pcb=&pcb_table[cur_process_idx];
		pcb->state=STATE_RUNNING;

		pcb->remain_quantum=QUANTUM;
		printf("[sched] alloc time quantum :%d %d\n",cur_process_idx,pcb->pid);
	}
}	
// main
int main(){
	srand(time(NULL));

	//signal handler config
	struct sigaction sa_io, sa_chld;
	
	signal(SIGALRM, sig_alm_handler);
	
	//SIGUSR2(i/o요청받음)
	memset(&sa_io, 0, sizeof(sa_io));
	sa_io.sa_sigaction=sig_io_handler;
	sa_io.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR2, &sa_io, NULL);

		
	//SIGCHLD(자식 프로세스 종료)
	memset(&sa_chld, 0, sizeof(sa_chld));
	sa_chld.sa_handler=sig_terminated_handler;
	sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigaction(SIGCHLD, &sa_chld, NULL);
	
	//자식 프로세스 생성
	for (int i=0;i<PROCESS_NUM;i++){
		pid_t pid=fork();
		if (pid==0){
			child_main();
			exit(0);
		}
		else{
			pcb_table[i].pid=pid;
				
			pcb_table[i].state=STATE_READY;

			pcb_table[i].remain_sleep=0;

			pcb_table[i].remain_quantum=0;
			enqueue(i);
		}
	}

	printf("스케줄링 시작\n");

	scheduler();
	alarm(1);//1초 뒤에 알람 시그널 발생
		
	while (done_process_cnt <PROCESS_NUM){
		pause(); //시그널 들어올때까지 대기
			 //
		//sigalrm으로 1초가 지나면
		if (alm_tick){
			alm_tick=0;

			printf("[%3ds] alm tick\n",elapsed_time);

			//sleep중인거 타임 1 줄이기
			for (int i=0;i<PROCESS_NUM;i++){
				if (pcb_table[i].state==STATE_SLEEP){
					pcb_table[i].remain_sleep--;
					if (pcb_table[i].remain_sleep==0){
						printf("pno %d:I/O완료\n", i);
						pcb_table[i].state=STATE_READY;
						enqueue(i);
					}
				}
			}
			
			//실행중인 프로세스 처리
			if (cur_process_idx!=-1){
						
				PCB *pcb=&pcb_table[cur_process_idx];
				pcb->remain_quantum--;
				printf("pno %d에게 작업지시\n",cur_process_idx);
				kill(pcb->pid,SIGUSR1);
				usleep(5000);//자식 프로세스 처리 대기(sleep은 sigalrm사용함으로 usleep사용);
					
				//io나 종료가 아니라면
				if (pcb->state==STATE_RUNNING && io_request!=pcb->pid && is_terminated==0){
					if (pcb->remain_quantum==0){
						printf("pno %d 퀀텀만료\n",cur_process_idx);
						scheduler();
					}else{

						printf("pno %d 계속 실행(남은 퀀텀: %d)\n",cur_process_idx,pcb->remain_quantum);
					}
				}
			}
			else{
				if (!is_queue_empty()){
					scheduler();
				}
			}
			if (done_process_cnt<PROCESS_NUM){
				alarm(1);
			}
		}
		//SIGUSR2(I/O요청)
		//
		if (io_request>0){
			pid_t req_pid =io_request;
			io_request=0;

			for (int i=0;i<PROCESS_NUM;i++){
				if(pcb_table[i].pid==req_pid){
					//io시간 랜덤으로 1~5
					int sleep_time=(rand()%5)+1;
					printf("[i/o] pno %d io request. sleep %d\n",i,sleep_time);
					pcb_table[i].state=STATE_SLEEP;

					pcb_table[i].remain_sleep=sleep_time;
					pcb_table[i].remain_quantum=0;
	
					scheduler();
					break;
				}
			}
		}	
		//SIGCHLD
		if (is_terminated){
			is_terminated=0;
			pid_t terminated_pid;

			//종료된 자식 프로세스 처리
			while ((terminated_pid = waitpid(-1,NULL,WNOHANG))>0){
				for (int i=0;i<PROCESS_NUM;i++){
					if (pcb_table[i].pid==terminated_pid){
						//종료된 자식 프로세스
						if (pcb_table[i].state!=STATE_DONE){
							printf("[CHLD] pno %d : 종료\n",i);
							pcb_table[i].state=STATE_DONE;
							pcb_table[i].remain_quantum=0;
							done_process_cnt++;

							if (i==cur_process_idx){
								scheduler();
							}
						}
					}
				}
			}
		}
	}
	printf("모든 프로세스 종료됨\n");
	return 0;
}
				


