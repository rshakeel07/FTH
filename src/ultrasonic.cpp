// gets ultrasnoic distance from the sensor, and sends bool of too close whenever it is closer than a meter
// we have 2 ultrasonic sensors that we want, 1 on left 1 on right and we need bool for both
// right is trigPin1 and echoPin1, left is trigPin2 and echoPin2

bool tooCloseleft = false;
bool tooCloseright = false;
const int trigPin1 = 9; // Trigger pin of the ultrasonic sensor
const int echoPin1 = 10; // Echo pin of the ultrasonic sensor
const int trigPin2 = 11; // Trigger pin of the ultrasonic sensor
const int echoPin2 = 12; // Echo pin of the ultrasonic sensor

int leftdist = 0;
int rightdist = 0;


long ultrasonic(int trigPin, int echoPin);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly: 
  leftdist = ultrasonic(trigPin1, echoPin1);
  rightdist = ultrasonic(trigPin2, echoPin2);

  if (leftdist < 100) {
    tooCloseleft = true;
  } else {
    tooCloseleft = false;
  }

  if (rightdist < 100) {
    tooCloseright = true;
  } else {
    tooCloseright = false;
  }
}


long ultrasonic(int trigPin, int echoPin) {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Sets the trigger pin HIGH for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Reads the echo pin, returns the sound wave travel time in microseconds
  long duration = pulseIn(echoPin, HIGH);
  
  // Calculating the distance (Speed of sound is ~343 m/s or 0.0343 cm/us)
  // Distance = (Duration * Speed of sound) / 2 (for the round trip)
  long distance = duration * 0.0343 / 2;
  
  return distance;
}