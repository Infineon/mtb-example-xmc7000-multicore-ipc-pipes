/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for CM7_0 in the MultiCore IPC Pipes
*              Application for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2022-2024, Cypress Semiconductor Corporation (an Infineon company) or
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
#include "cybsp.h"

/****************************************************************************
* Constants
*****************************************************************************/
#define CY_IPC_MAX_ENDPOINTS            (2UL) /* 2 endpoints */
#define CY_IPC_CYPIPE_CLIENT_CNT        (8UL)

#define CY_IPC_CHAN_CYPIPE_EP0          (CY_IPC_CHAN_USER) /* IPC data channel for CYPIPE EP0 */
#define CY_IPC_CYPIPE_CHAN_MASK_EP0     (0x0001UL << CY_IPC_CHAN_CYPIPE_EP0)
#define CY_IPC_INTR_CYPIPE_EP0          (CY_IPC_INTR_USER)   /* IPC Intr for EP0 */
#define CY_IPC_CYPIPE_INTR_MASK_EP0     (0x0001UL << CY_IPC_INTR_CYPIPE_EP0)   /* IPC Intr Mask for EP0 */
#define CY_IPC_INTR_CYPIPE_PRIOR_EP0    (1UL)   /* Notifier Priority */
#define CY_IPC_INTR_CYPIPE_MUX_EP0      (NvicMux3_IRQn)   /* Intr Mux for CM0P */
#define CY_IPC_EP_CYPIPE_CM0_ADDR       (0UL)   /* EP0 Index of Endpoint Array */
#define CY_CLIENT_CYPIPE0_CM0_ID0       (0UL)   /* EP0 Pipe0 (CM0 <--> CM7_0) Index of client cb Array */

#define CY_IPC_CHAN_CYPIPE_EP1          (CY_IPC_CHAN_USER + 1UL) /* IPC data channel for CYPIPE EP1 */
#define CY_IPC_CYPIPE_CHAN_MASK_EP1     (0x0001UL << CY_IPC_CHAN_CYPIPE_EP1)
#define CY_IPC_INTR_CYPIPE_EP1          (CY_IPC_INTR_USER + 1UL)   /* IPC Intr for EP1 */
#define CY_IPC_CYPIPE_INTR_MASK_EP1     (0x0001UL << CY_IPC_INTR_CYPIPE_EP1)   /* IPC Intr Mask for EP0 */
#define CY_IPC_INTR_CYPIPE_PRIOR_EP1    (1UL)   /* Notifier Priority */
#define CY_IPC_INTR_CYPIPE_MUX_EP1      (NvicMux4_IRQn)   /* Intr Mux for CM7_0 */
#define CY_IPC_EP_CYPIPE_CM7_0_ADDR     (1UL)   /* EP1 Index of Endpoint Array */
#define CY_CLIENT_CYPIPE0_CM7_0_ID0     (2UL)   /* EP1 Pipe0 (CM7_0 <--> CM0) Index of client cb Array */


/****************************************************************************
* The pipe configuration defines the IPC channel number, interrupt
* number, and the pipe interrupt mask for the endpoint.
*
* The format of the endPoint configuration
*   Bits[31:16] Interrupt Mask
*   Bits[15:8 ] IPC interrupt
*   Bits[ 7:0 ] IPC channel
*
*   Pipe addresses
*   CyPipe defines
*****************************************************************************/
#define CY_IPC_CYPIPE_INTR_MASK   ( CY_IPC_CYPIPE_CHAN_MASK_EP0 | CY_IPC_CYPIPE_CHAN_MASK_EP1)

#define CY_IPC_CYPIPE_CONFIG_EP0  ( (CY_IPC_CYPIPE_INTR_MASK << CY_IPC_PIPE_CFG_IMASK_Pos) \
                                   | (CY_IPC_INTR_CYPIPE_EP0 << CY_IPC_PIPE_CFG_INTR_Pos) \
                                    | CY_IPC_CHAN_CYPIPE_EP0)

#define CY_IPC_CYPIPE_CONFIG_EP1  ( (CY_IPC_CYPIPE_INTR_MASK << CY_IPC_PIPE_CFG_IMASK_Pos) \
                                   | (CY_IPC_INTR_CYPIPE_EP1 << CY_IPC_PIPE_CFG_INTR_Pos) \
                                    | CY_IPC_CHAN_CYPIPE_EP1)

/****************************************************************************
* Global variables
*****************************************************************************/
typedef struct
{
    uint8_t  clientID;      /* Client ID */
    uint8_t  pktType;       /* Message Type */
    uint16_t intrRelMask;   /* Mask */
} cy_stc_ipc_testmsg_t;

typedef enum
{
    CY_IPC_PKT_FROM_CM0_TO_CM7_0,
    CY_IPC_PKT_FROM_CM7_0_TO_CM0,
} cy_en_ipc_pktType_t;


