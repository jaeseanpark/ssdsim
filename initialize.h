/*****************************************************************************************************************************
  This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
  Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

  FileName： initialize.h
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
#ifndef INITIALIZE_H
#define INITIALIZE_H 10000

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include "avlTree.h"

#define SECTOR 512
#define BUFSIZE 200

#define DYNAMIC_ALLOCATION 0
#define STATIC_ALLOCATION 1

#define INTERLEAVE 0
#define TWO_PLANE 1

#define NORMAL    2
#define INTERLEAVE_TWO_PLANE 3
#define COPY_BACK	4

#define AD_RANDOM 1
#define AD_COPYBACK 2
#define AD_TWOPLANE 4
#define AD_INTERLEAVE 8
#define AD_TWOPLANE_READ 16

#define READ 1
#define WRITE 0

/*********************************all states of each objects************************************************
 *一下定义了channel的空闲，命令地址传输，数据传输，传输，其他等状态
 *还有chip的空闲，写忙，读忙，命令地址传输，数据传输，擦除忙，copyback忙，其他等状态
 *还有读写子请求（sub）的等待，读命令地址传输，读，读数据传输，写命令地址传输，写数据传输，写传输，完成等状态

 *채널의 유휴 상태, 명령 주소 전송, 데이터 전송, 전송 및 기타 상태 정의
 * 칩 유휴 상태, 쓰기 사용 중, 읽기 사용 중, 명령 주소 전송, 데이터 전송, 지우기 사용 중, 카피백 사용 중 및 기타 상태도 있습니다.
 * 읽기 및 쓰기 하위 요청(서브) 대기, 읽기 명령 주소 전송, 읽기, 읽기 데이터 전송, 쓰기 명령 주소 전송, 쓰기 데이터 전송, 쓰기 전송, 완료 및 기타 상태도 있습니다.
 ************************************************************************************************************/

#define CHANNEL_IDLE 000
#define CHANNEL_C_A_TRANSFER 3
#define CHANNEL_GC 4           
#define CHANNEL_DATA_TRANSFER 7
#define CHANNEL_TRANSFER 8
#define CHANNEL_UNKNOWN 9

#define CHIP_IDLE 100
#define CHIP_WRITE_BUSY 101
#define CHIP_READ_BUSY 102
#define CHIP_C_A_TRANSFER 103
#define CHIP_DATA_TRANSFER 107
#define CHIP_WAIT 108
#define CHIP_ERASE_BUSY 109
#define CHIP_COPYBACK_BUSY 110
#define UNKNOWN 111

#define SR_WAIT 200                 
#define SR_R_C_A_TRANSFER 201
#define SR_R_READ 202
#define SR_R_DATA_TRANSFER 203
#define SR_W_C_A_TRANSFER 204
#define SR_W_DATA_TRANSFER 205
#define SR_W_TRANSFER 206
#define SR_COMPLETE 299

#define REQUEST_IN 300         //다음 요청이 도착하면
#define OUTPUT 301             //다음 데이터 출력 시간

#define GC_WAIT 400
#define GC_ERASE_C_A 401
#define GC_COPY_BACK 402
#define GC_COMPLETE 403
#define GC_INTERRUPT 0
#define GC_UNINTERRUPT 1

#define CHANNEL(lsn) (lsn&0x0000)>>16      
#define chip(lsn) (lsn&0x0000)>>16 
#define die(lsn) (lsn&0x0000)>>16 
#define PLANE(lsn) (lsn&0x0000)>>16 
#define BLOKC(lsn) (lsn&0x0000)>>16 
#define PAGE(lsn) (lsn&0x0000)>>16 
#define SUBPAGE(lsn) (lsn&0x0000)>>16  

#define PG_SUB 0xffffffff			

/*****************************************
 * 함수 결과 상태 코드
 *Status는 함수 유형이고 값은 함수 결과 상태 코드입니다.
 ******************************************/
#define TRUE		1
#define FALSE		0
#define SUCCESS		1
#define FAILURE		0
#define ERROR		-1
#define INFEASIBLE	-2
#define OVERFLOW	-3
typedef int Status;     

