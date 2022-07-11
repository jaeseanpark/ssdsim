/*****************************************************************************************************************************
  This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
  Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName: ssd.c
Author: Hu Yang		Version: 2.1	Date:2011/12/02
Description: 

History:
<contributor>     <time>        <version>       <desc>                   <e-mail>
Yang Hu	        2009/09/25	      1.0		    Creat SSDsim       yanghu@foxmail.com
2010/05/01        2.x           Change 
Zhiming Zhu     2011/07/01        2.0           Change               812839842@qq.com
Shuangwu Zhang  2011/11/01        2.1           Change               820876427@qq.com
Chao Ren        2011/07/01        2.0           Change               529517386@qq.com
Hao Luo         2011/01/01        2.0           Change               luohao135680@gmail.com
 *****************************************************************************************************************************/



#include "ssd.h"

/********************************************************************************************************************************
  1，main函数中initiation()函数用来初始化ssd,；2，make_aged()函数使SSD成为aged，aged的ssd相当于使用过一段时间的ssd，里面有失效页，
  non_aged的ssd是新的ssd，无失效页，失效页的比例可以在初始化参数中设置；3，pre_process_page()函数提前扫一遍读请求，把读请求
  的lpn<--->ppn映射关系事先建立好，写请求的lpn<--->ppn映射关系在写的时候再建立，预处理trace防止读请求是读不到数据；4，simulate()是
  核心处理函数，trace文件从读进来到处理完成都由这个函数来完成；5，statistic_output()函数将ssd结构中的信息输出到输出文件，输出的是
  统计数据和平均数据，输出文件较小，trace_output文件则很大很详细；6，free_all_node()函数释放整个main函数中申请的节点

  1. main 함수의 initialization() 함수는 SSD를 초기화하는 데 사용됩니다. 2. make_aged() 함수는 SSD를 노화시킵니다.
  노화된 SSD는 일정 기간 동안 사용한 SSD에 해당하며, 유효하지 않은 페이지입니다. non_aged ssd는 유효하지 않은 페이지가 없는 새 ssd입니다.
  유효하지 않은 페이지의 비율은 초기화 매개변수에서 설정할 수 있습니다. 3. pre_process_page() 함수는 읽기 요청을 미리 스캔하고 읽기 요청을 저장합니다.
  lpn<--->ppn 매핑 관계를 미리 설정하고 쓰기 요청의 lpn<--->ppn 매핑 관계를 쓰기 시점에 설정하여 읽기 요청이 할 수 없는 것을 방지하기 위해
  트레이스를 전처리합니다. 데이터 읽기, 4, 시뮬레이트() 예) 핵심 처리 함수인 추적 파일은 이 함수에 의해 읽혀지고 처리됩니다.
  5. statistic_output() 함수는 ssd 구조의 정보를 출력 파일에 출력하고 출력은 다음과 같습니다.
  통계 데이터 및 평균 데이터, 출력 파일은 작고 trace_output 파일은 크고 상세함 6. free_all_node() 함수는 전체 메인 함수에서 적용된 노드를 해제
 *********************************************************************************************************************************/
