/*****************************************************************************************************************************
  This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
  Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

  FileName： flash.c
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

#include "flash.h"

/**********************
 *이 기능은 쓰기 요청에서만 작동합니다.
 ***********************/
Status allocate_location(struct ssd_info * ssd ,struct sub_request *sub_req)
{
    struct sub_request * update=NULL;
    unsigned int channel_num=0,chip_num=0,die_num=0,plane_num=0;
    struct local *location=NULL;

    channel_num=ssd->parameter->channel_number;
    chip_num=ssd->parameter->chip_channel[0];
    die_num=ssd->parameter->die_chip;
    plane_num=ssd->parameter->plane_die;

    //#정적할당: 정해진 규칙에 따라 채널, 칩, 다이, plane 결정. 동적할당: 정해진 규칙을 따르지 않고 동적으로 위치 결정 
    if (ssd->parameter->allocation_scheme==0)                                          /*동적 할당의 경우*/
    {
        /******************************************************************
         *동적 할당에서는 페이지 업데이트 작업이 카피백 작업을 사용할 수 없기 때문에,
         *읽기 요청이 생성되어야 하며, 읽기 요청이 완료된 후에만 페이지 쓰기가 가능합니다.
         *******************************************************************/
        if (ssd->dram->map->map_entry[sub_req->lpn].state!=0)    
        {
            if ((sub_req->state&ssd->dram->map->map_entry[sub_req->lpn].state)!=ssd->dram->map->map_entry[sub_req->lpn].state)
            {
                ssd->read_count++;
                ssd->update_read_count++;

                update=(struct sub_request *)malloc(sizeof(struct sub_request));
                alloc_assert(update,"update");
                memset(update,0, sizeof(struct sub_request));

                if(update==NULL)
                {
                    return ERROR;
                }
                update->location=NULL;
                update->next_node=NULL;
                update->next_subs=NULL;
                update->update=NULL;						
                location = find_location(ssd,ssd->dram->map->map_entry[sub_req->lpn].pn);
                update->location=location;
                update->begin_time = ssd->current_time;
                update->current_state = SR_WAIT;
                update->current_time=MAX_INT64;
                update->next_state = SR_R_C_A_TRANSFER;
                update->next_state_predict_time=MAX_INT64;
                update->lpn = sub_req->lpn;
                update->state=((ssd->dram->map->map_entry[sub_req->lpn].state^sub_req->state)&0x7fffffff);
                update->size=size(update->state);
                update->ppn = ssd->dram->map->map_entry[sub_req->lpn].pn;
                update->operation = READ;

                if (ssd->channel_head[location->channel].subs_r_tail!=NULL)            /*새 읽기 요청을 생성하고 채널의 subs_r_tail 대기열 끝에서 정지*/
                {
                    ssd->channel_head[location->channel].subs_r_tail->next_node=update;
                    ssd->channel_head[location->channel].subs_r_tail=update;
                } 
                else
                {
                    ssd->channel_head[location->channel].subs_r_tail=update;
                    ssd->channel_head[location->channel].subs_r_head=update;
                }
            }
        }
        /***************************************
         *다음은 동적 할당의 몇 가지 경우입니다.
         *0: 전체 동적 할당
         *1: 채널이 패키지, 다이 및 평면 역학을 결정함을 나타냅니다.
         ****************************************/
        switch(ssd->parameter->dynamic_allocation)
        {
            case 0:
                {
                    sub_req->location->channel=-1;
                    sub_req->location->chip=-1;
                    sub_req->location->die=-1;
                    sub_req->location->plane=-1;
                    sub_req->location->block=-1;
                    sub_req->location->page=-1;

                    if (ssd->subs_w_tail!=NULL) //write에 관한 sub request가 존재하는 경우 현재의 sub_request를 tail노드로 설정해줌
                    {
                        ssd->subs_w_tail->next_node=sub_req;
                        ssd->subs_w_tail=sub_req;
                    } 
                    else //write sub request가 존재하지 않으므로 현재 sub_req가 head이자 tail
                    {
                        ssd->subs_w_tail=sub_req;
                        ssd->subs_w_head=sub_req;
                    }

                    if (update!=NULL)
                    {
                        sub_req->update=update;
                    }

                    break;
                }
            case 1:
                {

                    sub_req->location->channel=sub_req->lpn%ssd->parameter->channel_number;
                    sub_req->location->chip=-1;
                    sub_req->location->die=-1;
                    sub_req->location->plane=-1;
                    sub_req->location->block=-1;
                    sub_req->location->page=-1;

                    if (update!=NULL)
                    {
                        sub_req->update=update;
                    }

                    break;
                }
            case 2:
                {
                    break;
                }
            case 3:
                {
                    break;
                }
        }

    }
    else//동적할당이 아닌 경우                                                                 
    {   /***************************************************************************
         *이것은 정적 할당 방식이므로 이 하위 요청의 최종 채널, 칩, 다이 및 평면을 모두 얻을 수 있습니다.
         *정적 할당 방식은 총 0, 1, 2, 3, 4, 5, 6가지가 있습니다.
         ****************************************************************************/
        switch (ssd->parameter->static_allocation)
        {
            case 0:         //no striping static allocation
                {
                    sub_req->location->channel=(sub_req->lpn/(plane_num*die_num*chip_num))%channel_num;
                    sub_req->location->chip=sub_req->lpn%chip_num;
                    sub_req->location->die=(sub_req->lpn/chip_num)%die_num;
                    sub_req->location->plane=(sub_req->lpn/(die_num*chip_num))%plane_num;
                    break;
                }
            case 1:
                {
                    sub_req->location->channel=sub_req->lpn%channel_num;
                    sub_req->location->chip=(sub_req->lpn/channel_num)%chip_num;
                    sub_req->location->die=(sub_req->lpn/(chip_num*channel_num))%die_num;
                    sub_req->location->plane=(sub_req->lpn/(die_num*chip_num*channel_num))%plane_num;

                    break;
                }
            case 2:
                {
                    sub_req->location->channel=sub_req->lpn%channel_num;
                    sub_req->location->chip=(sub_req->lpn/(plane_num*channel_num))%chip_num;
                    sub_req->location->die=(sub_req->lpn/(plane_num*chip_num*channel_num))%die_num;
                    sub_req->location->plane=(sub_req->lpn/channel_num)%plane_num;
                    break;
                }
            case 3:
                {
                    sub_req->location->channel=sub_req->lpn%channel_num;
                    sub_req->location->chip=(sub_req->lpn/(die_num*channel_num))%chip_num;
                    sub_req->location->die=(sub_req->lpn/channel_num)%die_num;
                    sub_req->location->plane=(sub_req->lpn/(die_num*chip_num*channel_num))%plane_num;
                    break;
                }
            case 4:  
                {
                    sub_req->location->channel=sub_req->lpn%channel_num;
                    sub_req->location->chip=(sub_req->lpn/(plane_num*die_num*channel_num))%chip_num;
                    sub_req->location->die=(sub_req->lpn/(plane_num*channel_num))%die_num;
                    sub_req->location->plane=(sub_req->lpn/channel_num)%plane_num;

                    break;
                }
            case 5:   
                {
                    sub_req->location->channel=sub_req->lpn%channel_num;
                    sub_req->location->chip=(sub_req->lpn/(plane_num*die_num*channel_num))%chip_num;
                    sub_req->location->die=(sub_req->lpn/channel_num)%die_num;
                    sub_req->location->plane=(sub_req->lpn/(die_num*channel_num))%plane_num;

                    break;
                }
            default : return ERROR;

        }
        if (ssd->dram->map->map_entry[sub_req->lpn].state!=0)
        {   /*다시 쓴 하위 요청의 논리 페이지는 이전에 다시 쓴 데이터를 덮어쓸 수 없습니다. 읽기 요청을 생성해야 합니다.*/ 
            if ((sub_req->state&ssd->dram->map->map_entry[sub_req->lpn].state)!=ssd->dram->map->map_entry[sub_req->lpn].state)  
            {
                ssd->read_count++;
                ssd->update_read_count++;
                update=(struct sub_request *)malloc(sizeof(struct sub_request));
                alloc_assert(update,"update");
                memset(update,0, sizeof(struct sub_request));

                if(update==NULL)
                {
                    return ERROR;
                }
                update->location=NULL;
                update->next_node=NULL;
                update->next_subs=NULL;
                update->update=NULL;						
                location = find_location(ssd,ssd->dram->map->map_entry[sub_req->lpn].pn);
                update->location=location;
                update->begin_time = ssd->current_time;
                update->current_state = SR_WAIT;
                update->current_time=MAX_INT64;
                update->next_state = SR_R_C_A_TRANSFER;
                update->next_state_predict_time=MAX_INT64;
                update->lpn = sub_req->lpn;
                update->state=((ssd->dram->map->map_entry[sub_req->lpn].state^sub_req->state)&0x7fffffff);
                update->size=size(update->state);
                update->ppn = ssd->dram->map->map_entry[sub_req->lpn].pn;
                update->operation = READ;

                if (ssd->channel_head[location->channel].subs_r_tail!=NULL)
                {
                    ssd->channel_head[location->channel].subs_r_tail->next_node=update;
                    ssd->channel_head[location->channel].subs_r_tail=update;
                } 
                else
                {
                    ssd->channel_head[location->channel].subs_r_tail=update;
                    ssd->channel_head[location->channel].subs_r_head=update;
                }
            }

            if (update!=NULL)
            {
                sub_req->update=update;

                sub_req->state=(sub_req->state|update->state);
                sub_req->size=size(sub_req->state);
            }

        }
    }
    if ((ssd->parameter->allocation_scheme!=0)||(ssd->parameter->dynamic_allocation!=0))
    {
        if (ssd->channel_head[sub_req->location->channel].subs_w_tail!=NULL)
        {
            ssd->channel_head[sub_req->location->channel].subs_w_tail->next_node=sub_req;
            ssd->channel_head[sub_req->location->channel].subs_w_tail=sub_req;
        } 
        else
        {
            ssd->channel_head[sub_req->location->channel].subs_w_tail=sub_req;
            ssd->channel_head[sub_req->location->channel].subs_w_head=sub_req;
        }
    }
    return SUCCESS;					
}	


/*******************************************************************************
 *insert2buffer 이 함수는 쓰기 요청에 대한 하위 요청 서비스를 위해 특별히 buffer_management에서 호출됩니다.
 ********************************************************************************/
struct ssd_info * insert2buffer(struct ssd_info *ssd,unsigned int lpn,int state,struct sub_request *sub,struct request *req)      
{
    int write_back_count,flag=0;     /*플래그는 새 데이터 쓰기를 위한 공간이 완료되었는지 여부를 나타내고, 0은 추가 공간이 필요함을 나타내고, 1은 비어 있음을 나타냅니다.*/
    unsigned int i,lsn,hit_flag,add_flag,sector_count,active_region_flag=0,free_sector=0;
    struct buffer_group *buffer_node=NULL,*pt,*new_node=NULL,key;
    struct sub_request *sub_req=NULL,*update=NULL;


    unsigned int sub_req_state=0, sub_req_size=0,sub_req_lpn=0;

#ifdef DEBUG
    printf("enter insert2buffer,  current time:%lld, lpn:%d, state:%d,\n",ssd->current_time,lpn,state);
#endif

    sector_count=size(state);                                                                /*버퍼에 기록해야 하는 섹터 수*/
    key.group=lpn;
    buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);    /*균형 이진 트리에서 버퍼 노드 찾기*/ 

    /************************************************************************************************
     *没有命中。
     *第一步根据这个lpn有多少子页需要写到buffer，去除已写回的lsn，为该lpn腾出位置，
     *首先即要计算出free sector（表示还有多少可以直接写的buffer节点）。
     *如果free_sector>=sector_count，即有多余的空间够lpn子请求写，不需要产生写回请求
     *否则，没有多余的空间供lpn子请求写，这时需要释放一部分空间，产生写回请求。就要creat_sub_request()

     *안타가 없습니다. (명중이 없다?)
     *첫 번째 단계는 이 lpn의 얼마나 많은 서브페이지를 버퍼에 기록해야 하는지에 따라 다시 기록된 lsn을 제거하고 lpn을 위한 공간을 만드는 것입니다.
     * 우선 여유 섹터(직접 쓸 수 있는 버퍼 노드의 수를 나타냄)를 계산해야 합니다.
     *free_sector>=sector_count이면 lpn 하위 요청이 쓸 수 있는 충분한 공간이 있으므로 다시 쓰기 요청을 생성할 필요가 없습니다.
     *그렇지 않으면 lpn 하위 요청이 쓸 추가 공간이 없고 다시 쓰기 요청을 생성하기 위해 공간의 일부를 해제해야 합니다. 그냥 creat_sub_request()
     *************************************************************************************************/
    if(buffer_node==NULL)
    {
        free_sector=ssd->dram->buffer->max_buffer_sector-ssd->dram->buffer->buffer_sector_count;   
        if(free_sector>=sector_count)
        {
            flag=1;    
        }
        if(flag==0)     
        {
            write_back_count=sector_count-free_sector; //추가로 더 필요한 공간
            ssd->dram->buffer->write_miss_hit=ssd->dram->buffer->write_miss_hit+write_back_count;
            while(write_back_count>0)
            {
                sub_req=NULL;
                sub_req_state=ssd->dram->buffer->buffer_tail->stored; 
                sub_req_size=size(ssd->dram->buffer->buffer_tail->stored);
                sub_req_lpn=ssd->dram->buffer->buffer_tail->group;
                sub_req=creat_sub_request(ssd,sub_req_lpn,sub_req_size,sub_req_state,req,WRITE);

                /**********************************************************************************
                 *req不为空，表示这个insert2buffer函数是在buffer_management中调用，传递了request进来
                 *req为空，表示这个函数是在process函数中处理一对多映射关系的读的时候，需要将这个读出
                 *的数据加到buffer中，这可能产生实时的写回操作，需要将这个实时的写回操作的子请求挂在
                 *这个读请求的总请求上

                 *req는 비어 있지 않으며, 이는 insert2buffer 함수가 buffer_management에서 호출되고 요청이 전달되었음을 나타냅니다.
                 *req는 비어 있으며, 프로세스 함수에서 일대다 매핑 관계 읽기를 처리할 때 이 함수가 이를 읽어야 함을 나타냅니다.
                 *실시간 쓰기 되돌림 작업을 생성할 수 있는 버퍼에 데이터가 추가되고 이 실시간 쓰기 되돌림 작업의 하위 요청이 이 읽기 요청의 전체 요청에 중단되어야 합니다.
                 ***********************************************************************************/
                if(req!=NULL)                                             
                {
                }
                else    
                {
                    sub_req->next_subs=sub->next_subs;
                    sub->next_subs=sub_req;
                }

                /*********************************************************************
                 *쓰기 요청이 균형 이진 트리에 삽입된 다음 dram의 buffer_sector_count를 수정해야 합니다.
                 * 균형 이진 트리를 유지하려면 avlTreeDel() 및 AVL_TREENODE_FREE() 함수를 호출하고 LRU 알고리즘을 유지합니다.
                 **********************************************************************/
                ssd->dram->buffer->buffer_sector_count=ssd->dram->buffer->buffer_sector_count-sub_req->size;
                pt = ssd->dram->buffer->buffer_tail; //dram buffer의 tail node를 pt에 할당
                avlTreeDel(ssd->dram->buffer, (TREE_NODE *) pt); //pt를 dram의 avlTree에서 제거
                if(ssd->dram->buffer->buffer_head->LRU_link_next == NULL){
                    ssd->dram->buffer->buffer_head = NULL;
                    ssd->dram->buffer->buffer_tail = NULL;
                }else{
                    ssd->dram->buffer->buffer_tail=ssd->dram->buffer->buffer_tail->LRU_link_pre;
                    ssd->dram->buffer->buffer_tail->LRU_link_next=NULL;
                }
                pt->LRU_link_next=NULL;
                pt->LRU_link_pre=NULL;
                AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *) pt);
                pt = NULL;

                write_back_count=write_back_count-sub_req->size;                            /*실시간 후기입 작업으로 인해 활성 후기입 작업 영역을 늘려야 합니다.*/
            }
        }

        /******************************************************************************
         * 버퍼 노드를 생성하고, 이 페이지의 상황에 따라 각 멤버를 할당하고, 팀 헤드 및 바이너리 트리에 추가
         *******************************************************************************/
        new_node=NULL;
        new_node=(struct buffer_group *)malloc(sizeof(struct buffer_group));
        alloc_assert(new_node,"buffer_group_node");
        memset(new_node,0, sizeof(struct buffer_group));

        new_node->group=lpn;
        new_node->stored=state;
        new_node->dirty_clean=state;
        new_node->LRU_link_pre = NULL;
        new_node->LRU_link_next=ssd->dram->buffer->buffer_head;
        if(ssd->dram->buffer->buffer_head != NULL){
            ssd->dram->buffer->buffer_head->LRU_link_pre=new_node;
        }else{
            ssd->dram->buffer->buffer_tail = new_node;
        }
        ssd->dram->buffer->buffer_head=new_node;
        new_node->LRU_link_pre=NULL;
        avlTreeAdd(ssd->dram->buffer, (TREE_NODE *) new_node);
        ssd->dram->buffer->buffer_sector_count += sector_count;
    }
    /****************************************************************************************
     *버퍼에 부딪힌 경우
     *적중했지만 LPN만 적중되었습니다. 새 쓰기 요청은 LPN 페이지의 일부 하위 페이지만 쓰기만 하면 될 수 있습니다.
     *이때 추가적인 판단이 필요하다
     *****************************************************************************************/
    else
    {
        for(i=0;i<ssd->parameter->subpage_page;i++)
        {
            /*************************************************************
             * 상태의 i번째 비트가 1인지 확인
             *그리고 i번째 섹터가 버퍼에 존재하는지 판단하고, 1은 존재함, 0은 존재하지 않음을 의미합니다.
             **************************************************************/
            if((state>>i)%2!=0)                                                         
            {
                lsn=lpn*ssd->parameter->subpage_page+i;
                hit_flag=0;
                hit_flag=(buffer_node->stored)&(0x00000001<<i);

                if(hit_flag!=0)				                                          /*히트, 버퍼의 헤드로 노드를 이동하고 히트 lsn을 표시해야 합니다.*/
                {	
                    active_region_flag=1;                                             /*이 버퍼 노드의 lsn이 적중되었는지 여부를 기록하는 데 사용되며, 이는 나중에 임계값을 결정하는 데 사용됩니다.*/

                    if(req!=NULL)
                    {
                        if(ssd->dram->buffer->buffer_head!=buffer_node)     
                        {				
                            if(ssd->dram->buffer->buffer_tail==buffer_node)
                            {				
                                ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;
                                buffer_node->LRU_link_pre->LRU_link_next=NULL;					
                            }				
                            else if(buffer_node != ssd->dram->buffer->buffer_head)
                            {					
                                buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;				
                                buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;
                            }				
                            buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;	
                            ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;
                            buffer_node->LRU_link_pre=NULL;				
                            ssd->dram->buffer->buffer_head=buffer_node;					
                        }					
                        ssd->dram->buffer->write_hit++;
                        req->complete_lsn_count++;                                        /*키 버퍼에 도달할 때 req->complete_lsn_count++를 사용하여 데이터가 버퍼에 기록되었음을 나타냅니다.*/					
                    }
                    else
                    {
                    }				
                }			
                else                 			
                {
                    /************************************************************************************************************
                     *lsn은 히트하지 않았지만 노드는 버퍼에 있습니다. 이 lsn은 버퍼의 해당 노드에 추가되어야 합니다.
                     * 버퍼의 끝에서 노드를 찾고, 노드에서 다시 쓰여진 lsn을 삭제하고(찾으면), 이 노드의 상태를 변경하고, 동시에 이 새 항목을 작성합니다.
                     *lsn이 해당 버퍼 노드에 추가되면 해당 노드가 버퍼 헤드에 있을 수 있으며 그렇지 않은 경우 헤드로 이동합니다. 다시 쓰여진 lsn을 찾을 수 없는 경우 버퍼에서
                     *노드는 전체적으로 다시 쓸 그룹을 찾고 이 요청에 대해 이 하위 요청을 중단합니다. 미리 채널에 걸 수 있습니다.
                     *첫 번째 단계: 새로운 lsn을 위한 공간을 만들기 위해 버퍼 큐 끝에 다시 쓰여진 노드를 삭제합니다. 여기에서 큐 끝에 있는 노드의 저장된 상태를 수정해야 합니다.
                     * 추가됨, 삭제할 수 있는 lsn이 없는 경우 LRU의 마지막 노드에 다시 쓰기 위해 새로운 쓰기 하위 요청을 생성해야 합니다.
                     *2단계: 버퍼 노드에 새 lsn을 추가합니다.
                     *************************************************************************************************************/	
                    ssd->dram->buffer->write_miss_hit++; //xx_miss_hit가 lsn은 miss지만 노드는 버퍼에 있는 경우를 말하는 듯

                    if(ssd->dram->buffer->buffer_sector_count>=ssd->dram->buffer->max_buffer_sector)
                    {
                        if (buffer_node==ssd->dram->buffer->buffer_tail)                  /*히트 노드가 버퍼의 마지막 노드인 경우 마지막 두 노드를 교환합니다.*/
                        {
                            pt = ssd->dram->buffer->buffer_tail->LRU_link_pre;
                            ssd->dram->buffer->buffer_tail->LRU_link_pre=pt->LRU_link_pre;
                            ssd->dram->buffer->buffer_tail->LRU_link_pre->LRU_link_next=ssd->dram->buffer->buffer_tail;
                            ssd->dram->buffer->buffer_tail->LRU_link_next=pt;
                            pt->LRU_link_next=NULL;
                            pt->LRU_link_pre=ssd->dram->buffer->buffer_tail;
                            ssd->dram->buffer->buffer_tail=pt;
                        }
                        sub_req=NULL;
                        sub_req_state=ssd->dram->buffer->buffer_tail->stored; 
                        sub_req_size=size(ssd->dram->buffer->buffer_tail->stored);
                        sub_req_lpn=ssd->dram->buffer->buffer_tail->group;
                        sub_req=creat_sub_request(ssd,sub_req_lpn,sub_req_size,sub_req_state,req,WRITE);

                        if(req!=NULL)           
                        {

                        }
                        else if(req==NULL)   
                        {
                            sub_req->next_subs=sub->next_subs;
                            sub->next_subs=sub_req;
                        }

                        ssd->dram->buffer->buffer_sector_count=ssd->dram->buffer->buffer_sector_count-sub_req->size;
                        pt = ssd->dram->buffer->buffer_tail;	
                        avlTreeDel(ssd->dram->buffer, (TREE_NODE *) pt);

                        /************************************************************************/
                        /*변경: 버퍼의 노드는 하위 요청이 중단된 직후에 삭제되지 않아야 합니다.	    */
                        /*삭제하기 전에 다시 쓸 때까지 기다려야 합니다.							   */
                        /************************************************************************/
                        if(ssd->dram->buffer->buffer_head->LRU_link_next == NULL)
                        {
                            ssd->dram->buffer->buffer_head = NULL;
                            ssd->dram->buffer->buffer_tail = NULL;
                        }else{
                            ssd->dram->buffer->buffer_tail=ssd->dram->buffer->buffer_tail->LRU_link_pre;
                            ssd->dram->buffer->buffer_tail->LRU_link_next=NULL;
                        }
                        pt->LRU_link_next=NULL;
                        pt->LRU_link_pre=NULL;
                        AVL_TREENODE_FREE(ssd->dram->buffer, (TREE_NODE *) pt);
                        pt = NULL;	
                    }

                    /*2단계: 버퍼 노드에 새 lsn 추가*/	
                    add_flag=0x00000001<<(lsn%ssd->parameter->subpage_page);

                    if(ssd->dram->buffer->buffer_head!=buffer_node)                      /*버퍼 노드가 버퍼의 큐 헤드에 없으면 이 노드를 큐의 헤드에 언급해야 합니다.*/
                    {//헤드로 옮기는 과정 + 원래 위치의 앞뒤노드 연결				
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
                    buffer_node->stored=buffer_node->stored|add_flag;		
                    buffer_node->dirty_clean=buffer_node->dirty_clean|add_flag;	
                    ssd->dram->buffer->buffer_sector_count++;
                }			

            }
        }
    }

    return ssd;
}

