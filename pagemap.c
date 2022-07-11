/*****************************************************************************************************************************
  This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
  Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

  FileName： pagemap.h
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

#define _CRTDBG_MAP_ALLOC

#include "pagemap.h"


/************************************************
 * Assert, 파일 열기 실패 시 "파일 이름 열기 오류" 출력
 *************************************************/
void file_assert(int error,char *s)
{
    if(error == 0) return;
    printf("open %s error\n",s);
    getchar();
    exit(-1);
}

/*****************************************************
 *Assert, 메모리 공간에 대한 응용 프로그램이 실패하면 "malloc 변수 이름 오류" 출력
 ******************************************************/
void alloc_assert(void *p,char *s)//断言
{
    if(p!=NULL) return;
    printf("malloc %s error\n",s);
    getchar();
    exit(-1);
}

/*********************************************************************************
 *Assert,
 *A, read time_t, device, lsn, size, op가 모두 <0인 경우 "trace error:....." 출력
 *B, read time_t, device, lsn, size, op가 모두 0일 때 "probable read a blank line" 출력
 *ope: 1 for read, 0 for write
 **********************************************************************************/
void trace_assert(int64_t time_t,int device,unsigned int lsn,int size,int ope)//断言
{
    if(time_t <0 || device < 0 || lsn < 0 || size < 0 || ope < 0)
    {
        printf("trace error:%lld %d %d %d %d\n",time_t,device,lsn,size,ope);
        getchar();
        exit(-1);
    }
    if(time_t == 0 && device == 0 && lsn == 0 && size == 0 && ope == 0)
    {
        printf("probable read a blank line\n");
        getchar();
    }
}


/************************************************************************************
 *함수의 기능은 물리적 페이지 번호 ppn에 따라 물리적 페이지가 위치한 채널, 칩, 다이, 플레인, 블록, 페이지를 찾는 것입니다.
 *획득한 채널, 칩, 다이, 플레인, 블록, 페이지를 구조체 위치에 배치하고 반환값으로 사용
 *************************************************************************************/
struct local *find_location(struct ssd_info *ssd,unsigned int ppn)
{
    struct local *location=NULL;
    unsigned int i=0;
    int pn,ppn_value=ppn;
    int page_plane=0,page_die=0,page_chip=0,page_channel=0;

    pn = ppn;

#ifdef DEBUG
    printf("enter find_location\n");
#endif

    location=(struct local *)malloc(sizeof(struct local));
    alloc_assert(location,"location");
    memset(location,0, sizeof(struct local));

    page_plane=ssd->parameter->page_block*ssd->parameter->block_plane; //block당 page수 * plane당 block수 = plane당 총 page수
    page_die=page_plane*ssd->parameter->plane_die; //plane당 총 page수 * die당 plane수 = die당 총 page수
    page_chip=page_die*ssd->parameter->die_chip;
    page_channel=page_chip*ssd->parameter->chip_channel[0]; //칩당 페이지 수 * 채널당 칩 수 = 채널당 페이지 수

    /*******************************************************************************
     *page_channel은 채널의 페이지 수, ppn/page_channel은 채널이 속한 채널을 가져옵니다.
     *같은 방법으로 칩, 다이, 플레인, 블록, 페이지를 얻을 수 있습니다.
     ********************************************************************************/
    location->channel = ppn/page_channel;
    location->chip = (ppn%page_channel)/page_chip;
    location->die = ((ppn%page_channel)%page_chip)/page_die;
    location->plane = (((ppn%page_channel)%page_chip)%page_die)/page_plane;
    location->block = ((((ppn%page_channel)%page_chip)%page_die)%page_plane)/ssd->parameter->page_block;
    location->page = (((((ppn%page_channel)%page_chip)%page_die)%page_plane)%ssd->parameter->page_block)%ssd->parameter->page_block;

    return location;
}


/*****************************************************************************
 *이 기능의 기능은 채널, 칩, 다이, plane, 블록, 페이지 매개변수에 따라 물리적 페이지 번호를 찾는 것입니다.
 *함수의 반환 값은 이 물리적 페이지 번호입니다.
 ******************************************************************************/
unsigned int find_ppn(struct ssd_info * ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int block,unsigned int page)
{
    unsigned int ppn=0;
    unsigned int i=0;
    int page_plane=0,page_die=0,page_chip=0;
    int page_channel[100];                  /*이 배열은 각 채널의 페이지 수를 저장합니다.*/

#ifdef DEBUG
    printf("enter find_psn,channel:%d, chip:%d, die:%d, plane:%d, block:%d, page:%d\n",channel,chip,die,plane,block,page);
#endif

    /*********************************************
     * 평면, 다이, 칩, 채널의 페이지 수 계산
     **********************************************/
    page_plane=ssd->parameter->page_block*ssd->parameter->block_plane;
    page_die=page_plane*ssd->parameter->plane_die;
    page_chip=page_die*ssd->parameter->die_chip;
    while(i<ssd->parameter->channel_number)
    {
        page_channel[i]=ssd->parameter->chip_channel[i]*page_chip;
        i++;
    }

    /****************************************************************************
     * 물리적 페이지 수 ppn을 계산합니다. ppn은 채널, 칩, 다이, 플레인, 블록, 페이지의 페이지 수의 합입니다.
     *****************************************************************************/
    i=0;
    while(i<channel)
    {
        ppn=ppn+page_channel[i];
        i++;
    }
    ppn=ppn+page_chip*chip+page_die*die+page_plane*plane+block*ssd->parameter->page_block+page;

    return ppn;
}

/********************************
 *이 함수는 읽기 하위 요청의 상태를 가져오는 것입니다.
 *********************************/
int set_entry_state(struct ssd_info *ssd,unsigned int lsn,unsigned int size)
{
    int temp,state,move;

    temp=~(0xffffffff<<size);
    move=lsn%ssd->parameter->subpage_page;
    state=temp<<move;

    return state;
}

/**************************************************
 *읽기 요청 전처리 기능, 읽기 요청으로 읽은 페이지에 데이터가 없을 때,
 *데이터를 읽을 수 있도록 이 페이지에 기록된 데이터를 전처리해야 합니다.
 ***************************************************/
struct ssd_info *pre_process_page(struct ssd_info *ssd)
{
    int fl=0;
    unsigned int device,lsn,size,ope,lpn,full_page;
    unsigned int largest_lsn,sub_size,ppn,add_size=0;
    unsigned int i=0,j,k;
    int map_entry_new,map_entry_old,modify;
    int flag=0;
    char buffer_request[200];
    struct local *location;
    int64_t time;

    printf("\n");
    printf("begin pre_process_page.................\n");

    ssd->tracefile=fopen(ssd->tracefilename,"r");
    if(ssd->tracefile == NULL )      /*요청을 읽을 추적 파일 열기*/
    {
        printf("the trace file can't open\n");
        return NULL;
    }

    full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
    /*ssd의 최대 논리 섹터 수를 계산*/
    largest_lsn=(unsigned int )((ssd->parameter->chip_num*ssd->parameter->die_chip*ssd->parameter->plane_die*ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->subpage_page)*(1-ssd->parameter->overprovide));

