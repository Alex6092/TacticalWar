#include "pch.h"
#include "CharacterView.h"

using namespace tw;

CharacterView::CharacterView(BaseCharacterModel * model)
	: AbstractCharacterView<sf::Sprite*>(model)
{
	testCharacterTexture.loadFromFile("../assets/Warrior/bottomright-sheet.png");
	testCharacterTexture.setSmooth(true);
	testSprite.setTexture(testCharacterTexture);
	testSprite.setTextureRect(sf::IntRect(0, 288, 173, 224));
	testOffsetX = 78;
	testOffsetY = 48;
}

sf::Sprite* CharacterView::getImageToDraw()
{
	// TODO : (team "Animation personnage") : R�aliser le code d'animation du personnage dans cette classe.
	// Cette m�thode (getImageToDraw) doit retourner l'image qui devra �tre dessin�e � l'�cran (en fonction de o� en est l'animation)
	// Vous devrez �galement utiliser les services de la team "Colorisation" pour obtenir un personnage color� (personnalisation des couleurs en fonction de l'�quipe).

	return &testSprite;
}

void CharacterView::update(float deltatime)
{

}