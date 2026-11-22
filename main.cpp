#include <iostream>
#include <thread>
#include <chrono>
#include <string>

using namespace std;

void Print(string Message)
{
    cout << Message << endl;
}

class Game
{
 public:

    void GameLoop()
    {
        StartTime = chrono::steady_clock::now();

        while(bIsActive)
        {
            Update();

            //this_thread::sleep_until( chrono::steady_clock::now() + FrameTime());
            
            if(chrono::steady_clock::now() - LastFrame >= (1000ms / FPS))
            {       
                FixedUpdate();
            }

            if(chrono::steady_clock::now() > StartTime + 5000ms)
            {
                EndGame();
            }
        }
    }

    void EndGame()
    {
        bIsActive = false;
        Print("Goodbye world!");
        printf("Update Frames: %lu\nFixed Frames: %lu\n\n", UpdateFrames, FixedFrames);
    }
    
    chrono::steady_clock::time_point StartTime;
    chrono::steady_clock::time_point LastFrame;
    chrono::milliseconds DeltaTime;

    void Update()
    {
        UpdateFrames++;
    }

    void FixedUpdate()
    {
        LastFrame = chrono::steady_clock::now(); 
        FixedFrames++;
    }

 private:

    bool bIsActive = true;
    uint8_t FPS = 30;

    // DEBUG
    uint64_t UpdateFrames = 0;
    uint64_t FixedFrames = 0;

    chrono::milliseconds FrameTime()
    {
        return chrono::milliseconds(1000 / FPS);
    }
};

int main()
{
    Print("Hello world!");
    Game* CurGame = new Game();
    CurGame->GameLoop();

    return 0;
}