/**************************************************************************************
 *함수의 기능은 활성 블록을 찾는 것입니다.각 plane에는 활성 블록이 하나만 있어야 하며 이 활성 블록만 작동할 수 있습니다.
 ***************************************************************************************/
Status  find_active_block(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane)
{
    unsigned int active_block;
    unsigned int free_page_num=0;
    unsigned int count=0;

    active_block=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block;
    free_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
    //last_write_page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;

    //active block => free page가 존재하는 block
    while((free_page_num==0)&&(count<ssd->parameter->block_plane)) //free page가 존재하는 블럭을 찾을 때까지 반복 (plane당 block의 수만큼 또 반복)
    {
        active_block=(active_block+1)%ssd->parameter->block_plane;	
        free_page_num=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num;
        count++;
    }
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].active_block=active_block; //plane에서 찾은 active block을 설정
    if(count<ssd->parameter->block_plane)
    {
        return SUCCESS;
    }
    else //count = block_plane이니까 해당 plane에 free page가 있는 block이 없는 경우
    {
        return FAILURE;
    }
}

/*************************************************
 *이 함수의 기능은 실제 쓰기 작업을 시뮬레이션하는 것입니다.
 *이 페이지의 관련 매개변수와 전체 ssd의 통계 매개변수를 변경하는 것입니다.
 **************************************************/
Status write_page(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int active_block,unsigned int *ppn)
{
    int last_write_page=0;
    last_write_page=++(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page);	
    if(last_write_page>=(int)(ssd->parameter->page_block))
    {
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].last_write_page=0;
        printf("error! the last write page larger than 64!!\n");
        return ERROR;
    }

    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].free_page_num--; 
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[active_block].page_head[last_write_page].written_count++;
    ssd->write_flash_count++;    
    *ppn=find_ppn(ssd,channel,chip,die,plane,active_block,last_write_page);

    return SUCCESS;
}

/**********************************************
 *이 함수의 기능은 lpn, 크기, 상태를 기반으로 하위 요청을 생성하는 것입니다.
 **********************************************/
struct sub_request * creat_sub_request(struct ssd_info * ssd,unsigned int lpn,int size,unsigned int state,struct request * req,unsigned int operation)
{
    struct sub_request* sub=NULL,* sub_r=NULL;
    struct channel_info * p_ch=NULL;
    struct local * loc=NULL;
    unsigned int flag=0;

    sub = (struct sub_request*)malloc(sizeof(struct sub_request));                        /*하위 요청 구조 요청*/
    alloc_assert(sub,"sub_request");
    memset(sub,0, sizeof(struct sub_request));

    if(sub==NULL)
    {
        return NULL;
    }
    sub->location=NULL;
    sub->next_node=NULL;
    sub->next_subs=NULL;
    sub->update=NULL;

    if(req!=NULL)
    {
        sub->next_subs = req->subs;
        req->subs = sub;
    }

    /*************************************************************************************
     *읽기 동작의 경우 읽기 서브 요청 큐에 이 서브 요청과 동일한 서브 요청이 있는지 여부를 미리 판단하는 것이 매우 중요합니다.
     *있는 경우 새 하위 요청을 실행할 필요가 없으며 새 하위 요청이 완료로 직접 할당됩니다.
     **************************************************************************************/
    if (operation == READ)
    {	
        loc = find_location(ssd,ssd->dram->map->map_entry[lpn].pn);
        sub->location=loc;
        sub->begin_time = ssd->current_time;
        sub->current_state = SR_WAIT;
        sub->current_time=MAX_INT64;
        sub->next_state = SR_R_C_A_TRANSFER;
        sub->next_state_predict_time=MAX_INT64;
        sub->lpn = lpn;
        sub->size=size;                                                               /*하위 요청의 요청 크기를 계산해야 합니다.*/

        p_ch = &ssd->channel_head[loc->channel];	
        sub->ppn = ssd->dram->map->map_entry[lpn].pn;
        sub->operation = READ;
        sub->state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);
        sub_r=p_ch->subs_r_head;                                                      /*다음 줄에는 읽기 하위 요청 대기열에 이 하위 요청과 동일한 하위 요청이 있는지 여부를 결정하는 플래그가 포함되어 있으며, 그렇다면 새 하위 요청을 완료된 것으로 할당합니다.*/
        flag=0;
        while (sub_r!=NULL)
        {
            if (sub_r->ppn==sub->ppn)
            {
                flag=1;
                break;
            }
            sub_r=sub_r->next_node;
        }
        if (flag==0)
        {
            if (p_ch->subs_r_tail!=NULL) //channel에 sub request가 존재하는 경우 sub를 tail로 설정
            {
                p_ch->subs_r_tail->next_node=sub;
                p_ch->subs_r_tail=sub;
            } 
            else //channel에 sub request가 없는 경우 sub를 head로 설정
            {
                p_ch->subs_r_head=sub;
                p_ch->subs_r_tail=sub;
            }
        }
        else
        {
            sub->current_state = SR_R_DATA_TRANSFER;
            sub->current_time=ssd->current_time;
            sub->next_state = SR_COMPLETE;
            sub->next_state_predict_time=ssd->current_time+1000;
            sub->complete_time=ssd->current_time+1000;
        }
    }
    /*************************************************************************************
     *쓰기 요청의 경우 정적 할당 및 동적 할당을 처리하기 위해 assign_location(ssd, sub) 함수를 사용해야 합니다.
     **************************************************************************************/
    else if(operation == WRITE)
    {                                
        sub->ppn=0;
        sub->operation = WRITE;
        sub->location=(struct local *)malloc(sizeof(struct local));
        alloc_assert(sub->location,"sub->location");
        memset(sub->location,0, sizeof(struct local));

        sub->current_state=SR_WAIT;
        sub->current_time=ssd->current_time;
        sub->lpn=lpn;
        sub->size=size;
        sub->state=state;
        sub->begin_time=ssd->current_time;

        if (allocate_location(ssd ,sub)==ERROR) //위치할당에 실패한 경우
        {
            free(sub->location);
            sub->location=NULL;
            free(sub);
            sub=NULL;
            return NULL;
        }

    }
    else //read/write가 아닌 경우
    {
        free(sub->location);
        sub->location=NULL;
        free(sub);
        sub=NULL;
        printf("\nERROR ! Unexpected command.\n");
        return NULL;
    }

    return sub;
}

/******************************************************
 *함수의 기능은 주어진 채널, 칩, 다이에서 읽기 하위 요청을 찾는 것입니다.
 * 이 하위 요청의 ppn은 해당 평면의 레지스터에 있는 ppn과 일치해야 합니다.
 *******************************************************/
struct sub_request * find_read_sub_request(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die)
{
    unsigned int plane=0;
    unsigned int address_ppn=0;
    struct sub_request *sub=NULL,* p=NULL;

    for(plane=0;plane<ssd->parameter->plane_die;plane++)
    {
        address_ppn=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].add_reg_ppn;
        if(address_ppn!=-1)
        {
            sub=ssd->channel_head[channel].subs_r_head; //sub request list의 head -> sub
            if(sub->ppn==address_ppn) //ppn이 바로 맞으면
            {
                if(sub->next_node==NULL) //다음 하위 요청이 없는 경우
                {
                    ssd->channel_head[channel].subs_r_head=NULL;
                    ssd->channel_head[channel].subs_r_tail=NULL;
                }
                ssd->channel_head[channel].subs_r_head=sub->next_node;
            }
            while((sub->ppn!=address_ppn)&&(sub->next_node!=NULL)) //다음 하위 요청 노드가 있는 경우
            {
                if(sub->next_node->ppn==address_ppn)
                {
                    p=sub->next_node;
                    if(p->next_node==NULL)
                    {
                        sub->next_node=NULL;
                        ssd->channel_head[channel].subs_r_tail=sub;
                    }
                    else
                    {
                        sub->next_node=p->next_node;
                    }
                    sub=p;
                    break;
                }
                sub=sub->next_node;
            }
            if(sub->ppn==address_ppn)
            {
                sub->next_node=NULL;
                return sub;
            }
            else 
            {
                printf("Error! Can't find the sub request.");
            }
        }
    }
    return NULL;
}

/*******************************************************************************
 *함수의 기능은 쓰기 하위 요청을 찾는 것입니다.
 * 두 가지 경우 1이 있습니다. 완전히 동적 할당인 경우 ssd->subs_w_head 대기열에서 찾을 수 있습니다.
 *2, 완전히 동적으로 할당되지 않은 경우 ssd->channel_head[channel].subs_w_head 대기열에서 조회합니다.
 ********************************************************************************/
struct sub_request * find_write_sub_request(struct ssd_info * ssd, unsigned int channel)
{
    struct sub_request * sub=NULL,* p=NULL;
    if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))    /*완전 동적 할당*/
    {
        sub=ssd->subs_w_head;
        while(sub!=NULL)        							
        {
            if(sub->current_state==SR_WAIT)								
            {
                if (sub->update!=NULL)                                                      /*미리 읽어야 할 페이지가 있는 경우*/
                {
                    if ((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))   //업데이트된 페이지를 읽었습니다.
                    {
                        break;
                    }
                } 
                else
                {
                    break;
                }						
            }
            p=sub;
            sub=sub->next_node;							
        }

        if (sub==NULL)                                                                      /*제공할 하위 요청이 없으면 for 루프를 중단합니다.*/
        {
            return NULL;
        }

        if (sub!=ssd->subs_w_head)
        {
            if (sub!=ssd->subs_w_tail)
            {
                p->next_node=sub->next_node;
            }
            else
            {
                ssd->subs_w_tail=p;
                ssd->subs_w_tail->next_node=NULL;
            }
        } 
        else
        {
            if (sub->next_node!=NULL)
            {
                ssd->subs_w_head=sub->next_node;
            } 
            else
            {
                ssd->subs_w_head=NULL;
                ssd->subs_w_tail=NULL;
            }
        }
        sub->next_node=NULL;
        if (ssd->channel_head[channel].subs_w_tail!=NULL)
        {
            ssd->channel_head[channel].subs_w_tail->next_node=sub;
            ssd->channel_head[channel].subs_w_tail=sub;
        } 
        else
        {
            ssd->channel_head[channel].subs_w_tail=sub;
            ssd->channel_head[channel].subs_w_head=sub;
        }
    }
    /**********************************************************
     *완전 동적 할당 방법 외에도 특정 채널에 다른 요청 방법이 할당되어 있습니다.
     * 채널에서 제공할 하위 요청을 찾기만 하면 됩니다.
     ***********************************************************/
    else  //정적할당
    {
        sub=ssd->channel_head[channel].subs_w_head;
        while(sub!=NULL)        						
        {
            if(sub->current_state==SR_WAIT)								
            {
                if (sub->update!=NULL)    
                {
                    if ((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))   //업데이트된 페이지를 읽었습니다.
                    {
                        break;
                    }
                } 
                else
                {
                    break;
                }						
            }
            p=sub;
            sub=sub->next_node;							
        }

        if (sub==NULL)
        {
            return NULL;
        }
    }

    return sub;
}

/*********************************************************************************************
 *읽기 하위 요청을 처리하는 전용 기능
 *1, 읽기 하위 요청의 현재 상태가 SR_R_C_A_TRANSFER인 경우에만
 *2, 읽기 서브 요청의 현재 상태가 SR_COMPLETE이거나 다음 상태가 SR_COMPLETE이고 다음 상태의 도달 시간이 현재 시간보다 짧은 경우
 **********************************************************************************************/
