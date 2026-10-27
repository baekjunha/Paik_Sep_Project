#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Json.h"
#include "K2Node_CallFunction.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintFlowAnalyzer, Log, All);

namespace BPFA
{
    using FObjectJson = TSharedPtr<FJsonObject>;
    using FObjectArray = TArray<TSharedPtr<FJsonValue>>;

    static FObjectJson NewObject() { return MakeShared<FJsonObject>(); }
    static TSharedPtr<FJsonValue> ObjectValue(const FObjectJson& Value)
    {
        return MakeShared<FJsonValueObject>(Value);
    }

    static FString GuidString(const FGuid& Guid)
    {
        return Guid.ToString(EGuidFormats::DigitsWithHyphens);
    }

    static bool WriteJson(const FString& FilePath, const FObjectJson& Root)
    {
        FString Text;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
        if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
        {
            UE_LOG(LogBlueprintFlowAnalyzer, Error, TEXT("JSON serialization failed: %s"), *FilePath);
            return false;
        }
        if (!FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            UE_LOG(LogBlueprintFlowAnalyzer, Error, TEXT("JSON file write failed: %s"), *FilePath);
            return false;
        }
        return true;
    }

    static FObjectJson PinToJson(const UEdGraphPin* Pin)
    {
        const FObjectJson Result = NewObject();
        Result->SetStringField(TEXT("id"), GuidString(Pin->PinId));
        Result->SetStringField(TEXT("name"), Pin->PinName.ToString());
        Result->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        Result->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        Result->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
        Result->SetBoolField(TEXT("is_exec"), Pin->PinType.PinCategory == FName(TEXT("exec")));
        Result->SetStringField(TEXT("default_value"), Pin->DefaultValue);
        Result->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
        Result->SetStringField(TEXT("default_object"), IsValid(Pin->DefaultObject.Get()) ? Pin->DefaultObject->GetPathName() : FString());
        Result->SetBoolField(TEXT("orphaned"), Pin->bOrphanedPin);
        if (Pin->ParentPin)
        {
            Result->SetStringField(TEXT("parent_pin_id"), GuidString(Pin->ParentPin->PinId));
        }
        FObjectArray Links;
        for (const UEdGraphPin* Target : Pin->LinkedTo)
        {
            if (!Target) { continue; }
            const UEdGraphNode* Owner = Target->GetOwningNode();
            FObjectJson Link = NewObject();
            Link->SetStringField(TEXT("node_id"), Owner ? GuidString(Owner->NodeGuid) : TEXT(""));
            Link->SetStringField(TEXT("pin_id"), GuidString(Target->PinId));
            Link->SetStringField(TEXT("pin_name"), Target->PinName.ToString());
            Link->SetStringField(TEXT("node_name"), Owner ? Owner->GetName() : TEXT(""));
            Links.Add(ObjectValue(Link));
        }
        Result->SetArrayField(TEXT("links"), Links);
        return Result;
    }

