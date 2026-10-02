#include "EditorEventListener.h"
#include "EditorUI.h"

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
	hmi->onCellPressed(x, y);
}

void EditorEventListener::onCellHover(int x, int y)
{
	hmi->onCellHover(x, y);
}

void EditorEventListener::onCellMouseDown(int x, int y)
{
	hmi->onCellDragged(x, y);
}

void EditorEventListener::onEvent(void * e)
{
	if (e != NULL)
		hmi->onSfmlEvent(*(sf::Event*)e);
}