Status services_2_r_cmd_trans_and_complete(struct ssd_info * ssd)
{
    unsigned int i=0;
    struct sub_request * sub=NULL, * p=NULL;
    for(i=0;i<ssd->parameter->channel_number;i++)                                       /*이 루프 처리는 채널의 시간을 필요로 하지 않으며(읽기 명령이 칩에 도달하고 칩이 준비에서 사용 중으로 변경됨) 읽기 요청이 완료되면 채널의 큐에서 제거됩니다.*/
    {
        sub=ssd->channel_head[i].subs_r_head;

        while(sub!=NULL)
        {
            if(sub->current_state==SR_R_C_A_TRANSFER)                                  /*읽기 명령이 전송된 후 해당 다이를 사용 중으로 설정하고 서브 상태를 수정합니다. 이 부분은 패스 명령의 현재 상태에서 다이 시작 사용 중 및 다이 시작 사용 중까지 읽기 요청을 처리하는 데 전념합니다. 채널을 비워둘 필요가 없으므로 별도로 나열됩니다.*/
            {
                if(sub->next_state_predict_time<=ssd->current_time)
                {
                    go_one_step(ssd, sub,NULL, SR_R_READ,NORMAL);                      /*상태 전환 핸들러*/

                }
            }
            else if((sub->current_state==SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=ssd->current_time)))					
            {			
                if(sub!=ssd->channel_head[i].subs_r_head)                             /*if the request is completed, we delete it from read queue */							
                {		
                    p->next_node=sub->next_node;						
                }			
                else					
                {	
                    if (ssd->channel_head[i].subs_r_head!=ssd->channel_head[i].subs_r_tail)
                    {
                        ssd->channel_head[i].subs_r_head=sub->next_node;
                    } 
                    else
                    {
                        ssd->channel_head[i].subs_r_head=NULL;
                        ssd->channel_head[i].subs_r_tail=NULL;
                    }							
                }			
            }
            p=sub;
            sub=sub->next_node;
        }
    }

    return SUCCESS;
}

/**************************************************************************
 *이 기능은 또한 읽기 하위 요청만 처리하며 처리 칩의 현재 상태는 CHIP_WAIT입니다.
 *또는 다음 상태가 CHIP_DATA_TRANSFER이고 다음 상태에 대한 예상 시간이 현재 시간보다 짧은 칩
 ***************************************************************************/
Status services_2_r_data_trans(struct ssd_info * ssd,unsigned int channel,unsigned int * channel_busy_flag, unsigned int * change_current_time_flag)
{
    int chip=0;
    unsigned int die=0,plane=0,address_ppn=0,die1=0;
    struct sub_request * sub=NULL, * p=NULL,*sub1=NULL;
    struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
    struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;
    for(chip=0;chip<ssd->channel_head[channel].chip;chip++)           			    
    {				       		      
        if((ssd->channel_head[channel].chip_head[chip].current_state==CHIP_WAIT)||((ssd->channel_head[channel].chip_head[chip].next_state==CHIP_DATA_TRANSFER)&&
                    (ssd->channel_head[channel].chip_head[chip].next_state_predict_time<=ssd->current_time)))					       					
        {
            for(die=0;die<ssd->parameter->die_chip;die++)
            {
                sub=find_read_sub_request(ssd,channel,chip,die);                   /*채널, 칩, 다이에서 읽기 하위 요청 찾기*/
                if(sub!=NULL)
                {
                    break;
                }
            }

            if(sub==NULL)
            {
                continue;
            }

            /**************************************************************************************
             *ssd가 고급 명령을 지원하면 AD_TWOPLANE_READ 및 AD_INTERLEAVE를 함께 지원하는 읽기 하위 요청을 처리할 수 있습니다.
             *1, 두 개의 평면 작업이 있을 수 있으며 이 경우 동일한 다이에 있는 두 평면의 데이터가 차례로 전송됩니다.
             *2, 인터리브 연산이 있을 수 있으며, 이 경우 서로 다른 다이에 있는 두 플레인의 데이터가 차례로 전송됩니다.
             ***************************************************************************************/
            if(((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)==AD_TWOPLANE_READ)||((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
            {
                if ((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)==AD_TWOPLANE_READ)     /*2 plane 연산 생성이 가능하며 이 경우 같은 다이에 있는 두 plane의 데이터가 차례로 출력된다.*/
                {
                    sub_twoplane_one=sub;
                    sub_twoplane_two=NULL;                                                      
                    /*발견된 sub_twoplane_two가 sub_twoplane_one과 다른지 확인하려면 add_reg_ppn=-1로 설정합니다.*/
                    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[sub->location->plane].add_reg_ppn=-1;
                    sub_twoplane_two=find_read_sub_request(ssd,channel,chip,die);               /*동일한 채널, 칩, 다이에서 다른 읽기 하위 요청 찾기*/

                    /******************************************************
                     *발견되면 TWO_PLANE의 상태 전환 기능 go_one_step을 실행합니다.
                     * 찾지 못한 경우 일반 명령의 상태 전환 기능 go_one_step을 실행합니다.
                     ******************************************************/
                    if (sub_twoplane_two==NULL)
                    {
                        go_one_step(ssd, sub_twoplane_one,NULL, SR_R_DATA_TRANSFER,NORMAL);   
                        *change_current_time_flag=0;   
                        *channel_busy_flag=1;

                    }
                    else
                    {
                        go_one_step(ssd, sub_twoplane_one,sub_twoplane_two, SR_R_DATA_TRANSFER,TWO_PLANE);
                        *change_current_time_flag=0;  
                        *channel_busy_flag=1;

                    }
                } 
                else if ((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)      /*인터리브 연산을 생성하는 것이 가능하며, 이 경우 서로 다른 다이에 있는 두 플레인의 데이터가 차례로 전송됩니다.*/
                {
                    sub_interleave_one=sub;
                    sub_interleave_two=NULL;
                    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[sub->location->plane].add_reg_ppn=-1;

                    for(die1=0;die1<ssd->parameter->die_chip;die1++)
                    {	
                        if(die1!=die)
                        {
                            sub_interleave_two=find_read_sub_request(ssd,channel,chip,die1);    /*다른 칩을 사용하여 동일한 채널에서 다른 읽기 하위 요청 찾기*/
                            if(sub_interleave_two!=NULL)
                            {
                                break;
                            }
                        }
                    }	
                    if (sub_interleave_two==NULL)
                    {
                        go_one_step(ssd, sub_interleave_one,NULL, SR_R_DATA_TRANSFER,NORMAL);

                        *change_current_time_flag=0;  
                        *channel_busy_flag=1;

                    }
                    else
                    {
                        go_one_step(ssd, sub_twoplane_one,sub_interleave_two, SR_R_DATA_TRANSFER,INTERLEAVE);

                        *change_current_time_flag=0;   
                        *channel_busy_flag=1;

                    }
                }
            }
            else                                                                                 /*ssd가 고급 명령을 지원하지 않는 경우 읽기 하위 요청을 하나씩 실행합니다.*/
            {

                go_one_step(ssd, sub,NULL, SR_R_DATA_TRANSFER,NORMAL);
                *change_current_time_flag=0;  
                *channel_busy_flag=1;

            }
            break;
        }		

        if(*channel_busy_flag==1)
        {
            break;
        }
    }		
    return SUCCESS;
}


/******************************************************
 *이 기능은 또한 읽기 하위 요청과 대기 상태의 읽기 하위 요청만 제공합니다.
 *******************************************************/
int services_2_r_wait(struct ssd_info * ssd,unsigned int channel,unsigned int * channel_busy_flag, unsigned int * change_current_time_flag)
{
    unsigned int plane=0,address_ppn=0;
    struct sub_request * sub=NULL, * p=NULL;
    struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
    struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;


    sub=ssd->channel_head[channel].subs_r_head;


    if ((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)==AD_TWOPLANE_READ)         /*to find whether there are two sub request can be served by two plane operation*/
    {
        sub_twoplane_one=NULL;
        sub_twoplane_two=NULL;                                                         
        /*two_plane을 수행할 수 있는 두 개의 읽기 요청 찾기*/
        find_interleave_twoplane_sub_request(ssd,channel,sub_twoplane_one,sub_twoplane_two,TWO_PLANE);

        if (sub_twoplane_two!=NULL)                                                     /*두 개의 평면 읽기 작업을 수행할 수 있습니다.*/
        {
            go_one_step(ssd, sub_twoplane_one,sub_twoplane_two, SR_R_C_A_TRANSFER,TWO_PLANE);

            *change_current_time_flag=0;
            *channel_busy_flag=1;                                                       /*이미 이 주기를 점유한 버스는 다이에서 데이터 반환을 수행할 필요가 없습니다.*/
        } 
        else if((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)       /*조건에 맞는 두 페이지가 없고, 인터리브 읽기 명령이 없을 때 한 페이지만 읽을 수 있음*/
        {
            while(sub!=NULL)                                                            /*if there are read requests in queue, send one of them to target die*/			
            {		
                if(sub->current_state==SR_WAIT)									
                {	                                                                    /*다음 판단 조건은 services_2_r_data_trans의 판단 조건과 다릅니다.*/
                    if((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state==CHIP_IDLE)&&
                                (ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state_predict_time<=ssd->current_time)))												
                    {	
                        go_one_step(ssd, sub,NULL, SR_R_C_A_TRANSFER,NORMAL);

                        *change_current_time_flag=0;
                        *channel_busy_flag=1;                                           /*이미 이 주기를 점유한 버스는 다이에서 데이터 반환을 수행할 필요가 없습니다.*/
                        break;										
                    }	
                    else
                    {
                        /*다이의 혼잡으로 인한 막힘*/
                    }
                }						
                sub=sub->next_node;								
            }
        }
    } 
    if ((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)               /*to find whether there are two sub request can be served by INTERLEAVE operation*/
    {
        sub_interleave_one=NULL;
        sub_interleave_two=NULL;
        find_interleave_twoplane_sub_request(ssd,channel,sub_interleave_one,sub_interleave_two,INTERLEAVE);

        if (sub_interleave_two!=NULL)                                                  /*인터리브 읽기 작업을 수행할 수 있습니다.*/
        {

            go_one_step(ssd, sub_interleave_one,sub_interleave_two, SR_R_C_A_TRANSFER,INTERLEAVE);

            *change_current_time_flag=0;
            *channel_busy_flag=1;                                                      /*이미 이 주기를 점유한 버스는 다이에서 데이터 반환을 수행할 필요가 없습니다.*/
        } 
        else                                                                           /*조건에 맞는 두 페이지가 없을 경우 한 페이지만 읽을 수 있습니다.*/
        {
            while(sub!=NULL)                                                           /*if there are read requests in queue, send one of them to target die*/			
            {		
                if(sub->current_state==SR_WAIT)									
                {	
                    if((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state==CHIP_IDLE)&&
                                (ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state_predict_time<=ssd->current_time)))												
                    {	

                        go_one_step(ssd, sub,NULL, SR_R_C_A_TRANSFER,NORMAL);

                        *change_current_time_flag=0;
                        *channel_busy_flag=1;                                          /*이미 이 주기를 점유한 버스는 다이에서 데이터 반환을 수행할 필요가 없습니다.*/
                        break;										
                    }	
                    else
                    {
                        /*다이의 혼잡으로 인한 막힘*/
                    }
                }						
                sub=sub->next_node;								
            }
        }
    }

    /*******************************
     *ssd가 고급 명령을 실행할 수 없는 경우
     *******************************/
    if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE_READ)!=AD_TWOPLANE_READ))
    {
        while(sub!=NULL)                                                               /*if there are read requests in queue, send one of them to target chip*/			
        {		
            if(sub->current_state==SR_WAIT)									
            {	                                                                       
                if((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state==CHIP_IDLE)&&
                            (ssd->channel_head[sub->location->channel].chip_head[sub->location->chip].next_state_predict_time<=ssd->current_time)))												
                {	

                    go_one_step(ssd, sub,NULL, SR_R_C_A_TRANSFER,NORMAL);

                    *change_current_time_flag=0;
                    *channel_busy_flag=1;                                              /*이미 이 주기를 점유한 버스는 다이에서 데이터 반환을 수행할 필요가 없습니다.*/
                    break;										
                }	
                else
                {
                    /*다이의 혼잡으로 인한 막힘*/
                }
            }						
            sub=sub->next_node;								
        }
    }

    return SUCCESS;
}

/*********************************************************************
 *쓰기 하위 요청이 처리되면 요청 큐에서 삭제해야 하며 이 기능을 수행하는 기능입니다.
 **********************************************************************/
int delete_w_sub_request(struct ssd_info * ssd, unsigned int channel, struct sub_request * sub )
{
    struct sub_request * p=NULL;
    if (sub==ssd->channel_head[channel].subs_w_head)                                   /*채널 대기열에서 이 하위 요청을 제거합니다.*/
    {
        if (ssd->channel_head[channel].subs_w_head!=ssd->channel_head[channel].subs_w_tail)
        {
            ssd->channel_head[channel].subs_w_head=sub->next_node;
        } 
        else
        {
            ssd->channel_head[channel].subs_w_head=NULL;
            ssd->channel_head[channel].subs_w_tail=NULL;
        }
    }
    else
    {
        p=ssd->channel_head[channel].subs_w_head;
        while(p->next_node !=sub)
        {
            p=p->next_node;
        }

        if (sub->next_node!=NULL)
        {
            p->next_node=sub->next_node;
        } 
        else
        {
            p->next_node=NULL;
            ssd->channel_head[channel].subs_w_tail=p;
        }
    }

    return SUCCESS;	
}

/*
 *기능의 기능은 copyback 명령의 기능을 실행하는 것이며,
 */
Status copy_back(struct ssd_info * ssd, unsigned int channel, unsigned int chip, unsigned int die,struct sub_request * sub)
{
    int old_ppn=-1, new_ppn=-1;
    long long time=0;
    if (ssd->parameter->greed_CB_ad==1)                                               /*카피백 고급 명령의 탐욕적인 사용 허용*/
    {
        old_ppn=-1;
        if (ssd->dram->map->map_entry[sub->lpn].state!=0)                             /*이것은 이 논리적 페이지가 이전에 작성되었음을 의미합니다. copyback+random input 명령을 사용해야 합니다. 그렇지 않으면 직접 작성할 수 있습니다.*/
        {
            if ((sub->state&ssd->dram->map->map_entry[sub->lpn].state)==ssd->dram->map->map_entry[sub->lpn].state)       
            {
                sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;	
            } 
            else
            {
                sub->next_state_predict_time=ssd->current_time+19*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                ssd->copy_back_count++;
                ssd->read_count++;
                ssd->update_read_count++;
                old_ppn=ssd->dram->map->map_entry[sub->lpn].pn;                       /*동일한 주소가 카피백 중에 홀수인지 짝수인지 판별하는 데 사용되는 원본 물리적 페이지를 기록합니다.*/
            }															
        } 
        else
        {
            sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        }
        sub->complete_time=sub->next_state_predict_time;		
        time=sub->complete_time;

        get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);

        if (old_ppn!=-1)                                                              /*카피백 연산을 사용하는 경우에는 패리티 주소 제한이 충족되는지 여부를 판별해야 합니다.*/
        {
            new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
            while (old_ppn%2!=new_ppn%2)                                              /*패리티 주소 제한이 충족되지 않았습니다. 다른 페이지를 찾아야 합니다.*/
            {
                get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
                ssd->program_count--;
                ssd->write_flash_count--;
                ssd->waste_page_count++;
                new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
            }
        }
    } 
    else                                                                              /*카피백 고급 명령을 탐욕스럽게 사용하지 마십시오.*/
    {
        if (ssd->dram->map->map_entry[sub->lpn].state!=0)
        {
            if ((sub->state&ssd->dram->map->map_entry[sub->lpn].state)==ssd->dram->map->map_entry[sub->lpn].state)        
            {
                sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
            } 
            else
            {
                old_ppn=ssd->dram->map->map_entry[sub->lpn].pn;                       /*동일한 주소가 카피백 중에 홀수인지 짝수인지 판별하는 데 사용되는 원본 물리적 페이지를 기록합니다.*/
                get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
                new_ppn=ssd->dram->map->map_entry[sub->lpn].pn;
                if (old_ppn%2==new_ppn%2)
                {
                    ssd->copy_back_count++;
                    sub->next_state_predict_time=ssd->current_time+19*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                } 
                else
                {
                    sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(size(ssd->dram->map->map_entry[sub->lpn].state))*ssd->parameter->time_characteristics.tRC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                }
                ssd->read_count++;
                ssd->update_read_count++;
            }
        } 
        else
        {
            sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
            get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);
        }
        sub->complete_time=sub->next_state_predict_time;		
        time=sub->complete_time;
    }

    /****************************************************************
     *copyback 고급 명령을 실행할 때 채널, 칩 상태, 시간 등을 수정해야 합니다.
     *****************************************************************/
    ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
    ssd->channel_head[channel].current_time=ssd->current_time;										
    ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
    ssd->channel_head[channel].next_state_predict_time=time;

    ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
    ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
    ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
    ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG;

    return SUCCESS;
}

/*****************
 *정적 쓰기 작업 구현
 ******************/