    static FObjectJson NodeToJson(const UEdGraphNode* Node, int32& InOutPinCount, int32& InOutLinkCount)
    {
        const FObjectJson Result = NewObject();
        Result->SetStringField(TEXT("id"), GuidString(Node->NodeGuid));
        Result->SetStringField(TEXT("name"), Node->GetName());
        Result->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
        Result->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        Result->SetStringField(TEXT("comment"), Node->NodeComment);
        Result->SetNumberField(TEXT("x"), Node->NodePosX);
        Result->SetNumberField(TEXT("y"), Node->NodePosY);

        if (const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
        {
            FObjectJson CallInfo = NewObject();
            CallInfo->SetStringField(TEXT("function_name"), Call->GetFunctionName().ToString());
            if (const UFunction* Function = Call->GetTargetFunction())
            {
                CallInfo->SetStringField(TEXT("target"), Function->GetPathName());
                CallInfo->SetBoolField(TEXT("net"), Function->HasAnyFunctionFlags(FUNC_Net));
                CallInfo->SetBoolField(TEXT("server_rpc"), Function->HasAnyFunctionFlags(FUNC_NetServer));
                CallInfo->SetBoolField(TEXT("client_rpc"), Function->HasAnyFunctionFlags(FUNC_NetClient));
                CallInfo->SetBoolField(TEXT("multicast_rpc"), Function->HasAnyFunctionFlags(FUNC_NetMulticast));
                CallInfo->SetBoolField(TEXT("reliable"), Function->HasAnyFunctionFlags(FUNC_NetReliable));
            }
            else
            {
                CallInfo->SetStringField(TEXT("resolution"), TEXT("unresolved"));
            }
            Result->SetObjectField(TEXT("call"), CallInfo);
        }

        // Read metadata on Custom Events and other specialized nodes without relying
        // on a specific derived class's editor-only member layout.
        if (const FNameProperty* CustomName = FindFProperty<FNameProperty>(Node->GetClass(), TEXT("CustomFunctionName")))
        {
            const FName Name = CustomName->GetPropertyValue_InContainer(Node);
            if (!Name.IsNone())
            {
                Result->SetStringField(TEXT("custom_event_name"), Name.ToString());
            }
        }

        FObjectArray Pins;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin) { continue; }
            Pins.Add(ObjectValue(PinToJson(Pin)));
            ++InOutPinCount;
            if (Pin->Direction == EGPD_Output)
            {
                InOutLinkCount += Pin->LinkedTo.Num();
            }
        }
        Result->SetArrayField(TEXT("pins"), Pins);
        return Result;
    }

    static FObjectJson GraphToJson(const UEdGraph* Graph, int32& InOutNodeCount, int32& InOutPinCount, int32& InOutLinkCount)
    {
        const FObjectJson Result = NewObject();
        Result->SetStringField(TEXT("name"), Graph->GetName());
        Result->SetStringField(TEXT("path"), Graph->GetPathName());
        Result->SetStringField(TEXT("id"), GuidString(Graph->GraphGuid));
        Result->SetStringField(TEXT("class"), Graph->GetClass()->GetPathName());
        FObjectArray Nodes;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) { continue; }
            Nodes.Add(ObjectValue(NodeToJson(Node, InOutPinCount, InOutLinkCount)));
            ++InOutNodeCount;
        }
        Result->SetArrayField(TEXT("nodes"), Nodes);
        return Result;
    }

    static FObjectJson BlueprintToJson(UBlueprint* Blueprint, int32& InOutGraphCount, int32& InOutNodeCount, int32& InOutPinCount, int32& InOutLinkCount)
    {
        FObjectJson Result = NewObject();
        Result->SetStringField(TEXT("asset"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("class"), Blueprint->GetClass()->GetPathName());
        Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
        Result->SetStringField(TEXT("generated_class"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());

        FObjectArray Variables;
        for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
        {
            FObjectJson V = NewObject();
            V->SetStringField(TEXT("name"), Variable.VarName.ToString());
            V->SetStringField(TEXT("id"), GuidString(Variable.VarGuid));
            V->SetStringField(TEXT("type"), Variable.VarType.PinCategory.ToString());
            V->SetBoolField(TEXT("replicated"), (Variable.PropertyFlags & CPF_Net) != 0);
            V->SetStringField(TEXT("rep_notify"), Variable.RepNotifyFunc.ToString());
            V->SetNumberField(TEXT("replication_condition"), static_cast<int32>(Variable.ReplicationCondition.GetValue()));
            Variables.Add(ObjectValue(V));
        }
        Result->SetArrayField(TEXT("declared_variables"), Variables);

        // Every graph owned by this asset, including nested state-machine and
        // transition graphs. No need to open each graph in the editor.
        TArray<UObject*> OwnedObjects;
        GetObjectsWithOuter(Blueprint, OwnedObjects, true);
        TArray<UEdGraph*> Graphs;
        for (UObject* Object : OwnedObjects)
        {
            if (UEdGraph* Graph = Cast<UEdGraph>(Object))
            {
                Graphs.Add(Graph);
            }
        }
        Graphs.Sort([](const UEdGraph& A, const UEdGraph& B)
        {
            return A.GetPathName() < B.GetPathName();
        });

        FObjectArray GraphItems;
        for (const UEdGraph* Graph : Graphs)
        {
            GraphItems.Add(ObjectValue(GraphToJson(Graph, InOutNodeCount, InOutPinCount, InOutLinkCount)));
            ++InOutGraphCount;
        }
        Result->SetArrayField(TEXT("graphs"), GraphItems);
        Result->SetStringField(TEXT("status"), Graphs.IsEmpty() ? TEXT("no_graphs") : TEXT("extracted"));
        return Result;
    }

    static void Run()
    {
        const FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BlueprintFlowAnalyzer"));
        IFileManager::Get().MakeDirectory(*OutputDir, true);
        const FString AnalysisFile = FPaths::Combine(OutputDir, TEXT("analysis.json"));
        const FString ReportFile = FPaths::Combine(OutputDir, TEXT("report.json"));

        const FObjectJson Analysis = NewObject();
        Analysis->SetStringField(TEXT("schema_version"), TEXT("0.1"));
        Analysis->SetStringField(TEXT("project"), FPaths::GetProjectFilePath());
        Analysis->SetStringField(TEXT("scope"), TEXT("/Game"));
        Analysis->SetStringField(TEXT("kind"), TEXT("static_editor_graph"));
        FObjectArray Blueprints;
        FObjectArray Errors;

        int32 GraphCount = 0, NodeCount = 0, PinCount = 0, LinkCount = 0;
        int32 Successful = 0, NoGraphs = 0, Failed = 0;
        TArray<FAssetData> Assets;

        IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        Registry.WaitForCompletion();
        FARFilter Filter;
        Filter.PackagePaths.Add(FName(TEXT("/Game")));
        Filter.bRecursivePaths = true;
        Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
        Filter.bRecursiveClasses = true;
        Registry.GetAssets(Filter, Assets);
        Assets.Sort([](const FAssetData& A, const FAssetData& B)
        {
            return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
        });

        for (const FAssetData& AssetData : Assets)
        {
            const FString AssetPath = AssetData.GetSoftObjectPath().ToString();
            UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
            if (!Blueprint)
            {
                FObjectJson Error = NewObject();
                Error->SetStringField(TEXT("asset"), AssetPath);
                Error->SetStringField(TEXT("error"), TEXT("Failed to load asset as UBlueprint"));
                Errors.Add(ObjectValue(Error));
                ++Failed;
                continue;
            }
            const FObjectJson Exported = BlueprintToJson(Blueprint, GraphCount, NodeCount, PinCount, LinkCount);
            if (Exported->GetStringField(TEXT("status")) == TEXT("no_graphs")) { ++NoGraphs; }
            else { ++Successful; }
            Blueprints.Add(ObjectValue(Exported));
        }
        Analysis->SetArrayField(TEXT("blueprints"), Blueprints);
        Analysis->SetArrayField(TEXT("errors"), Errors);
        const bool bAnalysisWritten = WriteJson(AnalysisFile, Analysis);

        const FObjectJson Report = NewObject();
        Report->SetStringField(TEXT("schema_version"), TEXT("0.1"));
        Report->SetStringField(TEXT("status"), (!bAnalysisWritten || Failed > 0) ? TEXT("incomplete") : TEXT("completed"));
        Report->SetStringField(TEXT("analysis_file"), AnalysisFile);
        Report->SetNumberField(TEXT("assets_discovered"), Assets.Num());
        Report->SetNumberField(TEXT("blueprints_extracted"), Successful);
        Report->SetNumberField(TEXT("blueprints_without_graphs"), NoGraphs);
        Report->SetNumberField(TEXT("blueprints_failed"), Failed);
        Report->SetNumberField(TEXT("graphs"), GraphCount);
        Report->SetNumberField(TEXT("nodes"), NodeCount);
        Report->SetNumberField(TEXT("pins"), PinCount);
        Report->SetNumberField(TEXT("outgoing_links"), LinkCount);
        Report->SetArrayField(TEXT("errors"), Errors);
        const bool bReportWritten = WriteJson(ReportFile, Report);

        UE_LOG(LogBlueprintFlowAnalyzer, Display,
            TEXT("Done. assets=%d extracted=%d no_graphs=%d failed=%d graphs=%d nodes=%d pins=%d outgoing_links=%d"),
            Assets.Num(), Successful, NoGraphs, Failed, GraphCount, NodeCount, PinCount, LinkCount);
        UE_LOG(LogBlueprintFlowAnalyzer, Display, TEXT("JSON output: %s"), *AnalysisFile);
        if (!bAnalysisWritten || !bReportWritten)
        {
            UE_LOG(LogBlueprintFlowAnalyzer, Error, TEXT("Output write failed"));
        }
    }
}

class FBlueprintFlowAnalyzerModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("BPAnalyzerRun")))
        {
            TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateRaw(this, &FBlueprintFlowAnalyzerModule::Tick), 0.25f);
        }
    }

    virtual void ShutdownModule() override
    {
        if (TickerHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
            TickerHandle.Reset();
        }
    }

private:
    bool Tick(float DeltaSeconds)
    {
        // The editor finishes initializing the Asset Registry before loading assets.
        BPFA::Run();
        FPlatformMisc::RequestExit(false);
        return false;
    }
    FTSTicker::FDelegateHandle TickerHandle;
};

IMPLEMENT_MODULE(FBlueprintFlowAnalyzerModule, BlueprintFlowAnalyzer)