struct ac_time_characteristics{
    int tPROG;     //program time
    int tDBSY;     //bummy busy time for two-plane program
    int tBERS;     //block erase time
    int tCLS;      //CLE setup time
    int tCLH;      //CLE hold time
    int tCS;       //CE setup time
    int tCH;       //CE hold time
    int tWP;       //WE pulse width
    int tALS;      //ALE setup time
    int tALH;      //ALE hold time
    int tDS;       //data setup time
    int tDH;       //data hold time
    int tWC;       //write cycle time
    int tWH;       //WE high hold time
    int tADL;      //address to data loading time
    int tR;        //data transfer from cell to register
    int tAR;       //ALE to RE delay
    int tCLR;      //CLE to RE delay
    int tRR;       //ready to RE low
    int tRP;       //RE pulse width
    int tWB;       //WE high to busy
    int tRC;       //read cycle time
    int tREA;      //RE access time
    int tCEA;      //CE access time
    int tRHZ;      //RE high to output hi-z
    int tCHZ;      //CE high to output hi-z
    int tRHOH;     //RE high to output hold
    int tRLOH;     //RE low to output hold
    int tCOH;      //CE high to output hold
    int tREH;      //RE high to output time
    int tIR;       //output hi-z to RE low
    int tRHW;      //RE high to WE low
    int tWHR;      //WE high to RE low
    int tRST;      //device resetting time
}ac_timing;


struct ssd_info{ 
    double ssd_energy;                   //SSD的能耗，是时间和芯片数的函数,能耗因子
    int64_t current_time;                //记录系统时间
    int64_t next_request_time;
    unsigned int real_time_subreq;       //전체 동적 할당의 경우 사용되는 실시간 쓰기 요청 수, 채널 우선 순위 기록
    int flag;
    int active_flag;                     //활성 쓰기 차단 여부를 기록합니다. 플런저가 발견되면 시간을 앞당길 필요가 있습니다. 0은 차단 없음, 1은 차단됨, 시간은 앞으로 진행해야 함을 의미합니다.
    unsigned int page;

    unsigned int token;                  //동적 할당에서 각 할당이 첫 번째 채널에서 토큰을 유지하는 것을 방지하기 위해 각 할당은 토큰이 가리키는 위치에서 시작합니다.
    unsigned int gc_request;             //SSD에 기록된 것 중, 현재 몇 개의 gc 작업 요청이 있는지

    unsigned int write_request_count;    //记录写操作的次数
    unsigned int read_request_count;     //记录读操作的次数
    int64_t write_avg;                   //记录用于计算写请求平均响应时间的时间
    int64_t read_avg;                    //记录用于计算读请求平均响应时间的时间

    unsigned int min_lsn;
    unsigned int max_lsn;
    unsigned long read_count;
    unsigned long program_count;
    unsigned long erase_count;
    unsigned long direct_erase_count;
    unsigned long copy_back_count;
    unsigned long m_plane_read_count;
    unsigned long m_plane_prog_count;
    unsigned long interleave_count;
    unsigned long interleave_read_count;
    unsigned long inter_mplane_count;
    unsigned long inter_mplane_prog_count;
    unsigned long interleave_erase_count;
    unsigned long mplane_erase_conut;
    unsigned long interleave_mplane_erase_count;
    unsigned long gc_copy_back;
    unsigned long write_flash_count;     //实际产生的对flash的写操作
    unsigned long waste_page_count;      //记录因为高级命令的限制导致的页浪费
    float ave_read_size;
    float ave_write_size;
    unsigned int request_queue_length;
    unsigned int update_read_count;      //记录因为更新操作导致的额外读出操作

    char parameterfilename[30];
    char tracefilename[30];
    char outputfilename[30];
    char statisticfilename[30];
    char statisticfilename2[30];

    FILE * outputfile;
    FILE * tracefile;
    FILE * statisticfile;
    FILE * statisticfile2;

