/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NUC131.h"

#include "project_config.h"

/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint32_t counter_tick = 0;
volatile uint32_t counter_systick = 0;

#define PLL_CLOCK           50000000

#if defined (ENABLE_TICK_EVENT)
typedef void (*sys_pvTimeFunPtr)(void);   /* function pointer */
typedef struct timeEvent_t
{
    uint8_t             active;
    unsigned long       initTick;
    unsigned long       curTick;
    sys_pvTimeFunPtr    funPtr;
} TimeEvent_T;

#define TICKEVENTCOUNT                                 (8)                   
volatile  TimeEvent_T tTimerEvent[TICKEVENTCOUNT];
volatile uint8_t _sys_uTimerEventCount = 0;             /* Speed up interrupt reponse time if no callback function */
#endif

// #define APROM_1
#define APROM_2

#define APROM_FW_VER_ADDR      		        (FMC_FLASH_PAGE_SIZE)
//put F/W verison at 2nd page , for indicator
#if defined (APROM_1)
const uint8_t FW_Version[] __attribute__((at(APROM_FW_VER_ADDR))) = "FW_VER_V001.001";
#elif defined (APROM_2)
const uint8_t FW_Version[] __attribute__((at(APROM_FW_VER_ADDR))) = "FW_VER_V002.002";
#endif


#define DEBUG_UART_PORT							(UART1)
#define DEBUG_UART_PORT_IRQn					(UART1_IRQn)
#define DEBUG_UART_IRQHandler					(UART1_IRQHandler)

// #define DATA_FLASH_OFFSET  						(0x10800)
// #define DATA_FLASH_AMOUNT						(48)
// #define DATA_FLASH_PAGE  						(4)     // flash page : 0x200 (512)

#define __noinit__ __attribute__((zero_init))
__noinit__ int flag_check_ISP_process __attribute__((at( 0x20001FFC)));

#define RST_ADDR_LDROM                          (0)
#define RST_ADDR_APROM                          (1)
#define RST_SEL_NVIC                            (0)
#define RST_SEL_CPU                             (1)
#define RST_SEL_CHIP                            (2)

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

void systick_counter(void)
{
	counter_systick++;
}

uint32_t get_systick(void)
{
	return (counter_systick);
}

void set_systick(uint32_t t)
{
	counter_systick = t;
}

