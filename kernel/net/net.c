#include <e1000.h>
#include <printk.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

#define RECV_BUFFER_NUM 32
#define RECV_BUFFER_SIZE 2048
int pt=0;
int ph=0;
char pk_dd[RECV_BUFFER_NUM];
char pk_buff[RECV_BUFFER_NUM][RECV_BUFFER_SIZE];
int pk_lens[RECV_BUFFER_NUM];

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device
    //for debug
    int head = e1000_read_reg(e1000,E1000_TDH);
    int tail = e1000_read_reg(e1000,E1000_TDT);
    //enable TXQE
    e1000_write_reg(e1000,E1000_IMS,E1000_IMS_TXQE);
    length =e1000_transmit(txpacket, length);//debug
    if(length != 0)
        return length;
    //failed to transmit, add to send block queue
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    current_running->net_buffer = (uint8_t*)get_kva((uintptr_t)txpacket,current_running->satp);
    current_running->net_length = length;
    current_running->status = TASK_BLOCKED;
    do_block(&current_running->list,&send_block_queue);
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full
    
    do_scheduler();
    return 0;  // Bytes it has transmitted
}

void update_net_send()
{
    if(!list_is_empty(&send_block_queue))
    {
        //available descriptor and blocked request
        //wake up blocked tasks
        list_node_t *node = send_block_queue.next;
        pcb_t *pcb = NODE_TO_PCB(node);
        e1000_transmit(pcb->net_buffer, pcb->net_length);
        do_unblock(node);
        pcb-> status = TASK_READY;
        regs_context_t * reg = (regs_context_t *) pcb->kernel_sp;
        reg->regs[10] = pcb->net_length;
    }else
    {
        //truly have nothing to send ......
        e1000_write_reg(e1000,E1000_IMC,E1000_IMC_TXQE);
    }
}

void update_net_recv()
{
    if(!list_is_empty(&recv_block_queue))
    {
        int len;
        pcb_t *pcb = NODE_TO_PCB(recv_block_queue.next);
        int i = pcb->net_curr_num;
        int * pkt_lens = pcb->net_packet_size;
        int bias = 0;
        for(int j = 0;j<i;j++)
        {
            bias += pkt_lens[j];
        }
        while ((len=poll(pcb->net_buffer+bias))&&(i<pcb->net_length))
        {
            /* code */
            bias += len;
            pcb->net_packet_size[i] = len;
            i++;
        }
        if(i!=pcb->net_length)//not enough packets received
        {
            pcb->net_curr_num = i;
        }else
        {
            do_unblock(recv_block_queue.next);
            pcb->status = TASK_READY;
            regs_context_t * reg = (regs_context_t *) pcb->kernel_sp;
            reg->regs[10] = i;
        }
    }
    else
    {
        //nearly full,decide to take some of the packets from e1000 device
        while(pk_dd[ph]==0 && ((pk_lens[ph]=e1000_poll(pk_buff[ph]))!=0))
        {
            pk_dd[ph]=1;
            ph = (ph+1)%RECV_BUFFER_NUM;
        }
        printk("ph:%d\n,pt:%d\n",ph,pt);
    }

}
/** 
 * @param rxbuffer: the buffer to store received packets
 * @param pkt_num: number of packets to receive
 * @param pkt_lens: the length of each packet
 * @return: the number of packets received , 0 if no packet received
*/
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    int i = 0;
    int len = 0;
    int bias = 0;
    while ((len=poll(rxbuffer+bias))&&(i<pkt_num))
    {
        /* code */
        bias += len;
        pkt_lens[i] = len;
        i++;
    }
    if(i != pkt_num)
    {
        current_running->net_buffer = get_kva(rxbuffer,current_running->satp);
        current_running->net_length = pkt_num;
        current_running->net_curr_num = i;
        current_running->net_packet_size = get_kva(pkt_lens,current_running->satp);
        current_running->status = TASK_BLOCKED;
        do_block(&current_running->list,&recv_block_queue);
        do_scheduler();
    }
    return pkt_num;  // Bytes it has received
}

int poll(void *rxbuffer)
{
    if(pk_dd[pt]==1)
    {
        int len = pk_lens[pt];
        pk_dd[pt]=0;
        memcpy(rxbuffer,pk_buff[pt],pk_lens[pt]);
        pt = (pt+1)%RECV_BUFFER_NUM;
        return len;
    }else
    {
        return e1000_poll(rxbuffer);
    }
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
    uint32_t icr = e1000_read_reg(e1000, E1000_ICR);
    printk("ICR: %x\n",icr);
    if(icr & E1000_ICR_RXDMT0)//room for recv
    {
        printk("RXDMT0 interrupt\n");
        update_net_recv();
    }
    if(icr & E1000_ICR_TXQE)//room for send
    {
        printk("TXQE interrupt\n");
        update_net_send();
    }
}