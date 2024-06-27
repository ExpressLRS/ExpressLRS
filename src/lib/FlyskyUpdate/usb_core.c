/**
  ******************************************************************************
  * @file    usb_core.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   This file provides the interface functions to USB cell registers
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
#include "usb_core.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief Set the CNTR register value 
  * @param   wRegValue: new register value
  * @retval None
  */
void SetCNTR(uint16_t wRegValue)
{
  _SetCNTR(wRegValue);
}

/**
  * @brief returns the CNTR register value 
  * @param   None
  * @retval CNTR register Value
  */
uint16_t GetCNTR(void)
{
  return(_GetCNTR());
}

/**
  * @brief Set the ISTR register value 
  * @param   wRegValue: new register value
  * @retval None
  */
void SetISTR(uint16_t wRegValue)
{
  _SetISTR(wRegValue);
}

/**
  * @brief Returns the ISTR register value 
  * @param   None
  * @retval ISTR register Value
  */
uint16_t GetISTR(void)
{
  return(_GetISTR());
}

/**
  * @brief Returns the FNR register value
  * @param   None
  * @retval FNR register Value
  */
uint16_t GetFNR(void)
{
  return(_GetFNR());
}

/**
  * @brief Set the DADDR register value
  * @param   wRegValue: new register value
  * @retval None
  */
void SetDADDR(uint16_t wRegValue)
{
  _SetDADDR(wRegValue);
}

/**
  * @brief Set the LPMCSR register value
  * @param   wRegValue: new register value
  * @retval None
  */
void SetLPMCSR(uint16_t wRegValue)
{
  _SetLPMCSR(wRegValue);
}


/**
  * @brief Returns the LPMCSR register value
  * @param   None
  * @retval LPMCSR register Value
  */
uint16_t GetLPMCSR(void)
{
  return(_GetLPMCSR());
}

/**
  * @brief Returns the DADDR register value
  * @param   None
  * @retval DADDR register Value
  */
uint16_t GetDADDR(void)
{
  return(_GetDADDR());
}

/**
  * @brief Set the BTABLE.
  * @param   wRegValue: New register value
  * @retval None
  */
void SetBTABLE(uint16_t wRegValue)
{
  _SetBTABLE(wRegValue);
}

/**
  * @brief Returns the BTABLE register value.
  * @param   None
  * @retval BTABLE address
  */
uint16_t GetBTABLE(void)
{
  return(_GetBTABLE());
}

/**
  * @brief Set the Endpoint register value.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void SetENDPOINT(uint8_t bEpNum, uint16_t wRegValue)
{
  _SetENDPOINT(bEpNum, wRegValue);
}

/**
  * @brief Return the Endpoint register value.
  * @param   bEpNum: Endpoint Number
  * @retval Endpoint register value.
  */
uint16_t GetENDPOINT(uint8_t bEpNum)
{
  return(_GetENDPOINT(bEpNum));
}

/**
  * @brief sets the type in the endpoint register.
  * @param   bEpNum: Endpoint Number
  * @param   wType: type definition
  * @retval None.
  */
void SetEPType(uint8_t bEpNum, uint16_t wType)
{
  _SetEPType(bEpNum, wType);
}

/**
  * @brief Returns the endpoint type.
  * @param   bEpNum: Endpoint Number
  * @retval Endpoint Type.
  */
uint16_t GetEPType(uint8_t bEpNum)
{
  return(_GetEPType(bEpNum));
}

/**
  * @brief Set the status of Tx endpoint.
  * @param   bEpNum: Endpoint Number
  * @param   wState: new state.
  * @retval None
  */
void SetEPTxStatus(uint8_t bEpNum, uint16_t wState)
{
  _SetEPTxStatus(bEpNum, wState);   
}

/**
  * @brief Set the status of Rx endpoint.
  * @param   bEpNum: Endpoint Number
  * @param   wState: new state.
  * @retval None
  */
void SetEPRxStatus(uint8_t bEpNum, uint16_t wState)
{
  _SetEPRxStatus(bEpNum, wState);
}

/**
  * @brief sets the status for Double Buffer Endpoint to STALL
  * @param   bEpNum: Endpoint Number
  * @param   bDir: Endpoint direction
  * @retval None
  */
