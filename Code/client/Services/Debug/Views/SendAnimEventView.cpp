#include <imgui.h>
#include <Services/DebugService.h>
#include <Games/Skyrim/Actor.h>

void DebugService::DrawSendAnimEventView()
{
    static uint32_t fetchFormId = 0x14;
    static BSFixedString sendAnimEventString("");
    static Actor* pActor = nullptr;

    ImGui::Begin("Send debug anim event");
    // input formID, then lookup button
    ImGui::InputScalar("Form ID", ImGuiDataType_U32, &fetchFormId, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
    if (ImGui::Button("Lookup"))
    {
        if (fetchFormId)
        {
            if (auto* pFetchForm = TESForm::GetById(fetchFormId))
                pActor = Cast<Actor>(pFetchForm);
        }
    }

    if (!pActor)
    {
        ImGui::End();
        return;
    }

    // Add input for animation event name
    char buffer[256];
    strncpy_s(buffer, sendAnimEventString.AsAscii(), sizeof(buffer));
    if (ImGui::InputText("Animation event name", buffer, sizeof(buffer)))
    {
        sendAnimEventString = BSFixedString(buffer);
    }

    // Send button
    if (ImGui::Button("Send"))
    {
        pActor->SendAnimationEvent(&sendAnimEventString);
    }

    ImGui::End();
    
}
