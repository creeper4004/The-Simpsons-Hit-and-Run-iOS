//===========================================================================
// Copyright (C) 2000 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   CGuiScreenPauseSettings
//
// Description: Implementation of the CGuiScreenPauseSettings class.
//
// Authors:     Tony Chu
//
// Revisions		Date			Author	    Revision
//                  2002/07/04      TChu        Created for SRR2
//
//===========================================================================

//===========================================================================
// Includes
//===========================================================================
#include <presentation/gui/ingame/guiscreenpausesettings.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <presentation/gui/guimenu.h>
#include <presentation/gui/guitextbible.h>
#include <presentation/gui/guisystem.h>

#include <camera/supercammanager.h>
#include <camera/supercamcentral.h>
#include <input/inputmanager.h>
#include <memory/srrmemory.h>
#include <mission/charactersheet/charactersheetmanager.h>
#include <presentation/tutorialmanager.h>
#include <worldsim/avatarmanager.h>

#include <raddebug.hpp> // Foundation
#include <Group.h>
#include <Screen.h>
#include <Page.h>
#include <Text.h>
#include <strings/unicodeString.h>


#if defined(RAD_ANDROID)
#include <data/config/androidconfigurationmanager.h>
#endif


#if defined(RAD_ANDROID)
  #include <android/log.h>
  #define LOG_TAG "SimpsonsHitAndRun"
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#elif defined(RAD_VITA)
  #include <psp2/kernel/clib.h>
  #define LOGI(...) do { sceClibPrintf(__VA_ARGS__); sceClibPrintf("\n"); } while(0)
  #define LOGE(...) do { sceClibPrintf(__VA_ARGS__); sceClibPrintf("\n"); } while(0)

#else
  #include <cstdio>
  #define LOGI(...) do { std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(0)
  #define LOGE(...) do { std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(0)
#endif


//===========================================================================
// Global Data, Local Data, Local Classes
//===========================================================================




const char* PAUSE_SETTINGS_MENU_ITEMS[] =
{
    "Camera",
    "JumpCamera",
#if !defined(RAD_PC) && !defined(RAD_IOS)
    "InvertCamControl",
#endif
    "IntersectNavSystem",
    "Radar",
#if !defined(RAD_PC) && !defined(RAD_IOS)
    "Vibration",
#endif
    #if !defined(RAD_ANDROID)
    "Tutorial",
    #endif

    ""
};

#ifdef RAD_PC
SuperCam::Type PC_CAMERAS_FOR_WALKING[] =
{
    SuperCam::WALKER_CAM,
    SuperCam::DEBUG_CAM,
    SuperCam::KULL_CAM
};

const int NUM_PC_CAMERAS_FOR_WALKING = sizeof(PC_CAMERAS_FOR_WALKING)/sizeof(SuperCam::Type);
const int NUM_PC_CAMERAS_FOR_WALKING_WITHOUT_CHEAT = 1;
#endif

//===========================================================================
// Public Member Functions
//===========================================================================

