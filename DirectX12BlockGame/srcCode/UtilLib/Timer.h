#pragma once

#include <windows.h>

class Timer
{
public:
	Timer()
	{
		__int64 countsPerSecond;
		QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSecond);
		secPerCount = 1.0 / (double)countsPerSecond;
	}

	float TotalTime() 
	{
		if (isStop)
			return (float)(((stopTime - pauseTime) - baseTime) * secPerCount);
		else
			return (float)(((currTime - pauseTime) - baseTime) * secPerCount);
	}

	float DeltaTime() 
	{
		return (float)deltaTIme;
	}

	void Reset() 
	{
		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);

		baseTime = now;
		currTime = now;
		stopTime = 0;
		isStop = false;
	}

	void Start() 
	{
		if (!isStop)
			return;

		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);

		pauseTime += now - stopTime;
		currTime = now;
		stopTime = 0;
		isStop = false;
	}

	void Stop() 
	{
		if (isStop)
			return;

		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);

		stopTime = now;
		isStop = true;
	}

	void Tick() 
	{
		if (isStop)
		{
			deltaTIme = 0.0;
			return;
		}

		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
		deltaTIme = (now - currTime)*secPerCount;

		currTime = now;

		deltaTIme = deltaTIme > 0.0 ? deltaTIme : 0.0;

	}

	

private:

	double secPerCount;
	double deltaTIme = -1.0;

	__int64 baseTime = 0;
	__int64 pauseTime = 0;
	__int64 stopTime = 0;
	__int64 currTime = 0;

	bool isStop = false;



};