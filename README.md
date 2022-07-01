# NUC131BSP_ISP_UART_APROM
 NUC131BSP_ISP_UART_APROM

update @ 2022/07/01

1. under LDROM project , add SW CRC32  , check define : ENABLE_SW_CRC32 to enable

2. check define : USE_SRAM_TABLE , USE_FLASH_TABLE , to use CRC32 table put in SRAM or FLASH

3. Scenario notice:

	- Boot loader project : ISP_UART 
	
		- under sct file (uart_iap.sct) , will allocate flash size 
		
				LDROM_Bootloader.bin : 0x100000 ~ 0xFFF (default LDROM size : 4K)
				
				use SRAM specific address to store flag : 0x20001FFC
			
				APROM_Bootloader.bin : 0xE000 0x2000 (reserve 8K size , to store extra boot loader code 

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/LDROM_KEIL_sct.jpg)		
	
		- when power on , will check power on source (ex : power on reset , nReset , from application code)
	
		- use CRC to calculate Application code checksum (length : 0xE000-4 : 0xDFFC )
		
		- load Application code checksum , from specific address (at 0xE000 last 4 bytes : 0xDFFC)
		
		- power on from LDROM , and CRC calculate correct		

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/boot_from_LDROM_to_APROM.jpg)
				
		- if two checksum result are different , will stuck in Boot loader , and wait for ISP code update
		
![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/LDROM_checksum_err.jpg)		
		
		- if boot from application code , to LDROM code , by press digit Z

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_press_Z_to_LDROM.jpg)	

		- when use ISP tool , select APROM , Reset and run 
		
![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/ISP_connect.jpg)

		- when ISP upgrade finish 
		
![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/LDROM_upgrade_finish.jpg)

		- if reset from application code , will entry timeout counting , jump to application code if TIMEOUE		

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_press_Z_timeout.jpg)

	
	- Application code project : AP
	
		- use SRecord , to calculate application code checksum , add binary to hex , by SRecord tool

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_KEIL_checksum_calculate.jpg)
	
		- SRecord file : srec_cat.exe 

		- under generateChecksum.bat will execute generateChecksum.cmd , generateCRCbinary.cmd , generateCRChex.cmd
	
			- generateChecksum.cmd : calculate checksum by load the original binary file , and display on KEIL project
		
			- generateCRCbinary.cmd : calculate checksum by load the original binary file , and fill 0xFF , range up to 0xE000
		
			- generateCRChex.cmd : conver binary file into hex file
		
		- check sum calculate will start from 0 to 0xE000-4 : 0xDFFC , and store in 0xDFFC , the last 4 bytes 
		
		- at KEIL output file , file name is APROM_application , under \obj folder , 
	
			which mapping to generateChecksum.cmd , generateCRCbinary.cmd , generateCRChex.cmd
	
			modify the file name in KEIL project , also need to modify the file name in these 3 generate***.cmd

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_KEIL_output_file.jpg)

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_SRecord_cmd_file.jpg)
		
		- after project compile finish , binary size will be 56K (total application code size : 0xE000)
		
		- under terminal , use keyboard , 'z' , 'Z' , will write specific value in SRAM flag , and return to boot loader
	
![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/KEIL_SRAM_alloction_for_LDROM_APOM.jpg)
			
	- reserve data flash address : 0x10000 , 4K
	
4. Flash allocation

	- LDROM_Bootloader.bin : 0x100000 ~ 0xFFF
	
	- APROM_Bootloader.bin : 0xE000 0x2000
	
	- Application code START address : 0x00000
	
	- Data flash : 0x10000 , 4K
	
	- Chcecksum storage : 0xDFFC

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/FLASH_calculate.jpg)
	
![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/FLASH_allocation.jpg)
	
5. Function assignment

	- debug port : UART1 (PB4 , PB5) , in Boot loader an Application code project
	
	- ISP UART port : UART0 (PB0 , PB1) , in Boot loader project
	
	- enable SRAM flag storage and software CRC , Timer1 module
	
6. Need to use ICP tool , to programm boot loader project file (LDROM_Bootloader.bin , APROM_Bootloader.bin)

below is boot loader project , Config setting 

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/LDROM_ICP_config.jpg)

below is boot loader project , ICP programming setting 

- LDROM_Bootloader.bin : under LDROM

- APROM_Bootloader.bin : 0xE000

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/LDROM_ICP_update.jpg)

7. under Application code KEIL project setting 

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_KEIL_checksum_calculate.jpg)

in Application project , press 'z' , 'Z' will reset to Boot loader 

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/APROM_press_Z.jpg)

8. under boot loader project , below is sct file content

![image](https://github.com/released/NUC131BSP_ISP_UART_APROM/blob/main/LDROM_KEIL_sct.jpg)
