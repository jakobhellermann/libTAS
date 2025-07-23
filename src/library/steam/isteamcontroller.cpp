/*
    Copyright 2015-2024 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "isteamcontroller.h"

#include "logging.h"
#include "inputs/inputs.h"
#include "global.h"
#include "shared/SharedConfig.h"

#include <string>
#include <vector>

namespace libtas {

static ControllerActionSetHandle_t controllerActionSets[AllInputs::MAXJOYS];
static std::vector<std::string> actionSets, digitalActions, analogActions;

bool ISteamController::Init()
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    return true;
}

bool ISteamController::Shutdown()
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    return true;
}

void ISteamController::RunFrame()
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
}

int ISteamController::GetConnectedControllers( ControllerHandle_t *handlesOut )
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    if (handlesOut) {
        for (int i = 0; i < Global::shared_config.nb_controllers; i++)
            handlesOut[i] = i + 1;
    }
    return Global::shared_config.nb_controllers;
}

bool ISteamController::ShowBindingPanel( ControllerHandle_t controllerHandle )
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    /* Return false to indicate that we are not in Big Picture Mode. */
    return false;
}

ControllerActionSetHandle_t ISteamController::GetActionSetHandle( const char *pszActionSetName )
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    for (unsigned i = 0; i < actionSets.size(); i++)
        if (actionSets[i] == pszActionSetName)
            return i + 1;
    actionSets.emplace_back(pszActionSetName);
    return actionSets.size();
}

void ISteamController::ActivateActionSet( ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle )
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    if (controllerHandle - 1 < ControllerHandle_t(Global::shared_config.nb_controllers))
        controllerActionSets[controllerHandle - 1] = actionSetHandle;
    else if (controllerHandle == STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS)
        for (int i = 0; i < Global::shared_config.nb_controllers; i++)
            controllerActionSets[i] = actionSetHandle;
}

ControllerActionSetHandle_t ISteamController::GetCurrentActionSet( ControllerHandle_t controllerHandle )
{
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    if (controllerHandle - 1 >= ControllerHandle_t(Global::shared_config.nb_controllers))
        return 0;
    return controllerActionSets[controllerHandle - 1];
}

ControllerDigitalActionHandle_t ISteamController::GetDigitalActionHandle( const char *pszActionName ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    for (unsigned i = 0; i < digitalActions.size(); i++)
        if (digitalActions[i] == pszActionName)
            return i + 1;
    digitalActions.emplace_back(pszActionName);
    return digitalActions.size();
}

ControllerDigitalActionData_t ISteamController::GetDigitalActionData( ControllerHandle_t controllerHandle, ControllerDigitalActionHandle_t digitalActionHandle ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    ControllerDigitalActionData_t data;
    data.bState = false;
    data.bActive = false;
    if (controllerHandle - 1 >= ControllerHandle_t(Global::shared_config.nb_controllers) ||
        digitalActionHandle - 1 >= digitalActions.size())
        return data;
    std::string digitalAction(digitalActions[digitalActionHandle - 1]);
    auto split = digitalAction.find('_');
    if (split != std::string::npos)
        digitalAction.resize(split);
    if (digitalAction == "a")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_A & 1;
    else if (digitalAction == "b")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_B & 1;
    else if (digitalAction == "x")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_X & 1;
    else if (digitalAction == "y")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_Y & 1;
    else if (digitalAction == "back")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_BACK & 1;
    else if (digitalAction == "start")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_START & 1;
    else if (digitalAction == "lshl")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_LEFTSHOULDER & 1;
    else if (digitalAction == "rshl")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_RIGHTSHOULDER & 1;
    else if (digitalAction == "dpup")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_DPAD_UP & 1;
    else if (digitalAction == "dpdn")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_DPAD_DOWN & 1;
    else if (digitalAction == "dplt")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_DPAD_LEFT & 1;
    else if (digitalAction == "dprt")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].buttons >> SingleInput::BUTTON_DPAD_RIGHT & 1;
    else if (digitalAction == "ltrg")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].axes[SingleInput::AXIS_TRIGGERLEFT] > 0;
    else if (digitalAction == "rtrg")
        data.bState = Inputs::game_ai.controllers[controllerHandle - 1].axes[SingleInput::AXIS_TRIGGERRIGHT] > 0;
    else
        return data;
    data.bActive = true;
    return data;
}