//===========================================================================
// CGuiScreenPauseSettings::CGuiScreenPauseSettings
//===========================================================================
// Description: Constructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CGuiScreenPauseSettings::CGuiScreenPauseSettings
(
    Scrooby::Screen* pScreen,
    CGuiEntity* pParent
)
:   CGuiScreen( pScreen, pParent, GUI_SCREEN_ID_SETTINGS ),
    m_pMenu( NULL ),
    m_currentCameraSelectionMode( CAMERA_SELECTION_NOT_AVAILABLE )
{
MEMTRACK_PUSH_GROUP( "CGuiScreenPauseSettings" );



    // Retrieve the Scrooby drawing elements.
    //
    Scrooby::Page* pPage = m_pScroobyScreen->GetPage( "PauseSettings" );



    rAssert( pPage );

    // Create a menu.
    //
    m_pMenu = new CGuiMenu( this, NUM_PAUSE_SETTINGS_MENU_ITEMS, GUI_TEXT_MENU, MENU_SFX_NONE );

    rAssert( m_pMenu != NULL );

    #if defined(RAD_ANDROID)

    for( int i = 0; i < NUM_PAUSE_SETTINGS_MENU_ITEMS; i++ )
    {
        mMenuItemToGuiMenuIndex[ i ] = -1;
        mGuiMenuIndexToMenuItem[ i ] = -1;
    }
    #endif

    // Add menu items.
    //
    char itemName[ 32 ];

    //
    // Diagnostic only:
    // Original PauseSettings code gets each item group directly from pPage.
    // We only check "Menu" to know whether Android layout differs.
    //
    Scrooby::Group* pMenuGroup = pPage->GetGroup( "Menu" );

#if defined(RAD_ANDROID)

    for( int i = 0; i < NUM_PAUSE_SETTINGS_MENU_ITEMS; i++ )
    {
        const char* item = PAUSE_SETTINGS_MENU_ITEMS[ i ];

        const bool added = AddPauseSettingsMenuItemIfAvailable
        (
            pPage,
            item,
            i
        );

        if( i == MENU_ITEM_INVERT_CAM_CONTROL )
        {
            mHasInvertCamControl = added;
        }
        else if( i == MENU_ITEM_VIBRATION )
        {
            mHasGamepadVibration = added;
        }
    }

#else

    for( int i = 0; i < NUM_PAUSE_SETTINGS_MENU_ITEMS; i++ )
    {
        const char* item = PAUSE_SETTINGS_MENU_ITEMS[ i ];



        //
        // IMPORTANT:
        // This is the original access pattern.
        //
        Scrooby::Group* group = pPage->GetGroup( item );

        //
        // Diagnostic only. Do not use this group yet.
        //


        if( group == NULL )
        {
            rAssert( group != NULL );
            MEMTRACK_POP_GROUP( "CGuiScreenPauseSettings" );
            return;
        }

        Scrooby::Text* pText = group->GetText( item );


        if( pText == NULL )
        {
            rAssert( pText != NULL );
            MEMTRACK_POP_GROUP( "CGuiScreenPauseSettings" );
            return;
        }

        pText->SetTextMode( Scrooby::TEXT_WRAP );

        sprintf( itemName, "%s_Value", item );
        Scrooby::Text* pTextValue = group->GetText( itemName );


        if( pTextValue == NULL )
        {
            rAssert( pTextValue != NULL );
            MEMTRACK_POP_GROUP( "CGuiScreenPauseSettings" );
            return;
        }

        pTextValue->SetTextMode( Scrooby::TEXT_WRAP );

        sprintf( itemName, "%s_LArrow", item );
        Scrooby::Sprite* pLArrow = group->GetSprite( itemName );
        sprintf( itemName, "%s_RArrow", item );
        Scrooby::Sprite* pRArrow = group->GetSprite( itemName );



        m_pMenu->AddMenuItem( pText,
                              pTextValue,
                              NULL,
                              NULL,
                              pLArrow,
                              pRArrow,
                              SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
    }
#endif

    #if defined(RAD_ANDROID)
    //
    // Dejamos de dibujar la opción tutorial en este menu
    //
    Scrooby::Group* pTutorialGroup = pPage->GetGroup( "Tutorial" );

    if( pTutorialGroup == NULL && pMenuGroup != NULL )
    {
        pTutorialGroup = pMenuGroup->GetGroup( "Tutorial" );
    }

    if( pTutorialGroup != NULL )
    {
        Scrooby::Text* pTutorialText = pTutorialGroup->GetText( "Tutorial" );
        if( pTutorialText != NULL )
        {
            pTutorialText->SetVisible( false );
        }

        Scrooby::Text* pTutorialValue = pTutorialGroup->GetText( "Tutorial_Value" );
        if( pTutorialValue != NULL )
        {
            pTutorialValue->SetVisible( false );
        }

        Scrooby::Sprite* pTutorialLArrow = pTutorialGroup->GetSprite( "Tutorial_LArrow" );
        if( pTutorialLArrow != NULL )
        {
            pTutorialLArrow->SetVisible( false );
        }

        Scrooby::Sprite* pTutorialRArrow = pTutorialGroup->GetSprite( "Tutorial_RArrow" );
        if( pTutorialRArrow != NULL )
        {
            pTutorialRArrow->SetVisible( false );
        }
    }
#endif

#ifdef RAD_GAMECUBE
    // change "Vibration" text to "Rumble"
    //
    Scrooby::Text* vibrationText = dynamic_cast<Scrooby::Text*>( m_pMenu->GetMenuItem( MENU_ITEM_VIBRATION )->GetItem() );
    rAssert( vibrationText != NULL );
    vibrationText->SetIndex( 1 );
#endif // RAD_GAMECUBE

    for( int j = 0; j < NUM_CAMERA_SELECTION_MODES; j++ )
    {
        m_cameraSelections[ j ] = NULL;
        m_numCameraSelections[ j ] = 0;
    }


    m_cameraSelections[ CAMERA_SELECTION_FOR_DRIVING ] = CAMERAS_FOR_DRIVING;

#ifdef RAD_PC
    m_cameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = PC_CAMERAS_FOR_WALKING;
#else
    m_cameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = CAMERAS_FOR_WALKING;
#endif



    if( GetCheatInputSystem()->IsCheatEnabled( CHEAT_ID_UNLOCK_CAMERAS ) )
    {
        m_numCameraSelections[ CAMERA_SELECTION_FOR_DRIVING ] = NUM_CAMERAS_FOR_DRIVING;

#ifdef RAD_PC
        m_numCameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = NUM_PC_CAMERAS_FOR_WALKING;
#else
        m_numCameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = NUM_CAMERAS_FOR_WALKING;
#endif
    }
    else
    {
        m_numCameraSelections[ CAMERA_SELECTION_FOR_DRIVING ] = NUM_CAMERAS_FOR_DRIVING_WITHOUT_CHEAT;

#ifdef RAD_PC
        m_numCameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = NUM_PC_CAMERAS_FOR_WALKING_WITHOUT_CHEAT;
#else
        m_numCameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = NUM_CAMERAS_FOR_WALKING_WITHOUT_CHEAT;
#endif
    }

    GetCheatInputSystem()->RegisterCallback( this );

MEMTRACK_POP_GROUP( "CGuiScreenPauseSettings" );
}
//===========================================================================
// CGuiScreenPauseSettings::~CGuiScreenPauseSettings
//===========================================================================
// Description: Destructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CGuiScreenPauseSettings::~CGuiScreenPauseSettings()
{
    GetCheatInputSystem()->UnregisterCallback( this );

    if( m_pMenu != NULL )
    {
        delete m_pMenu;
        m_pMenu = NULL;
    }
}


//===========================================================================
// CGuiScreenPauseSettings::HandleMessage
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPauseSettings::HandleMessage
(
	eGuiMessage message, 
	unsigned int param1,
	unsigned int param2 
)
{
    if( m_state == GUI_WINDOW_STATE_RUNNING )
    {
        switch( message )
        {
            case GUI_MSG_CONTROLLER_START:
            {
                // resume game
                //
                m_pParent->HandleMessage( GUI_MSG_UNPAUSE_INGAME );

                break;
            }

            case GUI_MSG_MENU_SELECTION_VALUE_CHANGED:
            {
            #if defined(RAD_ANDROID)
                const int logicalMenuItem =
                    GetMenuItemFromGuiMenuIndex( static_cast<int>( param1 ) );
            #else
                const int logicalMenuItem = static_cast<int>( param1 );
            #endif

                if( logicalMenuItem == MENU_ITEM_CAMERA )
                {
                    // set new super cam
                    //
                    SuperCam::Type superCamType =
                        m_cameraSelections[ m_currentCameraSelectionMode ][ param2 ];

                    GetSuperCamManager()->GetSCC( 0 )->SelectSuperCam
                    (
                        superCamType,
                        SuperCamCentral::CUT,
                        0
                    );

                    // and apply it right away
                    //
                    GetSuperCamManager()->GetSCC( 0 )->Update( 0 );
                }
#if !defined(RAD_PC) && !defined(RAD_ANDROID) && !defined(RAD_IOS)
                else if( param1 == MENU_ITEM_VIBRATION )
                {
                    if( param2 == 1 ) // vibration turned ON
                    {
                        // send vibration pulse to controller
                        //
                        int controllerID = GetInputManager()->GetControllerIDforPlayer( 0 );

#ifdef RAD_PS2
                        if ( GetInputManager()->IsControllerInPort( Input::USB0 ) )
                        {
                            GetInputManager()->TriggerRumblePulse( Input::USB0 );
                        }
                        else if ( GetInputManager()->IsControllerInPort( Input::USB1 ) )
                        {
                            GetInputManager()->TriggerRumblePulse( Input::USB1 );
                        }
                        else
#endif
                        {
                            GetInputManager()->TriggerRumblePulse( controllerID );
                        }
                    }
                }
#endif

                break;
            }

            default:
            {
                break;
            }
        }

        // relay message to menu
        if( m_pMenu != NULL )
        {
            m_pMenu->HandleMessage( message, param1, param2 );
        }
    }

	// Propogate the message up the hierarchy.
	//
	CGuiScreen::HandleMessage( message, param1, param2 );
}

void
CGuiScreenPauseSettings::OnCheatEntered( eCheatID cheatID, bool isEnabled )
{
    if( cheatID == CHEAT_ID_UNLOCK_CAMERAS && isEnabled )
    {
        m_numCameraSelections[ CAMERA_SELECTION_FOR_DRIVING ] = NUM_CAMERAS_FOR_DRIVING;
        m_numCameraSelections[ CAMERA_SELECTION_FOR_WALKING ] = NUM_CAMERAS_FOR_WALKING;
    }
}

//===========================================================================
// CGuiScreenPauseSettings::InitIntro
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPauseSettings::InitIntro()
{



    bool allowCameraToggle = GetSuperCamManager()->GetSCC( 0 )->AllowCameraToggle();

    rAssert( m_pMenu );

    #if defined(RAD_ANDROID)
    const int cameraMenuIndex =
        GetGuiMenuIndexFromMenuItem( MENU_ITEM_CAMERA );

    rAssert( cameraMenuIndex != -1 );

    m_pMenu->SetMenuItemEnabled( cameraMenuIndex, allowCameraToggle );
#else
    m_pMenu->SetMenuItemEnabled( MENU_ITEM_CAMERA, allowCameraToggle );
#endif

    if( allowCameraToggle )
    {
        // update camera selections
        //
        this->UpdateCameraSelections();

    }
    else
    {
        m_currentCameraSelectionMode = CAMERA_SELECTION_NOT_AVAILABLE;
    }

    // update other gameplay settings
    //
    bool isSettingOn = false;

#if !defined(RAD_PC) && !defined(RAD_IOS)

#if defined(RAD_ANDROID)
    {
        const int invertCamMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_INVERT_CAM_CONTROL );

        if( invertCamMenuIndex != -1 )
        {
            const bool realInvertCamera =
            GetAndroidConfigurationManager()->IsInvertCameraEnabled();
                
            m_pMenu->SetSelectionValue
            (
                invertCamMenuIndex,
                realInvertCamera  ? 1 : 0
            );
        }
    }
#else
    isSettingOn =
        GetSuperCamManager()->GetSCC( 0 )->IsInvertedCameraEnabled();

    m_pMenu->SetSelectionValue
    (
        MENU_ITEM_INVERT_CAM_CONTROL,
        isSettingOn ? 1 : 0
    );
#endif

#endif



    isSettingOn = GetSuperCamManager()->GetSCC( 0 )->JumpCamsEnabled();



    #if defined(RAD_ANDROID)
    {
        const int jumpCamerasMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_JUMP_CAMERAS );

        rAssert( jumpCamerasMenuIndex != -1 );

        m_pMenu->SetSelectionValue
        (
            jumpCamerasMenuIndex,
            isSettingOn ? 1 : 0
        );
    }
