#pragma once

namespace Core
{

class Time
{
public:
    static void Init();
    static void Update();

    static float DeltaTime();
    static float ElapsedTime();

private:
    static float s_DeltaTime;
    static float s_ElapsedTime;
    static float s_LastTime;
};

} // namespace Core
