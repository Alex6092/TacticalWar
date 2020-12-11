#pragma once

namespace tw
{
	class RendererEventListener
	{
	public:
		RendererEventListener();
		~RendererEventListener();

		virtual void onCellClicked(int cellX, int cellY) = 0;
		virtual void onCellHover(int cellX, int cellY) = 0;
		virtual void onCellMouseDown(int cellX, int cellY) = 0;
		virtual void onEvent(void * e) {};
	};
}