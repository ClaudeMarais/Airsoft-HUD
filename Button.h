// Manage a button presses

#ifndef _BUTTON
#define _BUTTON

#define DefineButton(buttonName, pin) \
  Button buttonName(pin); \
  void buttonName##ISR() { buttonName.onInterrupt(); }

#define SetupButton(buttonName) \
  buttonName.Setup(buttonName##ISR);

class Button
{
  public:
    Button(int pin) 
    {
      m_pin = pin;
      m_bIsPressed = false;
      m_bWasPressed = false;
      m_bWasLongPressed = false;
      lastState = !bIsPressed;
      lastStateChangeTime = millis();
    }

    void Setup(void(ISR)())
    {
      pinMode(m_pin, INPUT_PULLUP);
      attachInterrupt(digitalPinToInterrupt(m_pin), ISR, CHANGE);
    }

    void Reset()
    {
      m_bReset = true;
      m_bIsPressed = false;
      m_bWasPressed = false;
    }

    inline bool IsPressed() { return m_bIsPressed; }

    bool WasPressed()
    {
      if (m_bWasPressed)
      {
        m_bWasPressed = false;
        return true;
      }
      return false;
    }

    // Check if button was long pressed
    bool WasLongPressed()
    {
      if (m_bWasLongPressed)
      {
        m_bWasLongPressed = false;
        return true;
      }
      return false;
    }

    void onInterrupt()
    {
      unsigned long now = millis();

      unsigned long timeSinceLastStateChange = now - lastStateChangeTime;
      if (timeSinceLastStateChange > debounceDelay)
      {
        bool currentState = digitalRead(m_pin);

        // Check if there was a state change
        if (currentState != lastState)
        {
          lastStateChangeTime = now;
          lastState = currentState;

          m_bIsPressed = (currentState == bIsPressed);

          // Once the button is released, we record that it was pressed
          if (currentState != bIsPressed)
          {
            m_bWasPressed = true;

            if (timeSinceLastStateChange > longPressDelay)
            {
              m_bWasLongPressed = true;
            }
            else
            {
              m_bWasLongPressed = false;
            }
          }
          else
          {
            m_bWasLongPressed = false;
          }

          if (m_bReset)
          {
            m_bReset = false;
            m_bIsPressed = false;
            m_bWasPressed = false;            
            m_bWasLongPressed = false;
          }
        }
      }
    }

  private:
    static const unsigned long debounceDelay = 50;
    static const unsigned long longPressDelay = 1000;
    static const bool bIsPressed = LOW;

    int m_pin;
    unsigned long lastStateChangeTime;
    bool lastState;
    volatile bool m_bIsPressed;
    volatile bool m_bWasPressed;
    volatile bool m_bWasLongPressed;
    volatile bool m_bReset;
};


#endif