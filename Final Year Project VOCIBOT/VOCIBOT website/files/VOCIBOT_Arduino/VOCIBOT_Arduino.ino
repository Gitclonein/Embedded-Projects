// ============================================================
//  VOCIBOT - Arduino Uno Firmware  v2.0
//  Authors : Reshma Satpute, Vedant Pathrabe, Raksha Sonewane,
//            Lavanya Gabhane, Mahak Gupta
//  Guide   : Dr J.Y. Hande
//  College : Priyadarshini Bhagwati College of Engineering, Nagpur
//
//  PROTOCOL (all messages end with \n):
//
//  APP → ARDUINO:
//    F / B / L / R / S   Motion
//    1 / 2 / 3           Speed slow / medium / fast
//    H G A E K           Greetings (hello / morning / afternoon / evening / night)
//    N I W T             Name / Introduce / How-are-you / Thank-you
//    D                   Read distance now
//    POLL                Sensor status poll (auto every 2 s from app)
//    SPEAK:<text>        AI answer received → Arduino beeps, echo back
//
//  ARDUINO → APP:
//    VOCIBOT_READY       On boot
//    SAY:<text>          App will speak this text aloud
//    OK:<action>         Motion confirmed
//    BLOCKED:<dist>      Forward blocked by obstacle
//    DISTANCE:<cm>       Sensor reading
//    SENSOR:<d>|<path>|<spd>   Poll response
//    SPEAKING:<text>     Echo after SPEAK: received
//    SPEED:<label>       Speed changed
//    VERSION:<info>      System info
// ============================================================

#include <SoftwareSerial.h>

// ── Pins ─────────────────────────────────────────────────────
#define IN1  2
#define IN2  3
#define IN3  4
#define IN4  5
#define ENA  6    // PWM left
#define ENB  9    // PWM right
#define TRIG 10
#define ECHO 11
#define BUZ  12

// HC-05: phone TXD → Arduino D7, phone RXD ← Arduino D8
// Add 1kΩ+2kΩ voltage divider on D8→HC-05-RX line
SoftwareSerial BT(7, 8);

// ── Speeds ───────────────────────────────────────────────────
#define SPD_SLOW   90
#define SPD_MED   170
#define SPD_FAST  220
#define SPD_TURN  150
#define OBS_CM     20    // block forward if < 20 cm

int  spd  = SPD_MED;
bool obs  = false;

// ── Buffer for multi-char BT messages ────────────────────────
String buf = "";

// ═════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  int pins[] = {IN1, IN2, IN3, IN4, ENA, ENB, BUZ};

  for (int i = 0; i < 7; i++) {
    pinMode(pins[i], OUTPUT);
  }

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  stopAll();

  // 3-beep boot sequence
  beep(80); delay(90); beep(80); delay(90); beep(180);

  Serial.println(F("VOCIBOT v2.0 ready"));
  BT.println(F("VOCIBOT_READY|VOCIBOT|Arduino Uno|HC-05|L293D|HC-SR04"));
}
// ═════════════════════════════════════════════════════════════
void loop() {
  long d = dist();
  obs = (d > 0 && d < OBS_CM);

  while (BT.available()) {
    char c = (char)BT.read();
    if (c == '\n' || c == '\r') {
      buf.trim();
      if (buf.length()) { handle(buf, d); buf = ""; }
    } else {
      buf += c;
      if (buf.length() > 220) buf = ""; // safety flush
    }
  }
  delay(40);
}