    while(fgets(buffer_request,200,ssd->tracefile))
    {
        sscanf(buffer_request,"%lld %d %d %d %d",&time,&device,&lsn,&size,&ope);
        fl++;
        trace_assert(time,device,lsn,size,ope);                         /*Assert, read time, device, lsn, size, op가 유효하지 않으면 처리됩니다.*/

        add_size=0;                                                     /*add_size는 이 요청의 이미 사전 처리된 크기입니다.*/
        //ope 1 for read operation
        if(ope==1)                                                      /*이것은 읽기 요청의 전처리일 뿐이며 해당 위치의 정보는 그에 따라 미리 수정되어야 합니다.*/
        {
            while(add_size<size)
            {				
                lsn=lsn%largest_lsn;                                    /*얻은 lsn이 가장 큰 lsn보다 커지지 않도록 방지*/		
                sub_size=ssd->parameter->subpage_page-(lsn%ssd->parameter->subpage_page);		
                if(add_size+sub_size>=size)                             /*요청의 크기가 페이지의 크기보다 작거나 요청의 마지막 페이지를 처리할 때만 발생합니다.*/
                {		
                    sub_size=size-add_size;		
                    add_size+=sub_size;		
                }

                if((sub_size>ssd->parameter->subpage_page)||(add_size>size))/*하위 크기를 전처리할 때 크기가 페이지보다 크거나 이미 처리된 크기가 크기보다 크면 오류가 보고됩니다.*/		
                {		
                    printf("pre_process sub_size:%d\n",sub_size);		
                }

                /*******************************************************************************************************
                 *利用逻辑扇区号lsn计算出逻辑页号lpn
                 *判断这个dram中映射表map中在lpn位置的状态
                 *A，这个状态==0，表示以前没有写过，现在需要直接将ub_size大小的子页写进去写进去
                 *B，这个状态>0，表示，以前有写过，这需要进一步比较状态，因为新写的状态可以与以前的状态有重叠的扇区的地方

                 *논리 섹터 번호 lsn을 사용하여 논리 페이지 번호 lpn을 계산합니다.
                 * 이 dram의 매핑 테이블 맵에서 LPN 위치의 상태를 확인합니다.
                 *A, 이 상태 == 0, 이는 이전에 작성된 적이 없음을 의미하며 이제 ub_size 크기의 서브페이지를 여기에 직접 작성해야 합니다.
                 *B, 이 상태 > 0은 이전에 기록되었음을 의미하며 새로 기록된 상태가 이전 상태와 섹터가 겹칠 수 있기 때문에 상태에 대한 추가 비교가 필요합니다.
                 ********************************************************************************************************/
                lpn=lsn/ssd->parameter->subpage_page;
                if(ssd->dram->map->map_entry[lpn].state==0)                 /*상태는 0입니다. 즉, 한번도 사용되지 않은 상태*/
                {
                    /**************************************************************
                     *get_ppn_for_pre_process 함수를 사용하여 ppn을 가져온 다음 위치를 가져옵니다.
                     *ssd의 관련 매개변수, dram의 매핑 테이블 맵 및 위치 아래 페이지의 상태를 수정합니다.
                     ***************************************************************/
                    ppn=get_ppn_for_pre_process(ssd,lsn);                  
                    location=find_location(ssd,ppn);
                    ssd->program_count++;	
                    ssd->channel_head[location->channel].program_count++;
                    ssd->channel_head[location->channel].chip_head[location->chip].program_count++;		
                    ssd->dram->map->map_entry[lpn].pn=ppn;	
                    ssd->dram->map->map_entry[lpn].state=set_entry_state(ssd,lsn,sub_size);   //0001
                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=lpn;
                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=ssd->dram->map->map_entry[lpn].state;
                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~ssd->dram->map->map_entry[lpn].state)&full_page);

                    free(location);
                    location=NULL;
                }//if(ssd->dram->map->map_entry[lpn].state==0)
                else if(ssd->dram->map->map_entry[lpn].state>0)           /*상태가 0이 아닙니다. 즉, 한번 이상 사용된 상태*/
                {
                    map_entry_new=set_entry_state(ssd,lsn,sub_size);      /*새 상태를 가져오고 OR 원래 상태를 상태로 가져옵니다.*/
                    map_entry_old=ssd->dram->map->map_entry[lpn].state;
                    modify=map_entry_new|map_entry_old;
                    ppn=ssd->dram->map->map_entry[lpn].pn;
                    location=find_location(ssd,ppn);

                    ssd->program_count++;	
                    ssd->channel_head[location->channel].program_count++;
                    ssd->channel_head[location->channel].chip_head[location->chip].program_count++;		
                    ssd->dram->map->map_entry[lsn/ssd->parameter->subpage_page].state=modify; 
                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=modify;
                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=((~modify)&full_page);

                    free(location);
                    location=NULL;
                }//else if(ssd->dram->map->map_entry[lpn].state>0)
                lsn=lsn+sub_size;                                         /*다음 하위 요청의 시작 위치*/
                add_size+=sub_size;                                       /*처리된 add_size의 크기 변경*/
            }//while(add_size<size)
        }//if(ope==1) 
    }	

    printf("\n");
    printf("pre_process is complete!\n");

    fclose(ssd->tracefile);

    for(i=0;i<ssd->parameter->channel_number;i++)
        for(j=0;j<ssd->parameter->die_chip;j++)
            for(k=0;k<ssd->parameter->plane_die;k++)
            {
                fprintf(ssd->outputfile,"chip:%d,die:%d,plane:%d have free page: %d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);				
                fflush(ssd->outputfile);
            }

    return ssd;
}

/**************************************
 *함수 함수는 전처리 함수에 대한 물리적 페이지 번호 ppn을 얻는 것입니다.
 *Get 페이지 번호는 동적 획득과 정적 획득으로 구분됩니다.
 **************************************/
unsigned int get_ppn_for_pre_process(struct ssd_info *ssd,unsigned int lsn)     
{
    unsigned int channel=0,chip=0,die=0,plane=0; 
    unsigned int ppn,lpn;
    unsigned int active_block;
    unsigned int channel_num=0,chip_num=0,die_num=0,plane_num=0;

#ifdef DEBUG
    printf("enter get_psn_for_pre_process\n");
#endif

    channel_num=ssd->parameter->channel_number;
    chip_num=ssd->parameter->chip_channel[0];
    die_num=ssd->parameter->die_chip;
    plane_num=ssd->parameter->plane_die;
    lpn=lsn/ssd->parameter->subpage_page;

    if (ssd->parameter->allocation_scheme==0)                           /*동적 모드에서 ppn 가져오기*/
    {
        if (ssd->parameter->dynamic_allocation==0)                      /*전체 동적 모드에서 즉, 채널, 칩, 다이, 평면, 블록 등이 모두 동적으로 할당됨을 나타냅니다.  //表示全动态方式下，也就是channel，chip，die，plane，block等都是动态分配*/
        {
            channel=ssd->token;
            ssd->token=(ssd->token+1)%ssd->parameter->channel_number;
            chip=ssd->channel_head[channel].token;
            ssd->channel_head[channel].token=(chip+1)%ssd->parameter->chip_channel[0];
            die=ssd->channel_head[channel].chip_head[chip].token;
            ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
            plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
        } 
        else if (ssd->parameter->dynamic_allocation==1)                 /*반(半)동적 모드를 나타내며 채널은 정적으로 지정되며 패키지, 다이, 평면은 동적으로 할당됩니다. //表示半动态方式，channel静态给出，package，die，plane动态分配*/                 
        {
            channel=lpn%ssd->parameter->channel_number;
            chip=ssd->channel_head[channel].token;
            ssd->channel_head[channel].token=(chip+1)%ssd->parameter->chip_channel[0];
            die=ssd->channel_head[channel].chip_head[chip].token;
            ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
            plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
        }
    } 
    else if (ssd->parameter->allocation_scheme==1)                       /*정적 할당을 나타내며 0, 1, 2, 3, 4, 5의 6가지 정적 할당 방법이 있습니다.*/
    {
        switch (ssd->parameter->static_allocation)
        {

            case 0:         
                {
                    channel=(lpn/(plane_num*die_num*chip_num))%channel_num;
                    chip=lpn%chip_num;
                    die=(lpn/chip_num)%die_num;
                    plane=(lpn/(die_num*chip_num))%plane_num;
                    break;
                }
            case 1:
                {
                    channel=lpn%channel_num;
                    chip=(lpn/channel_num)%chip_num;
                    die=(lpn/(chip_num*channel_num))%die_num;
                    plane=(lpn/(die_num*chip_num*channel_num))%plane_num;

                    break;
                }
            case 2:
                {
                    channel=lpn%channel_num;
                    chip=(lpn/(plane_num*channel_num))%chip_num;
                    die=(lpn/(plane_num*chip_num*channel_num))%die_num;
                    plane=(lpn/channel_num)%plane_num;
                    break;
                }
            case 3:
                {
                    channel=lpn%channel_num;
                    chip=(lpn/(die_num*channel_num))%chip_num;
                    die=(lpn/channel_num)%die_num;
                    plane=(lpn/(die_num*chip_num*channel_num))%plane_num;
                    break;
                }
            case 4:  
                {
                    channel=lpn%channel_num;
                    chip=(lpn/(plane_num*die_num*channel_num))%chip_num;
                    die=(lpn/(plane_num*channel_num))%die_num;
                    plane=(lpn/channel_num)%plane_num;

                    break;
                }
            case 5:   
                {
                    channel=lpn%channel_num;
                    chip=(lpn/(plane_num*die_num*channel_num))%chip_num;
                    die=(lpn/channel_num)%die_num;
                    plane=(lpn/(die_num*channel_num))%plane_num;

                    break;
                }
            default : return 0;
        }
    }

    /******************************************************************************
     *위의 할당 방법에 따라 채널, 칩, 다이, 플레인을 찾은 후, 여기에서 active_block을 찾습니다.
     *그런 다음 ppn을 얻습니다.
     ******************************************************************************/
    if(find_active_block(ssd,channel,chip,die,plane)==FAILURE)
    {
        printf("the read operation is expand the capacity of SSD\n");	
        return 0;
    }
    active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
    if(write_page(ssd,channel,chip,die,plane,active_block,&ppn)==ERROR)
    {
        return 0;
    }

    return ppn;
}


