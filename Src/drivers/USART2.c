#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"
#include "stm32f4xx.h"



static bool txCompleted = true;
static bool new_data = false;
static uint8_t DMA_rx_buffer[USART2_RX_BUFFER_SIZE];
static uint8_t *_rx_buffer;
static uint16_t receivedBytes = 0;
static bool shouldRxStop = false;

static uint16_t skippedFrames = 0;

bool USART2_NewData(void){
    bool ret = new_data;
    new_data=false;
    return ret;
}
uint16_t USART2_GetReceivedBytes(void){
    uint16_t ret = receivedBytes;
    receivedBytes = 0;
    return ret;
}

uint16_t USART2_GetSkippedFrames(void){
    return skippedFrames;
}

bool USART2_Check_Tx_end(void){
    return txCompleted;
}

void USART2_Transmit_DMA(uint8_t* tx_buffer, uint16_t len){
    DMA1_Stream6->CR&=~(DMA_SxCR_EN);
    txCompleted = false;

    DMA1_Stream6->M0AR = (uint32_t)tx_buffer;
    DMA1_Stream6->NDTR = len;
    DMA1_Stream6->CR |= DMA_SxCR_EN;
}

void USART2_Receive_DMA(uint8_t* rx_buffer){
    shouldRxStop = false;
    DMA1_Stream5->CR&= ~(DMA_SxCR_EN);      //disable dma rx
    while(DMA1_Stream5->CR&DMA_SxCR_EN);    //wait for it
    _rx_buffer = rx_buffer;
    DMA1_Stream5->M0AR = (uint32_t)DMA_rx_buffer;
    DMA1_Stream5->NDTR = USART2_RX_BUFFER_SIZE; 
    DMA1_Stream5->CR |= DMA_SxCR_EN;   
}
void USART2_StopReceiving(void){
    DMA1_Stream5->CR&= ~(DMA_SxCR_EN); 
    USART2->SR = 0;
    shouldRxStop = true;
}

void USART2_IRQHandler(void)
{
  if (USART2->SR & USART_SR_IDLE)
    {
        USART2->DR;                             //If not read usart willl crush                  
        DMA1_Stream5->CR &= ~DMA_SxCR_EN;       /* Disable DMA on stream 5 - trigers dma TC */
    }  

}
void DMA1_Stream5_IRQHandler(void)
{
    if(DMA1->HISR & DMA_HISR_TCIF5){            //if interupt is TC
        DMA1->HIFCR = DMA_HIFCR_CTCIF5;         //clear tc flag
         new_data = true;
        if(receivedBytes!=0)                    //check if bytes were readed
            skippedFrames++;
        receivedBytes = USART2_RX_BUFFER_SIZE - DMA1_Stream5->NDTR;    //we expected USART2_RX_BUFFER_SIZE NDTR keeps how many bytes left to transfe
        memcpy(_rx_buffer,DMA_rx_buffer, receivedBytes);
        USART2_RC_Complete_Callback();
        memset(DMA_rx_buffer, 0, sizeof(DMA_rx_buffer));
        DMA1_Stream5->M0AR = (uint32_t)DMA_rx_buffer;                   //start new transfer
        DMA1_Stream5->NDTR = USART2_RX_BUFFER_SIZE;
        if(!shouldRxStop){
            DMA1_Stream5->CR |= DMA_SxCR_EN;    
        }                          
    }
}
void DMA1_Stream6_IRQHandler(void)
{
    if(DMA1->HISR & DMA_HISR_TCIF6){ //if interupt is TC
        txCompleted = true;
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;     //clear tc flag
    }
}