#else
    m_pMenu->SetSelectionValue
    (
        MENU_ITEM_JUMP_CAMERAS,
        isSettingOn ? 1 : 0
    );
#endif



    isSettingOn = GetCharacterSheetManager()->QueryNavSystemSetting();



    #if defined(RAD_ANDROID)
    {
        const int intersectNavMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_INTERSECT_NAV_SYSTEM );

        rAssert( intersectNavMenuIndex != -1 );

        m_pMenu->SetSelectionValue
        (
            intersectNavMenuIndex,
            isSettingOn ? 1 : 0
        );
    }
#else
    m_pMenu->SetSelectionValue
    (
        MENU_ITEM_INTERSECT_NAV_SYSTEM,
        isSettingOn ? 1 : 0
    );
#endif



    isSettingOn = GetGuiSystem()->IsRadarEnabled();



    #if defined(RAD_ANDROID)
    {
        const int radarMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_RADAR );

        rAssert( radarMenuIndex != -1 );

        m_pMenu->SetSelectionValue
        (
            radarMenuIndex,
            isSettingOn ? 1 : 0
        );
    }
#else
    m_pMenu->SetSelectionValue
    (
        MENU_ITEM_RADAR,
        isSettingOn ? 1 : 0
    );
#endif

#if !defined(RAD_PC) && !defined(RAD_IOS)

