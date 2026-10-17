// Solstice - Core layer: JSON -> Sim structs.

#include "Core/SolsticeDataLoader.h"

#include "Core/SolsticeLocalization.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace Solstice
{
	namespace
	{
		EElement ParseElement(const FString& In)
		{
			if (In.Equals(TEXT("metal"), ESearchCase::IgnoreCase)) { return EElement::Metal; }
			if (In.Equals(TEXT("wood"), ESearchCase::IgnoreCase)) { return EElement::Wood; }
			if (In.Equals(TEXT("water"), ESearchCase::IgnoreCase)) { return EElement::Water; }
			if (In.Equals(TEXT("fire"), ESearchCase::IgnoreCase)) { return EElement::Fire; }
			if (In.Equals(TEXT("earth"), ESearchCase::IgnoreCase)) { return EElement::Earth; }
			if (In.Equals(TEXT("light"), ESearchCase::IgnoreCase)) { return EElement::Light; }
			if (In.Equals(TEXT("dark"), ESearchCase::IgnoreCase)) { return EElement::Dark; }
			return EElement::None;
		}

		uint8 ParseCardClass(const FString& In)
		{
			if (In.Equals(TEXT("rite"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ECardClass::Rite); }
			if (In.Equals(TEXT("law"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ECardClass::Law); }
			return static_cast<uint8>(ECardClass::Matter);
		}

		uint8 ParseDuration(const FString& In)
		{
			if (In.Equals(TEXT("sustain"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EDuration::Sustain); }
			if (In.Equals(TEXT("permanent"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EDuration::Permanent); }
			if (In.Equals(TEXT("trigger"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EDuration::Trigger); }
			return static_cast<uint8>(EDuration::Instant);
		}

		uint8 ParseDirection(const FString& In)
		{
			if (In.Equals(TEXT("remove"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EDirection::Remove); }
			if (In.Equals(TEXT("transform"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EDirection::Transform); }
			return static_cast<uint8>(EDirection::Create);
		}

		uint8 ParseScope(const FString& In)
		{
			if (In.Equals(TEXT("global"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EScope::Global); }
			if (In.Equals(TEXT("area"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EScope::Area); }
			if (In.Equals(TEXT("brush"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EScope::Brush); }
			return static_cast<uint8>(EScope::Single);
		}

		uint8 ParseEffect(const FString& In)
		{
			if (In.Equals(TEXT("raise"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::RaiseTerrain); }
			if (In.Equals(TEXT("lower"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::LowerTerrain); }
			if (In.Equals(TEXT("flatten"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::FlattenTerrain); }
			if (In.Equals(TEXT("ridge"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::RidgeLine); }
			if (In.Equals(TEXT("paint"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::PaintSurface); }
			if (In.Equals(TEXT("ignite"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::Ignite); }
			if (In.Equals(TEXT("heat"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::HeatArea); }
			if (In.Equals(TEXT("chill"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::ChillArea); }
			if (In.Equals(TEXT("water"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::WaterLevel); }
			if (In.Equals(TEXT("ore"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::OreVein); }
			if (In.Equals(TEXT("seed-life"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::SeedLife); }
			if (In.Equals(TEXT("record"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::RecordPillar); }
			if (In.Equals(TEXT("erode"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::ErodeSurface); }
			if (In.Equals(TEXT("weather"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(EEffect::WeatherPush); }
			return static_cast<uint8>(EEffect::None);
		}

		uint8 ParseSpeciesKind(const FString& In)
		{
			if (In.Equals(TEXT("moss-grass"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ESpeciesKind::MossGrass); }
			if (In.Equals(TEXT("shrub"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ESpeciesKind::Shrub); }
			if (In.Equals(TEXT("grazer"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ESpeciesKind::Grazer); }
			if (In.Equals(TEXT("raptor"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ESpeciesKind::Raptor); }
			if (In.Equals(TEXT("stream-fish"), ESearchCase::IgnoreCase)) { return static_cast<uint8>(ESpeciesKind::StreamFish); }
			return static_cast<uint8>(ESpeciesKind::MossGrass);
		}

		int32 ParseTier(const TSharedPtr<FJsonValue>& Value)
		{
			if (!Value.IsValid())
			{
				return 0;
			}
			FString AsString;
			if (Value->TryGetString(AsString))
			{
				if (AsString.Equals(TEXT("over"), ESearchCase::IgnoreCase)) { return 10; }
				if (AsString.Equals(TEXT("ban"), ESearchCase::IgnoreCase)) { return 11; }
				return FCString::Atoi(*AsString);
			}
			double AsNumber = 0.0;
			if (Value->TryGetNumber(AsNumber))
			{
				return FMath::RoundToInt(AsNumber);
			}
			return 0;
		}

		EMaterial ParseMaterial(const FString& In)
		{
			if (In.Equals(TEXT("soil"), ESearchCase::IgnoreCase)) { return EMaterial::Soil; }
			if (In.Equals(TEXT("clay"), ESearchCase::IgnoreCase)) { return EMaterial::Clay; }
			if (In.Equals(TEXT("mud"), ESearchCase::IgnoreCase)) { return EMaterial::Mud; }
			if (In.Equals(TEXT("stone"), ESearchCase::IgnoreCase)) { return EMaterial::Stone; }
			if (In.Equals(TEXT("ore"), ESearchCase::IgnoreCase)) { return EMaterial::Ore; }
			if (In.Equals(TEXT("deadwood"), ESearchCase::IgnoreCase)) { return EMaterial::DeadWood; }
			if (In.Equals(TEXT("plant"), ESearchCase::IgnoreCase)) { return EMaterial::LivePlant; }
			if (In.Equals(TEXT("water"), ESearchCase::IgnoreCase)) { return EMaterial::Water; }
			if (In.Equals(TEXT("magma"), ESearchCase::IgnoreCase)) { return EMaterial::Magma; }
			if (In.Equals(TEXT("ice"), ESearchCase::IgnoreCase)) { return EMaterial::Ice; }
			if (In.Equals(TEXT("snow"), ESearchCase::IgnoreCase)) { return EMaterial::Snow; }
			if (In.Equals(TEXT("sand"), ESearchCase::IgnoreCase)) { return EMaterial::Sand; }
			if (In.Equals(TEXT("ash"), ESearchCase::IgnoreCase)) { return EMaterial::Ash; }
			if (In.Equals(TEXT("tissue"), ESearchCase::IgnoreCase)) { return EMaterial::Tissue; }
			return EMaterial::Bare;
		}

		int32 ParseAlgo(const FString& In)
		{
			if (In.Equals(TEXT("islands"), ESearchCase::IgnoreCase)) { return 1; }
			if (In.Equals(TEXT("canyon"), ESearchCase::IgnoreCase)) { return 2; }
			if (In.Equals(TEXT("plateau"), ESearchCase::IgnoreCase)) { return 3; }
			if (In.Equals(TEXT("basin"), ESearchCase::IgnoreCase)) { return 4; }
			if (In.Equals(TEXT("archipelago"), ESearchCase::IgnoreCase)) { return 5; }
			return 0;
		}

		FString ReadBi(const TSharedPtr<FJsonObject>& Object, const FString& Field, bool bChinese)
		{
			const TSharedPtr<FJsonObject>* Sub = nullptr;
			if (Object->TryGetObjectField(Field, Sub) && Sub && Sub->IsValid())
			{
				FString Value;
				if ((*Sub)->TryGetStringField(bChinese ? TEXT("zh") : TEXT("en"), Value))
				{
					return Value;
				}
				if ((*Sub)->TryGetStringField(bChinese ? TEXT("en") : TEXT("zh"), Value))
				{
					return Value;
				}
			}
			FString Flat;
			if (Object->TryGetStringField(Field, Flat))
			{
				return Flat;
			}
			return FString();
		}

		float ReadNumber(const TSharedPtr<FJsonObject>& Object, const FString& Field, float Fallback)
		{
			double Value = 0.0;
			if (Object->TryGetNumberField(Field, Value))
			{
				return static_cast<float>(Value);
			}
			return Fallback;
		}

		bool ReadBool(const TSharedPtr<FJsonObject>& Object, const FString& Field, bool Fallback)
		{
			bool Value = false;
			if (Object->TryGetBoolField(Field, Value))
			{
				return Value;
			}
			return Fallback;
		}

		FString ReadString(const TSharedPtr<FJsonObject>& Object, const FString& Field, const FString& Fallback = FString())
		{
			FString Value;
			if (Object->TryGetStringField(Field, Value))
			{
				return Value;
			}
			return Fallback;
		}

		void ReadParams(const TSharedPtr<FJsonObject>& Object, TMap<FString, float>& Out)
		{
			const TSharedPtr<FJsonObject>* ParamsObject = nullptr;
			if (!Object->TryGetObjectField(TEXT("params"), ParamsObject) || !ParamsObject || !ParamsObject->IsValid())
			{
				return;
			}
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ParamsObject)->Values)
			{
				if (!Pair.Value.IsValid())
				{
					continue;
				}
				// Material and species parameters arrive as names; translate them to indices so
				// the kernel stays free of string parsing.
				if (Pair.Key.Equals(TEXT("material"), ESearchCase::IgnoreCase))
				{
					FString MaterialName;
					if (Pair.Value->TryGetString(MaterialName))
					{
						Out.Add(TEXT("mat"), static_cast<float>(static_cast<int32>(ParseMaterial(MaterialName))));
					}
					continue;
				}
				if (Pair.Key.Equals(TEXT("species"), ESearchCase::IgnoreCase))
				{
					FString SpeciesName;
					if (Pair.Value->TryGetString(SpeciesName))
					{
						Out.Add(TEXT("species"), static_cast<float>(static_cast<int32>(ParseSpeciesKind(SpeciesName))));
					}
					continue;
				}
				if (Pair.Key.Equals(TEXT("weather"), ESearchCase::IgnoreCase))
				{
					FString WeatherName;
					if (Pair.Value->TryGetString(WeatherName))
					{
						int32 WeatherIndex = -1;
						for (int32 Index = 0; Index < static_cast<int32>(EWeather::Count); ++Index)
						{
							if (WeatherName.Equals(WeatherGlyph(static_cast<EWeather>(Index))))
							{
								WeatherIndex = Index;
								break;
							}
						}
						if (WeatherName.Equals(TEXT("clear"), ESearchCase::IgnoreCase)) { WeatherIndex = 0; }
						if (WeatherName.Equals(TEXT("cloudy"), ESearchCase::IgnoreCase)) { WeatherIndex = 1; }
						if (WeatherName.Equals(TEXT("rain"), ESearchCase::IgnoreCase)) { WeatherIndex = 2; }
						if (WeatherName.Equals(TEXT("storm"), ESearchCase::IgnoreCase)) { WeatherIndex = 3; }
						if (WeatherName.Equals(TEXT("fog"), ESearchCase::IgnoreCase)) { WeatherIndex = 4; }
						Out.Add(TEXT("weather"), static_cast<float>(WeatherIndex));
					}
					continue;
				}

				double AsNumber = 0.0;
				if (Pair.Value->TryGetNumber(AsNumber))
				{
					Out.Add(Pair.Key, static_cast<float>(AsNumber));
				}
				else
				{
					FString AsString;
					if (Pair.Value->TryGetString(AsString))
					{
						Out.Add(Pair.Key, FCString::Atof(*AsString));
					}
				}
			}
		}

		bool ParseCardObject(const TSharedPtr<FJsonObject>& Object, FCardDef& Out, bool bChinese)
		{
			const FString Id = ReadString(Object, TEXT("id"));
			if (Id.IsEmpty())
			{
				return false;
			}
			Out = FCardDef();
			Out.Id = FName(*Id);
			Out.NameZh = ReadBi(Object, TEXT("name"), true);
			Out.NameEn = ReadBi(Object, TEXT("name"), false);
			Out.DescZh = ReadBi(Object, TEXT("desc"), true);
			Out.DescEn = ReadBi(Object, TEXT("desc"), false);
			Out.Element = static_cast<uint8>(ParseElement(ReadString(Object, TEXT("element"))));
			const FString Aux = ReadString(Object, TEXT("element_aux"));
			Out.ElementAux = Aux.IsEmpty() ? static_cast<uint8>(EElement::None) : static_cast<uint8>(ParseElement(Aux));
			Out.Tier = static_cast<uint8>(FMath::Clamp(ParseTier(Object->TryGetField(TEXT("tier"))), 0, 11));
			Out.CardClass = ParseCardClass(ReadString(Object, TEXT("class")));
			Out.Duration = ParseDuration(ReadString(Object, TEXT("duration")));
			Out.Direction = ParseDirection(ReadString(Object, TEXT("direction")));
			Out.Scope = ParseScope(ReadString(Object, TEXT("scope")));
			Out.Effect = ParseEffect(ReadString(Object, TEXT("effect")));
			Out.bProvisional = ReadBool(Object, TEXT("provisional"), false);
			Out.Value = Tuning::CardValue[FMath::Clamp(static_cast<int32>(Out.Tier), 0, 11)];
			ReadParams(Object, Out.Params);
			(void)bChinese;
			return true;
		}

		bool ParseSpeciesObject(const TSharedPtr<FJsonObject>& Object, FSpeciesDef& Out)
		{
			const FString Id = ReadString(Object, TEXT("id"));
			if (Id.IsEmpty())
			{
				return false;
			}
			Out = FSpeciesDef();
			Out.Id = FName(*Id);
			Out.NameZh = ReadBi(Object, TEXT("name"), true);
			Out.NameEn = ReadBi(Object, TEXT("name"), false);
			Out.Kind = ParseSpeciesKind(ReadString(Object, TEXT("kind")));
			Out.Role = static_cast<uint8>(FMath::RoundToInt(ReadNumber(Object, TEXT("role"), 1.0f)));
			Out.HeightM = ReadNumber(Object, TEXT("height_m"), 0.5f);
			Out.LengthM = ReadNumber(Object, TEXT("length_m"), 0.8f);
			Out.LifespanDays = ReadNumber(Object, TEXT("lifespan_days"), 90.0f);
			Out.MaturityDays = ReadNumber(Object, TEXT("maturity_days"), 5.0f);
			Out.EnergyMax = ReadNumber(Object, TEXT("energy_max"), 100.0f);
			Out.EnergyDrainPerHour = ReadNumber(Object, TEXT("energy_drain_per_hour"), 1.0f);
			Out.EatVegetationPerHour = ReadNumber(Object, TEXT("eat_vegetation_per_hour"), 0.03f);
			Out.BreedEnergyFraction = ReadNumber(Object, TEXT("breed_energy_fraction"), 0.7f);
			Out.BreedVegetation = ReadNumber(Object, TEXT("breed_vegetation"), 0.5f);
			Out.BreedCostEnergy = ReadNumber(Object, TEXT("breed_cost_energy"), 40.0f);
			Out.MoveCellsPerHour = ReadNumber(Object, TEXT("move_cells_per_hour"), 2.0f);
			Out.TempMinC = ReadNumber(Object, TEXT("temp_min_c"), 10.0f);
			Out.TempMaxC = ReadNumber(Object, TEXT("temp_max_c"), 28.0f);
			Out.WaterReachCells = ReadNumber(Object, TEXT("water_reach_cells"), 8.0f);
			Out.PredationSuccess = ReadNumber(Object, TEXT("predation_success"), 0.3f);
			Out.StarvationDays = ReadNumber(Object, TEXT("starvation_days"), 4.0f);
			Out.GrowthPerHour = ReadNumber(Object, TEXT("growth_per_hour"), 0.02f);
			Out.FertilityCostPerHour = ReadNumber(Object, TEXT("fertility_cost_per_hour"), 0.01f);
			Out.CarryCapacityPerTile = FMath::RoundToInt(ReadNumber(Object, TEXT("carry_capacity_per_tile"), 1.0f));
			Out.bAquatic = ReadBool(Object, TEXT("aquatic"), false);
			Out.bFlying = ReadBool(Object, TEXT("flying"), false);
			Out.bCover = ReadBool(Object, TEXT("cover"), false);
			return true;
		}

		bool ParseWorldObject(const TSharedPtr<FJsonObject>& Object, FWorldConfig& Out, const FWorldConfig& Fallback)
		{
			Out = Fallback;
			if (!Object.IsValid())
			{
				return false;
			}
			Out.Size = FMath::RoundToInt(ReadNumber(Object, TEXT("size"), static_cast<float>(Fallback.Size)));
			Out.ChunkSize = FMath::RoundToInt(ReadNumber(Object, TEXT("chunk_size"), static_cast<float>(Fallback.ChunkSize)));
			Out.Algo = ParseAlgo(ReadString(Object, TEXT("algo"), TEXT("ridges")));
			Out.Seed = static_cast<uint32>(FMath::RoundToInt(ReadNumber(Object, TEXT("seed"), static_cast<float>(Fallback.Seed))));
			Out.BaseWaterLevel = ReadNumber(Object, TEXT("base_water_level"), Fallback.BaseWaterLevel);
			Out.Latitude = ReadNumber(Object, TEXT("latitude"), Fallback.Latitude);
			Out.TidalAmplitude = ReadNumber(Object, TEXT("tidal_amplitude"), Fallback.TidalAmplitude);
			Out.bTides = ReadBool(Object, TEXT("tides"), Fallback.bTides);
			Out.bChaosStart = ReadBool(Object, TEXT("chaos_start"), Fallback.bChaosStart);
			Out.bWithLifeSeed = ReadBool(Object, TEXT("life_seed"), Fallback.bWithLifeSeed);
			Out.RealmId = ReadString(Object, TEXT("realm_id"), Fallback.RealmId);

			FString RealmNameZh = ReadBi(Object, TEXT("realm_name"), true);
			FString RealmNameEn = ReadBi(Object, TEXT("realm_name"), false);
			if (!RealmNameZh.IsEmpty())
			{
				Out.RealmNameZh = RealmNameZh;
			}
			if (!RealmNameEn.IsEmpty())
			{
				Out.RealmNameEn = RealmNameEn;
			}
			return true;
		}
	}

	FString FDataLoader::GetDataDirectory()
	{
		return FPaths::ProjectConfigDir() / TEXT("Solstice/Data");
	}

	FString FDataLoader::GetLocalizationDirectory()
	{
		return FPaths::ProjectConfigDir() / TEXT("Solstice/Localization");
	}

	bool FDataLoader::LoadFileToString(const FString& RelativePath, FString& OutText)
	{
		const FString FullPath = FPaths::ProjectConfigDir() / RelativePath;
		if (!FPaths::FileExists(FullPath))
		{
			return false;
		}
		return FFileHelper::LoadFileToString(OutText, *FullPath);
	}

	bool FDataLoader::LoadAll(FLoadedContent& Out)
	{
		Out = FLoadedContent();
		const FString DataDir = GetDataDirectory();

		// ---- localisation -------------------------------------------------
		{
			const FString LocDir = GetLocalizationDirectory();
			const TCHAR* Languages[2] = { TEXT("zh-Hans"), TEXT("en") };
			for (const TCHAR* Language : Languages)
			{
				const FString Path = LocDir / FString::Printf(TEXT("%s.json"), Language);
				FString Text;
				if (!FFileHelper::LoadFileToString(Text, *Path))
				{
					Out.Warnings.Add(FString::Printf(TEXT("localization missing: %s"), *Path));
					continue;
				}
				TSharedPtr<FJsonObject> Root;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
				if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
				{
					Out.Warnings.Add(FString::Printf(TEXT("localization parse failed: %s"), *Path));
					continue;
				}
				TMap<FString, FString> Flat;
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
				{
					FString Value;
					if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
					{
						Flat.Add(Pair.Key, Value);
					}
				}
				FLocalization::Get().AddFlat(Language, Flat);
			}
		}

		// ---- species ------------------------------------------------------
		{
			const FString Path = DataDir / TEXT("species.json");
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *Path))
			{
				TSharedPtr<FJsonObject> Root;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
				if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
					if (Root->TryGetArrayField(TEXT("species"), Items) && Items)
					{
						for (const TSharedPtr<FJsonValue>& Item : *Items)
						{
							const TSharedPtr<FJsonObject>* Object = nullptr;
							if (Item.IsValid() && Item->TryGetObject(Object) && Object && Object->IsValid())
							{
								FSpeciesDef Def;
								if (ParseSpeciesObject(*Object, Def))
								{
									Out.Catalog.Species.Add(Def);
								}
							}
						}
					}
				}
			}
			else
			{
				Out.Warnings.Add(FString::Printf(TEXT("species.json not found at %s"), *Path));
			}
		}

		// ---- cards: every cards.*.json next to the species file ----------
		{
			TArray<FString> Found;
			IFileManager::Get().FindFiles(Found, *(DataDir / TEXT("cards.*.json")), true, false);
			Found.Sort();
			for (const FString& FileName : Found)
			{
				const FString Path = DataDir / FileName;
				FString Text;
				if (!FFileHelper::LoadFileToString(Text, *Path))
				{
					continue;
				}
				TSharedPtr<FJsonObject> Root;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
				if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
				{
					Out.Warnings.Add(FString::Printf(TEXT("card file parse failed: %s"), *FileName));
					continue;
				}
				const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
				if (!Root->TryGetArrayField(TEXT("cards"), Items) || !Items)
				{
					Out.Warnings.Add(FString::Printf(TEXT("card file has no \"cards\" array: %s"), *FileName));
					continue;
				}
				int32 Added = 0;
				for (const TSharedPtr<FJsonValue>& Item : *Items)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (Item.IsValid() && Item->TryGetObject(Object) && Object && Object->IsValid())
					{
						FCardDef Def;
						if (ParseCardObject(*Object, Def, true))
						{
							Out.Catalog.Cards.Add(Def);
							++Added;
						}
					}
				}
				Out.CardFiles.Add(FString::Printf(TEXT("%s (%d)"), *FileName, Added));
			}
			if (Found.Num() == 0)
			{
				Out.Warnings.Add(FString::Printf(TEXT("no cards.*.json found in %s"), *DataDir));
			}
		}

		// ---- world presets -----------------------------------------------
		Out.DefaultWorld.RealmId = FString::Printf(TEXT("realm.valley-%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d")));
		{
			const FString Path = DataDir / TEXT("world.json");
			FString Text;
			if (FFileHelper::LoadFileToString(Text, *Path))
			{
				TSharedPtr<FJsonObject> Root;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
				if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
				{
					const TSharedPtr<FJsonObject>* DefaultObject = nullptr;
					if (Root->TryGetObjectField(TEXT("default"), DefaultObject) && DefaultObject && DefaultObject->IsValid())
					{
						ParseWorldObject(*DefaultObject, Out.DefaultWorld, Out.DefaultWorld);
					}
					const TArray<TSharedPtr<FJsonValue>>* Presets = nullptr;
					if (Root->TryGetArrayField(TEXT("presets"), Presets) && Presets)
					{
						for (const TSharedPtr<FJsonValue>& Item : *Presets)
						{
							const TSharedPtr<FJsonObject>* Object = nullptr;
							if (Item.IsValid() && Item->TryGetObject(Object) && Object && Object->IsValid())
							{
								FWorldConfig Preset;
								if (ParseWorldObject(*Object, Preset, Out.DefaultWorld))
								{
									Out.Catalog.Presets.Add(Preset);
								}
							}
						}
					}
				}
				else
				{
					Out.Warnings.Add(TEXT("world.json parse failed"));
				}
			}
			else
			{
				Out.Warnings.Add(FString::Printf(TEXT("world.json not found at %s"), *Path));
			}
		}

		Out.bLoaded = (Out.Catalog.Cards.Num() > 0) && (Out.Catalog.Species.Num() > 0);
		return Out.bLoaded;
	}
}