    struct parameter_value *parameter;   //SSD参数因子
    struct dram_info *dram;
    struct request *request_queue;       //dynamic request queue
    struct request *request_tail;	     // the tail of the request queue
    struct sub_request *subs_w_head;     //当采用全动态分配时，分配是不知道应该挂载哪个channel上，所以先挂在ssd上，等进入process函数时才挂到相应的channel的读请求队列上
    struct sub_request *subs_w_tail;
    struct event_node *event;            //事件队列，每产生一个新的事件，按照时间顺序加到这个队列，在simulate函数最后，根据这个队列队首的时间，确定时间
    struct channel_info *channel_head;   //指向channel结构体数组的首地址
};


struct channel_info{
    int chip;                            //表示在该总线上有多少颗粒
    unsigned long read_count;
    unsigned long program_count;
    unsigned long erase_count;
    unsigned int token;                  //在动态分配中，为防止每次分配在第一个chip需要维持一个令牌，每次从令牌所指的位置开始分配

    int current_state;                   //channel has serveral states, including idle, command/address transfer,data transfer,unknown
    int next_state;
    int64_t current_time;                //记录该通道的当前时间
    int64_t next_state_predict_time;     //the predict time of next state, used to decide the sate at the moment

    struct event_node *event;
    struct sub_request *subs_r_head;     //channel上的读请求队列头，先服务处于队列头的子请求
    struct sub_request *subs_r_tail;     //channel上的读请求队列尾，新加进来的子请求加到队尾
    struct sub_request *subs_w_head;     //channel上的写请求队列头，先服务处于队列头的子请求
    struct sub_request *subs_w_tail;     //channel上的写请求队列，新加进来的子请求加到队尾
    struct gc_operation *gc_command;     //gc를 생성해야 하는 위치 기록
    struct chip_info *chip_head;        
};


struct chip_info{
    unsigned int die_num;               //表示一个颗粒中有多少个die
    unsigned int plane_num_die;         //indicate how many planes in a die
    unsigned int block_num_plane;       //indicate how many blocks in a plane
    unsigned int page_num_block;        //indicate how many pages in a block
    unsigned int subpage_num_page;      //indicate how many subpage in a page
    unsigned int ers_limit;             //该chip中每块能够被擦除的次数
    unsigned int token;                 //在动态分配中，为防止每次分配在第一个die需要维持一个令牌，每次从令牌所指的位置开始分配

    int current_state;                  //channel has serveral states, including idle, command/address transfer,data transfer,unknown
    int next_state;
    int64_t current_time;               //记录该通道的当前时间
    int64_t next_state_predict_time;    //the predict time of next state, used to decide the sate at the moment

    unsigned long read_count;           //how many read count in the process of workload
    unsigned long program_count;
    unsigned long erase_count;

    struct ac_time_characteristics ac_timing;  
    struct die_info *die_head;
};


struct die_info{

    unsigned int token;                 //동적 할당에서는 첫 번째 plane에서 각 할당을 방지하기 위해 토큰을 유지 관리해야 하며 각 할당은 토큰이 가리키는 위치에서 시작됩니다.
    struct plane_info *plane_head;

};


struct plane_info{
    int add_reg_ppn;                    //읽고 쓸 때 주소는 주소 레지스터를 나타내는 이 변수로 전송됩니다. 다이가 busy에서 idle로 변경되면 주소를 지우십시오. //일대다 매핑으로 인해 읽기 요청에 동일한 lpn이 여러 개 있을 수 있으므로 ppn을 사용하여 구별해야 합니다.
    unsigned int free_page;             //plane에 사용 가능한 페이지 수
    unsigned int ers_invalid;           //plane에서 지워진 블록의 수를 기록
    unsigned int active_block;          //if a die has a active block, 이 항목은 물리적 블록 번호를 나타냅니다.
    int can_erase_block;                //gc 연산에서 삭제될 평면에 기록된 블록, -1은 적절한 블록이 발견되지 않았음을 의미합니다. //记录在一个plane中准备在gc操作中被擦除操作的块,-1表示还没有找到合适的块
    struct direct_erase *erase_node;    //직접 삭제할 수 있는 블록 번호를 기록하는 데 사용되며, 새로운 ppn을 얻을 때 invalid_page_num==64가 발생할 때마다 GC 작업 중 직접 삭제를 위해 이 포인터에 추가됩니다. //用来记录可以直接删除的块号,在获取新的ppn时，每当出现invalid_page_num==64时，将其添加到这个指针上，供GC操作时直接删除
    struct blk_info *blk_head;
};


