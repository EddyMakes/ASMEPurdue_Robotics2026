#include <Servo.h>

Servo servo1;

Servo servo2;

int X = 0;
int Y = 1;

int inpVal; 

void setup() {
  // put your setup code here, to run once:
  servo1.attach(3);
  servo2.attach(5);

}

void loop() {
  // put your main code here, to run repeatedly:

inpVal = analogRead(X);
inpVal = map (inpVal, 0 ,1023, 0, 180);
servo1.write(inpVal);

inpVal = analogRead(Y);
inpVal = map (inpVal, 0 ,1023, 0, 180);
servo2.write(inpVal);
delay(15);


}
