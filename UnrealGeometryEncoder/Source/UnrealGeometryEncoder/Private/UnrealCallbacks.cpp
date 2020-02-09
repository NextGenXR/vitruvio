#pragma once

#include "UnrealCallbacks.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "StaticMeshOperations.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY(LogUnrealCallbacks);

namespace
{
	FPlane GetRow(const double* Mat, int32 Index)
	{
		return FPlane(Mat[Index + 0 * 4], Mat[Index + 1 * 4], Mat[Index + 2 * 4], Mat[Index + 3 * 4]);
	}

	UTexture2D* CreateTexture(UObject* Outer, const TArray<uint8>& PixelData, int32 InSizeX, int32 InSizeY, EPixelFormat InFormat, FName BaseName)
	{
		// Shamelessly copied from UTexture2D::CreateTransient with a few modifications
		if (InSizeX <= 0 || InSizeY <= 0 ||
			(InSizeX % GPixelFormats[InFormat].BlockSizeX) != 0 ||
			(InSizeY % GPixelFormats[InFormat].BlockSizeY) != 0)
		{
			UE_LOG(LogUnrealCallbacks, Warning, TEXT("Invalid parameters"));
			return nullptr;
		}

		// Most important difference with UTexture2D::CreateTransient: we provide the new texture with a name and an owner
		const FName TextureName = MakeUniqueObjectName(Outer, UTexture2D::StaticClass(), BaseName);
		UTexture2D* NewTexture = NewObject<UTexture2D>(Outer, TextureName, RF_Transient);

		NewTexture->PlatformData = new FTexturePlatformData();
		NewTexture->PlatformData->SizeX = InSizeX;
		NewTexture->PlatformData->SizeY = InSizeY;
		NewTexture->PlatformData->PixelFormat = InFormat;

		// Allocate first mipmap and upload the pixel data
		const int32 NumBlocksX = InSizeX / GPixelFormats[InFormat].BlockSizeX;
		const int32 NumBlocksY = InSizeY / GPixelFormats[InFormat].BlockSizeY;
		FTexture2DMipMap* Mip = new(NewTexture->PlatformData->Mips) FTexture2DMipMap();
		Mip->SizeX = InSizeX;
		Mip->SizeY = InSizeY;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		void* TextureData = Mip->BulkData.Realloc(NumBlocksX * NumBlocksY * GPixelFormats[InFormat].BlockBytes);
		FMemory::Memcpy(TextureData, PixelData.GetData(), PixelData.Num());
		Mip->BulkData.Unlock();

		NewTexture->UpdateResource();
		return NewTexture;
	}

	UTexture2D* LoadImageFromDisk(UObject* Outer, const FString& ImagePath)
	{
		static IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		
		if (!FPaths::FileExists(ImagePath))
		{
			UE_LOG(LogUnrealCallbacks, Error,  TEXT("File not found: %s"), *ImagePath);
			return nullptr;
		}

		TArray<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *ImagePath))
		{
			UE_LOG(LogUnrealCallbacks, Error, TEXT("Failed to load file: %s"), *ImagePath);
			return nullptr;
		}
		const EImageFormat ImageFormat = ImageWrapperModule.DetectImageFormat(FileData.GetData(), FileData.Num());
		if (ImageFormat == EImageFormat::Invalid)
		{
			UE_LOG(LogUnrealCallbacks, Error, TEXT("Unrecognized image file format: %s"), *ImagePath);
			return nullptr;
		}

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(ImageFormat);
		if (!ImageWrapper.IsValid())
		{
			UE_LOG(LogUnrealCallbacks, Error, TEXT("Failed to create image wrapper for file: %s"), *ImagePath);
			return nullptr;
		}

		// Decompress the image data
		const TArray<uint8>* RawData = nullptr;
		ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num());
		ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData);
		if (RawData == nullptr)
		{
			UE_LOG(LogUnrealCallbacks, Error, TEXT("Failed to decompress image file: %s"), *ImagePath);
			return nullptr;
		}

		// Create the texture and upload the uncompressed image data
		const FString TextureBaseName = TEXT("T_") + FPaths::GetBaseFilename(ImagePath);
		return CreateTexture(Outer, *RawData, ImageWrapper->GetWidth(), ImageWrapper->GetHeight(), EPixelFormat::PF_B8G8R8A8, FName(*TextureBaseName));
	}

	UTexture2D* GetTexture(UObject* Outer, const prt::AttributeMap* MaterialAttributes, wchar_t const* Key)
	{
		size_t ValuesCount = 0;
		wchar_t const* const* Values = MaterialAttributes->getStringArray(Key, &ValuesCount);
		for (int ValueIndex = 0; ValueIndex < ValuesCount; ++ValueIndex)
		{
			std::wstring TextureUri(Values[ValueIndex]);
			if (TextureUri.size() > 0)
			{
				return LoadImageFromDisk(Outer, FString(Values[ValueIndex]));
			}
		}
		return nullptr;
	}

	UMaterialInstanceDynamic* CreateMaterial(UObject* Outer, UMaterialInterface* Parent, const prt::AttributeMap* MaterialAttributes)
	{
		UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(Parent, Outer);

		// Convert AttributeMap to material
		size_t KeyCount = 0;
		wchar_t const* const* Keys = MaterialAttributes->getKeys(&KeyCount);
		for (int KeyIndex = 0; KeyIndex < KeyCount; KeyIndex++) 
		{
			wchar_t const* Key = Keys[KeyIndex];
			if (std::wstring(Key) == L"diffuseMap") MaterialInstance->SetTextureParameterValue(FName(Key), GetTexture(Outer, MaterialAttributes, Key));
			// TODO handle all keys
		}

		return MaterialInstance;
	}
}

