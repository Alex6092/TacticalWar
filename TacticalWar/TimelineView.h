#pragma once

#include <TGUI/TGUI.hpp>
#include <BaseCharacterModel.h>
#include <map>

class TimelineItem : public tgui::Panel
{
private:
	tgui::Picture::Ptr characterPicture;
	tgui::Label::Ptr characterName;

	static std::map<std::string, sf::Texture*> textureCache;
	static sf::Texture * getCachedTexture(std::string path);

public:
	TimelineItem(tw::BaseCharacterModel* model);
	void setActive(bool bActive)
	{
		characterName->getRenderer()->setTextColor(bActive ? tgui::Color::Red : tgui::Color::Black);
	}
};

class TimelineView : public tgui::Panel
{
private:
	static std::shared_ptr<TimelineView> instance;

	std::map<tw::BaseCharacterModel*, std::shared_ptr<TimelineItem>> items;

public:
	TimelineView();
	~TimelineView();
	static std::shared_ptr<TimelineView> getInstance();

	void setTimeline(std::vector<tw::BaseCharacterModel*> timeline);
	void setActiveCharacter(tw::BaseCharacterModel* c);
};