void SetDouBleBuffEPStall(uint8_t bEpNum, uint8_t bDir)
{
  uint16_t Endpoint_DTOG_Status;
  Endpoint_DTOG_Status = GetENDPOINT(bEpNum);
  if (bDir == EP_DBUF_OUT)
  { /* OUT double buffered endpoint */
    _SetENDPOINT(bEpNum, Endpoint_DTOG_Status & ~EPRX_DTOG1);
  }
  else if (bDir == EP_DBUF_IN)
  { /* IN double buffered endpoint */
    _SetENDPOINT(bEpNum, Endpoint_DTOG_Status & ~EPTX_DTOG1);
  }
}

/**
  * @brief Returns the endpoint Tx status
  * @param   bEpNum: Endpoint Number
  * @retval Endpoint TX Status
  */
uint16_t GetEPTxStatus(uint8_t bEpNum)
{
  return(_GetEPTxStatus(bEpNum));
}

/**
  * @brief Returns the endpoint Rx status
  * @param   bEpNum: Endpoint Number
  * @retval Endpoint Endpoint RX Status
  */
uint16_t GetEPRxStatus(uint8_t bEpNum)
{
  return(_GetEPRxStatus(bEpNum));
}

/**
  * @brief Valid the endpoint Tx Status.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void SetEPTxValid(uint8_t bEpNum)
{
  _SetEPTxStatus(bEpNum, EP_TX_VALID);
}

/**
  * @brief Valid the endpoint Rx Status.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void SetEPRxValid(uint8_t bEpNum)
{
  _SetEPRxStatus(bEpNum, EP_RX_VALID);
}

/**
  * @brief set the  EP_KIND bit.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void SetEP_KIND(uint8_t bEpNum)
{
  _SetEP_KIND(bEpNum);
}

/**
  * @brief ClearEP_KIND
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ClearEP_KIND(uint8_t bEpNum)
{
  _ClearEP_KIND(bEpNum);
}

/**
  * @brief Clear the Status Out of the related Endpoint
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void Clear_Status_Out(uint8_t bEpNum)
{
  _ClearEP_KIND(bEpNum);
}

/**
  * @brief Set the Status Out of the related Endpoint
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void Set_Status_Out(uint8_t bEpNum)
{
  _SetEP_KIND(bEpNum);
}

/**
  * @brief Enable the double buffer feature for the endpoint.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void SetEPDoubleBuff(uint8_t bEpNum)
{
  _SetEP_KIND(bEpNum);
}

/**
  * @brief Disable the double buffer feature for the endpoint
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ClearEPDoubleBuff(uint8_t bEpNum)
{
  _ClearEP_KIND(bEpNum);
}

/**
  * @brief Returns the Stall status of the Tx endpoint
  * @param   bEpNum: Endpoint Number
  * @retval Tx Stall status
  */
uint16_t GetTxStallStatus(uint8_t bEpNum)
{
  return(_GetTxStallStatus(bEpNum));
}

/**
  * @brief Returns the Stall status of the Rx endpoint
  * @param   bEpNum: Endpoint Number
  * @retval Rx Stall status.
  */
uint16_t GetRxStallStatus(uint8_t bEpNum)
{
  return(_GetRxStallStatus(bEpNum));
}

/**
  * @brief Returns the Stall status of the Rx endpoint
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ClearEP_CTR_RX(uint8_t bEpNum)
{
  _ClearEP_CTR_RX(bEpNum);
}

/**
  * @brief Clear the CTR_TX bit
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ClearEP_CTR_TX(uint8_t bEpNum)
{
  _ClearEP_CTR_TX(bEpNum);
}

/**
  * @brief Toggle the DTOG_RX bit.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ToggleDTOG_RX(uint8_t bEpNum)
{
  _ToggleDTOG_RX(bEpNum);
}

/**
  * @brief Toggle the DTOG_TX bit.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ToggleDTOG_TX(uint8_t bEpNum)
{
  _ToggleDTOG_TX(bEpNum);
}

/**
  * @brief Clear the DTOG_RX bit.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ClearDTOG_RX(uint8_t bEpNum)
{
  _ClearDTOG_RX(bEpNum);
}

/**
  * @brief Clear the DTOG_TX bit.
  * @param   bEpNum: Endpoint Number
  * @retval None
  */
void ClearDTOG_TX(uint8_t bEpNum)
{
  _ClearDTOG_TX(bEpNum);
}

/**
  * @brief Set the endpoint address.
  * @param   bEpNum: Endpoint Number
  * @param   bAddr: New endpoint address.
  * @retval None
  */
void SetEPAddress(uint8_t bEpNum, uint8_t bAddr)
{
  _SetEPAddress(bEpNum, bAddr);
}

/**
  * @brief Get the endpoint address.
  * @param   bEpNum: Endpoint Number
  * @retval Endpoint address.
  */
