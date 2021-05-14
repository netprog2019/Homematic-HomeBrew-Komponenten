//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// some parts (BME280 measurement) from HB-UNI-Sensor1
// 2018-05-11 Tom Major (Creative Commons)
// 2018-05-21 jp112sdl (Creative Commons)
//- -----------------------------------------------------------------------------------------------------------------------
//#define NDEBUG   // disable all serial debuf messages
//#define NSENSORS // if defined, only fake values are used


#define  EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <SPI.h>  // after including SPI Library - we can use LibSPI class
#include <AskSinPP.h>
#include <Register.h>
#include <MultiChannelDevice.h>

//#include <sensors\Veml6070.h>
#include "Sensors\Sens_Bme280.h"
#include "Sensors\Sens_As3935.h"
#include "Sensors\VentusW132.h"
#include <BH1750.h>
BH1750 lightMeter(0x23);


#define CONFIG_BUTTON_PIN   13   // Anlerntaster-Pin
#define LED1_PIN             4   // Arduino pin für die Status LED1
#define LED2_PIN             5   // Arduino pin für die Status LED2

//- UV-Sensor -----------------------------------------------------------------
#define UVINDEX_PIN                          A5   // UVIndex Pin des UVM-30A Sensors

//- Regendetector -------------------------------------------------------------
//bei Verwendung der Regensensorplatine von stall.biz (https://www.stall.biz/produkt/regenmelder-sensorplatine)
#define RAINDETECTOR_STALLBIZ_SENS_PIN       A7   // Pin, an dem der Kondensator angeschlossen ist (hier wird der analoge Wert für die Regenerkennung ermittelt)
#define RAINDETECTOR_STALLBIZ_CRG_PIN        33   // Pin, an dem der Widerstand für die Kondensatoraufladung angeschlossen is
#define RAINDETECTOR_STALLBIZ_HEAT_PIN       9    // Pin, an dem der Transistor für die Heizung angeschlossen ist
#define RAINDETECTOR_STALLBIZ_HEAT_DEWFALL_T 20   // Temperaturschwelle bei aktiviertem "automatisch Heizen bei Erreichen des Taupunkts"; default = 20 (heizen bei +/- 2,0°C um den Taupunkt)

//bei Verwendung eines Regensensors mit H/L-Pegel Ausgang
#define RAINDETECTOR_PIN                     9    // Pin, an dem der Regendetektor angeschlossen ist
#define RAINDETECTOR_PIN_LEVEL_ON_RAIN       LOW  // Pegel, wenn Regen erkannt wird

#define RAINDETECTOR_CHECK_INTERVAL          5    // alle x Sekunden auf Regen prüfen

//- Regenmengenzähler----------------------------------------------------------
#define RAINQUANTITYCOUNTER_PIN              18   // Regenmengenmesser

//- Gewittersensor ------------------------------------------------------------
#define AS3935_IRQ_PIN                       3    // IRQ Pin des Blitzdetektors
#define AS3935_CS_PIN                        7    // CS Pin des Blitzdetektors
#define AS3935_ENVIRONMENT                   ::Sens_As3935<>::AS3935_ENVIRONMENT_OUTDOOR

//- Ventus W132 Ersatz-Windmessers --------------------------------------------
// Windspeed-Pin
#define WINDSPEEDCOUNTER_PIN                 19   // Anemometer

// Windrichtungs-Pins für den Anschluss der 4 Photodioden 
#define VENTUSW132_PIN_N                     A0   // Rot
#define VENTUSW132_PIN_O                     A2   // Orange
#define VENTUSW132_PIN_S                     A4   // Blau
#define VENTUSW132_PIN_W                     A6   // Braun

//                            N       O         S         W
//entspricht Windrichtung in  0   1   2    3    4    5    6    7
const uint16_t WINDDIRS[] = { 0, 45, 90, 135, 180, 225, 270, 315 };

#define WINDSPEED_MEASUREINTERVAL_SECONDS    5    // Messintervall (Sekunden) für Windgeschwindigkeit / Böen
///////////////////////////////////////////////////////////////////////////////


//some static definitions
#define WINDSPEED_MAX              0x3FFF
#define GUSTSPEED_MAX              0x7FFF
#define RAINCOUNTER_MAX            0x7FFF
#define STORM_COND_VALUE_LO        100
#define STORM_COND_VALUE_HI        200
#define PEERS_PER_CHANNEL          4

using namespace as;

volatile uint32_t _rainquantity_isr_counter = 0;
volatile uint16_t _wind_isr_counter = 0;

void rainquantitycounterISR() {
  _rainquantity_isr_counter++;
}
void windspeedcounterISR() {
  _wind_isr_counter++;
}

