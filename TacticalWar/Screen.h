#pragma once

#include <SFML/Graphics.hpp>
#include <TGUI/TGUI.hpp>

namespace tw
{
	class Screen
	{
	private:
		float deltatime;
		float shaderEllapsedTime;


	protected:
		float getDeltatime() {
			return deltatime;
		}

		float getShaderEllapsedTime()
		{
			return shaderEllapsedTime;
		}

	public:
		Screen();
		virtual void handleEvents(sf::RenderWindow * window, tgui::Gui * gui) = 0;
		virtual void update(float deltatime) {
			this->deltatime = deltatime;
			shaderEllapsedTime += deltatime;
		}
		virtual void render(sf::RenderWindow * window) = 0;

		void setShaderEllapsedTime(float ellapsedTime)
		{
			this->shaderEllapsedTime = ellapsedTime;
		}
	};
}