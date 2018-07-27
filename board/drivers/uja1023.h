#define TICKS_PER_FRAME 66000 //1s = 33000

#ifdef PANDA
int lin_send_timer = TICKS_PER_FRAME;

LIN_FRAME_t assign_id_frame, io_cfg_1_frame, io_cfg_2_frame, io_cfg_3_frame, io_cfg_4_frame, px_req_frame, frame_to_send;


int initial_loop_num = 0;


void TIM4_IRQHandler(void) {
  if (TIM4->SR & TIM_SR_UIF) {
    if (lin_send_timer == 0) {
      //send every 1s
      lin_send_timer = TICKS_PER_FRAME;
      if (initial_loop_num < 3) {
        switch(initial_loop_num) {
        case 0 :
          frame_to_send = assign_id_frame;
          break;
        case 1 :
          frame_to_send = io_cfg_1_frame;
          break;
        case 2 :
          frame_to_send = io_cfg_2_frame;
          break;
        case 3 :
          frame_to_send = io_cfg_3_frame;
          break;
        }
        initial_loop_num++;
      }
      else {
        
        /*
        if (px_req_frame.data[0] == 0x02) {
          px_req_frame.data[0] = 0x00;
        }
        else {
          px_req_frame.data[0] = 0x02;
        }
        */
        frame_to_send = px_req_frame;
      }
      LIN_SendData(&lin1_ring, &frame_to_send);
    } //if counter = 0
    
    if (lin_send_timer == TICKS_PER_FRAME - 2000 && frame_to_send.has_response) {
      //receive frameID response
      LIN_FRAME_t frame_to_receive;
      frame_to_receive.data_len = 8;
      frame_to_receive.frame_id = 0x7D; //Response PID, 0x7D = 2 bit parity + 0x3C raw ID
  
      LIN_ReceiveData(&lin1_ring, &frame_to_receive);
      puts("Received Lin frame: ");
      for(int n=0; n < frame_to_receive.data_len; n++) {
        puth(frame_to_receive.data[n]);
      }
      puts("\n");
      
    } //receive every 1s, .5secs apart from the send
    
  lin_send_timer--;
  } //interrupt
  TIM4->SR = 0;
}