Status static_write(struct ssd_info * ssd, unsigned int channel,unsigned int chip, unsigned int die,struct sub_request * sub)
{
    long long time=0;
    if (ssd->dram->map->map_entry[sub->lpn].state!=0)                                    /*이것은 이 논리적 페이지가 이전에 작성된 적이 있다는 것을 의미합니다. 이를 사용하여 먼저 읽은 다음 기록해야 합니다. 그렇지 않으면 직접 기록할 수 있습니다.*/
    {
        if ((sub->state&ssd->dram->map->map_entry[sub->lpn].state)==ssd->dram->map->map_entry[sub->lpn].state)   /*덮을 수 있다*/
        {
            sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        } 
        else
        {
            sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(size((ssd->dram->map->map_entry[sub->lpn].state^sub->state)))*ssd->parameter->time_characteristics.tRC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
            ssd->read_count++;
            ssd->update_read_count++;
        }
    } 
    else
    {
        sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
    }
    sub->complete_time=sub->next_state_predict_time;		
    time=sub->complete_time;

    get_ppn(ssd,sub->location->channel,sub->location->chip,sub->location->die,sub->location->plane,sub);

    /****************************************************************
     *copyback 고급 명령을 실행할 때 채널, 칩 상태, 시간 등을 수정해야 합니다.
     *****************************************************************/
    ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
    ssd->channel_head[channel].current_time=ssd->current_time;										
    ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
    ssd->channel_head[channel].next_state_predict_time=time;

    ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
    ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
    ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
    ssd->channel_head[channel].chip_head[chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG;

    return SUCCESS;
}

/********************
  하위 요청에 대한 처리기 작성
 *********************/
Status services_2_write(struct ssd_info * ssd,unsigned int channel,unsigned int * channel_busy_flag, unsigned int * change_current_time_flag)
{
    int j=0,chip=0;
    unsigned int k=0;
    unsigned int  old_ppn=0,new_ppn=0;
    unsigned int chip_token=0,die_token=0,plane_token=0,address_ppn=0;
    unsigned int  die=0,plane=0;
    long long time=0;
    struct sub_request * sub=NULL, * p=NULL;
    struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
    struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;

    /************************************************************************************************************************
     *쓰기 하위 요청은 두 곳에서 중단됩니다. 하나는 channel_head[channel].subs_w_head이고 다른 하나는 ssd->subs_w_head입니다. 따라서 최소한 하나의 대기열이 비어 있지 않은지 확인하십시오.
     * 동시에 부요청 처리도 동적할당과 정적할당으로 나뉜다.
     *************************************************************************************************************************/
    if((ssd->channel_head[channel].subs_w_head!=NULL)||(ssd->subs_w_head!=NULL))      
    {
        if (ssd->parameter->allocation_scheme==0)                                       /*동적 할당*/
        {
            for(j=0;j<ssd->channel_head[channel].chip;j++)					
            {		
                if((ssd->channel_head[channel].subs_w_head==NULL)&&(ssd->subs_w_head==NULL)) 
                {
                    break;
                }

                chip_token=ssd->channel_head[channel].token;                            /*토큰*/
                if (*channel_busy_flag==0)
                {
                    if((ssd->channel_head[channel].chip_head[chip_token].current_state==CHIP_IDLE)||((ssd->channel_head[channel].chip_head[chip_token].next_state==CHIP_IDLE)&&(ssd->channel_head[channel].chip_head[chip_token].next_state_predict_time<=ssd->current_time)))				
                    {
                        if((ssd->channel_head[channel].subs_w_head==NULL)&&(ssd->subs_w_head==NULL)) 
                        {
                            break;
                        }
                        die_token=ssd->channel_head[channel].chip_head[chip_token].token;	
                        if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))       //can't use advanced commands
                        {
                            sub=find_write_sub_request(ssd,channel);
                            if(sub==NULL)
                            {
                                break;
                            }

                            if(sub->current_state==SR_WAIT)
                            {
                                plane_token=ssd->channel_head[channel].chip_head[chip_token].die_head[die_token].token;

                                get_ppn(ssd,channel,chip_token,die_token,plane_token,sub);

                                ssd->channel_head[channel].chip_head[chip_token].die_head[die_token].token=(ssd->channel_head[channel].chip_head[chip_token].die_head[die_token].token+1)%ssd->parameter->plane_die;

                                *change_current_time_flag=0;

                                if(ssd->parameter->ad_priority2==0)
                                {
                                    ssd->real_time_subreq--;
                                }
                                go_one_step(ssd,sub,NULL,SR_W_TRANSFER,NORMAL);       /*정상 상태 전환을 수행합니다.*/
                                delete_w_sub_request(ssd,channel,sub);                /*처리된 쓰기 하위 요청 삭제*/

                                *channel_busy_flag=1;
                                /**************************************************************************
                                 *for 루프를 벗어나기 전에 토큰을 수정하십시오.
                                 *여기서 토큰의 변경은 이 채널 칩 다이 플레인 아래의 쓰기 성공 여부에 전적으로 달려 있습니다.
                                 *성공하면 중단하고, 그렇지 않으면 성공적으로 쓸 수 있는 채널 칩 다이 평면을 찾을 때까지 토큰이 변경됩니다.
                                 ***************************************************************************/
                                ssd->channel_head[channel].chip_head[chip_token].token=(ssd->channel_head[channel].chip_head[chip_token].token+1)%ssd->parameter->die_chip;
                                ssd->channel_head[channel].token=(ssd->channel_head[channel].token+1)%ssd->parameter->chip_channel[channel];
                                break;
                            }
                        } 
                        else                                                          /*use advanced commands*/
                        {
                            if (dynamic_advanced_process(ssd,channel,chip_token)==NULL)
                            {
                                *channel_busy_flag=0;
                            }
                            else
                            {
                                *channel_busy_flag=1;                                 /*요청이 실행되고 데이터가 전송되고 버스가 점유되고 다음 채널로 점프해야 합니다.*/
                                ssd->channel_head[channel].chip_head[chip_token].token=(ssd->channel_head[channel].chip_head[chip_token].token+1)%ssd->parameter->die_chip;
                                ssd->channel_head[channel].token=(ssd->channel_head[channel].token+1)%ssd->parameter->chip_channel[channel];
                                break;
                            }
                        }	

                        ssd->channel_head[channel].chip_head[chip_token].token=(ssd->channel_head[channel].chip_head[chip_token].token+1)%ssd->parameter->die_chip;
                    }
                }

                ssd->channel_head[channel].token=(ssd->channel_head[channel].token+1)%ssd->parameter->chip_channel[channel];
            }
        } 
        else if(ssd->parameter->allocation_scheme==1)                                     /*정적 할당*/
        {
            for(chip=0;chip<ssd->channel_head[channel].chip;chip++)					
            {	
                if((ssd->channel_head[channel].chip_head[chip].current_state==CHIP_IDLE)||((ssd->channel_head[channel].chip_head[chip].next_state==CHIP_IDLE)&&(ssd->channel_head[channel].chip_head[chip].next_state_predict_time<=ssd->current_time)))				
                {		
                    if(ssd->channel_head[channel].subs_w_head==NULL)
                    {
                        break;
                    }
                    if (*channel_busy_flag==0)
                    {

                        if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))     /*고급 명령을 실행하지 마십시오.*/
                        {
                            for(die=0;die<ssd->channel_head[channel].chip_head[chip].die_num;die++)				
                            {	
                                if(ssd->channel_head[channel].subs_w_head==NULL)
                                {
                                    break;
                                }
                                sub=ssd->channel_head[channel].subs_w_head;
                                while (sub!=NULL)
                                {
                                    if ((sub->current_state==SR_WAIT)&&(sub->location->channel==channel)&&(sub->location->chip==chip)&&(sub->location->die==die))      /*이 하위 요청은 현재 die의 요청입니다.*/
                                    {
                                        break;
                                    }
                                    sub=sub->next_node;
                                }
                                if (sub==NULL)
                                {
                                    continue;
                                }

                                if(sub->current_state==SR_WAIT)
                                {
                                    sub->current_time=ssd->current_time;
                                    sub->current_state=SR_W_TRANSFER;
                                    sub->next_state=SR_COMPLETE;

                                    if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
                                    {
                                        copy_back(ssd, channel,chip, die,sub);      /*copyback 고급 명령을 실행할 수 있으면 copy_back(ssd, channel, chip, die, sub) 기능을 사용하여 쓰기 하위 요청을 처리합니다.*/
                                        *change_current_time_flag=0;
                                    } 
                                    else
                                    {
                                        static_write(ssd, channel,chip, die,sub);   /*copyback 고급 명령을 실행할 수 없는 경우 static_write(ssd, channel, chip, die, sub) 함수를 사용하여 쓰기 하위 요청을 처리합니다.*/ 
                                        *change_current_time_flag=0;
                                    }

                                    delete_w_sub_request(ssd,channel,sub);
                                    *channel_busy_flag=1;
                                    break;
                                }
                            }
                        } 
                        else                                                        /*고급 명령을 처리할 수 없음*/
                        {
                            if (dynamic_advanced_process(ssd,channel,chip)==NULL)
                            {
                                *channel_busy_flag=0;
                            }
                            else
                            {
                                *channel_busy_flag=1;                               /*요청이 실행되고 데이터가 전송되고 버스가 점유되고 다음 채널로 점프해야 합니다.*/
                                break;
                            }
                        }	

                    }
                }		
            }
        }			
    }
    return SUCCESS;	
}


/********************************************************
 *이 함수의 주요 기능은 읽기 하위 요청 및 쓰기 하위 요청의 상태 변경 처리를 제어하는 ​​것입니다.
 *********************************************************/

struct ssd_info *process(struct ssd_info *ssd)   
{

    /*********************************************************************************************************
     *flag_die는 다이의 혼잡으로 인해 시간이 차단되었는지 여부를 나타내며, -1은 아니오를 의미하고, non-1은 차단이 있음을 의미하고,
     *flag_die의 값은 다이 번호를 나타내며 이전 ppn은 카피백이 패리티 주소 제한을 준수하는지 여부를 판단하는 데 사용되는 카피백 이전의 물리적 페이지 번호를 기록합니다.
     *two_plane_bit[8], two_plane_place[8] 배열 멤버는 동일한 채널에서 각 다이의 요청 할당을 나타냅니다.
     *chg_cur_time_flag는 현재 시간 조정 여부에 대한 플래그로 채널이 사용 중이어서 요청이 차단되면 현재 시간을 조정해야 합니다.
     * 처음에는 조정이 필요하다고 생각하고 1로 설정하고 채널이 전송 명령 또는 데이터를 처리할 때 이 값은 0으로 설정되어 조정이 필요하지 않음을 나타냅니다.
     **********************************************************************************************************/
    int old_ppn=-1,flag_die=-1; 
    unsigned int i,chan,random_num;     
    unsigned int flag=0,new_write=0,chg_cur_time_flag=1,flag2=0,flag_gc=0;       
    int64_t time, channel_time=MAX_INT64;
    struct sub_request *sub;          

#ifdef DEBUG
    printf("enter process,  current time:%lld\n",ssd->current_time);
#endif

    /*********************************************************
     *읽기/쓰기 하위 요청이 있는지 확인하고 있으면 플래그가 0으로 설정되고 플래그가 없으면 1로 설정됩니다.
     *플래그가 1일 때 ssd에 gc 연산이 있으면 이때 gc 연산을 수행할 수 있음
     **********************************************************/
    for(i=0;i<ssd->parameter->channel_number;i++)
    {          
        if((ssd->channel_head[i].subs_r_head==NULL)&&(ssd->channel_head[i].subs_w_head==NULL)&&(ssd->subs_w_head==NULL))
        {
            flag=1;
        }
        else
        {
            flag=0;
            break;
        }
    }
    if(flag==1)
    {
        ssd->flag=1;                                                                
        if (ssd->gc_request>0)                                                            /*SSD에 gc 작업 요청이 있습니다.*/
        {
            gc(ssd,0,1);                                                                  /*이 gc는 모든 채널이*/
        }
        return ssd;
    }
    else
    {
        ssd->flag=0;
    }

    time = ssd->current_time;
    services_2_r_cmd_trans_and_complete(ssd);                                            /*프로세스 현재 상태가 SR_R_C_A_TRANSFER이거나 현재 상태가 SR_COMPLETE이거나 다음 상태가 SR_COMPLETE이고 다음 상태 예상 시간이 현재 상태 시간보다 짧습니다.*/

    random_num=ssd->program_count%ssd->parameter->channel_number;                        /*각 쿼리가 다른 채널에서 시작되도록 난수 생성*/

    /*****************************************
     *모든 채널에서 읽기 및 쓰기 하위 요청을 주기적으로 처리하여 읽기 요청 명령을 보내고 읽기 및 쓰기 데이터를 전송하려면 모두 버스를 점유해야 합니다.
     ******************************************/
    for(chan=0;chan<ssd->parameter->channel_number;chan++)	     
    {
        i=(random_num+chan)%ssd->parameter->channel_number;
        flag=0;
        flag_gc=0;                                                                       /*채널에 들어갈 때마다 gc 플래그 위치를 0으로 설정하고 기본값은 gc 작업이 수행되지 않는 것입니다.*/
        if((ssd->channel_head[i].current_state==CHANNEL_IDLE)||(ssd->channel_head[i].next_state==CHANNEL_IDLE&&ssd->channel_head[i].next_state_predict_time<=ssd->current_time))		
        {   
            if (ssd->gc_request>0)                                                       /*특정 판단이 필요한 gc 작업이 있습니다.*/
            {
                if (ssd->channel_head[i].gc_command!=NULL)
                {
                    flag_gc=gc(ssd,i,0);                                                 /*gc 함수는 gc 연산 수행 여부를 나타내는 값을 반환하며, gc 연산이 수행되면 현재 채널에서 다른 요청을 처리할 수 없습니다.*/
                }
                if (flag_gc==1)                                                          /*gc 작업을 수행한 후 이 루프에서 점프해야 합니다.*/
                {
                    continue;
                }
            }

            sub=ssd->channel_head[i].subs_r_head;                                        /*먼저 읽기 요청 처리*/
            services_2_r_wait(ssd,i,&flag,&chg_cur_time_flag);                           /*보류 중인 읽기 하위 요청 처리*/

            if((flag==0)&&(ssd->channel_head[i].subs_r_head!=NULL))                      /*if there are no new read request and data is ready in some dies, send these data to controller and response this request*/		
            {		     
                services_2_r_data_trans(ssd,i,&flag,&chg_cur_time_flag);                    

            }
            if(flag==0)                                                                  /*if there are no read request to take channel, we can serve write requests*/ 		
            {	
                services_2_write(ssd,i,&flag,&chg_cur_time_flag);

            }	
        }	
    }

    return ssd;
}

/****************************************************************************************************************************
 *ssd가 고급 명령을 지원하는 경우 이 기능의 기능은 고급 명령의 쓰기 하위 요청을 처리하는 것입니다.
 *요청 수에 따라 어떤 고급 명령을 선택할지 결정합니다(이 기능은 쓰기 요청만 처리하고 읽기 요청은 각 채널에 할당되므로 실행 사이에 해당 명령이 선택됨)
 *****************************************************************************************************************************/
struct ssd_info *dynamic_advanced_process(struct ssd_info *ssd,unsigned int channel,unsigned int chip)         
{
    unsigned int die=0,plane=0;
    unsigned int subs_count=0;
    int flag;
    unsigned int gate;                                                                    /*record the max subrequest that can be executed in the same channel. it will be used when channel-level priority order is highest and allocation scheme is full dynamic allocation*/
    unsigned int plane_place;                                                             /*record which plane has sub request in static allocation*/
    struct sub_request *sub=NULL,*p=NULL,*sub0=NULL,*sub1=NULL,*sub2=NULL,*sub3=NULL,*sub0_rw=NULL,*sub1_rw=NULL,*sub2_rw=NULL,*sub3_rw=NULL;
    struct sub_request ** subs=NULL;
    unsigned int max_sub_num=0;
    unsigned int die_token=0,plane_token=0;
    unsigned int * plane_bits=NULL;
    unsigned int interleaver_count=0;

    unsigned int mask=0x00000001;
    unsigned int i=0,j=0;

    max_sub_num=(ssd->parameter->die_chip)*(ssd->parameter->plane_die);
    gate=max_sub_num;
    subs=(struct sub_request **)malloc(max_sub_num*sizeof(struct sub_request *));
    alloc_assert(subs,"sub_request");

    for(i=0;i<max_sub_num;i++)
    {
        subs[i]=NULL;
    }

    if((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0)&&(ssd->parameter->ad_priority2==0))
    {
        gate=ssd->real_time_subreq/ssd->parameter->channel_number;

        if(gate==0)
        {
            gate=1;
        }
        else
        {
            if(ssd->real_time_subreq%ssd->parameter->channel_number!=0)
            {
                gate++;
            }
        }
    }

