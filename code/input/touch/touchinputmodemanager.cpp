#include <input/touch/touchinputmodemanager.h>
#if defined(RAD_IOS)
#include <SDL.h>
#endif

TouchInputModeManager& TouchInputModeManager::GetInstance()
{
    static TouchInputModeManager sInstance;
    return sInstance;
}

TouchInputModeManager::TouchInputModeManager()
    :
    mCurrentMode( TOUCH_INPUT_MODE_TOUCH ),
    mGamepadCandidate(false),
    mGamepadConnected( false ),
    mTouchHudEnabled( true ),
#if defined(RAD_IOS)
    mIosPhysicalGamepadConnected( false ),
    mIosTouchVisibility( 0 ),
#endif
    mGamepadInputThreshold( 0.20f ),
    mTimeMs( 0 ),
    mLastTouchInputTimeMs( 0 ),
    mLastGamepadInputTimeMs( 0 )
{
}

TouchInputModeManager::~TouchInputModeManager()
{
}

void TouchInputModeManager::Reset()
{
    mCurrentMode = TOUCH_INPUT_MODE_TOUCH;
    mGamepadCandidate = false;
    mGamepadConnected = false;
    mTouchHudEnabled = true;
#if defined(RAD_IOS)
    mIosPhysicalGamepadConnected = false;
    mIosTouchVisibility = 0;
#endif
    mGamepadInputThreshold = 0.20f;
    mTimeMs = 0;
    mLastTouchInputTimeMs = 0;
    mLastGamepadInputTimeMs = 0;
}

void TouchInputModeManager::Update( unsigned int elapsedMs )
{
    mTimeMs += elapsedMs;
#if defined(RAD_IOS)
    // Reconcile the device list each frame: covers startup, hot-plugging,
    // and removal of one controller while another remains connected.
    // Keyboard/mouse, accelerometer and SDL virtual devices must not hide UI.
    mIosPhysicalGamepadConnected = false;
    const int joystickCount = SDL_NumJoysticks();
    for ( int i = 0; i < joystickCount; ++i )
    {
        if ( SDL_IsGameController( i ) && !SDL_JoystickIsVirtual( i ) )
        {
            mIosPhysicalGamepadConnected = true;
            break;
        }
    }
#endif
}

void TouchInputModeManager::NotifyTouchInput()
{
    mLastTouchInputTimeMs = mTimeMs;

     /*
     * Confirmed physical gamepad has priority.
     * Touch input must not steal the active mode while a real gamepad
     * is confirmed and connected.
     */
    if ( mGamepadConnected )
    {
        return;
    }

    mCurrentMode = TOUCH_INPUT_MODE_TOUCH;
}

void TouchInputModeManager::NotifyGamepadCandidateConnected()
{
    /*
     * SDL/Android detected something that may be a physical gamepad.
     *
     * Important:
     * A candidate must NOT hide the touch HUD.
     * A candidate must NOT block touch input.
     * A candidate must NOT enable rumble.
     *
     * Only real input from the physical controller confirms the gamepad.
     */
    mGamepadCandidate = true;

    if ( !mGamepadConnected )
    {
        mCurrentMode = TOUCH_INPUT_MODE_TOUCH;
    }
}

void TouchInputModeManager::NotifyGamepadConnected()
{
#if defined(RAD_IOS)
    // iOS sideload is touch-primary; the simulator / SDL may report a
    // virtual gamepad (keyboard/mouse). Do not switch to gamepad mode or
    // the touch HUD gets suppressed.
    return;
#else
    /*
     * This now means:
     * A physical gamepad has been CONFIRMED by real input.
     *
     * Do not call this just because SDL enumerated a device.
     */
    if ( mGamepadConnected )
    {
        return;
    }
    mGamepadCandidate = true;
    mGamepadConnected = true;
    mCurrentMode = TOUCH_INPUT_MODE_GAMEPAD;
    mLastGamepadInputTimeMs = mTimeMs;
#endif
}

void TouchInputModeManager::NotifyGamepadDisconnected()
{
    mGamepadCandidate = false;
    mGamepadConnected = false;

    // On Android, if the external gamepad is gone, touch should become the
    // default available input method again.
    mCurrentMode = TOUCH_INPUT_MODE_TOUCH;
}

void TouchInputModeManager::NotifyGamepadInput( float magnitude )
{
    if ( magnitude < 0.0f )
    {
        magnitude = -magnitude;
    }

    if ( magnitude < mGamepadInputThreshold )
    {
        return;
    }

    // Nos confirma una pulsación real del mando
    // por lo que bloqueamos el tactil hasta que se nos envie que dicho mando se desconecte
    NotifyGamepadConnected();
}

void TouchInputModeManager::ForceMode( TouchInputMode mode )
{
    // mmm... Esto es una capa extra de seguridad  la cual no se si es estrictamente necesaria...
    // la dejo por el momento luego pruebo el sistema sin este if de seguridad
    // temo porque hay muchos telefonos distintos
    if ( mGamepadConnected && mode == TOUCH_INPUT_MODE_TOUCH )
    {
        return;
    }
    mCurrentMode = mode;
}