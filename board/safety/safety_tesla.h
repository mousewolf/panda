#include "../drivers/uja1023.h"

static void tesla_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  //Set UJA1023 outputs for camera swicther/etc.
  
  int addr = (to_push->RIR >> 21);
  
  //0x118 is DI_torque2
  if (addr == 0x118) {
    int drive_state = (to_push->RDLR >> 12) & 0x7; //DI_gear : 12|3@1+
    int brake_pressed = (to_push->RDLR & 0x8000) >> 15;

    //if the car goes into reverse, set UJA1023 output pin 0 to high. If Drive, set pin 1 high.
    //DI_gear 7 "DI_GEAR_SNA" 4 "DI_GEAR_D" 3 "DI_GEAR_N" 2 "DI_GEAR_R" 1 "DI_GEAR_P" 0 "DI_GEAR_INVALID" ;
    if (drive_state == 2) {
      set_uja1023_output_bits(1 << 0);
      //puts("Got Reverse\n");
    } else {
      clear_uja1023_output_bits(1 << 0);
    }
    if (drive_state == 3) {
      set_uja1023_output_bits(1 << 1);
      //puts("Got Drive\n");
    } else {
      clear_uja1023_output_bits(1 << 1);
    }

    //if the car's brake is pressed, set pin 2 to high
    if (brake_pressed == 1) {
      set_uja1023_output_bits(1 << 2);
      //puts("Brake on!\n");
    } else {
      clear_uja1023_output_bits(1 << 2);
    }
  }

  //0x45 is STW_ACTN_RQ
  if (addr == 0x45) {
    int turnSignalLever = (to_push->RDLR & 0x30000); //TurnIndLvr_Stat : 16|2@1+
    
    //TurnIndLvr_Stat 3 "SNA" 2 "RIGHT" 1 "LEFT" 0 "IDLE" ;
    if (turnSignalLever == 1) {
      //Left turn signal is on, turn on output pin 3
      set_uja1023_output_bits(1 << 3);
      //puts("Left turn on!\n");
    }
    else {
      clear_uja1023_output_bits(1 << 3);
    }
    if (turnSignalLever == 2) {
      //Right turn signal is on, turn on output pin 4
      set_uja1023_output_bits(1 << 4);
      //puts("Right turn on!\n");
    }
    else {
      clear_uja1023_output_bits(1 << 4);
    }
  }
    
    /*
    Set UJA1023 output pins. Bits = pins
    P0 = bit 0
    P1 = bit 1
    ...
    P7 = bit 7
    0b10101010 = P0 off, P1 on, P2 off, P3 on, P4 off, P5 on, P6 off, P7 on
    
    Examples:
    set bit 5:
    set_uja1023_output_bits(1 << 5); //turn on any pins that = 1, leave all other pins alone
    
    clear bit 5:
    clear_uja1023_output_bits(1 << 5); //turn off any pins that = 1, leave all other pins alone
    
    Or set the whole buffer at once:
    set_uja1023_output_buffer(0xFF); //turn all output pins on
    set_uja1023_output_buffer(0x00); //turn all output pins off
    */
}

// *** no output safety mode ***

static void tesla_init(int16_t param) {
  controls_allowed = 0;
  uja1023_init();
}

static int tesla_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  return false;
}

static int tesla_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  return false;
}

static int tesla_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  return -1;
}

const safety_hooks tesla_hooks = {
  .init = tesla_init,
  .rx = tesla_rx_hook,
  .tx = tesla_tx_hook,
  .tx_lin = tesla_tx_lin_hook,
  .fwd = tesla_fwd_hook,
};

// *** all output safety mode ***

static void tesla_alloutput_init(int16_t param) {
  controls_allowed = 1;
}

static int tesla_alloutput_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  return true;
}

static int tesla_alloutput_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  return true;
}

static int tesla_alloutput_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  return -1;
}

const safety_hooks tesla_alloutput_hooks = {
  .init = tesla_alloutput_init,
  .rx = tesla_rx_hook,
  .tx = tesla_alloutput_tx_hook,
  .tx_lin = tesla_alloutput_tx_lin_hook,
  .fwd = tesla_alloutput_fwd_hook,
};

