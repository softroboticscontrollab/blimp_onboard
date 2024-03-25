/*
 * active_capture_v04
 * Example code for the motor that runs the active capture mechanism for the BUUUM DTR blimp.
 * Can be used generally for single DC brushed motor control with the Pololu 2130 / TI DRV8833
 * 
 * USAGE:
 * Serial port set to the SERIAL_RATE below. In the arduino serial monitor, if you use this code manually, set the line endings to "new line"
 * Send either "f %f %f" or "r %f %f", which is forward/reverse at %f power (0.0-1.0) for %f seconds.
 * Example, "f 0.5 2.8" runs the motor at half power, forward open-loop, for 2.8 seconds.
 */

// For converting floats to cc register values. If 8-bit PWM...
#define MAX_CC 255
// better here than as an int
#define SERIAL_RATE 115200
// for strtok string conversion
#define BUFSIZE 200

// pin setup and constants
// will use two PWMs for one motor
int AIN1 = 0;
int AIN2 = 1;
// placeholders for what we'll read from the serial terminal
String received;
float duty_AIN1 = 0.0;
float duty_AIN2 = 0.0;
float pwr = 0.0;
int motor_runtime_millis = 0;
int raw_duty = 0;
float highlow_inv_duty = 0.0;
// because compiler is annoying and sscanf can't do floats directly
char *token;
char received_raw[BUFSIZE];
String cmd_letter = "";
// for a circuit that switches high-to-low, such as using BJTs instead of MOSFETS to control a robot
bool switch_highlow = true;
// Sensor reading periods. Example here with two sensors. In milliseconds.
// unsigned long sper1 = 500; // good number for the BendLabs One Axis sensor running over I2C at 400 Hz (kHz?) is 20 msec
// unsigned long sper2 = 1200; // the temperature sensor (MCP9600) samples much more slowly, even when setting the resolution low for its fastest readings is 100 msec
// Time management
unsigned long curr_time;
unsigned long prev_time;
bool motor_is_on = 0;

// helper function to turn a 0.0-1.0 float into an 8-bit duty cycle
int convert_duty(float duty)
{
  // If we're using the inverting BJT circuit, duty cycle is "opposite."
  highlow_inv_duty = duty;
  if(switch_highlow){
    highlow_inv_duty = 1.0 - highlow_inv_duty;
  }
  raw_duty = floor(highlow_inv_duty * MAX_CC);
  // constrain between 0 and MAX_CC.
  raw_duty = (raw_duty < 0) ? 0 : raw_duty;
  raw_duty = (raw_duty > MAX_CC) ? MAX_CC : raw_duty;
  return raw_duty;
}

void setup() {
  Serial.begin(SERIAL_RATE);
  Serial.setTimeout(10); // makes read faster
  // Wait until the port is ready
  while (!Serial)
    ;
  Serial.println("active_capture_v04 example. Type f %f %f  or  r %f %f to run the motor (A channel) at %f power (0.0-1.0) for %f seconds, forward or reverse.");
  // ensure we start with all off
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  analogWrite(AIN1, convert_duty(0.0));
  analogWrite(AIN2, convert_duty(0.0));
  // we'll also turn on the LED when any pin is set high
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  // Time management for sensor sampling:
  prev_time = millis();
  curr_time = millis();
}

void loop() {
  // If there is at least one byte in the serial buffer, someone is trying to send us data, so read that first (don't lose anything!)
  if( Serial.available() > 0){
    // presummably the user is smart enough to send the two floats... TODO validate the received string!
    received = Serial.readString();
    // using strtok to parse manually
    received.toCharArray(received_raw, BUFSIZE);
    token = strtok(received_raw, " ");
    if(token == NULL){
      Serial.println("Error! received empty string!");
    }
    else{
      // first, we should get either an f or an r
      cmd_letter = token;
      // then we should get a float for the power level...
      token = strtok(NULL, " ");
      pwr = atof(token);
      // then we should get a float for the amount of time to run the motor, in seconds...
      token = strtok(NULL, " ");
      motor_runtime_millis = int(atof(token)*1000.0);
    }
    Serial.println("You want to run the motor ");
    Serial.println(cmd_letter);
    Serial.println("At this power:");
    Serial.println(pwr);
    Serial.println("For this many milliseconds:");
    Serial.println(motor_runtime_millis);
    // If we got a good reading, set up for a timed loop:
    if(cmd_letter == "f" or cmd_letter == "r"){
      // Serial.println("Valid command.");
      // Reset the time
      prev_time = millis();
      curr_time = millis();
      // We choose fast decay here ("coast"). So, per https://www.ti.com/lit/ds/symlink/drv8833.pdf, ...
      if(cmd_letter == "f"){
        // if forward, AIN1 is PWM, and AIN2 is low.
        analogWrite(AIN1, convert_duty(pwr));
        analogWrite(AIN2, 0);
      }
      if(cmd_letter = "r"){
        // if reverse, AIN1 is low, and AIN2 is PWM.
        analogWrite(AIN1, 0);
        analogWrite(AIN2, convert_duty(pwr));
      }
      Serial.println("Turning motor ON");
      motor_is_on = 1;
      digitalWrite(LED_BUILTIN, HIGH);
      // run until time is up...
    }
  }
  // now, after handling any serial received, we update our time and check if we need to shut off the motor.
  if(motor_is_on){
    curr_time = millis();
    if( (curr_time - prev_time) > motor_runtime_millis ){
      // shut down and reset.
      analogWrite(AIN1, 0);
      analogWrite(AIN2, 0);
      motor_runtime_millis = 0;
      prev_time = millis();
      motor_is_on = 0;
      Serial.println("Turning motor OFF");
      digitalWrite(LED_BUILTIN, LOW);
    }
  }
}

//   // Next, sensor readings. If it's past the interval for either one, take a reading, and send.
//   curr_time = millis();
//   if( (curr_time - prev_time) > sper1){
//     sdata1 = read_sensor1();
//     prev_time = curr_time;
//     // Format and send
//     datatx(sdata1, sprefix1);
//   }
//   if( (curr_time - prev_time2) > sper2){
//     sdata2 = read_sensor2();
//     prev_time2 = curr_time;
//     // Format and send
//     datatx(sdata2, sprefix2);
//   }
// }

// String read_sensor1()
// {
//   dat1[0] = 6.5;
//   // do something interesting for a test
//   for(int i=0; i < n_sens1; i++){
//     dat1[i] = dat1[i] + (random(-100,100)/10.0);
//   }
//   return fmt_dat(dat1, n_sens1); 
// }

// String read_sensor2()
// {
//   dat2[0] = 2.7;
//   dat2[1] = 78.4;
//   // do something interesting for a test
//   for(int i=0; i < n_sens2; i++){
//     dat2[i] = dat2[i] + (random(-100,100)/10.0);
//   }
//   return fmt_dat(dat2, n_sens2); 
// }

String fmt_dat(float dat[], int nsens)
{
  // format the floats into a space-separated string
  String res = "";
  for(int i=0; i<nsens; i++){
    res = res + String(dat[i]) + " ";
  }
  return res;
}