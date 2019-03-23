//#define _XOPEN_SOURCE /* Mac compatibility. */
//#define _POSIX_C_SOURCE>=199309L
//gcc main.c -std=gnu11 -o a.out
//./a.out 0.001 test11.txt test22.txt test33.txt
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define STACK_SIZE 1024*1024*4
#define goto_unfinished() \
	do{ \
		do{ \
			c_i=(c_i+1)%c_s; \
		}while(coroutine_finished[c_i]); \
	}while(0)
#define switch_coro_fast() \
	do { \
		save = c_i; \
		goto_unfinished(); \
		if(c_i!=save){ \
			switch_cont_n[c_i]++; \
			swapcontext(&contexts[save], &contexts[c_i]); \
			c_i = save; \
		} \
}while(0)
#define switch_coro() \
	do { \
		clock_gettime(CLOCK_REALTIME, &t_end); \
		int delta = (t_end.tv_sec-t_start.tv_sec)*1000000000LLU + t_end.tv_nsec-t_start.tv_nsec; \
		if (delta>1000*(T/c_s)){ \
			contexts_times[c_i] += delta; \
			save = c_i; \
			goto_unfinished(); \
			if(c_i!=save){ \
				switch_cont_n[c_i]++; \
				swapcontext(&contexts[save], &contexts[c_i]); \
				c_i = save; \
			} \
			clock_gettime(CLOCK_REALTIME, &t_start); \
		} \
}while(0)
#define finish_others() \
	do{ \
		clock_gettime(CLOCK_REALTIME, &t_end); \
		contexts_times[c_i] += (t_end.tv_sec-t_start.tv_sec)*1000000000LLU + t_end.tv_nsec-t_start.tv_nsec; \
		if(len == orgn_len){ \
			c_ret_n++; \
			coroutine_finished[c_i]=1; \
			if(c_ret_n==c_s){ \
				swapcontext(&contexts[c_i], &main_context); \
			} \
			while(c_ret_n<c_s){ \
				switch_coro_fast(); \
			} \
		} \
		clock_gettime(CLOCK_REALTIME, &t_start); \
	}while(0)
#define handle_error(msg) \
   do { perror(msg); exit(EXIT_FAILURE); } while (0)
typedef struct arr{
	int * ptr;
	int len;
	int pos;
}arr;
int switch_n=0;
ucontext_t main_context;
ucontext_t * contexts;
unsigned long long *  contexts_times;
unsigned long long *  switch_cont_n;
int * coroutine_finished;
int c_i=0; //current context id
int c_s=0; //number of contexts
int c_ret_n=0;
double T=0;//target latency microsec
int min(arr * list_of_arr, int size, int * ret);
void merge(arr * list_of_arr, int size , FILE * fp);

void sort(int * ptr, int len, int orgn_len) {
	struct timespec t_start;
	struct timespec t_end;
	clock_gettime(CLOCK_REALTIME, &t_start);
	int save=0;switch_coro();
	int i=0, j=0;switch_coro();
	int temp;switch_coro();
	int pivot =  ptr[len / 2];switch_coro();
	if (len < 2) {
		finish_others();
		return;
	}
	for (i=0, j=len-1; ;i++,j--) {switch_coro();
		while (pivot>ptr[i]) {i++;switch_coro();}
		while (ptr[j]>pivot) {j--;switch_coro();}
		if (i>=j) break;switch_coro();
		temp = ptr[i];switch_coro();
		ptr[i] = ptr[j];switch_coro();
		ptr[j] =temp;switch_coro();
	}
	sort(ptr, i, orgn_len);
	sort(ptr+i, len-i, orgn_len);
	clock_gettime(CLOCK_REALTIME, &t_start);
	finish_others();
}

