/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "NUC131.h"

#include "project_config.h"

#include "ISP_USER.h"
#include "targetdev.h"
#include "uart_transfer.h"


/*_____ D E C L A R A T I O N S ____________________________________________*/

/*_____ D E F I N I T I O N S ______________________________________________*/
volatile uint32_t BitFlag = 0;
volatile uint32_t counter_tick = 0;
volatile uint32_t counter_systick = 0;

#define DEBUG_UART_PORT							(UART1)
#define DEBUG_UART_PORT_IRQn					(UART1_IRQn)
#define DEBUG_UART_IRQHandler					(UART1_IRQHandler)

#define PLLCON_SETTING      CLK_PLLCON_50MHz_HIRC
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

__IO uint32_t timeout_cnt = 0;
#define TIMEOUT_INTERVAL    	                (5)   // sec


#define ENABLE_SW_CRC32

// TODO:
/*
	boot loader code range :
	LDROM : 4K

	target APROM size : 68K = 0x11 000

	for checksum storage start address :	// 4 BYTES
	0x0D FFC ~ 0x0D FFF
	
	LDROM extra storage in APROM start address : // 8K
	0x0E 000 ~ 0x10 000

	DATA FALSH in APROM start address : // 4K
	0x10 000
*/

// #define DATA_FLASH_OFFSET  						(0x10800)
// #define DATA_FLASH_AMOUNT						(48)
// #define DATA_FLASH_PAGE  						(4)     // flash page : 0x200 (512)

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

#if defined (ENABLE_SW_CRC32)
#define USE_SRAM_TABLE
// #define USE_FLASH_TABLE

unsigned long state = 0xFFFFFFFF;

#if defined (USE_FLASH_TABLE)
const uint32_t table[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};
#elif defined (USE_SRAM_TABLE)
unsigned long table[256];
#define POLYNOMIAL 0xedb88320
void calculate_table(void)
{
    unsigned b = 0;
    unsigned long v = 0;
    int i = 0;

    for (b = 0; b < 256; ++b)
    {
        v = b;
        i = 8;
        for (; --i >= 0; )
            v = (v & 1) ? ((v >> 1) ^ POLYNOMIAL) : (v >> 1);
        table[b] = v;
    }
}
#endif

unsigned long UPDC32(unsigned char octet, unsigned long crc)
{
    // The original code had this as a #define
    return table[(crc ^ octet) & 0xFF] ^ (crc >> 8);
}

#endif

//
// check_reset_source
//
uint8_t check_reset_source(void)
{
    uint8_t tag = 0;
    uint32_t src = SYS_GetResetSrc();

    SYS->RSTSRC |= 0xFF;
    LDROM_DEBUG("Reset Source <0x%08X>\r\n", src);
   
    tag = read_magic_tag();
    
    if (src & SYS_RSTSRC_RSTS_POR_Msk) 
    {
        SYS_ClearResetSrc(SYS_RSTSRC_RSTS_POR_Msk);            
        if (tag == 0xA5) {
            write_magic_tag(0);
            LDROM_DEBUG("Enter BOOTLOADER from APPLICATION(POR)\r\n");
            return TRUE;
        } else if (tag == 0xBB) {
            write_magic_tag(0);
            LDROM_DEBUG("Upgrade finished...(POR)\r\n");
            return FALSE;
        } else {
            LDROM_DEBUG("Enter BOOTLOADER from POR\r\n");
            return FALSE;
        }
    } else if (src & SYS_RSTSRC_RSTS_CPU_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSRC_RSTS_CPU_Msk);        
        if (tag == 0xA5) {
            write_magic_tag(0);
            LDROM_DEBUG("Enter BOOTLOADER from APPLICATION(CPU)\r\n");
            return TRUE;
        } else if (tag == 0xBB) {
            write_magic_tag(0);
            LDROM_DEBUG("Upgrade finished...(CPU)\r\n");
            return FALSE;
        } else {
            LDROM_DEBUG("Enter BOOTLOADER from CPU reset\r\n");
            return FALSE;
        }          
    } else if (src & SYS_RSTSRC_RSTS_RESET_Msk)
    {
        SYS_ClearResetSrc(SYS_RSTSRC_RSTS_RESET_Msk);
        LDROM_DEBUG("Enter BOOTLOADER from nRESET pin\r\n");
        return FALSE;
    }
    
    LDROM_DEBUG("Enter BOOTLOADER from unhandle reset source\r\n");
    return FALSE;
}

