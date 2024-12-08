#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/net.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    // TODO: [p5-task1] Transmit one network packet via e1000 device

    //for debug
    int head = e1000_read_reg(e1000,E1000_TDH);
    int tail = e1000_read_reg(e1000,E1000_TDT);
    printk("head:%d tail:%d\n",head,tail);
    length =e1000_transmit(txpacket, length);
    if(length != 0)
        return length;
    //failed to transmit, add to send block queue
    // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
    current_running->status = TASK_BLOCKED;
    do_block(&current_running->list,&send_block_queue);
    // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full
    
    do_scheduler();
    return 0;  // Bytes it has transmitted
}

void update_net_send()
{
    if(update_tx_desc()& !list_is_empty(&send_block_queue))
    {
        //available descriptor and blocked request
        //wake up blocked tasks
        list_node_t *node = send_block_queue.next;
        pcb_t *pcb = NODE_TO_PCB(node);
        e1000_transmit(pcb->buffer, pcb->length);
        do_unblock(node);
        pcb-> status = TASK_READY;
        regs_context_t * reg = (regs_context_t *) pcb->kernel_sp;
        reg->regs[10] = pcb->length;
    }
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    // TODO: [p5-task2] Receive one network packet via e1000 device
    // TODO: [p5-task3] Call do_block when there is no packet on the way

    return 0;  // Bytes it has received
}

void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device
}