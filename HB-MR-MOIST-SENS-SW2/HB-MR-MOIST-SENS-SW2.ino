//- -------------------------------------------------------------------------- -
//-                       kapazitiver Bodenfeuchtesensor                       -
//-                                                                            -
//- -------------------------------------------------------------------------- -
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-05-03 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2019-05-04 stan23 Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//------------------------------------------------------------------------------

//Sensor:
//https://www.dfrobot.com/wiki/index.php/Capacitive_Soil_Moisture_Sensor_SKU:SEN0193

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>
#include <MultiChannelDevice.h>
#include <Register.h>

#include <Switch.h>
#include <ThreeState.h>

// Arduino Pro mini 8 Mhz
// ----------------------

// Arduino pin für den Config Button
#define CONFIG_BUTTON_PIN      8
// Arduino pin für die Status LED
#define LED_PIN                4

#define SENSOR_VCC_PIN         7
#define SENSOR_PIN             15

#define SWITCH_PIN1            3
#define SWITCH_PIN2            6

// Arduino pin für das c1101 Funkmodul
#define CC1101_GDO0_PIN        2
#define CC1101_CS_PIN          10
#define CC1101_MOSI_PIN        11
#define CC1101_MISO_PIN        12
#define CC1101_SCK_PIN         13

// number of available peers per channel
#define PEERS_PER_CHANNEL    4
#define PEERS_PER_SWCHANNEL  6
#define CYCLETIME seconds2ticks(60UL * 3 * 0.88)

// all library classes are placed in the namespace 'as'
using namespace as;

//Korrekturfaktor der Clock-Ungenauigkeit, wenn keine RTC verwendet wird
#define SYSCLOCK_FACTOR    0.88

// define all device properties
// Bei mehreren Geräten des gleichen Typs muss Device ID und Device Serial unterschiedlich sein!
const struct DeviceInfo PROGMEM devinfo = {
  {0xfb, 0xf2, 0x01},          // Device ID
  "MR1FDD9A01",                // Device Serial 13.05.21 - 1FDD9 / 21.05.75 - 3368F 
  {0xfb, 0xf2},                // Device Model0xF3, 0x11
  0x10,                        // Firmware Version
                               // die CCU Addon xml Datei ist mit der Zeile <parameter index="9.0" size="1.0" cond_op="E" const_value="0x10" />
                               // fest an diese Firmware Version gebunden! cond_op: E Equal, GE Greater or Equal
                               // bei Änderungen von Payload, message layout, Datenpunkt-Typen usw. muss die Version an beiden Stellen hochgezogen werden!
  0xAB,                        // Device Type
  {0x01, 0x01}                 // Info Bytes
};

/**
   Configure the used hardware
*/
typedef AvrSPI<CC1101_CS_PIN, CC1101_MOSI_PIN, CC1101_MISO_PIN, CC1101_SCK_PIN> SPIType;
typedef Radio<SPIType, CC1101_GDO0_PIN> RadioType;
typedef StatusLed<LED_PIN> LedType;
typedef AskSin<LedType, NoBattery, RadioType> Hal;
Hal hal;

// die "freien" Register 0x20/21 werden hier als 16bit memory für das Update
// Intervall in Sek. benutzt siehe auch hb-sen-leveljet.xml, <parameter id="Sendeintervall">
DEFREGISTER(Reg0, MASTERID_REGS, DREG_TRANSMITTRYMAX, DREG_CYCLICINFOMSG, 0x21, 0x22)
class SensorList0 : public RegList0<Reg0> {
public:
    SensorList0(uint16_t addr) : RegList0<Reg0>(addr) {}

    bool Sendeintervall (uint16_t value) const { return this->writeRegister(0x21, (value >> 8) & 0xff) && this->writeRegister(0x22, value & 0xff); }
    uint16_t Sendeintervall () const { return (this->readRegister(0x21, 0) << 8) + this->readRegister(0x22, 0); }

    void defaults() {
      clear();
      transmitDevTryMax(6);
      cycleInfoMsg(true); // CycleInfoMsg hinzugefügt, siehe auch DEFREGISTER 0 oben
      Sendeintervall(60);
    }
};


DEFREGISTER(Reg1, 0x01, 0x02, 0x03, 0x04, 0x23, 0x24, 0x25, 0x26)
class SensorList1 : public RegList1<Reg1> {
  public:
    SensorList1 (uint16_t addr) : RegList1<Reg1>(addr) {}

    bool HIGHValue (uint16_t value) const { return this->writeRegister(0x23, (value >> 8) & 0xff) && this->writeRegister(0x24, value & 0xff); }
    uint16_t HIGHValue () const { return (this->readRegister(0x23, 0) << 8) + this->readRegister(0x24, 0); }

