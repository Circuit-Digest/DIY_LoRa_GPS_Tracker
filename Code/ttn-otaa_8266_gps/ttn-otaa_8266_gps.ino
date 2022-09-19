#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <TinyGPSPlus.h>

TinyGPSPlus gps;
String ProcData="Hello,World";



#ifdef COMPILE_REGRESSION_TEST
# define FILLMEIN 0
#else
# warning "You must replace the values marked FILLMEIN with real values from the TTN control panel!"
# define FILLMEIN (#dont edit this, edit the lines that use FILLMEIN)
#endif

// This EUI must be in little-endian format, so least-significant-byte
// first. When copying an EUI from ttnctl output, this means to reverse
// the bytes. For TTN issued EUIs the last bytes should be 0xD5, 0xB3,
// 0x70.
static const u1_t PROGMEM APPEUI[8]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}

// This should also be in little endian format, see above.
static const u1_t PROGMEM DEVEUI[8]= {0x36, 0x52, 0x05, 0xD0, 0x7E, 0xD5, 0xB3, 0x70};
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}

// This key should be in big endian format (or, since it is not really a
// number but a block of memory, endianness does not really apply). In
// practice, a key taken from ttnctl can be copied as-is.
static const u1_t PROGMEM APPKEY[16] = {0x97, 0x7B, 0x4B, 0xA3, 0x9A, 0x8D, 0x33, 0x5E, 0x42, 0x13, 0x9C, 0x6B, 0xE9, 0xF9, 0x5E, 0xE1};
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16);}

uint8_t mydata[25] ;
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 2;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 15,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 16,
    .dio = {5, 4, LMIC_UNUSED_PIN},
};

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
      Serial1.print('0');
    Serial1.print(v, HEX);
}

void onEvent (ev_t ev) {
    Serial1.print(os_getTime());
    Serial1.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial1.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial1.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial1.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial1.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial1.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial1.println(F("EV_JOINED"));
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial1.print("netid: ");
              Serial1.println(netid, DEC);
              Serial1.print("devaddr: ");
              Serial1.println(devaddr, HEX);
              Serial1.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial1.print("-");
                printHex2(artKey[i]);
              }
              Serial1.println("");
              Serial1.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial1.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial1.println();
            }
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
	    // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_RFU1:
        ||     Serial1.println(F("EV_RFU1"));
        ||     break;
        */
        case EV_JOIN_FAILED:
            Serial1.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial1.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial1.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial1.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial1.print(F("Received "));
              Serial1.print(LMIC.dataLen);
              Serial1.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial1.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial1.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial1.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial1.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial1.println(F("EV_LINK_ALIVE"));
            break;
        /*
        || This event is defined but not used in the code. No
        || point in wasting codespace on it.
        ||
        || case EV_SCAN_FOUND:
        ||    Serial1.println(F("EV_SCAN_FOUND"));
        ||    break;
        */
        case EV_TXSTART:
            Serial1.println(F("EV_TXSTART"));
            break;
        case EV_TXCANCELED:
            Serial1.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial1.println(F("EV_JOIN_TXCOMPLETE: no JoinAccept"));
            break;

        default:
            Serial1.print(F("Unknown event: "));
            Serial1.println((unsigned) ev);
            break;
    }
}


void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) 
    {
        Serial1.println(F("OP_TXRXPEND, not sending"));
    } 
    else
    {
      
        // Prepare upstream data transmission at the next possible time.  
              unsigned long start = millis();
            
              do 
              {
                while (Serial.available())
                  gps.encode(Serial.read());
              } while (millis() - start < 1000);
              
        
          float flat=gps.location.lat();
          float flon=gps.location.lng();
          char charLat[20];  
          char charLong[20];  
         Serial1.print("Cords: ");
         Serial1.print(flat);
         Serial1.print(" , ");
         Serial1.println(flon);
              dtostrf(flat, 10, 7, charLat);
        dtostrf(flon, 10, 7, charLong);
        sprintf((char *)mydata, "%s,%sX", charLat,charLong);
         
        LMIC_setTxData2(1, mydata,25, 0);
        Serial1.println(F("Packet queued"));


        //int x=ProcData.length();
        //ProcData.toCharArray((char *)mydata,sizeof(mydata)); 
       
          
        
    }
    // Next TX is scheduled after TX_COMPLETE event.
}


void setup() {
    Serial1.begin(9600);
    Serial1.println(F("Starting"));
    Serial.begin(9600);
    // LMIC init
    os_init();
    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
    // Start job (sending automatically starts OTAA too)
    do_send(&sendjob);
}

void loop() 
{
    os_runloop_once();
}
