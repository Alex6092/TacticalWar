#pragma once

#include "pch.h"
#include "AbstractCharacterView.h"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <Pathfinder.h>
#include <deque>
#include <map>

namespace tw
{
	class CharacterView : public AbstractCharacterView<sf::Sprite*>
	{
	private:
		static std::map<std::string, sf::Texture*> * textureCache;
		static sf::Texture* getCachedTexture(std::string path);

		Orientation orientation;
		Animation animation;
		std::map<Orientation, std::map<Animation, std::vector<sf::Sprite*> > > animationsMap;
		
		double elsetime = 0;
		
		std::vector<sf::Sprite*> loadAnimation(std::string filename, bool flip = false);

	public:
		CharacterView(BaseCharacterModel * model);
		virtual ~CharacterView();
		virtual sf::Sprite * getImageToDraw();
		virtual void update(float deltatime);
		void setAnimation(Animation animation) {
			this->animation = animation;
		}
		void setOrientation(Orientation orientation)
		{
			this->orientation = orientation;
		}
	};
}