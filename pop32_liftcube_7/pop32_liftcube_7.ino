#include <POP32.h>
#include <MCP3XXX.h>

MCP3008 adc;
MCP3008 adcBack;

// กำหนดองศาการวาง
#define PUT_LEVEL1   18
#define PUT_LEVEL2   32
#define PUT_LEVEL3   48
#define PUT_LEVEL4   63
#define PUT_LEVEL5   78

#define NUM_SENSORS 8
uint8_t F_PIN[NUM_SENSORS] = { 0, 1, 2, 3, 4, 5, 6, 7 };

int F[NUM_SENSORS];
int REFmin[NUM_SENSORS], REFmax[NUM_SENSORS];
int last_value = 0, LastError, SumError;

/////////////////////////////////////////////////////////////
// ค่า Calibration เดิม
int MinValue[NUM_SENSORS] = { 238, 268, 103, 69, 73, 63, 61, 243 };
int MaxValue[NUM_SENSORS] = { 870, 891, 796, 781, 814, 801, 773, 808 };

// หลัง readCalibrate() ค่า F[] จะอยู่ช่วง 0-1000
// ดังนั้น REF[] ต้องอยู่ในสเกลเดียวกัน
int REF[NUM_SENSORS] = { 500, 500, 500, 500, 500, 500, 500, 500 };

//======================================================================
// SENSOR หลัง : MCP3008 ARRAY 8 ตัว
//======================================================================
// Sensor หน้า : adc     -> F[0] ถึง F[7]
// Sensor หลัง : adcBack -> B[0] ถึง B[7]
// MCP3008 ทั้ง 2 ตัวใช้ MOSI / MISO / SCK ร่วมกัน แต่ใช้ CS คนละขา
//======================================================================

#define NUM_BACK_SENSORS 8
#define ADC_BACK_CS      PA7

uint8_t B_PIN[NUM_BACK_SENSORS] = { 0, 1, 2, 3, 4, 5, 6, 7 };

int B[NUM_BACK_SENSORS];
int B_REFmin[NUM_BACK_SENSORS], B_REFmax[NUM_BACK_SENSORS];

int BMinValue[NUM_BACK_SENSORS] = {382,411,260,154,249,138,126,355};
int BMaxValue[NUM_BACK_SENSORS] = {930,937,854,850,922,926,872,872};
int BREF[NUM_BACK_SENSORS] = {500,500,500,500,500,500,500,500};

int BackLastError = 0;
/////////////////////////////////////////////////////////////

//----- ค่าปรับแต่งการทำงาน -----
#define OBJECT_DISTANCE_CM 8.0
#define FOLLOW_TIMEOUT_MS  7000
#define TURN_TIMEOUT_MS    3000
#define BACK_FOLLOW_TIMEOUT_MS 7000

void setup() {
  Serial.begin(115200);
  // adc.begin(cs, mosi, miso, sck);
  adc.begin(PA0, PA1, PA2, PA3);

  // MCP3008 Sensor หลัง 8 ตัว
  // ใช้ SPI bus เดียวกับ Sensor หน้า แต่แยก CS
  adcBack.begin(ADC_BACK_CS, PA1, PA2, PA3);
  // AutoRef();
  ServoHome();
  beep();
  sw_OK_press();  // Wait For Press SW
  beep();
  delay(100);

  // ShowCalibrate();   // Calibrate Light of Sensor
  // SerialF();         // Serial Sensor
  // SerialCalibrate();
  // SerialPosition();

  //----- ทดสอบ Sensor หลัง -----
  // ShowBackCalibrate();
  // SerialBackSensor();
  // SerialBackCalibrate();
  // SerialBackPosition();
}

void loop() {
  m3();

  while (1) {
    MotorStop();
  }  //จบการทำงาน
}

//------ SENSOR -----
void ReadSensor() {  //ค่าดิบ
  for (int _sensor = 0; _sensor < NUM_SENSORS; _sensor++) {
    // F[_sensor] = analog10(F_PIN[_sensor]);
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

    // ป้องกันหารด้วยศูนย์กรณี calibration ผิด
    if (calmax <= calmin) {
      F[i] = 0;
      continue;
    }

    x = map(F[i], calmin, calmax, 1000, 0);
    // ถ้าไม่เกาะเส้นให้แก้ 1000,0 เป็น 0,1000

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

  for (int startSamp = 0; startSamp < samples; startSamp++) {
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
  sw_OK_press();
  Serial.println("Calibrating...");
  beep();
  CalibrateSensor(20, 250);
  beep();
  Serial.println("Finish");
  Serial.println("  ");
  Serial.print("int MinValue[NUM_SENSORS] = {");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(MinValue[i]);
    if (i < NUM_SENSORS - 1)
      Serial.print(",");
    else
      Serial.println("};");

    REFmin[i] = MinValue[i];
  }

  Serial.print("int MaxValue[NUM_SENSORS] = {");

  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(MaxValue[i]);
    if (i < NUM_SENSORS - 1)
      Serial.print(",");
    else
      Serial.println("};");

    REFmax[i] = MaxValue[i];
  }

  // REF ใช้กับค่าที่ผ่าน readCalibrate() แล้ว
  Serial.print("int REF[NUM_SENSORS] = {");

  for (int i = 0; i < NUM_SENSORS; i++) {
    REF[i] = 500;
    Serial.print(REF[i]);
    if (i < NUM_SENSORS - 1)
      Serial.print(",");
    else
      Serial.println("};");
  }

  while (1)
    ;
}