    if ((ssd->parameter->allocation_scheme==0))                                           /*완전 동적 할당, ssd->subs_w_head에서 제공 대기 중인 하위 요청을 선택해야 합니다.*/
    {
        if(ssd->parameter->dynamic_allocation==0)
        {
            sub=ssd->subs_w_head;
        }
        else
        {
            sub=ssd->channel_head[channel].subs_w_head;
        }

        subs_count=0;

        while ((sub!=NULL)&&(subs_count<max_sub_num)&&(subs_count<gate))
        {
            if(sub->current_state==SR_WAIT)								
            {
                if ((sub->update==NULL)||((sub->update!=NULL)&&((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))))    //没有需要提前读出的页
                {
                    subs[subs_count]=sub;
                    subs_count++;
                }						
            }

            p=sub;
            sub=sub->next_node;	
        }

        if (subs_count==0)                                                               /*요청을 처리할 수 없습니다. NULL을 반환합니다.*/
        {
            for(i=0;i<max_sub_num;i++)
            {
                subs[i]=NULL;
            }
            free(subs);

            subs=NULL;
            free(plane_bits);
            return NULL;
        }
        if(subs_count>=2)
        {
            /*********************************************
             *평면과 인터리브를 모두 사용할 수 있습니다.
             *이 채널에서 실행할 interleave_two_plane을 선택하십시오.
             **********************************************/
            if (((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))     
            {                                                                        
                get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE_TWO_PLANE); 
            }
            else if (((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE))
            {
                if(subs_count>ssd->parameter->plane_die)
                {	
                    for(i=ssd->parameter->plane_die;i<subs_count;i++)
                    {
                        subs[i]=NULL;
                    }
                    subs_count=ssd->parameter->plane_die;
                }
                get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,TWO_PLANE);
            }
            else if (((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE)&&((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE))
            {

                if(subs_count>ssd->parameter->die_chip)
                {	
                    for(i=ssd->parameter->die_chip;i<subs_count;i++)
                    {
                        subs[i]=NULL;
                    }
                    subs_count=ssd->parameter->die_chip;
                }
                get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE);
            }
            else
            {
                for(i=1;i<subs_count;i++)
                {
                    subs[i]=NULL;
                }
                subs_count=1;
                get_ppn_for_normal_command(ssd,channel,chip,subs[0]);
            }

        }//if(subs_count>=2)
        else if(subs_count==1)     //only one request
        {
            get_ppn_for_normal_command(ssd,channel,chip,subs[0]);
        }

    }//if ((ssd->parameter->allocation_scheme==0)) 
    else                                                                                  /*정적 할당 방법, 이 특정 채널에서 제공되기를 기다리는 하위 요청을 선택하기만 하면 됩니다.*/
    {
        /*정적 할당 방법에서 사용할 명령은 채널에 대한 요청이 동일한 다이에 속하는 동일한 다이의 plane에 따라 결정됩니다.*/

        sub=ssd->channel_head[channel].subs_w_head;
        plane_bits=(unsigned int * )malloc((ssd->parameter->die_chip)*sizeof(unsigned int));
        alloc_assert(plane_bits,"plane_bits");
        memset(plane_bits,0, (ssd->parameter->die_chip)*sizeof(unsigned int));

        for(i=0;i<ssd->parameter->die_chip;i++)
        {
            plane_bits[i]=0x00000000;
        }
        subs_count=0;

        while ((sub!=NULL)&&(subs_count<max_sub_num))
        {
            if(sub->current_state==SR_WAIT)								
            {
                if ((sub->update==NULL)||((sub->update!=NULL)&&((sub->update->current_state==SR_COMPLETE)||((sub->update->next_state==SR_COMPLETE)&&(sub->update->next_state_predict_time<=ssd->current_time)))))
                {
                    if (sub->location->chip==chip)
                    {
                        plane_place=0x00000001<<(sub->location->plane);

                        if ((plane_bits[sub->location->die]&plane_place)!=plane_place)      //we have not add sub request to this plane
                        {
                            subs[sub->location->die*ssd->parameter->plane_die+sub->location->plane]=sub;
                            subs_count++;
                            plane_bits[sub->location->die]=(plane_bits[sub->location->die]|plane_place);
                        }
                    }
                }						
            }
            sub=sub->next_node;	
        }//while ((sub!=NULL)&&(subs_count<max_sub_num))

        if (subs_count==0)                                                            /*요청을 처리할 수 없습니다. NULL을 반환합니다.*/
        {
            for(i=0;i<max_sub_num;i++)
            {
                subs[i]=NULL;
            }
            free(subs);
            subs=NULL;
            free(plane_bits);
            return NULL;
        }

        flag=0;
        if (ssd->parameter->advanced_commands!=0)
        {
            if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)        /*모든 고급 명령을 사용할 수 있습니다.*/
            {
                if (subs_count>1)                                                    /*직접 처리할 수 있는 쓰기 요청이 1개 이상 있습니다.*/
                {
                    get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,COPY_BACK);
                } 
                else
                {
                    for(i=0;i<max_sub_num;i++)
                    {
                        if(subs[i]!=NULL)
                        {
                            break;
                        }
                    }
                    get_ppn_for_normal_command(ssd,channel,chip,subs[i]);
                }

            }// if ((ssd->parameter->advanced_commands&AD_COPYBACK)==AD_COPYBACK)
            else                                                                     /*카피백을 실행할 수 없습니다*/
            {
                if (subs_count>1)                                                    /*직접 처리할 수 있는 쓰기 요청이 1개 이상 있습니다.*/
                {
                    if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
                    {
                        get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE_TWO_PLANE);
                    } 
                    else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))
                    {
                        for(die=0;die<ssd->parameter->die_chip;die++)
                        {
                            if(plane_bits[die]!=0x00000000)
                            {
                                for(i=0;i<ssd->parameter->plane_die;i++)
                                {
                                    plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
                                    ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane_token+1)%ssd->parameter->plane_die;
                                    mask=0x00000001<<plane_token;
                                    if((plane_bits[die]&mask)==mask)
                                    {
                                        plane_bits[die]=mask;
                                        break;
                                    }
                                }
                                for(i=i+1;i<ssd->parameter->plane_die;i++)
                                {
                                    plane=(plane_token+1)%ssd->parameter->plane_die;
                                    subs[die*ssd->parameter->plane_die+plane]=NULL;
                                    subs_count--;
                                }
                                interleaver_count++;
                            }//if(plane_bits[die]!=0x00000000)
                        }//for(die=0;die<ssd->parameter->die_chip;die++)
                        if(interleaver_count>=2)
                        {
                            get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,INTERLEAVE);
                        }
                        else
                        {
                            for(i=0;i<max_sub_num;i++)
                            {
                                if(subs[i]!=NULL)
                                {
                                    break;
                                }
                            }
                            get_ppn_for_normal_command(ssd,channel,chip,subs[i]);	
                        }
                    }//else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)==AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)!=AD_TWOPLANE))
                    else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
                    {
                        for(i=0;i<ssd->parameter->die_chip;i++)
                        {
                            die_token=ssd->channel_head[channel].chip_head[chip].token;
                            ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
                            if(size(plane_bits[die_token])>1)
                            {
                                break;
                            }

                        }

                        if(i<ssd->parameter->die_chip)
                        {
                            for(die=0;die<ssd->parameter->die_chip;die++)
                            {
                                if(die!=die_token)
                                {
                                    for(plane=0;plane<ssd->parameter->plane_die;plane++)
                                    {
                                        if(subs[die*ssd->parameter->plane_die+plane]!=NULL)
                                        {
                                            subs[die*ssd->parameter->plane_die+plane]=NULL;
                                            subs_count--;
                                        }
                                    }
                                }
                            }
                            get_ppn_for_advanced_commands(ssd,channel,chip,subs,subs_count,TWO_PLANE);
                        }//if(i<ssd->parameter->die_chip)
                        else
                        {
                            for(i=0;i<ssd->parameter->die_chip;i++)
                            {
                                die_token=ssd->channel_head[channel].chip_head[chip].token;
                                ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
                                if(plane_bits[die_token]!=0x00000000)
                                {
                                    for(j=0;j<ssd->parameter->plane_die;j++)
                                    {
                                        plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die_token].token;
                                        ssd->channel_head[channel].chip_head[chip].die_head[die_token].token=(plane_token+1)%ssd->parameter->plane_die;
                                        if(((plane_bits[die_token])&(0x00000001<<plane_token))!=0x00000000)
                                        {
                                            sub=subs[die_token*ssd->parameter->plane_die+plane_token];
                                            break;
                                        }
                                    }
                                }
                            }//for(i=0;i<ssd->parameter->die_chip;i++)
                            get_ppn_for_normal_command(ssd,channel,chip,sub);
                        }//else
                    }//else if (((ssd->parameter->advanced_commands&AD_INTERLEAVE)!=AD_INTERLEAVE)&&((ssd->parameter->advanced_commands&AD_TWOPLANE)==AD_TWOPLANE))
                }//if (subs_count>1)  
                else
                {
                    for(i=0;i<ssd->parameter->die_chip;i++)
                    {
                        die_token=ssd->channel_head[channel].chip_head[chip].token;
                        ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
                        if(plane_bits[die_token]!=0x00000000)
                        {
                            for(j=0;j<ssd->parameter->plane_die;j++)
                            {
                                plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die_token].token;
                                ssd->channel_head[channel].chip_head[chip].die_head[die_token].token=(plane_token+1)%ssd->parameter->plane_die;
                                if(((plane_bits[die_token])&(0x00000001<<plane_token))!=0x00000000)
                                {
                                    sub=subs[die_token*ssd->parameter->plane_die+plane_token];
                                    break;
                                }
                            }
                            if(sub!=NULL)
                            {
                                break;
                            }
                        }
                    }//for(i=0;i<ssd->parameter->die_chip;i++)
                    get_ppn_for_normal_command(ssd,channel,chip,sub);
                }//else
            }
        }//if (ssd->parameter->advanced_commands!=0)
        else
        {
            for(i=0;i<ssd->parameter->die_chip;i++)
            {
                die_token=ssd->channel_head[channel].chip_head[chip].token;
                ssd->channel_head[channel].chip_head[chip].token=(die_token+1)%ssd->parameter->die_chip;
                if(plane_bits[die_token]!=0x00000000)
                {
                    for(j=0;j<ssd->parameter->plane_die;j++)
                    {
                        plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die_token].token;
                        ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane_token+1)%ssd->parameter->plane_die;
                        if(((plane_bits[die_token])&(0x00000001<<plane_token))!=0x00000000)
                        {
                            sub=subs[die_token*ssd->parameter->plane_die+plane_token];
                            break;
                        }
                    }
                    if(sub!=NULL)
                    {
                        break;
                    }
                }
            }//for(i=0;i<ssd->parameter->die_chip;i++)
            get_ppn_for_normal_command(ssd,channel,chip,sub);
        }//else

    }//else

    for(i=0;i<max_sub_num;i++)
    {
        subs[i]=NULL;
    }
    free(subs);
    subs=NULL;
    free(plane_bits);
    return ssd;
}

/****************************************
 *쓰기 하위 요청을 실행할 때 일반 쓰기 하위 요청에 대해 ppn 가져오기
 *****************************************/
Status get_ppn_for_normal_command(struct ssd_info * ssd, unsigned int channel,unsigned int chip, struct sub_request * sub)
{
    unsigned int die=0;
    unsigned int plane=0;
    if(sub==NULL)
    {
        return ERROR;
    }

    if (ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)
    {
        die=ssd->channel_head[channel].chip_head[chip].token;
        plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
        get_ppn(ssd,channel,chip,die,plane,sub);
        ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
        ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;

        compute_serve_time(ssd,channel,chip,die,&sub,1,NORMAL);
        return SUCCESS;
    }
    else
    {
        die=sub->location->die;
        plane=sub->location->plane;
        get_ppn(ssd,channel,chip,die,plane,sub);   
        compute_serve_time(ssd,channel,chip,die,&sub,1,NORMAL);
        return SUCCESS;
    }

}



/************************************************************************************************
 *为高级命令获取ppn
 *根据不同的命令，遵从在同一个block中顺序写的要求，选取可以进行写操作的ppn，跳过的ppn全部置为失效。
 *在使用two plane操作时，为了寻找相同水平位置的页，可能需要直接找到两个完全空白的块，这个时候原来
 *的块没有用完，只能放在这，等待下次使用，同时修改查找空白page的方法，将以前首先寻找free块改为，只
 *要invalid block!=64即可。
 *except find aim page, we should modify token and decide gc operation

 *고급 명령에 대한 ppn 가져오기
 * 다른 명령에 따라 동일한 블록에 순차적 쓰기 요구 사항에 따라 쓸 수 있는 ppn을 선택하고 건너뛴 모든 ppn을 무효로 설정합니다.
 *2평면 연산을 사용할 때 수평 위치가 같은 페이지를 찾기 위해서는 완전히 비어 있는 두 개의 블록을 직접 찾아야 할 수도 있습니다.
 *블록은 사용되지 않았으며 여기에만 배치할 수 있으며 다음 사용을 기다리고 빈 페이지를 찾는 방법을 수정하고 빈 블록에 대한 이전 검색을 먼저 다음으로 변경합니다.
 * invalid block != 64 조건이 충족되어야 함
 * 목표 페이지 찾기를 제외하고 토큰을 수정하고 gc 작업을 결정해야 합니다.
 *************************************************************************************************/
Status get_ppn_for_advanced_commands(struct ssd_info *ssd,unsigned int channel,unsigned int chip,struct sub_request * * subs ,unsigned int subs_count,unsigned int command)      
{
    unsigned int die=0,plane=0;
    unsigned int die_token=0,plane_token=0;
    struct sub_request * sub=NULL;
    unsigned int i=0,j=0,k=0;
    unsigned int unvalid_subs_count=0;
    unsigned int valid_subs_count=0;
    unsigned int interleave_flag=FALSE;
    unsigned int multi_plane_falg=FALSE;
    unsigned int max_subs_num=0;
    struct sub_request * first_sub_in_chip=NULL;
    struct sub_request * first_sub_in_die=NULL;
    struct sub_request * second_sub_in_die=NULL;
    unsigned int state=SUCCESS;
    unsigned int multi_plane_flag=FALSE;

    max_subs_num=ssd->parameter->die_chip*ssd->parameter->plane_die;

    if (ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)                         /*동적 할당 작업*/ 
    {
        if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))                      /*INTERLEAVE_TWO_PLANE 및 COPY_BACK*/
        {
            for(i=0;i<subs_count;i++)
            {
                die=ssd->channel_head[channel].chip_head[chip].token;
                if(i<ssd->parameter->die_chip)                                         /*각 subs[i]에 대해 ppn을 가져옵니다. i는 die_chip보다 작습니다.*/
                {
                    plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
                    get_ppn(ssd,channel,chip,die,plane,subs[i]);
                    ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
                }
                else                                                                  
                {   
                    /*********************************************************************************************************************************
                     *die_chip을 넘어 i가 가리키는 subs[i]와 subs[i%ssd->parameter->die_chip]은 같은 위치의 ppn을 얻습니다.
                     *획득에 성공하면 multi_plane_flag=TRUE로 설정하고 compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE_TWO_PLANE)을 실행합니다.
                     *그렇지 않으면 compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE)을 실행합니다.
                     ***********************************************************************************************************************************/
                    state=make_level_page(ssd,subs[i%ssd->parameter->die_chip],subs[i]);
                    if(state!=SUCCESS)                                                 
                    {
                        subs[i]=NULL;
                        unvalid_subs_count++;
                    }
                    else
                    {
                        multi_plane_flag=TRUE;
                    }
                }
                ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
            }
            valid_subs_count=subs_count-unvalid_subs_count;
            ssd->interleave_count++;
            if(multi_plane_flag==TRUE)
            {
                ssd->inter_mplane_count++;
                compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE_TWO_PLANE);/*쓰기 하위 요청의 상태 전환에 대한 쓰기 하위 요청의 처리 시간을 계산합니다.*/		
            }
            else
            {
                compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
            }
            return SUCCESS;
        }//if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
        else if(command==INTERLEAVE)
        {
            /***********************************************************************************************
             *TWO_PLANE 고급 명령의 처리보다 간단한 INTERLEAVE 고급 명령의 처리
             *two_plane의 요구 사항은 동일한 다이에서 다른 평면의 동일한 위치에 있는 페이지이고 인터리브의 요구 사항은 다른 다이에 있기 때문입니다.
             ************************************************************************************************/
            for(i=0;(i<subs_count)&&(i<ssd->parameter->die_chip);i++)
            {
                die=ssd->channel_head[channel].chip_head[chip].token;
                plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
                get_ppn(ssd,channel,chip,die,plane,subs[i]);
                ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane+1)%ssd->parameter->plane_die;
                ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
                valid_subs_count++;
            }
            ssd->interleave_count++;
            compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
            return SUCCESS;
        }//else if(command==INTERLEAVE)
        else if(command==TWO_PLANE)
        {
            if(subs_count<2)
            {
                return ERROR;
            }
            die=ssd->channel_head[channel].chip_head[chip].token;
            for(j=0;j<subs_count;j++)
            {
                if(j==1)
                {
                    state=find_level_page(ssd,channel,chip,die,subs[0],subs[1]);        /*subs[0]과 ppn 위치가 같은 subs[1]을 찾아 TWO_PLANE 고급 명령을 실행합니다.*/
                    if(state!=SUCCESS)
                    {
                        get_ppn_for_normal_command(ssd,channel,chip,subs[0]);           /*찾을 수 없으면 일반 명령으로 처리하십시오.*/
                        return FAILURE;
                    }
                    else
                    {
                        valid_subs_count=2;
                    }
                }
                else if(j>1)
                {
                    state=make_level_page(ssd,subs[0],subs[j]);                         /*subs[0]과 ppn 위치가 같은 subs[j]를 찾아 TWO_PLANE 고급 명령을 실행합니다.*/
                    if(state!=SUCCESS)
                    {
                        for(k=j;k<subs_count;k++)
                        {
                            subs[k]=NULL;
                        }
                        subs_count=j;
                        break;
                    }
                    else
                    {
                        valid_subs_count++;
                    }
                }
            }//for(j=0;j<subs_count;j++)
            ssd->channel_head[channel].chip_head[chip].token=(die+1)%ssd->parameter->die_chip;
            ssd->m_plane_prog_count++;
            compute_serve_time(ssd,channel,chip,die,subs,valid_subs_count,TWO_PLANE);
            return SUCCESS;
        }//else if(command==TWO_PLANE)
        else 
        {
            return ERROR;
        }
    }//if (ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)
    else                                                                              /*정적 할당의 경우*/
    {
        if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
        {
            for(die=0;die<ssd->parameter->die_chip;die++)
            {
                first_sub_in_die=NULL;
                for(plane=0;plane<ssd->parameter->plane_die;plane++)
                {
                    sub=subs[die*ssd->parameter->plane_die+plane];
                    if(sub!=NULL)
                    {
                        if(first_sub_in_die==NULL)
                        {
                            first_sub_in_die=sub;
                            get_ppn(ssd,channel,chip,die,plane,sub);
                        }
                        else
                        {
                            state=make_level_page(ssd,first_sub_in_die,sub);
                            if(state!=SUCCESS)
                            {
                                subs[die*ssd->parameter->plane_die+plane]=NULL;
                                subs_count--;
                                sub=NULL;
                            }
                            else
                            {
                                multi_plane_flag=TRUE;
                            }
                        }
                    }
                }
            }
            if(multi_plane_flag==TRUE)
            {
                ssd->inter_mplane_count++;
                compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE_TWO_PLANE);
                return SUCCESS;
            }
            else
            {
                compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);
                return SUCCESS;
            }
        }//if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
        else if(command==INTERLEAVE)
        {
            for(die=0;die<ssd->parameter->die_chip;die++)
            {	
                first_sub_in_die=NULL;
                for(plane=0;plane<ssd->parameter->plane_die;plane++)
                {
                    sub=subs[die*ssd->parameter->plane_die+plane];
                    if(sub!=NULL)
                    {
                        if(first_sub_in_die==NULL)
                        {
                            first_sub_in_die=sub;
                            get_ppn(ssd,channel,chip,die,plane,sub);
                            valid_subs_count++;
                        }
                        else
                        {
                            subs[die*ssd->parameter->plane_die+plane]=NULL;
                            subs_count--;
                            sub=NULL;
                        }
                    }
                }
            }
            if(valid_subs_count>1)
            {
                ssd->interleave_count++;
            }
            compute_serve_time(ssd,channel,chip,0,subs,valid_subs_count,INTERLEAVE);	
        }//else if(command==INTERLEAVE)
        else if(command==TWO_PLANE)
        {
            for(die=0;die<ssd->parameter->die_chip;die++)
            {	
                first_sub_in_die=NULL;
                second_sub_in_die=NULL;
                for(plane=0;plane<ssd->parameter->plane_die;plane++)
                {
                    sub=subs[die*ssd->parameter->plane_die+plane];
                    if(sub!=NULL)
                    {	
                        if(first_sub_in_die==NULL)
                        {
                            first_sub_in_die=sub;
                        }
                        else if(second_sub_in_die==NULL)
                        {
                            second_sub_in_die=sub;
                            state=find_level_page(ssd,channel,chip,die,first_sub_in_die,second_sub_in_die);
                            if(state!=SUCCESS)
                            {
                                subs[die*ssd->parameter->plane_die+plane]=NULL;
                                subs_count--;
                                second_sub_in_die=NULL;
                                sub=NULL;
                            }
                            else
                            {
                                valid_subs_count=2;
                            }
                        }
                        else
                        {
                            state=make_level_page(ssd,first_sub_in_die,sub);
                            if(state!=SUCCESS)
                            {
                                subs[die*ssd->parameter->plane_die+plane]=NULL;
                                subs_count--;
                                sub=NULL;
                            }
                            else
                            {
                                valid_subs_count++;
                            }
                        }
                    }//if(sub!=NULL)
                }//for(plane=0;plane<ssd->parameter->plane_die;plane++)
                if(second_sub_in_die!=NULL)
                {
                    multi_plane_flag=TRUE;
                    break;
                }
            }//for(die=0;die<ssd->parameter->die_chip;die++)
            if(multi_plane_flag==TRUE)
            {
                ssd->m_plane_prog_count++;
                compute_serve_time(ssd,channel,chip,die,subs,valid_subs_count,TWO_PLANE);
                return SUCCESS;
            }//if(multi_plane_flag==TRUE)
            else
            {
                i=0;
                sub=NULL;
                while((sub==NULL)&&(i<max_subs_num))
                {
                    sub=subs[i];
                    i++;
                }
                if(sub!=NULL)
                {
                    get_ppn_for_normal_command(ssd,channel,chip,sub);
                    return FAILURE;
                }
                else 
                {
                    return ERROR;
                }
            }//else
        }//else if(command==TWO_PLANE)
        else
        {
            return ERROR;
        }
    }//elseb 静态分配的情况
}


