#include "Internationalization/Internationalization.h"
#include "IPAddress.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#define LOCTEXT_NAMESPACE "SAMUdpSendNode"

namespace SAMUdpSendNodeNames
{
    METASOUND_PARAM(InputTrigger, "Send", "Trigger to send the text as a UDP datagram.");
    METASOUND_PARAM(InputText, "Text", "UDP payload text.");
    METASOUND_PARAM(InputHost, "Host", "Destination IPv4 host.");
    METASOUND_PARAM(InputPort, "Port", "Destination UDP port.");
    METASOUND_PARAM(OutputSent, "Sent", "Fires when a send attempt occurs.");
}

namespace Metasound
{
    class FSAMUdpSendOperator final : public TExecutableOperator<FSAMUdpSendOperator>
    {
    public:
        static const FVertexInterface& GetDefaultInterface()
        {
            using namespace SAMUdpSendNodeNames;

            static const FVertexInterface Interface(
                FInputVertexInterface(
                    TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
                    TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputText), FString(TEXT("hello from metasound"))),
                    TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputHost), FString(TEXT("127.0.0.1"))),
                    TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPort), 7001)
                ),
                FOutputVertexInterface(
                    TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputSent))
                )
            );

            return Interface;
        }

        static const FNodeClassMetadata& GetNodeInfo()
        {
            auto InitNodeInfo = []() -> FNodeClassMetadata
            {
                FNodeClassMetadata Info;
                Info.ClassName = { TEXT("SAM"), TEXT("UDPTextSend"), TEXT("Audio") };
                Info.MajorVersion = 1;
                Info.MinorVersion = 0;
                Info.DisplayName = LOCTEXT("NodeDisplayName", "UDP Send Text");
                Info.Description = LOCTEXT("NodeDescription", "Sends a text UDP datagram (for external SAM app control).");
                Info.Author = TEXT("Codex");
                Info.PromptIfMissing = LOCTEXT("NodeMissingPrompt", "SAM UDP node is missing");
                Info.DefaultInterface = GetDefaultInterface();
                Info.CategoryHierarchy = { LOCTEXT("Category", "Utility"), LOCTEXT("SubCategory", "Networking") };
                return Info;
            };

            static const FNodeClassMetadata Info = InitNodeInfo();
            return Info;
        }

        static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
        {
            using namespace SAMUdpSendNodeNames;

            const FInputVertexInterfaceData& Inputs = InParams.InputData;

            FTriggerReadRef InSend = Inputs.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
            FStringReadRef InText = Inputs.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(InputText), InParams.OperatorSettings, FString(TEXT("hello from metasound")));
            FStringReadRef InHost = Inputs.GetOrCreateDefaultDataReadReference<FString>(METASOUND_GET_PARAM_NAME(InputHost), InParams.OperatorSettings, FString(TEXT("127.0.0.1")));
            Int32ReadRef InPort = Inputs.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputPort), InParams.OperatorSettings, 7001);

            return MakeUnique<FSAMUdpSendOperator>(InSend, InText, InHost, InPort, InParams.OperatorSettings);
        }

        FSAMUdpSendOperator(FTriggerReadRef InSend,
                            FStringReadRef InText,
                            FStringReadRef InHost,
                            Int32ReadRef InPort,
                            const FOperatorSettings& InSettings)
            : SendTrigger(InSend)
            , Text(InText)
            , Host(InHost)
            , Port(InPort)
            , Sent(FTriggerWriteRef::CreateNew(InSettings))
        {
        }

        virtual ~FSAMUdpSendOperator() override
        {
            if (Socket != nullptr)
            {
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
                Socket = nullptr;
            }
        }

        void BindInputs(FInputVertexInterfaceData& InOutVertexData) {}

        void BindOutputs(FOutputVertexInterfaceData& InOutVertexData)
        {
            using namespace SAMUdpSendNodeNames;
            InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputSent), Sent);
        }

        void Execute()
        {
            Sent->AdvanceBlock();

            SendTrigger->ExecuteBlock(
                [](int32, int32) {},
                [this](int32 StartFrame, int32)
                {
                    SendUdp(*Text, *Host, *Port);
                    Sent->TriggerFrame(StartFrame);
                }
            );
        }

    private:
        void EnsureSocket(const FString& InHost, int32 InPort)
        {
            if (Socket == nullptr)
            {
                Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_DGram, TEXT("SAMUdpMetaSoundSocket"), false);
                if (Socket != nullptr)
                {
                    Socket->SetNonBlocking(true);
                    Socket->SetBroadcast(false);
                    Socket->SetReuseAddr(true);
                }
            }

            if (!RemoteAddr.IsValid())
            {
                RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
            }

            bool bOk = false;
            RemoteAddr->SetIp(*InHost, bOk);
            RemoteAddr->SetPort(FMath::Clamp(InPort, 1, 65535));
        }

        void SendUdp(const FString& InText, const FString& InHost, int32 InPort)
        {
            EnsureSocket(InHost, InPort);

            if (Socket == nullptr || !RemoteAddr.IsValid())
            {
                return;
            }

            FTCHARToUTF8 Payload(*InText);
            int32 BytesSent = 0;
            Socket->SendTo(reinterpret_cast<const uint8*>(Payload.Get()), Payload.Length(), BytesSent, *RemoteAddr);
        }

        FTriggerReadRef SendTrigger;
        FStringReadRef Text;
        FStringReadRef Host;
        Int32ReadRef Port;
        FTriggerWriteRef Sent;

        FSocket* Socket = nullptr;
        TSharedPtr<FInternetAddr> RemoteAddr;
    };

    class FSAMUdpSendNode final : public FNodeFacade
    {
    public:
        explicit FSAMUdpSendNode(const FNodeInitData& Init)
            : FNodeFacade(Init.InstanceName, Init.InstanceID, TFacadeOperatorClass<FSAMUdpSendOperator>())
        {
        }
    };
}

METASOUND_REGISTER_NODE(Metasound::FSAMUdpSendNode)

#undef LOCTEXT_NAMESPACE
