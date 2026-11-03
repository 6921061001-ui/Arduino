#include <POP32.h>
#include <MCP3XXX.h>
MCP3008 adc;

const int sonarPin = A8;  // ขา ADC ที่ต่อกับ ZX-SONAR1M

#define NUM_SENSORS 8
uint8_t F_PIN[NUM_SENSORS] = { 0, 1, 2, 3, 4, 5, 6, 7 };
int F[NUM_SENSORS];
int REFmin[NUM_SENSORS], REFmax[NUM_SENSORS];
int last_value = 0, LastError, SumError;

/////////////////////////////////////////////////////////////
int MinValue[NUM_SENSORS] = { 58, 48, 47, 47, 50, 45, 46, 40 };
int MaxValue[NUM_SENSORS] = { 802, 868, 843, 831, 806, 835, 787, 580 };
int REF[NUM_SENSORS] = { 430, 458, 445, 439, 428, 440, 416, 310 };
/////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  adc.begin(PA0, PA1, PA2, PA3);
  beep();

   oled.clear();
  oled.text(0, 0, "Sonar Filter");
  oled.show();
  //AutoRef();
  oled.text(1, 1, "Press OK");
  sw_OK_press();  // Wait For Press SW
  beep();
  delay(100);

  //ShowCalibrate(); //Calibrate Light of Sensor
  //SerialF(); //Serial Sensor
  //Program1();
}

void loop() {
  /*
  float distance = readSonar();
  oled.clear();
  oled.text(0, 0, "Distance");
  oled.text(1, 0, "%d cm", (int)distance);
  oled.show(); 
  Serial.println(distance); 
*/

  //Motor(-25, -25);
  //delay(2000);
  //stdPID(0, 0.02, 0.03, 100, -10) ;  //speed, kp, kd, Max, Min
  //stdPID(25, 0.05, 0.12, 100, -20) ;  //speed, kp, kd, Max, Min (ต้องทำให้ส่ายน้อยลงและจับเส้นเพิ่มอีกนิ๊ดดดดดนึงเท่านั้น)
  //stdPID(25, 0.05, 0.12, 100, -20) ;  //speed, kp, kd, Max, Min

  m1();
  while (1) { MotorStop(); }  //จบการทำงาน
}


//------ SENSOR -----
void ReadSensor() {  //ค่าดิบ
  for (int _sensor = 0; _sensor < NUM_SENSORS; _sensor++) {
    //F[_sensor] = analog10(F_PIN[_sensor]);
    //adc.begin(cs, mosi, miso, sck);

    F[_sensor] = adc.analogRead(F_PIN[_sensor]);
  }
}

void readCalibrate() {  //ค่าที่ Calibrate แล้ว
  ReadSensor();
  for (int i = 0; i < NUM_SENSORS; i++) {
    unsigned int calmin, calmax;
    long x = 0;
    calmin = MinValue[i];
    calmax = MaxValue[i];
    x = map(F[i], calmin, calmax, 0, 1000);  //ถ้าไม่เกาะเส้นให้แก้ 0, 1000 หรือ 1000, 0
    if (x < 0) x = 0;
    if (x > 1000) x = 1000;
    F[i] = x;
  }
}

void CalibrateSensor(int pauseTime, int samples) {
  for (int i = 0; i < NUM_SENSORS; i++) {
    MinValue[i] = 1023;
    MaxValue[i] = 0;
  }
  for (int startSamp = 0; startSamp <= samples; startSamp++) {
    ReadSensor();
    for (int i = 0; i < NUM_SENSORS; i++) {
      MinValue[i] = (F[i] <= MinValue[i] ? F[i] : MinValue[i]);
      MaxValue[i] = (F[i] >= MaxValue[i] ? F[i] : MaxValue[i]);
    }
    delay(pauseTime);
  }
}

void ShowCalibrate() {  // Calibrate ค่าแสง
  Serial.println("Press OK to Start Calibrate Sensor");
  sw_OK_press();  // Wait For Press SW
  Serial.println("Calibrating...");
  beep();
  CalibrateSensor(20, 250);
  beep();
  Serial.println("Finish");
  Serial.println("  ");

  Serial.print("int MinValue[NUM_SENSORS] = {");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(MinValue[i]);
    if (i < NUM_SENSORS - 1) Serial.print(",");
    else Serial.println("};");
    REFmin[i] = MinValue[i];
  }

  Serial.print("int MaxValue[NUM_SENSORS] = {");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(MaxValue[i]);
    if (i < NUM_SENSORS - 1) Serial.print(",");
    else Serial.println("};");
    REFmax[i] = MaxValue[i];
  }
  Serial.print("int REF[NUM_SENSORS] = {");
  for (int i = 0; i < NUM_SENSORS; i++) {
    REF[i] = (REFmin[i] + REFmax[i]) / 2;
    Serial.print(REF[i]);
    if (i < NUM_SENSORS - 1) Serial.print(",");
    else Serial.println("};");
  }
  while (1)
    ;
}

void AutoRef() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    REF[i] = (MinValue[i] + MaxValue[i]) / 2;
  }
}

void SerialCalibrate() {
  while (1) {
    readCalibrate();
    for (int _serialF = 0; _serialF < NUM_SENSORS; _serialF++) {
      Serial.print(F[_serialF]);
      Serial.print("\t");
    }
    Serial.println("");
    delay(50);
  }
}

void SerialF() {
  while (1) {
    ReadSensor();
    for (int _serialF = 0; _serialF < NUM_SENSORS; _serialF++) {
      Serial.print(F[_serialF]);
      Serial.print("\t");
    }
    Serial.println("");
    delay(50);
  }
}