uint8_t GetEPAddress(uint8_t bEpNum)
{
  return(_GetEPAddress(bEpNum));
}

/**
  * @brief Set the endpoint Tx buffer address.
  * @param   bEpNum: Endpoint Number
  * @param   wAddr: new address.
  * @retval Endpoint address.
  */
void SetEPTxAddr(uint8_t bEpNum, uint16_t wAddr)
{
  _SetEPTxAddr(bEpNum, wAddr);
}

/**
  * @brief Set the endpoint Rx buffer address.
  * @param   bEpNum: Endpoint Number
  * @param   wAddr: new address.
  * @retval Endpoint address.
  */
void SetEPRxAddr(uint8_t bEpNum, uint16_t wAddr)
{
  _SetEPRxAddr(bEpNum, wAddr);
}

/**
  * @brief Returns the endpoint Tx buffer address.
  * @param   bEpNum: Endpoint Number
  * @retval Tx buffer address.
  */
uint16_t GetEPTxAddr(uint8_t bEpNum)
{
  return(_GetEPTxAddr(bEpNum));
}

/**
  * @brief Returns the endpoint Rx buffer address.
  * @param   bEpNum: Endpoint Number
  * @retval Rx buffer address.
  */
uint16_t GetEPRxAddr(uint8_t bEpNum)
{
  return(_GetEPRxAddr(bEpNum));
}

/**
  * @brief Set the Tx count.
  * @param   bEpNum: Endpoint Number
  * @param   wCount: new count value.
  * @retval Rx buffer address.
  */
void SetEPTxCount(uint8_t bEpNum, uint16_t wCount)
{
  _SetEPTxCount(bEpNum, wCount);
}

/**
  * @brief Set the Count Rx Register value.
  * @param   *pdwReg: point to the register.
  * @param   wCount: the new register value.
  * @retval None
  */
void SetEPCountRxReg(uint16_t *pdwReg, uint16_t wCount)
{
  _SetEPCountRxReg(pdwReg, wCount);
}

/**
  * @brief Set the Rx count.
  * @param   bEpNum: Endpoint Number.
  * @param   wCount: the new register value.
  * @retval None
  */
void SetEPRxCount(uint8_t bEpNum, uint16_t wCount)
{
  _SetEPRxCount(bEpNum, wCount);
}

/**
  * @brief Get the Tx count.
  * @param   bEpNum: Endpoint Number.
  * @retval Tx count value.
  */
uint16_t GetEPTxCount(uint8_t bEpNum)
{
  return(_GetEPTxCount(bEpNum));
}

/**
  * @brief Get the Rx count.
  * @param   bEpNum: Endpoint Number.
  * @retval Rx count value.
  */
uint16_t GetEPRxCount(uint8_t bEpNum)
{
  return(_GetEPRxCount(bEpNum));
}

/**
  * @brief Set the addresses of the buffer 0 and 1.
  * @param   bEpNum: Endpoint Number.
  * @param   wBuf0Addr: new address of buffer 0.
  * @param   wBuf1Addr: new address of buffer 1.
  * @retval None
  */
void SetEPDblBuffAddr(uint8_t bEpNum, uint16_t wBuf0Addr, uint16_t wBuf1Addr)
{
  _SetEPDblBuffAddr(bEpNum, wBuf0Addr, wBuf1Addr);
}

/**
  * @brief Set the Buffer 1 address.
  * @param   bEpNum: Endpoint Number.
  * @param   wBuf0Addr: new address.
  * @retval None
  */
void SetEPDblBuf0Addr(uint8_t bEpNum, uint16_t wBuf0Addr)
{
  _SetEPDblBuf0Addr(bEpNum, wBuf0Addr);
}

/**
  * @brief Set the Buffer 1 address.
  * @param   bEpNum: Endpoint Number.
  * @param   wBuf1Addr: new address.
  * @retval None
  */
void SetEPDblBuf1Addr(uint8_t bEpNum, uint16_t wBuf1Addr)
{
  _SetEPDblBuf1Addr(bEpNum, wBuf1Addr);
}

/**
  * @brief Returns the address of the Buffer 0.
  * @param   bEpNum: Endpoint Number.
  * @retval None
  */
uint16_t GetEPDblBuf0Addr(uint8_t bEpNum)
{
  return(_GetEPDblBuf0Addr(bEpNum));
}

/**
  * @brief Returns the address of the Buffer 1.
  * @param   bEpNum: Endpoint Number.
  * @retval Address of the Buffer 1.
  */
uint16_t GetEPDblBuf1Addr(uint8_t bEpNum)
{
  return(_GetEPDblBuf1Addr(bEpNum));
}

