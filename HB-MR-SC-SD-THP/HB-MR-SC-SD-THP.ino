// =======================================================
// ==   -=> LoRa to HomeMatic  Garagensensor V1.0 <=-   ==
// =======================================================
// ==                                                   ==
// == Selbstbau HomeMatic Gerät eines Garagensensors,   ==
// == an den die Sensoren direkt angeschlossen werden   ==
// == können, oder für weitere Distanzen, diese über    ==
// == LoRa angesteuert werden können                    ==
// ==                                                   ==
// == folgende Werte werden übermittelt:                ==
// == ----------------------------------                ==
// == * ob das Garagentor offen oder geschlossen ist    ==
// == * ob Feueralarm (Rauchmelder) ausgelöst wurde     ==
// == * die aktuelle Temperatur                         ==
// ==                                                   ==
// ==                                                   ==
// ==            (C) by M. Rieper 15.04.2021            ==
// =======================================================

// define this to read the device id, serial and device type from bootloader section
//#define USE_OTA_BOOTLOADER
//#define NDEBUG

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <SPI.h>  // after including SPI Library - we can use LibSPI class
#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <ContactState.h>
#include <ThreeState.h>

#define CYCLETIME seconds2ticks(60UL * 3) // at least one message per day

#define CONFIG_BUTTON_PIN 13  // Pin für Anlerntaster
#define LED1_PIN          4   // Pin für Status LED1
#define LED2_PIN          5   // Pin für Status LED2


#define DOOR_SENS_PIN     18  // IRQ3 Pin für Türkontakt
#define SMOKE_SENS_PIN    19  // IRQ2 Pin für Rauchmelder



#define PEERS_PER_CHANNEL  6  // number of available peers per channel
#define MEASURE_INTERVAL   5  // alle xx Sekunden messen



// Klassen im Namensraum 'as' anlegen
// ==================================
using namespace as;

volatile int SmokeStatus = 0;

// Geräteeigenschaften definieren
// ==============================
const struct DeviceInfo PROGMEM devinfo = {
  {0xfb, 0xf5, 0x01},          // Device ID
  "MR24B95A01",                // Device Serial 15.04.21 - 24B95 / 21.05.75 - 3368F 
  {0xfb, 0xf5},                // Device Model
  0x10,                        // Firmware Version
  0xaa,                        // Device Type
  {0x01, 0x01}                 // Info Bytes
};


// verwendete Hardware konfigurieren
// =================================
typedef AskSin<DualStatusLed<LED2_PIN, LED1_PIN>, NoBattery, Radio<LibSPI<53>, 2>> Hal;
Hal hal;


// =======================================================
// == Gerätebezogene Register                           ==
// =======================================================
// ==                                                   ==
// == - existieren für jedes HomeMatic-Gerät nur einmal ==
// ==   und werden in der sogenannten List0 gespeichert ==
// ==                                                   ==
// =======================================================
DEFREGISTER(Reg0,MASTERID_REGS,DREG_TRANSMITTRYMAX,DREG_SABOTAGEMSG, 0x19, 0x20)
class UList0 : public RegList0<Reg0> {
public:
  UList0(uint16_t addr) : RegList0<Reg0>(addr) {}

  bool Sendeintervall (uint16_t value) const {
    return this->writeRegister(0x19, (value >> 8) & 0xff) && this->writeRegister(0x20, value & 0xff);
  }
  uint16_t Sendeintervall () const {
    return (this->readRegister(0x19, 0) << 8) + this->readRegister(0x20, 0);
  }

  void defaults () {
    clear();
    Sendeintervall(60);
    sabotageMsg(false);
    transmitDevTryMax(1);
  }
};


// ===============================================================
// == Kanalbezogene Register für Channel1 - Shutter Contact(SC) ==
// ===============================================================
DEFREGISTER(Reg1_1,CREG_AES_ACTIVE,CREG_MSGFORPOS,CREG_EVENTDELAYTIME,CREG_LEDONTIME,CREG_TRANSMITTRYMAX)
class SCList1 : public RegList1<Reg1_1> {
public:
  SCList1 (uint16_t addr) : RegList1<Reg1_1>(addr) {}
  void defaults () {
    clear();
    msgForPosA(1); // CLOSED
    msgForPosB(2); // OPEN
    aesActive(false);
    // eventDelaytime(0);
    ledOntime(100);
    transmitTryMax(1);
  }
};