enum eventMessageSources {EVENT_SRC_RAINING, EVENT_SRC_HEATING, EVENT_SRC_GUST};

const struct DeviceInfo PROGMEM devinfo = {
  {0xFB, 0xF4, 0x03},        // Device ID F1D003
  "JPWEA00003",              // Device Serial
  {0xFB, 0xF4},              // Device Model
  0x14,                      // Firmware Version
  as::DeviceType::THSensor,  // Device Type
  {0x01, 0x01}               // Info Bytes
};

// Configure the used hardware
typedef AskSin<DualStatusLed<LED1_PIN, LED2_PIN>, NoBattery, Radio<LibSPI<53>, 2>> Hal;
Hal hal;

class WeatherEventMsg : public Message {
  public:
    void init(uint8_t msgcnt, int16_t temp, uint16_t airPressure, uint8_t humidity, uint32_t brightness, bool israining, bool isheating, uint16_t raincounter,  uint16_t windspeed, uint8_t winddir, uint8_t winddirrange, uint16_t gustspeed, uint8_t uvindex, uint8_t lightningcounter, uint8_t lightningdistance) {

      //uint8_t flags = BCAST;
      //if ((msgcnt % 5) == 0 || (msgcnt % 10) == 1) {
      //    flags = BIDI;
      //}
      Message::init(0x1a, msgcnt, 0x70,(msgcnt % 20 == 1) ? (BIDI | WKMEUP) : BCAST, (temp >> 8) & 0x7f, temp & 0xff);
      pload[0] = (airPressure >> 8) & 0xff;
      pload[1] = airPressure & 0xff;
      pload[2] = humidity;
      pload[3] = (brightness >>  16) & 0xff;
      pload[4] = (brightness >>  8) & 0xff;
      pload[5] = brightness & 0xff;
      pload[6] = ((raincounter >> 8) & 0xff) | (israining << 7);
      pload[7] = raincounter & 0xff;
      pload[8] = ((windspeed >> 8) & 0xff) | (winddirrange << 6);
      pload[9] = windspeed & 0xff;
      pload[10] = winddir;
      pload[11] = ((gustspeed >> 8) & 0xff) | (isheating << 7);
      pload[12] = gustspeed & 0xff;
      pload[13] = (uvindex & 0xff) | (lightningdistance << 4);
      pload[14] = lightningcounter & 0xff;
    }
};

class ExtraEventMsg : public Message {
  public:
    void init(uint8_t msgcnt, bool israining, bool isheating, uint16_t gustspeed) {

      uint8_t flags = BCAST;
      if ((msgcnt % 5) == 0) {
          flags = BIDI;
      }
      Message::init(0x0d, msgcnt, 0x53, flags, 0x41, (israining & 0xff) | (isheating << 1));
      pload[0] =  (gustspeed >> 8) & 0xff;
      pload[1] =  gustspeed & 0xff;
    }
};

DEFREGISTER(Reg0, MASTERID_REGS, DREG_TRANSMITTRYMAX, 0x20, 0x21, 0x22, 0x23)
class SensorList0 : public RegList0<Reg0> {
  public:
    SensorList0(uint16_t addr) : RegList0<Reg0>(addr) {}

    bool updIntervall (uint16_t value) const {
      return this->writeRegister(0x20, (value >> 8) & 0xff) && this->writeRegister(0x21, value & 0xff);
    }
    uint16_t updIntervall () const {
      return (this->readRegister(0x20, 0) << 8) + this->readRegister(0x21, 0);
    }

    bool height (uint16_t value) const {
      return this->writeRegister(0x22, (value >> 8) & 0xff) && this->writeRegister(0x23, value & 0xff);
    }
    uint16_t height () const {
      return (this->readRegister(0x22, 0) << 8) + this->readRegister(0x23, 0);
    }

    void defaults () {
      clear();
      updIntervall(60);
      height(0);
      transmitDevTryMax(6);
    }
};

DEFREGISTER(Reg1, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16)
class SensorList1 : public RegList1<Reg1> {
  public:
    SensorList1 (uint16_t addr) : RegList1<Reg1>(addr) {}
    bool AnemometerRadius (uint8_t value) const {
      return this->writeRegister(0x01, value & 0xff);
    }
    uint8_t AnemometerRadius () const {
      return this->readRegister(0x01, 0);
    }

    bool AnemometerCalibrationFactor (uint16_t value) const {
      return this->writeRegister(0x02, (value >> 8) & 0xff) && this->writeRegister(0x03, value & 0xff);
    }
    uint16_t AnemometerCalibrationFactor () const {
      return (this->readRegister(0x02, 0) << 8) + this->readRegister(0x03, 0);
    }