struct blk_info{
    unsigned int erase_count;          //블록의 삭제 횟수, 이 항목은 램에 기록되어 GC에 사용됩니다.
    unsigned int free_page_num;        //위와 동일하게 블록의 여유 페이지 수를 기록합니다.
    unsigned int invalid_page_num;     //위와 동일하게 블록의 유효하지 않은 페이지 수를 기록합니다.
    int last_write_page;               //마지막 쓰기 작업에 의해 수행된 페이지 수를 기록합니다. -1은 블록에 기록된 페이지가 없음을 의미합니다.
    struct page_info *page_head;       //각 하위 페이지의 상태 기록
};


struct page_info{                      //lpn은 물리적 페이지에 저장된 논리적 페이지를 기록하는데 논리적 페이지가 유효하면 valid_state는 0보다 크고 free_state는 0보다 크다.
    int valid_state;                   //indicate the page is valid or invalid
    int free_state;                    //each bit indicates the subpage is free or occupted. 1 indicates that the bit is free and 0 indicates that the bit is used
    unsigned int lpn;                 
    unsigned int written_count;        //페이지가 작성된 횟수를 기록
};


struct dram_info{
    unsigned int dram_capacity;     
    int64_t current_time;

    struct dram_parameter *dram_paramters;      
    struct map_info *map;
    struct buffer_info *buffer; 

};


/*********************************************************************************************
 *Buffer에 다시 쓰여진 페이지의 관리 방법: buffer_info에 큐를 유지: write. 대기열에는 머리와 꼬리가 있습니다.
 *각 버퍼 관리에서 요청이 히트하면 그룹을 LRU의 선두로 이동함과 동시에 그룹에 기존 그룹이 있는지 확인해야 합니다.
 *다시 쓰여진 lsn이 있는 경우 그룹을 동시에 쓰기 대기열의 끝으로 이동해야 합니다. 이 대기열이 증가하고 감소하는 방식
 *다음: 다시 쓰여진 lsn을 삭제하여 새로운 쓰기 요청을 위한 공간을 만들어야 할 때 쓰여진 큐 헤드에서 삭제할 수 있는 lsn을 찾으십시오.
 * 새로운 write-back lsn이 추가될 필요가 있을 때, write back될 수 있는 페이지를 찾고, 이 그룹을 write_insert 포인터가 가리키는 쓰여진 큐에 추가합니다.
 *노드 전. 버퍼의 LRU 큐에서 가장 오래된 기록된 페이지를 가리키는 또 다른 포인터를 유지해야 합니다.
 *이 포인터에서 이전 그룹으로 돌아가서 다시 쓰기만 하면 됩니다.
 **********************************************************************************************/
typedef struct buffer_group{
    TREE_NODE node;                     //트리 노드의 구조는 사용자 정의 구조의 맨 위에 있어야 하므로 주의하십시오!
    struct buffer_group *LRU_link_next;	// next node in LRU list
    struct buffer_group *LRU_link_pre;	// previous node in LRU list

    unsigned int group;                 //the first data logic sector number of a group stored in buffer 
    unsigned int stored;                //indicate the sector is stored in buffer or not. 1 indicates the sector is stored and 0 indicate the sector isn't stored.EX.  00110011 indicates the first, second, fifth, sixth sector is stored in buffer.
    unsigned int dirty_clean;           //it is flag of the data has been modified, one bit indicates one subpage. EX. 0001 indicates the first subpage is dirty
    int flag;			                //indicates if this node is the last 20% of the LRU list	
}buf_node;


struct dram_parameter{
    float active_current;
    float sleep_current;
    float voltage;
    int clock_time;
};


struct map_info{
    struct entry *map_entry;            //이 항목은 매핑 테이블 구조에 대한 포인터입니다. each entry indicate a mapping information
    struct buffer_info *attach_info;	// info about attach map
};