int  main()
{
    unsigned  int i,j,k;
    struct ssd_info *ssd;

#ifdef DEBUG
    printf("enter main\n"); 
#endif

    ssd=(struct ssd_info*)malloc(sizeof(struct ssd_info));
    alloc_assert(ssd,"ssd");
    memset(ssd,0, sizeof(struct ssd_info));

    ssd=initiation(ssd);
    make_aged(ssd);
    pre_process_page(ssd);

    for (i=0;i<ssd->parameter->channel_number;i++)
    {
        for (j=0;j<ssd->parameter->die_chip;j++)
        {
            for (k=0;k<ssd->parameter->plane_die;k++)
            {
                printf("%d,0,%d,%d:  %5d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);
            }
        }
    }
    fprintf(ssd->outputfile,"\t\t\t\t\t\t\t\t\tOUTPUT\n");
    fprintf(ssd->outputfile,"****************** TRACE INFO ******************\n");

    ssd=simulate(ssd);
    statistic_output(ssd);  
    /*	free_all_node(ssd);*/

    printf("\n");
    printf("the simulation is completed!\n");

    return 1;
    /* 	_CrtDumpMemoryLeaks(); */
}


/******************simulate() *********************************************************************
 *simulate()는 핵심 처리 기능이며 주요 기능은 다음과 같습니다.
 *1, 추적 파일에서 요청을 받아 ssd->request에 매달아 두십시오.
 *2, ssd에 읽기 요청을 별도로 처리하는 dram이 있는지 여부에 따라 이러한 요청을 읽기 및 쓰기 하위 요청으로 처리하고 ssd->channel 또는 ssd에 중단
 *3, 이러한 읽기 및 쓰기 하위 요청은 이벤트의 순서에 따라 처리됩니다.
 *4, 각 요청의 하위 요청을 출력 파일 파일로 처리한 후 관련 정보를 출력합니다.
 **************************************************************************************************/
struct ssd_info *simulate(struct ssd_info *ssd)
{
    int flag=1,flag1=0;
    double output_step=0;
    unsigned int a=0,b=0;
    //errno_t err;

    printf("\n");
    printf("begin simulating.......................\n");
    printf("\n");
    printf("\n");
    printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

    ssd->tracefile = fopen(ssd->tracefilename,"r");
    if(ssd->tracefile == NULL)
    {  
        printf("the trace file can't open\n");
        return NULL;
    }

    fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time\n");	
    fflush(ssd->outputfile);

    while(flag!=100)      
    {

        flag=get_requests(ssd);

        if(flag == 1)
        {   
            //printf("once\n");
            if (ssd->parameter->dram_capacity!=0)
            {
                buffer_management(ssd);  
                distribute(ssd); 
            } 
            else
            {
                no_buffer_distribute(ssd);
            }		
        }

        process(ssd);    
        trace_output(ssd);
        if(flag == 0 && ssd->request_queue == NULL)
            flag = 100;
    }

    fclose(ssd->tracefile);
    return ssd;
}



/********    get_request    ******************************************************
 * 1. 이미 도착한 요청 가져오기
 * 2. 해당 요청 노드를 ssd->reuqest_queue에 추가합니다.
 * return 0: 추적 끝에 도달
 * -1: 추가된 요청이 없습니다.
 * 1: 목록에 하나의 요청 추가
 *SSD 시뮬레이터에는 세 가지 드라이브 모드가 있습니다. 클럭 드라이브(정확함, 너무 느림) 이벤트 드라이브(이 프로그램에서 사용) 트레이스 드라이브(),
 *이벤트를 진행하는 두 가지 방법: 채널/칩 상태 변경, 추적 파일 요청 도착.
 *채널/칩 상태 변경 및 추적 파일 요청 도착은 현재 상태에서 도착할 때마다 타임라인에 흩어져 있는 지점입니다.
 *다음 상태는 가장 가까운 상태에 도달해야 하며, 한 지점에 도달할 때마다 프로세스를 실행합니다.
 ********************************************************************************/
int get_requests(struct ssd_info *ssd)  
{  
    char buffer[200];
    unsigned int lsn=0;
    int device,  size, ope,large_lsn, i = 0,j=0;
    struct request *request1;
    int flag = 1;
    long filepoint; 
    int64_t time_t = 0;
    int64_t nearest_event_time;    

#ifdef DEBUG
    printf("enter get_requests,  current time:%lld\n",ssd->current_time);
#endif

    if(feof(ssd->tracefile))
        return 0; 

    filepoint = ftell(ssd->tracefile);	
    fgets(buffer, 200, ssd->tracefile); 
    sscanf(buffer,"%lld %d %d %d %d",&time_t,&device,&lsn,&size,&ope);

    if ((device<0)&&(lsn<0)&&(size<0)&&(ope<0))
    {
        return 100;
    }
    if (lsn<ssd->min_lsn) 
        ssd->min_lsn=lsn;
    if (lsn>ssd->max_lsn)
        ssd->max_lsn=lsn;
    /******************************************************************************************************
     *상위 파일 시스템에서 SSD로 보내는 모든 읽기 및 쓰기 명령에는 두 부분(LSN, 크기)이 포함됩니다. LSN은 논리 섹터 번호입니다.
     *저장 공간은 선형 연속 공간입니다. 예를 들어, 읽기 요청(260, 6)은 섹터 번호 260부터 시작하여 총 6개의 섹터인 논리 섹터를 읽어야 함을 나타냅니다.
     *large_lsn: 채널 아래에 있는 서브페이지 수, 즉 섹터 수. 초과 제공 계수: SSD의 모든 공간을 사용자가 사용할 수 있는 것은 아닙니다.
     *예를 들어, 32G SSD에는 다른 용도로 10%의 공간이 예약되어 있을 수 있으므로 1을 곱하십시오.
     ***********************************************************************************************************/
    large_lsn=(int)((ssd->parameter->subpage_page*ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->plane_die*ssd->parameter->die_chip*ssd->parameter->chip_num)*(1-ssd->parameter->overprovide));
    lsn = lsn%large_lsn;

    nearest_event_time=find_nearest_event(ssd);
    if (nearest_event_time==MAX_INT64)
    {
        ssd->current_time=time_t;           

        //if (ssd->request_queue_length>ssd->parameter->queue_length)    //如果请求队列的长度超过了配置文件中所设置的长度                     
        //{
        //printf("error in get request , the queue length is too long\n");
        //}
    }
    else
    {   
        if(nearest_event_time<time_t)
        {
            /*******************************************************************************
             *Rollback 즉, ssd->current_time에 time_t가 할당되지 않은 경우 추적 파일에서 읽은 레코드가 롤백됩니다.
             *filepoint는 fgets를 실행하기 전에 파일 포인터 위치를 기록하고 파일 헤더 + filepoint로 롤백합니다.
             *int fseek(FILE *stream, long offset, int fromwhere); 이 함수는 파일 포인터 스트림의 위치를 ​​설정합니다.
             * 실행이 성공하면 스트림은 fromwhere(오프셋 시작 위치: 파일 헤드 0, 현재 위치 1, 파일 테일 2)를 참조로 가리키고,
             * 오프셋(포인터 오프셋) 바이트에서의 위치. 오프셋이 파일 자체의 크기를 초과하는 것과 같이 실행이 실패하면 스트림이 가리키는 위치가 변경되지 않습니다.
             *텍스트 파일은 파일 헤더 0의 위치 지정 방법만 사용할 수 있습니다. 이 프로그램에서 파일 열기 방법은 "r"입니다. 텍스트 파일을 읽기 전용 모드로 엽니다.
             **********************************************************************************/
            fseek(ssd->tracefile,filepoint,0); 
            if(ssd->current_time<=nearest_event_time)
                ssd->current_time=nearest_event_time;
            return -1;
        }
        else
        {
            if (ssd->request_queue_length>=ssd->parameter->queue_length)
            {
                fseek(ssd->tracefile,filepoint,0);
                ssd->current_time=nearest_event_time;
                return -1;
            } 
            else
            {
                ssd->current_time=time_t;
            }
        }
    }

    if(time_t < 0)
    {
        printf("error!\n");
        while(1){}
    }

    if(feof(ssd->tracefile))
    {
        request1=NULL;
        return 0;
    }

    request1 = (struct request*)malloc(sizeof(struct request));
    alloc_assert(request1,"request");
    memset(request1,0, sizeof(struct request));

    request1->time = time_t;
    request1->lsn = lsn;
    request1->size = size;
    request1->operation = ope;	
    request1->begin_time = time_t;
    request1->response_time = 0;	
    request1->energy_consumption = 0;	
    request1->next_node = NULL;
    request1->distri_flag = 0;              // indicate whether this request has been distributed already
    request1->subs = NULL;
    request1->need_distr_flag = NULL;
    request1->complete_lsn_count=0;         //record the count of lsn served by buffer
    filepoint = ftell(ssd->tracefile);		// set the file point

    if(ssd->request_queue == NULL)          //The queue is empty
    {
        ssd->request_queue = request1;
        ssd->request_tail = request1;
        ssd->request_queue_length++;
    }
    else
    {			
        (ssd->request_tail)->next_node = request1;	
        ssd->request_tail = request1;			
        ssd->request_queue_length++;
    }

    if (request1->operation==1)             //쓰기에 대한 읽기 0에 대한 평균 요청 크기 1 계산
    {
        ssd->ave_read_size=(ssd->ave_read_size*ssd->read_request_count+request1->size)/(ssd->read_request_count+1);
    } 
    else
    {
        ssd->ave_write_size=(ssd->ave_write_size*ssd->write_request_count+request1->size)/(ssd->write_request_count+1);
    }


    filepoint = ftell(ssd->tracefile);	
    fgets(buffer, 200, ssd->tracefile);    //다음 요청의 도착 시간 찾기
    sscanf(buffer,"%lld %d %d %d %d",&time_t,&device,&lsn,&size,&ope);
    ssd->next_request_time=time_t;
    fseek(ssd->tracefile,filepoint,0);

    return 1;
}

/**********************************************************************************************************************************************
 *먼저, 버퍼는 쓰기 요청을 처리하기 위한 쓰기 버퍼인데, 플래시를 읽는 시간 tR이 20us이고 플래시를 쓰는 시간이 tprog인 200us이기 때문에 쓰기 요청을 처리하는 시간을 절약할 수 있습니다. 쓰기 서비스.
 * 읽기 동작: 버퍼가 히트하면 버퍼에서 읽고, 버퍼가 히트하지 않으면 채널의 I/O 버스를 점유하지 말고, 플래시에서 읽고, 채널의 I/O 버스를 점유하지만 버퍼에 들어가지 마십시오.
 * 쓰기 작업: 먼저 요청을 sub_request 하위 요청으로 나눕니다.동적으로 할당된 경우 sub_request는 ssd->sub_request에 중단됩니다.
 *          정적으로 할당된 경우 sub_request는 채널의 sub_request 체인에 연결되며, sub_request가 동적으로 할당되는지 정적으로 할당되는지 여부에 관계없이 요청의 sub_request 체인에 연결되어야 합니다.
 *          요청이 처리될 때마다 요청에 대한 정보가 traceoutput 파일에 출력되어야 하기 때문입니다. sub_request를 처리한 후 채널의 sub_request 체인에서 제거합니다.
 *          또는 ssd의 sub_request 체인을 제거하지만 traceoutput 파일이 하나를 출력한 후 요청의 sub_request 체인을 지웁니다.
 *          sub_request가 버퍼에 적중하면 버퍼에 쓰기만 하고 sub_page를 버퍼 체인 헤드(LRU)에 참조하게 하고, 적중하지 않고 버퍼가 가득 차면 버퍼 체인 테일의 sub_request가 먼저
 *          플래시에 쓰기(이렇게 하면 이 요청 요청의 sub_request 체인에 연결될 sub_request 쓰기 요청이 생성되고 동적으로 할당되는지 아니면 정적으로 할당되는지 여부에 따라 채널 또는 ssd에 연결됩니다.
 *          sub_request chain), 버퍼 체인 헤드에 쓸 sub_page 쓰기
 ***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{   
    unsigned int j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0, state,full_page;
    unsigned int flag=0,need_distb_flag,lsn_flag,flag1=1,active_region_flag=0;           
    struct request *new_request;
    struct buffer_group *buffer_node,key;
    unsigned int mask=0,offset1=0,offset2=0;

#ifdef DEBUG
    printf("enter buffer_management,  current time:%lld\n",ssd->current_time);
#endif
    ssd->dram->current_time=ssd->current_time;
    full_page=~(0xffffffff<<ssd->parameter->subpage_page);

    new_request=ssd->request_tail;
    lsn=new_request->lsn;
    lpn=new_request->lsn/ssd->parameter->subpage_page;
    last_lpn=(new_request->lsn+new_request->size-1)/ssd->parameter->subpage_page;
    first_lpn=new_request->lsn/ssd->parameter->subpage_page;

    new_request->need_distr_flag=(unsigned int*)malloc(sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
    alloc_assert(new_request->need_distr_flag,"new_request->need_distr_flag");
    memset(new_request->need_distr_flag, 0, sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));

    if(new_request->operation==READ) 
    {		
        while(lpn<=last_lpn)      		
        {
            /************************************************************************************************
             *need_distb_flag는 분배 함수가 실행되어야 하는지 여부를 나타내며, 1은 버퍼가 아닌 실행되어야 함을 나타내며, 0은 실행할 필요가 없음을 나타냅니다.
             *즉, 1은 분배가 필요함을 의미하고 0은 분배가 필요하지 않음을 의미하며 해당하는 모든 포인트가 초기에 1로 할당됨
             *************************************************************************************************/
            need_distb_flag=full_page;   
            key.group=lpn;
            buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);		// buffer node 

            while((buffer_node!=NULL)&&(lsn<(lpn+1)*ssd->parameter->subpage_page)&&(lsn<=(new_request->lsn+new_request->size-1)))             			
            {             	
                lsn_flag=full_page;
                mask=1 << (lsn%ssd->parameter->subpage_page);
                if(mask>31)
                {
                    printf("the subpage number is larger than 32!add some cases");
                    getchar(); 		   
                }
                else if((buffer_node->stored & mask)==mask)
                {
                    flag=1;
                    lsn_flag=lsn_flag&(~mask);
                }

                if(flag==1)				
                {	//버퍼 노드가 버퍼의 큐 헤드에 없으면 해당 노드를 큐의 헤드로 참조해야 하며 LRU 알고리즘을 구현하는 양방향 큐입니다.	       		
                    if(ssd->dram->buffer->buffer_head!=buffer_node)     
                    {		
                        if(ssd->dram->buffer->buffer_tail==buffer_node)								
                        {			
                            buffer_node->LRU_link_pre->LRU_link_next=NULL;					
                            ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;							
                        }				
                        else								
                        {				
                            buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;				
                            buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;								
                        }								
                        buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;
                        ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;
                        buffer_node->LRU_link_pre=NULL;			
                        ssd->dram->buffer->buffer_head=buffer_node;													
                    }						
                    ssd->dram->buffer->read_hit++;					
                    new_request->complete_lsn_count++;											
                }		
                else if(flag==0)
                {
                    ssd->dram->buffer->read_miss_hit++;
                }

                need_distb_flag=need_distb_flag&lsn_flag;

                flag=0;		
                lsn++;						
            }	

            index=(lpn-first_lpn)/(32/ssd->parameter->subpage_page); 			
            new_request->need_distr_flag[index]=new_request->need_distr_flag[index]|(need_distb_flag<<(((lpn-first_lpn)%(32/ssd->parameter->subpage_page))*ssd->parameter->subpage_page));	
            lpn++;

        }
    }  
    else if(new_request->operation==WRITE)
    {
        while(lpn<=last_lpn)           	
        {	
            need_distb_flag=full_page;
            mask=~(0xffffffff<<(ssd->parameter->subpage_page));
            state=mask;

            if(lpn==first_lpn)
            {
                offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-new_request->lsn);
                state=state&(0xffffffff<<offset1);
            }
            if(lpn==last_lpn)
            {
                offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(new_request->lsn+new_request->size));
                state=state&(~(0xffffffff<<offset2));
            }

            ssd=insert2buffer(ssd, lpn, state,NULL,new_request);
            lpn++;
        }
    }
    complete_flag = 1;
    for(j=0;j<=(last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32;j++)
    {
        if(new_request->need_distr_flag[j] != 0)
        {
            complete_flag = 0;
        }
    }

    /*************************************************************
     *요청이 버퍼에 의해 완전히 서비스된 경우 요청에 직접 응답할 수 있으며 결과가 출력됩니다.
     *여기서는 dram의 서비스 시간을 1000ns로 가정합니다.
     **************************************************************/
    if((complete_flag == 1)&&(new_request->subs==NULL))               
    {
        new_request->begin_time=ssd->current_time;
        new_request->response_time=ssd->current_time+1000;            
    }

    return ssd;
}

