#include "pch.h"
#include "AbstractRenderer.h"

using namespace tw;

AbstractRenderer::AbstractRenderer()
{
}


AbstractRenderer::~AbstractRenderer()
{
}

void AbstractRenderer::notifyCellClicked(int x, int y)
{
	for (int i = 0; i < listeners.size(); i++)
	{
		listeners[i]->onCellClicked(x, y);
	}
}

void AbstractRenderer::notifyCellHover(int x, int y)
{
	for (int i = 0; i < listeners.size(); i++)
	{
		listeners[i]->onCellHover(x, y);
	}
}

void AbstractRenderer::notifyCellMouseDown(int x, int y)
{
	for (int i = 0; i < listeners.size(); i++)
	{
		listeners[i]->onCellMouseDown(x, y);
	}
}

void AbstractRenderer::notifyEvent(void * e)
{
	for (int i = 0; i < listeners.size(); i++)
	{
		listeners[i]->onEvent(e);
	}
}

void AbstractRenderer::addEventListener(RendererEventListener * listener)
{
	std::vector<RendererEventListener*>::iterator it;
	if ((it = std::find(listeners.begin(), listeners.end(), listener)) == listeners.end())
	{
		listeners.push_back(listener);
	}
}

bool AbstractRenderer::removeEventListener(RendererEventListener * listener)
{
	std::vector<RendererEventListener*>::iterator it;
	if ((it = std::find(listeners.begin(), listeners.end(), listener)) != listeners.end())
	{
		listeners.erase(it);
		return true;
	}

	return false;
}