#include "EditorUI.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThreadAttribute]
int main(array<String^>^ args) {
	for (int i = 0; i < args->Length; i++)
	{
		bool hasValue = i + 1 < args->Length;
		if (String::Equals(args[i], L"--open") && hasValue)
			EnvironmentEditor::EditorUI::openMapId = Int32::Parse(args[++i]);
		else if (String::Equals(args[i], L"--screenshot") && hasValue)
			EnvironmentEditor::EditorUI::screenshotPath = args[++i];
		else if (String::Equals(args[i], L"--validate"))
			EnvironmentEditor::EditorUI::validateOnStart = true;
	}

	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);
	EnvironmentEditor::EditorUI form;
	Application::Run(%form);
	return 0;
}