/*****************************
 *lpn에서 ppn으로 변환
 ******************************/
unsigned int lpn2ppn(struct ssd_info *ssd,unsigned int lsn)
{
    int lpn, ppn;	
    struct entry *p_map = ssd->dram->map->map_entry;
#ifdef DEBUG
    printf("enter lpn2ppn,  current time:%lld\n",ssd->current_time);
#endif
    lpn = lsn/ssd->parameter->subpage_page;			//lpn
    ppn = (p_map[lpn]).pn;
    return ppn;
}

/**********************************************************************************
 *읽기 요청 할당 하위 요청 기능, 여기서 읽기 요청만 처리, 쓰기 요청은 buffer_management() 함수에서 처리됨
 *요청 큐 및 버퍼 히트 확인에 따라 각 요청은 하위 요청으로 분해되고, 하위 요청 큐는 채널에 매달리며,
 *다른 채널에는 자체 하위 요청 대기열이 있습니다.
 **********************************************************************************/

struct ssd_info *distribute(struct ssd_info *ssd) 
{
    unsigned int start, end, first_lsn,last_lsn,lpn,flag=0,flag_attached=0,full_page;
    unsigned int j, k, sub_size;
    int i=0;
    struct request *req;
    struct sub_request *sub;
    int* complt;

#ifdef DEBUG
    printf("enter distribute,  current time:%lld\n",ssd->current_time);
#endif
    full_page=~(0xffffffff<<ssd->parameter->subpage_page);