/***************************************************************************************************
 *함수는 주어진 채널, 칩, 다이, 평면에서 active_block을 찾은 다음 이 블록에서 페이지를 찾는 것입니다.
 * ppn을 다시 찾으려면 find_ppn을 사용하십시오.
 ****************************************************************************************************/
struct ssd_info *get_ppn(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,struct sub_request *sub)
{
    int old_ppn=-1;
    unsigned int ppn,lpn,full_page;
    unsigned int active_block;
    unsigned int block;
    unsigned int page,flag=0,flag1=0;
    unsigned int old_state=0,state=0,copy_subpage=0;
    struct local *location;
    struct direct_erase *direct_erase_node,*new_direct_erase;
    struct gc_operation *gc_node;

    unsigned int i=0,j=0,k=0,l=0,m=0,n=0;

#ifdef DEBUG
    printf("enter get_ppn,channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
#endif

    full_page=~(0xffffffff<<(ssd->parameter->subpage_page));
    lpn=sub->lpn;

    /*************************************************************************************
     * find_active_block 함수를 사용하여 채널, 칩, 다이, 평면에서 활성 블록을 찾습니다.
     *그리고 이 채널, chip, die, plane, active_block 아래에서 last_write_page 및 free_page_num을 수정하십시오.
     **************************************************************************************/
    if(find_active_block(ssd,channel,chip,die,plane)==FAILURE) //find_active_block으로 ssd->plane의 멤버변수 active_block에 free page가 있는 블록을 구해서 설정               
    {
        printf("ERROR :there is no free page in channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);	
        return ssd;
    }

    active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++; //active block의 last_write_page를 증가
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--; //active block의 free page 수를 감소

    if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>63)
    {
        printf("error! the last write page larger than 64!!\n");
        while(1){}
    }

    block=active_block;	
    page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;	

    if(ssd->dram->map->map_entry[lpn].state==0)  /*this is the first logical page*/
    {
        if(ssd->dram->map->map_entry[lpn].pn!=0) //state가 0이면 아직 사용되지 않은 경우인데 pn이 0이 아닌 다른 수라면 error
        {
            printf("Error in get_ppn()\n");
        }
        ssd->dram->map->map_entry[lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
        ssd->dram->map->map_entry[lpn].state=sub->state;
    }
    else  /*이 논리적 페이지가 업데이트되었으며 원본 페이지를 무효화해야 합니다.*/
    {
        ppn=ssd->dram->map->map_entry[lpn].pn;
        location=find_location(ssd,ppn); //업데이트 이전 블록 위치
        if(	ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn!=lpn)
        {
            printf("\nError in get_ppn()\n");
        }

        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;/*페이지가 유효하지 않으며 유효 및 자유 상태가 모두 0으로 표시됨을 나타냅니다.*/
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;/*페이지가 유효하지 않으며 유효 및 자유 상태가 모두 0으로 표시됨을 나타냅니다.*/
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

        /*******************************************************************************************
         *이 블록은 유효하지 않은 페이지로 가득 차 있습니다. 직접 삭제할 수 있으며, 지울 수 있는 노드를 생성하고 location 아래 plane 아래에 매달려 있습니다.
         ********************************************************************************************/
        if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num==ssd->parameter->page_block)    
        {
            new_direct_erase=(struct direct_erase *)malloc(sizeof(struct direct_erase));
            alloc_assert(new_direct_erase,"new_direct_erase");
            memset(new_direct_erase,0, sizeof(struct direct_erase));

            new_direct_erase->block=location->block;
            new_direct_erase->next_node=NULL;
            //erase_node는 plane에서 direct erase가 가능한 블록을 관리하는 연결리스트임
            direct_erase_node=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
            if (direct_erase_node==NULL) //direct_erase_node가 null => 현재 direct erase 연결리스트에 노드가 없으므로 new_direct_erase를 첫 노드로 추가
            {
                ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
            } 
            else //연결리스트에 노드가 있으면 next_node 포인터로 연결해주고 new direct erase를 head로 설정
            {
                new_direct_erase->next_node=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
                ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
            }
            //결국 이 segment에서는 direct erase할 블록을 추가하는 역할을 수행
        }

        free(location);
        location=NULL;
        ssd->dram->map->map_entry[lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
        ssd->dram->map->map_entry[lpn].state=(ssd->dram->map->map_entry[lpn].state|sub->state);
    }


    sub->ppn=ssd->dram->map->map_entry[lpn].pn;                                      /*하위 요청의 ppn, 위치 및 기타 변수 수정*/
    sub->location->channel=channel;
    sub->location->chip=chip;
    sub->location->die=die;
    sub->location->plane=plane;
    sub->location->block=active_block;
    sub->location->page=page;

    ssd->program_count++;                                                           /*ssd의 program_count, free_page 등의 변수 수정*/
    ssd->channel_head[channel].program_count++;
    ssd->channel_head[channel].chip_head[chip].program_count++;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].lpn=lpn;	
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].valid_state=sub->state;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].free_state=((~(sub->state))&full_page);
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
    ssd->write_flash_count++;

    //gc 대상 블록을 관리하는 연결리스트에 추가
    if (ssd->parameter->active_write==0)                                            /*활성 전략이 없으면 gc_hard_threshold만 사용되며 GC 프로세스를 중단할 수 없습니다.*/
    {                                                                               /*plane의 free_pages 수가 gc_hard_threshold에 의해 설정된 임계값보다 작으면 gc 작업이 생성됩니다.*/
        if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
        {
            gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
            alloc_assert(gc_node,"gc_node");
            memset(gc_node,0, sizeof(struct gc_operation));

            gc_node->next_node=NULL;
            gc_node->chip=chip;
            gc_node->die=die;
            gc_node->plane=plane;
            gc_node->block=0xffffffff;
            gc_node->page=0;
            gc_node->state=GC_WAIT;
            gc_node->priority=GC_UNINTERRUPT;
            gc_node->next_node=ssd->channel_head[channel].gc_command; //gc_command = gc를 생성해야 하는 위치 기록
            ssd->channel_head[channel].gc_command=gc_node;
            ssd->gc_request++;
        }
    } 

    return ssd;
}
/*****************************************************************************************
 *이 함수는 gc 연산에 대한 새로운 ppn을 찾는 것입니다. 원래 물리적 블록에 데이터를 저장하기 위해 gc 연산에서 새로운 물리적 블록을 찾아야 하기 때문입니다.
 *gc 작업을 반복하지 않고 gc에서 새로운 물리적 블록을 찾는 기능
 ******************************************************************************************/