struct controller_info{
    unsigned int frequency;             //컨트롤러의 작동 주파수를 나타냅니다.
    int64_t clock_time;                 //한 클럭 주기의 시간을 나타냅니다.
    float power;                        //컨트롤러의 단위 시간당 에너지 소비량을 나타냅니다.
};


struct request{
    int64_t time;                      //요청이 도착한 시간의 단위는 us입니다. 이것은 일반적인 습관과 다릅니다. 일반적으로 단위는 ms입니다. 여기에서 단위 변환 과정이 필요합니다.
    unsigned int lsn;                  //요청 시작 주소, 논리 주소
    unsigned int size;                 //요청의 크기, 즉 섹터 수
    unsigned int operation;            //요청 유형, 읽기용 1, 쓰기용 0

    unsigned int* need_distr_flag;
    unsigned int complete_lsn_count;   //record the count of lsn served by buffer

    int distri_flag;		           // indicate whether this request has been distributed already

    int64_t begin_time;
    int64_t response_time;
    double energy_consumption;         //요청의 에너지 소비를 uJ 단위로 기록합니다.

    struct sub_request *subs;          //이 요청에 속한 모든 하위 요청에 대한 링크
    struct request *next_node;         //다음 요청 구조를 가리킴
};


struct sub_request{
    unsigned int lpn;                  //하위 요청의 논리적 페이지 번호를 나타냅니다.
    unsigned int ppn;                  //이 하위 요청에 해당 물리적 ​​하위 페이지를 할당합니다. multi_chip_page_mapping에서 psn의 값은 서브페이지 요청이 발생할 때 알 수 있고, 다른 경우에는 page_map_read, page_map_write 등과 같은 최하위 FTL 함수에 의해 psn의 값이 생성된다. 
    unsigned int operation;            //하위 요청의 종류를 나타내며 1을 읽고 0을 쓰는 것 외에 지우기, 2 plane 등의 작업도 수행합니다.
    int size;

    unsigned int current_state;        //하위 요청의 상태를 나타냅니다. 매크로 정의 하위 요청을 참조하십시오.
    int64_t current_time;
    unsigned int next_state;
    int64_t next_state_predict_time;
    unsigned int state;              //상태의 최상위 비트를 사용하여 하위 요청이 일대다 매핑 관계 중 하나인지 여부를 나타내며, 그렇다면 버퍼로 읽어야 합니다. 1은 일대다를 의미하고 0은 버퍼에 쓰지 않음을 의미합니다.
    //읽기 요청에는 이 멤버가 필요하지 않으며 lsn+size는 페이지의 상태를 구별할 수 있습니다. 그러나 이 멤버는 쓰기 요청에 필요하며 대부분의 쓰기 하위 요청은 버퍼 write-back 작업에서 발생하며 하위 페이지의 불연속성과 같은 상황이 발생할 수 있으므로 이 멤버를 별도로 유지 관리해야 합니다.

    int64_t begin_time;               //하위 요청 시작 시간
    int64_t complete_time;            //하위 요청의 처리 시간, 즉 데이터를 실제로 쓰거나 읽는 시간을 기록합니다.

    struct local *location;           //정적 할당 및 혼합 할당 방법에서 알려진 lpn은 lpn이 할당되어야 하는 채널, 칩, 다이 및 평면을 알고 있으며 이 구조는 계산된 주소를 저장하는 데 사용됩니다.
    struct sub_request *next_subs;    //동일한 요청에 속하는 하위 요청을 가리킵니다.
    struct sub_request *next_node;    //동일한 채널의 다음 하위 요청 구조를 가리킵니다.
    struct sub_request *update;       //쓰기 작업에 업데이트 작업이 있기 때문에 동적 할당 모드에서는 copyback 작업을 사용할 수 없고 쓰기 작업을 수행하기 전에 원본 페이지를 읽어야 하므로 업데이트로 인한 읽기 작업은 이 포인터에 매달렸습니다.
};