    req = ssd->request_tail;
    if(req->response_time != 0){
        return ssd;
    }
    if (req->operation==WRITE)
    {
        return ssd;
    }

    if(req != NULL)
    {
        if(req->distri_flag == 0)
        {
            //아직 처리할 읽기 요청이 있는 경우
            if(req->complete_lsn_count != ssd->request_tail->size)
            {		
                first_lsn = req->lsn;				
                last_lsn = first_lsn + req->size;
                complt = req->need_distr_flag;
                start = first_lsn - first_lsn % ssd->parameter->subpage_page;
                end = (last_lsn/ssd->parameter->subpage_page + 1) * ssd->parameter->subpage_page;
                i = (end - start)/32;	

                while(i >= 0)
                {	
                    /*************************************************************************************
                     *32비트 정수 데이터의 각 비트는 서브페이지를 나타내며, 32/ssd->parameter->subpage_page는 페이지가 몇 개인지,
                     *여기에 있는 각 페이지의 상태는 req->need_distr_flag에 저장됩니다. 즉, complt의 비교를 통해 complt에 저장됩니다.
                     *full_page가 있는 각 항목은 이 페이지가 처리되었는지 여부를 알 수 있습니다. 처리되지 않은 경우 creat_sub_request 전달
                     *이 함수는 하위 요청을 생성합니다.
                     *************************************************************************************/
                    for(j=0; j<32/ssd->parameter->subpage_page; j++)
                    {	
                        k = (complt[((end-start)/32-i)] >>(ssd->parameter->subpage_page*j)) & full_page;	
                        if (k !=0)
                        {
                            lpn = start/ssd->parameter->subpage_page+ ((end-start)/32-i)*32/ssd->parameter->subpage_page + j;
                            sub_size=transfer_size(ssd,k,lpn,req);    
                            if (sub_size==0) 
                            {
                                continue;
                            }
                            else
                            {
                                sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation);
                            }	
                        }
                    }
                    i = i-1;
                }

            }
            else
            {
                req->begin_time=ssd->current_time;
                req->response_time=ssd->current_time+1000;   
            }

        }
    }
    return ssd;
}


/**********************************************************************
 *trace_output() 함수는 각 요청의 모든 하위 요청이 process() 함수에 의해 처리된 후,
 *해당 실행 결과를 출력 파일 파일에 인쇄합니다. 여기서 결과는 주로 실행 시간입니다.
 **********************************************************************/