#if defined(RAD_ANDROID)
    {
        const int vibrationMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_VIBRATION );

        if( vibrationMenuIndex != -1 )
        {
            isSettingOn =
                GetAndroidConfigurationManager()->IsGamepadVibrationEnabled();

            m_pMenu->SetSelectionValue
            (
                vibrationMenuIndex,
                isSettingOn ? 1 : 0
            );
        }
    }
#else
    isSettingOn = GetInputManager()->IsRumbleEnabled();

    m_pMenu->SetSelectionValue
    (
        MENU_ITEM_VIBRATION,
        isSettingOn ? 1 : 0
    );
#endif

#endif

#if !defined(RAD_ANDROID)
    isSettingOn = GetTutorialManager()->IsTutorialEventsEnabled();
    m_pMenu->SetSelectionValue( MENU_ITEM_TUTORIAL,
                                isSettingOn ? 1 : 0 );

    // show/hide tutorial setting depending on whether or not tutorial mode is enabled
    //
    if( GetTutorialManager()->IsTutorialModeEnabled() )
    {
        CGuiManagerInGame* guiManagerIngame = static_cast<CGuiManagerInGame*>( m_pParent );
        rAssert( guiManagerIngame != NULL );

        bool isTutorialToggleAllowed = (guiManagerIngame->GetResumeGameScreenID() != GUI_SCREEN_ID_TUTORIAL);
        m_pMenu->SetMenuItemEnabled( MENU_ITEM_TUTORIAL, isTutorialToggleAllowed );
    }
    else
    {
        m_pMenu->SetMenuItemEnabled( MENU_ITEM_TUTORIAL, false, true );
    }
