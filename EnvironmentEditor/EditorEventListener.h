#pragma once
#include <msclr/gcroot.h>
#include "RendererEventListener.h"

namespace EnvironmentEditor
{
	ref class EditorUI;

	// Relaie les événements du rendu (cases cliquées, survolées, événements SFML) à l'éditeur.
	class EditorEventListener : public tw::RendererEventListener
	{
		msclr::gcroot<EditorUI ^> hmi;
	public:
		EditorEventListener(EditorUI ^ hmi);
		~EditorEventListener();

		virtual void onCellClicked(int x, int y);
		virtual void onCellHover(int x, int y);
		virtual void onCellMouseDown(int x, int y);
		virtual void onEvent(void * e);
	};
}