void UnrealCallbacks::addMesh(const wchar_t* name, int32_t prototypeId, const double* vtx, size_t vtxSize, const double* nrm, size_t nrmSize, const uint32_t* faceVertexCounts,
							  size_t faceVertexCountsSize, const uint32_t* vertexIndices, size_t vertexIndicesSize, const uint32_t* normalIndices, size_t normalIndicesSize,

							  double const* const* uvs, size_t const* uvsSizes, uint32_t const* const* uvCounts, size_t const* uvCountsSizes, uint32_t const* const* uvIndices,
							  size_t const* uvIndicesSizes, size_t uvSets,

							  const uint32_t* faceRanges, size_t faceRangesSize,
							  const prt::AttributeMap** materials)
{
	UStaticMesh* Mesh = NewObject<UStaticMesh>();

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();

	const auto VertexUVs = Attributes.GetVertexInstanceUVs();
	Attributes.GetVertexInstanceUVs().SetNumIndices(uvSets);

	// Convert vertices and vertex instances
	const auto VertexPositions = Attributes.GetVertexPositions();
	for (size_t VertexIndex = 0; VertexIndex < vtxSize; VertexIndex += 3)
	{
		const FVertexID VertexID = Description.CreateVertex();
		VertexPositions[VertexID] = FVector(vtx[VertexIndex], vtx[VertexIndex + 2], vtx[VertexIndex + 1]) * 100;
	}
	
	// Create Polygons
	size_t BaseVertexIndex = 0;
	size_t PolygonGroupStartIndex = 0;
	for (size_t PolygonGroupIndex = 0; PolygonGroupIndex < faceRangesSize; ++PolygonGroupIndex)
	{
		const size_t PolygonFaceCount = faceRanges[PolygonGroupIndex];
		
		const FPolygonGroupID PolygonGroupId = Description.CreatePolygonGroup();

		// Create Material
		const prt::AttributeMap* MaterialAttributes = materials[0];
		UMaterialInstanceDynamic* MaterialInstance = CreateMaterial(Mesh, OpaqueParent, MaterialAttributes);
		const FName MaterialSlot = Mesh->AddMaterial(MaterialInstance);
		
		Attributes.GetPolygonGroupMaterialSlotNames()[PolygonGroupId] = MaterialSlot;

		// Create Geometry
		const auto Normals = Attributes.GetVertexInstanceNormals();
		for (size_t FaceIndex = 0; FaceIndex < PolygonFaceCount; ++FaceIndex)
		{
			const size_t FaceVertexCount = faceVertexCounts[PolygonGroupStartIndex + FaceIndex];

			TArray<FVertexInstanceID> PolygonVertexInstances;
			for (size_t FaceVertexIndex = 0; FaceVertexIndex < FaceVertexCount; ++FaceVertexIndex)
			{
				const uint32_t VertexIndex = vertexIndices[BaseVertexIndex + FaceVertexIndex];
				const uint32_t NormalIndex = normalIndices[BaseVertexIndex + FaceVertexIndex] * 3;
				FVertexInstanceID InstanceId = Description.CreateVertexInstance(FVertexID(VertexIndex));
				PolygonVertexInstances.Add(InstanceId);
				Normals[InstanceId] = FVector(nrm[NormalIndex], nrm[NormalIndex + 2], nrm[NormalIndex + 1]);

				for (size_t UVSet = 0; UVSet < uvSets; ++UVSet)
				{
					if (uvIndicesSizes[UVSet] > 0)
					{
						const uint32_t UVIndex = uvIndices[UVSet][BaseVertexIndex + FaceVertexIndex] * 2;
						VertexUVs.Set(InstanceId, 0, FVector2D(uvs[UVSet][UVIndex], -uvs[UVSet][UVIndex + 1]));
					}
				}
			}

			Description.CreatePolygon(PolygonGroupId, PolygonVertexInstances);
			BaseVertexIndex += FaceVertexCount;
		}
		
		PolygonGroupStartIndex += PolygonFaceCount;
	}


	// Build Mesh
	TArray<const FMeshDescription*> MeshDescriptionPtrs;
	MeshDescriptionPtrs.Emplace(&Description);
	Mesh->BuildFromMeshDescriptions(MeshDescriptionPtrs);

	PrototypeMap.Add(prototypeId, Mesh);
}