    bool LightningDetectorCapacitor (uint8_t value) const {
      return this->writeRegister(0x04, value & 0xff);
    }
    uint8_t LightningDetectorCapacitor () const {
      return this->readRegister(0x04, 0);
    }

    bool LightningDetectorMinStrikes (uint8_t value) const {
      return this->writeRegister(0x12, value & 0xff);
    }
    uint8_t LightningDetectorMinStrikes () const {
      return this->readRegister(0x12, 0);
    }

    bool LightningDetectorWatchdogThreshold (uint8_t value) const {
      return this->writeRegister(0x13, value & 0xff);
    }
    uint8_t LightningDetectorWatchdogThreshold () const {
      return this->readRegister(0x13, 0);
    }

    bool LightningDetectorNoiseFloorLevel (uint8_t value) const {
      return this->writeRegister(0x14, value & 0xff);
    }
    uint8_t LightningDetectorNoiseFloorLevel () const {
      return this->readRegister(0x14, 0);
    }

    bool LightningDetectorSpikeRejection (uint8_t value) const {
      return this->writeRegister(0x15, value & 0xff);
    }
    uint8_t LightningDetectorSpikeRejection () const {
      return this->readRegister(0x15, 0);
    }

    bool LightningDetectorDisturberDetection () const {
      return this->readBit(0x05, 0, true);
    }
    bool LightningDetectorDisturberDetection (bool v) const {
      return this->writeBit(0x05, 0, v);
    }

    bool ExtraMessageOnGustThreshold (uint8_t value) const {
      return this->writeRegister(0x06, value & 0xff);
    }
    uint8_t ExtraMessageOnGustThreshold () const {
      return this->readRegister(0x06, 0);
    }

    bool StormUpperThreshold (uint8_t value) const {
      return this->writeRegister(0x07, value & 0xff);
    }
    uint8_t StormUpperThreshold () const {
      return this->readRegister(0x07, 0);
    }

    bool StormLowerThreshold (uint8_t value) const {
      return this->writeRegister(0x08, value & 0xff);
    }
    uint8_t StormLowerThreshold () const {
      return this->readRegister(0x08, 0);
    }

    bool RaindetectorStallBizHiThresholdRain (uint16_t value) const {
      return this->writeRegister(0x09, (value >> 8) & 0xff) && this->writeRegister(0x0a, value & 0xff);
    }
    uint16_t RaindetectorStallBizHiThresholdRain () const {
      return (this->readRegister(0x09, 0) << 8) + this->readRegister(0x0a, 0);
    }
    bool RaindetectorStallBizLoThresholdRain (uint16_t value) const {
      return this->writeRegister(0x0b, (value >> 8) & 0xff) && this->writeRegister(0x0c, value & 0xff);
    }
    uint16_t RaindetectorStallBizLoThresholdRain () const {
      return (this->readRegister(0x0b, 0) << 8) + this->readRegister(0x0c, 0);
    }
    bool RaindetectorStallBizHiThresholdHeater (uint16_t value) const {
      return this->writeRegister(0x0d, (value >> 8) & 0xff) && this->writeRegister(0x0e, value & 0xff);
    }
    uint16_t RaindetectorStallBizHiThresholdHeater () const {
      return (this->readRegister(0x0d, 0) << 8) + this->readRegister(0x0e, 0);
    }
    bool RaindetectorStallBizLoThresholdHeater (uint16_t value) const {
      return this->writeRegister(0x0f, (value >> 8) & 0xff) && this->writeRegister(0x10, value & 0xff);
    }
    uint16_t RaindetectorStallBizLoThresholdHeater () const {
      return (this->readRegister(0x0f, 0) << 8) + this->readRegister(0x10, 0);
    }

    bool RaindetectorSensorType (uint8_t value) const {
      return this->writeRegister(0x11, value & 0xff);
    }
    uint8_t RaindetectorSensorType () const {
      return this->readRegister(0x11, 0);
    }

    bool RaindetectorStallBizHeatOnDewfall () const {
      return this->readBit(0x16, 0, true);
    }
    bool RaindetectorStallBizHeatOnDewfall (bool v) const {
      return this->writeBit(0x16, 0, v);
    }

