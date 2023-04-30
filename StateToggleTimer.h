// Manage a toggle state timer, for example flashing of an LED or a flashing indicator on a display

#ifndef _STATE_TOGGLE_TIMER
#define _STATE_TOGGLE_TIMER

class StateToggleTimer
{
  public:
    StateToggleTimer(const unsigned long duration) 
    {
      m_bIsOn = false;
      m_bPreviousState = false;
      m_bHasStarted = false;
      m_duration = duration;
      m_timer = millis();
    }

    inline bool IsOn() { return m_bIsOn; }
    inline bool HasStarted() { return m_bHasStarted; }
    
    inline void SetDuration(const unsigned long duration) { m_duration = duration; }

    void Start()
    {
      m_bIsOn = true;
      m_bHasStarted = true;
      m_timer = millis();
    }

    void Stop()
    {
      m_bIsOn = true;
      m_bHasStarted = false;
    }

    void Update(const unsigned long now)
    {
      if (m_bHasStarted)
      {
        if ((now - m_timer) > m_duration)
        {
          m_timer = now;
          m_bIsOn = !m_bIsOn;
        }
      }
      else
      {
        m_bIsOn = true;
      }
    }

    bool HasStateChanged()
    {
      bool bChanged = !(m_bPreviousState == m_bIsOn);
      m_bPreviousState = m_bIsOn;
      return bChanged;
    }

  private:
    bool m_bIsOn;               // Flashing changes this state between on/off
    bool m_bPreviousState;      // Keep track of the previous state
    bool m_bHasStarted;         // Is the flash system enabled?
    unsigned long m_duration;   // Duration of a flash
    unsigned long m_timer;      // Timer to control the flash
};

#endif