void UnrealCallbacks::addInstance(int32_t prototypeId, const double* transform)
{
	check(PrototypeMap.Contains(prototypeId))
	const FMatrix TransformationMat(GetRow(transform, 0), GetRow(transform, 1), GetRow(transform, 2), GetRow(transform, 3));
	const int32 SignumDet = FMath::Sign(TransformationMat.Determinant());

	FMatrix MatWithoutScale = TransformationMat.GetMatrixWithoutScale();
	MatWithoutScale = MatWithoutScale * SignumDet;
	MatWithoutScale.M[3][3] = 1;

	const FQuat CERotation = MatWithoutScale.ToQuat();
	const FVector CEScale = TransformationMat.GetScaleVector() * SignumDet;

	// convert from y-up (CE) to z-up (Unreal) (see https://stackoverflow.com/questions/16099979/can-i-switch-x-y-z-in-a-quaternion)
	const FQuat Rotation = FQuat(CERotation.X, CERotation.Z, CERotation.Y, CERotation.W);
	const FVector Scale = FVector(CEScale.X, CEScale.Y, CEScale.Z);

	// TODO this is actually the wrong indices to extract translation? are we setting up the transformation matrix correctly
	const FVector Translation = FVector(TransformationMat.M[0][3], TransformationMat.M[2][3], TransformationMat.M[1][3]) * 100;

	const FTransform Transform(Rotation, Translation, Scale);
	
	UStaticMesh* PrototypeMesh = PrototypeMap[prototypeId];
	Instances.FindOrAdd(PrototypeMesh).Add(Transform);
}

prt::Status UnrealCallbacks::attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value)
{
	AttributeMapBuilder->setBool(key, value);
	return prt::STATUS_OK;
}

prt::Status UnrealCallbacks::attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value)
{
	AttributeMapBuilder->setFloat(key, value);
	return prt::STATUS_OK;
}

prt::Status UnrealCallbacks::attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value)
{
	AttributeMapBuilder->setString(key, value);
	return prt::STATUS_OK;
}

prt::Status UnrealCallbacks::attrBoolArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const bool* values, size_t size)
{
	AttributeMapBuilder->setBoolArray(key, values, size);
	return prt::STATUS_OK;
}

prt::Status UnrealCallbacks::attrFloatArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const double* values, size_t size)
{
	AttributeMapBuilder->setFloatArray(key, values, size);
	return prt::STATUS_OK;
}

prt::Status UnrealCallbacks::attrStringArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* const* values, size_t size)
{
	AttributeMapBuilder->setStringArray(key, values, size);
	return prt::STATUS_OK;
}