/***********************************************
 *함수의 기능은 sub0과 sub1의 ppn이 위치한 페이지 위치를 동일하게 만드는 것입니다.
 ************************************************/
Status make_level_page(struct ssd_info * ssd, struct sub_request * sub0,struct sub_request * sub1)
{
    unsigned int i=0,j=0,k=0;
    unsigned int channel=0,chip=0,die=0,plane0=0,plane1=0,block0=0,block1=0,page0=0,page1=0;
    unsigned int active_block0=0,active_block1=0;
    unsigned int old_plane_token=0;

    if((sub0==NULL)||(sub1==NULL)||(sub0->location==NULL))
    {
        return ERROR;
    }
    channel=sub0->location->channel;
    chip=sub0->location->chip;
    die=sub0->location->die;
    plane0=sub0->location->plane;
    block0=sub0->location->block;
    page0=sub0->location->page;
    old_plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die].token;

    /***********************************************************************************************
     *动态分配的情况下
     *sub1的plane是根据sub0的ssd->channel_head[channel].chip_head[chip].die_head[die].token令牌获取的
     *sub1的channel，chip，die，block，page都和sub0的相同
     ************************************************************************************************/
    if(ssd->parameter->allocation_scheme==DYNAMIC_ALLOCATION)                             
    {
        old_plane_token=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
        for(i=0;i<ssd->parameter->plane_die;i++)
        {
            plane1=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
            if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].add_reg_ppn==-1)
            {
                find_active_block(ssd,channel,chip,die,plane1);                               /*plane1에서 활성 블록 찾기*/
                block1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].active_block;

                /*********************************************************************************************
                 * 발견된 block1이 block0과 동일한 경우에만 동일한 페이지를 계속 검색할 수 있습니다.
                 *페이지를 찾는 것은 비교적 간단합니다. last_write_page(작성된 마지막 페이지) +1을 사용하면 됩니다.
                 *찾은 페이지가 같지 않으면 ssd가 고급 명령을 탐욕스럽게 사용하도록 허용하여 작은 페이지를 큰 페이지에 더 가깝게 이동할 수 있습니다.
                 *********************************************************************************************/
                if(block1==block0)
                {
                    page1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].blk_head[block1].last_write_page+1;
                    if(page1==page0)
                    {
                        break;
                    }
                    else if(page1<page0)
                    {
                        if (ssd->parameter->greed_MPW_ad==1)                                  /*고급 명령을 탐욕스럽게 사용할 수 있습니다.*/
                        {                                                                   
                            //make_same_level(ssd,channel,chip,die,plane1,active_block1,page0); /*작은 페이지 주소는 큰 페이지 주소에 의존*/
                            make_same_level(ssd,channel,chip,die,plane1,block1,page0);
                            break;
                        }    
                    }
                }//if(block1==block0)
            }
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane1+1)%ssd->parameter->plane_die;
        }//for(i=0;i<ssd->parameter->plane_die;i++)
        if(i<ssd->parameter->plane_die)
        {
            flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page0);          /*이 기능의 기능은 page1에 해당하는 물리적 페이지, 위치 및 맵 테이블을 업데이트하는 것입니다.*/
            //flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page1);
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=(plane1+1)%ssd->parameter->plane_die;
            return SUCCESS;
        }
        else
        {
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane_token;
            return FAILURE;
        }
    }
    else                                                                                      /*정적 할당의 경우*/
    {
        if((sub1->location==NULL)||(sub1->location->channel!=channel)||(sub1->location->chip!=chip)||(sub1->location->die!=die))
        {
            return ERROR;
        }
        plane1=sub1->location->plane;
        if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].add_reg_ppn==-1)
        {
            find_active_block(ssd,channel,chip,die,plane1);
            block1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].active_block;
            if(block1==block0)
            {
                page1=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane1].blk_head[block1].last_write_page+1;
                if(page1>page0)
                {
                    return FAILURE;
                }
                else if(page1<page0)
                {
                    if (ssd->parameter->greed_MPW_ad==1)
                    { 
                        //make_same_level(ssd,channel,chip,die,plane1,active_block1,page0);    /*작은 페이지 주소는 큰 페이지 주소에 의존*/
                        make_same_level(ssd,channel,chip,die,plane1,block1,page0);
                        flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page0);
                        //flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page1);
                        return SUCCESS;
                    }
                    else
                    {
                        return FAILURE;
                    }					
                }
                else
                {
                    flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page0);
                    //flash_page_state_modify(ssd,sub1,channel,chip,die,plane1,block1,page1);
                    return SUCCESS;
                }

            }
            else
            {
                return FAILURE;
            }

        }
        else
        {
            return ERROR;
        }
    }

}

/******************************************************************************************************
 *이 기능의 기능은 두 평면 명령에 대해 수평 위치가 동일한 두 페이지를 찾아 통계 값을 수정하고 페이지의 상태를 수정하는 것입니다.
 *이 함수와 이전 함수의 차이점에 유의하십시오. make_level_page 함수, make_level_page 함수는 sub1과 sub0의 페이지 위치를 동일하게 만드는 것입니다.
 *find_level_page의 기능은 주어진 채널, 칩과 다이에서 같은 위치에 있는 두 개의 subA와 subB를 찾는 것입니다.
 *******************************************************************************************************/
Status find_level_page(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request *subA,struct sub_request *subB)       
{
    unsigned int i,planeA,planeB,active_blockA,active_blockB,pageA,pageB,aim_page,old_plane;
    struct gc_operation *gc_node;

    old_plane=ssd->channel_head[channel].chip_head[chip].die_head[die].token;

    /************************************************************
     *동적 할당의 경우
     *planeA는 초기 값이 주사위인 토큰이 할당되고, planeA가 짝수이면 planeB=planeA+1
     *평면A는 홀수이고,평면A+1은 짝수가 되고,평면B=평면A+1이 된다.
     *************************************************************/
    if (ssd->parameter->allocation_scheme==0)                                                
    {
        planeA=ssd->channel_head[channel].chip_head[chip].die_head[die].token;
        if (planeA%2==0)
        {
            planeB=planeA+1;
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=(ssd->channel_head[channel].chip_head[chip].die_head[die].token+2)%ssd->parameter->plane_die;
        } 
        else
        {
            planeA=(planeA+1)%ssd->parameter->plane_die;
            planeB=planeA+1;
            ssd->channel_head[channel].chip_head[chip].die_head[die].token=(ssd->channel_head[channel].chip_head[chip].die_head[die].token+3)%ssd->parameter->plane_die;
        }
    } 
    else                                                                                     /*정적 할당의 경우 평면A와 평면B에 직접 할당*/
    {
        planeA=subA->location->plane;
        planeB=subB->location->plane;
    }
    find_active_block(ssd,channel,chip,die,planeA);                                          /*active_block을 찾아라*/
    find_active_block(ssd,channel,chip,die,planeB);
    active_blockA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].active_block;
    active_blockB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].active_block;



    /*****************************************************
     *active_block이 같으면 이 두 블록에서 같은 페이지를 찾습니다.
     * 또는 greedy 방법을 사용하여 두 개의 동일한 페이지 찾기
     ******************************************************/
    if (active_blockA==active_blockB)
    {
        pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockA].last_write_page+1;      
        pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockB].last_write_page+1;
        if (pageA==pageB)                                                                    /*사용 가능한 두 페이지가 정확히 동일한 수평 위치에 있습니다.*/
        {
            flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,pageA);
            flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,pageB);
        } 
        else
        {
            if (ssd->parameter->greed_MPW_ad==1)                                             /*고급 명령의 탐욕스러운 사용*/
            {
                if (pageA<pageB)                                                            
                {
                    aim_page=pageB;
                    make_same_level(ssd,channel,chip,die,planeA,active_blockA,aim_page);     /*작은 페이지 주소는 큰 페이지 주소에 의존*/
                }
                else
                {
                    aim_page=pageA;
                    make_same_level(ssd,channel,chip,die,planeB,active_blockB,aim_page);    
                }
                flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,aim_page);
                flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,aim_page);
            } 
            else                                                                             /*고급 명령을 탐욕스럽게 사용하지 마십시오.*/
            {
                subA=NULL;
                subB=NULL;
                ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
                return FAILURE;
            }
        }
    }
    /*********************************
     *발견된 두 개의 active_blocks가 동일하지 않은 경우
     **********************************/
    else
    {   
        pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockA].last_write_page+1;      
        pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockB].last_write_page+1;
        if (pageA<pageB)
        {
            if (ssd->parameter->greed_MPW_ad==1)                                             /*고급 명령의 탐욕스러운 사용*/
            {
                /*******************************************************************************
                 * planeA에서는 active_blockB와 같은 위치에 있는 블록에서 pageB와 같은 위치에 있는 페이지를 사용할 수 있습니다.
                 * 즉, palneA에서 해당 수평 위치가 가능하며, planeB에서 가장 해당 페이지입니다.
                 * 그런 다음 planeA 및 active_blockB의 페이지를 pageB에 더 가깝게 이동할 수도 있습니다.
                 ********************************************************************************/
                if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockB].page_head[pageB].free_state==PG_SUB)    
                {
                    make_same_level(ssd,channel,chip,die,planeA,active_blockB,pageB);
                    flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockB,pageB);
                    flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,pageB);
                }
                /********************************************************************************
                 * planeA에서는 active_blockB와 같은 위치에 있는 블록에서 pageB와 같은 위치에 있는 페이지를 사용할 수 있습니다.
                 *그런 다음 블록을 다시 찾아야 합니다. 수평 위치가 동일한 한 쌍의 페이지를 다시 찾아야 합니다.
                 *********************************************************************************/
                else    
                {
                    for (i=0;i<ssd->parameter->block_plane;i++)
                    {
                        pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].last_write_page+1;
                        pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].last_write_page+1;
                        if ((pageA<ssd->parameter->page_block)&&(pageB<ssd->parameter->page_block))
                        {
                            if (pageA<pageB)
                            {
                                if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].page_head[pageB].free_state==PG_SUB)
                                {
                                    aim_page=pageB;
                                    make_same_level(ssd,channel,chip,die,planeA,i,aim_page);
                                    break;
                                }
                            } 
                            else
                            {
                                if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].page_head[pageA].free_state==PG_SUB)
                                {
                                    aim_page=pageA;
                                    make_same_level(ssd,channel,chip,die,planeB,i,aim_page);
                                    break;
                                }
                            }
                        }
                    }//for (i=0;i<ssd->parameter->block_plane;i++)
                    if (i<ssd->parameter->block_plane)
                    {
                        flash_page_state_modify(ssd,subA,channel,chip,die,planeA,i,aim_page);
                        flash_page_state_modify(ssd,subB,channel,chip,die,planeB,i,aim_page);
                    } 
                    else
                    {
                        subA=NULL;
                        subB=NULL;
                        ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
                        return FAILURE;
                    }
                }
            }//if (ssd->parameter->greed_MPW_ad==1)  
            else
            {
                subA=NULL;
                subB=NULL;
                ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
                return FAILURE;
            }
        }//if (pageA<pageB)
        else
        {
            if (ssd->parameter->greed_MPW_ad==1)     
            {
                if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockA].page_head[pageA].free_state==PG_SUB)
                {
                    make_same_level(ssd,channel,chip,die,planeB,active_blockA,pageA);
                    flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,pageA);
                    flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockA,pageA);
                }
                else    
                {
                    for (i=0;i<ssd->parameter->block_plane;i++)
                    {
                        pageA=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].last_write_page+1;
                        pageB=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].last_write_page+1;
                        if ((pageA<ssd->parameter->page_block)&&(pageB<ssd->parameter->page_block))
                        {
                            if (pageA<pageB)
                            {
                                if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[i].page_head[pageB].free_state==PG_SUB)
                                {
                                    aim_page=pageB;
                                    make_same_level(ssd,channel,chip,die,planeA,i,aim_page);
                                    break;
                                }
                            } 
                            else
                            {
                                if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[i].page_head[pageA].free_state==PG_SUB)
                                {
                                    aim_page=pageA;
                                    make_same_level(ssd,channel,chip,die,planeB,i,aim_page);
                                    break;
                                }
                            }
                        }
                    }//for (i=0;i<ssd->parameter->block_plane;i++)
                    if (i<ssd->parameter->block_plane)
                    {
                        flash_page_state_modify(ssd,subA,channel,chip,die,planeA,i,aim_page);
                        flash_page_state_modify(ssd,subB,channel,chip,die,planeB,i,aim_page);
                    } 
                    else
                    {
                        subA=NULL;
                        subB=NULL;
                        ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
                        return FAILURE;
                    }
                }
            } //if (ssd->parameter->greed_MPW_ad==1) 
            else
            {
                if ((pageA==pageB)&&(pageA==0))
                {
                    /*******************************************************************************************
                     *다음은 2가지 경우입니다.
                     *1, planeA 및 planeB의 active_blockA 및 pageA 위치는 모두 사용 가능하며, 다른 평면의 동일한 위치는 blockA의 적용을 받습니다.
                     *2, planeA 및 planeB의 active_blockB 및 pageA 위치는 모두 사용 가능하며, 다른 평면의 동일한 위치는 blockB의 적용을 받습니다.
                     ********************************************************************************************/
                    if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockA].page_head[pageA].free_state==PG_SUB)
                            &&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockA].page_head[pageA].free_state==PG_SUB))
                    {
                        flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockA,pageA);
                        flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockA,pageA);
                    }
                    else if ((ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].blk_head[active_blockB].page_head[pageA].free_state==PG_SUB)
                            &&(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].blk_head[active_blockB].page_head[pageA].free_state==PG_SUB))
                    {
                        flash_page_state_modify(ssd,subA,channel,chip,die,planeA,active_blockB,pageA);
                        flash_page_state_modify(ssd,subB,channel,chip,die,planeB,active_blockB,pageA);
                    }
                    else
                    {
                        subA=NULL;
                        subB=NULL;
                        ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
                        return FAILURE;
                    }
                }
                else
                {
                    subA=NULL;
                    subB=NULL;
                    ssd->channel_head[channel].chip_head[chip].die_head[die].token=old_plane;
                    return ERROR;
                }
            }
        }
    }

    if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeA].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
    {
        gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
        alloc_assert(gc_node,"gc_node");
        memset(gc_node,0, sizeof(struct gc_operation));

        gc_node->next_node=NULL;
        gc_node->chip=chip;
        gc_node->die=die;
        gc_node->plane=planeA;
        gc_node->block=0xffffffff;
        gc_node->page=0;
        gc_node->state=GC_WAIT;
        gc_node->priority=GC_UNINTERRUPT;
        gc_node->next_node=ssd->channel_head[channel].gc_command;
        ssd->channel_head[channel].gc_command=gc_node;
        ssd->gc_request++;
    }
    if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[planeB].free_page<(ssd->parameter->page_block*ssd->parameter->block_plane*ssd->parameter->gc_hard_threshold))
    {
        gc_node=(struct gc_operation *)malloc(sizeof(struct gc_operation));
        alloc_assert(gc_node,"gc_node");
        memset(gc_node,0, sizeof(struct gc_operation));

        gc_node->next_node=NULL;
        gc_node->chip=chip;
        gc_node->die=die;
        gc_node->plane=planeB;
        gc_node->block=0xffffffff;
        gc_node->page=0;
        gc_node->state=GC_WAIT;
        gc_node->priority=GC_UNINTERRUPT;
        gc_node->next_node=ssd->channel_head[channel].gc_command;
        ssd->channel_head[channel].gc_command=gc_node;
        ssd->gc_request++;
    }

    return SUCCESS;     
}

