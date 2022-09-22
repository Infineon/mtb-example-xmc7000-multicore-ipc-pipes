/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM0+ in the MultiCore IPC Pipes
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cycfg.h"
#include "cyhal.h"
#include "cybsp.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define CM7_DUAL                1

/* IPC channel number */
#define USED_IPC_CHANNEL        7

/* Release interrupt number. This interrupt is handled by notifier core (CM7) */
#define IPC_RELEASE_INT_NUMBER  6

/* Notify interrupt number. This interrupt is handled by notified core (CM0+) */
#define IPC_NOTIFY_INT_NUMBER   7

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void IpcNotifyInt_ISR(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
static cy_stc_sysint_t stcSysIntIpcNotifyInt =
{
    .intrSrc = ((NvicMux3_IRQn << 16) | (cy_en_intr_t)(cpuss_interrupts_ipc_0_IRQn + USED_IPC_CHANNEL)),
    .intrPriority = 2UL
};


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for the CM0 core.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    /* enable interrupts */
    __enable_irq();

    /* Enable CM7_0/1. CY_CORTEX_M7_APPL_ADDR is calculated in linker script, check it in case of problems. */
    Cy_SysEnableCM7(CORE_CM7_0, CY_CORTEX_M7_0_APPL_ADDR);
    Cy_SysLib_Delay(1000); /* To avoid race between CM7's while initializing the clocks */

#if CM7_DUAL
    Cy_SysEnableCM7(CORE_CM7_1, CY_CORTEX_M7_1_APPL_ADDR);
#endif /* CM7_DUAL */

    SystemCoreClockUpdate();

    /* Setup IPC interrupt line */
    Cy_SysInt_Init(&stcSysIntIpcNotifyInt,IpcNotifyInt_ISR);
    /* Enable the interrupt */
    NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);

    /* Don't set the release interrupt. If user needs to use the release interrupt on the client side
     * user must set it after CM0+ server works.
     */
    Cy_IPC_Drv_SetInterruptMask(Cy_IPC_Drv_GetIntrBaseAddr(IPC_NOTIFY_INT_NUMBER),CY_IPC_NO_NOTIFICATION,(1uL << USED_IPC_CHANNEL));

    /* Initialize LEDs GPIO */
    cyhal_gpio_init(CYBSP_USER_LED, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    cyhal_gpio_init(CYBSP_USER_LED2, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);
    cyhal_gpio_init(CYBSP_USER_LED3, CYHAL_GPIO_DIR_OUTPUT, CYHAL_GPIO_DRIVE_STRONG, CYBSP_LED_STATE_OFF);

    for(;;)
    {

    }
}

/*******************************************************************************
* Function Name: IpcNotifyInt_ISR
********************************************************************************
* Summary:
*  IPC interrupt handler .
*
* Parameters:
*  None
*
* Return:
*  None
*******************************************************************************/
void IpcNotifyInt_ISR(void)
{
    uint32_t u32Led = 0;
    /* Get all the enabled pending interrupts */
    uint32_t intr = Cy_IPC_Drv_GetInterruptStatusMasked(Cy_IPC_Drv_GetIntrBaseAddr(IPC_NOTIFY_INT_NUMBER));
    uint32_t interruptMasked = Cy_IPC_Drv_ExtractAcquireMask(intr);

    /* Check if the interrupt is caused by the notifier channel */
    if (interruptMasked == (1uL << USED_IPC_CHANNEL))
    {
        /* Clear the interrupt */
        Cy_IPC_Drv_ClearInterrupt(Cy_IPC_Drv_GetIntrBaseAddr(IPC_NOTIFY_INT_NUMBER),CY_IPC_NO_NOTIFICATION,interruptMasked);

        if(CY_IPC_DRV_SUCCESS == Cy_IPC_Drv_ReadMsgWord(Cy_IPC_Drv_GetIpcBaseAddress(USED_IPC_CHANNEL), &u32Led))
        {
            switch(u32Led)
            {
                case 0:
                    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_ON);   /* Led 1 On */
                    cyhal_gpio_write(CYBSP_USER_LED2, CYBSP_LED_STATE_OFF); /* Led 2 Off */
                    cyhal_gpio_write(CYBSP_USER_LED3, CYBSP_LED_STATE_OFF); /* Led 3 Off */
                    break;
                case 1:
                    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);  /* Led 1 Off */
                    cyhal_gpio_write(CYBSP_USER_LED2, CYBSP_LED_STATE_ON);  /* Led 2 On */
                    cyhal_gpio_write(CYBSP_USER_LED3, CYBSP_LED_STATE_OFF); /* Led 3 Off */
                    break;
                case 2:
                    cyhal_gpio_write(CYBSP_USER_LED, CYBSP_LED_STATE_OFF);  /* Led 1 Off */
                    cyhal_gpio_write(CYBSP_USER_LED2, CYBSP_LED_STATE_OFF); /* Led 2 Off */
                    cyhal_gpio_write(CYBSP_USER_LED3, CYBSP_LED_STATE_ON);  /* Led 3 On */
                    break;
                default:
                    break;
            }
        }
        /* Finally release the lock */
        Cy_IPC_Drv_ReleaseNotify(Cy_IPC_Drv_GetIpcBaseAddress(USED_IPC_CHANNEL), (1u << IPC_RELEASE_INT_NUMBER));
    }
}

/* [] END OF FILE */