void trace_output(struct ssd_info* ssd){
    int flag = 1;	
    int64_t start_time, end_time;
    struct request *req, *pre_node;
    struct sub_request *sub, *tmp;

#ifdef DEBUG
    printf("enter trace_output,  current time:%lld\n",ssd->current_time);
#endif

    pre_node=NULL;
    req = ssd->request_queue;
    start_time = 0;
    end_time = 0;

    if(req == NULL)
        return;

    while(req != NULL)	
    {
        sub = req->subs;
        flag = 1;
        start_time = 0;
        end_time = 0;
        if(req->response_time != 0)
        {
            fprintf(ssd->outputfile,"%16lld %10d %6d %2d %16lld %16lld %10lld\n",req->time,req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time-req->time);
            fflush(ssd->outputfile);

            if(req->response_time-req->begin_time==0)
            {
                printf("the response time is 0?? \n");
                getchar();
            }

            if (req->operation==READ)
            {
                ssd->read_request_count++;
                ssd->read_avg=ssd->read_avg+(req->response_time-req->time);
            } 
            else
            {
                ssd->write_request_count++;
                ssd->write_avg=ssd->write_avg+(req->response_time-req->time);
            }

            if(pre_node == NULL)
            {
                if(req->next_node == NULL)
                {
                    free(req->need_distr_flag);
                    req->need_distr_flag=NULL;
                    free(req);
                    req = NULL;
                    ssd->request_queue = NULL;
                    ssd->request_tail = NULL;
                    ssd->request_queue_length--;
                }
                else
                {
                    ssd->request_queue = req->next_node;
                    pre_node = req;
                    req = req->next_node;
                    free(pre_node->need_distr_flag);
                    pre_node->need_distr_flag=NULL;
                    free((void *)pre_node);
                    pre_node = NULL;
                    ssd->request_queue_length--;
                }
            }
            else
            {
                if(req->next_node == NULL)
                {
                    pre_node->next_node = NULL;
                    free(req->need_distr_flag);
                    req->need_distr_flag=NULL;
                    free(req);
                    req = NULL;
                    ssd->request_tail = pre_node;
                    ssd->request_queue_length--;
                }
                else
                {
                    pre_node->next_node = req->next_node;
                    free(req->need_distr_flag);
                    req->need_distr_flag=NULL;
                    free((void *)req);
                    req = pre_node->next_node;
                    ssd->request_queue_length--;
                }
            }
        }
        else
        {
            flag=1;
            while(sub != NULL)
            {
                if(start_time == 0)
                    start_time = sub->begin_time;
                if(start_time > sub->begin_time)
                    start_time = sub->begin_time;
                if(end_time < sub->complete_time)
                    end_time = sub->complete_time;
                if((sub->current_state == SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=ssd->current_time)))	// if any sub-request is not completed, the request is not completed
                {
                    sub = sub->next_subs;
                }
                else
                {
                    flag=0;
                    break;
                }

            }

            if (flag == 1)
            {		
                //fprintf(ssd->outputfile,"%10I64u %10u %6u %2u %16I64u %16I64u %10I64u\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time);
                fprintf(ssd->outputfile,"%16lld %10d %6d %2d %16lld %16lld %10lld\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time);
                fflush(ssd->outputfile);

                if(end_time-start_time==0)
                {
                    printf("the response time is 0?? \n");
                    getchar();
                }

                if (req->operation==READ)
                {
                    ssd->read_request_count++;
                    ssd->read_avg=ssd->read_avg+(end_time-req->time);
                } 
                else
                {
                    ssd->write_request_count++;
                    ssd->write_avg=ssd->write_avg+(end_time-req->time);
                }

                while(req->subs!=NULL)
                {
                    tmp = req->subs;
                    req->subs = tmp->next_subs;
                    if (tmp->update!=NULL)
                    {
                        free(tmp->update->location);
                        tmp->update->location=NULL;
                        free(tmp->update);
                        tmp->update=NULL;
                    }
                    free(tmp->location);
                    tmp->location=NULL;
                    free(tmp);
                    tmp=NULL;

                }

                if(pre_node == NULL)
                {
                    if(req->next_node == NULL)
                    {
                        free(req->need_distr_flag);
                        req->need_distr_flag=NULL;
                        free(req);
                        req = NULL;
                        ssd->request_queue = NULL;
                        ssd->request_tail = NULL;
                        ssd->request_queue_length--;
                    }
                    else
                    {
                        ssd->request_queue = req->next_node;
                        pre_node = req;
                        req = req->next_node;
                        free(pre_node->need_distr_flag);
                        pre_node->need_distr_flag=NULL;
                        free(pre_node);
                        pre_node = NULL;
                        ssd->request_queue_length--;
                    }
                }
                else
                {
                    if(req->next_node == NULL)
                    {
                        pre_node->next_node = NULL;
                        free(req->need_distr_flag);
                        req->need_distr_flag=NULL;
                        free(req);
                        req = NULL;
                        ssd->request_tail = pre_node;	
                        ssd->request_queue_length--;
                    }
                    else
                    {
                        pre_node->next_node = req->next_node;
                        free(req->need_distr_flag);
                        req->need_distr_flag=NULL;
                        free(req);
                        req = pre_node->next_node;
                        ssd->request_queue_length--;
                    }

                }
            }
            else
            {	
                pre_node = req;
                req = req->next_node;
            }
        }		
    }
}


