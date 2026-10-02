#include "EditorEventListener.h"
#include "EditorUI.h"

#include <iostream>

using namespace EnvironmentEditor;

EditorEventListener::EditorEventListener(EditorUI ^ hmi)
{
	this->hmi = hmi;
}

EditorEventListener::~EditorEventListener()
{
}

void EditorEventListener::onCellClicked(int x, int y)
{	
	// Clic sur la cellule X;Y indique à l'éditeur 
	// qu'il faut modifier l'environnement à cette position
	hmi->editCell(x, y);
}

void EditorEventListener::onCellHover(int x, int y)
{

}

void EditorEventListener::onCellMouseDown(int x, int y)
{
	hmi->editCell(x, y);
}