unsigned int get_ppn_for_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)     
{
    unsigned int ppn;
    unsigned int active_block,block,page;

#ifdef DEBUG
    printf("enter get_ppn_for_gc,channel:%d, chip:%d, die:%d, plane:%d\n",channel,chip,die,plane);
#endif

    if(find_active_block(ssd,channel,chip,die,plane)!=SUCCESS)
    {
        printf("\n\n Error int get_ppn_for_gc().\n");
        return 0xffffffff;
    }

    active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;

    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page++;	
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--;

    if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page>63)
    {
        printf("error! the last write page larger than 64!!\n");
        while(1){}
    }

    block=active_block;	
    page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page;	

    ppn=find_ppn(ssd,channel,chip,die,plane,block,page);

    ssd->program_count++;
    ssd->channel_head[channel].program_count++;
    ssd->channel_head[channel].chip_head[chip].program_count++;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[page].written_count++;
    ssd->write_flash_count++;

    return ppn;

}

/*********************************************************************************************************************
 *2011년 7월 28일 Zhu Zhiming에 의해 수정됨
 *함수의 기능은 채널, 칩, 다이, 평면 아래의 블록을 지우는 erase_operation 지우기 작업입니다.
 *즉, 이 블록의 관련 매개변수를 초기화하는 것입니다. 예: free_page_num=page_block, invalid_page_num=0, last_write_page=-1, erase_count++
 *이 블록 아래에 있는 각 페이지의 관련 매개변수도 수정해야 합니다.
 *********************************************************************************************************************/
//erase -> block 단위. 블록내의 모든 페이지의 변수들을 초기화하여 새 블록으로 만드는 과정
Status erase_operation(struct ssd_info * ssd,unsigned int channel ,unsigned int chip ,unsigned int die ,unsigned int plane ,unsigned int block)
{
    unsigned int i=0;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num=ssd->parameter->page_block; //free page 수를 원상복구
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num=0; //전부다 유효한 페이지
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page=-1; 
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].erase_count++;
    for (i=0;i<ssd->parameter->page_block;i++) //블록당 페이지수만큼 반복
    {
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state=PG_SUB;
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state=0;
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].lpn=-1;
    }
    ssd->erase_count++;
    ssd->channel_head[channel].erase_count++;			
    ssd->channel_head[channel].chip_head[chip].erase_count++;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page+=ssd->parameter->page_block;

    return SUCCESS;

}


/**************************************************************************************
 *이 함수의 기능은 INTERLEAVE_TWO_PLANE, INTERLEAVE, TWO_PLANE, NORMAL에서 지우기 작업을 처리하는 것입니다.
 ***************************************************************************************/
Status erase_planes(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die1, unsigned int plane1,unsigned int command)
{
    unsigned int die=0;
    unsigned int plane=0;
    unsigned int block=0;
    struct direct_erase *direct_erase_node=NULL;
    unsigned int block0=0xffffffff;
    unsigned int block1=0;

    if((ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node==NULL)||               
            ((command!=INTERLEAVE_TWO_PLANE)&&(command!=INTERLEAVE)&&(command!=TWO_PLANE)&&(command!=NORMAL)))     /*지우기 작업이 없거나 명령이 잘못된 경우 오류를 반환합니다.*/           
    {
        return ERROR;
    }

    /************************************************************************************************************
     *Eraser 동작을 처리할 때 Erase 명령이 먼저 전송되어야 하며 이것은 채널이며 칩은 명령을 전송하는 상태, 즉 CHANNEL_TRANSFER, CHIP_ERASE_BUSY
     *다음 상태는 CHANNEL_IDLE, CHIP_IDLE입니다.
     *************************************************************************************************************/
    block1=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node->block;

    ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
    ssd->channel_head[channel].current_time=ssd->current_time;										
    ssd->channel_head[channel].next_state=CHANNEL_IDLE;	

    ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;										
    ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
    ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;

    if(command==INTERLEAVE_TWO_PLANE)   /*고급 명령 INTERLEAVE_TWO_PLANE 처리*/
    {
        for(die=0;die<ssd->parameter->die_chip;die++)
        {
            block0=0xffffffff;
            if(die==die1)
            {
                block0=block1;
            }
            for (plane=0;plane<ssd->parameter->plane_die;plane++)
            {
                direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
                if(direct_erase_node!=NULL)
                {

                    block=direct_erase_node->block; 

                    if(block0==0xffffffff)
                    {
                        block0=block;
                    }
                    else
                    {
                        if(block!=block0)
                        {
                            continue;
                        }

                    }
                    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=direct_erase_node->next_node;
                    erase_operation(ssd,channel,chip,die,plane,block);     /*실제 지우기 작업 처리*/
                    free(direct_erase_node);                               
                    direct_erase_node=NULL;
                    ssd->direct_erase_count++;
                }

            }
        }

        ssd->interleave_mplane_erase_count++;                             /*인터리브 2면 소거 명령이 전송되고 처리 시간과 다음 상태까지의 시간이 계산됩니다.*/
        ssd->channel_head[channel].next_state_predict_time=ssd->current_time+18*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tWB;       
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time-9*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tBERS;

    }
    else if(command==INTERLEAVE)                                          /*고급 명령 INTERLEAVE 처리*/
    {
        for(die=0;die<ssd->parameter->die_chip;die++)
        {
            for (plane=0;plane<ssd->parameter->plane_die;plane++)
            {
                if(die==die1)
                {
                    plane=plane1;
                }
                direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
                if(direct_erase_node!=NULL)
                {
                    block=direct_erase_node->block;
                    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=direct_erase_node->next_node;
                    erase_operation(ssd,channel,chip,die,plane,block);
                    free(direct_erase_node);
                    direct_erase_node=NULL;
                    ssd->direct_erase_count++;
                    break;
                }	
            }
        }
        ssd->interleave_erase_count++;
        ssd->channel_head[channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;       
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
    }
    else if(command==TWO_PLANE)                                          /*고급 명령 TWO_PLANE 처리*/
    {

        for(plane=0;plane<ssd->parameter->plane_die;plane++)
        {
            direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane].erase_node;
            if((direct_erase_node!=NULL))
            {
                block=direct_erase_node->block;
                if(block==block1)
                {
                    ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane].erase_node=direct_erase_node->next_node;
                    erase_operation(ssd,channel,chip,die1,plane,block);
                    free(direct_erase_node);
                    direct_erase_node=NULL;
                    ssd->direct_erase_count++;
                }
            }
        }

        ssd->mplane_erase_conut++;
        ssd->channel_head[channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;      
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
    }
    else if(command==NORMAL)                                             /*일반 명령 NORMAL 처리*/
    {
        direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node;
        block=direct_erase_node->block;
        ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node=direct_erase_node->next_node;
        free(direct_erase_node);
        direct_erase_node=NULL;
        erase_operation(ssd,channel,chip,die1,plane1,block);

        ssd->direct_erase_count++;
        ssd->channel_head[channel].next_state_predict_time=ssd->current_time+5*ssd->parameter->time_characteristics.tWC;       								
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tWB+ssd->parameter->time_characteristics.tBERS;	
    }
    else
    {
        return ERROR;
    }

    direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die1].plane_head[plane1].erase_node;

    if(((direct_erase_node)!=NULL)&&(direct_erase_node->block==block1))
    {
        return FAILURE; 
    }
    else
    {
        return SUCCESS;
    }
}


