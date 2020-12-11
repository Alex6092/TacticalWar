#include "pch.h"
#include "SpellView.h"
#include <iostream>
#include <string>
#include <deque>
#include <StringUtils.h>
#include <SFML/System/FileInputStream.hpp>
std::map<std::string, sf::Texture*> * SpellView::textureCache = NULL;
sf::Texture * SpellView::getCachedTexture(std::string path)
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

SpellView::SpellView()
{



}

SpellView::~SpellView()
{
	//
}

std::vector<sf::Sprite*> SpellView::loadAnimation(std::string filename)
{

	std::vector<sf::Sprite *> anim;
	sf::Texture *testSpellTexture = getCachedTexture(filename + ".png");
	sf::FileInputStream stream;
	int NbImg = 0, Nb = 0;
	std::string str;
	std::vector<std::string> positionView;
	std::vector<std::string> position;
	if (stream.open(filename + ".txt")) {
		int length = stream.getSize();
		char * data = new char[length + 1];
		stream.read(data, length);
		data[length] = '\0';
		std::vector<std::string> lines = StringUtils::explode(data, '\n');
		delete data;
		std::vector<std::string> info;
		
		for (int i = 0; i < lines.size(); i++) {
			if (lines[i][2] == 'x' && lines[i][3] == 'y' || lines[i][2] == 's' && lines[i][3] == 'i' && lines[i][4] == 'z' && lines[i][5] == 'e') {
				info = StringUtils::explode(lines[i], ':');
				
				info[0] = { 0 };
				info = StringUtils::explode(info[1], '\r');
			for(int x = 0; x < info.size(); x++)
				if(info[x][0] != '\0')
					position.push_back(info[x]);
			}
		}
	}
	else
	{
		std::cout << "Echec de l'ouverture du fichier " << filename.c_str() << ".txt" << std::endl;
	}
	for (int x = 0; x < position.size(); x += 2) {
		positionView.push_back(position[x]+","+position[x+1]);
	}
	for (int i = 0; i < positionView.size(); i++)
		NbImg++;
	for (int i = 0; i < positionView.size(); i++)
		std::cout << positionView[i] << std::endl;
	
	testSpellTexture->setSmooth(true);

	for (int i = 0; i < NbImg; i++)
	{
		std::vector<std::string> result = StringUtils::explode(positionView[i], ',');
		std::vector<std::string> p;
		p.push_back(result[0] + result[1] + result[2] + result[3]);
		result = StringUtils::explode(p[0], ' ');
		p.clear();
		sf::Sprite * s = new sf::Sprite(*testSpellTexture, sf::IntRect(atoi(result[0].c_str()), atoi(result[1].c_str()), atoi(result[2].c_str()), atoi(result[3].c_str())));
		/*testOffsetX = std::atof(result[4].c_str());
		testOffsetY = std::atof(result[5].c_str());*/

		//s->setOrigin(atoi(result[4].c_str()), atoi(result[3].c_str()) - atoi(result[5].c_str()));
		anim.push_back(s);
		result.clear();
	}

	return anim;

}
sf::Sprite * SpellView::getImageToDraw()
{
	return nullptr;
}


void SpellView::update(float deltatime)
{
}
