#include <Servo.h>
Servo esc;


void setup() {
  esc.attach(9, 1000, 2000); 
  esc.write(1000); 
  delay(1000); 
  esc.write(0);
  delay(2000);
  esc.write(90);
  delay(2000);
}

void loop() {

  esc.write(1500 + 200); 
  delay(15000); 


  esc.write(1500); 
  delay(2000); 


  esc.write(1500 - 200); 
  delay(15000); 


  esc.write(1500); 
  delay(2000); 
}
