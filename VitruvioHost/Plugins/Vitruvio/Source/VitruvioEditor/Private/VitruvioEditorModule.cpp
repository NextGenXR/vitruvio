// Copyright © 2017-2020 Esri R&D Center Zurich. All rights reserved.

#include "VitruvioEditorModule.h"

#include "RulePackageAssetTypeActions.h"
#include "VitruvioComponentDetails.h"

#include "AssetToolsModule.h"
#include "Core.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "VitruvioEditorModule"

#include "ChooseRulePackageDialog.h"
#include "Editor/LevelEditor/Public/LevelEditor.h"

namespace
{

bool IsViableVitruvioActor(AActor* Actor)
{
	for (const auto& InitialShapeClasses : UVitruvioComponent::GetInitialShapesClasses())
	{
		UInitialShape* DefaultInitialShape = Cast<UInitialShape>(InitialShapeClasses->GetDefaultObject());
		if (DefaultInitialShape && DefaultInitialShape->CanConstructFrom(Actor))
		{
			return true;
		}
	}
	return false;
}

TArray<AActor*> GetViableVitruvioActorsInHiararchy(AActor* Root)
{
	TArray<AActor*> ViableActors;
	if (IsViableVitruvioActor(Root))
	{
		ViableActors.Add(Root);
	}

	TArray<AActor*> ChildActors;
	Root->GetAttachedActors(ChildActors);

	// If the actor has a VitruvioComponent attached we do not further check its children.
	if (Root->FindComponentByClass<UVitruvioComponent>() == nullptr)
	{
		for (AActor* Child : ChildActors)
		{
			ViableActors.Append(GetViableVitruvioActorsInHiararchy(Child));
		}
	}

	return ViableActors;
}

TSharedRef<FExtender> ExtendLevelViewportContextMenuForVitruvioComponents(const TSharedRef<FUICommandList> CommandList,
																		  TArray<AActor*> SelectedActors)
{
	TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);

	Extender->AddMenuExtension(
		"ActorControl", EExtensionHook::After, CommandList, FMenuExtensionDelegate::CreateLambda([SelectedActors](FMenuBuilder& MenuBuilder) {
			MenuBuilder.BeginSection("CreateVitruvio", FText::FromString("Vitruvio"));

			FUIAction AddVitruvioComponentAction(FExecuteAction::CreateLambda([SelectedActors]() {
				TOptional<URulePackage*> SelectedRpk = FChooseRulePackageDialog::OpenDialog();

				if (SelectedRpk.IsSet())
				{
					URulePackage* Rpk = SelectedRpk.GetValue();

					for (AActor* Actor : SelectedActors)
					{
						if (!Actor->FindComponentByClass<UVitruvioComponent>())
						{
							UVitruvioComponent* Component = NewObject<UVitruvioComponent>(Actor, TEXT("VitruvioComponent"));
							Actor->AddInstanceComponent(Component);
							Component->OnComponentCreated();
							Component->RegisterComponent();

							Component->Rpk = Rpk;
							Component->Generate(true);
						}
					}
				}
			}));
			MenuBuilder.AddMenuEntry(FText::FromString("Add Vitruvio Component"),
									 FText::FromString("Adds Vitruvio Components to the selected Actors"), FSlateIcon(), AddVitruvioComponentAction);

			MenuBuilder.EndSection();
		}));

	Extender->AddMenuExtension(
		"SelectMatinee", EExtensionHook::After, CommandList, FMenuExtensionDelegate::CreateLambda([SelectedActors](FMenuBuilder& MenuBuilder) {
			MenuBuilder.BeginSection("SelectPossibleVitruvio", FText::FromString("Vitruvio"));

			FUIAction SelectAllViableVitruvioActors(FExecuteAction::CreateLambda([SelectedActors]() {
				GEditor->SelectNone(false, true, false);
				for (AActor* SelectedActor : SelectedActors)
				{
					TArray<AActor*> NewSelection = GetViableVitruvioActorsInHiararchy(SelectedActor);
					for (AActor* ActorToSelect : NewSelection)
					{
						GEditor->SelectActor(ActorToSelect, true, false);
					}
				}
				GEditor->NoteSelectionChange();
			}));
			MenuBuilder.AddMenuEntry(FText::FromString("Select All Viable Vitruvio Actors In Hiararchy"),
									 FText::FromString("Selects all Actors which are viable to attach VitruvioComponents to in hiararchy."),
									 FSlateIcon(), SelectAllViableVitruvioActors);

			MenuBuilder.EndSection();
		}));

	return Extender.ToSharedRef();
}

FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors LevelViewportContextMenuVitruvioExtender;

} // namespace

void VitruvioEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.RegisterAssetTypeActions(MakeShareable(new FRulePackageAssetTypeActions()));

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(UVitruvioComponent::StaticClass()->GetFName(),
											 FOnGetDetailCustomizationInstance::CreateStatic(&FVitruvioComponentDetails::MakeInstance));

	LevelViewportContextMenuVitruvioExtender =
		FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&ExtendLevelViewportContextMenuForVitruvioComponents);
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	auto& MenuExtenders = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	MenuExtenders.Add(LevelViewportContextMenuVitruvioExtender);
	LevelViewportContextMenuVitruvioExtenderDelegateHandle = MenuExtenders.Last().GetHandle();
}

void VitruvioEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout(UVitruvioComponent::StaticClass()->GetFName());

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetAllLevelViewportContextMenuExtenders().RemoveAll(
		[&](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& Delegate) {
			return Delegate.GetHandle() == LevelViewportContextMenuVitruvioExtenderDelegateHandle;
		});
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(VitruvioEditorModule, VitruvioEditor)