/*******************************************************************************
 *statistic_output() 함수는 주로 요청 처리 후 관련 처리 정보를 출력한다.
 *1, 각 평면의 지우기 시간, 즉 plane_erase와 총 지우기 시간, 즉 지우기를 계산합니다.
 *2, min_lsn, max_lsn, read_count, program_count 및 기타 통계 정보를 파일 outputfile에 인쇄합니다.
 *3, 같은 정보를 statisticfile 파일에 출력
 *******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
    unsigned int lpn_count=0,i,j,k,m,erase=0,plane_erase=0;
    double gc_energy=0.0;
#ifdef DEBUG
    printf("enter statistic_output,  current time:%lld\n",ssd->current_time);
#endif

    for(i=0;i<ssd->parameter->channel_number;i++)
    {
        for(j=0;j<ssd->parameter->die_chip;j++)
        {
            for(k=0;k<ssd->parameter->plane_die;k++)
            {
                plane_erase=0;
                for(m=0;m<ssd->parameter->block_plane;m++)
                {
                    if(ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count>0)
                    {
                        erase=erase+ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
                        plane_erase+=ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
                    }
                }
                fprintf(ssd->outputfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,m,plane_erase);
                fprintf(ssd->statisticfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,m,plane_erase);
            }
        }
    }

    fprintf(ssd->outputfile,"\n");
    fprintf(ssd->outputfile,"\n");
    fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");	 
    fprintf(ssd->outputfile,"min lsn: %13d\n",ssd->min_lsn);	
    fprintf(ssd->outputfile,"max lsn: %13d\n",ssd->max_lsn);
    fprintf(ssd->outputfile,"read count: %13d\n",ssd->read_count);	  
    fprintf(ssd->outputfile,"program count: %13d",ssd->program_count);	
    fprintf(ssd->outputfile,"                        include the flash write count leaded by read requests\n");
    fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
    fprintf(ssd->outputfile,"erase count: %13d\n",ssd->erase_count);
    fprintf(ssd->outputfile,"direct erase count: %13d\n",ssd->direct_erase_count);
    fprintf(ssd->outputfile,"copy back count: %13d\n",ssd->copy_back_count);
    fprintf(ssd->outputfile,"multi-plane program count: %13d\n",ssd->m_plane_prog_count);
    fprintf(ssd->outputfile,"multi-plane read count: %13d\n",ssd->m_plane_read_count);
    fprintf(ssd->outputfile,"interleave write count: %13d\n",ssd->interleave_count);
    fprintf(ssd->outputfile,"interleave read count: %13d\n",ssd->interleave_read_count);
    fprintf(ssd->outputfile,"interleave two plane and one program count: %13d\n",ssd->inter_mplane_prog_count);
    fprintf(ssd->outputfile,"interleave two plane count: %13d\n",ssd->inter_mplane_count);
    fprintf(ssd->outputfile,"gc copy back count: %13d\n",ssd->gc_copy_back);
    fprintf(ssd->outputfile,"write flash count: %13d\n",ssd->write_flash_count);
    fprintf(ssd->outputfile,"interleave erase count: %13d\n",ssd->interleave_erase_count);
    fprintf(ssd->outputfile,"multiple plane erase count: %13d\n",ssd->mplane_erase_conut);
    fprintf(ssd->outputfile,"interleave multiple plane erase count: %13d\n",ssd->interleave_mplane_erase_count);
    fprintf(ssd->outputfile,"read request count: %13d\n",ssd->read_request_count);
    fprintf(ssd->outputfile,"write request count: %13d\n",ssd->write_request_count);
    fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
    fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
    fprintf(ssd->outputfile,"read request average response time: %lld\n",ssd->read_avg/ssd->read_request_count);
    fprintf(ssd->outputfile,"write request average response time: %lld\n",ssd->write_avg/ssd->write_request_count);
    fprintf(ssd->outputfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
    fprintf(ssd->outputfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
    fprintf(ssd->outputfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
    fprintf(ssd->outputfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
    fprintf(ssd->outputfile,"erase: %13d\n",erase);
    fflush(ssd->outputfile);

    fclose(ssd->outputfile);


    fprintf(ssd->statisticfile,"\n");
    fprintf(ssd->statisticfile,"\n");
    fprintf(ssd->statisticfile,"---------------------------statistic data---------------------------\n");	
    fprintf(ssd->statisticfile,"min lsn: %13d\n",ssd->min_lsn);	
    fprintf(ssd->statisticfile,"max lsn: %13d\n",ssd->max_lsn);
    fprintf(ssd->statisticfile,"read count: %13d\n",ssd->read_count);	  
    fprintf(ssd->statisticfile,"program count: %13d",ssd->program_count);	  
    fprintf(ssd->statisticfile,"                        include the flash write count leaded by read requests\n");
    fprintf(ssd->statisticfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
    fprintf(ssd->statisticfile,"erase count: %13d\n",ssd->erase_count);	  
    fprintf(ssd->statisticfile,"direct erase count: %13d\n",ssd->direct_erase_count);
    fprintf(ssd->statisticfile,"copy back count: %13d\n",ssd->copy_back_count);
    fprintf(ssd->statisticfile,"multi-plane program count: %13d\n",ssd->m_plane_prog_count);
    fprintf(ssd->statisticfile,"multi-plane read count: %13d\n",ssd->m_plane_read_count);
    fprintf(ssd->statisticfile,"interleave count: %13d\n",ssd->interleave_count);
    fprintf(ssd->statisticfile,"interleave read count: %13d\n",ssd->interleave_read_count);
    fprintf(ssd->statisticfile,"interleave two plane and one program count: %13d\n",ssd->inter_mplane_prog_count);
    fprintf(ssd->statisticfile,"interleave two plane count: %13d\n",ssd->inter_mplane_count);
    fprintf(ssd->statisticfile,"gc copy back count: %13d\n",ssd->gc_copy_back);
    fprintf(ssd->statisticfile,"write flash count: %13d\n",ssd->write_flash_count);
    fprintf(ssd->statisticfile,"waste page count: %13d\n",ssd->waste_page_count);
    fprintf(ssd->statisticfile,"interleave erase count: %13d\n",ssd->interleave_erase_count);
    fprintf(ssd->statisticfile,"multiple plane erase count: %13d\n",ssd->mplane_erase_conut);
    fprintf(ssd->statisticfile,"interleave multiple plane erase count: %13d\n",ssd->interleave_mplane_erase_count);
    fprintf(ssd->statisticfile,"read request count: %13d\n",ssd->read_request_count);
    fprintf(ssd->statisticfile,"write request count: %13d\n",ssd->write_request_count);
    fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
    fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
    fprintf(ssd->statisticfile,"read request average response time: %lld\n",ssd->read_avg/ssd->read_request_count);
    fprintf(ssd->statisticfile,"write request average response time: %lld\n",ssd->write_avg/ssd->write_request_count);
    fprintf(ssd->statisticfile,"buffer read hits: %13d\n",ssd->dram->buffer->read_hit);
    fprintf(ssd->statisticfile,"buffer read miss: %13d\n",ssd->dram->buffer->read_miss_hit);
    fprintf(ssd->statisticfile,"buffer write hits: %13d\n",ssd->dram->buffer->write_hit);
    fprintf(ssd->statisticfile,"buffer write miss: %13d\n",ssd->dram->buffer->write_miss_hit);
    fprintf(ssd->statisticfile,"erase: %13d\n",erase);
    fflush(ssd->statisticfile);

    fclose(ssd->statisticfile);
}


/***********************************************************************************
 * 각 페이지의 상태에 따라 처리할 하위 페이지의 수, 즉 하위 요청에 의해 처리될 하위 페이지의 수를 계산합니다.
 ************************************************************************************/