static void *
allocate_stack()
{
	void *stack = malloc(STACK_SIZE);
	mprotect(stack, STACK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
	return stack;
}

int main(int argc, char const *argv[]) {
	FILE * fp;
	int len=argc-2;
	c_s=len;
	int size=0;
	int tmp=0;
	double Elapsed;
	struct timeval t_start;
	struct timeval t_end;


	if(argc<3){

		printf("Usage :./f T filename ...\n");
		return 0;
	}
	arr *list_of_arr=malloc(len*sizeof(arr));
	contexts=malloc(len*sizeof(ucontext_t));
	contexts_times=malloc(len*sizeof(unsigned long long));
	switch_cont_n = malloc(len*sizeof(unsigned long long));
	coroutine_finished = malloc(len*sizeof(int));
	for(int i=0;i<len;i++){
		contexts_times[i] = 0;
		switch_cont_n[i] = 0;
		coroutine_finished[i] =0;
	}
	T = atof(argv[1]);
	for(int i=0;i<len;i++){
		fp = fopen(argv[i+2], "r");
		int l_i=0;
		while(fscanf(fp, "%d", &tmp)==1){
			l_i++;
		}
		size+=l_i;
		list_of_arr[i].ptr = malloc(l_i*sizeof(int));
		list_of_arr[i].len = l_i;
		list_of_arr[i].pos = 0;
		fclose(fp);
	}
	for(int i=0;i<len;i++){
		fp = fopen(argv[i+2], "r");
		int k=0;
		while(fscanf(fp, "%d", &list_of_arr[i].ptr[k])==1){
			k++;
		}
		fclose(fp);
	}
	void ** save_stacks= malloc(len*sizeof(void *));
	for(int i=len-1;i>=0;i--){
		if (getcontext(&contexts[i]) == -1)
			handle_error("getcontext");
		contexts[i].uc_stack.ss_sp=allocate_stack();
		save_stacks[i] = contexts[i].uc_stack.ss_sp;
		contexts[i].uc_stack.ss_size=STACK_SIZE;
		if(i==len-1)contexts[i].uc_link =&main_context;
		else contexts[i].uc_link =&contexts[i+1];
		makecontext(&contexts[i], (void (*)(void))sort, 3, list_of_arr[i].ptr, list_of_arr[i].len, list_of_arr[i].len);
	}
	getcontext(&main_context);

	int ch=0;
	gettimeofday(&t_start, 0);
	if(!ch){ ch=1;

		if (swapcontext(&main_context, &contexts[c_i=0]) == -1)
		handle_error("swapcontext");
	}
	gettimeofday(&t_end, 0);
	for(int i=0;i<len;i++){
		printf("coro%d time = %12llu, switch_n = %llu\n", i, contexts_times[i]/1000, switch_cont_n[i]);
	}
	fp = fopen("result.txt", "w");
	merge(list_of_arr, len, fp);
	fclose(fp);
	for(int i=0;i<len;i++){
		free(list_of_arr[i].ptr);
		free(save_stacks[i]);
	}
	free(save_stacks);
	free(list_of_arr);
	free(contexts);
	free(contexts_times);
	free(switch_cont_n);
	free(coroutine_finished);
	printf("Elapsed =    %12lld\n", (t_end.tv_sec-t_start.tv_sec)*1000000LL + (t_end.tv_usec-t_start.tv_usec));
	return 0;
}

int min(arr * list_of_arr, int size, int * ret){
	int min;
	int k=0;
	int s=0;
	int i=0;
	for(i =0; i<size;i++){
		if(list_of_arr[i].pos<list_of_arr[i].len){
			min = list_of_arr[i].ptr[list_of_arr[i].pos];
			k=i;
			break;
		}
	}
	if(i==size){
		return 0;
	}
	for(int i =k; i<size;i++){
		if(list_of_arr[i].pos<list_of_arr[i].len){
			if(min>=list_of_arr[i].ptr[list_of_arr[i].pos]){
				min = list_of_arr[i].ptr[list_of_arr[i].pos];
				s=i;
			}
		}
	}
	list_of_arr[s].pos++;
	*ret = min;
	return 1;
}

void merge(arr * list_of_arr, int size , FILE * fp){
	int a;
  while(min(list_of_arr, size, &a)){
		fprintf(fp, "%d\n", a);
	}
}