/*******************************************************************************
* Function Prototypes
********************************************************************************/
void Pipe0_cm7_0_ReleaseCallback(void);
void Cy_SysIpcPipeIsrCm7_0(void);
void handle_error(void);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for the CM7_0 core.
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
    uint32_t  u32Led= 0;
    cy_rslt_t result;
    cy_en_ipc_pipe_status_t pipeStatus;
    cy_stc_ipc_testmsg_t cm7_0MsgData0;
    static cy_stc_ipc_pipe_ep_t IpcPipeEpArray[CY_IPC_MAX_ENDPOINTS]; /* Create an array of endpoint structures */
    static cy_ipc_pipe_callback_ptr_t ep1CbArray[CY_IPC_CYPIPE_CLIENT_CNT]; /* CB Array for EP1 */

    /* Pipe-0 endpoint-1 and endpoint-0. CM7_0 <--> CM0 */
    static const cy_stc_ipc_pipe_config_t systemIpcPipe0ConfigCm7_0 =
    {
        /* receiver endpoint CM7_0 */
        {
            CY_IPC_INTR_CYPIPE_EP1,       /* .ipcNotifierNumber    */
            CY_IPC_INTR_CYPIPE_PRIOR_EP1, /* .ipcNotifierPriority  */
            CY_IPC_INTR_CYPIPE_MUX_EP1,   /* .ipcNotifierMuxNumber */
            CY_IPC_EP_CYPIPE_CM7_0_ADDR,  /* .epAddress            */
            CY_IPC_CYPIPE_CONFIG_EP1      /* .epConfig             */
        },
        /* sender endpoint CM0 */
        {
            CY_IPC_INTR_CYPIPE_EP0,       /* .ipcNotifierNumber    */
            CY_IPC_INTR_CYPIPE_PRIOR_EP0, /* .ipcNotifierPriority  */
            CY_IPC_INTR_CYPIPE_MUX_EP0,   /* .ipcNotifierMuxNumber */
            CY_IPC_EP_CYPIPE_CM0_ADDR,    /* .epAddress            */
            CY_IPC_CYPIPE_CONFIG_EP0      /* .epConfig             */
        },

        CY_IPC_CYPIPE_CLIENT_CNT, /* .endpointClientsCount     */
        ep1CbArray,               /* .endpointsCallbacksArray  */
        &Cy_SysIpcPipeIsrCm7_0    /* .userPipeIsrHandler       */
    };

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    Cy_IPC_Pipe_Config(IpcPipeEpArray);


    Cy_IPC_Pipe_Init(&systemIpcPipe0ConfigCm7_0); /* PIPE-0 EP1 <--> EP0 */

    for (;;)
    {
        /* Wait 0.5 [s] */
        Cy_SysLib_Delay(500);
        u32Led = (u32Led + 1u) % 3u;
        /* Send message to CM0 in Pipe-0 */
        cm7_0MsgData0.clientID = CY_CLIENT_CYPIPE0_CM0_ID0;      /* Client CM0_ID0 will process this message */
        cm7_0MsgData0.pktType = u32Led;
        cm7_0MsgData0.intrRelMask = CY_IPC_CYPIPE_INTR_MASK_EP1; /* set the interrupt mask for EP1 interrupt number */

        pipeStatus = Cy_IPC_Pipe_SendMessage(CY_IPC_EP_CYPIPE_CM0_ADDR, CY_IPC_EP_CYPIPE_CM7_0_ADDR, (void *) &cm7_0MsgData0, Pipe0_cm7_0_ReleaseCallback);
        if (pipeStatus !=CY_IPC_PIPE_SUCCESS)
        {
            handle_error();
        }
    }
}

/*******************************************************************************
* Function Name: Pipe0_cm7_0_ReleaseCallback
********************************************************************************
* Summary:
* The sent data is already processed on the receiver side,the pipe is ready for
* next transactions.
*
* Parameters:
*  None
*
* Return:
*  None
*******************************************************************************/
void Pipe0_cm7_0_ReleaseCallback(void)
{

}

/*******************************************************************************
* Function Name: Cy_SysIpcPipeIsrCm7_0
********************************************************************************
* Summary:
* This is the interrupt service routine for the pipe.
*
* Parameters:
*  None
*
* Return:
*  None
*******************************************************************************/
void Cy_SysIpcPipeIsrCm7_0(void)
{
    Cy_IPC_Pipe_ExecuteCallback(CY_IPC_EP_CYPIPE_CM7_0_ADDR);
}

/*******************************************************************************
* Function Name: handle_error
********************************************************************************
* Summary:
* This function processes unrecoverable errors.
* In case of such error the system will stay in an infinite loop of this function.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void handle_error(void)
{
     /* Disable all interrupts */
    __disable_irq();

    /* Halt the CPU */
    CY_ASSERT(0);
}

/* [] END OF FILE */