    bool LOWValue (uint16_t value) const { return this->writeRegister(0x25, (value >> 8) & 0xff) && this->writeRegister(0x26, value & 0xff); }
    uint16_t LOWValue () const { return (this->readRegister(0x25, 0) << 8) + this->readRegister(0x26, 0); }

    void defaults () {
      clear();
      HIGHValue(1000);//Trocken 830
      LOWValue(80);//Feucht 420
    }
};


class MessEventMsg : public Message {
public:
    void init(uint8_t msgcnt, uint8_t h) {
      //uint8 typ = 0x53; // AS_MESSAGE_SENSOR_DATA;
      uint8 typ = 0x5E; // AS_MESSAGE_POWER_EVENT_CYCLIC;

      // als Standard wird BCAST gesendet um Energie zu sparen, siehe Beschreibung HB-UNI-Sensor1.
      // Bei jeder 10. Nachricht senden wir stattdessen BIDI|WKMEUP, um eventuell anstehende Konfigurationsänderungen auch
      // ohne Betätigung des Anlerntaster übernehmen zu können (mit Verzögerung, worst-case 10x Sendeintervall).
      uint8_t flags = BCAST;
      if ((msgcnt % 5) == 2) {
        flags = BIDI | WKMEUP;
      }

      Message::init(11, msgcnt, typ, flags , h, 0);
      DPRINT(F("+Humidity         : ")); DDEC(h); DPRINTLN(F(" %"));
      //pload[0] = h;
    }
};


class Messkanal : public Channel<Hal, SensorList1, EmptyList, List4, PEERS_PER_CHANNEL, SensorList0>, public Alarm {
    MessEventMsg    msg;
    uint8_t         humidity;
    uint8_t         sabotage;

public:
    Messkanal() : Channel(), Alarm(0), sabotage(0) {}
    virtual ~Messkanal() {}

    virtual void trigger(AlarmClock& clock) {
        uint8_t msgcnt = device().nextcount();
        measure();
        msg.init(msgcnt, humidity);
        device().sendPeerEvent(msg, *this);
        // reactivate for next measure

        uint16_t updCycle = this->device().getList0().Sendeintervall();
        set(seconds2ticks(updCycle));
        clock.add(*this);
        //sysclock.add(*this);
    }

    bool testSabotage (uint8_t OldValue, uint8_t NewValue) {
		    if ((OldValue > NewValue && (OldValue - NewValue) > 25) || (NewValue == 0 && OldValue == 0) || (NewValue == 100 && OldValue == 100)) {
             DPRINT(F("+Sensor Sabotage erkannt: OldValue:")); DDEC(OldValue); DPRINT(F(" / NewValue:"));DDECLN(NewValue);
			       return true;
		    } else {
			       return false;
		    }
	  }

    void measure() {
      // save last value
      uint8_t lastvalue = humidity;

      //now measure sensor power on
      pinMode(SENSOR_VCC_PIN, OUTPUT);
      digitalWrite(SENSOR_VCC_PIN, HIGH);
      //wait a moment to settle
      _delay_ms(500);

      //measure 8 times and calculate average
      uint16_t sens_val = 0;
      for (uint8_t i = 0; i < 8; i++) {
        sens_val += analogRead(SENSOR_PIN);
        _delay_ms(10);
      }
      sens_val /= 8;

      DPRINT(F("+Analoger Messwert: ")); DDEC(sens_val);
      uint16_t upper_limit = this->getList1().HIGHValue();
      uint16_t lower_limit = this->getList1().LOWValue();
      if (sens_val > upper_limit) {
        humidity = 0;
        DPRINTLN(F(" - größer als HIGH Value (0% Feuchte / Trocken)!"));
      }
      else if (sens_val < lower_limit) {
        humidity = 100;
        DPRINTLN(F(" - kleiner als LOW Value (100% Feuchte / Nass)!"));
      } else {
        uint16_t range = upper_limit - lower_limit;
        uint16_t base = sens_val - lower_limit;
        uint8_t pct_inv = (base * 100) / range;
        humidity = 100 - pct_inv;
        DPRINTLN("");
      }

      humidity     = random(100);

      if( testSabotage(lastvalue,humidity) == true ) {
         sabotage = 1;
         changed(true); // this triggers StatusInfoMessage to CCU
      } else {
        sabotage = 0;
        changed(true); // this triggers StatusInfoMessage to CCU
      }

      //disable all moisture sensors
      digitalWrite(SENSOR_VCC_PIN, LOW);
    }

