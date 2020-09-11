#pragma once

#include <vector>
#include "RendererEventListener.h"

namespace tw
{
	class Environment;
	class BaseCharacterModel;

	class AbstractRenderer
	{
		std::vector<RendererEventListener *> listeners;

	protected:
		void notifyCellClicked(int x, int y);
		void notifyCellHover(int x, int y);
		void notifyCellMouseDown(int x, int y);

	public:
		AbstractRenderer();
		virtual ~AbstractRenderer();

		virtual void render(Environment* environment, std::vector<BaseCharacterModel*> & characters) = 0;
		void addEventListener(RendererEventListener * listener);
		bool removeEventListener(RendererEventListener * listener);
	};
}
