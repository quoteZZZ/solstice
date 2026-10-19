// Solstice - Core layer: the reality anchor.
//
// The first selling point is "the bridge to reality" (docs/WORLD.md SS1 / SS4): the
// system clock drives the season and the solar path, and a free weather API drives the sky.
// Network failure must never block play - three consecutive failures degrade to virtual
// weather and the UI says so (docs/PLAN.md SS2 item 6).

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Sim/SolsticeKernel.h"

namespace Solstice
{

/** Built-in city table; lat/long is all the simulation needs (docs/WORLD.md SS4.2). */
struct FRealityCity
{
	const TCHAR* Key;
	const TCHAR* NameZh;
	const TCHAR* NameEn;
	float Latitude;
	float Longitude;
};

class FRealityAnchor
{
public:
	FRealityAnchor();
	~FRealityAnchor();

	void Initialize(const FString& CityKey);
	void Tick(double RealDeltaSeconds, bool bAllowNetwork);

	/** The sample the world clock and the sky read from. */
	FRealitySample GetSample() const { return Sample; }

	void SetLatitude(float InLatitude) { Latitude = InLatitude; }
	float GetLatitude() const { return Latitude; }
	FString GetCityName() const;

	bool IsWeatherFromNetwork() const { return bWeatherFromNetwork; }
	bool IsOffline() const { return FailureCount >= 3; }
	FString GetStatusZh() const;
	FString GetStatusEn() const;
	FString GetLastError() const { return LastError; }

	static const TArray<FRealityCity>& GetCities();

private:
	void RefreshClock();
	void BeginWeatherRequest();
	void OnWeatherResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedOk);

	FRealitySample Sample;
	FString CityKey = TEXT("shanghai");
	float Latitude = 31.23f;
	float Longitude = 121.47f;
	double SecondsSinceWeatherFetch = 100000.0;
	int32 FailureCount = 0;
	bool bRequestInFlight = false;
	bool bWeatherFromNetwork = false;
	FString LastError;
};

} // namespace Solstice