void tick_counter(void)
{
	counter_tick++;
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void compare_buffer(uint8_t *src, uint8_t *des, int nBytes)
{
    uint16_t i = 0;	
	
    for (i = 0; i < nBytes; i++)
    {
        if (src[i] != des[i])
        {
            printf("error idx : %4d : 0x%2X , 0x%2X\r\n", i , src[i],des[i]);
			set_flag(flag_error , ENABLE);
        }
    }

	if (!is_flag_set(flag_error))
	{
    	printf("%s finish \r\n" , __FUNCTION__);	
		set_flag(flag_error , DISABLE);
	}

}

void reset_buffer(void *dest, unsigned int val, unsigned int size)
{
    uint8_t *pu8Dest;
//    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;

	#if 1
	while (size-- > 0)
		*pu8Dest++ = val;
	#else
	memset(pu8Dest, val, size * (sizeof(pu8Dest[0]) ));
	#endif
	
}

void copy_buffer(void *dest, void *src, unsigned int size)
{
    uint8_t *pu8Src, *pu8Dest;
    unsigned int i;
    
    pu8Dest = (uint8_t *)dest;
    pu8Src  = (uint8_t *)src;


	#if 0
	  while (size--)
	    *pu8Dest++ = *pu8Src++;
	#else
    for (i = 0; i < size; i++)
        pu8Dest[i] = pu8Src[i];
	#endif
}

void dump_buffer(uint8_t *pucBuff, int nBytes)
{
    uint16_t i = 0;
    
    printf("dump_buffer : %2d\r\n" , nBytes);    
    for (i = 0 ; i < nBytes ; i++)
    {
        printf("0x%2X," , pucBuff[i]);
        if ((i+1)%8 ==0)
        {
            printf("\r\n");
        }            
    }
    printf("\r\n\r\n");
}

void dump_buffer_hex(uint8_t *pucBuff, int nBytes)
{
    int     nIdx, i;

    nIdx = 0;
    while (nBytes > 0)
    {
        printf("0x%04X  ", nIdx);
        for (i = 0; i < 16; i++)
            printf("%02X ", pucBuff[nIdx + i]);
        printf("  ");
        for (i = 0; i < 16; i++)
        {
            if ((pucBuff[nIdx + i] >= 0x20) && (pucBuff[nIdx + i] < 127))
                printf("%c", pucBuff[nIdx + i]);
            else
                printf(".");
            nBytes--;
        }
        nIdx += 16;
        printf("\n");
    }
    printf("\n");
}



void delay_ms(uint16_t ms)
{
	TIMER_Delay(TIMER0, 1000*ms);
}


void SystemReboot_RST(unsigned char addr , unsigned char sel)
{
    while(!UART_IS_TX_EMPTY(UART1));
        
    /* Unlock protected registers */
    SYS_UnlockReg();
    /* Enable FMC ISP function */
    FMC_Open();

    switch(addr) // CONFIG: w/ IAP
    {
        case RST_ADDR_LDROM:
            /* Mask all interrupt before changing VECMAP to avoid wrong interrupt handler fetched */
            __set_PRIMASK(1);    
            FMC_SetVectorPageAddr(FMC_LDROM_BASE);
            FMC_SET_LDROM_BOOT();        
            break;
        case RST_ADDR_APROM:
            /* Mask all interrupt before changing VECMAP to avoid wrong interrupt handler fetched */
            __set_PRIMASK(1);    
            FMC_SetVectorPageAddr(FMC_APROM_BASE);
            FMC_SET_APROM_BOOT();        
            break;            
    }

    switch(sel)
    {
        case RST_SEL_NVIC:  // Reset I/O and peripherals , only check BS(FMC_ISPCTL[1])
            NVIC_SystemReset();
            break;
        case RST_SEL_CPU:   // Not reset I/O and peripherals
            SYS_ResetCPU();
            break;   
        case RST_SEL_CHIP:
            SYS_ResetChip();// Reset I/O and peripherals ,  BS(FMC_ISPCTL[1]) reload from CONFIG setting (CBS)
            break;                       
    } 
}

uint8_t read_magic_tag(void)
{
    uint8_t tag = 0;

    tag = (uint8_t) flag_check_ISP_process;

    printf("Read MagicTag <0x%02X>\r\n", tag);
    
    return tag;
}

void write_magic_tag(uint8_t tag)
{
    flag_check_ISP_process = tag;    

    printf("Write MagicTag <0x%02X>\r\n", tag);
}


void GPIO_Init (void)
{
    SYS->GPB_MFP &= ~(SYS_GPB_MFP_PB14_Msk | SYS_GPB_MFP_PB14_Msk);
    SYS->GPB_MFP |= (SYS_GPB_MFP_PB14_GPIO | SYS_GPB_MFP_PB15_GPIO);

	
    GPIO_SetMode(PB, BIT14, GPIO_PMD_OUTPUT);
    GPIO_SetMode(PB, BIT15, GPIO_PMD_OUTPUT);	
}

#if defined (ENABLE_TICK_EVENT)
void TickCallback_processB(void)
{
    printf("%s test \r\n" , __FUNCTION__);
}

void TickCallback_processA(void)
{
    printf("%s test \r\n" , __FUNCTION__);
}

void TickClearTickEvent(uint8_t u8TimeEventID)
{
    if (u8TimeEventID > TICKEVENTCOUNT)
        return;

    if (tTimerEvent[u8TimeEventID].active == TRUE)
    {
        tTimerEvent[u8TimeEventID].active = FALSE;
        _sys_uTimerEventCount--;
    }
}

signed char TickSetTickEvent(unsigned long uTimeTick, void *pvFun)
{
    int  i;
    int u8TimeEventID = 0;

    for (i = 0; i < TICKEVENTCOUNT; i++)
    {
        if (tTimerEvent[i].active == FALSE)
        {
            tTimerEvent[i].active = TRUE;
            tTimerEvent[i].initTick = uTimeTick;
            tTimerEvent[i].curTick = uTimeTick;
            tTimerEvent[i].funPtr = (sys_pvTimeFunPtr)pvFun;
            u8TimeEventID = i;
            _sys_uTimerEventCount += 1;
            break;
        }
    }

    if (i == TICKEVENTCOUNT)
    {
        return -1;    /* -1 means invalid channel */
    }
    else
    {
        return u8TimeEventID;    /* Event ID start from 0*/
    }
}

void TickCheckTickEvent(void)
{
    uint8_t i = 0;

    if (_sys_uTimerEventCount)
    {
        for (i = 0; i < TICKEVENTCOUNT; i++)
        {
            if (tTimerEvent[i].active)
            {
                tTimerEvent[i].curTick--;

                if (tTimerEvent[i].curTick == 0)
                {
                    (*tTimerEvent[i].funPtr)();
                    tTimerEvent[i].curTick = tTimerEvent[i].initTick;
                }
            }
        }
    }
}

void TickInitTickEvent(void)
{
    uint8_t i = 0;

    _sys_uTimerEventCount = 0;

    /* Remove all callback function */
    for (i = 0; i < TICKEVENTCOUNT; i++)
        TickClearTickEvent(i);

    _sys_uTimerEventCount = 0;
}
#endif 

void SysTick_Handler(void)
{
	static uint32_t LOG = 0;   

    systick_counter();
    if (get_systick() >= 0xFFFFFFFF)
    {
      set_systick(0);      
    }

    if ((get_systick() % 1000) == 0)
    {
        printf("%s : %4d\r\n",__FUNCTION__,LOG++);
    }

    #if defined (ENABLE_TICK_EVENT)
    TickCheckTickEvent();
    #endif    
}

void enable_sys_tick(int ticks_per_second)
{
    set_systick(0);
    if (SysTick_Config(SystemCoreClock / ticks_per_second))
    {
        /* Setup SysTick Timer for 1 second interrupts  */
        printf("Set system tick error!!\n");
        while (1);
    }

    #if defined (ENABLE_TICK_EVENT)
    TickInitTickEvent();
    #endif
}

void TMR1_IRQHandler(void)
{
	static uint32_t LOG = 0;
	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		#if defined (APROM_1)
		if ((get_tick() % 1000) == 0)
		{
        	printf("1)%s : %4d\r\n",__FUNCTION__,LOG++);
			PB14 ^= 1;
		}
		#endif

		#if defined (APROM_2)
		if ((get_tick() % 500) == 0)
		{
        	printf("2)%s : %4d\r\n",__FUNCTION__,LOG++);
			PB14 ^= 1;
		}
		#endif

		if ((get_tick() % 50) == 0)
		{

		}	
    }
}