int ISteamController::GetDigitalActionOrigins( ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle, ControllerDigitalActionHandle_t digitalActionHandle, EControllerActionOrigin *originsOut ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    if (controllerHandle - 1 >= ControllerHandle_t(Global::shared_config.nb_controllers) ||
        actionSetHandle - 1 >= actionSets.size() ||
        digitalActionHandle - 1 >= digitalActions.size())
        return 0;
    std::string digitalAction(digitalActions[digitalActionHandle - 1]);
    auto split = digitalAction.find('_');
    if (split != std::string::npos)
        digitalAction.resize(split);
    if (digitalAction == "a")
        originsOut[0] = k_EControllerActionOrigin_XBox360_A;
    else if (digitalAction == "b")
        originsOut[0] = k_EControllerActionOrigin_XBox360_B;
    else if (digitalAction == "x")
        originsOut[0] = k_EControllerActionOrigin_XBox360_X;
    else if (digitalAction == "y")
        originsOut[0] = k_EControllerActionOrigin_XBox360_Y;
    else if (digitalAction == "back")
        originsOut[0] = k_EControllerActionOrigin_XBox360_Back;
    else if (digitalAction == "start")
        originsOut[0] = k_EControllerActionOrigin_XBox360_Start;
    else if (digitalAction == "lshl")
        originsOut[0] = k_EControllerActionOrigin_XBox360_LeftBumper;
    else if (digitalAction == "rshl")
        originsOut[0] = k_EControllerActionOrigin_XBox360_RightBumper;
    else if (digitalAction == "dpup")
        originsOut[0] = k_EControllerActionOrigin_XBox360_DPad_North;
    else if (digitalAction == "dpdn")
        originsOut[0] = k_EControllerActionOrigin_XBox360_DPad_South;
    else if (digitalAction == "dplt")
        originsOut[0] = k_EControllerActionOrigin_XBox360_DPad_West;
    else if (digitalAction == "dprt")
        originsOut[0] = k_EControllerActionOrigin_XBox360_DPad_East;
    else if (digitalAction == "ltrg")
        originsOut[0] = k_EControllerActionOrigin_XBox360_LeftTrigger_Click;
    else if (digitalAction == "rtrg")
        originsOut[0] = k_EControllerActionOrigin_XBox360_RightTrigger_Click;
    else
        return 0;
    return 1;
}

ControllerAnalogActionHandle_t ISteamController::GetAnalogActionHandle( const char *pszActionName ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    for (unsigned i = 0; i < analogActions.size(); i++)
        if (analogActions[i] == pszActionName)
            return i + 1;
    analogActions.emplace_back(pszActionName);
    return analogActions.size();
}

ControllerAnalogActionData_t ISteamController::GetAnalogActionData( ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t analogActionHandle ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    ControllerAnalogActionData_t data;
    data.eMode = k_EControllerSourceMode_None;
    data.x = data.y = 0.0f;
    data.bActive = false;
    if (controllerHandle - 1 >= ControllerHandle_t(Global::shared_config.nb_controllers) ||
        analogActionHandle - 1 >= analogActions.size())
        return data;
    std::string analogAction(analogActions[analogActionHandle - 1]);
    auto split = analogAction.find('_');
    if (split != std::string::npos)
        analogAction.resize(split);
    if (analogAction == "analogl") {
        data.x = (Inputs::game_ai.controllers[controllerHandle - 1].axes[SingleInput::AXIS_LEFTX] + 0.5f) * (2/65535.0f);
        data.y = (Inputs::game_ai.controllers[controllerHandle - 1].axes[SingleInput::AXIS_LEFTY] + 0.5f) * (2/65535.0f);
    }
    else if (analogAction == "analogr") {
        data.x = (Inputs::game_ai.controllers[controllerHandle - 1].axes[SingleInput::AXIS_RIGHTX] + 0.5f) * (2/65535.0f);
        data.y = (Inputs::game_ai.controllers[controllerHandle - 1].axes[SingleInput::AXIS_RIGHTX] + 0.5f) * (2/65535.0f);
    }
    else
        return data;
    data.eMode = k_EControllerSourceMode_JoystickMove;
    data.bActive = true;
    return data;
}

int ISteamController::GetAnalogActionOrigins( ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle, ControllerAnalogActionHandle_t analogActionHandle, EControllerActionOrigin *originsOut ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
    if (controllerHandle - 1 >= ControllerHandle_t(Global::shared_config.nb_controllers) ||
        actionSetHandle  - 1 > actionSets.size() ||
        analogActionHandle - 1 >= analogActions.size())
        return 0;
    std::string analogAction(analogActions[analogActionHandle - 1]);
    auto split = analogAction.find('_');
    if (split != std::string::npos)
        analogAction.resize(split);
    if (analogAction == "analogl")
        originsOut[0] = k_EControllerActionOrigin_XBox360_LeftStick_Move;
    else if (analogAction == "analogr")
        originsOut[0] = k_EControllerActionOrigin_XBox360_RightStick_Move;
    else
        return 0;
    return 1;
}

void ISteamController::StopAnalogActionMomentum( ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t eAction ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK | LCF_TODO);
}

void ISteamController::TriggerHapticPulse( ControllerHandle_t controllerHandle, ESteamControllerPad eTargetPad, unsigned short usDurationMicroSec ) {
    LOGTRACE(LCF_STEAM | LCF_JOYSTICK);
}

}