void SerialPosition() {
  while (1) {
    int Pos = readPosition(250, 50);
    int Error_F = Pos - 3500;
    Serial.print("Pos = ");
    Serial.println(Pos);
    Serial.print("                  Error = ");
    Serial.println(Error_F);
  }
}

void Motor(int leftsp, int rightsp) {
  motor(1, leftsp);
  motor(2, rightsp);
}

void MotorStop() {
  Motor(0, 0);
  delay(10);
  //ao(); delay(10);
}

//----- PID ------
int readPosition(int keepTrack, int noise) {
  unsigned char i, online = 0;
  unsigned long avg = 0;
  unsigned int sum = 0;
  static int last_value = 0;
  readCalibrate();
  for (i = 0; i < NUM_SENSORS; i++) {
    int values = F[i];
    if (values > keepTrack) {
      online = 1;
    }
    if (values > noise) {
      avg += (long)(values) * (i * 1000);
      sum += values;
    }
  }
  if (!online) {
    if (last_value < (NUM_SENSORS - 1) * 1000 / 2) {
      return 0;
    } else {
      return (NUM_SENSORS - 1) * 1000;
    }
  }
  last_value = avg / sum;
  return last_value;
}

void stdPID(int baseSpeed, float Kp, float Kd, int maxsp, int minsp) {
  int Pos = readPosition(250, 50);
  int Error = Pos - 3500;  //3500
  int PID_Value = (Kp * Error) + (Kd * (Error - LastError));
  LastError = Error;
  int LeftPower = baseSpeed - PID_Value;
  int RightPower = baseSpeed + PID_Value;
  if (LeftPower > 100) LeftPower = maxsp;
  if (LeftPower < 0) LeftPower = minsp;
  if (RightPower > 100) RightPower = maxsp;
  if (RightPower < 0) RightPower = minsp;
  Motor(LeftPower, RightPower);
}

void FFtimer(int baseSpeed, float Kp, float Kd, int totalTime, int maxsp, int minsp) {
  unsigned long startTime = millis();
  unsigned long endTime = startTime + totalTime;
  while (millis() <= endTime) {
    stdPID(baseSpeed, Kp, Kd, maxsp, minsp);
  }
}

void FF(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  while (1) {
    readCalibrate();
    if ((F[0] >= REF[0] && F[1] >= REF[1] && F[2] >= REF[2] && F[3] >= REF[3] && F[4] >= REF[4] && F[5] >= REF[5] && F[6] >= REF[6] && F[7] >= REF[7])) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);
  }

  MotorStop();
  if (select == 's') {
    Motor(-spd, -spd);
    delay(dl);
    while (1) {
      readCalibrate();
      if (F[0] >= REF[0] && F[7] >= REF[7]) break;
      Motor(-20, -20);
    }
    MotorStop();
  } else if (select == 'p') {
    Motor(spd, spd);
    delay(dl);
    while (1) {
      readCalibrate();
      if (F[0] >= REF[0] && F[7] >= REF[7]) break;
      Motor(20, 20);
    }
    MotorStop();

  } else if (select == 'l') {
    Motor(spd, spd);
    delay(dl);
    SpinL();
  } else if (select == 'r') {
    Motor(spd, spd);
    delay(dl);
    SpinR();
  }
}



void FFL(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  while (1) {
    readCalibrate();
    if (((F[0] <= REF[0] && F[4] <= REF[4]) && (F[6] >= REF[6] && F[7] >= REF[7]))) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);
  }
  MotorStop();
}

void FFR(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  while (1) {
    readCalibrate();
    if (((F[7] <= REF[7] && F[3] <= REF[3]) && (F[0] >= REF[0] && F[4] >= REF[4]))) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);
  }
  MotorStop();
}

void FFC(int spd, float kp, float kd) {
  while (1) {
    readCalibrate();
    if ((F[3] <= REF[3] || F[4] <= REF[4] || F[5] <= REF[5])) {
      break;
    }
    Motor(spd, spd);
    delay(50);
  }
  MotorStop();
}

void SpinL() {
  //Motor(100, 100); delay(100); MotorStop();
  Motor(-40, 40);
  delay(300);
  while (1) {
    readCalibrate();
    if (F[0] >= REF[0]) {
      MotorStop();
      Motor(-20, 20);
      delay(10);
      break;
    }
    Motor(20, -20);
  }
  MotorStop();
}

void SpinR() {
  //Motor(100, 100); delay(100); MotorStop();
  Motor(40, -40);
  delay(300);
  while (1) {
    readCalibrate();
    if (F[7] >= REF[7]) {
      MotorStop();
      Motor(20, -20);
      delay(10);
      break;
    }
    Motor(-20, 20);
  }
  MotorStop();
}


void SerialPos() {
  while (1) {
    int Pos = readPosition(250, 50);
    int Error_F = Pos - 3500;
    Serial.print("Pos = ");
    Serial.print(Pos);
    Serial.print("                  Error = ");
    Serial.println(Error_F);
  }
}


void sonar() {
  while (1) {
    int adc = analogRead(sonarPin);
    //Serial.println(adc);
    if (adc < 1000) {
      MotorStop();
    } else {
      //FF(25, 0.02, 0.03, 'p', 20, 80, -10);
      stdPID(50, 0.02, 0.03, 100, -10);
    }
  }
}

float readSonar()
{
  const int samples = 20;
  long total = 0;

  for (int i = 0; i < samples; i++)
  {
    total += analog(A8);
    delay(5);
  }

  float averageADC = total / (float)samples;
  return averageADC / 41.0;
}