/*
 *함수의 기능은 찾은 페이지의 상태와 해당 드램의 매핑 테이블 값을 수정하는 것입니다.
 */
struct ssd_info *flash_page_state_modify(struct ssd_info *ssd,struct sub_request *sub,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int block,unsigned int page)
{
    unsigned int ppn,full_page;
    struct local *location;
    struct direct_erase *new_direct_erase,*direct_erase_node;

    full_page=~(0xffffffff<<ssd->parameter->subpage_page);
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page=page;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num--;

    if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page>63)
    {
        printf("error! the last write page larger than 64!!\n");
        while(1){}
    }

    if(ssd->dram->map->map_entry[sub->lpn].state==0)                                          /*this is the first logical page*/
    {
        ssd->dram->map->map_entry[sub->lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
        ssd->dram->map->map_entry[sub->lpn].state=sub->state;
    }
    else                                                                                      /*이 논리적 페이지가 업데이트되었으며 원본 페이지를 무효화해야 합니다.*/
    {
        ppn=ssd->dram->map->map_entry[sub->lpn].pn;
        location=find_location(ssd,ppn);
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].valid_state=0;        //表示某一页失效，同时标记valid和free状态都为0
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].free_state=0;         //表示某一页失效，同时标记valid和free状态都为0
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].page_head[location->page].lpn=0;
        ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num++;
        if (ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].blk_head[location->block].invalid_page_num==ssd->parameter->page_block)    //该block中全是invalid的页，可以直接删除
        {
            new_direct_erase=(struct direct_erase *)malloc(sizeof(struct direct_erase));
            alloc_assert(new_direct_erase,"new_direct_erase");
            memset(new_direct_erase,0, sizeof(struct direct_erase));

            new_direct_erase->block=location->block;
            new_direct_erase->next_node=NULL;
            direct_erase_node=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
            if (direct_erase_node==NULL)
            {
                ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
            } 
            else
            {
                new_direct_erase->next_node=ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node;
                ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].erase_node=new_direct_erase;
            }
        }
        free(location);
        location=NULL;
        ssd->dram->map->map_entry[sub->lpn].pn=find_ppn(ssd,channel,chip,die,plane,block,page);
        ssd->dram->map->map_entry[sub->lpn].state=(ssd->dram->map->map_entry[sub->lpn].state|sub->state);
    }

    sub->ppn=ssd->dram->map->map_entry[sub->lpn].pn;
    sub->location->channel=channel;
    sub->location->chip=chip;
    sub->location->die=die;
    sub->location->plane=plane;
    sub->location->block=block;
    sub->location->page=page;

    ssd->program_count++;
    ssd->channel_head[channel].program_count++;
    ssd->channel_head[channel].chip_head[chip].program_count++;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].lpn=sub->lpn;	
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].valid_state=sub->state;
    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page].free_state=((~(sub->state))&full_page);
    ssd->write_flash_count++;

    return ssd;
}


/********************************************
 *기능의 기능은 위치가 다른 두 페이지를 동일한 위치로 만드는 것입니다.
 *********************************************/
struct ssd_info *make_same_level(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,unsigned int plane,unsigned int block,unsigned int aim_page)
{
    int i=0,step,page;
    struct direct_erase *new_direct_erase,*direct_erase_node;

    page=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page+1;                  /*조정해야 하는 현재 블록의 쓰기 가능한 페이지 번호*/
    step=aim_page-page;
    while (i<step)
    {
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page+i].valid_state=0;     /*페이지가 유효하지 않으며 유효 및 자유 상태가 모두 0으로 표시됨을 나타냅니다.*/
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page+i].free_state=0;      /*페이지가 유효하지 않으며 유효 및 자유 상태가 모두 0으로 표시됨을 나타냅니다.*/
        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].page_head[page+i].lpn=0;

        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num++;

        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].free_page_num--;

        ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].free_page--;

        i++;
    }

    ssd->waste_page_count+=step;

    ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page=aim_page-1;

    if (ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].invalid_page_num==ssd->parameter->page_block)    /*이 블록의 모든 페이지는 유효하지 않으며 직접 삭제할 수 있습니다.*/
    {
        new_direct_erase=(struct direct_erase *)malloc(sizeof(struct direct_erase));
        alloc_assert(new_direct_erase,"new_direct_erase");
        memset(new_direct_erase,0, sizeof(struct direct_erase));

        direct_erase_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
        if (direct_erase_node==NULL)
        {
            ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=new_direct_erase;
        } 
        else
        {
            new_direct_erase->next_node=ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node;
            ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].erase_node=new_direct_erase;
        }
    }

    if(ssd->channel_head[channel].chip_head[chip].die_head[die].plane_head[plane].blk_head[block].last_write_page>63)
    {
        printf("error! the last write page larger than 64!!\n");
        while(1){}
    }

    return ssd;
}


/****************************************************************************
 * 고급 명령의 쓰기 서브 요청을 처리할 때 이 기능의 기능은 처리 시간과 처리 상태 전이를 계산하는 것입니다.
 *기능이 완벽하지 않아 개선이 필요하며 수정시에는 정적할당과 동적할당으로 나누어야 한다.
 *****************************************************************************/
struct ssd_info *compute_serve_time(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request **subs, unsigned int subs_count,unsigned int command)
{
    unsigned int i=0;
    unsigned int max_subs_num=0;
    struct sub_request *sub=NULL,*p=NULL;
    struct sub_request * last_sub=NULL;
    max_subs_num=ssd->parameter->die_chip*ssd->parameter->plane_die;

    if((command==INTERLEAVE_TWO_PLANE)||(command==COPY_BACK))
    {
        for(i=0;i<max_subs_num;i++)
        {
            if(subs[i]!=NULL)
            {
                last_sub=subs[i];
                subs[i]->current_state=SR_W_TRANSFER;
                subs[i]->current_time=ssd->current_time;
                subs[i]->next_state=SR_COMPLETE;
                subs[i]->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                subs[i]->complete_time=subs[i]->next_state_predict_time;

                delete_from_channel(ssd,channel,subs[i]);
            }
        }
        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=last_sub->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;	
    }
    else if(command==TWO_PLANE)
    {
        for(i=0;i<max_subs_num;i++)
        {
            if(subs[i]!=NULL)
            {

                subs[i]->current_state=SR_W_TRANSFER;
                if(last_sub==NULL)
                {
                    subs[i]->current_time=ssd->current_time;
                }
                else
                {
                    subs[i]->current_time=last_sub->complete_time+ssd->parameter->time_characteristics.tDBSY;
                }

                subs[i]->next_state=SR_COMPLETE;
                subs[i]->next_state_predict_time=subs[i]->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                subs[i]->complete_time=subs[i]->next_state_predict_time;
                last_sub=subs[i];

                delete_from_channel(ssd,channel,subs[i]);
            }
        }
        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=last_sub->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
    }
    else if(command==INTERLEAVE)
    {
        for(i=0;i<max_subs_num;i++)
        {
            if(subs[i]!=NULL)
            {

                subs[i]->current_state=SR_W_TRANSFER;
                if(last_sub==NULL)
                {
                    subs[i]->current_time=ssd->current_time;
                }
                else
                {
                    subs[i]->current_time=last_sub->complete_time;
                }
                subs[i]->next_state=SR_COMPLETE;
                subs[i]->next_state_predict_time=subs[i]->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[i]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                subs[i]->complete_time=subs[i]->next_state_predict_time;
                last_sub=subs[i];

                delete_from_channel(ssd,channel,subs[i]);
            }
        }
        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=last_sub->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
    }
    else if(command==NORMAL)
    {
        subs[0]->current_state=SR_W_TRANSFER;
        subs[0]->current_time=ssd->current_time;
        subs[0]->next_state=SR_COMPLETE;
        subs[0]->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(subs[0]->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        subs[0]->complete_time=subs[0]->next_state_predict_time;

        delete_from_channel(ssd,channel,subs[0]);

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=subs[0]->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
    }
    else
    {
        return NULL;
    }

    return ssd;

}


/*****************************************************************************************
 *함수의 기능은 ssd->subs_w_head 또는 ssd->channel_head[채널].subs_w_head에서 하위 요청을 삭제하는 것입니다.
 ******************************************************************************************/
struct ssd_info *delete_from_channel(struct ssd_info *ssd,unsigned int channel,struct sub_request * sub_req)
{
    struct sub_request *sub,*p;

    /******************************************************************
     * 완전히 동적으로 할당된 하위 요청은 ssd->subs_w_head에 있습니다.
     *ssd->channel_head[channel].subs_w_head에서 완전히 동적으로 할당되지 않은 하위 요청
     *******************************************************************/
    if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))    
    {
        sub=ssd->subs_w_head;
    } 
    else
    {
        sub=ssd->channel_head[channel].subs_w_head;
    }
    p=sub;

    while (sub!=NULL)
    {
        if (sub==sub_req)
        {
            if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0))
            {
                if(ssd->parameter->ad_priority2==0)
                {
                    ssd->real_time_subreq--;
                }

                if (sub==ssd->subs_w_head)                                                     /*하위 요청 대기열에서 이 하위 요청을 제거합니다.*/
                {
                    if (ssd->subs_w_head!=ssd->subs_w_tail)
                    {
                        ssd->subs_w_head=sub->next_node;
                        sub=ssd->subs_w_head;
                        continue;
                    } 
                    else
                    {
                        ssd->subs_w_head=NULL;
                        ssd->subs_w_tail=NULL;
                        p=NULL;
                        break;
                    }
                }//if (sub==ssd->subs_w_head) 
                else
                {
                    if (sub->next_node!=NULL)
                    {
                        p->next_node=sub->next_node;
                        sub=p->next_node;
                        continue;
                    } 
                    else
                    {
                        ssd->subs_w_tail=p;
                        ssd->subs_w_tail->next_node=NULL;
                        break;
                    }
                }
            }//if ((ssd->parameter->allocation_scheme==0)&&(ssd->parameter->dynamic_allocation==0)) 
            else
            {
                if (sub==ssd->channel_head[channel].subs_w_head)                               /*채널 대기열에서 이 하위 요청을 제거합니다.*/
                {
                    if (ssd->channel_head[channel].subs_w_head!=ssd->channel_head[channel].subs_w_tail)
                    {
                        ssd->channel_head[channel].subs_w_head=sub->next_node;
                        sub=ssd->channel_head[channel].subs_w_head;
                        continue;;
                    } 
                    else
                    {
                        ssd->channel_head[channel].subs_w_head=NULL;
                        ssd->channel_head[channel].subs_w_tail=NULL;
                        p=NULL;
                        break;
                    }
                }//if (sub==ssd->channel_head[channel].subs_w_head)
                else
                {
                    if (sub->next_node!=NULL)
                    {
                        p->next_node=sub->next_node;
                        sub=p->next_node;
                        continue;
                    } 
                    else
                    {
                        ssd->channel_head[channel].subs_w_tail=p;
                        ssd->channel_head[channel].subs_w_tail->next_node=NULL;
                        break;
                    }
                }//else
            }//else
        }//if (sub==sub_req)
        p=sub;
        sub=sub->next_node;
    }//while (sub!=NULL)

    return ssd;
}


struct ssd_info *un_greed_interleave_copyback(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request *sub1,struct sub_request *sub2)
{
    unsigned int old_ppn1,ppn1,old_ppn2,ppn2,greed_flag=0;

    old_ppn1=ssd->dram->map->map_entry[sub1->lpn].pn;
    get_ppn(ssd,channel,chip,die,sub1->location->plane,sub1);                                  /*발견된 ppn은 카피백 작업을 사용하기 전에 하위 요청과 동일한 평면에서 발생해야 합니다.*/
    ppn1=sub1->ppn;

    old_ppn2=ssd->dram->map->map_entry[sub2->lpn].pn;
    get_ppn(ssd,channel,chip,die,sub2->location->plane,sub2);                                  /*발견된 ppn은 카피백 작업을 사용하기 전에 하위 요청과 동일한 평면에서 발생해야 합니다.*/
    ppn2=sub2->ppn;

    if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2==ppn2%2))
    {
        ssd->copy_back_count++;
        ssd->copy_back_count++;

        sub1->current_state=SR_W_TRANSFER;
        sub1->current_time=ssd->current_time;
        sub1->next_state=SR_COMPLETE;
        sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub1->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub1->complete_time=sub1->next_state_predict_time;

        sub2->current_state=SR_W_TRANSFER;
        sub2->current_time=sub1->complete_time;
        sub2->next_state=SR_COMPLETE;
        sub2->next_state_predict_time=sub2->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub2->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub2->complete_time=sub2->next_state_predict_time;

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=sub2->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

        delete_from_channel(ssd,channel,sub1);
        delete_from_channel(ssd,channel,sub2);
    } //if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2==ppn2%2))
    else if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2!=ppn2%2))
    {
        ssd->interleave_count--;
        ssd->copy_back_count++;

        sub1->current_state=SR_W_TRANSFER;
        sub1->current_time=ssd->current_time;
        sub1->next_state=SR_COMPLETE;
        sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub1->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub1->complete_time=sub1->next_state_predict_time;

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

        delete_from_channel(ssd,channel,sub1);
    }//else if ((old_ppn1%2==ppn1%2)&&(old_ppn2%2!=ppn2%2))
    else if ((old_ppn1%2!=ppn1%2)&&(old_ppn2%2==ppn2%2))
    {
        ssd->interleave_count--;
        ssd->copy_back_count++;

        sub2->current_state=SR_W_TRANSFER;
        sub2->current_time=ssd->current_time;
        sub2->next_state=SR_COMPLETE;
        sub2->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub2->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub2->complete_time=sub2->next_state_predict_time;

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=sub2->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

        delete_from_channel(ssd,channel,sub2);
    }//else if ((old_ppn1%2!=ppn1%2)&&(old_ppn2%2==ppn2%2))
    else
    {
        ssd->interleave_count--;

        sub1->current_state=SR_W_TRANSFER;
        sub1->current_time=ssd->current_time;
        sub1->next_state=SR_COMPLETE;
        sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+2*(ssd->parameter->subpage_page*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub1->complete_time=sub1->next_state_predict_time;

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;

        delete_from_channel(ssd,channel,sub1);
    }//else

    return ssd;
}


struct ssd_info *un_greed_copyback(struct ssd_info *ssd,unsigned int channel,unsigned int chip,unsigned int die,struct sub_request *sub1)
{
    unsigned int old_ppn,ppn;

    old_ppn=ssd->dram->map->map_entry[sub1->lpn].pn;
    get_ppn(ssd,channel,chip,die,0,sub1);                                                     /*발견된 ppn은 카피백 작업을 사용하기 전에 하위 요청과 동일한 평면에서 발생해야 합니다.*/
    ppn=sub1->ppn;

