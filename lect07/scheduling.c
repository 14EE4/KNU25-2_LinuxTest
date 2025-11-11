#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

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
4. I/O요청시 랜덤으로 할당(1~5) -> sleep큐 -> 만료시 ready큐
5. 모든 프로세스의 타임퀀텀 0 -> 전체 프로세스의 타임퀀텀 초기화
*/

/*
 *자식 프로세스
 생성시 cpu버스트 랜덤 초기화(1~10)
 sleep상태로 대기하다가 부모 프로세스가 시그널 보내면 시작
 -실행시
 1. CPU버스트-1
 2. 0되면 종료or I/O(랜덤)
 3. I/O시 부모프로세스에세 I/O요청 시그널 보내기
 */
typedef enum State{
	STATE_NEW,
	STATE_READY,
	STATE_RUNNING,
	STATE_SLEEP,
	STATE_TERMINATED
}

typedef struct PCB{
	int PID;
	int time;
	State state;
}

int main(){

