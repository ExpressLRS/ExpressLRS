// #include "debug.h"

// void BeginFastSync()
// { //sync and get lock to reciever
//   DEBUG_PRINTLN("Begin fastsync");
//   uint8_t RXdata[7];
//   Radio.StopTimerTask();
//   SetRFLinkRate(RF_RATE_50HZ);
//   Radio.SetFrequency(GetInitialFreq());

//   // Radio.TXdataBuffer[0] = PacketHeaderAddr;
//   // Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
//   // Radio.TXdataBuffer[2] = (Radio.NonceTX << 4) + (ExpressLRS_currAirRate.enum_rate & 0b1111);
//   // Radio.TXdataBuffer[3] = TxBaseMac[3];
//   // Radio.TXdataBuffer[4] = TxBaseMac[4];
//   // Radio.TXdataBuffer[5] = TxBaseMac[5];

//   GenerateSyncPacketData();
//   Radio.TXdataBuffer[3] = ExpressLRS_prevAirRate.enum_rate;
//   Radio.TXdataBuffer[4] = 1;
//   uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
//   Radio.TXdataBuffer[6] = crc;

//   while (1)
//   {
//     Radio.SetFrequency(FHSSgetNextFreq());
//     Radio.TX((uint8_t *)Radio.TXdataBuffer, 7);
//     //vTaskDelay(1);
//     if (Radio.RXsingle(RXdata, 7, 2 * (RF_RATE_50HZ.interval / 1000)) == ERR_NONE)
//     {
//       DEBUG_PRINTLN("got fastsync resp 1");
//       break;
//     }
//   }

//   uint8_t calculatedCRC = CalcCRC(RXdata, 6);
//   uint8_t inCRC = RXdata[6];
//   uint8_t type = RXdata[0] & 0b11;
//   uint8_t packetAddr = (RXdata[0] & 0b11111100) >> 2;

//   if (packetAddr == DeviceAddr)
//   {
//     if ((inCRC == calculatedCRC))
//     {
//       if (type == 0b10) //sync packet
//       {
//         if (RXdata[4] == 2)
//         {
//           DEBUG_PRINTLN("got SYNACK"); // got SYNACK
//           LastTLMpacketRecvMillis = millis();
//           GenerateSyncPacketData();

//           Radio.TXdataBuffer[3] = ExpressLRS_prevAirRate.enum_rate;
//           Radio.TXdataBuffer[4] = 3;
//           uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
//           Radio.TXdataBuffer[6] = crc;
//           Radio.TX((uint8_t *)Radio.TXdataBuffer, 7);
//           SetRFLinkRate(ExpressLRS_prevAirRate);
//           Radio.StartTimerTask();
//           DEBUG_PRINTLN("fastsync done!");
//         }
//       }
//     }
//   }
//   else
//   {
//     BeginFastSync();
//   }
// }


//esp8266 side
 
// if (millis() > (LastValidPacket + LostConnectionDelay))
// {
//     if (LostConnection)
//     {
//         DEBUG_PRINTLN("begin fastsync");
//         gotFHSSsync = false;
//         StopHWtimer();
//         Radio.SetFrequency(GetInitialFreq());

//         switch (scanIndex)
//         {
//         case 1:
//             SetRFLinkRate(RF_RATE_200HZ);
//             break;
//         case 2:
//             SetRFLinkRate(RF_RATE_100HZ);
//             break;
//         case 3:
//             SetRFLinkRate(RF_RATE_50HZ);
//             break;

//         default:
//             break;
//         }

//         if (Radio.RXsingle((uint8_t *)Radio.RXdataBuffer, 7, 1000) == !ERR_NONE)
//         {
//             return;
//         }

//         digitalWrite(16, LED);
//         LED = !LED;

//         if (scanIndex == 3)
//         {
//             scanIndex = 1;
//         }
//         else
//         {

//             scanIndex++;
//         }

//         uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 6);
//         uint8_t inCRC = Radio.RXdataBuffer[6];
//         uint8_t type = Radio.RXdataBuffer[0] & 0b11;
//         uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

//         if (packetAddr == DeviceAddr)
//         {
//             if ((inCRC == calculatedCRC))
//             {
//                 if (type == 0b10) //sync packet
//                 {
//                     switch (Radio.RXdataBuffer[4])
//                     {
//                     case 0: // normal sync packet
//                         DEBUG_PRINTLN("Regular sync packet");
//                         ProcessRFPacket();
//                         break;

//                     case 1: // fastsync packet
//                         DEBUG_PRINTLN("Fastsync packet");

//                         FHSSsetCurrIndex(Radio.RXdataBuffer[1]);
//                         NonceRXlocal = Radio.RXdataBuffer[2];

//                         DEBUG_PRINTLN("Sending Resp. stage 2");
//                         GenerateSyncPacketData();
//                         Radio.TXdataBuffer[4] = 2;
//                         Radio.TXdataBuffer[6] = CalcCRC(Radio.TXdataBuffer, 6);
//                         Radio.TX((uint8_t *)Radio.TXdataBuffer, 7);
//                         InitHarwareTimer();

//                         if (Radio.RXsingle((uint8_t *)Radio.RXdataBuffer, 7, 2 * (RF_RATE_50HZ.interval / 1000)) == !ERR_RX_TIMEOUT)
//                         {
//                             DEBUG_PRINTLN("Got Resp. Stage 3");
//                             uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 6);
//                             uint8_t inCRC = Radio.RXdataBuffer[6];
//                             uint8_t type = Radio.RXdataBuffer[0] & 0b11;
//                             uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

//                             if (Radio.RXdataBuffer[4] == 3)
//                             {
//                                 DEBUG_PRINTLN("fastsync done!");
//                                 LastValidPacket = millis();
//                                 LostConnection = false;
//                             }
//                             break;
//                         }
//                         else
//                         {
//                             return;
//                         }

//                     default:
//                         break;
//                     }
//                     //DEBUG_PRINTLN("update rate");
//                     switch (Radio.RXdataBuffer[3])
//                     {
//                     case 0:
//                         SetRFLinkRate(RF_RATE_200HZ);
//                         ExpressLRS_currAirRate = RF_RATE_200HZ;
//                         DEBUG_PRINTLN("update rate 200hz");
//                         break;
//                     case 1:
//                         SetRFLinkRate(RF_RATE_100HZ);
//                         ExpressLRS_currAirRate = RF_RATE_100HZ;
//                         DEBUG_PRINTLN("update rate 100hz");
//                         break;
//                     case 2:
//                         SetRFLinkRate(RF_RATE_50HZ);
//                         ExpressLRS_currAirRate = RF_RATE_50HZ;
//                         DEBUG_PRINTLN("update rate 50hz");
//                         break;
//                     default:
//                         break;
//                     }
//                     digitalWrite(16, 1);
//                 }
//             }
//         }
//     }
// }

// if (offset == 3)
// {
//     offset = -3;
// }
// else
// {

//     offset++;
// }
// DEBUG_PRINTLN(offset);
// delay(10000);