void AutoRef() {
  // F[] หลัง calibration อยู่ 0-1000
  // จุดกึ่งกลางจึงใช้ 500
  for (int i = 0; i < NUM_SENSORS; i++) {
    REF[i] = 500;
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
    delay(20);
  }
}


//======================================================================
// SENSOR หลัง : MCP3008 ARRAY 8 ตัว
//======================================================================

void ReadBackSensor() {
  for (int _sensor = 0; _sensor < NUM_BACK_SENSORS; _sensor++) {
    B[_sensor] = adcBack.analogRead(B_PIN[_sensor]);
  }
}

void readBackCalibrate() {
  ReadBackSensor();

  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    unsigned int calmin = BMinValue[i];
    unsigned int calmax = BMaxValue[i];
    long x = 0;

    if (calmax <= calmin) {
      B[i] = 0;
      continue;
    }

    x = map(B[i], calmin, calmax, 1000, 0);

    if (x < 0) x = 0;
    if (x > 1000) x = 1000;

    B[i] = x;
  }
}

void CalibrateBackSensor(int pauseTime, int samples) {
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    BMinValue[i] = 1023;
    BMaxValue[i] = 0;
  }

  for (int startSamp = 0; startSamp < samples; startSamp++) {
    ReadBackSensor();

    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      BMinValue[i] = (B[i] <= BMinValue[i] ? B[i] : BMinValue[i]);
      BMaxValue[i] = (B[i] >= BMaxValue[i] ? B[i] : BMaxValue[i]);
    }

    delay(pauseTime);
  }
}

void ShowBackCalibrate() {
  Serial.println("Press OK to Start Back Sensor Calibrate");
  sw_OK_press();

  Serial.println("Back Sensor Calibrating...");
  beep();

  CalibrateBackSensor(20, 250);

  beep();
  Serial.println("Finish");

  Serial.print("int BMinValue[NUM_BACK_SENSORS] = {");
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    Serial.print(BMinValue[i]);
    if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    else Serial.println("};");
    B_REFmin[i] = BMinValue[i];
  }

  Serial.print("int BMaxValue[NUM_BACK_SENSORS] = {");
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    Serial.print(BMaxValue[i]);
    if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    else Serial.println("};");
    B_REFmax[i] = BMaxValue[i];
  }

  Serial.print("int BREF[NUM_BACK_SENSORS] = {");
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    BREF[i] = 500;
    Serial.print(BREF[i]);
    if (i < NUM_BACK_SENSORS - 1) Serial.print(",");
    else Serial.println("};");
  }

  while (1);
}

void AutoBackRef() {
  for (int i = 0; i < NUM_BACK_SENSORS; i++) {
    BREF[i] = 500;
  }
}

void SerialBackSensor() {
  while (1) {
    ReadBackSensor();

    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      Serial.print(B[i]);
      Serial.print("\t");
    }

    Serial.println("");
    delay(50);
  }
}

void SerialBackCalibrate() {
  while (1) {
    readBackCalibrate();

    for (int i = 0; i < NUM_BACK_SENSORS; i++) {
      Serial.print(B[i]);
      Serial.print("\t");
    }

    Serial.println("");
    delay(50);
  }
}

int readBackPosition(int keepTrack, int noise) {
  unsigned char i, online = 0;
  unsigned long avg = 0;
  unsigned int sum = 0;
  static int last_value = 3500;

  readBackCalibrate();

  for (i = 0; i < NUM_BACK_SENSORS; i++) {
    int values = B[i];

    if (values > keepTrack) {
      online = 1;
    }

    if (values > noise) {
      avg += (long)(values) * (i * 1000);
      sum += values;
    }
  }

  if (!online || sum == 0) {
    if (last_value < 3500) return 0;
    else return 7000;
  }

  last_value = avg / sum;
  return last_value;
}

void SerialBackPosition() {
  while (1) {
    int Pos = readBackPosition(250, 50);
    int Error_B = Pos - 3500;

    Serial.print("Back Pos = ");
    Serial.print(Pos);
    Serial.print("     Back Error = ");
    Serial.println(Error_B);

    delay(20);
  }
}


//------ MOTOR -----
void Motor(int leftsp, int rightsp) {
  motor(1, leftsp);
  motor(2, rightsp);
}

void MotorStop() {
  Motor(0, 0);
  delay(10);
  // ao(); delay(10);
}