uint32_t caculate_crc32_checksum(uint32_t start, uint32_t size)
{
    volatile uint32_t addr, data;    

    #if defined (USE_FLASH_TABLE)
    LDROM_DEBUG("ENABLE_SW_CRC32:[FLASH_TABLE]\r\n");    
    #elif defined (USE_SRAM_TABLE)
    LDROM_DEBUG("ENABLE_SW_CRC32:[SRAM_TABLE]\r\n");       
    calculate_table();
    #endif

    state = 0xFFFFFFFF;    
    addr = start;

    for(addr = start; addr < (start+size) ; addr += 4){
        data = FMC_Read(addr);
        state = UPDC32(_GET_BYTE0(data), state);
        state = UPDC32(_GET_BYTE1(data), state);
        state = UPDC32(_GET_BYTE2(data), state);
        state = UPDC32(_GET_BYTE3(data), state); 
    }

    return ~state;  

}

uint8_t verify_application_chksum(void)
{
    uint32_t chksum_cal, chksum_app;
    
    LDROM_DEBUG("Verify Checksum\r\n");
    
    chksum_cal = caculate_crc32_checksum(0x00000000, (g_apromSize - 4));//(g_apromSize - FMC_FLASH_PAGE_SIZE)
    LDROM_DEBUG("Caculated .....<0x%08X>\r\n", chksum_cal);
    
    chksum_app = FMC_Read(g_apromSize - 4);    
    LDROM_DEBUG("In APROM ......<0x%08X>\r\n", chksum_app);
    
    if (chksum_cal == chksum_app) {
        LDROM_DEBUG("Verify ........<PASS>\r\n");
        return TRUE;
    } else {
        LDROM_DEBUG("Verify ........<FAIL>\r\n");
        return FALSE;
    }
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
	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
        timeout_cnt++;
    }
}


void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
//    TIMER_Start(TIMER1);
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(DEBUG_UART_PORT);

	if (res > 0x7F)
	{
		LDROM_DEBUG("invalid command\r\n");
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
				NVIC_SystemReset();		
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
	LDROM_DEBUG("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetHXTFreq());
	LDROM_DEBUG("CLK_GetHXTFreq : %8d\r\n",CLK_GetHCLKFreq());
	LDROM_DEBUG("CLK_GetLXTFreq : %8d\r\n",CLK_GetPCLKFreq());	
	LDROM_DEBUG("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetCPUFreq());

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

    /* Set core clock as PLL_CLOCK from PLL */
    CLK_SetCoreClock(PLL_CLOCK);

    CLK_EnableModuleClock(UART0_MODULE);
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_CLKDIV_UART(1));

    CLK_EnableModuleClock(UART1_MODULE);
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UART_S_HIRC, CLK_CLKDIV_UART(1));

    CLK_EnableModuleClock(TMR0_MODULE);
  	CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0_S_HIRC, 0);

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1_S_HIRC, 0);

    // UART 0 for ISP 
    /* Set GPB multi-function pins for UART0 RXD and TXD */
    SYS->GPB_MFP &= ~(SYS_GPB_MFP_PB0_Msk | SYS_GPB_MFP_PB1_Msk);
    SYS->GPB_MFP |= (SYS_GPB_MFP_PB0_UART0_RXD | SYS_GPB_MFP_PB1_UART0_TXD);

	// UART 1 for debug
    SYS->GPB_MFP &= ~(SYS_GPB_MFP_PB4_Msk | SYS_GPB_MFP_PB5_Msk);
    SYS->GPB_MFP |= (SYS_GPB_MFP_PB4_UART1_RXD | SYS_GPB_MFP_PB5_UART1_TXD);

   /* Update System Core Clock */
    SystemCoreClockUpdate();
}

