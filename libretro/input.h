
#pragma once

namespace Input
{
void Init();
void Update();
void Shutdown();
void RumbleEnabled(bool enabled, int percent);
void setRumbleLevel(int percent);
}
