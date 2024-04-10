/*
 * active_capture_v07
 * Example code for the motor that runs the active capture mechanism for the BUUUM DTR blimp.
 * Can be used generally for single DC brushed motor control with the Pololu 2130 / TI DRV8833
 * Now autonomous open timed loops for both capture and motion.
 * 
 * USAGE:
 * No input. Just run it. Will output misc debugging messages if you plug in over USB.
 */

#include <Servo.h>
Servo esc;

// For converting floats to cc register values. If 8-bit PWM...
#define MAX_CC 255
// better here than as an int
#define SERIAL_RATE 115200
// for strtok string conversion
#define BUFSIZE 200

// pin setup and constants
// will use two PWMs for one motor
int AIN1 = 5;
int AIN2 = 6;
// for the fan
int ESC_PIN = 9;
int ESC_MAX = 2000;
int ESC_MIN = 1000;
int ESC_RANGE = ESC_MAX - ESC_MIN;
int ESC_HALFRANGE = int(ESC_RANGE/2.0);
int ESC_AVG = ESC_HALFRANGE + ESC_MIN;
// placeholder variables for the fan conversion function
float raw_speed = 0.0; // since can be negative
int adjusted_speed = 0; // is a positive number
// placeholders for what we'll read from the serial terminal
String received;
float pwr = 0.0;
int motor_runtime_millis = 0;
int raw_duty = 0;
// because compiler is annoying and sscanf can't do floats directly
char *token;
char received_raw[BUFSIZE];
String cmd_letter = "";

// For the open loop, set our motion parameters
int ascend_time_millis = 10000;
int descend_time_millis = 10000;
int capture_open_pulse = 500;
int capture_close_pulse = 500;
int capture_time_millis = 10000;
float fan_pwr_default = 0.2;
// flags for the state machine of capture and fan
int fan_state = 0; // States: 0 = not running, 1 = ascend, 2 = descend
int capture_state = 0; // States: 0 = capture open waiting, 1 = capture closing, 2 = capture closed waiting, 3 = capture opening
// Time management
unsigned long curr_time;
unsigned long prev_time_fan;
unsigned long prev_time_capture;
bool motor_is_on = 0;

// helper function to turn a 0.0-1.0 float into an 8-bit duty cycle
int convert_duty(float duty)
{
  raw_duty = floor(duty * MAX_CC);
  // constrain between 0 and MAX_CC.
  raw_duty = (raw_duty < 0) ? 0 : raw_duty;
  raw_duty = (raw_duty > MAX_CC) ? MAX_CC : raw_duty;
  return raw_duty;
}

// helper function to turn a -1.0 to +1.0 float into an int for the Servo library.
// Assumes that ESC_MIN and ESC_MAX are declared and assigned values.
int convert_esc(float percent_speed)
{
  raw_speed = percent_speed * ESC_HALFRANGE; // this number is "fraction of 100% power but centered at 0"
  raw_speed = raw_speed + ESC_AVG; // should be between ESC_MAX and ESC_MIN
  adjusted_speed = floor(raw_speed); // now is a positive number only
  // constrain percent speed to be between min and max just in case someone passed in a too-big number.
  adjusted_speed = (adjusted_speed < ESC_MIN) ? ESC_MIN : adjusted_speed;
  adjusted_speed = (adjusted_speed > ESC_MAX) ? ESC_MAX : adjusted_speed;
  // DEBUGGING
  Serial.println("Setting ESC speed to:");
  Serial.println(adjusted_speed);
  return adjusted_speed;
}

// helper that toggles our fan. state = int, see above, pwr = percent if override desired.
void update_fan(int state){
  update_fan(state, fan_pwr_default);
}
void update_fan(int state, float pwr)
{
  switch(state){
    case 0:
      // not running. State currently unused.
      break;

    case 1:
      // ascend
      esc.write(convert_esc(pwr));
      break;

    case 2:
      // descend
      esc.write(convert_esc(-pwr)); // pwr should be positive, so descending means negative direction.
      break;
  }
}

void setup() {
  Serial.begin(SERIAL_RATE);
  Serial.setTimeout(10); // makes read faster
  // Wait until the port is ready
  while (!Serial)
    ;
  Serial.println("active_capture_v07. Runs open loop.");
  // ensure we start with all off
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  analogWrite(AIN1, convert_duty(0.0));
  analogWrite(AIN2, convert_duty(0.0));
  // we'll also turn on the LED when any pin is set high
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  // set up the ESC
  pinMode(ESC_PIN, OUTPUT);
  esc.attach(ESC_PIN, ESC_MIN, ESC_MAX);
  esc.write(ESC_AVG);
  // test the ESC
  Serial.println("ESC NEUTRAL");
  esc.write(ESC_AVG);
  delay(10000);
  // Serial.println("ESC ON FWD");
  // esc.write(convert_esc(0.2));
  // delay(10000);
  // Serial.println("ESC ON REV");
  // esc.write(convert_esc(-0.2));
  // delay(10000);
  // Serial.println("ESC NEUTRAL");
  // esc.write(ESC_AVG);
  // Time management for timed motor loops
  prev_time_fan = millis();
  prev_time_capture = millis();
  curr_time = millis();
  // Turn on the fan and set the mouth to waiting open.
  fan_state = 1;
  update_fan(fan_state);
  capture_state = 0;
}

