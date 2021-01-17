#include "TimelineView.h"

//------------------------------------------------------
// Timeline Item :
//------------------------------------------------------

std::map<std::string, sf::Texture*> TimelineItem::textureCache;

sf::Texture * TimelineItem::getCachedTexture(std::string path)
{
	sf::Texture * result = NULL;
	if (textureCache.find(path) == textureCache.end())
	{
		result = new sf::Texture();
		result->loadFromFile(path);
		result->setSmooth(true);
		textureCache[path] = result;
	}
	
	result = textureCache[path];
	return result;
}

TimelineItem::TimelineItem(tw::BaseCharacterModel * model)
	: tgui::Panel(tgui::Layout2d(190, 90))
{
	getRenderer()->setBackgroundColor(tgui::Color(255, 255, 255, 255));
	sf::Texture * texture = getCachedTexture(model->getClassPreviewPath());
	characterPicture = tgui::Picture::create(*texture);
	characterName = tgui::Label::create(model->getPseudo());
	characterName->getRenderer()->setTextColor(tgui::Color::Black);
	//characterName->getRenderer()->setTextOutlineColor(tgui::Color::White);
	//characterName->getRenderer()->setTextOutlineThickness(1);

	add(characterPicture);
	add(characterName);

	sf::Vector2u size = texture->getSize();
	int width = (float)size.x * (float)((float) getSize().y / (float)size.y);

	characterPicture->setSize(width, getSize().y);
	characterPicture->setPosition(0, 0);
	characterName->setPosition(width + 10, (getSize().y / 2.0) - (characterName->getSize().y / 2.0));
}

//------------------------------------------------------



//------------------------------------------------------
// Timeline View :
//------------------------------------------------------

std::shared_ptr<TimelineView> TimelineView::instance = std::make_shared<TimelineView>();

std::shared_ptr<TimelineView> TimelineView::getInstance()
{
	return instance;
}

TimelineView::TimelineView()
	: tgui::Panel(tgui::Layout2d(tgui::Layout(200), tgui::Layout(100)))
{
	setPosition(1720, 100);
	getRenderer()->setBackgroundColor(tgui::Color(128, 128, 128, 128));

	/*
	tgui::Label::Ptr title = tgui::Label::create("Status joueurs :");
	add(title);
	title->setPosition(5, 5);
	title->getRenderer()->setTextColor(tgui::Color::White);

	tgui::Label::Ptr joueurs = tgui::Label::create("");
	add(joueurs, "playerLabel");
	joueurs->setPosition(5, 30);
	joueurs->getRenderer()->setTextColor(tgui::Color::White);
	*/

	//LinkToServer::getInstance()->addListener(this);
}

TimelineView::~TimelineView()
{
	//LinkToServer::getInstance()->removeListener(this);
}

void TimelineView::setTimeline(std::vector<tw::BaseCharacterModel*> timeline)
{
	removeAllWidgets();
	items.clear();
	setSize(tgui::Layout2d(200, 5 + (timeline.size() * 95)));
	
	for (int i = 0; i < timeline.size(); i++)
	{
		std::shared_ptr<TimelineItem> item = std::make_shared<TimelineItem>(timeline[i]);
		add(item);
		item->setPosition(5, 5 + i * (item->getSize().y + 5));
		items[timeline[i]] = item;
	}
}

void TimelineView::setActiveCharacter(tw::BaseCharacterModel* c)
{
	for (auto it = items.begin(); it != items.end(); it++)
		(*it).second->setActive(false);

	items[c]->setActive(true);
}