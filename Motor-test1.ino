#include <servo.h> //servo library
Servo esc; //creates servo object named esc
const int escPin = 9; //variable for which pin

void setup() {
  // put your setup code here, to run once:
  esc.attach(escPin, 1000, 2000); //attaches esc to pin 9 with limits

  esc.writeMicroseconds(1000); //writes 1000us signal to pin 9 (motor rest)
  delay(3000);
}

void loop() {
  // put your main code here, to run repeatedly:
  //for loop slowly ramping up signal to 60%
  for (int pulse = 1000; pulse <= 1600; pulse += 10) 
  {
    esc.writeMicroseconds(pulse);
    delay(50);
  }

  delay(3000); //hold at 60% for 3 seconds

  //for loop ramping motor back down to rest
  for (pulse = 1600; pulse >= 1000; pulse -= 10)
  {
    esc.writeMicroseconds(pulse);
    delay(50);
  }

  delay(3000); //wait 3 seconds before repeating
}