//----- PID ------
int readPosition(int keepTrack, int noise) {
  unsigned char i, online = 0;
  unsigned long avg = 0;
  unsigned int sum = 0;
  static int last_value = 3500;
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

  // ไม่พบเส้น ให้จำทิศทางล่าสุด
  if (!online || sum == 0) {

    if (last_value < 3500) {
      return 0;
    } else {
      return 7000;
    }
  }


  last_value = avg / sum;

  return last_value;
}


void stdPID(int baseSpeed, float Kp, float Kd, int maxsp, int minsp) {
  int Pos = readPosition(250, 50);
  int Error = Pos - 3500;
  int PID_Value = (Kp * Error) + (Kd * (Error - LastError));
  LastError = Error;
  int LeftPower = baseSpeed - PID_Value;
  int RightPower = baseSpeed + PID_Value;
  // จำกัดค่าตาม maxsp/minsp จริง
  if (LeftPower > maxsp) LeftPower = maxsp;
  if (LeftPower < minsp) LeftPower = minsp;
  if (RightPower > maxsp) RightPower = maxsp;
  if (RightPower < minsp) RightPower = minsp;
  Motor(LeftPower, RightPower);
}


void FFtimer(int baseSpeed, float Kp, float Kd, int totalTime, int maxsp, int minsp) {
  unsigned long startTime = millis();
  while (millis() - startTime < (unsigned long)totalTime) {
    stdPID(baseSpeed, Kp, Kd, maxsp, minsp);
  }
  MotorStop();
}