/*******************************************************************************************************************
 *GC操作由某个plane的free块少于阈值进行触发，当某个plane被触发时，GC操作占据这个plane所在的die，因为die是一个独立单元。
 *对一个die的GC操作，尽量做到四个plane同时erase，利用interleave erase操作。GC操作应该做到可以随时停止（移动数据和擦除
 *时不行，但是间隙时间可以停止GC操作），以服务新到达的请求，当请求服务完后，利用请求间隙时间，继续GC操作。可以设置两个
 *GC阈值，一个软阈值，一个硬阈值。软阈值表示到达该阈值后，可以开始主动的GC操作，利用间歇时间，GC可以被新到的请求中断；
 *当到达硬阈值后，强制性执行GC操作，且此GC操作不能被中断，直到回到硬阈值以上。
 *在这个函数里面，找出这个die所有的plane中，有没有可以直接删除的block，要是有的话，利用interleave two plane命令，删除
 *这些block，否则有多少plane有这种直接删除的block就同时删除，不行的话，最差就是单独这个plane进行删除，连这也不满足的话，
 *直接跳出，到gc_parallelism函数进行进一步GC操作。该函数寻找全部为invalid的块，直接删除，找到可直接删除的返回1，没有找
 *到返回-1。

 *GC 연산은 플레인의 자유 블록이 임계값보다 작을 때 트리거되며, 플레인이 트리거되면 다이가 독립된 단위이기 때문에 플레인이 위치한 다이를 GC 연산이 차지합니다.
 *다이의 GC 연산의 경우 4개의 평면을 동시에 지우고 인터리브 지우기 연산을 사용하십시오. GC 작업은 언제든지 중지해야 합니다(데이터 이동 및 삭제
 *불가능하지만 gap time은 GC 작업을 중지할 수 있음) 새로 도착한 요청을 처리하기 위해 요청이 처리되면 요청 gap time을 사용하여 GC 작업을 계속합니다. 2개 설정 가능
 *GC 임계값, 하나의 소프트 임계값, 하나의 하드 임계값. 소프트 임계값은 임계값에 도달한 후 활성 GC 작업이 시작될 수 있고 간헐적 시간을 사용하는 새 요청에 의해 GC가 중단될 수 있음을 나타냅니다.
 *하드 임계값에 도달하면 GC 작업을 강제로 수행하고 하드 임계값으로 돌아갈 때까지 GC 작업을 중단할 수 없습니다.
 *이 함수에서 이 die의 모든 plane에서 직접 삭제할 수 있는 블록이 있는지 확인합니다.있는 경우 interleave two plane 명령을 사용하여 삭제합니다.
 *이 블록, 그렇지 않으면 직접 삭제 된 블록이 있는 평면의 수는 동시에 삭제 됩니다. 그렇지 않으면이 평면을 단독으로 삭제 하는 것이 최악의 경우 만족스럽지 않더라도,
 * 직접 뛰어 나와 추가 GC 작업을 위해 gc_parallelism 함수로 이동합니다. 이 함수는 모든 유효하지 않은 블록을 찾아 직접 삭제하고, 발견되면 1을 반환하고, 발견되지 않으면 1을 반환합니다.
 * -1을 반환합니다.
 *********************************************************************************************************************/
