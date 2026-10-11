#ifndef TOUCHINPUTMODEMANAGER_H_
#define TOUCHINPUTMODEMANAGER_H_

#include <input/touch/touchtypes.h>

//=============================================================================
// TouchInputModeManager
//
// Decides whether the current active input mode is Touch or Gamepad.
//
// Important:
// - This does NOT process SDL events directly yet.
// - This does NOT modify gameplay.
// - Later, the SDL event loop will call NotifyTouchInput(),
//   NotifyGamepadInput(), NotifyGamepadConnected(), etc.
//=============================================================================

class TouchInputModeManager
{
public:
    static TouchInputModeManager& GetInstance();

    void Reset();

    void Update( unsigned int elapsedMs );

    void NotifyTouchInput();
    void NotifyGamepadCandidateConnected();
    void NotifyGamepadConnected();
    void NotifyGamepadDisconnected();
    void NotifyGamepadInput( float magnitude = 1.0f );

    void ForceMode( TouchInputMode mode );

    TouchInputMode GetMode() const;

    bool IsTouchMode() const;
    bool IsGamepadMode() const;

    bool IsGamepadConnected() const;

    bool IsGamepadCandidate() const;
    bool IsGamepadConfirmed() const;
    bool IsTouchInputBlockedByGamepad() const;

    void SetTouchHudEnabled( bool enabled );
    bool IsTouchHudEnabled() const;
    bool ShouldShowTouchHud() const;

    void SetGamepadInputThreshold( float threshold );
    float GetGamepadInputThreshold() const;

    unsigned int GetTimeMs() const;
    unsigned int GetLastTouchInputTimeMs() const;
    unsigned int GetLastGamepadInputTimeMs() const;

private:
    TouchInputModeManager();
    ~TouchInputModeManager();

    TouchInputModeManager( const TouchInputModeManager& );
    TouchInputModeManager& operator=( const TouchInputModeManager& );

    TouchInputMode mCurrentMode;

    bool mGamepadCandidate;
    bool mGamepadConnected;
    bool mTouchHudEnabled;
#if defined(RAD_IOS)
    bool mIosPhysicalGamepadConnected;
#endif

    float mGamepadInputThreshold;

    unsigned int mTimeMs;
    unsigned int mLastTouchInputTimeMs;
    unsigned int mLastGamepadInputTimeMs;
};

inline TouchInputMode TouchInputModeManager::GetMode() const
{
    return mCurrentMode;
}

inline bool TouchInputModeManager::IsTouchMode() const
{
    return mCurrentMode == TOUCH_INPUT_MODE_TOUCH;
}

inline bool TouchInputModeManager::IsGamepadMode() const
{
    return mCurrentMode == TOUCH_INPUT_MODE_GAMEPAD;
}

inline bool TouchInputModeManager::IsGamepadConnected() const
{
    return mGamepadConnected;
}

inline bool TouchInputModeManager::IsGamepadCandidate() const
{
    return mGamepadCandidate;
}

inline bool TouchInputModeManager::IsGamepadConfirmed() const
{
    return mGamepadConnected;
}

inline bool TouchInputModeManager::IsTouchInputBlockedByGamepad() const
{
#if defined(RAD_IOS)
    // iOS sideload is touch-primary; the simulator / SDL may report a
    // virtual gamepad which must not suppress the touch HUD.
    return false;
#else
    return mGamepadConnected;
#endif
}

inline void TouchInputModeManager::SetTouchHudEnabled( bool enabled )
{
    mTouchHudEnabled = enabled;
}

inline bool TouchInputModeManager::IsTouchHudEnabled() const
{
    return mTouchHudEnabled;
}

inline bool TouchInputModeManager::ShouldShowTouchHud() const
{
#if defined(RAD_IOS)
    // Hide controls for a real gamepad, even before its first button press.
    return mTouchHudEnabled && !mIosPhysicalGamepadConnected && IsTouchMode();
#else
    return mTouchHudEnabled &&
           !IsTouchInputBlockedByGamepad() &&
           IsTouchMode();
#endif
}

inline void TouchInputModeManager::SetGamepadInputThreshold( float threshold )
{
    mGamepadInputThreshold = threshold;
}

inline float TouchInputModeManager::GetGamepadInputThreshold() const
{
    return mGamepadInputThreshold;
}

inline unsigned int TouchInputModeManager::GetTimeMs() const
{
    return mTimeMs;
}

inline unsigned int TouchInputModeManager::GetLastTouchInputTimeMs() const
{
    return mLastTouchInputTimeMs;
}

inline unsigned int TouchInputModeManager::GetLastGamepadInputTimeMs() const
{
    return mLastGamepadInputTimeMs;
}

#endif // TOUCHINPUTMODEMANAGER_H_