    if (old_ppn%2==ppn%2)
    {
        ssd->copy_back_count++;
        sub1->current_state=SR_W_TRANSFER;
        sub1->current_time=ssd->current_time;
        sub1->next_state=SR_COMPLETE;
        sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+(sub1->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub1->complete_time=sub1->next_state_predict_time;

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
    }//if (old_ppn%2==ppn%2)
    else
    {
        sub1->current_state=SR_W_TRANSFER;
        sub1->current_time=ssd->current_time;
        sub1->next_state=SR_COMPLETE;
        sub1->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC+ssd->parameter->time_characteristics.tR+2*(ssd->parameter->subpage_page*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
        sub1->complete_time=sub1->next_state_predict_time;

        ssd->channel_head[channel].current_state=CHANNEL_TRANSFER;										
        ssd->channel_head[channel].current_time=ssd->current_time;										
        ssd->channel_head[channel].next_state=CHANNEL_IDLE;										
        ssd->channel_head[channel].next_state_predict_time=sub1->complete_time;

        ssd->channel_head[channel].chip_head[chip].current_state=CHIP_WRITE_BUSY;										
        ssd->channel_head[channel].chip_head[chip].current_time=ssd->current_time;									
        ssd->channel_head[channel].chip_head[chip].next_state=CHIP_IDLE;										
        ssd->channel_head[channel].chip_head[chip].next_state_predict_time=ssd->channel_head[channel].next_state_predict_time+ssd->parameter->time_characteristics.tPROG;
    }//else

    delete_from_channel(ssd,channel,sub1);

    return ssd;
}


/****************************************************************************************
 * 함수의 기능은 읽기 하위 요청의 고급 명령을 처리할 때 one_page와 일치하는 다른 페이지, 즉 two_page를 찾는 것입니다.
 *one_page로 투 플레인 또는 인터리브 작업을 수행할 수 있는 페이지를 찾을 수 없습니다. one_page를 한 노드 뒤로 이동해야 합니다.
 *****************************************************************************************/
struct sub_request *find_interleave_twoplane_page(struct ssd_info *ssd, struct sub_request *one_page,unsigned int command)
{
    struct sub_request *two_page;

    if (one_page->current_state!=SR_WAIT)
    {
        return NULL;                                                            
    }
    if (((ssd->channel_head[one_page->location->channel].chip_head[one_page->location->chip].current_state==CHIP_IDLE)||((ssd->channel_head[one_page->location->channel].chip_head[one_page->location->chip].next_state==CHIP_IDLE)&&
                    (ssd->channel_head[one_page->location->channel].chip_head[one_page->location->chip].next_state_predict_time<=ssd->current_time))))
    {
        two_page=one_page->next_node;
        if(command==TWO_PLANE)
        {
            while (two_page!=NULL)
            {
                if (two_page->current_state!=SR_WAIT)
                {
                    two_page=two_page->next_node;
                }
                else if ((one_page->location->chip==two_page->location->chip)&&(one_page->location->die==two_page->location->die)&&(one_page->location->block==two_page->location->block)&&(one_page->location->page==two_page->location->page))
                {
                    if (one_page->location->plane!=two_page->location->plane)
                    {
                        return two_page;                                                       /*one_page로 두 개의 평면 작업을 수행할 수 있는 페이지를 찾았습니다.*/
                    }
                    else
                    {
                        two_page=two_page->next_node;
                    }
                }
                else
                {
                    two_page=two_page->next_node;
                }
            }//while (two_page!=NULL)
            if (two_page==NULL)                                                               /*one_page로 two_plane 작업을 수행할 수 있는 페이지를 찾을 수 없습니다. one_page를 한 노드 뒤로 이동해야 합니다.*/
            {
                return NULL;
            }
        }//if(command==TWO_PLANE)
        else if(command==INTERLEAVE)
        {
            while (two_page!=NULL)
            {
                if (two_page->current_state!=SR_WAIT)
                {
                    two_page=two_page->next_node;
                }
                else if ((one_page->location->chip==two_page->location->chip)&&(one_page->location->die!=two_page->location->die))
                {
                    return two_page;                                                           /*one_page로 인터리브 작업을 수행할 수 있는 페이지를 찾았습니다.*/
                }
                else
                {
                    two_page=two_page->next_node;
                }
            }
            if (two_page==NULL)                                                                /*one_page와 interleave 작업을 수행할 수 있는 페이지가 없습니다. one_page를 한 노드 뒤로 이동해야 합니다.*/
            {
                return NULL;
            }//while (two_page!=NULL)
        }//else if(command==INTERLEAVE)

    } 
    {
        return NULL;
    }
}


/*************************************************************************
 *read sub-request 고급 명령을 처리할 때 이를 사용하여 고급 명령을 실행할 수 있는 sub_request를 찾습니다.
 **************************************************************************/
int find_interleave_twoplane_sub_request(struct ssd_info * ssd, unsigned int channel,struct sub_request * sub_request_one,struct sub_request * sub_request_two,unsigned int command)
{
    sub_request_one=ssd->channel_head[channel].subs_r_head;
    while (sub_request_one!=NULL)
    {
        sub_request_two=find_interleave_twoplane_page(ssd,sub_request_one,command);                /*위치 조건 및 시간 조건을 포함하여 two_plane 또는 인터리브를 수행할 수 있는 두 개의 읽기 하위 요청 찾기*/
        if (sub_request_two==NULL)
        {
            sub_request_one=sub_request_one->next_node;
        }
        else if (sub_request_two!=NULL)                                                            /*two plane 작업을 수행할 수 있는 두 페이지를 찾았습니다.*/
        {
            break;
        }
    }

    if (sub_request_two!=NULL)
    {
        if (ssd->request_queue!=ssd->request_tail)      
        {                                                                                         /*interleave read의 하위 요청이 첫 번째 요청의 하위 요청인지 확인합니다.*/
            if ((ssd->request_queue->lsn-ssd->parameter->subpage_page)<(sub_request_one->lpn*ssd->parameter->subpage_page))  
            {
                if ((ssd->request_queue->lsn+ssd->request_queue->size+ssd->parameter->subpage_page)>(sub_request_one->lpn*ssd->parameter->subpage_page))
                {
                }
                else
                {
                    sub_request_two=NULL;
                }
            }
            else
            {
                sub_request_two=NULL;
            }
        }//if (ssd->request_queue!=ssd->request_tail) 
    }//if (sub_request_two!=NULL)

    if(sub_request_two!=NULL)
    {
        return SUCCESS;
    }
    else
    {
        return FAILURE;
    }

}


/**************************************************************************
 *이 기능은 매우 중요하며 읽기 서브 요청의 상태 천이와 시간 계산은 모두 이 기능에 의해 처리됩니다.
 * 일반 명령어 실행시 서브요청을 쓰는 상태도 있고, 시간 계산도 이 함수로 처리합니다.
 ****************************************************************************/
Status go_one_step(struct ssd_info * ssd, struct sub_request * sub1,struct sub_request *sub2, unsigned int aim_state,unsigned int command)
{
    unsigned int i=0,j=0,k=0,m=0;
    long long time=0;
    struct sub_request * sub=NULL ; 
    struct sub_request * sub_twoplane_one=NULL, * sub_twoplane_two=NULL;
    struct sub_request * sub_interleave_one=NULL, * sub_interleave_two=NULL;
    struct local * location=NULL;
    if(sub1==NULL)
    {
        return ERROR;
    }

    /***************************************************************************************************
     *일반 명령을 처리할 때 읽기 하위 요청의 대상 상태는 SR_R_READ, SR_R_C_A_TRANSFER, SR_R_DATA_TRANSFER의 경우로 나뉩니다.
     * 쓰기 하위 요청의 대상 상태는 SR_W_TRANSFER입니다.
     ****************************************************************************************************/
    if(command==NORMAL)
    {
        sub=sub1;
        location=sub1->location;
        switch(aim_state)						
        {	
            case SR_R_READ:
                {   
                    /*****************************************************************************************************
                     *이 목표 상태는 플래시가 데이터를 읽고 있는 상태임을 의미하며, 서브의 다음 상태는 데이터 SR_R_DATA_TRANSFER를 전송해야 함을 의미합니다.
                     *이때 채널과는 아무런 관련이 없고 칩만 있으므로 칩의 상태를 CHIP_READ_BUSY로 수정해야 하며 다음 상태는 CHIP_DATA_TRANSFER
                     ******************************************************************************************************/
                    sub->current_time=ssd->current_time;
                    sub->current_state=SR_R_READ;
                    sub->next_state=SR_R_DATA_TRANSFER;
                    sub->next_state_predict_time=ssd->current_time+ssd->parameter->time_characteristics.tR;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_READ_BUSY;
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_DATA_TRANSFER;
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+ssd->parameter->time_characteristics.tR;

                    break;
                }
            case SR_R_C_A_TRANSFER:
                {   
                    /*******************************************************************************************************
                     *대상 상태가 명령 주소 전송일 때 서브의 다음 상태는 SR_R_READ
                     *이 상태는 채널 및 칩과 관련이 있으므로 채널을 수정하기 위해 칩의 상태는 각각 CHANNEL_C_A_TRANSFER, CHIP_C_A_TRANSFER입니다.
                     *다음 상태는 CHANNEL_IDLE, CHIP_READ_BUSY입니다.
                     *******************************************************************************************************/
                    sub->current_time=ssd->current_time;									
                    sub->current_state=SR_R_C_A_TRANSFER;									
                    sub->next_state=SR_R_READ;									
                    sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC;									
                    sub->begin_time=ssd->current_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=sub->ppn;
                    ssd->read_count++;

                    ssd->channel_head[location->channel].current_state=CHANNEL_C_A_TRANSFER;									
                    ssd->channel_head[location->channel].current_time=ssd->current_time;										
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;								
                    ssd->channel_head[location->channel].next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_C_A_TRANSFER;								
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;						
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_READ_BUSY;							
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC;

                    break;

                }
            case SR_R_DATA_TRANSFER:
                {   
                    /**************************************************************************************************************
                     *목표 상태가 데이터 전송일 때, 서브의 다음 상태는 완료 상태 SR_COMPLETE
                     *이 상태의 처리는 채널 및 칩과도 관련이 있으므로 채널 및 칩의 현재 상태는 CHANNEL_DATA_TRANSFER, CHIP_DATA_TRANSFER가 됩니다.
                     *다음 상태는 각각 CHANNEL_IDLE, CHIP_IDLE입니다.
                     ***************************************************************************************************************/
                    sub->current_time=ssd->current_time;					
                    sub->current_state=SR_R_DATA_TRANSFER;		
                    sub->next_state=SR_COMPLETE;				
                    sub->next_state_predict_time=ssd->current_time+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;			
                    sub->complete_time=sub->next_state_predict_time;

                    ssd->channel_head[location->channel].current_state=CHANNEL_DATA_TRANSFER;		
                    ssd->channel_head[location->channel].current_time=ssd->current_time;		
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;	
                    ssd->channel_head[location->channel].next_state_predict_time=sub->next_state_predict_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_DATA_TRANSFER;				
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;			
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;			
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=sub->next_state_predict_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=-1;

                    break;
                }
            case SR_W_TRANSFER:
                {
                    /******************************************************************************************************
                     * 쓰기 서브 요청 처리 시 상태 천이 및 시간 계산입니다.
                     * 쓰기 요청의 처리 상태는 읽기 하위 요청과 동일하지만 쓰기 요청은 데이터를 상단에서 평면으로 전송합니다.
                     *이와 같이 여러 상태를 하나의 상태로 취급할 수 있으며, 이는 SR_W_TRANSFER 상태로 처리되며, 서브의 다음 상태는 완료 상태입니다.
                     *이때 채널과 칩의 현재 상태는 CHANNEL_TRANSFER, CHIP_WRITE_BUSY가 됩니다.
                     *다음 상태는 CHANNEL_IDLE, CHIP_IDLE이 됩니다.
                     *******************************************************************************************************/
                    sub->current_time=ssd->current_time;
                    sub->current_state=SR_W_TRANSFER;
                    sub->next_state=SR_COMPLETE;
                    sub->next_state_predict_time=ssd->current_time+7*ssd->parameter->time_characteristics.tWC+(sub->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tWC;
                    sub->complete_time=sub->next_state_predict_time;		
                    time=sub->complete_time;

                    ssd->channel_head[location->channel].current_state=CHANNEL_TRANSFER;										
                    ssd->channel_head[location->channel].current_time=ssd->current_time;										
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;										
                    ssd->channel_head[location->channel].next_state_predict_time=time;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_WRITE_BUSY;										
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;									
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;										
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=time+ssd->parameter->time_characteristics.tPROG;

                    break;
                }
            default :  return ERROR;

        }//switch(aim_state)	
    }//if(command==NORMAL)
    else if(command==TWO_PLANE)
    {   
        /**********************************************************************************************
         * TWO_PLANE 고급 명령이 하위 요청을 읽기 위한 고급 명령인 고급 명령 TWO_PLANE의 처리
         *상태 전환은 일반 명령과 동일하지만 SR_R_C_A_TRANSFER에서는 채널 채널을 공유하기 때문에 계산 시간이 직렬이라는 차이점이 있습니다.
         *그리고 SR_R_DATA_TRANSFER도 채널을 공유합니다.
         **********************************************************************************************/
        if((sub1==NULL)||(sub2==NULL))
        {
            return ERROR;
        }
        sub_twoplane_one=sub1;
        sub_twoplane_two=sub2;
        location=sub1->location;

        switch(aim_state)						
        {	
            case SR_R_C_A_TRANSFER:
                {
                    sub_twoplane_one->current_time=ssd->current_time;									
                    sub_twoplane_one->current_state=SR_R_C_A_TRANSFER;									
                    sub_twoplane_one->next_state=SR_R_READ;									
                    sub_twoplane_one->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;									
                    sub_twoplane_one->begin_time=ssd->current_time;

                    ssd->channel_head[sub_twoplane_one->location->channel].chip_head[sub_twoplane_one->location->chip].die_head[sub_twoplane_one->location->die].plane_head[sub_twoplane_one->location->plane].add_reg_ppn=sub_twoplane_one->ppn;
                    ssd->read_count++;

                    sub_twoplane_two->current_time=ssd->current_time;									
                    sub_twoplane_two->current_state=SR_R_C_A_TRANSFER;									
                    sub_twoplane_two->next_state=SR_R_READ;									
                    sub_twoplane_two->next_state_predict_time=sub_twoplane_one->next_state_predict_time;									
                    sub_twoplane_two->begin_time=ssd->current_time;

                    ssd->channel_head[sub_twoplane_two->location->channel].chip_head[sub_twoplane_two->location->chip].die_head[sub_twoplane_two->location->die].plane_head[sub_twoplane_two->location->plane].add_reg_ppn=sub_twoplane_two->ppn;
                    ssd->read_count++;
                    ssd->m_plane_read_count++;

                    ssd->channel_head[location->channel].current_state=CHANNEL_C_A_TRANSFER;									
                    ssd->channel_head[location->channel].current_time=ssd->current_time;										
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;								
                    ssd->channel_head[location->channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_C_A_TRANSFER;								
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;						
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_READ_BUSY;							
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;


                    break;
                }
            case SR_R_DATA_TRANSFER:
                {
                    sub_twoplane_one->current_time=ssd->current_time;					
                    sub_twoplane_one->current_state=SR_R_DATA_TRANSFER;		
                    sub_twoplane_one->next_state=SR_COMPLETE;				
                    sub_twoplane_one->next_state_predict_time=ssd->current_time+(sub_twoplane_one->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;			
                    sub_twoplane_one->complete_time=sub_twoplane_one->next_state_predict_time;

                    sub_twoplane_two->current_time=sub_twoplane_one->next_state_predict_time;					
                    sub_twoplane_two->current_state=SR_R_DATA_TRANSFER;		
                    sub_twoplane_two->next_state=SR_COMPLETE;				
                    sub_twoplane_two->next_state_predict_time=sub_twoplane_two->current_time+(sub_twoplane_two->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;			
                    sub_twoplane_two->complete_time=sub_twoplane_two->next_state_predict_time;

                    ssd->channel_head[location->channel].current_state=CHANNEL_DATA_TRANSFER;		
                    ssd->channel_head[location->channel].current_time=ssd->current_time;		
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;	
                    ssd->channel_head[location->channel].next_state_predict_time=sub_twoplane_one->next_state_predict_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_DATA_TRANSFER;				
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;			
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;			
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=sub_twoplane_one->next_state_predict_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=-1;

                    break;
                }
            default :  return ERROR;
        }//switch(aim_state)	
    }//else if(command==TWO_PLANE)
    else if(command==INTERLEAVE)
    {
        if((sub1==NULL)||(sub2==NULL))
        {
            return ERROR;
        }
        sub_interleave_one=sub1;
        sub_interleave_two=sub2;
        location=sub1->location;

        switch(aim_state)						
        {	
            case SR_R_C_A_TRANSFER:
                {
                    sub_interleave_one->current_time=ssd->current_time;									
                    sub_interleave_one->current_state=SR_R_C_A_TRANSFER;									
                    sub_interleave_one->next_state=SR_R_READ;									
                    sub_interleave_one->next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;									
                    sub_interleave_one->begin_time=ssd->current_time;

                    ssd->channel_head[sub_interleave_one->location->channel].chip_head[sub_interleave_one->location->chip].die_head[sub_interleave_one->location->die].plane_head[sub_interleave_one->location->plane].add_reg_ppn=sub_interleave_one->ppn;
                    ssd->read_count++;

                    sub_interleave_two->current_time=ssd->current_time;									
                    sub_interleave_two->current_state=SR_R_C_A_TRANSFER;									
                    sub_interleave_two->next_state=SR_R_READ;									
                    sub_interleave_two->next_state_predict_time=sub_interleave_one->next_state_predict_time;									
                    sub_interleave_two->begin_time=ssd->current_time;

                    ssd->channel_head[sub_interleave_two->location->channel].chip_head[sub_interleave_two->location->chip].die_head[sub_interleave_two->location->die].plane_head[sub_interleave_two->location->plane].add_reg_ppn=sub_interleave_two->ppn;
                    ssd->read_count++;
                    ssd->interleave_read_count++;

                    ssd->channel_head[location->channel].current_state=CHANNEL_C_A_TRANSFER;									
                    ssd->channel_head[location->channel].current_time=ssd->current_time;										
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;								
                    ssd->channel_head[location->channel].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_C_A_TRANSFER;								
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;						
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_READ_BUSY;							
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=ssd->current_time+14*ssd->parameter->time_characteristics.tWC;

                    break;

                }
            case SR_R_DATA_TRANSFER:
                {
                    sub_interleave_one->current_time=ssd->current_time;					
                    sub_interleave_one->current_state=SR_R_DATA_TRANSFER;		
                    sub_interleave_one->next_state=SR_COMPLETE;				
                    sub_interleave_one->next_state_predict_time=ssd->current_time+(sub_interleave_one->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;			
                    sub_interleave_one->complete_time=sub_interleave_one->next_state_predict_time;

                    sub_interleave_two->current_time=sub_interleave_one->next_state_predict_time;					
                    sub_interleave_two->current_state=SR_R_DATA_TRANSFER;		
                    sub_interleave_two->next_state=SR_COMPLETE;				
                    sub_interleave_two->next_state_predict_time=sub_interleave_two->current_time+(sub_interleave_two->size*ssd->parameter->subpage_capacity)*ssd->parameter->time_characteristics.tRC;			
                    sub_interleave_two->complete_time=sub_interleave_two->next_state_predict_time;

                    ssd->channel_head[location->channel].current_state=CHANNEL_DATA_TRANSFER;		
                    ssd->channel_head[location->channel].current_time=ssd->current_time;		
                    ssd->channel_head[location->channel].next_state=CHANNEL_IDLE;	
                    ssd->channel_head[location->channel].next_state_predict_time=sub_interleave_two->next_state_predict_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].current_state=CHIP_DATA_TRANSFER;				
                    ssd->channel_head[location->channel].chip_head[location->chip].current_time=ssd->current_time;			
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state=CHIP_IDLE;			
                    ssd->channel_head[location->channel].chip_head[location->chip].next_state_predict_time=sub_interleave_two->next_state_predict_time;

                    ssd->channel_head[location->channel].chip_head[location->chip].die_head[location->die].plane_head[location->plane].add_reg_ppn=-1;

                    break;
                }
            default :  return ERROR;	
        }//switch(aim_state)				
    }//else if(command==INTERLEAVE)
    else
    {
        printf("\nERROR: Unexpected command !\n" );
        return ERROR;
    }

    return SUCCESS;
}

