#include "CEC_Device.h"

// ------------------------------------------------------------
// Sony S-Link
// ------------------------------------------------------------

const byte SLINK_OUTPUT_PIN = 4;
const byte SLINK_INPUT_PIN  = 18;
const byte SLINK_PULSE_BUFFER_SIZE = 200;
const byte SONY_VOLUME_STEPS_PER_CEC_PRESS = 3; // damit ein Mal Drücken auf der FB spürbar die Lautstärke ändert
const unsigned int SONY_VOLUME_STEP_DELAY_MS = 60;

volatile unsigned long slinkTimeLowTransition = 0;
volatile byte slinkBufferReadPosition = 0;
volatile byte slinkBufferWritePosition = 0;
volatile byte slinkPulseBuffer[SLINK_PULSE_BUFFER_SIZE];

String slinkPulseLengths;

void setupSlink()
{
  pinMode(SLINK_OUTPUT_PIN, OUTPUT);
  digitalWrite(SLINK_OUTPUT_PIN, LOW);

  pinMode(SLINK_INPUT_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(SLINK_INPUT_PIN), slinkBusChange, CHANGE);

  Serial.println("Sony S-Link initialized");
}

void slinkBusChange()
{
  static unsigned long timeOfPreviousInterrupt = 0;
  unsigned long timeNow = micros();

  if (timeNow - timeOfPreviousInterrupt < 100) {
    return;
  }

  timeOfPreviousInterrupt = timeNow;

  int busState = digitalRead(SLINK_INPUT_PIN);

  if (busState == LOW) {
    slinkTimeLowTransition = timeNow;
    return;
  }

  int timeLow = timeNow - slinkTimeLowTransition;

  if ((slinkBufferWritePosition + 1) % SLINK_PULSE_BUFFER_SIZE == slinkBufferReadPosition) {
    // Keine Serial-Ausgabe im Interrupt
    return;
  }

  slinkPulseBuffer[slinkBufferWritePosition] = min(255, timeLow / 10);
  slinkBufferWritePosition = (slinkBufferWritePosition + 1) % SLINK_PULSE_BUFFER_SIZE;
}

bool isSlinkBusIdle()
{
  noInterrupts();
  bool idle = micros() - slinkTimeLowTransition > 1200 + 600 + 20000;
  interrupts();

  return idle;
}

void sendSlinkPulseDelimiter()
{
  digitalWrite(SLINK_OUTPUT_PIN, LOW);
  delayMicroseconds(600);
}

void sendSlinkSyncPulse()
{
  digitalWrite(SLINK_OUTPUT_PIN, HIGH);
  delayMicroseconds(2400);
  sendSlinkPulseDelimiter();
}

void sendSlinkBit(int bit)
{
  digitalWrite(SLINK_OUTPUT_PIN, HIGH);

  if (bit) {
    delayMicroseconds(1200);
  }
  else {
    delayMicroseconds(600);
  }

  sendSlinkPulseDelimiter();
}

void sendSlinkByte(int value)
{
  for (int i = 7; i >= 0; --i) {
    sendSlinkBit(bitRead(value, i));
  }
}

void idleAfterSlinkCommand()
{
  delayMicroseconds(20000);
}

bool sendSlinkCommand(const byte command[], int commandLength)
{
  if (!isSlinkBusIdle()) {
    Serial.println("Sony S-Link bus busy, command not sent");
    return false;
  }

  noInterrupts();

  sendSlinkSyncPulse();

  for (int i = 0; i < commandLength; ++i) {
    sendSlinkByte(command[i]);
  }

  // Beim Mega sind Interrupt-Flags anders als beim Uno.
  // Für den Anfang kann man das Weglöschen der EIFR-Flags weglassen
  // oder später gezielt für den verwendeten Interrupt ergänzen.

  interrupts();

  idleAfterSlinkCommand();

  return true;
}