unsigned int size(unsigned int stored)
{
    unsigned int i,total=0,mask=0x80000000;

#ifdef DEBUG
    printf("enter size\n");
#endif
    for(i=1;i<=32;i++)
    {
        if(stored & mask) total++;
        stored<<=1;
    }
#ifdef DEBUG
    printf("leave size\n");
#endif
    return total;
}


/*********************************************************
 *transfer_size() 함수의 함수는 처리해야 하는 하위 요청의 크기를 계산하는 것입니다.
 *first_lpn 및 last_lpn의 두 가지 특별한 경우는 함수에서 별도로 처리됩니다.
 *두 경우 모두 전체 페이지가 아니라 페이지의 일부가 처리될 가능성이 매우 높습니다.
 *lsn의 경우 페이지의 첫 번째 하위 페이지가 없을 수 있습니다.
 *********************************************************/
unsigned int transfer_size(struct ssd_info *ssd,int need_distribute,unsigned int lpn,struct request *req)
{
    unsigned int first_lpn,last_lpn,state,trans_size;
    unsigned int mask=0,offset1=0,offset2=0;

    first_lpn=req->lsn/ssd->parameter->subpage_page;
    last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;

    mask=~(0xffffffff<<(ssd->parameter->subpage_page));
    state=mask;
    if(lpn==first_lpn)
    {
        offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
        state=state&(0xffffffff<<offset1);
    }
    if(lpn==last_lpn)
    {
        offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
        state=state&(~(0xffffffff<<offset2));
    }

    trans_size=size(state&need_distribute);

    return trans_size;
}


/**********************************************************************************************************  
 *int64_t find_nearest_event(구조체 ssd_info *ssd)
 *모든 하위 요청 중 가장 빨리 도착한 다음 상태 시간을 찾고, 요청의 다음 상태 시간이 현재 시간보다 작거나 같으면 먼저 요청의 다음 상태 시간을 확인합니다.
 *요청이 차단되었음을 나타내며 해당 채널 또는 해당 다이의 다음 상태 시간을 확인해야 합니다. Int64는 부호 있는 64비트 정수 데이터 유형이며 값 유형은 다음 사이의 값을 나타냅니다.
 *-2^63( -9,223,372,036,854,775,808 )과 2^63-1(+9,223,372,036,854,775,807 ) 사이의 정수입니다. 저장 공간은 8바이트를 차지합니다.
 *채널과 다이는 이벤트 진행의 핵심 요소입니다. 세 가지 상황이 이벤트를 계속 진행하게 할 수 있습니다. 채널과 다이는 각각 유휴 상태로 돌아갑니다.
 * 데이터 읽기 준비
 ***********************************************************************************************************/
int64_t find_nearest_event(struct ssd_info *ssd) 
{
    unsigned int i,j;
    int64_t time=MAX_INT64;
    int64_t time1=MAX_INT64;
    int64_t time2=MAX_INT64;

    for (i=0;i<ssd->parameter->channel_number;i++)
    {
        if (ssd->channel_head[i].next_state==CHANNEL_IDLE)
            if(time1>ssd->channel_head[i].next_state_predict_time)
                if (ssd->channel_head[i].next_state_predict_time>ssd->current_time)    
                    time1=ssd->channel_head[i].next_state_predict_time;
        for (j=0;j<ssd->parameter->chip_channel[i];j++)
        {
            if ((ssd->channel_head[i].chip_head[j].next_state==CHIP_IDLE)||(ssd->channel_head[i].chip_head[j].next_state==CHIP_DATA_TRANSFER))
                if(time2>ssd->channel_head[i].chip_head[j].next_state_predict_time)
                    if (ssd->channel_head[i].chip_head[j].next_state_predict_time>ssd->current_time)    
                        time2=ssd->channel_head[i].chip_head[j].next_state_predict_time;	
        }   
    } 

    /*****************************************************************************************************
     *시간은 이하와 같다. A. 다음 상태는 CHANNEL_IDLE이고 다음 상태의 예상 시간은 ssd의 현재 시간 CHANNEL 다음 상태의 예상 시간보다 큽니다.
     *                  B. 다음 상태가 CHIP_IDLE이고 다음 상태 예상 시간이 ssd의 현재 시간보다 큰 DIE의 다음 상태 예상 시간
     *                  C. 다음 상태가 CHIP_DATA_TRANSFER이고 다음 상태 예상 시간이 ssd의 현재 시간보다 큰 DIE의 다음 상태 예상 시간
     *CHIP_DATA_TRANSFER 읽기 준비 상태, 데이터가 매체에서 레지스터로 전송됨, 다음 상태는 레지스터에서 버퍼로 전송된 최소값
     *요구 사항을 충족하는 시간이 없을 수 있으며 시간은 0x7ffffffffffffffff를 반환합니다.
     *****************************************************************************************************/
    time=(time1>time2)?time2:time1;
    return time;
}

