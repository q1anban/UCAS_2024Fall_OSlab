#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000; // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
    /* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

    /* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

    /**
     * Delay to allow any outstanding PCI transactions to complete before
     * resetting the device
     */
    latency(1);

    /* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);
    //enable TXQE and RXDMT0 interrupts
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE|E1000_IMS_RXDMT0);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR))
        ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for (int i = 0; i < TXDESCS; i++)
    {
        // DEXT=0 means using legacy descriptor format, RS means report status
        // we assume that every frame is the end of a packet
        tx_desc_array[i].cmd = ((~E1000_TXD_CMD_DEXT) & E1000_TXD_CMD_RS) | E1000_TXD_CMD_EOP;
        tx_desc_array[i].status = E1000_TXD_STAT_DD;
        tx_desc_array[i].length = TX_PKT_SIZE;
        tx_desc_array[i].addr = (uint64_t)(kva2pa((uintptr_t)tx_pkt_buffer[i]));
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)kva2pa((uintptr_t)tx_desc_array));
    e1000_write_reg(e1000, E1000_TDBAH, (uint32_t)((kva2pa((uintptr_t)tx_desc_array)) >> 32));
    e1000_write_reg(e1000, E1000_TDLEN, TXDESCS * sizeof(struct e1000_tx_desc));

    /* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);
    /* TODO: [p5-task1] Program the Transmit Control Register */
    e1000_write_reg(e1000, E1000_TCTL, E1000_TCTL_PSP | E1000_TCTL_EN | (0x10 << 4) | (0x40 << 12));
    // E1000_TCTL_CT 0x00000ff0 , set to 0x10
    // E1000_TCTL_COLD 0x003ff000 , set to 0x40
    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    uint32_t RAL0_v = (enetaddr[3] << 24) | (enetaddr[2] << 16) | (enetaddr[1] << 8) | (enetaddr[0]);
    uint32_t RAH0_v = E1000_RAH_AV | (enetaddr[5] << 8) | (enetaddr[4]);
    e1000_write_reg_array(e1000, E1000_RA, 0, RAL0_v);
    e1000_write_reg_array(e1000, E1000_RA, 1, RAH0_v);
    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i = 0; i < RXDESCS; i++)
    {
        rx_desc_array[i].addr = (uint64_t)(kva2pa((uintptr_t)&rx_pkt_buffer[i]));
        rx_desc_array[i].csum = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].length = 0;
        rx_desc_array[i].special = 0;
    }
    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)kva2pa((uintptr_t)rx_desc_array));
    e1000_write_reg(e1000, E1000_RDBAH, (uint32_t)((kva2pa((uintptr_t)rx_desc_array)) >> 32));
    e1000_write_reg(e1000, E1000_RDLEN, RXDESCS * sizeof(struct e1000_rx_desc));
    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);
    /* TODO: [p5-task2] Program the Receive Control Register */
    e1000_write_reg(e1000, E1000_RCTL, (E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_SZ_2048|E1000_RCTL_RDMTS_HALF) & (~E1000_RCTL_BSEX));
    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */
    local_flush_dcache();
    int tail = e1000_read_reg(e1000, E1000_TDT);
    if (tx_desc_array[tail].status & E1000_TXD_STAT_DD)
    {
        // available descriptor
        memcpy((uint8_t*)tx_pkt_buffer[tail], (uint8_t*)txpacket, length);
        tx_desc_array[tail].length = length;
        tx_desc_array[tail].status ^= E1000_TXD_STAT_DD; // remove DD bit for head pointer to set it
        e1000_write_reg(e1000, E1000_TDT, (tail + 1) % TXDESCS);

        local_flush_dcache();
        return length;
    }
    else
    {
        // no more available descriptor
        return 0;
    }
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * receive one frame if available
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet , 0 if no packet available
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */
    local_flush_dcache();
    int tail = e1000_read_reg(e1000, E1000_RDT);
    int head = e1000_read_reg(e1000, E1000_RDH);//debug
    local_flush_dcache();
    tail = (tail + 1) % RXDESCS;
    if (rx_desc_array[tail].status & E1000_RXD_STAT_DD)
    {
        memcpy((uint8_t*)rxbuffer, (uint8_t*)rx_pkt_buffer[tail], rx_desc_array[tail].length);
        int length = rx_desc_array[tail].length;
        rx_desc_array[tail].status = 0;
        rx_desc_array[tail].length = 0;
        e1000_write_reg(e1000, E1000_RDT, tail);
        local_flush_dcache();
        return length;
    }
    return 0;
}

/**
 * @return - if tail pointer is available for transmitting return 1, else return 0
 * **/
int update_tx_desc()
{
    local_flush_dcache();
    int tail = e1000_read_reg(e1000, E1000_TDT);
    if (tx_desc_array[tail].status & E1000_TXD_STAT_DD)
        return 1;
    else
        return 0;
}