    void setup(Device<Hal, SensorList0>* dev, uint8_t number, uint16_t addr) {
        Channel::setup(dev, number, addr);

        set(seconds2ticks(20));    // first message in 20 sec.
        sysclock.add(*this);
    }

    void configChanged() {
      DPRINT(F("Config changed List1 (CH "));DDEC(number());DPRINTLN(F(")"));

      DPRINT(F("*HIGHValue : ")); DDECLN(this->getList1().HIGHValue());
      DPRINT(F("*LOWValue  : ")); DDECLN(this->getList1().LOWValue());
    }

    uint8_t status() const { return 0; }

    //uint8_t flags() const { return 0; }
    uint8_t flags () const {
      uint8_t flags = sabotage ? 0x07 << 1 : 0x00;
      return flags;
    }
};

/*
class myDevice: public MultiChannelDevice<Hal, Messkanal, 1, SensorList0> {
public:
    typedef MultiChannelDevice<Hal, Messkanal, 1, SensorList0> TSDevice;
    myDevice(const DeviceInfo& info, uint16_t addr)
        : TSDevice(info, addr)
    {
    }
    virtual ~myDevice() {}

    virtual void configChanged()
    {
        TSDevice::configChanged();
        DPRINTLN("Config Changed: List0");

        uint8_t txDevTryMax = this->getList0().transmitDevTryMax();
        DPRINT("transmitDevTryMax: ");
        DDECLN(txDevTryMax);

        uint16_t updCycle = this->getList0().Sendeintervall();
        DPRINT("updCycle: ");
        DDECLN(updCycle);
    }
};
*/

typedef SwitchChannel<Hal,PEERS_PER_SWCHANNEL,SensorList0>  SwChannel;
//typedef ThreeStateChannel<Hal,SensorList0,SensorList1,List4,PEERS_PER_CHANNEL> SensChannel;

class myDevice : public ChannelDevice<Hal,VirtBaseChannel<Hal,SensorList0>,3,SensorList0> {
  class CycleInfoAlarm : public Alarm {
        myDevice& dev;
      public:
        CycleInfoAlarm (myDevice& d) : Alarm (CYCLETIME), dev(d) {}
        virtual ~CycleInfoAlarm () {}

        void trigger (AlarmClock& clock)  {
          set(CYCLETIME);
          clock.add(*this);
          //dev.Messkanal(1).changed(true);
        }
    } cycle;

public:
  VirtChannel<Hal,Messkanal,SensorList0> c1;
  VirtChannel<Hal,SwChannel,SensorList0> c2,c3;


  typedef ChannelDevice<Hal,VirtBaseChannel<Hal,SensorList0>,3,SensorList0> DeviceType;
  myDevice (const DeviceInfo& info,uint16_t addr) : DeviceType(info, addr), cycle(*this) {
    DeviceType::registerChannel(c1,1);
    DeviceType::registerChannel(c2,2);
    DeviceType::registerChannel(c3,3);
  }
  virtual ~myDevice () {}

  Messkanal& sensorChannel () { return c1; }
  SwChannel& switch1Channel ()  { return c2; }
  SwChannel& switch2Channel ()  { return c3; }


  virtual void configChanged() {
      DPRINTLN("Config Changed: List0");

      uint8_t txDevTryMax = this->getList0().transmitDevTryMax();
      DPRINT("transmitDevTryMax: ");
      DDECLN(txDevTryMax);

      uint16_t updCycle = this->getList0().Sendeintervall();
      DPRINT("updCycle: ");
      DDECLN(updCycle);

      if (/* this->getSwList0().cycleInfoMsg() ==*/ true ) {
        DPRINTLN("Activate Cycle Msg");
        sysclock.cancel(cycle);
        cycle.set(CYCLETIME);
        sysclock.add(cycle);
      }
      else {
        DPRINTLN("Deactivate Cycle Msg");
        sysclock.cancel(cycle);
      }
  }
};



myDevice               sdev(devinfo, 0x20);
ConfigButton<myDevice> cfgBtn(sdev);

void setup() {
    // !! Serial Port Init (DINIT) via AskSinPP/Debug.h wird 2fach benutzt: Tx AskSinPP Debug out, Rx LevelJet (19200 Baud erforderlich)
    DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
    sdev.init(hal);
    sdev.switch1Channel().init(SWITCH_PIN1);
    sdev.switch2Channel().init(SWITCH_PIN2);

    buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
    sdev.initDone();
}

void loop() {
    bool worked = hal.runready();
    bool poll   = sdev.pollRadio();
    if (worked == false && poll == false) {
        // Device ist Netzteil betrieben und sollte wach bleiben um die Mess-Telegramme der Bodenfeuchte zu empfangen
    }
}
