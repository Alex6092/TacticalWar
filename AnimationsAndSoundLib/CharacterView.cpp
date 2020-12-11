#include "pch.h"
#include "CharacterView.h"
#include <iostream>
#include <string>
#include <StringUtils.h>
#include <SFML/System/FileInputStream.hpp>

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
	orientation = Orientation::BOTTOM_RIGHT;
	animation = Animation::RUN;
	animationsMap[Orientation::BOTTOM_RIGHT][Animation::RUN] = loadAnimation(model->getGraphicsPath() + "bottomright-sheet");
	animationsMap[Orientation::BOTTOM_LEFT][Animation::RUN] = loadAnimation(model->getGraphicsPath() + "bottomright-sheet", true);
	animationsMap[Orientation::BOTTOM_RIGHT][Animation::IDLE] = loadAnimation(model->getGraphicsPath() + "bottomright-static-sheet");
	animationsMap[Orientation::BOTTOM_LEFT][Animation::IDLE] = loadAnimation(model->getGraphicsPath() + "bottomright-static-sheet", true);
	animationsMap[Orientation::BOTTOM_RIGHT][Animation::DIE] = loadAnimation(model->getGraphicsPath() + "bottomright-die-sheet");
	animationsMap[Orientation::BOTTOM_LEFT][Animation::DIE] = loadAnimation(model->getGraphicsPath() + "bottomright-die-sheet", true);
	animationsMap[Orientation::BOTTOM_RIGHT][Animation::ATTACK1] = loadAnimation(model->getGraphicsPath() + "bottomright-magical_attack-sheet");
	animationsMap[Orientation::BOTTOM_LEFT][Animation::ATTACK1] = loadAnimation(model->getGraphicsPath() + "bottomright-magical_attack-sheet", true);
	animationsMap[Orientation::BOTTOM_RIGHT][Animation::ATTACK2] = loadAnimation(model->getGraphicsPath() + "bottomright-physical_attack-sheet");
	animationsMap[Orientation::BOTTOM_LEFT][Animation::ATTACK2] = loadAnimation(model->getGraphicsPath() + "bottomright-physical_attack-sheet", true);
	animationsMap[Orientation::BOTTOM_RIGHT][Animation::TAKE_DAMAGE] = loadAnimation(model->getGraphicsPath() + "bottomright-take_damage-sheet");
	animationsMap[Orientation::BOTTOM_LEFT][Animation::TAKE_DAMAGE] = loadAnimation(model->getGraphicsPath() + "bottomright-take_damage-sheet", true);



	animationsMap[Orientation::TOP_RIGHT][Animation::RUN] = loadAnimation(model->getGraphicsPath() + "topright-sheet");
	animationsMap[Orientation::TOP_LEFT][Animation::RUN] = loadAnimation(model->getGraphicsPath() + "topright-sheet", true);
	animationsMap[Orientation::TOP_RIGHT][Animation::IDLE] = loadAnimation(model->getGraphicsPath() + "topright-static-sheet");
	animationsMap[Orientation::TOP_LEFT][Animation::IDLE] = loadAnimation(model->getGraphicsPath() + "topright-static-sheet", true);
	animationsMap[Orientation::TOP_RIGHT][Animation::DIE] = loadAnimation(model->getGraphicsPath() + "topright-die-sheet");
	animationsMap[Orientation::TOP_LEFT][Animation::DIE] = loadAnimation(model->getGraphicsPath() + "topright-die-sheet", true);
	animationsMap[Orientation::TOP_RIGHT][Animation::ATTACK1] = loadAnimation(model->getGraphicsPath() + "topright-magical_attack-sheet");
	animationsMap[Orientation::TOP_LEFT][Animation::ATTACK1] = loadAnimation(model->getGraphicsPath() + "topright-magical_attack-sheet", true);
	animationsMap[Orientation::TOP_RIGHT][Animation::ATTACK2] = loadAnimation(model->getGraphicsPath() + "topright-physical_attack-sheet");
	animationsMap[Orientation::TOP_LEFT][Animation::ATTACK2] = loadAnimation(model->getGraphicsPath() + "topright-physical_attack-sheet", true);
	animationsMap[Orientation::TOP_RIGHT][Animation::TAKE_DAMAGE] = loadAnimation(model->getGraphicsPath() + "topright-take_damage-sheet");
	animationsMap[Orientation::TOP_LEFT][Animation::TAKE_DAMAGE] = loadAnimation(model->getGraphicsPath() + "topright-take_damage-sheet", true);

	
}