/***********************************************************************
 * 이벤트 노드는 시간의 증가를 제어하며, 각 시간 증가는 시간의 최신 이벤트에 따라 결정됩니다.
 ************************************************************************/
struct event_node{
    int type;                        //이벤트 유형을 기록. 1은 명령 유형을 나타내고, 2는 데이터 전송 유형을 나타냅니다.
    int64_t predict_time;            //이 시간의 실행을 미리 방지하기 위해 이 시간의 시작 예상 시간을 기록합니다.
    struct event_node *next_node;
    struct event_node *pre_node;
};

struct parameter_value{
    unsigned int chip_num;          //SSD에 몇 개의 파티클이 있는지 기록
    unsigned int dram_capacity;     //SSD의 DRAM 용량 기록
    unsigned int cpu_sdram;         //记录片内有多少 //얼마나 많은 기록에

    unsigned int channel_number;    //SSD에 몇 개의 채널이 있는지 기록, 각 채널은 별도의 버스입니다.
    unsigned int chip_channel[100]; //SSD의 채널 수와 채널당 파티클 수 설정

    unsigned int die_chip;    
    unsigned int plane_die;
    unsigned int block_plane;
    unsigned int page_block;
    unsigned int subpage_page;

    unsigned int page_capacity;
    unsigned int subpage_capacity;


    unsigned int ers_limit;         //각 블록을 지울 수 있는 횟수 기록
    int address_mapping;            //매핑 유형 기록, 1: page, 2: block, 3: fast
    int wear_leveling;              // WL算法
    int gc;                         //기록 GC 전략
    int clean_in_background;        //삭제 작업이 전경에서 수행되는지 여부 //清除操作是否在前台完成
    int alloc_pool;                 //allocation pool 大小(plane，die，chip，channel), 즉, active_block이 있는 유닛
    float overprovide;
    float gc_threshold;             //이 임계값에 도달하면 GC 작업이 시작됩니다. 활성 쓰기 전략에서는 GC 작업이 시작된 후 새 요청을 처리하기 위해 GC 작업을 일시적으로 중단할 수 있습니다. 일반 전략에서는 GC를 중단할 수 없습니다. //当达到这个阈值时，开始GC操作，在主动写策略中，开始GC操作后可以临时中断GC操作，服务新到的请求；在普通策略中，GC不可中断

    double operating_current;       //NAND FLASH의 작동 전류 단위는 uA입니다. //NAND FLASH的工作电流单位是uA
    double supply_voltage;	
    double dram_active_current;     //cpu sdram work current   uA
    double dram_standby_current;    //cpu sdram work current   uA
    double dram_refresh_current;    //cpu sdram work current   uA
    double dram_voltage;            //cpu sdram work voltage  V

    int buffer_management;          //indicates that there are buffer management or not
    int scheduling_algorithm;       //记录使用哪种调度算法，1:FCFS
    float quick_radio;
    int related_mapping;

    unsigned int time_step;
    unsigned int small_large_write; //the threshould of large write, large write do not occupt buffer, which is written back to flash directly

    int striping;                   //스트라이핑 사용 여부를 나타냅니다. 0은 아니오, 1은 예를 의미합니다.
    int interleaving;
    int pipelining;
    int threshold_fixed_adjust;
    int threshold_value;
    int active_write;               //활성 쓰기 작업을 수행할지 여부를 나타냅니다. 1, yes, 0, no
    float gc_hard_threshold;        //이 매개변수는 일반 전략에서는 사용되지 않으며 활성 쓰기 전략에서만 이 임계값을 충족하면 GC 작업을 중단할 수 없습니다.
    int allocation_scheme;          //할당 방법 선택 기록, 0은 동적 할당, 1은 정적 할당
    int static_allocation;          //기록은 ICS09 문서에 설명된 모든 정적 할당과 같은 종류의 정적 할당입니다.
    int dynamic_allocation;         //동적 할당을 기록하는 방법
    int advanced_commands;  
    int ad_priority;                //record the priority between two plane operation and interleave operation
    int ad_priority2;               //record the priority of channel-level, 0 indicates that the priority order of channel-level is highest; 1 indicates the contrary
    int greed_CB_ad;                //0 don't use copyback advanced commands greedily; 1 use copyback advanced commands greedily
    int greed_MPW_ad;               //0 don't use multi-plane write advanced commands greedily; 1 use multi-plane write advanced commands greedily
    int aged;                       //1은 이 SSD를 에이징 상태로 전환해야 함을 의미하고 0은 이 SSD를 에이징되지 않은 상태로 유지해야 함을 의미합니다.
    float aged_ratio; 
    int queue_length;               //요청 큐의 길이 제한