    void defaults () {
      clear();
      AnemometerRadius(65);
      AnemometerCalibrationFactor(10);
      LightningDetectorCapacitor(80);
      LightningDetectorDisturberDetection(true);
      ExtraMessageOnGustThreshold(0);
      StormUpperThreshold(0);
      StormLowerThreshold(0);
      RaindetectorSensorType(0);
      RaindetectorStallBizHiThresholdRain(920);   //<920 = Regen
      RaindetectorStallBizLoThresholdRain(921);   //>921 = kein Regen
      RaindetectorStallBizHiThresholdHeater(920); //<920 = Heizung 'ein'
      RaindetectorStallBizLoThresholdHeater(921); //>921 = Heizung 'aus'
      RaindetectorStallBizHeatOnDewfall(true);
      LightningDetectorMinStrikes(0);
      LightningDetectorSpikeRejection(2);
      LightningDetectorWatchdogThreshold(2);
      LightningDetectorNoiseFloorLevel(2);
    }
};

class WeatherChannel : public Channel<Hal, SensorList1, EmptyList, List4, PEERS_PER_CHANNEL, SensorList0>, public Alarm {
    int16_t       temperature;
    uint16_t      airPressure;
    uint8_t       humidity;
    uint32_t      brightness;
    bool          israining;
    bool          isheating;
    uint16_t      raincounter;
    uint16_t      windspeed;
    uint16_t      gustspeed;
    uint16_t      uvindex;
    int           voltage;
    uint8_t       lightningcounter;
    uint8_t       lightningdistance;

    uint8_t       winddir;
    uint8_t       winddirrange;

    uint16_t      stormUpperThreshold;
    uint16_t      stormLowerThreshold;

    bool          initComplete;
    bool          initLightningDetectorDone;
    uint8_t       short_interval_measure_count;
    uint8_t       israining_alarm_count;

    Sens_Bme280   bme280;
    VentusW132<VENTUSW132_PIN_N, VENTUSW132_PIN_O, VENTUSW132_PIN_S, VENTUSW132_PIN_W> ventus;

  public:
    Sens_As3935<AS3935_IRQ_PIN, AS3935_CS_PIN> as3935;

  public:
    WeatherChannel () : Channel(), Alarm(seconds2ticks(60)), israining(false), windspeed(0), uvindex(0), stormUpperThreshold(0), stormLowerThreshold(0), initComplete(false), initLightningDetectorDone(false), short_interval_measure_count(0), israining_alarm_count(0), wind_and_uv_measure(*this), lightning_and_raining_check(*this)  {}
    virtual ~WeatherChannel () {}

    class WindSpeedAndUVMeasureAlarm : public Alarm {
        WeatherChannel& chan;
      public:
        WindSpeedAndUVMeasureAlarm (WeatherChannel& c) : Alarm (seconds2ticks(WINDSPEED_MEASUREINTERVAL_SECONDS)), chan(c) {}
        virtual ~WindSpeedAndUVMeasureAlarm () {}

        void trigger (__attribute__ ((unused)) AlarmClock& clock)  {
          chan.measure_windspeed();
          chan.measure_uvindex();
          tick = (seconds2ticks(WINDSPEED_MEASUREINTERVAL_SECONDS));
          clock.add(*this);
          chan.short_interval_measure_count++;
        }
    } wind_and_uv_measure;

    class LightningAndRainingAlarm : public Alarm {
        WeatherChannel& chan;
      public:
        LightningAndRainingAlarm (WeatherChannel& c) : Alarm (seconds2ticks(1)), chan(c) {}
        virtual ~LightningAndRainingAlarm () {}

        void trigger (__attribute__ ((unused)) AlarmClock& clock)  {
          chan.measure_lightning();
          chan.measure_israining();
          tick = (seconds2ticks(1));
          clock.add(*this);
        }
    } lightning_and_raining_check;


    virtual void trigger (__attribute__ ((unused)) AlarmClock& clock) {
      measure_winddirection();
      measure_thpb();
      measure_rainquantity();
      measure_uvindex();
      
      if (initComplete) {
        windspeed = windspeed / short_interval_measure_count;
        if (windspeed > WINDSPEED_MAX) windspeed = WINDSPEED_MAX;
        //uvindex = uvindex / short_interval_measure_count;
      }

      DPRINT(F("GUSTSPEED     : ")); DDECLN(gustspeed);
      DPRINT(F("WINDSPEED     : ")); DDECLN(windspeed);
      DPRINT(F("UV Indexes    : ")); DDECLN(uvindex);
      WeatherEventMsg& msg = (WeatherEventMsg&)device().message();
      uint8_t msgcnt = device().nextcount();
      msg.init(msgcnt, temperature, airPressure, humidity, brightness, israining, isheating, raincounter, windspeed, winddir, winddirrange, gustspeed, uvindex, lightningcounter, lightningdistance);
      if (msgcnt % 20 == 1) {
        device().sendMasterEvent(msg);
      } else {
        device().broadcastEvent(msg, *this);
      }
      uint16_t updCycle = this->device().getList0().updIntervall();
      tick = seconds2ticks(updCycle);

      initComplete = true;
      windspeed = 0;
      gustspeed = 0;
      uvindex = 0;
      short_interval_measure_count = 0;
      sysclock.add(*this);
    }