void processSlinkInput()
{
  static byte currentByte = 0;
  static byte currentBit = 0;
  static bool partialOutput = false;

  while (slinkBufferReadPosition != slinkBufferWritePosition) {
    int timeLow = slinkPulseBuffer[slinkBufferReadPosition] * 10;

    slinkBufferReadPosition = (slinkBufferReadPosition + 1) % SLINK_PULSE_BUFFER_SIZE;

    if (timeLow > 2000) {
      slinkPulseLengths = String();
    }
    else {
      slinkPulseLengths += " ";
    }

    slinkPulseLengths += String(timeLow, DEC);

    if (timeLow > 2000) {
      if (partialOutput) {
        if (currentBit != 0) {
          Serial.print("S-Link: !Discarding ");
          Serial.print(currentBit);
          Serial.print(" stray bits");
        }

        Serial.println();
        partialOutput = false;
      }

      currentBit = 0;
      continue;
    }

    partialOutput = true;
    currentBit += 1;

    if (timeLow > 900) {
      bitSet(currentByte, 8 - currentBit);
    }
    else {
      bitClear(currentByte, 8 - currentBit);
    }

    if (currentBit == 8) {
      Serial.print("S-Link RX byte: 0x");

      if (currentByte <= 0x0F) {
        Serial.print("0");
      }

      Serial.println(currentByte, HEX);

      currentBit = 0;
    }
  }

  if (partialOutput && isSlinkBusIdle()) {
    Serial.println();
    partialOutput = false;
  }
}

// -------------------------------------------------------------
// Funktionen zum Aufruf von Sony-Kommandos
// -------------------------------------------------------------
void sonyPowerOn()
{
  const byte cmd[] = { 0xC0, 0x2E };
  Serial.println("Sony S-Link TX: POWER ON");
  sendSlinkCommand(cmd, sizeof(cmd));
}

void sonyPowerOff()
{
  const byte cmd[] = { 0xC0, 0x2F };
  Serial.println("Sony S-Link TX: POWER OFF");
  sendSlinkCommand(cmd, sizeof(cmd));
}

void sonyVolumeUp()
{
  const byte cmd[] = { 0xC0, 0x14 };
  Serial.println("Sony S-Link TX: VOLUME UP");
  sendSlinkCommand(cmd, sizeof(cmd));
}

void sonyVolumeDown()
{
  const byte cmd[] = { 0xC0, 0x15 };
  Serial.println("Sony S-Link TX: VOLUME DOWN");
  sendSlinkCommand(cmd, sizeof(cmd));
}

void sonyMute()
{
  const byte cmd[] = { 0xC0, 0x06 };
  Serial.println("Sony S-Link TX: MUTE");
  sendSlinkCommand(cmd, sizeof(cmd));
}

void sonyUnmute()
{
  const byte cmd[] = { 0xC0, 0x07 };
  Serial.println("Sony S-Link TX: UNMUTE");
  sendSlinkCommand(cmd, sizeof(cmd));
}

void sonyVolumeUpSteps(byte steps)
{
  for (byte i = 0; i < steps; i++) {
    sonyVolumeUp();

    if (i + 1 < steps) {
      delay(SONY_VOLUME_STEP_DELAY_MS);
    }
  }
}

void sonyVolumeDownSteps(byte steps)
{
  for (byte i = 0; i < steps; i++) {
    sonyVolumeDown();

    if (i + 1 < steps) {
      delay(SONY_VOLUME_STEP_DELAY_MS);
    }
  }
}

// --------------------------------------------------------------
// HDMI CEC Teil
// --------------------------------------------------------------

#define IN_LINE 2
#define OUT_LINE 3
#define HPD_LINE 10

// Debug-Makro für Antworten in OnReceive()
#define report(X) do { DbgPrint("report " #X "\n"); report ## X (); } while (0)

#define phy1 ((_physicalAddress >> 8) & 0xFF)
#define phy2 ((_physicalAddress >> 0) & 0xFF)

class MyCEC: public CEC_Device {
public:
  bool systemAudioMode = true;
  unsigned char currentVolume = 30;  // 0..100
  bool muted = false;

  MyCEC(int physAddr): CEC_Device(physAddr, IN_LINE, OUT_LINE) { }

  // ------------------------------------------------------------
  // Allgemeine CEC-Reports
  // ------------------------------------------------------------

  // void reportPhysAddr() {
  //   unsigned char frame[4] = { 0x84, phy1, phy2, 0x05 };

