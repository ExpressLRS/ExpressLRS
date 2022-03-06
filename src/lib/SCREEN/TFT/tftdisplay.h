#include "display.h"

class TFTDisplay : public Display
{
  public:
    void init();
    void doScreenBackLight(screen_backlight_t state);

    void displaySplashScreen();
    void displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index);
    void displayMainMenu(menu_item_t menu);
    void displayValue(menu_item_t menu, uint8_t value_index);
    void displayWiFiConfirm();
    void displayWiFiStatus();
    void displayBindConfirm();
    void displayBindStatus();

  private:
    void updateIdleTemperature(uint16_t bgcolor, uint8_t temperature);
    void displayFontCenter(uint32_t font_start_x, uint32_t font_end_x, uint32_t font_start_y,
                                            int font_size, int font_type, String font_string,
                                            uint16_t fgColor, uint16_t bgColor);
};
