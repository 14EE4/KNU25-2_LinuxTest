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
	
	int remain_quantum;//남은 타임퀀텀
	int remain_sleep;//남은 IO시간

	State state;
} PCB; 

PCB pdb_table[PROCESS_NUM];

int ready_queue[PROCESS_NUM*2];
int rq_head=0;
int rq_tail=0;

int cur_process_idx=-1;//현재 실행중인 프로세스

int done_process_cnt=0;//끝난 프로세스 수

//signal flag
volatile sig_atomic_t alm_tick=0;
volatile sig_atomic_t io_request=0;
volatile sig_atomic_t is_terminated=0;

//큐 함수
void enqueue(int p_idx){
	ready_queue[rq_tail++]=p_idx;
}
int dequeue(){
	if (rq_head==rq_tail)return -1;
	return ready_queue[rq_head++];
}

//parant signal handler
void sig_alm_handler(int sig){
	alm_tick=1;
}
void sig_io_handler(int sig, siginfo_t *info, void *context){
	io_request=info->si_pid//io요청한 자식의 pid
}
void sig_terminated_handler(int sig){
	is_terminated=1;
}
//child ''
volatile sig_atomic_t burst_left=0;

void child_sig_handler(int sig){
	

int main(){
	

	