int gc_direct_erase(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)     
{
    unsigned int lv_die=0,lv_plane=0;                                                           /*중복된 이름을 피하기 위해 사용되는 지역 변수*/
    unsigned int interleaver_flag=FALSE,muilt_plane_flag=FALSE;
    unsigned int normal_erase_flag=TRUE;

    struct direct_erase * direct_erase_node1=NULL;
    struct direct_erase * direct_erase_node2=NULL;

    direct_erase_node1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
    if (direct_erase_node1==NULL)
    {
        return FAILURE;
    }

    /********************************************************************************************************
     * TWOPLANE 고급 명령을 처리할 수 있는 경우 해당 채널, 칩 및 다이에서 서로 다른 두 평면에서 TWOPLANE 작업을 수행할 수 있는 블록을 찾습니다.
     * muilt_plane_flag를 TRUE로 연결
     *********************************************************************************************************/
    if((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)
    {	
        for(lv_plane=0;lv_plane<ssd->parameter->plane_die;lv_plane++)
        {
            direct_erase_node2=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
            if((lv_plane!=plane)&&(direct_erase_node2!=NULL))
            {
                if((direct_erase_node1->block)==(direct_erase_node2->block))
                {
                    muilt_plane_flag=TRUE;
                    break;
                }
            }
        }
    }

    /***************************************************************************************
     * INTERLEAVE 고급 명령이 처리될 수 있는 경우 칩은 해당 채널에서 INTERLEAVE를 실행할 수 있는 두 개의 블록을 찾습니다.
     * interleaver_flag를 TRUE로 연결
     ****************************************************************************************/
    if((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)
    {
        for(lv_die=0;lv_die<ssd->parameter->die_chip;lv_die++)
        {
            if(lv_die!=die)
            {
                for(lv_plane=0;lv_plane<ssd->parameter->plane_die;lv_plane++)
                {
                    if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node!=NULL)
                    {
                        interleaver_flag=TRUE;
                        break;
                    }
                }
            }
            if(interleaver_flag==TRUE)
            {
                break;
            }
        }
    }

    /************************************************************************************************************************
     *A, twoplane의 두 블록과 인터리버의 두 블록이 모두 실행될 수 있으면 INTERLEAVE_TWO_PLANE의 고급 명령 삭제 작업을 실행합니다.
     *B, 인터리버를 실행할 수 있는 블록이 두 개뿐인 경우 INTERLEAVE 고급 명령의 지우기 작업을 실행합니다.
     *C, TWO_PLANE를 실행할 수 있는 블록이 두 개뿐인 경우 TWO_PLANE 고급 명령의 지우기 작업을 실행합니다.
     *D, 위의 조건이 없으면 일반 지우기 작업만 수행할 수 있습니다.
     *************************************************************************************************************************/
    if ((muilt_plane_flag==TRUE)&&(interleaver_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))     
    {
        if(erase_planes(ssd,channel,chip,die,plane,INTERLEAVE_TWO_PLANE)==SUCCESS)
        {
            return SUCCESS;
        }
    } 
    else if ((interleaver_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
    {
        if(erase_planes(ssd,channel,chip,die,plane,INTERLEAVE)==SUCCESS)
        {
            return SUCCESS;
        }
    }
    else if ((muilt_plane_flag==TRUE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
    {
        if(erase_planes(ssd,channel,chip,die,plane,TWO_PLANE)==SUCCESS)
        {
            return SUCCESS;
        }
    }

    if ((normal_erase_flag==TRUE))                              /*모든 평면에 직접 삭제할 수 있는 블록이 있는 것은 아니며 현재 평면에서 일반 지우기 작업만 수행되거나 일반 명령만 실행할 수 있습니다.*/
    {
        if (erase_planes(ssd,channel,chip,die,plane,NORMAL)==SUCCESS)
        {
            return SUCCESS;
        } 
        else
        {
            return FAILURE;                                     /*대상 평면에는 직접 삭제할 수 있는 블록이 없으므로 대상 삭제 블록을 검색한 후 삭제 작업을 구현해야 합니다.*/
        }
    }
    return SUCCESS;
}


Status move_page(struct ssd_info * ssd, struct local *location, unsigned int * transfer_size)
{
    struct local *new_location=NULL;
    unsigned int free_state=0,valid_state=0;
    unsigned int lpn=0,old_ppn=0,ppn=0;

    lpn=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn;
    valid_state=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state;
    free_state=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state;
    old_ppn=find_ppn(ssd,location->channel,location->chip,location->die,location->plane,location->block,location->page);      /*이 유효한 모바일 페이지의 ppn을 기록하고 지도 또는 추가 매핑 관계의 ppn을 비교하고 삭제 및 추가 작업을 수행합니다. 记录这个有效移动页的ppn，对比map或者额外映射关系中的ppn，进行删除和添加操作*/
    ppn=get_ppn_for_gc(ssd,location->channel,location->chip,location->die,location->plane);                /*발견된 ppn은 gc 작업이 발생하는 plane에 있어야 합니다. copyback 작업을 사용하여 gc 작업에 대한 ppn을 얻으려면 //找出来的ppn一定是在发生gc操作的plane中,才能使用copyback操作，为gc操作获取ppn*/
    //get_ppn_for_gc를 통해 active block의 새로운 페이지를 찾고 그 페이지의 ppn을 반환
    new_location=find_location(ssd,ppn);                                                                   /*새로 얻은 ppn을 기반으로 new_location 가져오기*/

    if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
    {
        if (ssd->parameter->greed_CB_ad==1)                                                                /*고급 명령의 탐욕스러운 사용*/
        {
            ssd->copy_back_count++;
            ssd->gc_copy_back++;
            while (old_ppn%2!=ppn%2)
            {
                ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state=0;
                ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn=0;
                ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state=0;
                ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].invalid_page_num++;

                free(new_location);
                new_location=NULL;

                ppn=get_ppn_for_gc(ssd,location->channel,location->chip,location->die,location->plane);    /*발견된 ppn은 gc 작업이 발생하는 plane에 있어야 하며 카피백 작업을 사용하기 전에 패리티 주소 제한이 충족되어야 합니다. //找出来的ppn一定是在发生gc操作的plane中，并且满足奇偶地址限制,才能使用copyback操作*/
                ssd->program_count--;
                ssd->write_flash_count--;
                ssd->waste_page_count++;
            }
            if(new_location==NULL)
            {
                new_location=find_location(ssd,ppn);
            }
            //이전 위치의 상태, lpn, free_state등을 복사함
            ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state=free_state;
            ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn=lpn;
            ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state=valid_state;
        } 
        else
        {
            if (old_ppn%2!=ppn%2)
            {
                (* transfer_size)+=size(valid_state);
            }
            else
            {

                ssd->copy_back_count++;
                ssd->gc_copy_back++;
            }
        }	
    } 
    else
    {
        (* transfer_size)+=size(valid_state);
    }
    ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].free_state=free_state;
    ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].lpn=lpn;
    ssd->channel_head[new_location->channel].chip_head[new_location->chip].die_head[new_location->die].plane_head[new_location->plane].blk_head[new_location->block].page_head[new_location->page].valid_state=valid_state;


    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;
    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;
    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;

    if (old_ppn==ssd->dram->map->map_entry[lpn].pn)                                                     /*매핑 테이블 수정*/
    {
        ssd->dram->map->map_entry[lpn].pn=ppn;
    }

    free(new_location);
    new_location=NULL;

    return SUCCESS;
}

/*******************************************************************************************************************************************
 *대상 평면에는 직접 삭제할 수 있는 블록이 없습니다. 대상 지우기 블록을 찾은 다음 지우기 작업을 구현해야 합니다. 중단 없는 gc 작업에서 사용됩니다. 블록이 성공적으로 삭제되면 1을 반환합니다. 블록이 삭제되지 않으면 -1을 반환합니다.
 *이 함수에서 대상 채널 또는 다이가 유휴 상태인지 여부에 관계없이 가장 invalid_page_num이 가장 큰 블록을 지웁니다.
 ********************************************************************************************************************************************/
int uninterrupt_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)       
{
    unsigned int i=0,invalid_page=0;
    unsigned int block,active_block,transfer_size,free_page,page_move_count=0;                           /*invalid 페이지가 가장 많은 블록 번호를 기록*/
    struct local *  location=NULL;
    unsigned int total_invalid_page_num=0;

    if(find_active_block(ssd,channel,chip,die,plane)!=SUCCESS)                                           /*활성 블록 가져오기*/
    {
        printf("\n\n Error in uninterrupt_gc().\n");
        return ERROR;
    }
    active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;

    invalid_page=0;
    transfer_size=0;
    block=-1;
    for(i=0;i<ssd->parameter->block_plane;i++)                                                           /*invalid_page가 가장 크고 invalid_page_num이 가장 큰 블록 번호를 찾습니다.*/
    {	
        total_invalid_page_num+=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
        if((active_block!=i)&&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num>invalid_page))						
        {				
            invalid_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
            block=i;						
        }
    }
    if (block==-1)
    {
        return 1;
    }

    //if(invalid_page<5)
    //{
    //printf("\ntoo less invalid page. \t %d\t %d\t%d\t%d\t%d\t%d\t\n",invalid_page,channel,chip,die,plane,block);
    //}

    free_page=0;
    for(i=0;i<ssd->parameter->page_block;i++)		                                                     /*유효한 데이터가 있는 페이지를 다른 위치로 이동하여 저장해야 하는 경우 각 페이지를 하나씩 확인합니다.//逐个检查每个page，如果有有效数据的page需要移动到其他地方存储*/		
    {		
        if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].free_state&PG_SUB)==0x0000000f)
        {
            free_page++;
        }
        if(free_page!=0)
        {
            printf("\ntoo much free page. \t %d\t .%d\t%d\t%d\t%d\t\n",free_page,channel,chip,die,plane);
        }
        if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[i].valid_state>0) /*이 페이지는 유효한 페이지이며 카피백 작업이 필요합니다.//该页是有效页，需要copyback操作*/		
        {	
            location=(struct local * )malloc(sizeof(struct local ));
            alloc_assert(location,"location");
            memset(location,0, sizeof(struct local));

            location->channel=channel;
            location->chip=chip;
            location->die=die;
            location->plane=plane;
            location->block=block;
            location->page=i;
            move_page( ssd, location, &transfer_size);                                                   /*실제 move_page 작업*/
            page_move_count++;

            free(location);	
            location=NULL;
        }				
    }
    erase_operation(ssd,channel ,chip , die,plane ,block);	                                              /*move_page 작업이 수행된 후 즉시 블록 삭제 작업이 수행됩니다.*/

    ssd->channel_head[channel].current_state=CHANNEL_GC;									
    ssd->channel_head[channel].current_time=ssd->current_time;										
    ssd->channel_head[channel].next_state=CHANNEL_IDLE;	
    ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;								
    ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;						
    ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;			

    /***************************************************************
     *COPYBACK 고급 명령을 실행할 수 있고 COPYBACK 고급 명령을 실행할 수 없는 두 경우 모두,
     *채널의 다음 상태 시간 계산 및 칩의 다음 상태 시간 계산
     ***************************************************************/
    if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
    {
        if (ssd->parameter->greed_CB_ad==1)
        {

            ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG);			
            ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;
        } 
    } 
    else
    {

        ssd->channel_head[channel].next_state_predict_time=ssd->current_time+page_move_count*(7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tPROG)+transfer_size*SECTOR*(ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tRC);					
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;

    }

    return 1;
}