  //   bool ok = TransmitFrame(0x0F, frame, sizeof(frame));

  //   Serial.print("TransmitFrame Report Physical Address returned: ");
  //   Serial.println(ok ? "true" : "false");

  //   Serial.print("Sent: Report Physical Address ");
  //   Serial.print(phy1, HEX);
  //   Serial.print(" ");
  //   Serial.print(phy2, HEX);
  //   Serial.println(" type=Audio System");
  // }

  void reportPhysAddr() {
  unsigned char frame[4] = { 0x84, phy1, phy2, 0x05 };

  Serial.print("About to send CEC frame: ");
  Serial.print((0x05 << 4) | 0x0F, HEX);
  Serial.print(" ");
  for (int i = 0; i < 4; i++) {
    if (frame[i] < 0x10) Serial.print("0");
    Serial.print(frame[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  bool ok = TransmitFrame(0x0F, frame, sizeof(frame));

  Serial.print("TransmitFrame Report Physical Address returned: ");
  Serial.println(ok ? "true" : "false");
}

  void reportPowerState() {
    // 0x90 = Report Power Status
    // 0x00 = On
    unsigned char frame[2] = { 0x90, 0x00 };
    TransmitFrame(0x00, frame, sizeof(frame)); // an TV

    Serial.println("Sent: Report Power Status ON");
  }

  void reportCECVersion() {
    // 0x9E = CEC Version
    // 0x04 = CEC Version 1.3a
    unsigned char frame[2] = { 0x9E, 0x04 };
    TransmitFrame(0x00, frame, sizeof(frame)); // an TV

    Serial.println("Sent: CEC Version 1.3a");
  }

  void reportOSDName() {
    // 0x47 = Set OSD Name
    // Name: AVR
    unsigned char frame[8] = { 0x47, 'A', 'r', 'd', 'u', 'i', 'n', 'o' };
    TransmitFrame(0x00, frame, sizeof(frame)); // an TV

    Serial.println("Sent: OSD Name Arduino");
  }

  void reportVendorID() {
    // 0x87 = Device Vendor ID
    // Fake Vendor ID: 00:F1:0E
    unsigned char frame[4] = { 0x87, 0x00, 0xF1, 0x0E };
    TransmitFrame(0x00, frame, sizeof(frame)); // an TV

    Serial.println("Sent: Device Vendor ID 00:F1:0E");
  }

  void printLogicalAddress() {
    Serial.print("Logical address: 0x");
    Serial.println(_logicalAddress, HEX);
  }
  
  void announceAudioSystem() {
    Serial.println("Announcing HDMI-CEC Audio System");

    reportPhysAddr();
    delay(200);

    reportOSDName();
    delay(200);

    reportVendorID();
    delay(200);

    reportPowerState();
    delay(200);

    reportSystemAudioModeStatus();
    delay(200);

    reportAudioStatus();
  }

  // void OnReady() {
  //   Serial.println("CEC OnReady called");

  //   if (_logicalAddress != 0x05) {
  //     _logicalAddress = 0x05;
  //   }

  //   printLogicalAddress();

  //   delay(200);
  //   reportPhysAddr();

  //   delay(200);
  //   reportOSDName();

  //   delay(200);
  //   reportVendorID();

  //   delay(200);
  //   reportSystemAudioModeStatus();

  //   delay(200);
  //   reportAudioStatus();
  // } 
  
  void OnReady() {
    Serial.println("CEC OnReady called");
    printLogicalAddress();

    if (_logicalAddress != 0x05) {
      Serial.print("OnReady: logical address is 0x");
      Serial.print(_logicalAddress, HEX);
      Serial.println(", forcing to 0x5 now.");
      _logicalAddress = 0x05;
      printLogicalAddress();
    }
    announceAudioSystem();

    // Serial.println("Ready. Send 'a' manually.");
    Serial.println("Ready.");
  }

  void forceAudioLogicalAddress() {
    _logicalAddress = 0x05;
    Serial.println("Forced logical address to 0x5");
  }
  
  void testTransmitCallback() {
    OnTransmitComplete(true);
  }


  // ------------------------------------------------------------
  // Audio-spezifische CEC-Reports
  // ------------------------------------------------------------

  void reportAudioStatus() {
    unsigned char status = currentVolume & 0x7F;

    if (muted) {
      status |= 0x80;
    }

    unsigned char frame[2] = { 0x7A, status };

    bool ok = TransmitFrame(0x00, frame, sizeof(frame));

    Serial.print("TransmitFrame Report Audio Status returned: ");
    Serial.println(ok ? "true" : "false");

    Serial.print("Sent: Report Audio Status volume=");
    Serial.print(currentVolume);
    Serial.print(" muted=");
    Serial.println(muted ? "yes" : "no");
  }

  void reportSystemAudioModeStatus() {
    unsigned char frame[2] = { 0x7E, systemAudioMode ? 0x01 : 0x00 };

    bool ok = TransmitFrame(0x00, frame, sizeof(frame));

    Serial.print("TransmitFrame System Audio Mode Status returned: ");
    Serial.println(ok ? "true" : "false");

    Serial.print("Sent: System Audio Mode Status ");
    Serial.println(systemAudioMode ? "ON" : "OFF");
  }

  void requestSystemAudioMode() {
    // 0x70 = System Audio Mode Request
    // Parameter: physische Adresse des Audio Systems
    unsigned char frame[3] = { 0x70, phy1, phy2 };
    TransmitFrame(0x00, frame, sizeof(frame));  // an TV

    Serial.print("Sent: System Audio Mode Request for ");
    if (phy1 < 0x10) Serial.print("0");
    Serial.print(phy1, HEX);
    Serial.print(" ");
    if (phy2 < 0x10) Serial.print("0");
    Serial.println(phy2, HEX);
  }

  // ------------------------------------------------------------
  // Fernbedienungstasten / User Control
  // ------------------------------------------------------------

  void handleKey(unsigned char key) {
    Serial.print("CEC key pressed: 0x");
    if (key < 0x10) Serial.print("0");
    Serial.print(key, HEX);
    Serial.print(" -> ");

    switch (key) {
    case 0x41:
      Serial.println("VOLUME UP");

      if (muted) {
        muted = false;
        sonyUnmute();
      }

      sonyVolumeUpSteps(SONY_VOLUME_STEPS_PER_CEC_PRESS);

      if (currentVolume <= 100 - SONY_VOLUME_STEPS_PER_CEC_PRESS) {
        currentVolume += SONY_VOLUME_STEPS_PER_CEC_PRESS;
      }
      else {
        currentVolume = 100;
      }

      reportAudioStatus();
      break;

      case 0x42:
        Serial.println("VOLUME DOWN");

        if (muted) {
          muted = false;
          sonyUnmute();
        }

        sonyVolumeDownSteps(SONY_VOLUME_STEPS_PER_CEC_PRESS);

        if (currentVolume >= SONY_VOLUME_STEPS_PER_CEC_PRESS) {
          currentVolume -= SONY_VOLUME_STEPS_PER_CEC_PRESS;
        }
        else {
          currentVolume = 0;
        }

        reportAudioStatus();
        break;

      case 0x43:
        Serial.println("MUTE");

        muted = !muted;

        if (muted) {
          sonyMute();
        } else {
          sonyUnmute();
        }

        reportAudioStatus();
        break;

      case 0x00:
        Serial.println("SELECT / OK");
        break;

      case 0x01:
        Serial.println("UP");
        break;

      case 0x02:
        Serial.println("DOWN");
        break;

      case 0x03:
        Serial.println("LEFT");
        break;

      case 0x04:
        Serial.println("RIGHT");
        break;

      case 0x0D:
        Serial.println("EXIT / BACK");
        break;

       case 0x6B:
      Serial.println("POWER");
      sonyPowerOn();
      reportPowerState();
      break;

      case 0x6C:
        Serial.println("POWER OFF FUNCTION");
        sonyPowerOff();
        break;

      case 0x6D:
        Serial.println("POWER TOGGLE FUNCTION");
        break;

      case 0x6E:
        Serial.println("POWER ON FUNCTION");
        sonyPowerOn();
        reportPowerState();
        break;

      default:
        Serial.println("UNKNOWN");
        break;
    }
  }

  // ------------------------------------------------------------
  // Empfangene CEC-Nachrichten
  // ------------------------------------------------------------

  void OnReceive(int source, int dest, unsigned char* buffer, int count) {
    Serial.print("RX CEC source=0x");
    Serial.print(source, HEX);
    Serial.print(" dest=0x");
    Serial.print(dest, HEX);
    Serial.print(" count=");
    Serial.print(count);
    Serial.print(" data=");

    for (int i = 0; i < count; i++) {
      if (buffer[i] < 0x10) Serial.print("0");
      Serial.print(buffer[i], HEX);
      Serial.print(" ");
    }

    Serial.println();

    if (count == 0) return;

    Serial.print("CEC received from ");
    Serial.print(source, HEX);
    Serial.print(" to ");
    Serial.print(dest, HEX);
    Serial.print(", opcode: 0x");
    if (buffer[0] < 0x10) Serial.print("0");
    Serial.println(buffer[0], HEX);

    switch (buffer[0]) {

      case 0x36:
        // Standby
        Serial.println("CEC command: STANDBY");
        sonyPowerOff();
        break;

      case 0x83:
        // Give Physical Address
        Serial.println("CEC command: GIVE PHYSICAL ADDRESS");
        reportPhysAddr();
        break;

      case 0x86:
        // Set Stream Path
        // Für Audio System NICHT mit Active Source antworten.
        if (count >= 3) {
          Serial.print("CEC command: SET STREAM PATH ");
          if (buffer[1] < 0x10) Serial.print("0");
          Serial.print(buffer[1], HEX);
          Serial.print(" ");
          if (buffer[2] < 0x10) Serial.print("0");
          Serial.println(buffer[2], HEX);
          Serial.println("Ignored: Audio System does not send Active Source.");
        }
        break;

      case 0x8F:
        // Give Device Power Status
        Serial.println("CEC command: GIVE DEVICE POWER STATUS");
        reportPowerState();
        break;

      case 0x9F:
        // Get CEC Version
        Serial.println("CEC command: GET CEC VERSION");
        reportCECVersion();
        break;

      case 0x46:
        // Give OSD Name
        Serial.println("CEC command: GIVE OSD NAME");
        reportOSDName();
        break;

      case 0x8C:
        // Give Device Vendor ID
        Serial.println("CEC command: GIVE DEVICE VENDOR ID");
        reportVendorID();
        break;

      case 0x70:
        // System Audio Mode Request
        Serial.println("CEC command: SYSTEM AUDIO MODE REQUEST");
        sonyPowerOn();
        systemAudioMode = true;
        reportSystemAudioModeStatus();
        reportAudioStatus();
        break;

      case 0x72:
        // Set System Audio Mode
        if (count >= 2) {
          systemAudioMode = buffer[1] != 0x00;
          Serial.print("CEC command: SET SYSTEM AUDIO MODE ");
          Serial.println(systemAudioMode ? "ON" : "OFF");
          reportSystemAudioModeStatus();
        } else {
          Serial.println("CEC command: SET SYSTEM AUDIO MODE but parameter missing");
        }
        break;

      case 0x7D:
        // Give System Audio Mode Status
        Serial.println("CEC command: GIVE SYSTEM AUDIO MODE STATUS");
        reportSystemAudioModeStatus();
        break;

      case 0x71:
        // Give Audio Status
        Serial.println("CEC command: GIVE AUDIO STATUS");
        reportAudioStatus();
        break;

      case 0x44:
        // User Control Pressed
        if (count >= 2) {
          Serial.println("CEC command: USER CONTROL PRESSED");
          handleKey(buffer[1]);
        } else {
          Serial.println("CEC command: USER CONTROL PRESSED but key byte missing");
        }
        break;

      case 0x45:
        // User Control Released
        Serial.println("CEC command: USER CONTROL RELEASED");
        break;

      default:
        Serial.println("CEC command: unhandled, passing to base class");
        CEC_Device::OnReceive(source, dest, buffer, count);
        break;
    }
  }
};

// ------------------------------------------------------------
// Physische Adresse
// ------------------------------------------------------------
// 0x2000 entspricht HDMI-Adresse 2.0.0.0.
// Falls dein Gerät an HDMI 1 hängt: 0x1000
// HDMI 2: 0x2000
// HDMI 3: 0x3000
// HDMI 4: 0x4000
MyCEC device(0x3000);

void printHelp() {
  Serial.println("Available commands:");
  Serial.println("  a = announce physical address as Audio System");
  Serial.println("  m = report System Audio Mode Status");
  Serial.println("  r = report Audio Status");
  Serial.println("  + = volume up locally");
  Serial.println("  - = volume down locally");
  Serial.println("  x = toggle mute locally");
  Serial.println("  p = force promiscuous/monitor mode ON");
  Serial.println("  h = read hotplug state");
  Serial.println("  v = request vendor ID from TV");
  Serial.println("  q = request System Audio Mode from TV");
  Serial.println("  ? = help");
}

void setup()
{
  pinMode(HPD_LINE, INPUT);

  Serial.begin(115200);
  delay(1000);

  setupSlink();

  Serial.println("CEC device starting...");
  Serial.println("Mode: Audio Device + Sony S-Link");

  // Für Debugging aktivieren
  device.MonitorMode = false; // wenn true wird nichts gesendet
  device.Promiscuous = true;

  // Als Audio System initialisieren
  // Falls deine Library einen anderen Namen verwendet,
  // in CEC_Device.h nach dem passenden Enum suchen.
  device.Initialize(CEC_LogicalDevice::CDT_AUDIO_SYSTEM);
  
  // NICHT direkt erzwingen
  // device.forceAudioLogicalAddress();
  
  device.printLogicalAddress();

  // Zur Sicherheit nach Initialize erneut setzen
  device.MonitorMode = false; // wenn true wird nichts gesendet
  device.Promiscuous = true;

  Serial.println("CEC device initialized.");
  Serial.println("Device ready");

  printHelp();
}

void loop()
{
  processSlinkInput();

  if (Serial.available())
  {

    unsigned char c = Serial.read();

    // Zeilenenden vom Serial Monitor ignorieren
    if (c == '\r' || c == '\n') {
      device.Run();
      return;
    }

    Serial.print("Command: ");
    Serial.println((char)c);

    unsigned char buffer[2] = { c, 0 };

    switch (c)
    {
      case 'a':
        device.reportPhysAddr();
        break;

      case 'm':
        device.reportSystemAudioModeStatus();
        break;

      case 'r':
        device.reportAudioStatus();
        break;

      case '+':
        if (device.currentVolume < 100) {
          device.currentVolume++;
        }
        device.muted = false;
        device.reportAudioStatus();
        break;

      case '-':
        if (device.currentVolume > 0) {
          device.currentVolume--;
        }
        device.muted = false;
        device.reportAudioStatus();
        break;

      case 'x':
        device.muted = !device.muted;
        device.reportAudioStatus();
        break;

      case 'p':
        // Nicht toggeln, sondern für Debugging immer einschalten
        device.MonitorMode = true;
        device.Promiscuous = true;
        Serial.println("MonitorMode: ON");
        Serial.println("Promiscuous mode: ON");
        break;

      case 'h':
        Serial.print("Hotplug state: ");
        Serial.println(digitalRead(HPD_LINE));
        break;

      case 'v':
      {
        unsigned char frame[1] = { 0x8C }; // Give Device Vendor ID

        Serial.println("About to send CEC frame: 50 8C");

        bool ok = device.TransmitFrame(0x00, frame, sizeof(frame));

        Serial.print("TransmitFrame Give Device Vendor ID returned: ");
        Serial.println(ok ? "true" : "false");

        unsigned long start = millis();
        while (millis() - start < 3000) {
          device.Run();
        }

        Serial.println("TX run window done");
        break;
      }

      case 'q':
        device.requestSystemAudioMode();
        break;

      case '?': 
        printHelp();
        break;
  
      case 't':
        device.testTransmitCallback();
        break;
  
      default:
        Serial.println("Unknown command");
        printHelp();
        break;
    }
  }

  device.Run();
}