// ===============================================================
// == Kanalbezogene Register für Channel2 - Smoke Detector(SD)  ==
// ===============================================================
DEFREGISTER(Reg1_2, CREG_AES_ACTIVE,CREG_MSGFORPOS,CREG_EVENTDELAYTIME,CREG_LEDONTIME,CREG_TRANSMITTRYMAX)
class SDList1 : public RegList1<Reg1_2> {
  public:
    SDList1 (uint16_t addr) : RegList1<Reg1_2>(addr) {}
    void defaults () {
      clear();
      msgForPosA(1); // CLOSED
      msgForPosB(2); // OPEN
      aesActive(false);
      // eventDelaytime(0);
      ledOntime(100);
      transmitTryMax(1);
    }
};


// ===============================================================
// == Kanalbezogene Register für Channel3 - WeatherChannel      ==
// ===============================================================
DEFREGISTER(Reg1_3, CREG_AES_ACTIVE)
class WeatherList1 : public RegList1<Reg1_3> {
  public:
    WeatherList1 (uint16_t addr) : RegList1<Reg1_3>(addr) {}
    void defaults () {
      clear();
    }
};



// ===============================================================
// == Channel 1 - Shutter Contact(SC) definieren                ==
// ===============================================================
class DoorPosition : public Position {
private:
  uint8_t pin;
public:
  DoorPosition () : pin(0) {}
  void init (uint8_t p) {
    DPRINT(F("Init ShutterContact to Pin: "));
    DPRINTLN(p);
    pin = p;
    pinMode(p,INPUT_PULLUP);
  }

  void measure (__attribute__((unused)) bool async=false) {
    _position = digitalRead(pin) == LOW ? State::PosB : State::PosA;
    DPRINT(F("ShutterContact Position: "));
    DPRINTLN(_position);
  }
  uint32_t interval () { return 0; }
};

template <class HALTYPE,class List0Type,class List1Type,class List4Type,int PEERCOUNT>
class TwoPinChannel : public StateGenericChannel<DoorPosition,HALTYPE,List0Type,List1Type,List4Type,PEERCOUNT> {
public:
  typedef StateGenericChannel<DoorPosition,HALTYPE,List0Type,List1Type,List4Type,PEERCOUNT> BaseChannel;

  TwoPinChannel () : BaseChannel() {};
  ~TwoPinChannel () {}

  void init (uint8_t pin) {
    BaseChannel::possens.init(pin);
  }

  uint32_t interval () { return BaseChannel::possens.interval(); }
};
typedef TwoPinChannel<Hal, UList0, SCList1, DefList4, PEERS_PER_CHANNEL> ShutterContact_Channel;



// ===============================================================
// == Channel 2 - Smoke Detector(SD) definieren                 ==
// ===============================================================
class SmokeDetect : public Position {
private:
  uint8_t pin;
public:
  SmokeDetect () : pin(0) {}
  void init (uint8_t p) {
    DPRINT(F("Init SmokeDetector to Pin: "));
    DPRINTLN(p);
    pin = p;
    pinMode(p,INPUT_PULLUP);
  }

  void measure (__attribute__((unused)) bool async=false) {
    _position = digitalRead(pin) == LOW ? State::PosA : State::PosB;
    DPRINT(F("SmokeDetect: "));
    DPRINTLN(_position);
  }
  uint32_t interval () { return 0; }
};

template <class HALTYPE,class List0Type,class List1Type,class List4Type,int PEERCOUNT>
class TwoPinChannel2 : public StateGenericChannel<SmokeDetect,HALTYPE,List0Type,List1Type,List4Type,PEERCOUNT> {
public:
  typedef StateGenericChannel<SmokeDetect,HALTYPE,List0Type,List1Type,List4Type,PEERCOUNT> BaseChannel2;

  TwoPinChannel2 () : BaseChannel2() {};
  ~TwoPinChannel2 () {}

  void init (uint8_t pin) {
    BaseChannel2::possens.init(pin);
  }

  uint32_t interval () { return BaseChannel2::possens.interval(); }
};
typedef TwoPinChannel2<Hal, UList0, SDList1, DefList4, PEERS_PER_CHANNEL> SmokeDetector_Channel;