void uja1023_init(void) {  
  //make frame for Assign frame ID
  //LIN_FRAME_t assign_id_frame;
  assign_id_frame.has_response = 1;
  assign_id_frame.data_len = 8;
  assign_id_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  assign_id_frame.data[0]  = 0x60; //D0, iniital node address, set to 0x60 default
  assign_id_frame.data[1]  = 0x06; //D1, protocol control info (PCI); should be 0x06
  assign_id_frame.data[2]  = 0xB1; //D2, service id (SID); should be 0xB4
  assign_id_frame.data[3]  = 0x11; //D3, supplier ID low byte; should be 0x11
  assign_id_frame.data[4]  = 0x00; //D4, supplier ID high byte; should be 0x00
  assign_id_frame.data[5]  = 0x00; //D5, function id low byte; should be 0x00
  assign_id_frame.data[6]  = 0x00; //D6, function id high byte; should be 0x00
  assign_id_frame.data[7]  = 0x04; //D7, slave node address (NAD). 0x04 means ID(PxReq) = 04, ID(PxResp) = 05

  //make frame for io_cfg_1; configure Px pins for push pull level output
  //LIN_FRAME_t io_cfg_1_frame;
  io_cfg_1_frame.has_response = 1;
  io_cfg_1_frame.data_len = 8;
  io_cfg_1_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_1_frame.data[0]  = 0x60; //D0, slave node address (NAD) (default 0x60)
  io_cfg_1_frame.data[1]  = 0x06; //D1, protocol control info (PCI); should be 0x06
  io_cfg_1_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_1_frame.data[3]  = 0x00; //D3, bits 7-6 are 00 for first cfg block. Bits 5-0 are for IM/INH, RxDL, ADCIN cfg. D3 default is 0x00
  io_cfg_1_frame.data[4]  = 0xFF; //D4, High side enable (HSE). Set to 0xFF for High side driver or push pull
  io_cfg_1_frame.data[5]  = 0xFF; //D4, Low side enable (LSE). Set to 0xFF for Low side driver or push pull
  io_cfg_1_frame.data[6]  = 0x00; //D6, Output mode (low byte) (OM0). Set to 0x00 for level output
  io_cfg_1_frame.data[7]  = 0x00; //D7, Output mode (high byte) (OM1). Set to 0x00 for level output

  //make frame for io_cfg_2; defaults
  //LIN_FRAME_t io_cfg_2_frame;
  io_cfg_2_frame.has_response = 1;
  io_cfg_2_frame.data_len = 8;
  io_cfg_2_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_2_frame.data[0]  = 0x60; //D0, slave node address (NAD) (default 0x60)
  io_cfg_2_frame.data[1]  = 0x06; //D1, protocol control info (PCI); should be 0x06
  io_cfg_2_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_2_frame.data[3]  = 0x40; //D3, bits 7-6 are 01 for second cfg block. Bits 5-0 are for LSLP, TxDL, SMC, SMW, SM. D3 default is 0x40 (every bit 0 except for 7-6)
  io_cfg_2_frame.data[4]  = 0x00; //D4, Capture Mode (low byte) CM0. Set to 0x00 for no capture
  io_cfg_2_frame.data[5]  = 0x00; //D5, Capture Mode (high byte) CM1. Set to 0x00 for no capture
  io_cfg_2_frame.data[6]  = 0x00; //D6, Threshold select TH2 TH1. Default 0x00
  io_cfg_2_frame.data[7]  = 0x00; //D7, Local Wake up pin mask LWM. Default 0x00

  //make frame for io_cfg_3; set LH value, classic checksum, and LIN speeds up to 20kbps
  //LIN_FRAME_t io_cfg_3_frame;
  io_cfg_3_frame.has_response = 1;
  io_cfg_3_frame.data_len = 8;
  io_cfg_3_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_3_frame.data[0]  = 0x60; //D0, slave node address (NAD) (default 0x60)
  io_cfg_3_frame.data[1]  = 0x04; //D1, protocol control info (PCI); should be 0x04
  io_cfg_3_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_3_frame.data[3]  = 0x80; //D3, bits 7-6 are 10 for third cfg block. Bits 5-0 are for LSC and ECC (classic vs enhanced checksum). D3 default is 0x80 (every bit 0 except for 7-6)
  io_cfg_3_frame.data[4]  = 0x00; //D4, Limp home mode output value. This is the value the output pins will go to if the bus times out or another error occurs
  io_cfg_3_frame.data[5]  = 0x10; //D5, PWM initial value; shouldn't matter but is 0x10 per datasheet example
  io_cfg_3_frame.data[6]  = 0xFF; //D6, Not used, set to 0xFF
  io_cfg_3_frame.data[7]  = 0xFF; //D7, Not used, set to 0xFF


  //make frame for io_cfg_4; diagnostic data frame 
  //LIN_FRAME_t io_cfg_4_frame;
  io_cfg_4_frame.has_response = 1;
  io_cfg_4_frame.data_len = 8;
  io_cfg_4_frame.frame_id = 0x3C; //0x3C is for diagnostic frames
  io_cfg_4_frame.data[0]  = 0x60; //D0, slave node address (NAD) (default 0x60)
  io_cfg_4_frame.data[1]  = 0x02; //D1, protocol control info (PCI); should be 0x02
  io_cfg_4_frame.data[2]  = 0xB4; //D2, service id (SID); should be 0xB4
  io_cfg_4_frame.data[3]  = 0xC0; //D3, bits 7-6 are 11 for fourth cfg block. Bits 5-0 are unused and should be set to 0; should be 0xC0
  io_cfg_4_frame.data[4]  = 0xFF; //D4, Not used, set to 0xFF
  io_cfg_4_frame.data[5]  = 0xFF; //D5, Not used, set to 0xFF
  io_cfg_4_frame.data[6]  = 0xFF; //D6, Not used, set to 0xFF
  io_cfg_4_frame.data[7]  = 0xFF; //D7, Not used, set to 0xFF

    
  //make frame for PxReq frame (this is what sets the UJA1023's output pins)
  //LIN_FRAME_t px_req_frame;
  px_req_frame.has_response = 0;
  px_req_frame.data_len = 2;
  px_req_frame.frame_id = 0xC4; //PID, 0xC4 = 2 bit parity + 0x04 raw ID
  px_req_frame.data[0]  = 0x02; //D0, bit 7 = P7, bit 6 = P6 ... bit 0 = P0. 0x02 would set P1 to on
  px_req_frame.data[1]  = 0x80; //D1, PWM Value; shouldn't matter but is 0x80 per datasheet example
  
  // setup
  TIM4->PSC = 48-1;          // tick on 1 us
  TIM4->CR1 = TIM_CR1_CEN;   // enable
  TIM4->ARR = 30-1;          // 33.3 kbps

  // in case it's disabled
  NVIC_EnableIRQ(TIM4_IRQn);

  // run the interrupt
  TIM4->DIER = TIM_DIER_UIE; // update interrupt
  TIM4->SR = 0;
  
  initial_loop_num = 0;
  
}

void set_uja1023_output(uint8_t to_set) {
  px_req_frame.data[0]  = to_set;
}

#endif