/***********************************************
 *free_all_node()函数的作用就是释放所有申请的节点
 ************************************************/
void free_all_node(struct ssd_info *ssd)
{
    unsigned int i,j,k,l,n;
    struct buffer_group *pt=NULL;
    struct direct_erase * erase_node=NULL;
    for (i=0;i<ssd->parameter->channel_number;i++)
    {
        for (j=0;j<ssd->parameter->chip_channel[0];j++)
        {
            for (k=0;k<ssd->parameter->die_chip;k++)
            {
                for (l=0;l<ssd->parameter->plane_die;l++)
                {
                    for (n=0;n<ssd->parameter->block_plane;n++)
                    {
                        free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
                        ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head=NULL;
                    }
                    free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head=NULL;
                    while(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node!=NULL)
                    {
                        erase_node=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
                        ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node=erase_node->next_node;
                        free(erase_node);
                        erase_node=NULL;
                    }
                }

                free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
                ssd->channel_head[i].chip_head[j].die_head[k].plane_head=NULL;
            }
            free(ssd->channel_head[i].chip_head[j].die_head);
            ssd->channel_head[i].chip_head[j].die_head=NULL;
        }
        free(ssd->channel_head[i].chip_head);
        ssd->channel_head[i].chip_head=NULL;
    }
    free(ssd->channel_head);
    ssd->channel_head=NULL;

    avlTreeDestroy( ssd->dram->buffer);
    ssd->dram->buffer=NULL;

    free(ssd->dram->map->map_entry);
    ssd->dram->map->map_entry=NULL;
    free(ssd->dram->map);
    ssd->dram->map=NULL;
    free(ssd->dram);
    ssd->dram=NULL;
    free(ssd->parameter);
    ssd->parameter=NULL;

    free(ssd);
    ssd=NULL;
}


/*****************************************************************************
 *make_aged() 함수의 기능은 일정 기간 동안 사용된 실제 SSD를 시뮬레이션하는 것입니다.
 *그러면 ssd의 해당 매개변수가 변경되므로 이 기능은 본질적으로 ssd의 각 매개변수를 할당하는 것입니다.
 ******************************************************************************/
struct ssd_info *make_aged(struct ssd_info *ssd)
{
    unsigned int i,j,k,l,m,n,ppn;
    int threshould,flag=0;

    if (ssd->parameter->aged==1)
    {
        //임계값(threshould)은 plane에서 미리 무효화해야 하는 페이지 수를 나타냅니다.
        threshould=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio);  
        for (i=0;i<ssd->parameter->channel_number;i++)
            for (j=0;j<ssd->parameter->chip_channel[i];j++)
                for (k=0;k<ssd->parameter->die_chip;k++)
                    for (l=0;l<ssd->parameter->plane_die;l++)
                    {  
                        flag=0;
                        for (m=0;m<ssd->parameter->block_plane;m++)
                        {  
                            if (flag>=threshould)
                            {
                                break;
                            }
                            for (n=0;n<(ssd->parameter->page_block*ssd->parameter->aged_ratio+1);n++)
                            {  
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].valid_state=0;        //페이지가 유효하지 않으며 valid 및 free 상태가 모두 0으로 표시됨을 나타냅니다.
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].free_state=0;         //페이지가 유효하지 않으며 valid 및 free 상태가 모두 0으로 표시됨을 나타냅니다.
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].lpn=0;  //페이지가 유효하지 않음을 나타내려면 valid_state free_state lpn을 0으로 설정합니다. 테스트할 때 세 항목이 모두 감지됩니다. Lpn=0만으로도 유효한 페이지가 될 수 있습니다.
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num--;
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].invalid_page_num++;
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_page++;
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page--;
                                flag++;

                                ppn=find_ppn(ssd,i,j,k,l,m,n);

                            }
                        } 
                    }	 
    }  
    else
    {
        return ssd;
    }

    return ssd;
}


/*********************************************************************************************
 *no_buffer_distribute() 함수는 ssd에 dram이 없을 때 처리하는 것입니다.
 *읽기 및 쓰기 요청이므로 버퍼에서 검색할 필요가 없으며 creat_sub_request() 함수를 직접 사용하여 하위 요청을 생성한 후 처리합니다.
 *********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
    unsigned int lsn,lpn,last_lpn,first_lpn,complete_flag=0, state;
    unsigned int flag=0,flag1=1,active_region_flag=0;           //to indicate the lsn is hitted or not
    struct request *req=NULL;
    struct sub_request *sub=NULL,*sub_r=NULL,*update=NULL;
    struct local *loc=NULL;
    struct channel_info *p_ch=NULL;


    unsigned int mask=0; 
    unsigned int offset1=0, offset2=0;
    unsigned int sub_size=0;
    unsigned int sub_state=0;

    ssd->dram->current_time=ssd->current_time;
    req=ssd->request_tail;       
    lsn=req->lsn;
    lpn=req->lsn/ssd->parameter->subpage_page;
    last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;
    first_lpn=req->lsn/ssd->parameter->subpage_page;

    if(req->operation==READ)        
    {		
        while(lpn<=last_lpn) 		
        {
            sub_state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);
            sub_size=size(sub_state);
            sub=creat_sub_request(ssd,lpn,sub_size,sub_state,req,req->operation);
            lpn++;
        }
    }
    else if(req->operation==WRITE)
    {
        while(lpn<=last_lpn)     	
        {	
            mask=~(0xffffffff<<(ssd->parameter->subpage_page));
            state=mask;
            if(lpn==first_lpn)
            {
                offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
                state=state&(0xffffffff<<offset1);
            }
            if(lpn==last_lpn)
            {
                offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
                state=state&(~(0xffffffff<<offset2));
            }
            sub_size=size(state);

            sub=creat_sub_request(ssd,lpn,sub_size,state,req,req->operation);
            lpn++;
        }
    }

    return ssd;
}


