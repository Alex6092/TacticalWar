#pragma once

#include "pch.h"
#include "AbstractCharacterView.h"
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <map>

namespace tw
{
	class CharacterView : public AbstractCharacterView<sf::Sprite*>
	{
	private:
		static std::map<std::string, sf::Texture*> * textureCache;
		static sf::Texture* getCachedTexture(std::string path);

		sf::Texture *testCharacterTexture;
		sf::Sprite testSprite;
		int testOffsetX;
		int testOffsetY;
		std::vector<int> myvector;
		
		double NbImg = -1, elsetime = 0;
		float time;
		int tabs[6] = {0};
		char lignes[600] = { 0 }, ligneset[30][60] = { 0 };
		
	public:
		CharacterView(BaseCharacterModel * model);
		virtual ~CharacterView();
		virtual sf::Sprite * getImageToDraw();
		virtual void update(float deltatime);
	};
}