    void sendExtraMessage (uint8_t t) {
      DPRINT(F("SENDING EXTRA MESSAGE ")); DDECLN(t);
      ExtraEventMsg& extramsg = (ExtraEventMsg&)device().message();
      extramsg.init(device().nextcount(), israining, isheating, gustspeed);
      device().sendMasterEvent(extramsg);
    }

    void measure_windspeed() {
      #ifdef NSENSORS
        _wind_isr_counter = random(20);
      #endif
      //V = 2 * R * Pi * N
      //  int kmph =  3.141593 * 2 * ((float)anemometerRadius / 100)   * ((float)_wind_isr_counter / (float)WINDSPEED_MEASUREINTERVAL_SECONDS)        * 3.6 * ((float)anemometerCalibrationFactor / 10);
      uint16_t kmph = ((226L * this->getList1().AnemometerRadius() * this->getList1().AnemometerCalibrationFactor() * _wind_isr_counter) / WINDSPEED_MEASUREINTERVAL_SECONDS) / 10000;
      if (kmph > gustspeed) {
        gustspeed = (kmph > GUSTSPEED_MAX) ? GUSTSPEED_MAX : kmph;
      }

      if (this->getList1().ExtraMessageOnGustThreshold() > 0 && kmph > (this->getList1().ExtraMessageOnGustThreshold() * 10)) {
        sendExtraMessage(EVENT_SRC_GUST);
      }
      DPRINT(F("WIND Counter  : ")); DDECLN(_wind_isr_counter);
      DPRINT(F("WIND kmph     : ")); DDECLN(kmph);
      DPRINT(F("UPPER THRESH  : ")); DDECLN(stormUpperThreshold);
      DPRINT(F("LOWER THRESH  : ")); DDECLN(stormLowerThreshold);

      static uint8_t STORM_COND_VALUE_Last = STORM_COND_VALUE_LO;
      static uint8_t STORM_COND_VALUE      = STORM_COND_VALUE_LO;

      if (stormUpperThreshold > 0) {
        if (kmph >= stormUpperThreshold || kmph <= stormLowerThreshold) {
          static uint8_t evcnt = 0;

          if (kmph >= stormUpperThreshold) STORM_COND_VALUE = STORM_COND_VALUE_HI;
          if (kmph <= stormLowerThreshold) STORM_COND_VALUE = STORM_COND_VALUE_LO;

          if (STORM_COND_VALUE != STORM_COND_VALUE_Last) {
            SensorEventMsg& rmsg = (SensorEventMsg&)device().message();
            DPRINT(F("PEER THRESHOLD DETECTED ")); DDECLN(STORM_COND_VALUE);
            rmsg.init(device().nextcount(), number(), evcnt++, STORM_COND_VALUE, false , false);
            device().sendPeerEvent(rmsg, *this);
          }
          STORM_COND_VALUE_Last = STORM_COND_VALUE;
        }
      }

      windspeed += kmph;
      _wind_isr_counter = 0;
      //measure_winddirection();
    }


    void measure_israining() {
      static bool wasraining = false;

      if (initComplete) {
        if (israining_alarm_count >= RAINDETECTOR_CHECK_INTERVAL) {
          switch (this->getList1().RaindetectorSensorType()) {
            case 0:
              israining = (digitalRead(RAINDETECTOR_PIN) == RAINDETECTOR_PIN_LEVEL_ON_RAIN);
              break;
            case 1:
              digitalWrite(RAINDETECTOR_STALLBIZ_CRG_PIN, HIGH);
              _delay_ms(5);
              digitalWrite(RAINDETECTOR_STALLBIZ_CRG_PIN, LOW);
              uint16_t rdVal = analogRead(RAINDETECTOR_STALLBIZ_SENS_PIN);
              DPRINT(F("RainDetect Ein: < ")); DDECLN(this->getList1().RaindetectorStallBizHiThresholdRain()); 
              DPRINT(F("RainDetect Aus: > ")); DDECLN(this->getList1().RaindetectorStallBizLoThresholdRain());
              DPRINT(F("RainDetect    : = ")); DDECLN(rdVal);

              if (rdVal < this->getList1().RaindetectorStallBizHiThresholdRain()) {
                israining = true;
              }
              if (rdVal > this->getList1().RaindetectorStallBizLoThresholdRain()) {
                israining = false;
              }

              static bool mustheat = false;
              if (rdVal < this->getList1().RaindetectorStallBizHiThresholdHeater()) {
                mustheat = true;
              }
              if (rdVal > (this->getList1().RaindetectorStallBizLoThresholdHeater())) {
                mustheat = false;
              }

              bool dewfall = false;
              if (this->getList1().RaindetectorStallBizHeatOnDewfall() == true)
                dewfall = bme280.present() ? (abs(bme280.temperature() - bme280.dewPoint()) < RAINDETECTOR_STALLBIZ_HEAT_DEWFALL_T) : false;
                DPRINT(F("RD Taupunkt   : ")); DDECLN(bme280.dewPoint()/10);
                
              // Heizung schalten
              raindetector_heater(mustheat || dewfall);
          }

          DPRINT(F("RD israining  : ")); DDECLN(israining);

          if (wasraining != israining) {
            sendExtraMessage(EVENT_SRC_RAINING);
            static uint8_t evcnt = 0;
            SensorEventMsg& rmsg = (SensorEventMsg&)device().message();
            rmsg.init(device().nextcount(), number(), evcnt++, israining ? 200 : 0, true , false);
            device().sendPeerEvent(rmsg, *this);
          }
          wasraining = israining;
          israining_alarm_count = 0;
        }
        israining_alarm_count++;
      }
    }