/**
  * @brief Set the number of bytes for a double Buffer
  * @param   bEpNum,bDir, wCount
  * @retval Address of the Buffer 1.
  */
void SetEPDblBuffCount(uint8_t bEpNum, uint8_t bDir, uint16_t wCount)
{
  _SetEPDblBuffCount(bEpNum, bDir, wCount);
}

/**
  * @brief Set the number of bytes in the buffer 0 of a double Buffer endpoint.
  * @param   bEpNum,bDir, wCount
  * @retval None
  */
void SetEPDblBuf0Count(uint8_t bEpNum, uint8_t bDir, uint16_t wCount)
{
  _SetEPDblBuf0Count(bEpNum, bDir, wCount);
}

/**
  * @brief Set the number of bytes in the buffer 0 of a double Buffer
  * @param   bEpNum,bDir, wCount
  * @retval None
  */
void SetEPDblBuf1Count(uint8_t bEpNum, uint8_t bDir, uint16_t wCount)
{
  _SetEPDblBuf1Count(bEpNum, bDir, wCount);
}

/**
  * @brief Returns the number of byte received in the buffer 0 of a double 
  *         Buffer endpoint.
  * @param   bEpNum: Endpoint Number.
  * @retval Endpoint Buffer 0 count
  */
uint16_t GetEPDblBuf0Count(uint8_t bEpNum)
{
  return(_GetEPDblBuf0Count(bEpNum));
}

/**
  * @brief Returns the number of data received in the buffer 1 of a double
  * @param   bEpNum: Endpoint Number.
  * @retval Endpoint Buffer 1 count
  */
uint16_t GetEPDblBuf1Count(uint8_t bEpNum)
{
  return(_GetEPDblBuf1Count(bEpNum));
}

/**
  * @brief gets direction of the double buffered endpoint
  * @param   bEpNum: Endpoint Number.
  * @retval EP_DBUF_OUT, EP_DBUF_IN,
  *         EP_DBUF_ERR if the endpoint counter not yet programmed.
  */
EP_DBUF_DIR GetEPDblBufDir(uint8_t bEpNum)
{
  if ((uint16_t)(*_pEPRxCount(bEpNum) & 0xFC00) != 0)
    return(EP_DBUF_OUT);
  else if (((uint16_t)(*_pEPTxCount(bEpNum)) & 0x03FF) != 0)
    return(EP_DBUF_IN);
  else
    return(EP_DBUF_ERR);
}

/**
  * @brief free buffer used from the application realizing it to the line
          toggles bit SW_BUF in the double buffered endpoint register
  * @param   bEpNum, bDir
  * @retval None
  */
void FreeUserBuffer(uint8_t bEpNum, uint8_t bDir)
{
  if (bDir == EP_DBUF_OUT)
  { /* OUT double buffered endpoint */
    _ToggleDTOG_TX(bEpNum);
  }
  else if (bDir == EP_DBUF_IN)
  { /* IN double buffered endpoint */
    _ToggleDTOG_RX(bEpNum);
  }
}

/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA)
  * @param   pbUsrBuf: pointer to user memory area.
  * @param   wPMABufAddr: address into PMA.
  * @param   wNBytes: no. of bytes to be copied.
  * @retval None
  */
void UserToPMABufferCopy(uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
  uint32_t n = (wNBytes + 1) >> 1; 
  uint32_t i;
  uint16_t temp1, temp2;
  uint16_t *pdwVal;
  pdwVal = (uint16_t *)(wPMABufAddr + PMAAddr);
  
  for (i = n; i != 0; i--)
  {
    temp1 = (uint16_t) * pbUsrBuf;
    pbUsrBuf++;
    temp2 = temp1 | (uint16_t) * pbUsrBuf << 8;
    *pdwVal++ = temp2;
    pbUsrBuf++;
  }
}

/**
  * @brief Copy a buffer from user memory area to packet memory area (PMA)
  * @param   pbUsrBuf    = pointer to user memory area.
  * @param   wPMABufAddr: address into PMA.
  * @param   wNBytes: no. of bytes to be copied.
  * @retval None
  */
void PMAToUserBufferCopy(uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
  uint32_t n = (wNBytes + 1) >> 1;
  uint32_t i;
  uint16_t *pdwVal;
  pdwVal = (uint16_t *)(wPMABufAddr + PMAAddr);
  for (i = n; i != 0; i--)
  {
    *(uint16_t*)pbUsrBuf++ = *pdwVal++;
    pbUsrBuf++;
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
