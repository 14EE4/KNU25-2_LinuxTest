#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]){

	int a=atoi(argv[1]);
	int b=atoi(argv[3]);

	char* operator=argv[2];

	int ans;
	if (strcmp("+",operator)==0){
		ans=a+b;
	}
	else if(strcmp("x",operator)==0){
		ans=a*b;
	}


	printf("%d\n",ans);
}