//   // now, after handling any serial received, we update our time and check if we need to shut off the motor.
  // if(motor_is_on){
  //   curr_time = millis();
  //   if( (curr_time - prev_time) > motor_runtime_millis ){
  //     // shut down and reset.
  //     analogWrite(AIN1, 0);
  //     analogWrite(AIN2, 0);
  //     motor_runtime_millis = 0;
  //     prev_time = millis();
  //     motor_is_on = 0;
  //     Serial.println("Turning motor OFF");
  //     digitalWrite(LED_BUILTIN, LOW);
  //   }
  // }

void loop() {
  curr_time = millis();
  // For the fan:
  switch(fan_state){
    case 0:
      // not running. State currently unused.
      break;

    case 1:
      // ascending
      // Check if our ascent time is up. If so, switch to descending
      if( (curr_time - prev_time_capture) > ascend_time_millis){
        fan_state = 2;
        prev_time_capture = millis();
        // DEBUGGING
        Serial.println("Switching to descent...");
        update_fan(fan_state);
      }
      break;

    case 2:
      // descending
      // Check if our descent time is up. If so, switch to ascending.
      if((curr_time - prev_time_capture) > descend_time_millis){
        fan_state = 1;
        prev_time_capture = millis();
        Serial.println("Switching to ascent...");
        update_fan(fan_state);
      }
      break;
  }
  // update_fan(fan_state);

  // If there is at least one byte in the serial buffer, someone is trying to send us data, so read that first (don't lose anything!)
  // if( Serial.available() > 0){
  //   // presummably the user is smart enough to send the right string... TODO validate!
  //   received = Serial.readString();
  //   // using strtok to parse manually
  //   received.toCharArray(received_raw, BUFSIZE);
  //   token = strtok(received_raw, " ");
  //   if(token == NULL){
  //     Serial.println("Error! received empty string!");
  //   }
  //   else{
  //     // first, we should get either an f or an r
  //     cmd_letter = token;
  //     // then we should get a float for the power level...
  //     token = strtok(NULL, " ");
  //     pwr = atof(token);
  //     // then we should get a float for the amount of time to run the motor, in seconds...
  //     token = strtok(NULL, " ");
  //     motor_runtime_millis = int(atof(token)*1000.0);
  //   }
    //////////////////
    // DEBUGGING
    // Serial.println("You want to run the motor ");
    // Serial.println(cmd_letter);
    // Serial.println("At this power:");
    // Serial.println(pwr);
    // Serial.println("For this many milliseconds:");
    // Serial.println(motor_runtime_millis);
    // END DEBUGGING
    //////////////////
    // If we got a good reading, set up for a timed loop:
    // if(cmd_letter == "f" or cmd_letter == "r"){
    //   // Reset the time
    //   prev_time = millis();
    //   curr_time = millis();
    //   // We choose fast decay here ("coast"). So, per https://www.ti.com/lit/ds/symlink/drv8833.pdf, ...
    //   if(cmd_letter == "f"){
    //     // if forward, AIN1 is PWM, and AIN2 is low.
    //     Serial.println("Turning motor ON FWD");
    //     analogWrite(AIN1, convert_duty(pwr));
    //     // digitalWrite(AIN1, 1);
    //     analogWrite(AIN2, 0);
    //   }
    //   if(cmd_letter == "r"){
    //     // if reverse, AIN1 is low, and AIN2 is PWM.
    //     Serial.println("Turning motor ON REV");
    //     analogWrite(AIN1, 0);
    //     analogWrite(AIN2, convert_duty(pwr));
    //   }
    //   motor_is_on = 1;
    //   digitalWrite(LED_BUILTIN, HIGH);
    //   // run until time is up...
    // }
  // }
  // // now, after handling any serial received, we update our time and check if we need to shut off the motor.
  // if(motor_is_on){
  //   curr_time = millis();
  //   if( (curr_time - prev_time) > motor_runtime_millis ){
  //     // shut down and reset.
  //     analogWrite(AIN1, 0);
  //     analogWrite(AIN2, 0);
  //     motor_runtime_millis = 0;
  //     prev_time = millis();
  //     motor_is_on = 0;
  //     Serial.println("Turning motor OFF");
  //     digitalWrite(LED_BUILTIN, LOW);
  //   }
  // }
}