int main()
{
    uint32_t lcmd;
    uint32_t fw_addr = 0;
    uint8_t i = 0;    
    uint8_t buffer[16] = {0};

    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Init System, peripheral clock and multi-function I/O */
    SYS_Init();
    /* Init UART to 115200-8n1 */

	DEBUG_UART_Init();
    UART_Init();//ISP_UART_Init();
	TIMER1_Init();
    
    /* Enable FMC ISP */
    CLK->AHBCLK |= CLK_AHBCLK_ISP_EN_Msk;
    // Enable FMC and APROM update
    //
    FMC_Open();
    FMC_ENABLE_AP_UPDATE();
    
    //
    // Get APROM and data flash information
    //
    g_apromSize = APROM_APPLICATION_SIZE;
    GetDataFlashInfo(&g_dataFlashAddr, &g_dataFlashSize);

    //
    // Stay in BOOTLOADER or jump to APPLICATION
    //
    if (!check_reset_source()) 
	{
        if (verify_application_chksum()) 
		{
            goto exit;
        } 
		else 
		{
            LDROM_DEBUG("Stay in BOOTLOADER (checksum invaild)\r\n");
        }
    } 
	else 
    {
        LDROM_DEBUG("Stay in BOOTLOADER (from APPLICATION)\r\n");
        
        //
        // start timer
        //
        LDROM_DEBUG("Time-out counter start....\r\n");
        TIMER_Start(TIMER1);
    }

    SYS_UnlockReg();
    FMC_Open();    
    while(1)
    {
        if ((bufhead >= 4) || (bUartDataReady == TRUE))
        {
            // uint32_t lcmd;
            lcmd = inpw(uart_rcvbuf);

            if (lcmd == CMD_CONNECT)
            {
                goto _ISP;
            }
            else
            {
                bUartDataReady = FALSE;
                bufhead = 0;
            }
        }

        if (timeout_cnt > TIMEOUT_INTERVAL) {
            LDROM_DEBUG("Time-out, perform RESET\r\n");
            // while(!UART_IS_TX_EMPTY(DEBUG_UART_PORT));
        
            // // Reset chip to enter bootloader
            // SYS_UnlockReg();
            // SYS_ResetChip();
            
            SystemReboot_RST(RST_ADDR_LDROM,RST_SEL_CHIP);
        }

    }

_ISP:

    while (1)
    {
        if (bUartDataReady == TRUE)
        {         
            bUartDataReady = FALSE;
            ParseCmd(uart_rcvbuf, 64);
            PutString();
        }
    }


exit:
    fw_addr = APROM_FW_VER_ADDR;
    for (i = 0 ; i <16 ; i++)
    {
        buffer[i] = *(__IO uint8_t *)fw_addr;
        fw_addr++;
    }
    LDROM_DEBUG("Jump to <APPLICATION>,%s\r\n",buffer);
    while(!UART_IS_TX_EMPTY(DEBUG_UART_PORT));
    
    #if 1
    SystemReboot_RST(RST_ADDR_APROM,RST_SEL_CPU);

    #else
    /* Reset system and boot from APROM */
    SYS->RSTSRC = (SYS_RSTSRC_RSTS_POR_Msk | SYS_RSTSRC_RSTS_RESET_Msk); /* Clear reset status flag */
    FMC->ISPCON = FMC->ISPCON & 0xFFFFFFFC;
    SCB->AIRCR = (V6M_AIRCR_VECTKEY_DATA | V6M_AIRCR_SYSRESETREQ);
    #endif
    
    /* Trap the CPU */
    while (1);   

	
}

/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
