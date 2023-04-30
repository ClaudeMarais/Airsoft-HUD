# ESP32-CAM With SSD1306 OLED Display
 A simple Arduino project to show how to use a SSD1306 OLED display with ESP32-CAM. The reason why this is interesting is that the SSD1306 uses I2C, not SPI. Since the ESP32-CAM doesn't have I2C pins, we have to use the Wire lib to define our own I2C pins.
 