/*******************************************************************************************************************************************
 *대상 평면에는 직접 삭제할 수 있는 블록이 없습니다. 대상 지우기 블록을 찾은 다음 지우기 작업을 구현해야 합니다. 인터럽트 가능한 gc 작업에서 사용됩니다. 블록이 성공적으로 삭제되면 1을 반환합니다. 블록이 삭제되지 않으면 -1을 반환합니다.
 *이 기능에서는 대상 채널 또는 다이가 유휴 상태인지 여부에 관계없이
 ********************************************************************************************************************************************/
int interrupt_gc(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,struct gc_operation *gc_node)        
{
    unsigned int i,block,active_block,transfer_size,invalid_page=0;
    struct local *location;

    active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
    transfer_size=0;
    if (gc_node->block>=ssd->parameter->block_plane)
    {
        for(i=0;i<ssd->parameter->block_plane;i++)
        {			
            if((active_block!=i)&&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num>invalid_page))						
            {				
                invalid_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[i].invalid_page_num;
                block=i;						
            }
        }
        gc_node->block=block;
    }
    //valid page가 존재하므로 copyback 작업 수행
    if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[gc_node->block].invalid_page_num!=ssd->parameter->page_block)     /*또한 카피백 작업을 수행해야 합니다.*/
    {
        for (i=gc_node->page;i<ssd->parameter->page_block;i++)
        {
            if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[gc_node->block].page_head[i].valid_state>0)
            {
                location=(struct local * )malloc(sizeof(struct local ));
                alloc_assert(location,"location");
                memset(location,0, sizeof(struct local));

                location->channel=channel;
                location->chip=chip;
                location->die=die;
                location->plane=plane;
                location->block=block;
                location->page=i;
                transfer_size=0;

                move_page( ssd, location, &transfer_size); //copyback

                free(location);
                location=NULL;

                gc_node->page=i+1;
                ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[gc_node->block].invalid_page_num++;
                ssd->channel_head[channel].current_state=CHANNEL_C_A_TRANSFER;									
                ssd->channel_head[channel].current_time=ssd->current_time;										
                ssd->channel_head[channel].next_state=CHANNEL_IDLE;	
                ssd->channel_head[channel].chip_head[chip].current_state=CHIP_COPYBACK_BUSY;								
                ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;						
                ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;		

                if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
                {					
                    ssd->channel_head[channel].next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+7*ssd->parameter->time_characteristics.tWC;		
                    ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
                } 
                else
                {	
                    ssd->channel_head[channel].next_state_predict_time=ssd->current_time+(7+transfer_size*SECTOR)*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(7+transfer_size*SECTOR)*ssd->parameter->time_characteristics.tWC;					
                    ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
                }
                return 0;    
            }
        }
    }
    else //block내에 valid page가 존재하지 않은 경우 copyback 작업 없이 바로 erase
    {
        erase_operation(ssd,channel ,chip, die,plane,gc_node->block);	

        ssd->channel_head[channel].current_state=CHANNEL_C_A_TRANSFER;									
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;								
        ssd->channel_head[channel].next_state_predict_time=ssd->current_time+5*ssd->parameter->time_characteristics.tWC;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_ERASE_BUSY;								
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;						
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;							
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tBERS;

        return 1;                                                                      /*gc 작업이 완료되고 1이 반환되며 채널의 gc 요청 노드를 삭제할 수 있습니다.*/
    }

    printf("there is a problem in interrupt_gc\n");
    return 1;
}

/*************************************************************
 *함수의 기능은 gc 연산을 처리한 후 gc 체인에서 gc_node를 삭제하는 것입니다.
 **************************************************************/
int delete_gc_node(struct ssd_info *ssd, unsigned int channel,struct gc_operation *gc_node)
{
    struct gc_operation *gc_pre=NULL;
    if(gc_node==NULL)                                                                  
    {
        return ERROR;
    }

    if (gc_node==ssd->channel_head[channel].gc_command)
    {
        ssd->channel_head[channel].gc_command=gc_node->next_node;
    }
    else
    {
        gc_pre=ssd->channel_head[channel].gc_command;
        while (gc_pre->next_node!=NULL)
        {
            if (gc_pre->next_node==gc_node)
            {
                gc_pre->next_node=gc_node->next_node;
                break;
            }
            gc_pre=gc_pre->next_node;
        }
    }
    free(gc_node);
    gc_node=NULL;
    ssd->gc_request--;
    return SUCCESS;
}

/***************************************
 * 이 함수의 기능은 채널의 각 gc 연산을 처리하는 것입니다.
 ****************************************/
Status gc_for_channel(struct ssd_info *ssd, unsigned int channel)
{
    int flag_direct_erase=1,flag_gc=1,flag_invoke_gc=1;
    unsigned int chip,die,plane,flag_priority=0;
    unsigned int current_state=0, next_state=0;
    long long next_state_predict_time=0;
    struct gc_operation *gc_node=NULL,*gc_p=NULL;

    /*******************************************************************************************
     * 각 gc_node를 찾고, gc_node가 위치한 칩의 현재 상태, 다음 상태, 다음 상태의 예상 시간을 얻습니다.
     *현재 상태가 유휴 상태이거나 다음 상태가 유휴 상태이고 다음 상태의 예상 시간이 현재 시간보다 짧고 무정전 gc인 경우
     *그런 다음 flag_priority를 ​​1로 설정합니다. 그렇지 않으면 0입니다.
     ********************************************************************************************/
    gc_node=ssd->channel_head[channel].gc_command;
    while (gc_node!=NULL)
    {
        current_state=ssd->channel_head[channel].chip_head[gc_node->chip].current_state;
        next_state=ssd->channel_head[channel].chip_head[gc_node->chip].next_state;
        next_state_predict_time=ssd->channel_head[channel].chip_head[gc_node->chip].next_state_predict_time;
        if((current_state==CHIP_IDLE)||((next_state==CHIP_IDLE)&&(next_state_predict_time<=ssd->current_time)))
        {
            if (gc_node->priority==GC_UNINTERRUPT)                                     /*이 gc 요청은 중단할 수 없으며 이 gc 작업이 먼저 서비스됩니다.*/
            {
                flag_priority=1;
                break;
            }
        }
        gc_node=gc_node->next_node;
    }
    if (flag_priority!=1)                                                              /*중단되지 않는 gc 요청이 없습니다. 먼저 팀 리더의 gc 요청을 실행하십시오.*/
    {
        gc_node=ssd->channel_head[channel].gc_command; //gc대상 블록 연결리스트의 헤드를 가져옴
        while (gc_node!=NULL) //gc 대상 블록 리스트의 모든 노드를 다 제거할 때까지 반복
        {
            current_state=ssd->channel_head[channel].chip_head[gc_node->chip].current_state;
            next_state=ssd->channel_head[channel].chip_head[gc_node->chip].next_state;
            next_state_predict_time=ssd->channel_head[channel].chip_head[gc_node->chip].next_state_predict_time;
            /**********************************************
             *gc 작업이 필요한 대상 칩은 gc 작업을 수행할 수 있기 전에 유휴 상태입니다.
             ***********************************************/
            if((current_state==CHIP_IDLE)||((next_state==CHIP_IDLE)&&(next_state_predict_time<=ssd->current_time)))   
            {
                break;
            }
            gc_node=gc_node->next_node;
        }

    }
    if(gc_node==NULL)
    {
        return FAILURE;
    }

    chip=gc_node->chip;
    die=gc_node->die;
    plane=gc_node->plane;

    if (gc_node->priority==GC_UNINTERRUPT)
    {
        flag_direct_erase=gc_direct_erase(ssd,channel,chip,die,plane);
        if (flag_direct_erase!=SUCCESS)
        {
            flag_gc=uninterrupt_gc(ssd,channel,chip,die,plane);                         /*완전한 gc 작업이 완료되면(블록이 지워지고 일정량의 플래시 공간이 회수됨) 1을 반환하고 채널에서 해당 gc 작업 요청 노드를 삭제합니다.*/
            if (flag_gc==1)
            {
                delete_gc_node(ssd,channel,gc_node);
            }
        }
        else
        {
            delete_gc_node(ssd,channel,gc_node);
        }
        return SUCCESS;
    }
    /*******************************************************************************
     *중단 가능한 gc 요청의 경우 현재 이 채널을 사용해야 하는 하위 요청이 채널에 없는지 먼저 확인해야 합니다.
     *없으면 gc 작업을 수행하고 있는 것입니다. 있으면 gc 작업을 수행하지 않습니다.
     ********************************************************************************/
    else        
    {
        flag_invoke_gc=decide_gc_invoke(ssd,channel);                                  /*채널을 필요로 하는 하위 요청이 있는지 확인하고 이 채널을 필요로 하는 하위 요청이 있으면 gc 작업이 중단됩니다.*/

        if (flag_invoke_gc==1) //gc 작업 가능한 경우 (subrequest가 없음)
        {
            flag_direct_erase=gc_direct_erase(ssd,channel,chip,die,plane);
            if (flag_direct_erase==-1)
            {
                flag_gc=interrupt_gc(ssd,channel,chip,die,plane,gc_node);             /*완전한 gc 작업이 완료되면(블록이 지워지고 일정량의 플래시 공간이 회수됨) 1을 반환하고 채널에서 해당 gc 작업 요청 노드를 삭제합니다.*/
                if (flag_gc==1)
                {
                    delete_gc_node(ssd,channel,gc_node);
                }
            }
            else if (flag_direct_erase==1)
            {
                delete_gc_node(ssd,channel,gc_node);
            }
            return SUCCESS;
        } 
        else
        {
            return FAILURE;
        }		
    }
}