#endif

}
//===========================================================================
// CGuiScreenPauseSettings::InitRunning
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPauseSettings::InitRunning()
{
}


//===========================================================================
// CGuiScreenPauseSettings::InitOutro
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenPauseSettings::InitOutro()
{
    // apply new gameplay settings
    //
    rAssert( m_pMenu != NULL );
    bool isSettingOn = false;

    #if !defined(RAD_PC) && !defined(RAD_IOS)

#if defined(RAD_ANDROID)
    {
        const int invertCamMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_INVERT_CAM_CONTROL );

        if( invertCamMenuIndex != -1 )
        {
            const bool menuInvertCamera =( m_pMenu->GetSelectionValue( invertCamMenuIndex ) == 1 );

            const bool runtimeInvertCamera = !menuInvertCamera;

            GetSuperCamManager()->GetSCC( 0 )->EnableInvertedCamera( runtimeInvertCamera );

            GetAndroidConfigurationManager()->SetInvertCameraEnabled( menuInvertCamera );
         }
    }
#else
    isSettingOn =
        ( m_pMenu->GetSelectionValue( MENU_ITEM_INVERT_CAM_CONTROL ) == 1 );

    GetSuperCamManager()->GetSCC( 0 )->EnableInvertedCamera( isSettingOn );
#endif

#endif



    #if defined(RAD_ANDROID)
    {
        const int jumpCamerasMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_JUMP_CAMERAS );

        rAssert( jumpCamerasMenuIndex != -1 );

        isSettingOn =
            ( m_pMenu->GetSelectionValue( jumpCamerasMenuIndex ) == 1 );

        GetSuperCamManager()->GetSCC( 0 )->EnableJumpCams( isSettingOn );
    }
#else
    isSettingOn =
        ( m_pMenu->GetSelectionValue( MENU_ITEM_JUMP_CAMERAS ) == 1 );

    GetSuperCamManager()->GetSCC( 0 )->EnableJumpCams( isSettingOn );
#endif



    #if defined(RAD_ANDROID)
    {
        const int intersectNavMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_INTERSECT_NAV_SYSTEM );

        rAssert( intersectNavMenuIndex != -1 );

        isSettingOn =
            ( m_pMenu->GetSelectionValue( intersectNavMenuIndex ) == 1 );

        GetCharacterSheetManager()->SetNavSystemOn( isSettingOn );
    }
#else
    isSettingOn =
        ( m_pMenu->GetSelectionValue( MENU_ITEM_INTERSECT_NAV_SYSTEM ) == 1 );

    GetCharacterSheetManager()->SetNavSystemOn( isSettingOn );