    void raindetector_heater(bool State) {
      static bool washeating = false;
      static uint8_t pwmval = 0;
      isheating = State;

      if (State == true) {
        if (pwmval < 255) pwmval = pwmval + 51;
      } else {
        pwmval = 0;
      }
      analogWrite(RAINDETECTOR_STALLBIZ_HEAT_PIN, pwmval);

      DPRINT(F("RD HEAT       : ")); DDECLN(State);
      DPRINT(F("RD HEAT PWM   : ")); DDECLN(pwmval);


      if (washeating != State) {
        sendExtraMessage(EVENT_SRC_HEATING);
      }
      washeating = State;
    }


    void measure_uvindex() {
      #ifdef NSENSORS
        uvindex += random(11);
        voltage += random(1024);
      #else
        voltage = (analogRead(UVINDEX_PIN) * (5.0 / 1023.0))*1000;
        //voltage = analogRead(UVINDEX_PIN)/1023*5;
        if(voltage<=227) { uvindex = 0;}
        else if (voltage>227 && voltage<=318)   { uvindex = 1; }
        else if (voltage>318 && voltage<=408)   { uvindex = 2; }
        else if (voltage>408 && voltage<=503)   { uvindex = 3; }
        else if (voltage>503 && voltage<=606)   { uvindex = 4; }
        else if (voltage>606 && voltage<=696)   { uvindex = 5; }
        else if (voltage>696 && voltage<=795)   { uvindex = 6; }
        else if (voltage>795 && voltage<=881)   { uvindex = 7; }
        else if (voltage>881 && voltage<=976)   { uvindex = 8; }
        else if (voltage>976 && voltage<=1079)  { uvindex = 9; }
        else if (voltage>1079 && voltage<=1170) { uvindex = 10; }
        else if (voltage>1170) { uvindex = 11; }
      #endif
      
      DPRINT(F("UV Index      : ")); DDEC(uvindex); DPRINT(F(" - (")); DDEC(voltage); DPRINTLN(F("mV)"));
    }


    void measure_winddirection() {
      DPRINTLN(F("WINDDIR Measure"));
      
      //Windrichtung Grad/3: 60° = 20; 0° = Norden
      winddir = 0;
      uint8_t idxwdir = 0;
      #ifdef NSENSORS
        idxwdir = random(15);
        winddir = (idxwdir * 15 + 2 / 2) / 2;
      #else
        //winddir = ventus.winddirValue();
      #endif
      idxwdir = ventus.winddirValue(true);
      winddir = WINDDIRS[idxwdir] / 3;
      DPRINT(F("WINDDIR value (=dir / 3) : ")); DDECLN(winddir);
        
      //Schwankungsbreite
      static uint8_t idxoldwdir = 0;
      winddirrange = 3; // 0  - 3 (0, 22,5, 45, 67,5°)
      int idxdiff = abs(idxwdir - idxoldwdir);

      if (idxdiff <= 3) winddirrange = idxdiff;
      if (idxwdir <= 2 && idxoldwdir >= 13) winddirrange = (sizeof(WINDDIRS) / sizeof(uint16_t)) - idxdiff;
      if (winddirrange > 3) winddirrange = 3;

      idxoldwdir = idxwdir;

      //DPRINT(F("WINDDIR dir/3 : ")); DDECLN(winddir);
      //DPRINT(F("WINDDIR range : ")); DDECLN(winddirrange);
    }