std::vector<sf::Sprite*> CharacterView::loadAnimation(std::string filename, bool flip)
{
	std::vector<sf::Sprite *> anim;
	sf::Texture *testCharacterTexture = getCachedTexture(filename + ".png");
	sf::FileInputStream stream;
	string str;
	std::deque<std::string> positionView;
	int NbImg = 0;
	int testOffsetX, testOffsetY;

		if (stream.open(filename + ".anim.exode")) {
			int length = stream.getSize();
			char * data = new char[length + 1];
			stream.read(data, length);
			data[length] = '\0';
			std::vector<std::string> lines = StringUtils::explode(data, '\n');
			delete data;
			for (int i = 0; i < lines.size(); i++)
				positionView.push_back(lines[i]);	// tant que l'on peut mettre la ligne dans "positionView"	

		}
		else
		{
			std::cout << "Echec de l'ouverture du fichier " << filename.c_str() << ".anim.exode" << std::endl;
		}
		for (int y = 0; y < positionView.size(); y++) {		// compte le nombre d'imagne dans le fichier
			if (positionView[y][0] != '\0')
				NbImg++;
		}

		testCharacterTexture->setSmooth(true);

		for (int i = 0; i < NbImg; i++)
		{
			std::vector<std::string> result = StringUtils::explode(positionView[i], ';');
			sf::Sprite * s = new sf::Sprite(*testCharacterTexture, sf::IntRect(atoi(result[0].c_str()), atoi(result[1].c_str()), atoi(result[2].c_str()), atoi(result[3].c_str())));
			/*testOffsetX = std::atof(result[4].c_str());
			testOffsetY = std::atof(result[5].c_str());*/

			s->setOrigin(atoi(result[4].c_str()), atoi(result[3].c_str()) - atoi(result[5].c_str()));
			if(flip)
				s->setScale(-1, 1);
			anim.push_back(s);
			result.clear();
		}
		return anim;
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
	
	double p = 0.5, time = elsetime;

	tw::Animation neededAnimation = getModel()->getNeededAnimation();
	float animationDuration = getModel()->getAnimationDuration();
	bool reinit = getModel()->getReinitViewTime();

	if (animationDuration > 0 && neededAnimation != Animation::IDLE && neededAnimation != Animation::RUN && reinit)
	{
		setAnimation(neededAnimation);
		elsetime = 0;
	}

	if (neededAnimation != Animation::IDLE && neededAnimation != Animation::RUN && animationDuration - time <= 0)
	{
		setAnimation(Animation::IDLE);
	}
	else if (neededAnimation != Animation::IDLE && neededAnimation != Animation::RUN)
	{
		p = animationDuration;
		setAnimation(neededAnimation);
	}

	int nbImg = animationsMap[orientation][animation].size();
	p /= nbImg;
	int index;
	index = fmod(time / p, nbImg);
	return animationsMap[orientation][animation][index];
}

void CharacterView::update(float deltatime)
{
	elsetime += deltatime;

	BaseCharacterModel * m = getModel();
	if (m->hasTargetPosition())
	{
		setAnimation(Animation::RUN);
		if (m->getTargetX() > m->getCurrentX())
		{
			orientation = Orientation::BOTTOM_RIGHT;
		}
		else if (m->getTargetX() < m->getCurrentX())
		{
			orientation = Orientation::TOP_LEFT;
		}

		if (m->getTargetY() > m->getCurrentY())
		{
			orientation = Orientation::BOTTOM_LEFT;
		}
		else if (m->getTargetY() < m->getCurrentY())
		{
			orientation = Orientation::TOP_RIGHT;
		}
	}
	else
	{
		setAnimation(Animation::IDLE);
	}
}