#endif



    #if defined(RAD_ANDROID)
    {
        const int radarMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_RADAR );

        rAssert( radarMenuIndex != -1 );

        isSettingOn =
            ( m_pMenu->GetSelectionValue( radarMenuIndex ) == 1 );

        GetGuiSystem()->SetRadarEnabled( isSettingOn );
    }
#else
    isSettingOn =
        ( m_pMenu->GetSelectionValue( MENU_ITEM_RADAR ) == 1 );

    GetGuiSystem()->SetRadarEnabled( isSettingOn );
#endif

#if !defined(RAD_PC) && !defined(RAD_IOS)

#if defined(RAD_ANDROID)
    {
        const int vibrationMenuIndex =
            GetGuiMenuIndexFromMenuItem( MENU_ITEM_VIBRATION );

        if( vibrationMenuIndex != -1 )
        {
            isSettingOn =
                ( m_pMenu->GetSelectionValue( vibrationMenuIndex ) == 1 );

            GetAndroidConfigurationManager()->SetGamepadVibrationEnabled( isSettingOn );
            // FIX BUG:
            //muy importante esta linea de abajo para que cuando el juego arranque
            // con Simpsons_configuration.txt en false para vibracion poder activarla in-game 
            // usando el menuSettings extended de la versión de consolas, contenido en el ingame.p3d
             GetInputManager()->SetRumbleEnabled( isSettingOn );
        }
    }
#else
    isSettingOn =
        ( m_pMenu->GetSelectionValue( MENU_ITEM_VIBRATION ) == 1 );

    GetInputManager()->SetRumbleEnabled( isSettingOn );
#endif

#endif

#if !defined(RAD_ANDROID)
    isSettingOn = (m_pMenu->GetSelectionValue( MENU_ITEM_TUTORIAL ) == 1) 
#ifdef RAD_PC
                  && !(GetInputManager()->GetController(0)->IsTutorialDisabled())
#endif                  
                  ;
    GetTutorialManager()->EnableTutorialEvents( isSettingOn );
#endif

#if defined(RAD_ANDROID)
    GetAndroidConfigurationManager()->SaveIfDirty();
#endif

}

//---------------------------------------------------------------------
// Private Functions
//---------------------------------------------------------------------

void
CGuiScreenPauseSettings::UpdateCameraSelections()
{
    rAssert( m_pMenu );

    // set current camera selection mode
    //
    bool isPlayerInCar = GetAvatarManager()->GetAvatarForPlayer( 0 )->IsInCar();



    m_currentCameraSelectionMode = isPlayerInCar ? CAMERA_SELECTION_FOR_DRIVING : CAMERA_SELECTION_FOR_WALKING;



    // get current active camera
    //
    SuperCam* currentSuperCam = GetSuperCamManager()->GetSCC( 0 )->GetActiveSuperCam();



    rAssert( currentSuperCam );

    SuperCam::Type currentSuperCamType = currentSuperCam->GetType();



    int currentCameraIndex = -1;

    // check to see if current camera is in the selection list
    //
    for( int i = 0; i < m_numCameraSelections[ m_currentCameraSelectionMode ]; i++ )
    {


        if( m_cameraSelections[ m_currentCameraSelectionMode ][ i ] == currentSuperCamType )
        {
            currentCameraIndex = i;

            break;
        }
    }

    if( currentCameraIndex == -1 )
    {


        // current camera not found in selection list, disable camera selection
        //
        m_pMenu->SetMenuItemEnabled( MENU_ITEM_CAMERA, false );

        m_currentCameraSelectionMode = CAMERA_SELECTION_NOT_AVAILABLE;

        return;
    }



    m_pMenu->SetMenuItemEnabled( MENU_ITEM_CAMERA, true );



    // set number of camera selections available
    //
    m_pMenu->SetSelectionValueCount( MENU_ITEM_CAMERA,
                                     m_numCameraSelections[ m_currentCameraSelectionMode ] );

    // update camera names
    //
    for( int i = 0; i < m_numCameraSelections[ m_currentCameraSelectionMode ]; i++ )
    {
        GuiMenuItem* menuItemCamera = m_pMenu->GetMenuItem( MENU_ITEM_CAMERA );


        rAssert( menuItemCamera != NULL );

        SuperCam* superCam = GetSuperCamManager()->GetSCC( 0 )->GetSuperCam( m_cameraSelections[ m_currentCameraSelectionMode ][ i ] );

        rAssert( superCam );

        HeapMgr()->PushHeap( GMA_LEVEL_HUD );

        P3D_UNICODE* stringBuffer = GetTextBibleString( superCam->GetName() );

        rAssert( stringBuffer );

        UnicodeString unicodeString;
        unicodeString.ReadUnicode( static_cast<UnicodeChar*>( stringBuffer ) );

        Scrooby::Text* menuItemValue = dynamic_cast<Scrooby::Text*>( menuItemCamera->GetItemValue() );



        rAssert( menuItemValue != NULL );
        menuItemValue->SetString( i, unicodeString );

        HeapMgr()->PopHeap(GMA_LEVEL_HUD);
    }

    // set current camera selection
    //
    m_pMenu->SetSelectionValue( MENU_ITEM_CAMERA, currentCameraIndex );


}