// เดินตามเส้นจนเจอทางตัด
void FF(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  unsigned long startTime = millis();
  LastError = 0;
  while (1) {
    readCalibrate();
    // เจอแถบดำ/ทางตัดเต็มเซนเซอร์
    if (F[0] <= REF[0] &&
        F[1] <= REF[1] &&
        F[2] <= REF[2] &&
        F[3] <= REF[3] &&
        F[4] <= REF[4] &&
        F[5] <= REF[5] &&
        F[6] <= REF[6] &&
        F[7] <= REF[7]) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);
    // ป้องกันหุ่นยนต์ค้างใน while ตลอดไป
    if (millis() - startTime > FOLLOW_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();

  if (select == 's') {
    Motor(-spd, -spd);
    delay(dl);
    startTime = millis();
    while (1) {
      readCalibrate();
      if (F[0] >= REF[0] && F[7] >= REF[7])
        break;
      Motor(-20, -20);
      if (millis() - startTime > 1500)
        break;
    }
    MotorStop();
 
  } else if (select == 'p') {
    Motor(spd, spd);
    delay(dl);
    startTime = millis();
    while (1) {
      readCalibrate();
      if (F[0] >= REF[0] && F[7] >= REF[7])
        break;
      Motor(20, 20);
      if (millis() - startTime > 1500)
        break;
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


void FFC(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  unsigned long startTime = millis();
  LastError = 0;
  while (1) {
    readCalibrate();
    // เจอแถบดำ/ทางตัดเต็มเซนเซอร์
    if (F[0] <= REF[0] &&
        F[7] <= REF[7]) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);
    // ป้องกันหุ่นยนต์ค้างใน while ตลอดไป
    if (millis() - startTime > FOLLOW_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();

  if (select == 's') {
    Motor(-spd, -spd);
    delay(dl);
    startTime = millis();
    while (1) {
      readCalibrate();
      if (F[0] >= REF[0] && F[7] >= REF[7])
        break;
      Motor(-20, -20);
      if (millis() - startTime > 1500)
        break;
    }
    MotorStop();
 
  } else if (select == 'p') {
    Motor(spd, spd);
    delay(dl);
    startTime = millis();
    while (1) {
      readCalibrate();
      if (F[0] >= REF[0] && F[7] >= REF[7])
        break;
      Motor(20, 20);
      if (millis() - startTime > 1500)
        break;
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

// เดินตามเส้นจนเจอด้านซ้าย
void FFL(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  unsigned long startTime = millis();
  LastError = 0;
  while (1) {
    readCalibrate();
    if ((F[0] <= REF[0] && F[4] <= REF[4]) &&
        (F[6] >= REF[6] && F[7] >= REF[7])) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);
    if (millis() - startTime > FOLLOW_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}

// เดินตามเส้นจนเจอด้านขวา
void FFR(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {
  unsigned long startTime = millis();
  LastError = 0;
  while (1) {
    readCalibrate();
    if ((F[7] <= REF[7] && F[3] <= REF[3]) &&
        (F[0] >= REF[0] && F[4] >= REF[4])) {
      break;
    }
    stdPID(spd, kp, kd, maxsp, minsp);

    if (millis() - startTime > FOLLOW_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}

// เดินตรงเข้าหาเส้นกลาง
void FFC(int spd, float kp, float kd) {
  unsigned long startTime = millis();
  while (1) {
    readCalibrate();
    if (F[3] <= REF[3] ||
        F[4] <= REF[4] ||
        F[5] <= REF[5]) {
      break;
    }
    Motor(spd, spd);
    delay(10);
    if (millis() - startTime > FOLLOW_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}


//======================================================================
// PID ถอยหลังตามเส้นด้วย Sensor หลัง 8 ตัว
//======================================================================

void stdPIDBack(int baseSpeed, float Kp, float Kd, int maxsp, int minsp) {
  int Pos = readBackPosition(250, 50);
  int Error = Pos - 3500;

  int PID_Value = (Kp * Error) + (Kd * (Error - BackLastError));
  BackLastError = Error;

  int LeftPower  = -(baseSpeed + PID_Value);
  int RightPower = -(baseSpeed - PID_Value);

  if (LeftPower < -maxsp) LeftPower = -maxsp;
  if (LeftPower > -minsp) LeftPower = -minsp;

  if (RightPower < -maxsp) RightPower = -maxsp;
  if (RightPower > -minsp) RightPower = -minsp;

  Motor(LeftPower, RightPower);
}

void BBtimer(int baseSpeed, float Kp, float Kd,
             int totalTime, int maxsp, int minsp) {
  unsigned long startTime = millis();
  BackLastError = 0;

  while (millis() - startTime < (unsigned long)totalTime) {
    stdPIDBack(baseSpeed, Kp, Kd, maxsp, minsp);
  }

  MotorStop();
}

void BB(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {

  unsigned long startTime = millis();

  BackLastError = 0;

  //============================================================
  // STEP 1 : ถอยหลังตามเส้นด้วย PID
  //          จน Sensor หลังทั้ง 8 ตัวตรวจพบทางตัด
  //============================================================
  while (1) {

    readBackCalibrate();

    // เจอทางตัดเต็มแผง Sensor หลัง
    if (B[0] <= BREF[0] &&
        B[1] <= BREF[1] &&
        B[2] <= BREF[2] &&
        B[3] <= BREF[3] &&
        B[4] <= BREF[4] &&
        B[5] <= BREF[5] &&
        B[6] <= BREF[6] &&
        B[7] <= BREF[7]) {
      break;
    }

    stdPIDBack(spd, kp, kd, maxsp, minsp);

    // ป้องกันถอยค้าง หากหาเส้นตัดไม่เจอ
    if (millis() - startTime > BACK_FOLLOW_TIMEOUT_MS) {

      MotorStop();

      beep();
      delay(100);
      beep();

      return;
    }
  }

  //============================================================
  // STEP 2 : เจอทางตัดแล้ว
  //          ถอยต่อจน Sensor หลังพ้นทางตัดเดิม
  //
  // เหตุผล:
  // ถ้าหยุดคาทางตัด เมื่อเรียก BB() ครั้งต่อไป
  // Sensor จะยังเห็นทางตัดเดิมและหยุดทันที
  //
  // เมื่อถอยพ้นทางตัดก่อนหยุด
  // BB() ครั้งต่อไปจึงสามารถค้นหาทางตัดถัดไปได้
  //============================================================

  Motor(-spd, -spd);
  delay(dl);

  startTime = millis();

  while (1) {

    readBackCalibrate();

    // พ้นทางตัดแล้ว เมื่อ Sensor ขอบซ้ายและขอบขวา
    // กลับมาอยู่บนพื้น/แนวเส้นปกติ
    if (B[0] >= BREF[0] &&
        B[7] >= BREF[7]) {
      break;
    }

    Motor(-20, -20);

    // Timeout ช่วงถอยออกจากทางตัด
    if (millis() - startTime > 1500) {
      break;
    }
  }

  MotorStop();


  //============================================================
  // select
  //
  // 's' = หยุดหลังพ้นทางตัด
  // 'p' = ถอยเพิ่มอีก dl หลังพ้นทางตัด
  //
  // ปกติแนะนำใช้ 's'
  //============================================================
  if (select == 's') {

    MotorStop();

  } else if (select == 'p') {

    Motor(-spd, -spd);
    delay(dl);

    MotorStop();
  }
}

void BBC(int spd, float kp, float kd, char select, int dl, int maxsp, int minsp) {

  unsigned long startTime = millis();

  BackLastError = 0;

  //============================================================
  // STEP 1 : ถอยหลังตามเส้นด้วย PID
  //          จน Sensor หลังทั้ง 8 ตัวตรวจพบทางตัด
  //============================================================
  while (1) {

    readBackCalibrate();

    // เจอทางตัดเต็มแผง Sensor หลัง
    if (B[0] <= BREF[0] &&
        B[7] <= BREF[7]) {
      break;
    }

    stdPIDBack(spd, kp, kd, maxsp, minsp);

    // ป้องกันถอยค้าง หากหาเส้นตัดไม่เจอ
    if (millis() - startTime > BACK_FOLLOW_TIMEOUT_MS) {

      MotorStop();

      beep();
      delay(100);
      beep();

      return;
    }
  }

  //============================================================
  // STEP 2 : เจอทางตัดแล้ว
  //          ถอยต่อจน Sensor หลังพ้นทางตัดเดิม
  //
  // เหตุผล:
  // ถ้าหยุดคาทางตัด เมื่อเรียก BB() ครั้งต่อไป
  // Sensor จะยังเห็นทางตัดเดิมและหยุดทันที
  //
  // เมื่อถอยพ้นทางตัดก่อนหยุด
  // BB() ครั้งต่อไปจึงสามารถค้นหาทางตัดถัดไปได้
  //============================================================

  Motor(-spd, -spd);
  delay(dl);

  startTime = millis();

  while (1) {

    readBackCalibrate();

    // พ้นทางตัดแล้ว เมื่อ Sensor ขอบซ้ายและขอบขวา
    // กลับมาอยู่บนพื้น/แนวเส้นปกติ
    if (B[0] >= BREF[0] &&
        B[7] >= BREF[7]) {
      break;
    }

    Motor(-20, -20);

    // Timeout ช่วงถอยออกจากทางตัด
    if (millis() - startTime > 1500) {
      break;
    }
  }

  MotorStop();


  //============================================================
  // select
  //
  // 's' = หยุดหลังพ้นทางตัด
  // 'p' = ถอยเพิ่มอีก dl หลังพ้นทางตัด
  //
  // ปกติแนะนำใช้ 's'
  //============================================================
  if (select == 's') {

    MotorStop();

  } else if (select == 'p') {

    Motor(-spd, -spd);
    delay(dl);

    MotorStop();
  }
}
//======================================================================
// BBLine()
// ถอยหลังตามเส้นและนับจำนวนทางตัด
//
// ใช้เมื่อจำเป็นต้องถอยผ่านมากกว่า 1 ทางตัด
//
// lineTarget = จำนวนทางตัดที่ต้องการ
//
// ตัวอย่าง:
// BBLine(25, 0.08, 0.05, 3, 's', 20, 60, 5);
//
// หมายถึง:
// ถอยตามเส้น -> ผ่านทางตัดที่ 1 -> ผ่านทางตัดที่ 2
// -> เจอทางตัดที่ 3 -> หยุด
//======================================================================
void BBLine(int spd, float kp, float kd,
            int lineTarget,
            char select, int dl,
            int maxsp, int minsp) {

  int lineCount = 0;
  bool onCrossLine = false;

  unsigned long lastLineTime = millis();

  BackLastError = 0;

  if (lineTarget < 1) {
    lineTarget = 1;
  }

  while (1) {

    readBackCalibrate();

    // ตรวจว่า Sensor หลังทั้ง 8 ตัวอยู่บนทางตัดหรือไม่
    bool crossLine =
      (B[0] <= BREF[0] &&
       B[1] <= BREF[1] &&
       B[2] <= BREF[2] &&
       B[3] <= BREF[3] &&
       B[4] <= BREF[4] &&
       B[5] <= BREF[5] &&
       B[6] <= BREF[6] &&
       B[7] <= BREF[7]);

    // นับเฉพาะจังหวะ "เข้า" ทางตัด
    // ป้องกันนับทางตัดเดียวซ้ำหลายครั้ง
    if (crossLine && !onCrossLine) {

      lineCount++;
      onCrossLine = true;

      Serial.print("Back Line Count = ");
      Serial.println(lineCount);

      beep();

      // ถึงจำนวนทางตัดที่ต้องการแล้ว
      if (lineCount >= lineTarget) {
        break;
      }

      // Reset timeout หลังพบแต่ละทางตัด
      // เพื่อให้วิ่งต่อไปหาทางตัดถัดไป
      lastLineTime = millis();
    }

    // เมื่อออกจากทางตัดแล้ว จึงพร้อมนับทางตัดใหม่
    if (!crossLine) {
      onCrossLine = false;
    }

    // ถอยตามเส้นด้วย PID
    stdPIDBack(spd, kp, kd, maxsp, minsp);

    // ถ้าหาทางตัดถัดไปไม่เจอภายในเวลาที่กำหนด ให้หยุด
    if (millis() - lastLineTime > BACK_FOLLOW_TIMEOUT_MS) {

      MotorStop();

      beep();
      delay(100);
      beep();

      Serial.println("Back Line Timeout");

      return;
    }
  }

  MotorStop();

  // 's' = หยุดตรงทางตัดเป้าหมาย
  if (select == 's') {

    MotorStop();

  }
  // 'p' = ถอยเลยทางตัดเป้าหมายไปอีกเล็กน้อย
  else if (select == 'p') {

    Motor(-spd, -spd);
    delay(dl);

    MotorStop();
  }
}


//------ TURN -----
void SpinL() {
  unsigned long startTime;
  // หมุนซ้าย
  Motor(-60, 60);
  delay(200);
  startTime = millis();
  while (1) {
    readCalibrate();
    // รอจน Sensor ซ้ายกลางพบเส้น
    if (F[3] <= REF[3]) {
      MotorStop();
      // แตะกลับเล็กน้อยลดการหมุนเกิน
      Motor(15, -15);
      delay(15);
      beep();
      break;
    }

    // สำคัญ: ต้องหมุนทิศเดิม ไม่กลับทิศกับช่วงแรก
    Motor(-20, 20);

    if (millis() - startTime > TURN_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}

void SpinR() {
  unsigned long startTime;
  // หมุนขวา
  Motor(60, -60);
  delay(200);
  startTime = millis();
  while (1) {
    readCalibrate();
    // รอจน Sensor ขวากลางพบเส้น
    if (F[4] <= REF[4]) {
      MotorStop();
      // แตะกลับเล็กน้อยลดการหมุนเกิน
      Motor(-15, 15);
      delay(15);
      beep();
      break;
    }
    // สำคัญ: ต้องหมุนทิศเดิม ไม่กลับทิศกับช่วงแรก
    Motor(20, -20);

    if (millis() - startTime > TURN_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}

void UturnR() {
  unsigned long startTime;
  // หมุนขวา
  Motor(60, -60);
  delay(360);
  startTime = millis();
  while (1) {
    readCalibrate();
    // รอจน Sensor ขวากลางพบเส้น
    if (F[5] <= REF[5]) {
      MotorStop();
      // แตะกลับเล็กน้อยลดการหมุนเกิน
      Motor(-20, 20);
      delay(20);
      beep();
      break;
    }
    // สำคัญ: ต้องหมุนทิศเดิม ไม่กลับทิศกับช่วงแรก
    Motor(20, -20);

    if (millis() - startTime > TURN_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}

void TR() {
  Motor(40, -40);
  delay(100);
}


void TL() {
  Motor(-40, 40);
  delay(100);
}

void SpinLB() {
  unsigned long startTime = millis();
  // เริ่มหมุนซ้าย
  Motor(-50, 50);
  delay(100);
  while (1) {
    readBackCalibrate();
    // B[4] เจอเส้นดำ
    if (B[4] <= BREF[4]) {
      MotorStop();
      break;
    }
    // หมุนซ้ายต่อ
    Motor(-20, 20);
    // ป้องกันหมุนค้าง
    if (millis() - startTime > TURN_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
  }
  MotorStop();
}

void SpinRB() {
  unsigned long startTime = millis();
  // เริ่มหมุนขวา
  Motor(40, -40);
  delay(200);
  while (1) {
    readBackCalibrate();
    // B[5] เจอเส้นดำ
    if (B[5] <= BREF[5]) {
      MotorStop();
      break;
    }
    // หมุนขวาต่อ
    Motor(20, -20);
    // ป้องกันหมุนค้าง
    if (millis() - startTime > TURN_TIMEOUT_MS) {
      MotorStop();
      beep();
      return;
    }
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
    delay(20);
  }
}

//------ OBJECT SENSOR -----
float readSonar() {
  const int samples = 10;
  long total = 0;
  for (int i = 0; i < samples; i++) {
    total += analog(A8);
    delay(3);
  }
  float averageADC = total / (float)samples;
  return averageADC / 41.0;
}

// เดินตามเส้นพร้อมตรวจหากล่อง
// ใช้ชื่อ/รูปแบบพารามิเตอร์ใกล้เคียง FF เดิม
bool FFCan(int spd, float kp, float kd, float distanceCM, int maxsp, int minsp) {
  unsigned long startTime = millis();
  LastError = 0;
  while (1) {
    float distance = readSonar();
    // ตรวจระยะให้มากกว่า 0 เพื่อกรองค่าผิดพลาด
    if (distance > 0 && distance <= distanceCM) {
      MotorStop();
      // อ่านยืนยันอีกครั้ง ลดการคีบจาก noise
      delay(60);
      float confirmDistance = readSonar();
      if (confirmDistance > 0 && confirmDistance <= distanceCM + 1.0) {
        beep();
        return true;
      }
    }
    stdPID(spd, kp, kd, maxsp, minsp);
    if (millis() - startTime > FOLLOW_TIMEOUT_MS) {
      MotorStop();
      return false;
    }
  }
}

//======================================================================
// SERVO 3 ตัว
//======================================================================
// Servo ช่อง 1 : ยกชุดก้ามคีบขึ้น / ลง
// Servo ช่อง 2 : ก้ามคีบด้านซ้าย
// Servo ช่อง 3 : ก้ามคีบด้านขวา
//
// เงื่อนไขการทำงาน
// - Servo 1 ทำหน้าที่ยกชุด Servo 2 และ Servo 3 ขึ้น/ลง
// - Servo 2 และ Servo 3 ทำหน้าที่คีบกล่อง
// - ตอนคีบ Servo 2 และ Servo 3 ต้องหุบเข้าพร้อมกัน
// - ตอนปล่อย Servo 2 และ Servo 3 ต้องกางออกพร้อมกัน
// - Servo 2 และ Servo 3 หมุนสวนทางกัน เพราะติดตั้งคนละด้าน
//======================================================================

#define SERVO_LIFT       1
#define SERVO_GRIP_L     2
#define SERVO_GRIP_R     3

//----- Servo 1 : ยกชุดก้ามคีบ -----
#define LIFT_DOWN_ANGLE  0
#define LIFT_UP_ANGLE    90

//----- Servo 2 : ก้ามซ้าย -----
#define GRIP_L_OPEN      30
#define GRIP_L_CLOSE     90

//----- Servo 3 : ก้ามขวา -----
// หมุนสวนทางกับ Servo 2
#define GRIP_R_OPEN      150  //150
#define GRIP_R_CLOSE     90

// ความหน่วงต่อการขยับ 1 องศา
#define SERVO_DELAY      3



// จำตำแหน่งปัจจุบันของ Servo
int ServoLiftPos  = LIFT_UP_ANGLE;
int ServoGripLPos = GRIP_L_OPEN;
int ServoGripRPos = GRIP_R_OPEN;


//----------------------------------------------------------------------
// ServoLiftMove()
// ขยับ Servo 1 ไปยังมุมเป้าหมายแบบค่อย ๆ ขยับ
//----------------------------------------------------------------------
void ServoLiftMove(int target) {
  target = constrain(target, 0, 180);
  while (ServoLiftPos != target) {
    if (ServoLiftPos < target) {
      ServoLiftPos++;
    } else {
      ServoLiftPos--;
    }
    servo(SERVO_LIFT, ServoLiftPos);
    delay(SERVO_DELAY);
  }
}

//----------------------------------------------------------------------
// ServoGripMove()
// ขยับ Servo 2 และ Servo 3 พร้อมกัน
//----------------------------------------------------------------------
void ServoGripMove(int targetL, int targetR) {
  targetL = constrain(targetL, 0, 180);
  targetR = constrain(targetR, 0, 180);
  while (ServoGripLPos != targetL || ServoGripRPos != targetR) {
    if (ServoGripLPos < targetL) {
      ServoGripLPos++;
    } else if (ServoGripLPos > targetL) {
      ServoGripLPos--;
    }

    if (ServoGripRPos < targetR) {
      ServoGripRPos++;
    } else if (ServoGripRPos > targetR) {
      ServoGripRPos--;
    }

    // สั่งก้ามซ้ายและขวาในรอบเดียวกัน
    servo(SERVO_GRIP_L, ServoGripLPos);
    servo(SERVO_GRIP_R, ServoGripRPos);
    delay(SERVO_DELAY);
  }
}

//----------------------------------------------------------------------
// ServoHome()
// ตำแหน่งเริ่มต้นของ Servo ทั้ง 3 ตัว
// Servo 1 = ยกขึ้น
// Servo 2 = กางออก
// Servo 3 = กางออก
//----------------------------------------------------------------------
void ServoHome() {
  MotorStop();
  ServoLiftPos  = LIFT_UP_ANGLE;
  ServoGripLPos = GRIP_L_OPEN;
  ServoGripRPos = GRIP_R_OPEN;
  servo(SERVO_LIFT, ServoLiftPos);
  servo(SERVO_GRIP_L, ServoGripLPos);
  servo(SERVO_GRIP_R, ServoGripRPos);
  delay(500);
}

//----------------------------------------------------------------------
// ServoGrip()
// Servo 2 และ Servo 3 หุบเข้าพร้อมกันเพื่อคีบกล่อง
//----------------------------------------------------------------------
void ServoGrip() {
  ServoGripMove(GRIP_L_CLOSE, GRIP_R_CLOSE);
}

//----------------------------------------------------------------------
// ServoPut()
// Servo 2 และ Servo 3 กางออกพร้อมกันเพื่อปล่อยกล่อง
//----------------------------------------------------------------------
void ServoPut() {
  ServoGripMove(GRIP_L_OPEN, GRIP_R_OPEN);
}

//----------------------------------------------------------------------
// ServoGripDown()
// Servo 1 ลดชุดก้ามคีบลง
//----------------------------------------------------------------------
void ServoGripDown() {
  ServoLiftMove(LIFT_DOWN_ANGLE);
}

//----------------------------------------------------------------------
// ServoGripUp()
// Servo 1 ยกชุดก้ามคีบขึ้น
//----------------------------------------------------------------------
void ServoGripUp() {
  ServoLiftMove(LIFT_UP_ANGLE);
}

//----------------------------------------------------------------------
// Grip()
// หุบ Servo 2 และ Servo 3 เพื่อคีบกล่อง
//----------------------------------------------------------------------
void Grip() {
  ao();
  ServoGrip();
  delay(200);
}

//----------------------------------------------------------------------
// Put()
// กาง Servo 2 และ Servo 3 เพื่อปล่อย หรือเตรียมคีบกล่อง
//----------------------------------------------------------------------
void Put() {
  ao();
  ServoPut();
  delay(100);
}

//----------------------------------------------------------------------
// GripUp()
// Servo 1 ยกชุดก้ามคีบขึ้น
//----------------------------------------------------------------------
void GripUp() {
  ao();
  ServoGripUp();
  delay(100);
}

//----------------------------------------------------------------------
// GripDown()
// Servo 1 ลดชุดก้ามคีบลง
//----------------------------------------------------------------------
void GripDown() {
  ao();
  ServoGripDown();
  delay(100);
}

//----------------------------------------------------------------------
// GripCan()
// ลำดับการคีบกล่อง
//
// 1. หยุดมอเตอร์
// 2. Servo 2 และ 3 กางออก
// 3. Servo 1 ลดชุดก้ามลง
// 4. Servo 2 และ 3 หุบเข้าพร้อมกัน
// 5. Servo 1 ยกชุดก้ามพร้อมกล่องขึ้น
//----------------------------------------------------------------------
void GripCan() {
  MotorStop();
  Put();       // Servo 2 และ 3 กางออก
  GripDown();  // Servo 1 ลดชุดก้ามลง
  delay(100);
  Grip();      // Servo 2 และ 3 หุบเข้าพร้อมกัน
  delay(150);
  GripUp();    // Servo 1 ยกชุดก้ามพร้อมกล่องขึ้น
}

//----------------------------------------------------------------------
// PutCan()
// ลำดับการวางกล่อง
//
// 1. หยุดมอเตอร์
// 2. Servo 1 ลดชุดก้ามพร้อมกล่องลง
// 3. Servo 2 และ 3 กางออกพร้อมกัน
// 4. Servo 1 ยกชุดก้ามขึ้น
//----------------------------------------------------------------------
// void PutCan() {
//   MotorStop();
//   GripDown();  // Servo 1 ลดชุดก้ามลง
//   delay(100);
//   Put();       // Servo 2 และ 3 กางออกพร้อมกัน
//   delay(100);
//   GripUp();    // Servo 1 ยกชุดก้ามขึ้น
// }

void PutCan(int PutAngle) {
  MotorStop();
  ServoLiftMove(PutAngle);
  delay(200);
  Put();
  delay(140);
  ServoGripUp();
}

//----------------------------------------------------------------------
// ServoTest()
// ทดสอบ Servo 1, 2, 3 แยกทีละช่อง
// แล้วทดสอบ Servo 2 และ 3 พร้อมกัน
//
// ใช้ตรวจสอบกรณี Servo ช่อง 3 ไม่ทำงาน
//
// วิธีใช้ชั่วคราว:
//
// void loop() {
//   ServoTest();
//   while (1);
// }
//----------------------------------------------------------------------
void ServoTest() {
  MotorStop();
  Serial.println("================================");
  Serial.println("SERVO TEST");
  Serial.println("Servo 1 = Lift");
  Serial.println("Servo 2 = Left Grip");
  Serial.println("Servo 3 = Right Grip");
  Serial.println("================================");

  //----- ทดสอบ Servo 1 -----
  Serial.println("Test Servo 1 : DOWN");
  servo(SERVO_LIFT, LIFT_DOWN_ANGLE);
  delay(1000);
  Serial.println("Test Servo 1 : UP");
  servo(SERVO_LIFT, LIFT_UP_ANGLE);
  delay(1000);

  //----- ทดสอบ Servo 2 แยกเดี่ยว -----
  Serial.println("Test Servo 2 : OPEN");
  servo(SERVO_GRIP_L, GRIP_L_OPEN);
  delay(1000);
  Serial.println("Test Servo 2 : CLOSE");
  servo(SERVO_GRIP_L, GRIP_L_CLOSE);
  delay(1000);

  //----- ทดสอบ Servo 3 แยกเดี่ยว -----
  Serial.println("Test Servo 3 : OPEN");
  servo(SERVO_GRIP_R, GRIP_R_OPEN);
  delay(1000);
  Serial.println("Test Servo 3 : CLOSE");
  servo(SERVO_GRIP_R, GRIP_R_CLOSE);
  delay(1000);

  // ปรับตัวแปรตำแหน่งให้ตรงกับสถานะที่สั่งไว้
  ServoLiftPos  = LIFT_UP_ANGLE;
  ServoGripLPos = GRIP_L_CLOSE;
  ServoGripRPos = GRIP_R_CLOSE;

  //----- ทดสอบ Servo 2 และ 3 พร้อมกัน -----
  Serial.println("Test Servo 2 + 3 : OPEN");
  ServoPut();
  delay(1000);
  Serial.println("Test Servo 2 + 3 : CLOSE");
  ServoGrip();
  delay(1000);
  Serial.println("Test Servo 2 + 3 : OPEN");
  ServoPut();
  delay(1000);

  // กลับสู่ตำแหน่งพร้อมเริ่ม Mission
  ServoLiftMove(LIFT_UP_ANGLE);
  Serial.println("SERVO TEST FINISH");
  Serial.println("================================");
  MotorStop();
}