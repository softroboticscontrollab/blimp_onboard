/*
 * active_capture_v07
 * Example code for the motor that runs the active capture mechanism for the BUUUM DTR blimp.
 * Can be used generally for single DC brushed motor control with the Pololu 2130 / TI DRV8833
 * Now autonomous open timed loops for both capture and motion.
 * 
 * USAGE:
 * No input. Just run it. Will output misc debugging messages if you plug in over USB.
 */

// for the brushless motor
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
int raw_duty = 0;

// For the open loop, set our motion parameters
int ascend_time_millis = 4000;
int descend_time_millis = 15000;
int capture_open_pulse = 700;
int capture_close_pulse = 1500;
int capture_waittime_millis = 8000;
float fan_pwr_default = 0.5;
float capture_pwr_default = 0.5;
// flags for the state machine of capture and fan
int fan_state = 0; // States: 0 = not running, 1 = ascend, 2 = descend
int capture_state = 0; // States: 0 = capture open waiting, 1 = capture closing, 2 = capture closed waiting, 3 = capture opening
// Time management
unsigned long curr_time;
unsigned long prev_time_fan;
unsigned long prev_time_capture;

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
  // Serial.println("Setting ESC speed to:");
  // Serial.println(adjusted_speed);
  return adjusted_speed;
}

void fan_startup()
{
  // Toggle the fan back and forth at 100% power a few times.
  Serial.println("Toggling fan...");
  esc.write(ESC_MAX);
  delay(3000);
  esc.write(ESC_MIN);
  delay(3000);
  esc.write(ESC_AVG);
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
      // HACK 2024-04-10: ascend and descend are flipped. To descend faster, add power here. Twice as fast?
      esc.write(convert_esc(pwr));
      break;

    case 2:
      // descend
      // HACK 2024-04-10: ascend and desend are flipped. Since the blimp is heavy, make it full negative... which is MIN here.
      // esc.write(convert_esc(-pwr)); // pwr should be positive, so descending means negative direction.
      esc.write(ESC_MIN); // pwr should be positive, so descending means negative direction.
      break;
  }
}

// helper that toggles our capture mechanism. state = int, see above, pwr = percent if override desired.
void update_capture(int state){
  update_capture(state, capture_pwr_default);
}
void update_capture(int state, float pwr)
{
  // States: 0 = capture open waiting, 1 = capture closing, 2 = capture closed waiting, 3 = capture opening
  switch(state){
    case 0:
      // no motion.
      analogWrite(AIN1, 0);
      analogWrite(AIN2, 0);
      digitalWrite(LED_BUILTIN, LOW);
      break;

    case 1:
      // closing (forward)
      analogWrite(AIN1, convert_duty(pwr));
      analogWrite(AIN2, 0);
      digitalWrite(LED_BUILTIN, HIGH);
      break;

    case 2:
      // no motion.
      analogWrite(AIN1, 0);
      analogWrite(AIN2, 0);
      digitalWrite(LED_BUILTIN, LOW);
      break;
    
    case 3:
      // opening (reverse)
      analogWrite(AIN1, 0);
      analogWrite(AIN2, convert_duty(pwr));
      digitalWrite(LED_BUILTIN, HIGH);
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
  // Startup sequence blink to know the code is running
  int cycles = 3;
  for(int i=0; i<cycles; i++){
    Serial.println("Starting up (LED blink to indicate)...");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
  }
  // set up the ESC
  pinMode(ESC_PIN, OUTPUT);
  esc.attach(ESC_PIN, ESC_MIN, ESC_MAX);
  esc.write(ESC_AVG);
  Serial.println("ESC NEUTRAL, Waiting for esc to start up...");
  delay(10000);
  fan_startup();
  Serial.println("Beginning open loop motion...");
  // Time management for timed motor loops
  prev_time_fan = millis();
  prev_time_capture = millis();
  curr_time = millis();
  // Turn on the fan and set the mouth to waiting open.
  fan_state = 1;
  update_fan(fan_state);
  capture_state = 0;
  update_capture(capture_state);
}

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
      if( (curr_time - prev_time_fan) > ascend_time_millis){
        fan_state = 2;
        prev_time_fan = millis();
        // DEBUGGING
        Serial.println("Switching to descend...");
        update_fan(fan_state);
      }
      break;

    case 2:
      // descending
      // Check if our descent time is up. If so, switch to ascending.
      if((curr_time - prev_time_fan) > descend_time_millis){
        fan_state = 1;
        prev_time_fan = millis();
        Serial.println("Switching to ascend...");
        update_fan(fan_state);
      }
      break;
  }

  // for the capture:
  switch(capture_state){
    case 0:
      // capture is open, waiting. Switch to closing if timed out
      if((curr_time - prev_time_capture) > capture_waittime_millis){
        capture_state = 1;
        prev_time_capture = millis();
        Serial.println("CAPTURE OPENING...");
        update_capture(capture_state);
      }
      break;
    
    case 1:
      // capture is opening. Stop opening if timed out
      if((curr_time - prev_time_capture) > capture_open_pulse){
        capture_state = 2;
        prev_time_capture = millis();
        Serial.println("CAPTURE WAITING...");
        update_capture(capture_state);
      }
      break;
    
    case 2:
      // capture is closed, waiting. Switch to opening if timed out
      if((curr_time - prev_time_capture) > capture_waittime_millis){
        capture_state = 3;
        prev_time_capture = millis();
        Serial.println("CAPTURE CLOSING...");
        update_capture(capture_state);
      }
      break;
    
    case 3:
      // capture is closing. Stop closing if timed out
      if((curr_time - prev_time_capture) > capture_close_pulse){
        capture_state = 0;
        prev_time_capture = millis();
        Serial.println("CAPTURE WAITING...");
        update_capture(capture_state);
      }
      break;
  }
}