    struct ac_time_characteristics time_characteristics;
};

/********************************************************
 * 매핑 정보, 상태의 최상위 비트는 추가 매핑 관계가 있는지 여부를 나타냅니다.
 *********************************************************/
struct entry{                       
    unsigned int pn;                //물리적 페이지 번호, 물리적 하위 페이지 번호 또는 물리적 블록 번호를 나타낼 수 있는 물리적 번호
    int state;                      //16진수 표현은 0000-FFFF이며 각 비트는 해당 서브페이지가 유효한지(페이지 매핑) 여부를 나타냅니다. 예를 들어 이 페이지에서 하위 페이지 0과 1은 유효하고 하위 페이지 2와 3은 유효하지 않습니다. 이것은 0x0003이어야 합니다.//十六进制表示的话是0000-FFFF，每位表示相应的子页是否有效（页映射）。比如在这个页中，0，1号子页有效，2，3无效，这个应该是0x0003.
};


struct local{          
    unsigned int channel;
    unsigned int chip;
    unsigned int die;
    unsigned int plane;
    unsigned int block;
    unsigned int page;
    unsigned int sub_page;
};


struct gc_info{
    int64_t begin_time;            //plane에서 gc 작업을 시작할 때 기록
    int copy_back_count;    
    int erase_count;
    int64_t process_time;          //plane에서 gc 작업에 소요되는 시간
    double energy_consumption;     //plane에서 gc 작업에 소비하는 에너지
};


struct direct_erase{
    unsigned int block;
    struct direct_erase *next_node;
};


/**************************************************************************************
 *GC 동작이 발생하면 해당 채널에 이 구조를 hang 시키고, 채널이 유휴 상태일 때 GC 동작 명령을 내린다.
 ***************************************************************************************/
struct gc_operation{          
    unsigned int chip;
    unsigned int die;
    unsigned int plane;
    unsigned int block;           //이 매개변수는 최근에 발견된 대상 블록 번호를 기록하기 위해 인터럽트 가능한 gc 함수(gc_interrupt)에서만 사용됩니다.
    unsigned int page;            //이 매개변수는 완료된 데이터 마이그레이션의 페이지 번호를 기록하기 위해 인터럽트 가능한 gc 함수(gc_interrupt)에서만 사용됩니다.
    unsigned int state;           //현재 gc 요청의 상태를 기록합니다.
    unsigned int priority;        //gc 작업의 우선 순위를 기록합니다. 1은 중단할 수 없음을 의미하고 0은 중단할 수 있음을 의미합니다(소프트 임계값에 의해 생성된 gc 요청)
    struct gc_operation *next_node;
};

/*
 *add by ninja
 *used for map_pre function
 */
typedef struct Dram_write_map
{
    unsigned int state; 
}Dram_write_map;


struct ssd_info *initiation(struct ssd_info *);
struct parameter_value *load_parameters(char parameter_file[30]);
struct page_info * initialize_page(struct page_info * p_page);
struct blk_info * initialize_block(struct blk_info * p_block,struct parameter_value *parameter);
struct plane_info * initialize_plane(struct plane_info * p_plane,struct parameter_value *parameter );
struct die_info * initialize_die(struct die_info * p_die,struct parameter_value *parameter,long long current_time );
struct chip_info * initialize_chip(struct chip_info * p_chip,struct parameter_value *parameter,long long current_time );
struct ssd_info * initialize_channels(struct ssd_info * ssd );
struct dram_info * initialize_dram(struct ssd_info * ssd);

#endif

