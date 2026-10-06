#pragma once

#include <TGUI/TGUI.hpp>
#include <CharacterView.h>

class PictureCharacterView : public tgui::Picture
{
private:
	tw::CharacterView * v;
	// Couleurs d'armure et de cheveux (apparence), appliquées avec le masque du personnage.
	bool tinted = false;
	sf::Color armor;
	sf::Color hair;

	static sf::Shader * maskShader()
	{
		static sf::Shader shader;
		static bool loaded = sf::Shader::isAvailable() && shader.loadFromFile("./assets/shaders/vertex.vert", "./assets/shaders/fragment.frag");
		return loaded ? &shader : NULL;
	}

public:
	PictureCharacterView(tw::CharacterView * view = NULL)
	{
		v = view;
	}

	void setTint(const int armorColor[3], const int hairColor[3])
	{
		tinted = true;
		armor = sf::Color(armorColor[0], armorColor[1], armorColor[2]);
		hair = sf::Color(hairColor[0], hairColor[1], hairColor[2]);
	}

	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override
	{
		if (v != NULL)
		{
			v->setAnimation(tw::Animation::RUN);
			sf::Sprite * s = v->getImageToDraw();
			s->setOrigin(s->getOrigin().x, 0);
			s->setPosition(getPosition());
			sf::Shader * shader = tinted ? maskShader() : NULL;
			sf::Texture * mask = v->getMaskToDraw();
			if (shader != NULL && mask != NULL)
			{
				shader->setUniform("mask", *mask);
				shader->setUniform("color1", sf::Glsl::Vec4(armor));
				shader->setUniform("color2", sf::Glsl::Vec4(hair));
				shader->setUniform("color3", sf::Glsl::Vec4(sf::Color(202, 165, 150)));
				target.draw(*s, shader);
			}
			else
			{
				target.draw(*s);
			}
		}
	}

	void setCharacterView(tw::CharacterView * view)
	{
		v = view;
	}

	sf::FloatRect getSize()
	{
		sf::FloatRect rect;
		if (v != NULL)
		{
			rect = v->getImageToDraw()->getGlobalBounds();
		}

		return rect;
	}
};

