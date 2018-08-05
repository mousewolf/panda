#include "../drivers/uja1023.h"

// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      regen paddle
//      accel rising edge
//      brake rising edge
//      brake > 0mph

// lateral limits
const int16_t MAX_ANGLE = 20; //Degrees

int tesla_brake_prev = 0;
int tesla_gas_prev = 0;
int tesla_speed = 0;
int current_car_time = -1;
int time_at_last_stalk_pull = -1;
int eac_status = 0;

int tesla_ignition_started = 0;

static void tesla_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  set_gmlan_digital_output(GMLAN_HIGH);
  reset_gmlan_switch_timeout(); //we're still in tesla safety mode, reset the timeout counter and make sure our output is enabled

  //int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr;
  if (to_push->RIR & 4) {
    // Extended
    // Not looked at, but have to be separated
    // to avoid address collision
    addr = to_push->RIR >> 3;
  } else {
    // Normal
    addr = to_push->RIR >> 21;
  }

   // Record the current car time in current_car_time (for use with double-pulling cruise stalk)
  if (addr == 0x318) {
    int hour = (to_push->RDLR & 0x1F000000) >> 24;
    int minute = (to_push->RDHR & 0x3F00) >> 8;
    int second = (to_push->RDLR & 0x3F0000) >> 16;
    current_car_time = (hour * 3600) + (minute * 60) + second;
  }
  
  if (addr == 0x45) {
    // 6 bits starting at position 0
    int lever_position = (to_push->RDLR & 0x3F);
    if (lever_position == 2) { // pull forward
      // activate openpilot
      // TODO: uncomment the if to use double pull to activate
      //if (current_car_time <= time_at_last_stalk_pull + 1 && current_car_time != -1 && time_at_last_stalk_pull != -1) {
          controls_allowed = 1;
      //}
      time_at_last_stalk_pull = current_car_time;
    } else if (lever_position == 1) { // push towards the back
      // deactivate openpilot
      controls_allowed = 0;
    }
  }  
  
  // Detect drive rail on (ignition) (start recording)
  if (addr == 0x348) {
    // GTW_status
    int drive_rail_on = (to_push->RDLR & 0x0001);
    tesla_ignition_started = drive_rail_on == 1;
  }

  // exit controls on brake press
  // DI_torque2::DI_brakePedal 0x118
  //if (addr == 0x118) {
    // 1 bit at position 16
  //  if (((to_push->RDLR & 0x8000)) >> 15 == 1) {
  //    controls_allowed = 0;
  //  }
  //}  
  
  // exit controls on EPAS error
  // EPAS_sysStatus::EPAS_eacStatus 0x370
  if (addr == 0x370) {
    // if EPAS_eacStatus is not 1 or 2, disable control
    eac_status = ((to_push->RDHR >> 21)) & 0x7;
    if (eac_status != 1 && eac_status != 2) {
      controls_allowed = 0;
    }
  }

  //Set UJA1023 outputs for camera swicther/etc.
  
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
}

// all commands: gas/regen, friction brake and steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int tesla_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  //uint32_t addr;
  //int angle_raw;
  //int angle_steer;

  // 1 allows the message through
  //addr = to_send->RIR >> 21;
  
  // do not transmit CAN message if steering angle too high
  // DAS_steeringControl::DAS_steeringAngleRequest
  //if (addr == 0x488) {
  //  angle_raw = to_send->RDLR & 0x7F;
  //  angle_steer = angle_raw * 0.1 - 1638.35;
  //  if ( (angle_steer > MAX_ANGLE) || (angle_steer < -MAX_ANGLE) ) {
  //    return 0;
  //  }
  //}  
  
  return true;
}

static int tesla_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  // LIN is not used on the Tesla
  return false;
}

static void tesla_init(int16_t param) {
  controls_allowed = 0;
  tesla_ignition_started = 0;
  gmlan_switch_init(1); //init the gmlan switch with 1s timeout enabled
  uja1023_init();
}

static int tesla_ign_hook() {
  return tesla_ignition_started;
}

static int tesla_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  
  int32_t addr = to_fwd->RIR >> 21;
  
  if (bus_num == 0) {
    
    // change inhibit of GTW_epasControl to WITH_BOTH
    if (addr == 0x101) {
      to_fwd->RDLR = to_fwd->RDLR | 0xC000;
      int checksum = (((to_fwd->RDLR & 0xFF00) >> 8) + (to_fwd->RDLR & 0xFF) + 2) & 0xFF;
      to_fwd->RDLR = to_fwd->RDLR & 0xFFFF;
      to_fwd->RDLR = to_fwd->RDLR + (checksum << 16);
    }

    return 2; // Custom EPAS bus
  }
  if (bus_num == 2) {
    
    // remove GTW_epasControl in forwards
   if (addr == 0x101) {
     return false;
   }

    return 0; // Chassis CAN
  }
  return false;
}

const safety_hooks tesla_hooks = {
  .init = tesla_init,
  .rx = tesla_rx_hook,
  .tx = tesla_tx_hook,
  .tx_lin = tesla_tx_lin_hook,
  .ignition = tesla_ign_hook,
  .fwd = tesla_fwd_hook,
};
