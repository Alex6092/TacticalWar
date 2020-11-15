#include "pch.h"
#include "CharacterView.h"
#include <iostream>
#include <string>
#include <fstream>
using namespace tw;
using namespace std;
std::map<std::string, sf::Texture*> * CharacterView::textureCache = NULL;

sf::Texture* CharacterView::getCachedTexture(std::string path)
{
	if (textureCache == NULL)
		textureCache = new std::map<std::string, sf::Texture*>();

	sf::Texture *texture;
	if (textureCache->find(path) != textureCache->end())
	{
		texture = (*textureCache)[path];
	}
	else
	{
		texture = new sf::Texture();
		texture->loadFromFile(path);
		(*textureCache)[path] = texture;
	}

	return texture;
}

CharacterView::CharacterView(BaseCharacterModel * model)
	: AbstractCharacterView<sf::Sprite*>(model)
{
	testCharacterTexture = getCachedTexture("./assets/Warrior/bottomright-sheet.png");
	/*testCharacterTexture = new sf::Texture();
	testCharacterTexture->loadFromFile("./assets/Warrior/bottomright-sheet.png");*/
	ifstream position("./assets/Warrior/bottomright-sheet.anim.exode", ios::in);
	if (position)
		position.get(lignes, 600, '/n');  // tant que l'on peut mettre la ligne dans "contenu"	
	int x = 0, z = 0;
	for (int y = 0; y < strlen(lignes); y++) {
		if(lignes[y] != '\n'){
	
			ligneset[z][x] = lignes[y];
			x++;
		}
		else {
			z++; x = 0;
		}
	}
	for (int y = 0; y < 15; y++) {
		if (ligneset[y][0] != '\0') {
			NbImg++;
		}
		else
			y = 15;
	}
	
	testCharacterTexture->setSmooth(true);
	testSprite.setTexture(*testCharacterTexture);
	testSprite.setTextureRect(sf::IntRect(0, 288, 173, 224));
	testOffsetX = 78;
	testOffsetY = 48;

}

CharacterView::~CharacterView()
{
	//delete testCharacterTexture;
}

sf::Sprite* CharacterView::getImageToDraw()
{
	// TODO : (team "Animation personnage") : Réaliser le code d'animation du personnage dans cette classe.
	// Cette méthode (getImageToDraw) doit retourner l'image qui devra être dessinée à l'écran (en fonction de où en est l'animation)
	// Vous devrez également utiliser les services de la team "Colorisation" pour obtenir un personnage coloré (personnalisation des couleurs en fonction de l'équipe).
	
	double p = 1, time = elsetime;
	p /= NbImg;
	int index;
	index = fmod(time / p, NbImg);
	char tab[40] = { 0 };
	int b = 0;

	for (int y = 0; y < strlen(ligneset[index]); y++) {
		if (ligneset[index][y] != ';' ) {
				tab[b] = ligneset[index][y];
				b++;
		}
		else {
			myvector.push_back(atof(tab));
			for (int o = 0; o < 5; o++)
				tab[o] = { 0 };
			b = 0;
		}
	}
	if (tab[0] != '\0') {
			myvector.push_back(atof(tab));
	}
	int sum = 0;
	while (!myvector.empty())
	{
		tabs[sum] = myvector.back();
		myvector.pop_back();
		sum++;
	}
	testSprite.setTextureRect(sf::IntRect(tabs[5], tabs[4], tabs[3], tabs[2]));
	testOffsetX = tabs[1];
	testOffsetY = tabs[0];
	return &testSprite;
}

void CharacterView::update(float deltatime)
{
	elsetime += deltatime;
}