#include <TFT_eSPI.h> // Hardware-specific library
TFT_eSPI tft = TFT_eSPI(); 
void testColorConversion()
{
  uint16_t color16 = tft.color565(100, 150, 200);
  uint32_t color24 = tft.color16to24(color16);
  uint8_t r = (color24 >> 16) & 0xFF;
  uint8_t g = (color24 >> 8) & 0xFF;
  uint8_t b = color24 & 0xFF;
  uint16_t recoveredColor16 = tft.color565(r, g, b);

  Serial.print("Original 16-bit color: ");
  Serial.println(color16, HEX);
  Serial.print("Recovered 16-bit color: ");
  Serial.println(recoveredColor16, HEX);
}


void setup()
{
    Serial.begin(9600);
      tft.init();
  tft.setRotation(2); 
 
}
void loop()
{
    delay(5000);
  testColorConversion();
    
}
