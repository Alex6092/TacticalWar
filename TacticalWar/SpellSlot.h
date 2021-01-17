#pragma once

#include <TGUI/TGUI.hpp>
#include <BaseCharacterModel.h>
#include <map>

#define BORDER_OFFSET 20
#define BORDER_OFFDIV (BORDER_OFFSET / 2)

class SpellSlot : public tgui::Picture
{
	int attackNumber;
	tgui::Picture::Ptr spellPicture;
	tgui::Label::Ptr spellCooldownTxt;
	tw::BaseCharacterModel * model;
	sf::Font font;

	static std::map<std::string, sf::Texture> textureCache;
	static sf::Texture * getCachedTexture(std::string path)
	{
		if (textureCache.find(path) == textureCache.end())
		{
			textureCache[path].loadFromFile(path);
			textureCache[path].setSmooth(true);
		}

		return &textureCache[path];
	}

public:
	SpellSlot(tw::BaseCharacterModel * model = NULL, int attackNumber = 0, std::string spellIconPath = "");
	tgui::Picture::Ptr getSpellPicture() {
		return spellPicture;
	}

	tgui::Label::Ptr getSpellCooldownTxt()
	{
		return spellCooldownTxt;
	}

	void setModel(tw::BaseCharacterModel * m)
	{
		this->model = m;
		updateSpellPicture();
	}

	void updateSpellPicture()
	{
		if (model != NULL)
		{
			sf::Texture * spellTexture = getCachedTexture(model->getSpellIconPath(attackNumber));
			spellPicture->getRenderer()->setTexture(*spellTexture);
			spellPicture->setSize(getSize().x - 4, getSize().y - 4);
			setPosition(getPosition());

			// Update tooltip text :
			tgui::Label::Ptr text = std::dynamic_pointer_cast<tgui::Label, tgui::Widget>(spellPicture->getToolTip());
			text->getRenderer()->setBackgroundColor(tgui::Color::Black);
			text->getRenderer()->setTextColor(tgui::Color::White);
			text->setMaximumTextWidth(200);

			std::string str = "";
			str += model->getSpellName(attackNumber) + " (Coût : " + std::to_string(model->getAttackPACost(attackNumber)) + " PA)\n";
			if (model->getAttackCooldown(attackNumber) > 0) str += "Interval de relance : " + std::to_string(model->getAttackCooldown(attackNumber)) + " tour(s)\n";
			str += "Portée min : " + std::to_string(model->getSpellMinPO(attackNumber)) + "\n";
			str += "Portée max : " + std::to_string(model->getSpellMaxPO(attackNumber)) + "\n";

			std::string zoneType = "Undefined"; 
			
			switch (model->getSpellLaunchZoneType(attackNumber))
			{
			case TypeZoneLaunch::NORMAL:
				zoneType = "Standard";
				break;

			case TypeZoneLaunch::LINE:
				zoneType = "En ligne";
				break;

			case TypeZoneLaunch::DIAGONAL:
				zoneType = "En diagonale";
				break;

			case TypeZoneLaunch::STAR:
				zoneType = "En étoile";
				break;
			}
			str += "Type de lancer : " + zoneType + "\n";

			str += "\n" + model->getSpellDescription(attackNumber);

			text->setText(str);
		}
	}

	virtual void setPosition(const tgui::Layout2d & position)
	{
		tgui::Picture::setPosition(position);
		if(spellPicture != NULL)
			spellPicture->setPosition(position.x + BORDER_OFFDIV + 1, position.y + BORDER_OFFDIV + 1);

		if (spellCooldownTxt != NULL)
		{
			spellCooldownTxt->setPosition(position.x + (getSize().x / 2.0) - (spellCooldownTxt->getSize().x / 2.0), position.y + (getSize().y / 2.0) - (spellCooldownTxt->getSize().y / 2.0));
		}
	}

	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const override
	{
		bool canDoAttack = model->canDoAttack(attackNumber);
		spellPicture->setEnabled(canDoAttack);
		spellCooldownTxt->setText(canDoAttack ? "" : std::to_string(model->getAttackCooldown(attackNumber)));

		if (spellCooldownTxt != NULL)
		{
			spellCooldownTxt->setPosition(getPosition().x + (getSize().x / 2.0) - (spellCooldownTxt->getSize().x / 2.0), getPosition().y + (getSize().y / 2.0) - (spellCooldownTxt->getSize().y / 2.0));
		}

		tgui::Picture::draw(target, states);
		spellPicture->draw(target, states);

		if(spellCooldownTxt->isVisible())
			spellCooldownTxt->draw(target, states);
	}

	virtual void setSize(const tgui::Layout2d & size)
	{
		tgui::Picture::setSize(size);
		if(spellPicture != NULL)
			spellPicture->setSize(size.x - BORDER_OFFSET - 2, size.y - BORDER_OFFSET);

		if (spellCooldownTxt != NULL)
		{
			spellCooldownTxt->setPosition(getPosition().x + (getSize().x / 2.0) - (spellCooldownTxt->getSize().x / 2.0), getPosition().y + (getSize().y / 2.0) - (spellCooldownTxt->getSize().y / 2.0));
		}
	}


	virtual bool mouseOnWidget(tgui::Vector2f pos) const
	{
		return (tgui::Picture::mouseOnWidget(pos) || spellPicture->mouseOnWidget(pos) || spellCooldownTxt->mouseOnWidget(pos));
	}

	void setCooldownVisible(bool bVisible)
	{
		spellCooldownTxt->setVisible(bVisible);
	}

	virtual Widget::Ptr askToolTip(tgui::Vector2f mousePos)
	{
		Widget::Ptr result = Picture::askToolTip(mousePos);
		if (result != nullptr)
		{
			float height = result->getSize().y;
			tgui::ToolTip::setDistanceToMouse({ 12, -height });
		}

		return result;
	}
};

