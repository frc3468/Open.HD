#include <Servo.h>

//////////////////////CONFIGURATION///////////////////////////////
#define CHANNEL_NUMBER 8  //set the number of chanels
#define CHANNEL_DEFAULT_VALUE 1500  //set the default servo value
#define SERVO_DEADBAND 100 // servo value deadband
#define FAILSAFE 500 //activate failsafe after 500ms of inactivity
#define FAILSAFE_THROTTLE_VALUE 1500 // value we put to throttle if failsafe is activated

// Channels
#define LX_AXIS 0
#define LY_AXIS 1
#define LT_AXIS 2
#define RX_AXIS 3
#define RY_AXIS 4
#define RT_AXIS 5
#define UK_AXIS 6
#define BTNS 7

// BTNS Bitmap
#define A_BTN 0
#define B_BTN 1
#define X_BTN 2
#define Y_BTN 3
#define LS_BTN 4
#define RS_BTN 5
#define BACK_BTN 6
#define START_BTN 7
#define XBOX_BTN 8
#define LJ_BTN 9
#define RJ_BTN 10
#define UK_BTN 11

// HAT States
#define HAT_CENTERED  0x00
#define HAT_UP    0x01
#define HAT_RIGHT   0x02
#define HAT_DOWN    0x04
#define HAT_LEFT    0x08
#define HAT_RIGHTUP   (HAT_RIGHT|HAT_UP)
#define HAT_RIGHTDOWN (HAT_RIGHT|HAT_DOWN)
#define HAT_LEFTUP    (HAT_LEFT|HAT_UP)
#define HAT_LEFTDOWN  (HAT_LEFT|HAT_DOWN)

// Drive Pins
uint8_t left_drive_spark = 2;
uint8_t right_drive_spark = 3;
Servo left_drive;
Servo right_drive;

// Camera Pins
uint8_t cam_left_pin = 9;
uint8_t cam_right_pin = 10;
uint8_t cam_up_pin = 11;
uint8_t cam_down_pin = 13;

int channels[CHANNEL_NUMBER];
uint8_t msp_packet[22];
int msp_packet_index=0;
long int last_receive;

int get_axis(uint8_t axis) {
  if (axis < CHANNEL_NUMBER) {
    return(channels[axis]);
  }
  return(-1);
}

bool get_btn(uint8_t button) {
  if (button < 16) {
    return(channels[BTNS]&1<<button);
  }
  return(false);
}

bool get_hat() {
  return channels[BTNS]>>12;
}

int apply_deadband(int value) {
  if (value > CHANNEL_DEFAULT_VALUE-SERVO_DEADBAND && value < CHANNEL_DEFAULT_VALUE+SERVO_DEADBAND) {
    return(CHANNEL_DEFAULT_VALUE);
  }
  return(value);
}

int apply_servo_constraint(int value) {
  return(constrain(value, 1000, 2000));
}



void setup() {  

  //initiallize default ppm values
  for(int i=0; i<CHANNEL_NUMBER; i++) {
      channels[i]= CHANNEL_DEFAULT_VALUE;
  }

  last_receive=millis();

  Serial1.begin(115200);
  
  left_drive.attach(left_drive_spark);
  right_drive.attach(right_drive_spark);

  pinMode(cam_left_pin, OUTPUT);
  pinMode(cam_right_pin, OUTPUT);
  pinMode(cam_up_pin, OUTPUT);
  pinMode(cam_down_pin, OUTPUT);
}


void loop() {
  
  while(Serial1.available()) {
    uint8_t c=Serial1.read();
    
    if(msp_packet_index==0 && c!=36) { // $ character
      continue;
    }
    if(msp_packet_index==1 && c!=77) { // M character
      continue;
    }
    if(msp_packet_index==2 && c!=60) { // < character
      continue;
    }

    msp_packet[msp_packet_index]=c;

    if(msp_packet_index==21) { // packet complete
      int crc=0;
      for(int i=3;i<21;i++) {
        crc^=msp_packet[i];
      }

      //msp fame valid
      if(crc==msp_packet[21]) {
        last_receive=millis();
        
        for(int channel_index=0;channel_index<CHANNEL_NUMBER;channel_index++) {
          channels[channel_index]=(msp_packet[5+channel_index*2+1]<<8)+msp_packet[5+channel_index*2];
        }
      } else {
        Serial1.println("corrupt");
      }

      msp_packet_index=0;
    } else {
      msp_packet_index++;
    }
  }

  // Watchdog
  if((millis()-last_receive)>FAILSAFE) {
    channels[LX_AXIS]=FAILSAFE_THROTTLE_VALUE;
    channels[LY_AXIS]=FAILSAFE_THROTTLE_VALUE;
    channels[LT_AXIS]=0;
    channels[RX_AXIS]=FAILSAFE_THROTTLE_VALUE;
    channels[RY_AXIS]=FAILSAFE_THROTTLE_VALUE;
    channels[RT_AXIS]=0;
    channels[UK_AXIS]=FAILSAFE_THROTTLE_VALUE;
    channels[BTNS]=0;
  }

  //======Arcade Drive====================================================
  int speed = map(apply_deadband(channels[LY_AXIS]), 1000, 2000, 100, -100); //Invert
  int rotation = map(apply_deadband(channels[LX_AXIS]), 1000, 2000, -100, 100);
  int max_input = max(abs(speed), abs(rotation));
  if (speed < 0) {
    max_input = -1*max_input;
  }
  
  int left_speed = 0;
  int right_speed = 0;
  
  if (speed >= 0) {
    if (rotation >= 0) {
      left_speed = max_input;
      right_speed = speed - rotation;
    } else {
      left_speed = speed + rotation;
      right_speed = max_input;
    }
  } else {
    if (rotation >= 0) {
      left_speed = speed + rotation;
      right_speed = max_input;
    } else {
      left_speed = max_input;
      right_speed = speed - rotation;
    }
  }

  left_speed = map(constrain(left_speed, -100, 100), -100, 100, 1000, 2000);
  right_speed = map(constrain(right_speed, -100, 100), -100, 100, 2000, 1000); //Invert
  
  left_drive.writeMicroseconds(left_speed);
  right_drive.writeMicroseconds(right_speed);
  //======================================================================

  //======Camera Control==================================================
  int vertical_speed = map(apply_deadband(channels[RY_AXIS]), 1000, 2000, 255, -255); //Invert
  int horizontal_speed = map(apply_deadband(channels[RX_AXIS]), 1000, 2000, -255, 255);

  if (vertical_speed > 0) {
    analogWrite(cam_up_pin, vertical_speed);
    digitalWrite(cam_down_pin, LOW);
  } else if(vertical_speed < 0) {
    digitalWrite(cam_up_pin, LOW);
    analogWrite(cam_down_pin, abs(vertical_speed));
  } else {
    digitalWrite(cam_up_pin, LOW);
    digitalWrite(cam_down_pin, LOW);
  }

  if (horizontal_speed < 0) {
    analogWrite(cam_left_pin, horizontal_speed);
    digitalWrite(cam_right_pin, LOW);
  } else if(vertical_speed > 0) {
    digitalWrite(cam_left_pin, LOW);
    analogWrite(cam_right_pin, abs(horizontal_speed));
  } else {
    digitalWrite(cam_left_pin, LOW);
    digitalWrite(cam_right_pin, LOW);
  }
  //======================================================================

}

