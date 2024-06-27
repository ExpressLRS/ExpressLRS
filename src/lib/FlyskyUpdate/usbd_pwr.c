/**
  ******************************************************************************
  * @file    usbd_pwr.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    31-January-2014
  * @brief   This file provides functions for power management
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "usbd_pwr.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
struct
{
  __IO RESUME_STATE eState;
  __IO uint8_t bESOFcnt;
}
ResumeS;

 __IO uint32_t remotewakeupon=0;
 
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Sets suspend mode operating conditions
  * @param  None
  * @retval USB_SUCCESS
  */
void Suspend(void)
{
  uint16_t wCNTR;

  /*Store CNTR value */
  wCNTR = _GetCNTR();   
  /* Set FSUSP bit in USB_CNTR register*/
  wCNTR |= CNTR_FSUSP;
  _SetCNTR(wCNTR);
  
  /* force low-power mode in the macrocell */
  wCNTR = _GetCNTR();
  wCNTR |= CNTR_LPMODE;
  _SetCNTR(wCNTR);
  
#ifdef USB_DEVICE_LOW_PWR_MGMT_SUPPORT
  
  /* enter system in STOP mode, only when wakeup flag in not set */
  if((_GetISTR()&ISTR_WKUP)==0)
  { 
    /*Enter STOP mode with SLEEPONEXIT*/
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_SLEEPONEXIT);
  }
  else
  {
    /* Clear Wakeup flag */
    _SetISTR(CLR_WKUP);
    /* clear FSUSP to abort entry in suspend mode  */
    wCNTR = _GetCNTR();
    wCNTR&=~CNTR_FSUSP;
    _SetCNTR(wCNTR);
  }
#endif
}

/**
  * @brief  Handles wake-up restoring normal operations
  * @param  None
  * @retval USB_SUCCESS
  */
void Resume_Init(void)
{
  uint16_t wCNTR;
  
  /* ------------------ ONLY WITH BUS-POWERED DEVICES ---------------------- */
  /* restart the clocks */
  /* ...  */

  /* CNTR_LPMODE = 0 */
  wCNTR = _GetCNTR();
  wCNTR &= (~CNTR_LPMODE);
  _SetCNTR(wCNTR);    
#ifdef USB_DEVICE_LOW_PWR_MGMT_SUPPORT   
  /* restore full power */
  /* ... on connected devices */
  Leave_LowPowerMode();
#endif
  /* reset FSUSP bit */
  _SetCNTR(IMR_MSK);

}


/**
  * @brief  Provides the state machine handling resume operations and
  *         timing sequence. The control is based on the Resume structure
  *         variables and on the ESOF interrupt calling this subroutine
  *         without changing machine state.
  * @param  a state machine value (RESUME_STATE)
  *         RESUME_ESOF doesn't change ResumeS.eState allowing
  *         decrementing of the ESOF counter in different states.
  * @retval Status
  */
void Resume(RESUME_STATE eResumeSetVal)
{
 uint16_t wCNTR;

  if (eResumeSetVal != RESUME_ESOF)
    ResumeS.eState = eResumeSetVal;
  switch (ResumeS.eState)
  {
    case RESUME_EXTERNAL:
      
if (remotewakeupon ==0)
      {
        Resume_Init();
        ResumeS.eState = RESUME_OFF;
      }
      else /* RESUME detected during the RemoteWAkeup signalling => keep RemoteWakeup handling*/
      {
        ResumeS.eState = RESUME_ON;
      }
      break;
    case RESUME_INTERNAL:
      Resume_Init();
      ResumeS.eState = RESUME_START;
      remotewakeupon = 1;
      break;
    case RESUME_LATER:
      ResumeS.bESOFcnt = 2;
      ResumeS.eState = RESUME_WAIT;
      break;
    case RESUME_WAIT:
      ResumeS.bESOFcnt--;
      if (ResumeS.bESOFcnt == 0)
        ResumeS.eState = RESUME_START;
      break;
    case RESUME_START:
      wCNTR = _GetCNTR();
      wCNTR |= CNTR_RESUME;
      _SetCNTR(wCNTR);
      ResumeS.eState = RESUME_ON;
      ResumeS.bESOFcnt = 10;
      break;
    case RESUME_ON:    
      ResumeS.bESOFcnt--;
      if (ResumeS.bESOFcnt == 0)
      {
        wCNTR = _GetCNTR();
        wCNTR &= (~CNTR_RESUME);
        _SetCNTR(wCNTR);
        ResumeS.eState = RESUME_OFF;
        remotewakeupon = 0;
      }
      break;
    case RESUME_OFF:
    case RESUME_ESOF:
    default:
      ResumeS.eState = RESUME_OFF;
      break;
  }
}

/**
  * @brief  Restores system clocks and power while exiting suspend mode
  * @param  None
  * @retval None
  */
void Leave_LowPowerMode(void)
{

}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