#if defined(RAD_ANDROID)

bool CGuiScreenPauseSettings::AddPauseSettingsMenuItemIfAvailable
(
    Scrooby::Page* pPage,
    const char* item,
    int logicalMenuItem
)
{
    char itemName[ 32 ];

    Scrooby::Group* group = pPage->GetGroup( item );

    if( group == NULL )
    {
        if( IsAndroidOptionalMenuItem( logicalMenuItem ) )
        {
            return false;
        }

        rAssert( group != NULL );
        return false;
    }

    Scrooby::Text* pText = group->GetText( item );

    if( pText == NULL )
    {
        if( IsAndroidOptionalMenuItem( logicalMenuItem ) )
        {
            return false;
        }

        rAssert( pText != NULL );
        return false;
    }

    pText->SetTextMode( Scrooby::TEXT_WRAP );

    sprintf( itemName, "%s_Value", item );
    Scrooby::Text* pTextValue = group->GetText( itemName );

    if( pTextValue == NULL )
    {
        if( IsAndroidOptionalMenuItem( logicalMenuItem ) )
        {
            pText->SetVisible( false );
            return false;
        }

        rAssert( pTextValue != NULL );
        return false;
    }

    pTextValue->SetTextMode( Scrooby::TEXT_WRAP );

    sprintf( itemName, "%s_LArrow", item );
    Scrooby::Sprite* pLArrow = group->GetSprite( itemName );

    sprintf( itemName, "%s_RArrow", item );
    Scrooby::Sprite* pRArrow = group->GetSprite( itemName );

    if( IsAndroidOptionalMenuItem( logicalMenuItem ) &&
        ( pLArrow == NULL || pRArrow == NULL ) )
    {
        pText->SetVisible( false );
        pTextValue->SetVisible( false );

        if( pLArrow != NULL )
        {
            pLArrow->SetVisible( false );
        }

        if( pRArrow != NULL )
        {
            pRArrow->SetVisible( false );
        }

        return false;
    }

    mMenuItemToGuiMenuIndex[ logicalMenuItem ] = mNumActiveMenuItems;
    mGuiMenuIndexToMenuItem[ mNumActiveMenuItems ] = logicalMenuItem;

    m_pMenu->AddMenuItem
    (
        pText,
        pTextValue,
        NULL,
        NULL,
        pLArrow,
        pRArrow,
        SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED
    );

    mNumActiveMenuItems++;

    return true;
}

bool CGuiScreenPauseSettings::IsAndroidOptionalMenuItem( int logicalMenuItem ) const
{
    return logicalMenuItem == MENU_ITEM_INVERT_CAM_CONTROL ||
           logicalMenuItem == MENU_ITEM_VIBRATION;
}

bool CGuiScreenPauseSettings::HasMenuItem( int logicalMenuItem ) const
{
    return GetGuiMenuIndexFromMenuItem( logicalMenuItem ) != -1;
}

int CGuiScreenPauseSettings::GetGuiMenuIndexFromMenuItem( int logicalMenuItem ) const
{
    if( logicalMenuItem < 0 || logicalMenuItem >= NUM_PAUSE_SETTINGS_MENU_ITEMS )
    {
        return -1;
    }

    return mMenuItemToGuiMenuIndex[ logicalMenuItem ];
}

int CGuiScreenPauseSettings::GetMenuItemFromGuiMenuIndex( int guiMenuIndex ) const
{
    if( guiMenuIndex < 0 || guiMenuIndex >= mNumActiveMenuItems )
    {
        return -1;
    }

    return mGuiMenuIndexToMenuItem[ guiMenuIndex ];
}

#endif