    void measure_rainquantity() {
      #ifdef NSENSORS
        rainquantity_isr_counter++;
      #endif
      if (!initComplete) {
        _rainquantity_isr_counter = 0;
        //DPRINTLN(F("RAINCOUNTER   : initalize"));
      } else {
        if (_rainquantity_isr_counter > RAINCOUNTER_MAX) {
          _rainquantity_isr_counter = 1;
        }
        raincounter = _rainquantity_isr_counter;
      }
      DPRINT(F("RAINCOUNTER   : ")); DDECLN(_rainquantity_isr_counter);
    }


    void measure_lightning() {
      #ifdef NSENSORS
        lightningcounter++;
        lightningdistance = random(15);
      #else
        if (!initLightningDetectorDone) {
          as3935.init(this->getList1().LightningDetectorCapacitor(),
                      this->getList1().LightningDetectorDisturberDetection(),
                      AS3935_ENVIRONMENT,
                      this->getList1().LightningDetectorMinStrikes(),
                      this->getList1().LightningDetectorWatchdogThreshold(),
                      this->getList1().LightningDetectorNoiseFloorLevel(),
                      this->getList1().LightningDetectorSpikeRejection());
         initLightningDetectorDone = true;
        } else {
          uint8_t lightning_dist_km = 0;
          if (as3935.LightningIsrCounter() > 0) {
            switch (as3935.GetInterruptSrc()) {
              case 0:
                DPRINTLN(F("LD IRQ SRC NOT EXPECTED"));
                break;
              case 1:
                lightning_dist_km = as3935.LightningDistKm();
                DPRINT(F("LD LIGHTNING IN ")); DDEC(lightning_dist_km); DPRINTLN(" km");
                lightningcounter++;
                // Wenn Zähler überläuft (255 + 1), dann 1 statt 0
                if (lightningcounter == 0) lightningcounter = 1;
                lightningdistance = (lightning_dist_km + 1) / 3;
                break;
              case 2:
                DPRINTLN(F("LD DIST"));
                break;
              case 3:
                DPRINTLN(F("LD NOISE"));
                break;
            }
            as3935.ResetLightninIsrCounter();
          }
        }
      #endif
      //DPRINT(F("LD CNT        : ")); DDECLN(lightningcounter);
      //DPRINT(F("LD DIST       : ")); DDECLN(lightningdistance);
    }


    void measure_thpb() {
      uint16_t height = this->device().getList0().height();
      #ifdef NSENSORS
        airPressure = 9000 + random(2000);   // 1024 hPa +x
        humidity    = 66 + random(7);     // 66% +x
        temperature = 150 + random(50);   // 15C +x
        brightness = 1700000 + random(10000);   // 67000 Lux +x
      #else
        bme280.measure(height);
        temperature = bme280.temperature();     
        airPressure = bme280.pressureNN();      
        humidity    = bme280.humidity();
                
        brightness = lightMeter.readLightLevel()*1.2;       
      #endif
      
      DPRINT(F("Temperature   : ")); DDEC(temperature); DPRINTLN(F("C°"));
      DPRINT(F("AirPressure   : ")); DDEC(airPressure); DPRINTLN(F("hPa"));
      DPRINT(F("Humidity      : ")); DDEC(humidity); DPRINTLN(F("%"));
      DPRINT(F("Brightness    : ")); DDEC(brightness); DPRINTLN(F("lux"));
    }

    void setup(Device<Hal, SensorList0>* dev, uint8_t number, uint16_t addr) {
      bme280.init();
      lightMeter.begin();
      Channel::setup(dev, number, addr);
      tick = seconds2ticks(3);  // first message in 3 sec.

      ventus.init();

      sysclock.add(*this);
      sysclock.add(wind_and_uv_measure);
      sysclock.add(lightning_and_raining_check);
    }

