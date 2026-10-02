#pragma once

#include "pch.h"
#include <AbstractCharacterView.h>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <Pathfinder.h>
#include <deque>
#include <map>
#include <BaseCharacterModel.h>

namespace tw
{
	template <typename T> int getSign(T val) {
		return (T(0) < val) - (val < T(0));
	}

	class CharacterView : public AbstractCharacterView<sf::Sprite*>, CharacterEventListener
	{
	private:
		static std::map<std::string, sf::Texture*> * textureCache;
		static sf::Texture* getCachedTexture(std::string path);

		Orientation orientation;
		Animation animation;
		std::map<Orientation, std::map<Animation, std::vector<sf::Sprite*> > > animationsMap;
		std::map<Orientation, std::map<Animation, sf::Texture * > > animationsMaskMap;

		sf::Font font;
		sf::Text pseudoTxt;
		sf::Sprite lifeBg;
		sf::Sprite paBg;
		sf::Sprite pmBg;
		sf::Text lifeTxt;
		sf::Text paTxt;
		sf::Text pmTxt;
		
		double elsetime = 0;
		
		std::vector<sf::Sprite*> loadAnimation(std::string filename, bool flip = false, bool mask = false);
		sf::Texture * loadTexture(std::string filename);

	public:
		CharacterView(BaseCharacterModel * model);
		virtual ~CharacterView();
		virtual sf::Sprite * getImageToDraw();

		virtual sf::Texture * getMaskToDraw();

		sf::Text * getPseudoText()
		{
			return &pseudoTxt;
		}

		sf::Text * getLifeText()
		{
			lifeTxt.setString(std::to_string(getModel()->getCurrentLife()) + "/" + std::to_string(getModel()->getBaseMaxLife()));
			return &lifeTxt;
		}

		sf::Text * getPaText()
		{
			paTxt.setString(std::to_string(getModel()->getCurrentPA()));
			return &paTxt;
		}

		sf::Text * getPmText()
		{
			pmTxt.setString(std::to_string(getModel()->getCurrentPM()));
			return &pmTxt;
		}

		sf::Sprite * getLifeBackground()
		{
			return &lifeBg;
		}

		sf::Sprite * getPaBackground()
		{
			return &paBg;
		}

		sf::Sprite * getPmBackground()
		{
			return &pmBg;
		}

		virtual void update(float deltatime);
		void setAnimation(Animation animation) {
			this->animation = animation;
		}
		void setOrientation(Orientation orientation)
		{
			this->orientation = orientation;
		}

		float getHeight()
		{
			float height = animationsMap[Orientation::BOTTOM_RIGHT][Animation::IDLE][0]->getGlobalBounds().height + 10;
			return height;
		}

		// CharacterEventListener
		virtual void onPositionChanged(BaseCharacterModel * c, int newPositionX, int newPositionY) {}

		virtual void onLookAt(int targetX, int targetY)
		{
			int diffX = targetX - getModel()->getCurrentX();
			int diffY = targetY - getModel()->getCurrentY();

			// Vecteur nord = [1;-1];

			if (diffX != 0 || diffY != 0) {  // Si c'est pas un lancer sur soi-même
				double normeVecteur = sqrt((diffX * diffX) + (diffY * diffY));
				double normeNorth = sqrt(2.0);
				double scalaire = (diffX * 1) + (diffY * -1);
				double cosTeta = scalaire / (normeVecteur * normeNorth);
				double s = (1 * diffY) - (-1 * diffX); // Permet de trouver le signe de l'angle
				double sign = (double)getSign(s);
				double radAngle = (sign != 0 ? sign : 1) * acos(cosTeta);

				double pi2 = M_PI / 2;

				// Regarde vers le nord :
				if (radAngle >= 0 && radAngle < pi2) {
					std::cout << "Look at -- Nord" << std::endl;
					setOrientation(Orientation::BOTTOM_RIGHT);
				}
				// Regarde vers l'est
				else if (radAngle >= pi2 && radAngle <= M_PI) {
					std::cout << "Look at -- Est" << std::endl;
					setOrientation(Orientation::BOTTOM_LEFT);
				}
				// Regarde vers le sud
				else if (radAngle <= -pi2 && radAngle > -M_PI) {
					std::cout << "Look at -- Sud" << std::endl;
					setOrientation(Orientation::TOP_LEFT);
				}
				// Regarde vers l'ouest
				else {
					std::cout << "Look at -- Ouest" << std::endl;
					setOrientation(Orientation::TOP_RIGHT);
				}
			}
		}
	};
}