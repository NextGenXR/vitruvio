// Copyright 2019 - 2020 Esri. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/Object.h"

#include "RuleAttributes.generated.h"

using FAttributeGroups = TArray<FString>;

enum FilesystemMode
{
	File,
	Directory,
	None
};
enum AnnotationType
{
	FileSystem,
	Range,
	Enum,
	Color
};

class AttributeAnnotation
{
public:
	virtual ~AttributeAnnotation() = default;
	virtual AnnotationType GetAnnotationType() = 0;
};

class ColorAnnotation : public AttributeAnnotation
{
	AnnotationType GetAnnotationType() override
	{
		return Color;
	}
};

class FilesystemAnnotation : public AttributeAnnotation
{
public:
	FilesystemMode Mode = None;
	FString Extensions;

	AnnotationType GetAnnotationType() override
	{
		return FileSystem;
	}
};

class RangeAnnotation : public AttributeAnnotation
{
public:
	TOptional<double> Min;
	TOptional<double> Max;
	double StepSize = 0.1;
	bool Restricted = true;

	AnnotationType GetAnnotationType() override
	{
		return Range;
	}
};

template <typename T> class EnumAnnotation : public AttributeAnnotation
{
public:
	TArray<T> Values;
	bool Restricted = true;

	AnnotationType GetAnnotationType() override
	{
		return Enum;
	}
};

UCLASS()
class VITRUVIO_API URuleAttribute : public UObject
{
	GENERATED_BODY()

protected:
	TSharedPtr<AttributeAnnotation> Annotation;

public:
	FString Name;
	FString DisplayName;

	FString Description;
	FAttributeGroups Groups;
	int Order;
	int GroupOrder;

	bool Hidden;

	void SetAnnotation(TSharedPtr<AttributeAnnotation> InAnnotation)
	{
		this->Annotation = MoveTemp(InAnnotation);
	}
};

UCLASS()
class VITRUVIO_API UStringAttribute : public URuleAttribute
{
	GENERATED_BODY()

public:
	FString Value;

	TSharedPtr<EnumAnnotation<FString>> GetEnumAnnotation() const
	{
		return Annotation && Annotation->GetAnnotationType() == Enum ? StaticCastSharedPtr<EnumAnnotation<FString>>(Annotation) : TSharedPtr<EnumAnnotation<FString>>();
	}

	TSharedPtr<ColorAnnotation> GetColorAnnotation() const
	{
		return Annotation && Annotation->GetAnnotationType() == Color ? StaticCastSharedPtr<ColorAnnotation>(Annotation) : TSharedPtr<ColorAnnotation>();
	}
};

UCLASS()
class VITRUVIO_API UFloatAttribute : public URuleAttribute
{
	GENERATED_BODY()

public:
	double Value;

	TSharedPtr<EnumAnnotation<double>> GetEnumAnnotation() const
	{
		return Annotation && Annotation->GetAnnotationType() == Enum ? StaticCastSharedPtr<EnumAnnotation<double>>(Annotation) : TSharedPtr<EnumAnnotation<double>>();
	}

	TSharedPtr<RangeAnnotation> GetRangeAnnotation() const
	{
		return Annotation && Annotation->GetAnnotationType() == Range ? StaticCastSharedPtr<RangeAnnotation>(Annotation) : TSharedPtr<RangeAnnotation>();
	}
};

UCLASS()
class VITRUVIO_API UBoolAttribute : public URuleAttribute
{
	GENERATED_BODY()

public:
	bool Value;
};