/************************************************************************************************************
 *flag는 전체 SSD가 유휴 상태일 때 gc 함수가 호출되는지 여부(1) 또는 채널, 칩, 다이 및 플레인이 호출되는지 여부(0)를 표시하는 데 사용됩니다.
 * gc 기능에 진입하면 무중단 gc 연산인지 판단해야 하며 만약 그렇다면 대상 블록의 전체 블록을 완료하기 전에 완전히 지워야 한다.
 *GC 작업을 수행하기 전에 해당 채널과 다이에 작업을 기다리는 서브 요청이 있는지 판단해야 하며, 없으면 대상을 찾기 위한 단계별 작업을 시작합니다.
 *차단 후, 한 번에 하나의 copyback 작업을 수행하고 gc 기능에서 점프한 다음 시간이 경과한 후 다음 copyback 또는 삭제 작업을 수행합니다.
 *gc 기능 진입은 반드시 gc 연산을 필요로 하는 것은 아니지만 특정 판단이 필요합니다. hard threshold 미만일 때 gc 연산을 수행해야 하며, soft threshold 미만일 때, gc 연산을 수행해야 합니다.
 *이 채널에 대기 중인 하위 요청이 있는지 판단해야 합니다(대기 중인 쓰기 하위 요청이 있고 gc의 대상 다이가 사용 중 상태가 아닌 경우 작동하지 않음).
 * 있을 경우 gc를 실행하지 않고 jump out, 그렇지 않으면 한 단계 작업을 수행할 수 있습니다.
 ************************************************************************************************************/
unsigned int gc(struct ssd_info *ssd,unsigned int channel, unsigned int flag)
{
    unsigned int i;
    int flag_direct_erase=1,flag_gc=1,flag_invoke_gc=1;
    unsigned int flag_priority=0;
    struct gc_operation *gc_node=NULL,*gc_p=NULL;

    if (flag==1)                                                                       /*전체 ssd가 IDLE인 경우*/
    {
        for (i=0;i<ssd->parameter->channel_number;i++)
        {
            flag_priority=0;
            flag_direct_erase=1;
            flag_gc=1;
            flag_invoke_gc=1;
            gc_node=NULL;
            gc_p=NULL;
            if((ssd->channel_head[i].current_state==CHANNEL_IDLE)||(ssd->channel_head[i].next_state==CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time<=ssd->current_time))
            {
                channel=i;
                if (ssd->channel_head[channel].gc_command!=NULL)
                {
                    gc_for_channel(ssd, channel);
                }
            }
        }
        return SUCCESS;

    } 
    else                                                                               /*특정 채널, 칩, 다이에 대한 gc 요청 작업만 수행하면 됩니다(대상 다이가 유휴 상태인지 확인하기 위해 판단).*/
    {
        if ((ssd->parameter->allocation_scheme==1)||((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==1)))
        {
            if ((ssd->channel_head[channel].subs_r_head!=NULL)||(ssd->channel_head[channel].subs_w_head!=NULL))    /*대기열에 요청이 있습니다. 먼저 요청을 처리하십시오.*/
            {
                return 0;
            }
        }

        gc_for_channel(ssd,channel);
        return SUCCESS;
    }
}



/**********************************************************
 *하위 요청 혈액 약 채널이 있는지 확인하고 1을 반환하지 않으면 gc 작업을 보낼 수 있습니다.
 *0이 반환되면 gc 연산을 수행할 수 없으며 gc 연산이 중단됩니다.
 ***********************************************************/
int decide_gc_invoke(struct ssd_info *ssd, unsigned int channel)      
{
    struct sub_request *sub;
    struct local *location;

    if ((ssd->channel_head[channel].subs_r_head==NULL)&&(ssd->channel_head[channel].subs_w_head==NULL))    /*여기에서 읽기 및 쓰기 하위 요청이 이 채널을 점유해야 하는지 여부를 확인합니다. 그렇지 않은 경우 GC 작업을 수행할 수 있습니다.*/
    {
        return 1;                                                                        /*현재 이 채널에 채널을 점유해야 하는 하위 요청이 없음을 나타냅니다.*/
    }
    else
    {
        if (ssd->channel_head[channel].subs_w_head!=NULL)
        {
            return 0;
        }
        else if (ssd->channel_head[channel].subs_r_head!=NULL)
        {
            sub=ssd->channel_head[channel].subs_r_head;
            while (sub!=NULL)
            {
                if (sub->current_state==SR_WAIT)                                         /*이 읽기 요청은 대기 상태이며 대상 다이가 유휴 상태이면 gc 작업을 수행할 수 없으며 0이 반환됩니다.*/
                {
                    location=find_location(ssd,sub->ppn);
                    if ((ssd->channel_head[location->channel].chip_head[location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[location->channel].chip_head[location->chip].next_state==CHIP_IDLE)&&
                                (ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time<=ssd->current_time)))
                    {
                        free(location);
                        location=NULL;
                        return 0;
                    }
                    free(location);
                    location=NULL;
                }
                else if (sub->next_state==SR_R_DATA_TRANSFER)
                {
                    location=find_location(ssd,sub->ppn);
                    if (ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time<=ssd->current_time)
                    {
                        free(location);
                        location=NULL;
                        return 0;
                    }
                    free(location);
                    location=NULL;
                }
                sub=sub->next_node;
            }
        }
        return 1;
    }
}