    void configChanged() {
      DPRINTLN("* Config changed       : List1");
      //DPRINTLN(F("* ANEMOMETER           : "));
      //DPRINT(F("*  - RADIUS            : ")); DDECLN(this->getList1().AnemometerRadius());
      //DPRINT(F("*  - CALIBRATIONFACTOR : ")); DDECLN(this->getList1().AnemometerCalibrationFactor());
      //DPRINT(F("*  - GUST MSG THRESHOLD: ")); DDECLN(this->getList1().ExtraMessageOnGustThreshold());
      //DPRINTLN(F("* LIGHTNINGDETECTOR    : "));
      //DPRINT(F("*  - CAPACITOR         : ")); DDECLN(this->getList1().LightningDetectorCapacitor());
      //DPRINT(F("*  - DISTURB.DETECTION : ")); DDECLN(this->getList1().LightningDetectorDisturberDetection());
      //DPRINT(F("*  - WATCHDOGTHRESHOLD : ")); DDECLN(this->getList1().LightningDetectorWatchdogThreshold());
      //DPRINT(F("*  - SPIKREJECTION     : ")); DDECLN(this->getList1().LightningDetectorSpikeRejection());
      //DPRINT(F("*  - NOISEFLOORLEVEL   : ")); DDECLN(this->getList1().LightningDetectorNoiseFloorLevel());
      //DPRINT(F("*  - MINSTRIKES        : ")); DDECLN(this->getList1().LightningDetectorMinStrikes());
      //DPRINT(F("PEERSETTING UPPER  = ")); DDECLN(this->getList1().StormUpperThreshold());
      stormUpperThreshold = this->getList1().StormUpperThreshold() * 10;
      //DPRINT(F("PEERSETTING LOWER  = ")); DDECLN(this->getList1().StormLowerThreshold());
      stormLowerThreshold = this->getList1().StormLowerThreshold() * 10;
      //DPRINT(F("RAINDETECTOR SENSORTYPE  = ")); DDECLN(this->getList1().RaindetectorSensorType());
      //DPRINT(F("RaindetectorStallBizHiThresholdRain    = ")); DDECLN(this->getList1().RaindetectorStallBizHiThresholdRain());
      //DPRINT(F("RaindetectorStallBizLoThresholdRain    = ")); DDECLN(this->getList1().RaindetectorStallBizLoThresholdRain());
      //DPRINT(F("RaindetectorStallBizHiThresholdHeater  = ")); DDECLN(this->getList1().RaindetectorStallBizHiThresholdHeater());
      //DPRINT(F("RaindetectorStallBizLoThresholdHeater  = ")); DDECLN(this->getList1().RaindetectorStallBizLoThresholdHeater());
      //DPRINT(F("RaindetectorStallBizHeatOnDewfall      = ")); DDECLN(this->getList1().RaindetectorStallBizHeatOnDewfall());
      switch (this->getList1().RaindetectorSensorType()) {
        case 0:
          pinMode(RAINDETECTOR_PIN, INPUT_PULLUP);
          break;
        case 1:
          pinMode(RAINDETECTOR_STALLBIZ_CRG_PIN, OUTPUT);
          pinMode(RAINDETECTOR_STALLBIZ_HEAT_PIN, OUTPUT);
          pinMode(RAINDETECTOR_STALLBIZ_SENS_PIN, INPUT);
          break;
      }

      initLightningDetectorDone = false;
    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }
};

class SensChannelDevice : public MultiChannelDevice<Hal, WeatherChannel, 1, SensorList0> {
  public:
    typedef MultiChannelDevice<Hal, WeatherChannel, 1, SensorList0> TSDevice;
    SensChannelDevice(const DeviceInfo& info, uint16_t addr) : TSDevice(info, addr) {}
    virtual ~SensChannelDevice () {}

    virtual void configChanged () {
      TSDevice::configChanged();
      DPRINTLN("* Config Changed       : List0");
      DPRINT(F("* SENDEINTERVALL       : ")); DDECLN(this->getList0().updIntervall());
      //DPRINT(F("* ALTITUDE             : ")); DDECLN(this->getList0().height());
      //DPRINT(F("* TRANSMITTRYMAX       : ")); DDECLN(this->getList0().transmitDevTryMax());
    }
};

SensChannelDevice sdev(devinfo, 0x20);
ConfigButton<SensChannelDevice> cfgBtn(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);  
  sdev.init(hal); 
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
  sdev.startPairing();
  
  pinMode(RAINQUANTITYCOUNTER_PIN, INPUT_PULLUP);
  pinMode(WINDSPEEDCOUNTER_PIN, INPUT_PULLUP);
  
  if ( digitalPinToInterrupt(RAINQUANTITYCOUNTER_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(RAINQUANTITYCOUNTER_PIN, rainquantitycounterISR, RISING); else attachInterrupt(digitalPinToInterrupt(RAINQUANTITYCOUNTER_PIN), rainquantitycounterISR, RISING);
  if ( digitalPinToInterrupt(WINDSPEEDCOUNTER_PIN) == NOT_AN_INTERRUPT ) enableInterrupt(WINDSPEEDCOUNTER_PIN, windspeedcounterISR, RISING); else attachInterrupt(digitalPinToInterrupt(WINDSPEEDCOUNTER_PIN), windspeedcounterISR, RISING);
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    // hal.activity.savePower<Idle<false, true>>(hal);
  }
}