// ===============================================================
// == Channel 3 - Temperatur definieren                         ==
// ===============================================================
class WeatherChannel : public Channel<Hal, List1, EmptyList, List4, PEERS_PER_CHANNEL, UList0>, public Alarm {
    class WeatherEventMsg : public Message {
      public:
        void init(uint8_t msgcnt, int16_t temp, uint16_t airPressure, uint8_t humidity) {
          uint8_t t1 = (temp >> 8) & 0x7f;
          uint8_t t2 = temp & 0xff;

          uint8_t flags = BCAST;
          if ((msgcnt % 20) == 2) {
            flags = BIDI | WKMEUP;
          }

          Message::init(0x0e, msgcnt, 0x70,  flags, t1, t2); //Nachrichtenlänge 0x0a,0x0b
          pload[0] = (airPressure >> 8) & 0xff;                     //Nachrichtenlänge 0x0c
          pload[1] = airPressure & 0xff;                            //Nachrichtenlänge 0x0d
          pload[2] = humidity;                                      //Nachrichtenlänge 0x0e

          //uint64_t& counter für Stromzähler
          //pload[0] = (counter >> 32) & 0xff;
          //pload[1] = (counter >> 24) & 0xff;
          //pload[2] = (counter >> 16) & 0xff;
          //pload[3] = (counter >>  8) & 0xff;
          //pload[4] = counter & 0xff;
          //pload[5] = 0x00; //
        }
    };
  protected:
    WeatherEventMsg msg;
  private:
    uint8_t measurecnt;
    int16_t       temperature;
    uint16_t      airPressure;
    uint8_t       humidity;
    bool firstrun;
  public:
    WeatherChannel () : Channel(), Alarm(5), measurecnt(0), firstrun(true), temperature(0),airPressure(0),humidity(0) {}
    virtual ~WeatherChannel () {}

    void measure () {
      temperature = 100 * random(-20, 30);
      DPRINT("T = "); DDECLN(temperature);

      airPressure = 9000 + random(2000);
      DPRINT("P = "); DDECLN(airPressure);

      humidity    = 44 + random(9);
      DPRINT("H = "); DDECLN(humidity);
    }

    virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
      uint8_t msgcnt = device().nextcount();

      tick = seconds2ticks(MEASURE_INTERVAL);

      clock.add(*this);
      measure();
      measurecnt++;

      uint16_t sendInterval = max(10, device().getList0().Sendeintervall());

      if (firstrun || measurecnt * MEASURE_INTERVAL >= sendInterval) {
        msg.init(msgcnt, temperature, airPressure, humidity);
        device().broadcastEvent(msg);
        measurecnt = 0;
        firstrun = false;
      }
    }

    void setup(Device<Hal, UList0>* dev, uint8_t number, uint16_t addr) {
      Channel::setup(dev, number, addr);
      sysclock.add(*this);
    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }
};











//typedef ThreeStateChannel<Hal, UList0, SCList1, DefList4, PEERS_PER_CHANNEL> SensChannel; //Temperaturanzeige auf Kanal 3

// ===============================================================
// == Gerät anlegen                                             ==
// ===============================================================
class MixDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, UList0>, 3, UList0> {
public:
  VirtChannel<Hal, ShutterContact_Channel, UList0> c1;
  VirtChannel<Hal, SmokeDetector_Channel,  UList0> c2;
  VirtChannel<Hal, WeatherChannel,         UList0> c3;

public:
  typedef ChannelDevice<Hal,VirtBaseChannel<Hal,UList0>,3,UList0> DeviceType;

  MixDevice (const DeviceInfo& info,uint16_t addr) : DeviceType(info,addr) {
    DeviceType::registerChannel(c1,1);
    DeviceType::registerChannel(c2,2);
    DeviceType::registerChannel(c3,3);

  }
  virtual ~MixDevice () {}

  ShutterContact_Channel& sw1Channel ()  { return c1; }
  SmokeDetector_Channel&  sw2Channel ()  { return c2; }
  WeatherChannel& sensorChannel () { return c3; }


  virtual void configChanged () {
    DeviceType::configChanged();
      DPRINT(F("*Sendeintervall: ")); DDECLN(this->getList0().Sendeintervall());

    if( /*this->getList0().sabotageMsg() ==*/ true ) {
      DPRINTLN("*Activate Sabotage Msg");
    } else {
      DPRINTLN("*Deactivate Sabotage Msg");
    }
  }
};
MixDevice sdev(devinfo,0x20);
ConfigButton<MixDevice> cfgBtn(sdev);


void funcISR_ShutterContact() {
  delay(50);
  // we simply activate the alarm
  Alarm& a = sdev.sw1Channel();
  sysclock.cancel(a);
  sysclock.add(a);
}

void funcISR_SmokeDetect() {
  // we simply activate the alarm
  Alarm& b = sdev.sw2Channel();
  sysclock.cancel(b);
  sysclock.add(b);
}


void setup () {
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.sw1Channel().init(DOOR_SENS_PIN);
  sdev.sw2Channel().init(SMOKE_SENS_PIN);
  sdev.initDone();

  contactISR(DOOR_SENS_PIN, funcISR_ShutterContact);
  contactISR(SMOKE_SENS_PIN, funcISR_SmokeDetect);

  sdev.sw2Channel().changed(false);
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if( worked == false && poll == false ) {
    //hal.activity.savePower<Idle<> >(hal);
  }
}