void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(DEBUG_UART_PORT);

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		switch(res)
		{
			case '1':
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
    			read_magic_tag();			
	            write_magic_tag(0xA5);
    			read_magic_tag();

	            printf("Perform RST to enter BOOTLOADER\r\n");
	            SystemReboot_RST(RST_ADDR_LDROM,RST_SEL_CPU);	
				break;
		}
	}
}

void DEBUG_UART_IRQHandler(void)
{
    if(UART_GET_INT_FLAG(DEBUG_UART_PORT, UART_ISR_RDA_INT_Msk | UART_ISR_TOUT_IF_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(DEBUG_UART_PORT) == 0)
        {
			UARTx_Process();
        }
    }

    if(DEBUG_UART_PORT->FSR & (UART_FSR_BIF_Msk | UART_FSR_FEF_Msk | UART_FSR_PEF_Msk | UART_FSR_RX_OVER_IF_Msk))
    {
        UART_ClearIntFlag(DEBUG_UART_PORT, (UART_ISR_RLS_INT_Msk| UART_ISR_BUF_ERR_INT_Msk));
    }	
}

void DEBUG_UART_Init(void)
{
    SYS_ResetModule(UART1_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(DEBUG_UART_PORT, 115200);
    UART_EnableInt(DEBUG_UART_PORT, UART_IER_RDA_IEN_Msk | UART_IER_TOUT_IEN_Msk);
    NVIC_EnableIRQ(DEBUG_UART_PORT_IRQn);
	
	#if (_debug_log_UART_ == 1)	//debug
	printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetHXTFreq());
	printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHCLKFreq());
	printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetPCLKFreq());	
	printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetCPUFreq());

	#endif
}

void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    CLK_EnableXtalRC(CLK_PWRCON_OSC22M_EN_Msk);
    CLK_WaitClockReady(CLK_CLKSTATUS_OSC22M_STB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCON_XTL12M_EN_Msk);
//    CLK_WaitClockReady(CLK_CLKSTATUS_XTL12M_STB_Msk);

    /* Select HCLK clock source as HIRC and HCLK source divider as 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_HIRC, CLK_CLKDIV_HCLK(1));

    CLK_SetCoreClock(PLL_CLOCK);

    // CLK_EnableModuleClock(UART0_MODULE);
    // CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_CLKDIV_UART(1));

    CLK_EnableModuleClock(UART1_MODULE);
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_CLKDIV_UART(1));

    CLK_EnableModuleClock(TMR0_MODULE);
  	CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0_S_HIRC, 0);

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1_S_HIRC, 0);

    // UART 0 for ISP 
    /* Set GPB multi-function pins for UART0 RXD and TXD */
    // SYS->GPB_MFP &= ~(SYS_GPB_MFP_PB0_Msk | SYS_GPB_MFP_PB1_Msk);
    // SYS->GPB_MFP |= (SYS_GPB_MFP_PB0_UART0_RXD | SYS_GPB_MFP_PB1_UART0_TXD);

	// UART 1 for debug
    SYS->GPB_MFP &= ~(SYS_GPB_MFP_PB4_Msk | SYS_GPB_MFP_PB5_Msk);
    SYS->GPB_MFP |= (SYS_GPB_MFP_PB4_UART1_RXD | SYS_GPB_MFP_PB5_UART1_TXD);

   /* Update System Core Clock */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}

int main()
{
    SYS_Init();

	GPIO_Init();
	DEBUG_UART_Init();
	TIMER1_Init();

    read_magic_tag();// for debug

    #if defined (ENABLE_TICK_EVENT)
    enable_sys_tick(1000);
    TickSetTickEvent(1000, TickCallback_processA);  // 1000 ms
    TickSetTickEvent(5000, TickCallback_processB);  // 5000 ms
    #endif

    /* Got no where to go, just loop forever */
    while(1)
    {


    }
}

/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