// ═════════════════════════════════════════════════════════════
void handle(String m, long d) {
  Serial.print(F("RX:")); Serial.println(m);

  // ── Multi-char commands ───────────────────────────────────
  if (m == "POLL") {
    BT.print(F("SENSOR:")); BT.print(d);
    BT.print(obs ? F("|BLOCKED|") : F("|CLEAR|"));
    BT.println(spd == SPD_SLOW ? F("SLOW") : spd == SPD_FAST ? F("FAST") : F("MEDIUM"));
    return;
  }
  if (m.startsWith("SPEAK:")) {
    // App sent us the Gemini answer — double-beep and echo
    beep(70); delay(55); beep(70);
    BT.print(F("SPEAKING:")); BT.println(m.substring(6));
    return;
  }

  // ── Single-char commands ──────────────────────────────────
  if (m.length() != 1) { BT.print(F("UNKNOWN:")); BT.println(m); return; }
  char c = m.charAt(0);
  if (c>='a'&&c<='z') c-=32;

  switch(c) {

    // MOTION
    case 'F':
      if (!obs){ fwd(); BT.println(F("OK:FORWARD")); }
      else { stopAll(); BT.print(F("BLOCKED:")); BT.println(d); beep(350); }
      break;
    case 'B': bwd(); BT.println(F("OK:BACKWARD")); break;
    case 'L': lft(); BT.println(F("OK:LEFT"));     break;
    case 'R': rgt(); BT.println(F("OK:RIGHT"));    break;
    case 'S': stopAll(); BT.println(F("OK:STOP")); break;

    // SPEED
    case '1': spd=SPD_SLOW;  BT.println(F("SPEED:SLOW"));   break;
    case '2': spd=SPD_MED;   BT.println(F("SPEED:MEDIUM")); break;
    case '3': spd=SPD_FAST;  BT.println(F("SPEED:FAST"));   break;

    // GREETINGS – Arduino sends SAY: → app speaks via TTS
    case 'H':
      stopAll(); dblBeep();
      BT.println(F("SAY:Hello! I am VOCIBOT, your voice-controlled AI robot. How can I help you today?"));
      break;
    case 'G':
      stopAll(); dblBeep();
      BT.println(F("SAY:Good morning! VOCIBOT here, fully charged and ready to assist you!"));
      break;
    case 'A':
      stopAll(); dblBeep();
      BT.println(F("SAY:Good afternoon! VOCIBOT at your service. What would you like me to do?"));
      break;
    case 'E':
      stopAll(); dblBeep();
      BT.println(F("SAY:Good evening! VOCIBOT checking in. Ready for evening operations!"));
      break;
    case 'K':
      stopAll(); dblBeep();
      BT.println(F("SAY:Good night! VOCIBOT says rest well. I will be here when you return!"));
      break;
    case 'N':
      stopAll(); triBeep();
      BT.println(F("SAY:My name is VOCIBOT — Voice Controlled Intelligent Bot! Built at Priyadarshini Bhagwati College of Engineering, Nagpur by Reshma, Vedant, Raksha, Lavanya, and Mahak!"));
      break;
    case 'I':
      stopAll(); dblBeep();
      BT.println(F("SAY:I am VOCIBOT! I run on Arduino Uno with HC-05 Bluetooth, L293D motor driver, ultrasonic sensor, and ESP32-CAM for live video. My AI runs through Google Gemini on your phone!"));
      break;
    case 'W':
      stopAll(); dblBeep();
      BT.println(F("SAY:I am running perfectly! Motors, sensors, Bluetooth, and camera are all operational. How can I assist you?"));
      break;
    case 'T':
      dblBeep();
      BT.println(F("SAY:You are most welcome! That is what VOCIBOT is here for!"));
      break;

    // SENSOR
    case 'D':
      BT.print(F("DISTANCE:")); BT.println(d);
      break;

    case 'V':
      BT.println(F("VERSION:VOCIBOT v2.0 | Arduino Uno | HC-05 | L293D | HC-SR04 | ESP32-CAM | Gemini AI"));
      break;

    default:
      BT.print(F("UNKNOWN:")); BT.println(c);
  }
}

// ═════════════════════════════════════════════════════════════
//  Motor helpers
// ═════════════════════════════════════════════════════════════
void fwd(){
  analogWrite(ENA,spd); analogWrite(ENB,spd);
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
}
void bwd(){
  analogWrite(ENA,spd); analogWrite(ENB,spd);
  digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH);
  digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH);
}
void lft(){
  analogWrite(ENA,SPD_TURN); analogWrite(ENB,SPD_TURN);
  digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);
}
void rgt(){
  analogWrite(ENA,SPD_TURN); analogWrite(ENB,SPD_TURN);
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH);
}
void stopAll(){
  analogWrite(ENA,0); analogWrite(ENB,0);
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW);
}

// ═════════════════════════════════════════════════════════════
//  Ultrasonic
// ═════════════════════════════════════════════════════════════
long dist(){
  digitalWrite(TRIG,LOW); delayMicroseconds(2);
  digitalWrite(TRIG,HIGH); delayMicroseconds(10);
  digitalWrite(TRIG,LOW);
  long dur = pulseIn(ECHO,HIGH,25000UL);
  return dur ? dur*0.034/2 : 999;
}

// ═════════════════════════════════════════════════════════════
//  Buzzer patterns
// ═════════════════════════════════════════════════════════════
void beep(int ms){ digitalWrite(BUZ,HIGH); delay(ms); digitalWrite(BUZ,LOW); }
void dblBeep(){ beep(80); delay(60); beep(80); }
void triBeep(){ beep(80); delay(55); beep(80); delay(55); beep(120); }
