#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#define N 4

void sinx_taylor(int num_elements,int terms, double* x, double* result){
	pid_t pids[num_elements];
	int pipes[num_elements][2];

	//create pipes
	
	for (int i=0;i<num_elements;i++){
		if(pipe(pipes[i])==-1){
			perror("pipe");
			exit(1);
		}
	}
	//child process
	for (int i=0;i<num_elements;i++){
		pids[i]=fork();

		if (pids[i]==-1){
			perror("fork");
			exit(1);
		}

		
		if (pids[i]==0){
			close(pipes[i][0]);
			double value=x[i];
			double numer=x[i]*x[i]*x[i];
			double denom =6.;
			int sign=-1;

			for (int j=1;j<=terms;j++){
				value+=(double)sign*numer/denom;
				numer*=x[i]*x[i];
				denom*=(2.*(double)j+2.)*(2.*(double)j+3.);
				sign*=-1;
			}

			if (write(pipes[i][1],&value,sizeof(value))==-1){
				perror("write");
				exit(1);
			}
			close(pipes[i][1]);
			//자식 프로세스 종료
			exit(0);
		}
	}

	//모든 자식 프로세스가 끝날떄까지 기다리기
	for (int i=0;i<num_elements;i++){
		waitpid(pids[i],NULL,0);
	}	
	//parent
	for (int i=0;i<num_elements;i++){
		double val_from_child;
		
		close(pipes[i][1]);
		if (read(pipes[i][0],&val_from_child,sizeof(val_from_child))==-1){
			perror("read");
			exit(1);
		}	
				
		result[i]=val_from_child;
		close(pipes[i][0]);
	}

		
}

int main(){
	double x[N] = {0,M_PI/6.,M_PI/3.,0.134};
	double res[N];
	
	sinx_taylor(N,3,x,res);
	for (int i=0;i<N;i++){
		printf("sin(%.2f) by taylor series= %f\n",x[i],res[i]);
		printf("sin(%.2f) = %f\n",x[i],sin(x[i]));
	}
	return 0;
}


